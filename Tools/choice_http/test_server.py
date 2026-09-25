import concurrent.futures
from pathlib import Path
import tempfile
import time
import unittest
from uuid import uuid4

from fastapi.testclient import TestClient
from server import create_app


class RelayTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.db = Path(self.temp.name) / "choices.sqlite3"
        self.app = create_app(self.db, "test-writer", "test-reader")
        self.client = TestClient(self.app)
        self.addCleanup(self.client.close)
        self.session = str(uuid4())
        self.base = f"/api/sessions/{self.session}"
        self.write = {"Authorization": "Bearer test-writer", "X-Game-Time": "1.5"}
        self.read = {"Authorization": "Bearer test-reader"}

    def post(self, sequence, value="1", headers=None):
        return self.client.post(f"{self.base}/decisions/{sequence}", content=value,
                                headers=self.write if headers is None else headers)

    def next(self, client=None, wait=0, consumer="pi-test"):
        return (client or self.client).get(f"{self.base}/next", headers=self.read,
                                           params={"consumer_id": consumer, "wait": wait})

    def ack(self, sequence):
        return self.client.post(f"{self.base}/ack/{sequence}", headers=self.read,
                                params={"consumer_id": "pi-test"})

    def test_lost_post_response_retry_does_not_duplicate(self):
        self.assertEqual(self.post(1).status_code, 201)
        retry = self.post(1)
        self.assertEqual(retry.status_code, 200)
        self.assertEqual(retry.json()["sequence"], "1")
        self.assertEqual(retry.json()["status"], "duplicate")
        self.assertEqual(self.post(1, "0").status_code, 409)
        self.assertEqual(self.next().json()["value"], 1)
        self.assertEqual(self.ack(1).status_code, 204)
        self.assertEqual(self.next().status_code, 204)

    def test_out_of_order_arrival_waits_for_missing_decision(self):
        self.post(2, "0")
        self.assertEqual(self.next().status_code, 204)
        self.assertEqual(self.ack(2).status_code, 409)
        self.post(1, "1")
        self.assertEqual(self.next().json()["sequence"], "1")
        self.assertEqual(self.next().json()["sequence"], "1")  # GET is not an ACK.
        self.ack(1)
        self.assertEqual(self.next().json()["sequence"], "2")
        self.ack(2)
        self.assertEqual(self.ack(1).status_code, 204)  # Retry cannot move cursor backwards.
        self.assertEqual(self.next().status_code, 204)

    def test_restart_preserves_events_and_execution_cursor(self):
        self.post(1)
        self.post(2, "0")
        self.ack(1)
        with TestClient(create_app(self.db, "test-writer", "test-reader")) as restarted:
            self.assertEqual(self.next(restarted).json()["sequence"], "2")

    def test_long_poll_wakes_when_next_individual_decision_arrives(self):
        with concurrent.futures.ThreadPoolExecutor() as pool:
            result = pool.submit(self.next, None, 2)
            time.sleep(0.1)
            self.post(1)
            self.assertEqual(result.result(timeout=2).json()["value"], 1)

    def test_lost_ack_response_and_restart_do_not_replay_confirmed_choice(self):
        self.post(1)
        self.ack(1)
        self.assertEqual(self.ack(1).status_code, 204)
        self.assertEqual(self.next().status_code, 204)

    def test_invalid_values_time_and_sequence(self):
        for value in ["", "2", "01", "[1]", '{"value":1}', "true", "1\n"]:
            self.assertEqual(self.post(1, value).status_code, 422, value)
        for seq in [0, -1, 2**63]:
            self.assertEqual(self.post(seq).status_code, 422)
        for game_time in ["NaN", "inf", "not-a-number"]:
            self.assertEqual(self.post(1, headers={**self.write, "X-Game-Time": game_time}).status_code, 422)
        self.assertEqual(self.post(1, headers={"Authorization": "Bearer test-writer"}).status_code, 422)

    def test_writer_and_reader_permissions_are_separate(self):
        self.assertEqual(self.post(1, headers={}).status_code, 401)
        self.assertEqual(self.post(1, headers=self.read).status_code, 401)
        self.assertEqual(self.client.get("/api/sessions", headers=self.write).status_code, 401)
        self.assertEqual(self.client.get("/api/sessions", headers=self.read).status_code, 200)

    def test_independent_sessions_and_consumers(self):
        self.post(1)
        self.ack(1)
        self.assertEqual(self.next(consumer="another-device").json()["sequence"], "1")
        self.session = str(uuid4())
        self.base = f"/api/sessions/{self.session}"
        self.assertEqual(self.next().status_code, 204)
        self.post(1, "0")
        self.assertEqual(self.next().json()["value"], 0)

    def test_no_ack_of_nonexistent_decision(self):
        self.assertEqual(self.ack(1).status_code, 409)

    def test_concurrent_duplicate_posts_store_one_event(self):
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            statuses = list(pool.map(lambda _: self.post(1).status_code, range(8)))
        self.assertEqual(statuses.count(201), 1)
        self.assertEqual(statuses.count(200), 7)

    def test_unauthenticated_development_is_loopback_only(self):
        app = create_app(Path(self.temp.name) / "dev.sqlite3")
        with TestClient(app, client=("127.0.0.1", 12345)) as local:
            self.assertEqual(local.get("/api/sessions").status_code, 200)
            self.assertEqual(local.get("/api/sessions", headers={"X-Forwarded-For": "203.0.113.1"}).status_code, 403)
        with TestClient(app, client=("203.0.113.1", 12345)) as remote:
            self.assertEqual(remote.get("/api/sessions").status_code, 403)


if __name__ == "__main__":
    unittest.main()
