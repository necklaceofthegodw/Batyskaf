# Decyzje Batyskafu przez HTTP

Każde `ChoiceSubsystem::AddRow(Time, Value)` wysyła **osobny POST** z treścią
`0` albo `1` (jeden bajt UTF-8). Nie ma zbierania paczek ani timera opóźniającego
pierwszą wysyłkę. Istniejące wywołania w Blueprintach i `SaveCSV()` pozostają dostępne.

Zawartość tego katalogu: API serwera i jego testy. **Odbiornik Raspberry Pi i sterowanie
urządzeniem nie zostały zaimplementowane**, zgodnie z zakresem zadania.

## Uruchomienie lokalne

Python 3.10 lub nowszy. Polecenia wykonuj w katalogu głównym projektu:

```powershell
python -m venv Tools/choice_http/.venv
Tools/choice_http/.venv/Scripts/python.exe -m pip install -r Tools/choice_http/requirements.txt
Tools/choice_http/.venv/Scripts/python.exe Tools/choice_http/server.py
```

Serwer nasłuchuje na `http://127.0.0.1:8088`. Dane zapisuje w
`Saved/ChoiceHttp/server.sqlite3`. Zatrzymanie: Ctrl+C. Uruchomienie ponownie z tym
samym plikiem bazy zachowuje decyzje i potwierdzenia wykonania.

Po przebudowaniu projektu uruchom edytor ponownie, aby załadować nowy subsystem.
Uruchom grę i podejmij decyzję wywołującą `AddRow()`. Identyfikator rozgrywki znajdziesz
w Output Log (`LogChoiceHttp`) lub przez Blueprint `GetChoiceSessionId()`.

```powershell
Invoke-RestMethod http://127.0.0.1:8088/health
Invoke-RestMethod http://127.0.0.1:8088/api/sessions
```

Brak wpisów przed pierwszą decyzją jest normalny. Dokumentacja API jest pod `/docs`.

## Konfiguracja gry

Sekcja w `Config/DefaultGame.ini`:

```ini
[/Script/Batyskaf.ChoiceSubsystem]
bHttpEnabled=True
HttpBaseUrl=http://127.0.0.1:8088
RequestTimeoutSeconds=5.0
RetryBaseDelaySeconds=0.5
RetryMaxDelaySeconds=10.0
MaxInFlightRequests=8
```

`BATYSKAF_CHOICE_URL` nadpisuje URL. Token zapisu podaje się wyłącznie zmienną
środowiskową `BATYSKAF_CHOICE_WRITE_TOKEN`; nie jest zapisywany w plikach kolejki.
Zmienne ustaw przed uruchomieniem edytora/gry, ponieważ już uruchomiony proces
nie odziedziczy późniejszych zmian środowiska powłoki.

`bHttpEnabled=False` pozostawia tylko dotychczasowy zapis CSV. `AddRow` aktualizuje
tablicę CSV w pamięci; plik CSV powstaje przy `SaveCSV()`, jak dotychczas.
Wartości inne niż 0/1 i niefinitywny czas są odrzucane z błędem w logu.

## Serwer w internecie

Uruchom API na serwerze z trwałym dyskiem, za reverse proxy obsługującym HTTPS.
Na początek użyj jednego procesu serwera. Proxy powinno przekazywać żądania do
`127.0.0.1:8088`, mieć limit czasu powyżej 30 sekund i nie cache'ować API.
Nie korzystaj z hostingu z nietrwałym systemem plików dla tej konfiguracji SQLite.

Na serwerze ustaw **dwa różne losowe tokeny**:

```powershell
$env:BATYSKAF_CHOICE_WRITE_TOKEN = '<token zapisu>'
$env:BATYSKAF_CHOICE_READ_TOKEN = '<inny token odczytu i potwierdzeń>'
Tools/choice_http/.venv/Scripts/python.exe Tools/choice_http/server.py --db 'D:/ChoiceData/decisions.sqlite3'
```

Na komputerze z grą ustaw tylko token zapisu i docelowy adres HTTPS:

```powershell
$env:BATYSKAF_CHOICE_URL = 'https://twoj-serwer.example'
$env:BATYSKAF_CHOICE_WRITE_TOKEN = '<ten sam token zapisu>'
```

Uruchom grę z tej powłoki albo skonfiguruj te zmienne w środowisku, z którego ją
uruchamiasz. Domyślny adres lokalny nie wysyła nic do internetu. Instalacja serwera,
domena, certyfikat i docelowe tokeny wymagają konfiguracji na wybranym hostingu.

Bez tokenów API dopuszcza tylko bezpośrednie połączenia z loopback, bez nagłówków
proxy. Nie wystawiaj tego trybu przez proxy. Przy wiązaniu z `0.0.0.0` CLI wymaga
obu tokenów. Tokeny dają dostęp do wszystkich sesji tej instalacji; to serwer jednego
eksperymentu, a nie system kont wielu niezależnych użytkowników.

## Protokół

Numeracja zaczyna się od 1. Każde uruchomienie GameInstance tworzy nowy UUID sesji.
Numer pierwszej decyzji nowej rozgrywki nie jest kontynuacją poprzedniej.

### Gra → serwer

