"""HTTP relay for individual Batyskaf choices. No Raspberry Pi receiver included."""
from __future__ import annotations

import argparse
from contextlib import closing
import hmac
import math
import os
from pathlib import Path
import sqlite3
import threading
import time
from typing import Annotated
from uuid import UUID

from fastapi import Depends, FastAPI, HTTPException, Path as ApiPath, Query, Request
from fastapi.responses import JSONResponse, Response
from starlette.concurrency import run_in_threadpool

MAX_SEQUENCE = 2**63 - 1
Sequence = Annotated[int, ApiPath(ge=1, le=MAX_SEQUENCE)]
Consumer = Annotated[str, Query(min_length=1, max_length=64, pattern=r"^[A-Za-z0-9_-]+$")]


class Conflict(Exception):
    pass


class ChoiceStore:
    """SQLite is authoritative; the condition only wakes long-polling requests."""

    def __init__(self, path: str | Path):
        self.path = Path(path)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.changed = threading.Condition()
        with closing(self.connect()) as db:
            db.execute("PRAGMA journal_mode=WAL")
            db.executescript("""
                CREATE TABLE IF NOT EXISTS decisions (
                    session_id TEXT NOT NULL,
                    sequence INTEGER NOT NULL CHECK(sequence > 0),
                    value INTEGER NOT NULL CHECK(value IN (0, 1)),
                    game_time REAL NOT NULL,
                    received_at REAL NOT NULL,
                    PRIMARY KEY(session_id, sequence)
                );
                CREATE TABLE IF NOT EXISTS acknowledgements (
                    session_id TEXT NOT NULL,
                    consumer_id TEXT NOT NULL,
                    sequence INTEGER NOT NULL CHECK(sequence > 0),
                    PRIMARY KEY(session_id, consumer_id)
                );
            """)

    def connect(self):
        db = sqlite3.connect(self.path, timeout=5)
        db.row_factory = sqlite3.Row
        db.execute("PRAGMA synchronous=FULL")
        return db

    def put(self, session: str, sequence: int, value: int, game_time: float) -> bool:
        with self.changed, closing(self.connect()) as db, db:
            db.execute("BEGIN IMMEDIATE")
            old = db.execute(
                "SELECT value, game_time FROM decisions WHERE session_id=? AND sequence=?",
                (session, sequence),
            ).fetchone()
            if old:
                if old["value"] != value or old["game_time"] != game_time:
                    raise Conflict("This sequence already contains a different decision")
                return False
            db.execute("INSERT INTO decisions VALUES (?, ?, ?, ?, ?)",
                       (session, sequence, value, game_time, time.time()))
            db.commit()  # Notify only after the decision is committed to durable storage.
            self.changed.notify_all()
            return True

    @staticmethod
    def last_ack(db, session: str, consumer: str) -> int:
        row = db.execute(
            "SELECT sequence FROM acknowledgements WHERE session_id=? AND consumer_id=?",
            (session, consumer),
        ).fetchone()
        return row["sequence"] if row else 0

    def next_decision(self, session: str, consumer: str, wait: float):
        deadline = time.monotonic() + wait
        with self.changed:
            while True:
                with closing(self.connect()) as db:
                    expected = self.last_ack(db, session, consumer) + 1
                    row = db.execute(
                        "SELECT * FROM decisions WHERE session_id=? AND sequence=?",
                        (session, expected),
                    ).fetchone()
                if row:
                    event = dict(row)
                    event["sequence"] = str(event["sequence"])
                    return event
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    return None
                # Periodic recheck also covers a second process writing the database.
                self.changed.wait(min(remaining, 0.5))

    def acknowledge(self, session: str, consumer: str, sequence: int):
        with self.changed, closing(self.connect()) as db, db:
            db.execute("BEGIN IMMEDIATE")
            previous = self.last_ack(db, session, consumer)
            if sequence <= previous:
                return  # Safe to retry an ACK if its response was lost.
            exists = db.execute("SELECT 1 FROM decisions WHERE session_id=? AND sequence=?",
                                (session, sequence)).fetchone()
            if sequence != previous + 1 or not exists:
                raise Conflict("ACK must refer to the next existing decision in order")
            db.execute("""INSERT INTO acknowledgements VALUES (?, ?, ?)
                ON CONFLICT(session_id, consumer_id) DO UPDATE SET sequence=excluded.sequence""",
                       (session, consumer, sequence))
            db.commit()
            self.changed.notify_all()

    def sessions(self):
        with closing(self.connect()) as db:
            return [dict(r) for r in db.execute("""
                SELECT session_id, COUNT(*) AS received_count,
                       CAST(MAX(sequence) AS TEXT) AS highest_received_sequence,
                       MIN(received_at) AS first_received_at
                FROM decisions GROUP BY session_id ORDER BY first_received_at DESC LIMIT 100
            """)]