```http
POST /api/sessions/{session_id}/decisions/{sequence}
Content-Type: application/json
X-Game-Time: 42.5
Authorization: Bearer <token zapisu>

1
```

Treść musi zawierać dokładnie jeden znak `0` lub `1`, bez nowej linii.
`X-Game-Time` przechowuje przekazany do `AddRow` czas gry; nie jest zegarem UTC
i nie służy do ustalania kolejności. Decyzję identyfikuje para `(session_id, sequence)`.

Po zatwierdzeniu transakcji SQLite serwer odpowiada `201`:

```json
{"session_id":"UUID sesji","sequence":"1","status":"stored"}
```

Ponowienie identycznej decyzji zwraca `200`, ze statusem `duplicate`. Zmiana wartości
lub czasu pod tym samym numerem daje `409`. Numer w JSON jest tekstem, żeby nie
tracić precyzji dużych identyfikatorów. Potwierdzenie zapisu na serwerze **nie oznacza
jeszcze wykonania decyzji przez urządzenie**.

### API do przyszłego odbiornika

- `GET /api/sessions` — maksymalnie 100 ostatnio utworzonych na serwerze sesji.
  Odbiornik powinien wskazać konkretną sesję, a nie automatycznie wybierać najnowszą.
- `GET /api/sessions/{session_id}/next?consumer_id=pi-1&wait=25` — long polling;
  zwraca jedną następną niepotwierdzoną decyzję albo `204` po upływie czasu.
- `POST /api/sessions/{session_id}/ack/{sequence}?consumer_id=pi-1` — potwierdzenie
  **po wykonaniu** danej decyzji; odpowiedź `204`. Powtórzenie ACK jest bezpieczne.

Te trzy adresy wymagają tokenu odczytu. GET `/next` sam nie przesuwa kursora.
Ponowny GET bez ACK zwróci tę samą decyzję. Gdy jest decyzja 2, ale brakuje 1,
serwer czeka na 1. Nie pozwala też potwierdzić 2 przed 1. Stan potwierdzeń jest
przechowywany osobno dla każdej pary sesja/odbiornik. Jeden `consumer_id` powinien
mieć tylko jeden aktywny proces odbiorczy, aby nie wykonać tej samej decyzji równolegle.

Wynik `/next` zawiera `session_id`, `sequence`, `value`, `game_time` i `received_at`
(czas UTC serwera w sekundach Unix). Brak wpisu lub nieznana jeszcze sesja daje
oczekiwanie, a następnie `204`, nigdy przeskoczenie brakującej decyzji.

## Błędy, kolejność i opóźnienie

- Gra najpierw zapisuje decyzję do `Saved/ChoiceHttp/Outbox`, a następnie rozpoczyna
  osobne asynchroniczne żądanie. Krótki zapis dyskowy jest kosztem odporności na restart.
- Nie czeka na odpowiedź poprzedniego POST. Domyślnie dopuszcza 8 żądań w toku;
  nadmiar podczas awarii/przeciążenia czeka w kolejce. Nie jest łączony w paczki.
- Błędy sieci, 408, 429 i błędy serwera są ponawiane z odstępem 0,5 / 1 / 2 / 4 / 8 /
  10 sekund. Ticker co 0,1 s obsługuje ponowienia i kolejkę przy przeciążeniu.
- Inne błędy 4xx blokują dany wpis i zgłaszają błąd. Po poprawieniu konfiguracji
  uruchom grę ponownie. Wpis pozostaje na dysku. Nie kasuj go, jeśli każda decyzja
  ma zostać odtworzona.
- Potwierdzenie z błędnym identyfikatorem lub bez prawidłowego JSON nie usuwa wpisu.
- Przy restarcie gra ponawia niepotwierdzone decyzje poprzednich sesji. Dane dla
  innego adresu serwera zostają na dysku i nie są automatycznie kierowane na nowy adres.
- Kolejność gwarantuje kursor odbioru, nie kolejność zakończenia żądań HTTP.
  Przerwa w numeracji wstrzymuje późniejsze decyzje.
- Nie ma automatycznego kasowania historii serwera. Kopie zapasowe i retencję trzeba
  dobrać do eksperymentu. Nie uruchamiaj dwóch instancji gry korzystających z tego
  samego katalogu Saved/ChoiceHttp/Outbox jednocześnie.
- Dostarczenie i ponawianie nie gwarantują pojedynczego fizycznego wykonania po
  awarii zasilania urządzenia. To wymaga zaprojektowania odbiornika i jego obsługi ACK.
- Zapis dyskowy, planowanie wątku gry, sieć i serwer wpływają na czas dostarczenia.
  Nie jest to komunikacja o gwarantowanym czasie rzeczywistym.

## Weryfikacja

```powershell
Tools/choice_http/.venv/Scripts/python.exe -m pip install -r Tools/choice_http/requirements-test.txt
Tools/choice_http/.venv/Scripts/python.exe -m unittest discover -s Tools/choice_http -p 'test_*.py' -v
```

Testy obejmują duplikaty i utratę potwierdzeń, odwróconą kolejność nadejścia,
restart bazy, long polling, walidację danych i rozdzielenie tokenów.