def create_app(db_path: str | Path, write_token: str = "", read_token: str = "") -> FastAPI:
    if bool(write_token) != bool(read_token):
        raise ValueError("Set both write and read tokens, or neither for loopback development")
    if write_token and hmac.compare_digest(write_token, read_token):
        raise ValueError("Write and read tokens must be different")
    store = ChoiceStore(db_path)
    app = FastAPI(title="Batyskaf individual decision relay", version="1.0")
    app.state.store = store

    def authorize(request: Request, token: str):
        if token:
            supplied = request.headers.get("Authorization", "")
            if not hmac.compare_digest(supplied.encode(), ("Bearer " + token).encode()):
                raise HTTPException(401, "Invalid credentials", headers={"WWW-Authenticate": "Bearer"})
        elif (not request.client or request.client.host not in {"127.0.0.1", "::1"}
              or "forwarded" in request.headers or "x-forwarded-for" in request.headers):
            raise HTTPException(403, "Unauthenticated development access is loopback-only")

    def writer(request: Request):
        authorize(request, write_token)

    def reader(request: Request):
        authorize(request, read_token)

    @app.middleware("http")
    async def no_cache(request: Request, call_next):
        response = await call_next(request)
        response.headers["Cache-Control"] = "no-store"
        return response

    @app.exception_handler(Conflict)
    async def conflict_handler(request, exc):
        return JSONResponse({"detail": str(exc)}, status_code=409)

    @app.get("/health")
    def health():
        return {"status": "ok"}

    @app.post("/api/sessions/{session_id}/decisions/{sequence}", dependencies=[Depends(writer)])
    async def receive(session_id: UUID, sequence: Sequence, request: Request):
        body = bytearray()
        async for chunk in request.stream():
            if len(body) + len(chunk) > 1:
                raise HTTPException(422, "Send exactly one ASCII byte: 0 or 1")
            body.extend(chunk)
        if body not in (b"0", b"1"):
            raise HTTPException(422, "Send exactly one ASCII byte: 0 or 1")
        try:
            game_time = float(request.headers["X-Game-Time"])
            if not math.isfinite(game_time):
                raise ValueError()
        except (KeyError, ValueError):
            raise HTTPException(422, "X-Game-Time must be a finite number") from None
        session = str(session_id)
        created = await run_in_threadpool(store.put, session, sequence, int(body), game_time)
        return JSONResponse({"session_id": session, "sequence": str(sequence),
                             "status": "stored" if created else "duplicate"},
                            status_code=201 if created else 200)

    @app.get("/api/sessions", dependencies=[Depends(reader)])
    def sessions():
        return store.sessions()

    @app.get("/api/sessions/{session_id}/next", dependencies=[Depends(reader)])
    def next_decision(session_id: UUID, consumer_id: Consumer,
                      wait: Annotated[float, Query(ge=0, le=25)] = 25):
        event = store.next_decision(str(session_id), consumer_id, wait)
        return event if event else Response(status_code=204)

    @app.post("/api/sessions/{session_id}/ack/{sequence}", dependencies=[Depends(reader)])
    def acknowledge(session_id: UUID, sequence: Sequence, consumer_id: Consumer):
        store.acknowledge(str(session_id), consumer_id, sequence)
        return Response(status_code=204)

    return app


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8088)
    parser.add_argument("--db", default=str(Path(__file__).resolve().parents[2] / "Saved/ChoiceHttp/server.sqlite3"))
    args = parser.parse_args()
    write_token = os.environ.get("BATYSKAF_CHOICE_WRITE_TOKEN", "")
    read_token = os.environ.get("BATYSKAF_CHOICE_READ_TOKEN", "")
    if args.host not in {"127.0.0.1", "::1", "localhost"} and not (write_token and read_token):
        parser.error("Non-loopback binding requires both authentication tokens")
    import uvicorn
    uvicorn.run(create_app(args.db, write_token, read_token), host=args.host, port=args.port,
                proxy_headers=False, timeout_keep_alive=60)


if __name__ == "__main__":
    main()
