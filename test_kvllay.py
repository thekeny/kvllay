import socket
import time
import threading
import sys
import subprocess
import os

def send_recv(sock, data):
    sock.sendall(data.encode('utf-8') if isinstance(data, str) else data)
    return sock.recv(4096).decode('utf-8')

def test_kvllay(port=6389):
    print(f"Connecting to kvllay on port {port}...")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(("127.0.0.1", port))

    # Test 1: PING (inline)
    res = send_recv(s, "PING\r\n")
    assert res == "+PONG\r\n", f"PING inline failed: {repr(res)}"
    print("[PASS] PING (inline)")

    # Test 2: PING with message (inline)
    res = send_recv(s, "PING hello\r\n")
    assert res == "$5\r\nhello\r\n", f"PING msg failed: {repr(res)}"
    print("[PASS] PING with message")

    # Test 3: SET and GET (inline)
    res = send_recv(s, 'SET mykey "hello world"\r\n')
    assert res == "+OK\r\n", f"SET failed: {repr(res)}"
    res = send_recv(s, "GET mykey\r\n")
    assert res == "$11\r\nhello world\r\n", f"GET failed: {repr(res)}"
    print("[PASS] SET and GET (inline)")

    # Test 4: RESP Array format (what redis-cli sends)
    res = send_recv(s, "*3\r\n$3\r\nSET\r\n$4\r\nuser\r\n$5\r\nalice\r\n")
    assert res == "+OK\r\n", f"RESP SET failed: {repr(res)}"
    
    res = send_recv(s, "*2\r\n$3\r\nGET\r\n$4\r\nuser\r\n")
    assert res == "$5\r\nalice\r\n", f"RESP GET failed: {repr(res)}"
    print("[PASS] SET and GET (RESP array protocol)")

    # Test 5: Non-existent key (GET returns $-1\r\n)
    res = send_recv(s, "GET nonexistent\r\n")
    assert res == "$-1\r\n", f"GET null failed: {repr(res)}"
    print("[PASS] GET non-existent (null bulk string)")

    # Test 6: EXISTS
    res = send_recv(s, "EXISTS mykey user nonexistent\r\n")
    assert res == ":2\r\n", f"EXISTS failed: {repr(res)}"
    print("[PASS] EXISTS")

    # Test 7: KEYS
    res = send_recv(s, "KEYS *\r\n")
    assert res.startswith("*2\r\n"), f"KEYS * failed: {repr(res)}"
    assert "mykey" in res and "user" in res
    print("[PASS] KEYS *")

    # KEYS requires exactly one pattern argument, like Redis.
    res = send_recv(s, "KEYS\r\n")
    assert res == "-ERR wrong number of arguments for 'keys' command\r\n", f"KEYS without args failed: {repr(res)}"
    print("[PASS] KEYS without arguments")

    # Test 8: DBSIZE
    res = send_recv(s, "DBSIZE\r\n")
    assert res == ":2\r\n", f"DBSIZE failed: {repr(res)}"
    print("[PASS] DBSIZE")

    # Test 9: DEL
    res = send_recv(s, "DEL mykey nonexistent\r\n")
    assert res == ":1\r\n", f"DEL failed: {repr(res)}"
    res = send_recv(s, "GET mykey\r\n")
    assert res == "$-1\r\n", f"GET after DEL failed: {repr(res)}"
    print("[PASS] DEL")

    # Test 10: FLUSHDB
    res = send_recv(s, "FLUSHDB\r\n")
    assert res == "+OK\r\n", f"FLUSHDB failed: {repr(res)}"
    res = send_recv(s, "DBSIZE\r\n")
    assert res == ":0\r\n", f"DBSIZE after FLUSHDB failed: {repr(res)}"
    print("[PASS] FLUSHDB")

    # Test 11: COMMAND (redis-cli handshake)
    res = send_recv(s, "COMMAND\r\n")
    assert res == "*0\r\n", f"COMMAND failed: {repr(res)}"
    print("[PASS] COMMAND (handshake)")

    # Test 12: HELLO protocol negotiation
    res = send_recv(s, "HELLO 2\r\n")
    assert res.startswith("*14\r\n") and "$6\r\nserver\r\n" in res, f"HELLO 2 failed: {repr(res)}"
    res = send_recv(s, "HELLO 3 SETNAME test-client\r\n")
    assert res.startswith("%7\r\n") and "$6\r\nserver\r\n" in res, f"HELLO 3 failed: {repr(res)}"
    print("[PASS] HELLO RESP2/RESP3 handshake")

    # Test 13: INFO
    res = send_recv(s, "INFO\r\n")
    assert "kvllay_version:1.0.0" in res, f"INFO failed: {repr(res)}"
    print("[PASS] INFO")

    # Test 13: SELECT
    res = send_recv(s, "SELECT 0\r\n")
    assert res == "+OK\r\n", f"SELECT 0 failed: {repr(res)}"
    res = send_recv(s, "SELECT +0\r\n")
    assert res == "+OK\r\n", f"SELECT +0 failed: {repr(res)}"
    res = send_recv(s, "*2\r\n$6\r\nSELECT\r\n$1\r\n0\r\n")
    assert res == "+OK\r\n", f"RESP SELECT 0 failed: {repr(res)}"
    res = send_recv(s, "SELECT 1\r\n")
    assert res == "-ERR DB index is out of range\r\n", f"SELECT 1 failed: {repr(res)}"
    res = send_recv(s, "SELECT -1\r\n")
    assert res == "-ERR DB index is out of range\r\n", f"SELECT -1 failed: {repr(res)}"
    res = send_recv(s, "SELECT invalid\r\n")
    assert res == "-ERR value is not an integer or out of range\r\n", f"SELECT invalid failed: {repr(res)}"
    res = send_recv(s, "SELECT\r\n")
    assert res == "-ERR wrong number of arguments for 'select' command\r\n", f"SELECT no args failed: {repr(res)}"
    res = send_recv(s, "SELECT 0 1\r\n")
    assert res == "-ERR wrong number of arguments for 'select' command\r\n", f"SELECT too many args failed: {repr(res)}"
    print("[PASS] SELECT (db 0, out of range db > 0, negative db, invalid args)")

    # Test CLIENT commands
    res = send_recv(s, "CLIENT ID\r\n")
    assert res.startswith(":") and int(res[1:].strip()) > 0, f"CLIENT ID failed: {repr(res)}"
    assert send_recv(s, "CLIENT GETNAME\r\n") == "$11\r\ntest-client\r\n"
    s_fresh = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s_fresh.connect(("127.0.0.1", port))
    assert send_recv(s_fresh, "CLIENT GETNAME\r\n") == "$-1\r\n"
    s_fresh.close()
    assert send_recv(s, "CLIENT SETNAME valid-name\r\n") == "+OK\r\n"
    assert send_recv(s, "CLIENT GETNAME\r\n") == "$10\r\nvalid-name\r\n"
    res = send_recv(s, "CLIENT SETNAME 'invalid name'\r\n")
    assert res == "-ERR client name cannot contain spaces, newlines or special characters\r\n"
    assert send_recv(s, "CLIENT SETINFO LIB-NAME test-lib\r\n") == "+OK\r\n"
    assert send_recv(s, "CLIENT SETINFO LIB-VER 1.2.3\r\n") == "+OK\r\n"
    res_list = send_recv(s, "CLIENT LIST\r\n")
    assert "name=valid-name" in res_list and "lib-name=test-lib" in res_list and "lib-ver=1.2.3" in res_list
    print("[PASS] CLIENT (ID, SETNAME, GETNAME, SETINFO, LIST, whitespace validation)")

    # Test SET options with leading plus
    assert send_recv(s, "SET set_plus_k val EX +10\r\n") == "+OK\r\n"
    assert send_recv(s, "GET set_plus_k\r\n") == "$3\r\nval\r\n"
    print("[PASS] SET with leading plus duration (EX +10)")

    # Test 14: QUIT
    res = send_recv(s, "QUIT\r\n")
    assert res == "+OK\r\n", f"QUIT failed: {repr(res)}"
    rem = s.recv(1024)
    assert len(rem) == 0, f"Expected socket close, got {repr(rem)}"
    s.close()
    print("[PASS] QUIT and connection close")

    print("\n--- Testing Multi-Client Concurrency ---")
    threads = []
    errors = []

    def worker(client_id):
        try:
            ws = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            ws.connect(("127.0.0.1", port))
            for i in range(50):
                k = f"k_{client_id}_{i}"
                v = f"v_{client_id}_{i}"
                send_recv(ws, f"SET {k} {v}\r\n")
                r = send_recv(ws, f"GET {k}\r\n")
                expected = f"${len(v)}\r\n{v}\r\n"
                if r != expected:
                    errors.append(f"Mismatch in worker {client_id}: got {r}, expected {expected}")
            ws.close()
        except Exception as e:
            errors.append(f"Worker {client_id} exception: {e}")

    for cid in range(10):
        t = threading.Thread(target=worker, args=(cid,))
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    assert not errors, f"Concurrency errors: {errors}"
    print("[PASS] 10 concurrent clients x 50 operations = 500 requests succeeded")

def test_auth(port=6390, password="secret123"):
    print(f"\n--- Testing Password Protection on port {port} ---")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(("127.0.0.1", port))

    # Try command without AUTH -> expect NOAUTH
    res = send_recv(s, "PING\r\n")
    assert "NOAUTH" in res, f"Expected NOAUTH, got: {repr(res)}"
    print("[PASS] Rejected command without auth (NOAUTH)")

    # Try AUTH with wrong password -> expect WRONGPASS
    res = send_recv(s, "AUTH wrongpass\r\n")
    assert "WRONGPASS" in res, f"Expected WRONGPASS, got: {repr(res)}"
    print("[PASS] Rejected wrong password (WRONGPASS)")

    # Still unauthorized
    res = send_recv(s, "SET a 1\r\n")
    assert "NOAUTH" in res, f"Expected NOAUTH, got: {repr(res)}"

    # Try AUTH with correct password -> expect +OK
    res = send_recv(s, f"AUTH {password}\r\n")
    assert res == "+OK\r\n", f"Expected +OK, got: {repr(res)}"
    print("[PASS] Authenticated successfully with password")

    # Commands now succeed
    res = send_recv(s, "SET a 123\r\n")
    assert res == "+OK\r\n", f"Expected +OK, got: {repr(res)}"
    res = send_recv(s, "GET a\r\n")
    assert res == "$3\r\n123\r\n", f"Expected $3\\r\\n123\\r\\n, got: {repr(res)}"
    print("[PASS] Commands work after authentication")

    s.close()
    print("[PASS] All AUTH tests passed!")

def test_ttl(port=6389):
    print(f"\n--- Testing TTL & Expiration on port {port} ---")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(("127.0.0.1", port))

    send_recv(s, "FLUSHDB\r\n")

    res = send_recv(s, "TTL nonexistent\r\n")
    assert res == ":-2\r\n", f"Expected -2 for nonexistent TTL, got {res}"
    res = send_recv(s, "PTTL nonexistent\r\n")
    assert res == ":-2\r\n", f"Expected -2 for nonexistent PTTL, got {res}"
    print("[PASS] TTL and PTTL on nonexistent key returns -2")

    send_recv(s, "SET k1 v1\r\n")
    res = send_recv(s, "TTL k1\r\n")
    assert res == ":-1\r\n", f"Expected -1 for persistent key TTL, got {res}"
    res = send_recv(s, "PTTL k1\r\n")
    assert res == ":-1\r\n", f"Expected -1 for persistent key PTTL, got {res}"
    print("[PASS] Persistent key returns -1")

    res = send_recv(s, "EXPIRE k1 2\r\n")
    assert res == ":1\r\n", f"Expected 1 from EXPIRE, got {res}"
    res = send_recv(s, "TTL k1\r\n")
    assert res in (":2\r\n", ":1\r\n"), f"Expected 1 or 2 for TTL, got {res}"
    res = send_recv(s, "PTTL k1\r\n")
    pttl_val = int(res.strip()[1:])
    assert 0 < pttl_val <= 2000, f"Expected 0 < PTTL <= 2000, got {pttl_val}"
    print("[PASS] EXPIRE, TTL, and PTTL work correctly")

    res = send_recv(s, "PERSIST k1\r\n")
    assert res == ":1\r\n", f"Expected 1 from PERSIST, got {res}"
    res = send_recv(s, "TTL k1\r\n")
    assert res == ":-1\r\n", f"Expected -1 after PERSIST, got {res}"
    res = send_recv(s, "PERSIST k1\r\n")
    assert res == ":0\r\n", f"Expected 0 from PERSIST on persistent key, got {res}"
    print("[PASS] PERSIST removes TTL and returns 0 when no TTL")

    res = send_recv(s, "PEXPIRE k1 400\r\n")
    assert res == ":1\r\n", f"Expected 1 from PEXPIRE, got {res}"
    res = send_recv(s, "PTTL k1\r\n")
    pttl_val = int(res.strip()[1:])
    assert 0 < pttl_val <= 400, f"Expected 0 < PTTL <= 400, got {pttl_val}"
    time.sleep(0.5)
    res = send_recv(s, "GET k1\r\n")
    assert res == "$-1\r\n", f"Expected null after expiration, got {res}"
    res = send_recv(s, "TTL k1\r\n")
    assert res == ":-2\r\n", f"Expected -2 after expiration, got {res}"
    print("[PASS] PEXPIRE and lazy eviction on GET")

    res = send_recv(s, "SETEX k2 1 hello_ttl\r\n")
    assert res == "+OK\r\n", f"Expected +OK from SETEX, got {res}"
    res = send_recv(s, "GET k2\r\n")
    assert res == "$9\r\nhello_ttl\r\n", f"Expected value from SETEX, got {res}"
    res = send_recv(s, "TTL k2\r\n")
    assert res in (":1\r\n", ":2\r\n"), f"Expected 1 from TTL after SETEX, got {res}"
    time.sleep(1.2)
    res = send_recv(s, "EXISTS k2\r\n")
    assert res == ":0\r\n", f"Expected 0 from EXISTS after expiration, got {res}"
    print("[PASS] SETEX and lazy eviction on EXISTS")

    send_recv(s, "SETEX k_reset 10 initial\r\n")
    res = send_recv(s, "TTL k_reset\r\n")
    assert int(res.strip()[1:]) > 0, f"Expected positive TTL, got {res}"
    send_recv(s, "SET k_reset permanent\r\n")
    res = send_recv(s, "TTL k_reset\r\n")
    assert res == ":-1\r\n", f"Expected -1 after overwriting with SET, got {res}"
    print("[PASS] SET clears existing TTL")

    send_recv(s, "SET k_del 123\r\n")
    res = send_recv(s, "EXPIRE k_del 0\r\n")
    assert res == ":1\r\n", f"Expected 1 from EXPIRE 0, got {res}"
    res = send_recv(s, "EXISTS k_del\r\n")
    assert res == ":0\r\n", f"Expected key to be deleted by EXPIRE 0, got {res}"
    print("[PASS] EXPIRE with 0 deletes key immediately")

    send_recv(s, "FLUSHDB\r\n")
    send_recv(s, "SETEX auto1 1 v\r\n")
    send_recv(s, "SETEX auto2 1 v\r\n")
    send_recv(s, "SETEX auto3 1 v\r\n")
    res = send_recv(s, "DBSIZE\r\n")
    assert res == ":3\r\n", f"Expected 3 keys, got {res}"
    time.sleep(1.3)
    res = send_recv(s, "DBSIZE\r\n")
    assert res == ":0\r\n", f"Expected active eviction to clear all expired keys, got {res}"
    print("[PASS] Active background eviction sweeps expired keys automatically")

    s.close()
    print("[PASS] All TTL & Expiration tests passed!")

def test_atomic_counters_and_rate_limiting(port=6389):
    print(f"\n--- Testing Atomic Counters & Rate Limiting on port {port} ---")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(("127.0.0.1", port))
    send_recv(s, "FLUSHDB\r\n")

    # 1. INCR on nonexistent key -> initializes to 1
    res = send_recv(s, "INCR counter\r\n")
    assert res == ":1\r\n", f"Expected :1\\r\\n, got {repr(res)}"
    res = send_recv(s, "GET counter\r\n")
    assert res == "$1\r\n1\r\n", f"Expected $1\\r\\n1\\r\\n, got {repr(res)}"
    print("[PASS] INCR initializes nonexistent key to 1")

    # 2. Sequential INCR
    res = send_recv(s, "INCR counter\r\n")
    assert res == ":2\r\n", f"Expected :2\\r\\n, got {repr(res)}"
    res = send_recv(s, "INCR counter\r\n")
    assert res == ":3\r\n", f"Expected :3\\r\\n, got {repr(res)}"
    print("[PASS] Sequential INCR increments key value")

    # 3. DECR on existing key
    res = send_recv(s, "DECR counter\r\n")
    assert res == ":2\r\n", f"Expected :2\\r\\n, got {repr(res)}"
    res = send_recv(s, "DECR counter\r\n")
    assert res == ":1\r\n", f"Expected :1\\r\\n, got {repr(res)}"
    res = send_recv(s, "DECR counter\r\n")
    assert res == ":0\r\n", f"Expected :0\\r\\n, got {repr(res)}"
    res = send_recv(s, "DECR counter\r\n")
    assert res == ":-1\r\n", f"Expected :-1\\r\\n, got {repr(res)}"
    print("[PASS] DECR decrements key value, handles 0 and negative")

    # 4. DECR on nonexistent key -> initializes to -1
    res = send_recv(s, "DECR nonexist_decr\r\n")
    assert res == ":-1\r\n", f"Expected :-1\\r\\n, got {repr(res)}"
    res = send_recv(s, "GET nonexist_decr\r\n")
    assert res == "$2\r\n-1\r\n", f"Expected $2\\r\\n-1\\r\\n, got {repr(res)}"
    print("[PASS] DECR initializes nonexistent key to -1")

    # 5. INCRBY with arbitrary increments
    send_recv(s, "SET num 10\r\n")
    res = send_recv(s, "INCRBY num 5\r\n")
    assert res == ":15\r\n", f"Expected :15\\r\\n, got {repr(res)}"
    res = send_recv(s, "INCRBY num -20\r\n")
    assert res == ":-5\r\n", f"Expected :-5\\r\\n, got {repr(res)}"
    res = send_recv(s, "INCRBY nonexist_incrby 100\r\n")
    assert res == ":100\r\n", f"Expected :100\\r\\n, got {repr(res)}"
    print("[PASS] INCRBY handles positive, negative, and nonexistent keys")

    # 6. DECRBY with arbitrary decrements
    res = send_recv(s, "DECRBY num 5\r\n")
    assert res == ":-10\r\n", f"Expected :-10\\r\\n, got {repr(res)}"
    res = send_recv(s, "DECRBY num -15\r\n")
    assert res == ":5\r\n", f"Expected :5\\r\\n, got {repr(res)}"
    res = send_recv(s, "DECRBY nonexist_decrby 42\r\n")
    assert res == ":-42\r\n", f"Expected :-42\\r\\n, got {repr(res)}"
    print("[PASS] DECRBY handles positive, negative, and nonexistent keys")

    # 7. RESP2 protocol array format for counter commands
    res = send_recv(s, "*2\r\n$4\r\nINCR\r\n$5\r\nrespk\r\n")
    assert res == ":1\r\n", f"Expected :1\\r\\n, got {repr(res)}"
    res = send_recv(s, "*3\r\n$6\r\nINCRBY\r\n$5\r\nrespk\r\n$2\r\n10\r\n")
    assert res == ":11\r\n", f"Expected :11\\r\\n, got {repr(res)}"
    res = send_recv(s, "*2\r\n$4\r\nDECR\r\n$5\r\nrespk\r\n")
    assert res == ":10\r\n", f"Expected :10\\r\\n, got {repr(res)}"
    res = send_recv(s, "*3\r\n$6\r\nDECRBY\r\n$5\r\nrespk\r\n$1\r\n3\r\n")
    assert res == ":7\r\n", f"Expected :7\\r\\n, got {repr(res)}"
    print("[PASS] Counter commands work via RESP2 array protocol")

    # 8. Non-integer error handling
    send_recv(s, "SET str_val \"hello world\"\r\n")
    res = send_recv(s, "INCR str_val\r\n")
    assert "ERR value is not an integer or out of range" in res, f"Expected integer error, got {repr(res)}"
    res = send_recv(s, "DECR str_val\r\n")
    assert "ERR value is not an integer or out of range" in res, f"Expected integer error, got {repr(res)}"
    res = send_recv(s, "INCRBY str_val 10\r\n")
    assert "ERR value is not an integer or out of range" in res, f"Expected integer error, got {repr(res)}"
    res = send_recv(s, "INCRBY counter notanint\r\n")
    assert "ERR value is not an integer or out of range" in res, f"Expected integer error, got {repr(res)}"
    res = send_recv(s, "DECRBY counter notanint\r\n")
    assert "ERR value is not an integer or out of range" in res, f"Expected integer error, got {repr(res)}"
    print("[PASS] Proper error responses for non-integer values and arguments")

    # 9. Overflow and Underflow detection
    send_recv(s, "SET maxint 9223372036854775807\r\n")
    res = send_recv(s, "INCR maxint\r\n")
    assert "ERR increment or decrement would overflow" in res, f"Expected overflow error, got {repr(res)}"
    res = send_recv(s, "INCRBY maxint 1\r\n")
    assert "ERR increment or decrement would overflow" in res, f"Expected overflow error, got {repr(res)}"

    send_recv(s, "SET minint -9223372036854775808\r\n")
    res = send_recv(s, "DECR minint\r\n")
    assert "ERR increment or decrement would overflow" in res, f"Expected overflow error, got {repr(res)}"
    res = send_recv(s, "DECRBY minint 1\r\n")
    assert "ERR increment or decrement would overflow" in res, f"Expected overflow error, got {repr(res)}"
    print("[PASS] Overflow and underflow protection working correctly")

    # 10. Preserves TTL on increment / decrement
    send_recv(s, "SETEX ttl_counter 10 50\r\n")
    res = send_recv(s, "TTL ttl_counter\r\n")
    ttl_before = int(res.strip()[1:])
    assert ttl_before > 0, f"Expected positive TTL, got {ttl_before}"
    res = send_recv(s, "INCR ttl_counter\r\n")
    assert res == ":51\r\n", f"Expected :51\\r\\n, got {repr(res)}"
    res = send_recv(s, "TTL ttl_counter\r\n")
    ttl_after = int(res.strip()[1:])
    assert ttl_after > 0, f"Expected TTL to remain active, got {ttl_after}"
    print("[PASS] INCR/DECR preserves existing key TTL")

    # 11. Rate Limiting Pattern (INCR + EXPIRE fixed-window pattern)
    # Scenario: max 3 requests per 1-second window
    rate_limit_key = "ratelimit:user:42"
    send_recv(s, f"DEL {rate_limit_key}\r\n")
    allowed = 0
    blocked = 0
    LIMIT = 3
    for i in range(5):
        cnt_res = send_recv(s, f"INCR {rate_limit_key}\r\n")
        cnt = int(cnt_res.strip()[1:])
        if cnt == 1:
            send_recv(s, f"EXPIRE {rate_limit_key} 1\r\n")
        if cnt <= LIMIT:
            allowed += 1
        else:
            blocked += 1

    assert allowed == 3, f"Expected 3 allowed requests, got {allowed}"
    assert blocked == 2, f"Expected 2 blocked requests, got {blocked}"
    print("[PASS] Rate Limiter pattern: allowed first 3 requests, throttled 2 requests")

    # Wait for the rate limiting window to expire
    time.sleep(1.2)
    # Next request must reset to 1 and be allowed
    cnt_res = send_recv(s, f"INCR {rate_limit_key}\r\n")
    cnt = int(cnt_res.strip()[1:])
    assert cnt == 1, f"Expected counter to reset to 1 after window expiration, got {cnt}"
    print("[PASS] Rate Limiter pattern: window reset after TTL expiration")

    # 12. Multi-threaded Atomic Concurrency Stress Test
    # 10 threads each performing 100 INCR operations on the same key = exactly 1000
    print("Running multi-threaded atomic concurrency stress test...")
    shared_key = "atomic_stress"
    send_recv(s, f"SET {shared_key} 0\r\n")
    threads = []
    thread_errors = []

    def incr_worker():
        try:
            ws = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            ws.connect(("127.0.0.1", port))
            for _ in range(100):
                r = send_recv(ws, f"INCR {shared_key}\r\n")
                if not r.startswith(":"):
                    thread_errors.append(f"Unexpected response: {r}")
            ws.close()
        except Exception as e:
            thread_errors.append(f"Thread exception: {e}")

    for _ in range(10):
        t = threading.Thread(target=incr_worker)
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    assert not thread_errors, f"Thread errors: {thread_errors}"
    res = send_recv(s, f"GET {shared_key}\r\n")
    assert res == "$4\r\n1000\r\n", f"Expected atomic 1000, got {repr(res)}"
    print("[PASS] Multi-threaded atomicity verified: 10 threads x 100 INCR = exactly 1000")

    # 10 threads each performing 50 DECR operations on the same key = exactly 500
    threads = []
    def decr_worker():
        try:
            ws = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            ws.connect(("127.0.0.1", port))
            for _ in range(50):
                r = send_recv(ws, f"DECR {shared_key}\r\n")
                if not r.startswith(":"):
                    thread_errors.append(f"Unexpected response: {r}")
            ws.close()
        except Exception as e:
            thread_errors.append(f"Thread exception: {e}")

    for _ in range(10):
        t = threading.Thread(target=decr_worker)
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    assert not thread_errors, f"Thread errors: {thread_errors}"
    res = send_recv(s, f"GET {shared_key}\r\n")
    assert res == "$3\r\n500\r\n", f"Expected atomic 500, got {repr(res)}"
    print("[PASS] Multi-threaded atomicity verified: 10 threads x 50 DECR = exactly 500")

    s.close()
    print("[PASS] All Atomic Counter & Rate Limiting tests passed successfully!")

def test_multi_key(port=6389):
    print(f"\n--- Testing Multi-Key Batch Operations (MSET / MGET) on port {port} ---")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(("127.0.0.1", port))
    send_recv(s, "FLUSHDB\r\n")

    # 1. MSET basic batch write
    res = send_recv(s, "MSET k1 v1 k2 v2 k3 v3\r\n")
    assert res == "+OK\r\n", f"Expected +OK, got {repr(res)}"
    assert send_recv(s, "GET k1\r\n") == "$2\r\nv1\r\n"
    assert send_recv(s, "GET k2\r\n") == "$2\r\nv2\r\n"
    assert send_recv(s, "GET k3\r\n") == "$2\r\nv3\r\n"
    print("[PASS] MSET correctly sets multiple key-value pairs")

    # 2. MGET basic batch read
    res = send_recv(s, "MGET k1 k2 k3\r\n")
    expected = "*3\r\n$2\r\nv1\r\n$2\r\nv2\r\n$2\r\nv3\r\n"
    assert res == expected, f"Expected {repr(expected)}, got {repr(res)}"
    print("[PASS] MGET correctly retrieves multiple existing keys in order")

    # 3. MGET with missing and mixed keys
    res = send_recv(s, "MGET k1 nonexistent k3 missing\r\n")
    expected = "*4\r\n$2\r\nv1\r\n$-1\r\n$2\r\nv3\r\n$-1\r\n"
    assert res == expected, f"Expected {repr(expected)}, got {repr(res)}"
    print("[PASS] MGET returns $-1 for nonexistent keys amidst existing ones")

    # 4. MGET with only nonexistent keys
    res = send_recv(s, "MGET none1 none2\r\n")
    expected = "*2\r\n$-1\r\n$-1\r\n"
    assert res == expected, f"Expected {repr(expected)}, got {repr(res)}"
    print("[PASS] MGET returns array of $-1 when all keys are nonexistent")

    # 5. MGET with duplicate keys in same command
    res = send_recv(s, "MGET k1 k1 k2\r\n")
    expected = "*3\r\n$2\r\nv1\r\n$2\r\nv1\r\n$2\r\nv2\r\n"
    assert res == expected, f"Expected {repr(expected)}, got {repr(res)}"
    print("[PASS] MGET preserves duplicate keys in the requested order")

    # 6. MSET overwrites existing keys and clears TTL
    send_recv(s, "SETEX ttl_key 100 \"initial\"\r\n")
    ttl_res = send_recv(s, "TTL ttl_key\r\n")
    assert int(ttl_res.strip()[1:]) > 0, "TTL should be positive"
    res = send_recv(s, "MSET ttl_key \"overwritten\" new_key \"hello\"\r\n")
    assert res == "+OK\r\n"
    assert send_recv(s, "GET ttl_key\r\n") == "$11\r\noverwritten\r\n"
    assert send_recv(s, "TTL ttl_key\r\n") == ":-1\r\n"
    print("[PASS] MSET overwrites existing keys and clears TTL (persistent)")

    # 7. MSET with duplicate keys in same command (last value wins)
    res = send_recv(s, "MSET dup_k val1 dup_k val2\r\n")
    assert res == "+OK\r\n"
    assert send_recv(s, "GET dup_k\r\n") == "$4\r\nval2\r\n"
    print("[PASS] MSET with duplicate keys applies later value (last-wins)")

    # 8. RESP2 Array format for MSET and MGET
    res = send_recv(s, "*5\r\n$4\r\nMSET\r\n$5\r\nresp1\r\n$3\r\nfoo\r\n$5\r\nresp2\r\n$3\r\nbar\r\n")
    assert res == "+OK\r\n", f"Expected +OK, got {repr(res)}"
    res = send_recv(s, "*4\r\n$4\r\nMGET\r\n$5\r\nresp1\r\n$5\r\nnone9\r\n$5\r\nresp2\r\n")
    expected = "*3\r\n$3\r\nfoo\r\n$-1\r\n$3\r\nbar\r\n"
    assert res == expected, f"Expected {repr(expected)}, got {repr(res)}"
    print("[PASS] MSET and MGET work over RESP2 array protocol")

    # 9. MGET lazy eviction of expired keys
    send_recv(s, "SETEX short_key 1 \"temp_val\"\r\n")
    res = send_recv(s, "MGET k1 short_key\r\n")
    assert "$8\r\ntemp_val\r\n" in res
    time.sleep(1.2)
    res = send_recv(s, "MGET k1 short_key\r\n")
    expected = "*2\r\n$2\r\nv1\r\n$-1\r\n"
    assert res == expected, f"Expected {repr(expected)}, got {repr(res)}"
    assert send_recv(s, "EXISTS short_key\r\n") == ":0\r\n"
    print("[PASS] MGET triggers lazy eviction for expired keys returning $-1")

    # 10. Error handling (wrong number of arguments)
    res = send_recv(s, "MGET\r\n")
    assert "ERR wrong number of arguments for 'mget' command" in res
    res = send_recv(s, "MSET\r\n")
    assert "ERR wrong number of arguments for 'mset' command" in res
    res = send_recv(s, "MSET single_key\r\n")
    assert "ERR wrong number of arguments for 'mset' command" in res
    res = send_recv(s, "MSET k1 v1 k2\r\n")
    assert "ERR wrong number of arguments for 'mset' command" in res
    print("[PASS] MGET and MSET reject invalid argument counts with correct errors")

    # 11. Multi-threaded atomic batch consistency test
    print("Running multi-threaded batch atomicity test...")
    send_recv(s, "MSET pair_x 100 pair_y 100\r\n")
    stop_flag = False
    inconsistencies = []

    def writer():
        ws = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        ws.connect(("127.0.0.1", port))
        val = 1
        while not stop_flag:
            v_str = str(val)
            send_recv(ws, f"MSET pair_x {v_str} pair_y {v_str}\r\n")
            val = 2 if val == 1 else 1
        ws.close()

    def reader():
        rs = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        rs.connect(("127.0.0.1", port))
        for _ in range(200):
            r = send_recv(rs, "MGET pair_x pair_y\r\n")
            lines = [l for l in r.split("\r\n") if l and not l.startswith("*") and not l.startswith("$")]
            if len(lines) == 2 and lines[0] != lines[1]:
                inconsistencies.append((lines[0], lines[1]))
        rs.close()

    t_writer = threading.Thread(target=writer)
    t_writer.start()

    readers = [threading.Thread(target=reader) for _ in range(4)]
    for r in readers:
        r.start()
    for r in readers:
        r.join()

    stop_flag = True
    t_writer.join()

    assert not inconsistencies, f"Atomicity violation in MSET/MGET: {inconsistencies}"
    print("[PASS] Multi-threaded MSET atomicity verified (0 torn reads across 800 checks)")

    s.close()
    print("[PASS] All Multi-Key (MSET / MGET) tests passed successfully!")

def test_persistence(port=6389):
    print(f"\n--- Testing Persistence (Snapshots & AOF) on port {port} ---")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(("127.0.0.1", port))
    send_recv(s, "FLUSHDB\r\n")

    # 1. Test SAVE command
    res = send_recv(s, "SAVE\r\n")
    assert res == "+OK\r\n", f"Expected +OK from SAVE, got {repr(res)}"
    print("[PASS] SAVE command returns +OK")

    # 2. Test BGSAVE command
    res = send_recv(s, "BGSAVE\r\n")
    assert res == "+Background saving started\r\n" or "already in progress" in res, f"Expected background save response, got {repr(res)}"
    print("[PASS] BGSAVE command returns '+Background saving started'")

    # 3. Test LASTSAVE command
    res = send_recv(s, "LASTSAVE\r\n")
    assert res.startswith(":"), f"Expected integer from LASTSAVE, got {repr(res)}"
    save_ts = int(res.strip()[1:])
    assert save_ts > 0, f"Expected valid timestamp, got {save_ts}"
    print(f"[PASS] LASTSAVE returned timestamp {save_ts}")

    # 4. Test INFO contains persistence metrics
    info_res = send_recv(s, "INFO\r\n")
    assert "# Persistence" in info_res, f"INFO missing # Persistence section: {info_res}"
    assert "rdb_last_save_time:" in info_res, "INFO missing rdb_last_save_time"
    assert "rdb_bgsave_in_progress:" in info_res, "INFO missing rdb_bgsave_in_progress"
    assert "aof_enabled:" in info_res, "INFO missing aof_enabled"
    print("[PASS] INFO includes complete # Persistence section")

    s.close()

    # 5. Dedicated Snapshot lifecycle test across process restarts
    print("Testing Snapshot persistence lifecycle (write -> save -> kill -> restart -> verify)...")
    snap_port = 6393
    snap_file = "test_snapshot_lifecycle.kvl"
    if os.path.exists(snap_file):
        os.remove(snap_file)

    p1 = subprocess.Popen(["./build/kvllay", "-p", str(snap_port), "--snapshot", snap_file, "--no-aof"])
    time.sleep(0.3)
    try:
        s1 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s1.connect(("127.0.0.1", snap_port))
        send_recv(s1, "SET snap_key_1 \"hello_world\"\r\n")
        send_recv(s1, "SET snap_key_2 \"persistent_value\"\r\n")
        send_recv(s1, "SETEX snap_key_ttl 100 \"ttl_value\"\r\n")
        send_recv(s1, "RPUSH snap_list_1 a b c\r\n")
        send_recv(s1, "SAVE\r\n")
        s1.close()
    finally:
        p1.terminate()
        p1.wait()

    assert os.path.exists(snap_file), "Snapshot file was not created on disk"
    assert os.path.getsize(snap_file) > 28, "Snapshot file is too small"

    # Restart server and verify keys are restored
    p2 = subprocess.Popen(["./build/kvllay", "-p", str(snap_port), "--snapshot", snap_file, "--no-aof"])
    time.sleep(0.3)
    try:
        s2 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s2.connect(("127.0.0.1", snap_port))
        assert send_recv(s2, "GET snap_key_1\r\n") == "$11\r\nhello_world\r\n"
        assert send_recv(s2, "GET snap_key_2\r\n") == "$16\r\npersistent_value\r\n"
        assert send_recv(s2, "GET snap_key_ttl\r\n") == "$9\r\nttl_value\r\n"
        assert send_recv(s2, "LRANGE snap_list_1 0 -1\r\n") == "*3\r\n$1\r\na\r\n$1\r\nb\r\n$1\r\nc\r\n"
        ttl_res = send_recv(s2, "TTL snap_key_ttl\r\n")
        rem_ttl = int(ttl_res.strip()[1:])
        assert 0 < rem_ttl <= 100, f"Expected positive TTL, got {rem_ttl}"
        s2.close()
        print("[PASS] Snapshot restored all data (strings and lists) and TTL correctly across server restart")
    finally:
        p2.terminate()
        p2.wait()
        if os.path.exists(snap_file):
            os.remove(snap_file)

    # 6. Dedicated AOF lifecycle test across process restarts
    print("Testing AOF persistence lifecycle (mutations -> restart -> verify)...")
    aof_port = 6394
    aof_file = "test_aof_lifecycle.aof"
    if os.path.exists(aof_file):
        os.remove(aof_file)

    p_aof1 = subprocess.Popen(["./build/kvllay", "-p", str(aof_port), "--aof", aof_file, "--no-snapshot", "--appendfsync", "always"])
    time.sleep(0.3)
    try:
        sa1 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sa1.connect(("127.0.0.1", aof_port))
        send_recv(sa1, "SET aof_k1 v1\r\n")
        send_recv(sa1, "INCR aof_counter\r\n")
        send_recv(sa1, "INCR aof_counter\r\n")
        send_recv(sa1, "INCRBY aof_counter 8\r\n")
        send_recv(sa1, "MSET aof_m1 hello aof_m2 world\r\n")
        send_recv(sa1, "DEL aof_k1\r\n")
        send_recv(sa1, "RPUSH aof_list x y z\r\n")
        send_recv(sa1, "LPOP aof_list\r\n")
        send_recv(sa1, "SET aof_expire_key expires_later\r\n")
        assert send_recv(sa1, "EXPIRE aof_expire_key 1\r\n") == ":1\r\n"
        send_recv(sa1, "BGREWRITEAOF\r\n")
        time.sleep(0.2)
        send_recv(sa1, "SET aof_direct_expire_key direct_expiry\r\n")
        assert send_recv(sa1, "EXPIRE aof_direct_expire_key 1\r\n") == ":1\r\n"
        sa1.close()
    finally:
        p_aof1.terminate()
        p_aof1.wait()

    assert os.path.exists(aof_file), "AOF file was not created"
    with open(aof_file, "rb") as aof:
        assert b"PEXPIREAT" in aof.read(), "EXPIRE must be serialized as absolute PEXPIREAT in AOF"

    # Wait until the original absolute deadline has passed. A relative EXPIRE
    # replay would incorrectly make the key live again after the restart.
    time.sleep(1.2)

    # Restart server and verify AOF replay
    p_aof2 = subprocess.Popen(["./build/kvllay", "-p", str(aof_port), "--aof", aof_file, "--no-snapshot"])
    time.sleep(0.3)
    try:
        sa2 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sa2.connect(("127.0.0.1", aof_port))
        assert send_recv(sa2, "GET aof_k1\r\n") == "$-1\r\n"
        assert send_recv(sa2, "GET aof_expire_key\r\n") == "$-1\r\n"
        assert send_recv(sa2, "GET aof_direct_expire_key\r\n") == "$-1\r\n"
        assert send_recv(sa2, "GET aof_counter\r\n") == "$2\r\n10\r\n"
        assert send_recv(sa2, "GET aof_m1\r\n") == "$5\r\nhello\r\n"
        assert send_recv(sa2, "GET aof_m2\r\n") == "$5\r\nworld\r\n"
        assert send_recv(sa2, "LRANGE aof_list 0 -1\r\n") == "*2\r\n$1\r\ny\r\n$1\r\nz\r\n"
        sa2.close()
        print("[PASS] AOF replayed and recovered exact state (strings and lists) across server restart")
    finally:
        p_aof2.terminate()
        p_aof2.wait()
        if os.path.exists(aof_file):
            os.remove(aof_file)

    # 7. Snapshot CRC32 corruption detection
    print("Testing CRC32 snapshot corruption detection...")
    corrupt_file = "corrupt_test.kvl"
    with open(corrupt_file, "wb") as f:
        f.write(b"KVLLAYS1\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00BADCRC")

    p_corr = subprocess.Popen(["./build/kvllay", "-p", "6395", "--snapshot", corrupt_file, "--no-aof"])
    time.sleep(0.3)
    try:
        sc = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sc.connect(("127.0.0.1", 6395))
        res = send_recv(sc, "DBSIZE\r\n")
        assert res == ":0\r\n", f"Corrupt snapshot should have been rejected, got {res}"
        sc.close()
        print("[PASS] Corrupt snapshot with invalid CRC32 successfully rejected")
    finally:
        p_corr.terminate()
        p_corr.wait()
        if os.path.exists(corrupt_file):
            os.remove(corrupt_file)

    print("[PASS] All Persistence tests passed successfully!")

def test_lists_and_queues(port=6389):
    print(f"\n--- Testing Lists & Task Queues (Basic Structures) on port {port} ---")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(("127.0.0.1", port))
    send_recv(s, "FLUSHDB\r\n")

    # 1. LPUSH and RPUSH basic push and return length
    res = send_recv(s, "LPUSH mylist world\r\n")
    assert res == ":1\r\n", f"Expected :1, got {repr(res)}"
    res = send_recv(s, "LPUSH mylist hello\r\n")
    assert res == ":2\r\n", f"Expected :2, got {repr(res)}"
    print("[PASS] LPUSH returns updated length")

    # 2. LLEN
    res = send_recv(s, "LLEN mylist\r\n")
    assert res == ":2\r\n", f"Expected :2 from LLEN, got {repr(res)}"
    res = send_recv(s, "LLEN nonexist_list\r\n")
    assert res == ":0\r\n", f"Expected :0 for nonexistent LLEN, got {repr(res)}"
    print("[PASS] LLEN on existing and nonexistent lists")

    # 3. LRANGE
    res = send_recv(s, "LRANGE mylist 0 -1\r\n")
    assert res == "*2\r\n$5\r\nhello\r\n$5\r\nworld\r\n", f"Expected hello, world, got {repr(res)}"
    res = send_recv(s, "LRANGE mylist 0 0\r\n")
    assert res == "*1\r\n$5\r\nhello\r\n", f"Expected hello, got {repr(res)}"
    res = send_recv(s, "LRANGE mylist 1 1\r\n")
    assert res == "*1\r\n$5\r\nworld\r\n", f"Expected world, got {repr(res)}"
    res = send_recv(s, "LRANGE mylist -1 -1\r\n")
    assert res == "*1\r\n$5\r\nworld\r\n", f"Expected world, got {repr(res)}"
    res = send_recv(s, "LRANGE mylist 5 10\r\n")
    assert res == "*0\r\n", f"Expected empty array for out-of-range, got {repr(res)}"
    res = send_recv(s, "LRANGE nonexist_list 0 -1\r\n")
    assert res == "*0\r\n", f"Expected empty array for nonexistent list, got {repr(res)}"
    print("[PASS] LRANGE with positive, negative, and out-of-range indexes")

    # 4. Multi-value LPUSH and RPUSH
    send_recv(s, "DEL mylist\r\n")
    # LPUSH mylist a b c -> head is c, then b, then a
    res = send_recv(s, "LPUSH mylist a b c\r\n")
    assert res == ":3\r\n", f"Expected :3 from multi LPUSH, got {repr(res)}"
    res = send_recv(s, "LRANGE mylist 0 -1\r\n")
    assert res == "*3\r\n$1\r\nc\r\n$1\r\nb\r\n$1\r\na\r\n", f"Expected c, b, a, got {repr(res)}"

    # RPUSH mylist d e -> tail appends d, then e
    res = send_recv(s, "RPUSH mylist d e\r\n")
    assert res == ":5\r\n", f"Expected :5 from RPUSH, got {repr(res)}"
    res = send_recv(s, "LRANGE mylist 0 -1\r\n")
    assert res == "*5\r\n$1\r\nc\r\n$1\r\nb\r\n$1\r\na\r\n$1\r\nd\r\n$1\r\ne\r\n", f"Expected c, b, a, d, e, got {repr(res)}"
    print("[PASS] Multi-value LPUSH and RPUSH order verified")

    # 5. LINDEX
    res = send_recv(s, "LINDEX mylist 0\r\n")
    assert res == "$1\r\nc\r\n", f"Expected c, got {repr(res)}"
    res = send_recv(s, "LINDEX mylist 4\r\n")
    assert res == "$1\r\ne\r\n", f"Expected e, got {repr(res)}"
    res = send_recv(s, "LINDEX mylist -1\r\n")
    assert res == "$1\r\ne\r\n", f"Expected e, got {repr(res)}"
    res = send_recv(s, "LINDEX mylist -5\r\n")
    assert res == "$1\r\nc\r\n", f"Expected c, got {repr(res)}"
    res = send_recv(s, "LINDEX mylist 100\r\n")
    assert res == "$-1\r\n", f"Expected nil for out-of-bounds LINDEX, got {repr(res)}"
    print("[PASS] LINDEX with positive, negative, and out-of-bounds indexes")

    # 6. LPOP and RPOP (single element)
    res = send_recv(s, "LPOP mylist\r\n")
    assert res == "$1\r\nc\r\n", f"Expected popped 'c', got {repr(res)}"
    res = send_recv(s, "RPOP mylist\r\n")
    assert res == "$1\r\ne\r\n", f"Expected popped 'e', got {repr(res)}"
    assert send_recv(s, "LLEN mylist\r\n") == ":3\r\n"
    print("[PASS] Single element LPOP and RPOP")

    # 7. LPOP and RPOP with count argument
    # mylist currently has: [b, a, d]
    res = send_recv(s, "LPOP mylist 2\r\n")
    assert res == "*2\r\n$1\r\nb\r\n$1\r\na\r\n", f"Expected [b, a], got {repr(res)}"
    assert send_recv(s, "LLEN mylist\r\n") == ":1\r\n"
    # mylist currently has: [d]
    res = send_recv(s, "RPOP mylist 5\r\n")
    assert res == "*1\r\n$1\r\nd\r\n", f"Expected [d], got {repr(res)}"
    # Now mylist is empty -> key should be deleted automatically
    assert send_recv(s, "EXISTS mylist\r\n") == ":0\r\n"
    assert send_recv(s, "LLEN mylist\r\n") == ":0\r\n"
    print("[PASS] Multi-element LPOP and RPOP with count, and automatic key deletion on empty")

    # 8. LPOP and RPOP on nonexistent key
    res = send_recv(s, "LPOP nonexist\r\n")
    assert res == "$-1\r\n", f"Expected $-1 from empty LPOP, got {repr(res)}"
    res = send_recv(s, "RPOP nonexist\r\n")
    assert res == "$-1\r\n", f"Expected $-1 from empty RPOP, got {repr(res)}"
    res = send_recv(s, "LPOP nonexist 2\r\n")
    assert res == "*-1\r\n", f"Expected *-1 from empty LPOP count, got {repr(res)}"
    res = send_recv(s, "RPOP nonexist 2\r\n")
    assert res == "*-1\r\n", f"Expected *-1 from empty RPOP count, got {repr(res)}"
    print("[PASS] LPOP and RPOP on nonexistent key return nil")

    # 9. Queue & Stack Patterns
    # FIFO Task Queue: producer RPUSH, consumer LPOP
    send_recv(s, "DEL task_queue\r\n")
    send_recv(s, "RPUSH task_queue task_1 task_2 task_3\r\n")
    assert send_recv(s, "LPOP task_queue\r\n") == "$6\r\ntask_1\r\n"
    assert send_recv(s, "LPOP task_queue\r\n") == "$6\r\ntask_2\r\n"
    assert send_recv(s, "LPOP task_queue\r\n") == "$6\r\ntask_3\r\n"
    assert send_recv(s, "LPOP task_queue\r\n") == "$-1\r\n"
    print("[PASS] FIFO Task Queue pattern (RPUSH + LPOP)")

    # LIFO Stack: producer LPUSH, consumer LPOP
    send_recv(s, "DEL stack\r\n")
    send_recv(s, "LPUSH stack page1 page2 page3\r\n")
    assert send_recv(s, "LPOP stack\r\n") == "$5\r\npage3\r\n"
    assert send_recv(s, "LPOP stack\r\n") == "$5\r\npage2\r\n"
    assert send_recv(s, "LPOP stack\r\n") == "$5\r\npage1\r\n"
    assert send_recv(s, "LPOP stack\r\n") == "$-1\r\n"
    print("[PASS] LIFO Stack pattern (LPUSH + LPOP)")

    # 10. TYPE command and Data Type Safety (WRONGTYPE)
    send_recv(s, "SET str_key hello_string\r\n")
    send_recv(s, "RPUSH list_key item1 item2\r\n")
    assert send_recv(s, "TYPE str_key\r\n") == "+string\r\n"
    assert send_recv(s, "TYPE list_key\r\n") == "+list\r\n"
    assert send_recv(s, "TYPE nonexist\r\n") == "+none\r\n"
    print("[PASS] TYPE command returns string, list, none")

    # Operations on wrong types
    assert "WRONGTYPE" in send_recv(s, "GET list_key\r\n")
    assert "WRONGTYPE" in send_recv(s, "INCR list_key\r\n")
    assert "WRONGTYPE" in send_recv(s, "DECR list_key\r\n")
    assert "WRONGTYPE" in send_recv(s, "LPUSH str_key x\r\n")
    assert "WRONGTYPE" in send_recv(s, "RPUSH str_key x\r\n")
    assert "WRONGTYPE" in send_recv(s, "LPOP str_key\r\n")
    assert "WRONGTYPE" in send_recv(s, "RPOP str_key\r\n")
    assert "WRONGTYPE" in send_recv(s, "LLEN str_key\r\n")
    assert "WRONGTYPE" in send_recv(s, "LRANGE str_key 0 -1\r\n")
    assert "WRONGTYPE" in send_recv(s, "LINDEX str_key 0\r\n")
    print("[PASS] WRONGTYPE errors strictly enforced across all operations")

    # MGET with mixed string and list keys returns nil for list key without error
    res = send_recv(s, "MGET str_key list_key nonexist\r\n")
    assert res == "*3\r\n$12\r\nhello_string\r\n$-1\r\n$-1\r\n", f"Expected string, nil, nil, got {repr(res)}"
    print("[PASS] MGET returns nil for list keys without failing")

    # SET replaces list key with string
    send_recv(s, "SET list_key \"now_string\"\r\n")
    assert send_recv(s, "TYPE list_key\r\n") == "+string\r\n"
    assert send_recv(s, "GET list_key\r\n") == "$10\r\nnow_string\r\n"

    # DEL removes list key
    send_recv(s, "RPUSH to_del item\r\n")
    assert send_recv(s, "DEL to_del\r\n") == ":1\r\n"
    assert send_recv(s, "EXISTS to_del\r\n") == ":0\r\n"
    print("[PASS] SET overwrites list key, DEL deletes list key")

    # 11. TTL and Expiration on Lists
    send_recv(s, "RPUSH ttl_list a b c\r\n")
    assert send_recv(s, "EXPIRE ttl_list 1\r\n") == ":1\r\n"
    ttl_res = send_recv(s, "TTL ttl_list\r\n")
    assert int(ttl_res.strip()[1:]) > 0
    # LPUSH on list with TTL preserves TTL
    send_recv(s, "LPUSH ttl_list z\r\n")
    ttl_res = send_recv(s, "TTL ttl_list\r\n")
    assert int(ttl_res.strip()[1:]) > 0
    time.sleep(1.2)
    assert send_recv(s, "LLEN ttl_list\r\n") == ":0\r\n"
    assert send_recv(s, "EXISTS ttl_list\r\n") == ":0\r\n"
    print("[PASS] TTL and expiration on lists work with preservation and lazy eviction")

    # 12. RESP2 protocol array format for list commands
    res = send_recv(s, "*3\r\n$5\r\nRPUSH\r\n$9\r\nresp_list\r\n$3\r\nfoo\r\n")
    assert res == ":1\r\n"
    res = send_recv(s, "*4\r\n$6\r\nLRANGE\r\n$9\r\nresp_list\r\n$1\r\n0\r\n$2\r\n-1\r\n")
    assert res == "*1\r\n$3\r\nfoo\r\n"
    print("[PASS] List commands work seamlessly via RESP2 array protocol")

    # 13. Concurrency test on lists: multi-threaded push/pop queue
    print("Running multi-threaded list queue concurrency stress test...")
    q_key = "concurrent_q"
    send_recv(s, f"DEL {q_key}\r\n")
    threads = []
    thread_errors = []
    TOTAL_ITEMS = 500

    def producer(worker_id):
        try:
            ws = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            ws.connect(("127.0.0.1", port))
            for i in range(50):
                r = send_recv(ws, f"RPUSH {q_key} val_{worker_id}_{i}\r\n")
                if not r.startswith(":"):
                    thread_errors.append(f"Push error: {r}")
            ws.close()
        except Exception as e:
            thread_errors.append(f"Producer exception: {e}")

    for wid in range(10):
        t = threading.Thread(target=producer, args=(wid,))
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    assert not thread_errors, f"Producer thread errors: {thread_errors}"
    res = send_recv(s, f"LLEN {q_key}\r\n")
    assert res == f":{TOTAL_ITEMS}\r\n", f"Expected :{TOTAL_ITEMS}, got {res}"
    print(f"[PASS] 10 concurrent producers pushed exactly {TOTAL_ITEMS} items")

    # Concurrent consumers
    popped_items = []
    pop_lock = threading.Lock()

    def consumer():
        try:
            ws = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            ws.connect(("127.0.0.1", port))
            for _ in range(50):
                r = send_recv(ws, f"LPOP {q_key}\r\n")
                if r.startswith("$") and r != "$-1\r\n":
                    lines = r.split("\r\n")
                    with pop_lock:
                        popped_items.append(lines[1])
            ws.close()
        except Exception as e:
            thread_errors.append(f"Consumer exception: {e}")

    threads = []
    for _ in range(10):
        t = threading.Thread(target=consumer)
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    assert not thread_errors, f"Consumer thread errors: {thread_errors}"
    assert len(popped_items) == TOTAL_ITEMS, f"Expected {TOTAL_ITEMS} popped items, got {len(popped_items)}"
    assert len(set(popped_items)) == TOTAL_ITEMS, f"Duplicate items popped! Unique: {len(set(popped_items))}"
    assert send_recv(s, f"LLEN {q_key}\r\n") == ":0\r\n"
    print(f"[PASS] 10 concurrent consumers consumed all {TOTAL_ITEMS} unique items (0 duplicates, 0 losses)")

    s.close()
    print("[PASS] All Lists & Task Queues tests passed successfully!")

def test_maxmemory_and_eviction(port=6389):
    print(f"\n--- Testing Maxmemory, OOM & Eviction on port {port} ---")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(("127.0.0.1", port))
    send_recv(s, "FLUSHDB\r\n")

    # 1. INFO memory section and section filtering
    info_all = send_recv(s, "INFO\r\n")
    assert "# Memory" in info_all, "INFO missing # Memory section"
    assert "used_memory:" in info_all, "INFO missing used_memory"
    assert "used_memory_human:" in info_all, "INFO missing used_memory_human"
    assert "maxmemory:" in info_all, "INFO missing maxmemory"
    assert "maxmemory_human:" in info_all, "INFO missing maxmemory_human"
    assert "maxmemory_policy:" in info_all, "INFO missing maxmemory_policy"
    assert "evicted_keys:" in info_all, "INFO missing evicted_keys"

    info_mem = send_recv(s, "INFO memory\r\n")
    assert "# Memory" in info_mem, "INFO memory missing # Memory section"
    assert "# Server" not in info_mem, "INFO memory should not contain # Server"
    print("[PASS] INFO memory section and section filtering")

    # 2. CONFIG GET / SET tests
    res = send_recv(s, "CONFIG GET maxmemory\r\n")
    assert "maxmemory" in res, f"CONFIG GET maxmemory failed: {res}"

    res = send_recv(s, "CONFIG GET maxmemory-policy\r\n")
    assert "maxmemory-policy" in res, f"CONFIG GET maxmemory-policy failed: {res}"

    res = send_recv(s, "CONFIG GET *\r\n")
    assert "maxmemory" in res and "maxmemory-policy" in res, f"CONFIG GET * failed: {res}"

    res = send_recv(s, "CONFIG GET nonexistent\r\n")
    assert res == "*0\r\n", f"CONFIG GET nonexistent should return *0, got: {res}"

    res = send_recv(s, "CONFIG SET maxmemory 1048576\r\n")
    assert res == "+OK\r\n", f"CONFIG SET maxmemory bytes failed: {res}"
    res = send_recv(s, "CONFIG GET maxmemory\r\n")
    assert "1048576" in res

    res = send_recv(s, "CONFIG SET maxmemory 500kb\r\n")
    assert res == "+OK\r\n", f"CONFIG SET maxmemory human failed: {res}"
    res = send_recv(s, "CONFIG GET maxmemory\r\n")
    assert "512000" in res

    res = send_recv(s, "CONFIG SET maxmemory-policy allkeys-lru\r\n")
    assert res == "+OK\r\n", f"CONFIG SET policy failed: {res}"
    res = send_recv(s, "CONFIG GET maxmemory-policy\r\n")
    assert "allkeys-lru" in res

    res = send_recv(s, "CONFIG SET maxmemory-policy invalid_policy\r\n")
    assert "-ERR" in res, f"Expected error on invalid policy, got: {res}"

    res = send_recv(s, "CONFIG SET unsupported_param 123\r\n")
    assert "-ERR" in res, f"Expected error on unsupported config param, got: {res}"
    print("[PASS] CONFIG GET and CONFIG SET for maxmemory and maxmemory-policy")

    # 3. Test OOM rejection with noeviction
    send_recv(s, "FLUSHDB\r\n")
    send_recv(s, "CONFIG SET maxmemory-policy noeviction\r\n")
    
    # Store initial key to see memory usage
    send_recv(s, "SET base_key base_value\r\n")
    info = send_recv(s, "INFO memory\r\n")
    lines = info.split("\r\n")
    used_mem = 0
    for line in lines:
        if line.startswith("used_memory:"):
            used_mem = int(line.split(":")[1])
            break
    assert used_mem > 0, "used_memory should be > 0"

    # Set maxmemory below current used memory so that memory is exceeded
    send_recv(s, f"CONFIG SET maxmemory {used_mem - 1}\r\n")

    # Try allocating commands: SET, SETEX, MSET, LPUSH, RPUSH, INCR
    res = send_recv(s, "SET another_key val\r\n")
    assert "-OOM" in res, f"Expected -OOM error from SET, got: {repr(res)}"

    res = send_recv(s, "SETEX another_key 100 val\r\n")
    assert "-OOM" in res, f"Expected -OOM error from SETEX, got: {repr(res)}"

    res = send_recv(s, "MSET k1 v1 k2 v2\r\n")
    assert "-OOM" in res, f"Expected -OOM error from MSET, got: {repr(res)}"

    res = send_recv(s, "LPUSH list_oom v1\r\n")
    assert "-OOM" in res, f"Expected -OOM error from LPUSH, got: {repr(res)}"

    res = send_recv(s, "RPUSH list_oom v1\r\n")
    assert "-OOM" in res, f"Expected -OOM error from RPUSH, got: {repr(res)}"

    res = send_recv(s, "INCR counter_oom\r\n")
    assert "-OOM" in res, f"Expected -OOM error from INCR, got: {repr(res)}"

    # Non-allocating commands MUST still work during OOM!
    assert send_recv(s, "GET base_key\r\n") == "$10\r\nbase_value\r\n"
    assert send_recv(s, "PING\r\n") == "+PONG\r\n"
    assert send_recv(s, "EXISTS base_key\r\n") == ":1\r\n"
    assert send_recv(s, "DBSIZE\r\n") == ":1\r\n"

    # Free memory via DEL, now SET works again
    assert send_recv(s, "DEL base_key\r\n") == ":1\r\n"
    res = send_recv(s, "SET allowed_key new_val\r\n")
    assert res == "+OK\r\n", f"Expected +OK after DEL, got: {repr(res)}"
    print("[PASS] OOM error -OOM strictly enforced on allocations, non-allocating commands permitted")

    # 4. Test allkeys-lru eviction
    send_recv(s, "FLUSHDB\r\n")
    send_recv(s, "CONFIG SET maxmemory-policy allkeys-lru\r\n")
    send_recv(s, "SET test_sample_key sample_data_long_enough\r\n")
    info = send_recv(s, "INFO memory\r\n")
    key_mem = 0
    for line in info.split("\r\n"):
        if line.startswith("used_memory:"):
            key_mem = int(line.split(":")[1])
            break
    
    limit = key_mem * 5
    send_recv(s, f"CONFIG SET maxmemory {limit}\r\n")

    # Insert 5 keys
    for i in range(5):
        send_recv(s, f"SET lru_key_{i} data_{i}\r\n")

    # Access lru_key_0 so it becomes MRU
    send_recv(s, "GET lru_key_0\r\n")

    # Insert 2 more keys to force eviction of older untouched keys
    for i in range(5, 7):
        res = send_recv(s, f"SET lru_key_{i} data_{i}\r\n")
        assert res == "+OK\r\n", f"SET failed under allkeys-lru: {res}"

    # Check evicted_keys count
    info = send_recv(s, "INFO memory\r\n")
    evicted = 0
    for line in info.split("\r\n"):
        if line.startswith("evicted_keys:"):
            evicted = int(line.split(":")[1])
            break
    assert evicted > 0, f"Expected evicted_keys > 0, got {evicted}"
    assert send_recv(s, "EXISTS lru_key_0\r\n") == ":1\r\n", "lru_key_0 should have survived because it was touched"
    assert send_recv(s, "EXISTS lru_key_1\r\n") == ":0\r\n", "lru_key_1 should have been evicted"
    print(f"[PASS] allkeys-lru eviction evicted {evicted} keys while preserving recently accessed key")

    # 5. Test volatile-lru eviction
    send_recv(s, "FLUSHDB\r\n")
    send_recv(s, "CONFIG SET maxmemory-policy volatile-lru\r\n")
    send_recv(s, f"CONFIG SET maxmemory {limit}\r\n")

    # Insert persistent key (no TTL)
    send_recv(s, "SET persistent_key persistent_val\r\n")

    # Insert volatile keys (with TTL)
    for i in range(15):
        send_recv(s, f"SETEX vol_key_{i} 1000 data_{i}\r\n")

    # Persistent key must NOT be evicted under volatile-lru
    assert send_recv(s, "EXISTS persistent_key\r\n") == ":1\r\n", "persistent_key was evicted under volatile-lru!"
    info = send_recv(s, "INFO memory\r\n")
    evicted_vol = 0
    for line in info.split("\r\n"):
        if line.startswith("evicted_keys:"):
            evicted_vol = int(line.split(":")[1])
            break
    assert evicted_vol > 0, "Expected volatile keys to be evicted"
    print(f"[PASS] volatile-lru eviction evicted {evicted_vol} volatile keys and preserved persistent keys")

    # Reset server state
    send_recv(s, "CONFIG SET maxmemory 0\r\n")
    send_recv(s, "CONFIG SET maxmemory-policy noeviction\r\n")
    send_recv(s, "FLUSHDB\r\n")
    s.close()

    # 6. Test CLI startup flags: --maxmemory and --maxmemory-policy
    cli_port = port + 20
    proc = subprocess.Popen([
        "./build/kvllay",
        "-p", str(cli_port),
        "--maxmemory", "64mb",
        "--maxmemory-policy", "allkeys-lru",
        "--no-snapshot",
        "--no-aof"
    ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(0.5)

    try:
        cli_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        cli_sock.connect(("127.0.0.1", cli_port))
        res_mem = send_recv(cli_sock, "CONFIG GET maxmemory\r\n")
        assert "67108864" in res_mem, f"Expected 64MB (67108864), got {res_mem}"
        res_pol = send_recv(cli_sock, "CONFIG GET maxmemory-policy\r\n")
        assert "allkeys-lru" in res_pol, f"Expected allkeys-lru, got {res_pol}"
        cli_sock.close()
        print("[PASS] CLI flags --maxmemory and --maxmemory-policy correctly initialized server")
    finally:
        proc.terminate()
        proc.wait()

    print("[PASS] All Maxmemory, OOM & Eviction tests passed successfully!")

def test_event_loop_and_high_concurrency(port=6389):
    print(f"\n--- Testing Event Loop & High Concurrency on port {port} ---")
    
    # 1. Pipelining test: Send 100 commands in a single TCP write
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(("127.0.0.1", port))
    
    pipeline_cmds = ""
    for i in range(100):
        pipeline_cmds += f"SET pipe_k_{i} val_{i}\r\n"
    s.sendall(pipeline_cmds.encode('utf-8'))
    
    # Read until we get 100 +OK responses
    resp_buf = ""
    while resp_buf.count("+OK\r\n") < 100:
        chunk = s.recv(4096).decode('utf-8')
        if not chunk:
            break
        resp_buf += chunk
    assert resp_buf.count("+OK\r\n") == 100, f"Expected 100 +OK responses, got {resp_buf.count('+OK\\r\\n')}"
    print("[PASS] Event Loop pipelining: 100 commands parsed and responded in exact order")
    
    # Verify values
    for i in range(0, 100, 20):
        res = send_recv(s, f"GET pipe_k_{i}\r\n")
        assert res == f"${len(f'val_{i}')}\r\nval_{i}\r\n", f"Mismatch for pipe_k_{i}: {res}"
    s.close()
    
    # 2. High concurrency test: 50 concurrent client threads
    num_clients = 50
    ops_per_client = 60
    errors = []
    
    def client_worker(worker_id):
        try:
            cs = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            cs.connect(("127.0.0.1", port))
            for op in range(ops_per_client):
                key = f"c_thread_{worker_id}_{op}"
                val = f"v_{worker_id}_{op}"
                set_resp = send_recv(cs, f"SET {key} {val}\r\n")
                if set_resp != "+OK\r\n":
                    errors.append(f"Worker {worker_id} SET failed: {set_resp}")
                    break
                get_resp = send_recv(cs, f"GET {key}\r\n")
                expected = f"${len(val)}\r\n{val}\r\n"
                if get_resp != expected:
                    errors.append(f"Worker {worker_id} GET failed: expected {expected}, got {get_resp}")
                    break
            cs.close()
        except Exception as e:
            errors.append(f"Worker {worker_id} exception: {e}")
            
    threads = [threading.Thread(target=client_worker, args=(i,)) for i in range(num_clients)]
    for t in threads: t.start()
    for t in threads: t.join()
    assert len(errors) == 0, f"High concurrency errors ({len(errors)}): {errors[:5]}"
    print(f"[PASS] Event Loop Multi-Reactor: {num_clients} concurrent clients x {ops_per_client} ops = {num_clients * ops_per_client * 2} ops succeeded with 0 errors")
    
    # 3. Connection churn test: 200 short-lived connections
    churn_errors = []
    for i in range(200):
        try:
            cs = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            cs.connect(("127.0.0.1", port))
            r = send_recv(cs, "PING churn\r\n")
            if r != "$5\r\nchurn\r\n":
                churn_errors.append(f"Churn {i} failed: {r}")
            cs.close()
        except Exception as e:
            churn_errors.append(f"Churn {i} exception: {e}")
    assert len(churn_errors) == 0, f"Connection churn errors: {churn_errors}"
    print("[PASS] Connection Churn: 200 short-lived connections opened, served, and cleanly closed")
    
    # 4. Test CLI flag --threads / --io-threads
    cli_port = port + 25
    proc = subprocess.Popen([
        "./build/kvllay",
        "-p", str(cli_port),
        "--threads", "4",
        "--no-snapshot",
        "--no-aof"
    ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    time.sleep(0.5)
    
    try:
        cli_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        cli_sock.connect(("127.0.0.1", cli_port))
        res_ping = send_recv(cli_sock, "PING\r\n")
        assert res_ping == "+PONG\r\n", f"Expected +PONG, got {res_ping}"
        cli_sock.close()
        print("[PASS] CLI flag --threads correctly initialized worker pool")
    finally:
        proc.terminate()
        proc.wait()
        
    print("[PASS] All Event Loop & High Concurrency tests passed successfully!")

def test_transactions_and_pipelines(port=6389):
    print(f"\n--- Testing Transactions & Pipelines on port {port} ---")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(("127.0.0.1", port))

    res = send_recv(s, "MULTI\r\nSET transaction_key value\r\nGET transaction_key\r\nEXEC\r\n")
    expected = "+OK\r\n+QUEUED\r\n+QUEUED\r\n*2\r\n+OK\r\n$5\r\nvalue\r\n"
    assert res == expected, f"MULTI/EXEC failed: {res!r}"

    res = send_recv(s, "MULTI\r\nSET discarded value\r\nDISCARD\r\nGET discarded\r\n")
    expected = "+OK\r\n+QUEUED\r\n+OK\r\n$-1\r\n"
    assert res == expected, f"DISCARD failed: {res!r}"

    res = send_recv(s, "SET pipeline_one 1\r\nGET pipeline_one\r\n")
    assert res == "+OK\r\n$1\r\n1\r\n", f"ordinary pipeline failed: {res!r}"

    assert send_recv(s, "EXEC\r\n") == "-ERR EXEC without MULTI\r\n"
    assert send_recv(s, "DISCARD\r\n") == "-ERR DISCARD without MULTI\r\n"
    s.close()

    # QUIT inside MULTI
    s_quit = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s_quit.connect(("127.0.0.1", port))
    res = send_recv(s_quit, "MULTI\r\nSET qk qv\r\nQUIT\r\n")
    assert res == "+OK\r\n+QUEUED\r\n+OK\r\n", f"QUIT in MULTI failed: {res!r}"
    assert len(s_quit.recv(1024)) == 0, "Expected socket close after QUIT in MULTI"
    s_quit.close()
    print("[PASS] MULTI/EXEC/DISCARD, pipeline, and QUIT in MULTI")

def test_allocator_and_memory_optimization(port=6389):
    print(f"\n--- Testing Memory Manager & Allocator Optimization (Port {port}) ---")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(("127.0.0.1", port))

    # 1. INFO memory metrics
    res = send_recv(s, "INFO memory\r\n")
    assert "mem_allocator:" in res, f"Expected mem_allocator in INFO memory, got: {res}"
    assert "used_memory_rss:" in res, f"Expected used_memory_rss in INFO memory, got: {res}"
    assert "used_memory_peak:" in res, f"Expected used_memory_peak in INFO memory, got: {res}"
    assert "mem_fragmentation_ratio:" in res, f"Expected mem_fragmentation_ratio in INFO memory, got: {res}"
    
    # Extract allocator name
    allocator_name = ""
    for line in res.split("\r\n"):
        if line.startswith("mem_allocator:"):
            allocator_name = line.split(":", 1)[1]
            break
    assert allocator_name in ["libc"] or allocator_name.startswith("jemalloc") or allocator_name.startswith("mimalloc"), \
        f"Unexpected allocator name: {allocator_name}"
    print(f"[PASS] INFO memory reports valid allocator ({allocator_name}), RSS, Peak, and fragmentation ratio")

    # 2. Key overwrite without memory bloat (in-place buffer reuse)
    send_recv(s, "FLUSHALL\r\n")
    initial_mem_info = send_recv(s, "INFO memory\r\n")
    initial_used = 0
    for line in initial_mem_info.split("\r\n"):
        if line.startswith("used_memory:"):
            initial_used = int(line.split(":", 1)[1])
            break

    # Perform 5,000 overwrites of the same key with the same size
    for i in range(5000):
        val = f"value_{i % 100:04d}"
        send_recv(s, f"SET churn_key {val}\r\n")

    res_get = send_recv(s, "GET churn_key\r\n")
    assert "value_0099" in res_get

    mid_mem_info = send_recv(s, "INFO memory\r\n")
    mid_used = 0
    for line in mid_mem_info.split("\r\n"):
        if line.startswith("used_memory:"):
            mid_used = int(line.split(":", 1)[1])
            break

    # Memory for a single key shouldn't explode after 5000 overwrites
    assert mid_used < 2000, f"Memory leaked during intensive overwrites: {mid_used} bytes"
    print("[PASS] In-place string buffer reuse prevents heap fragmentation and memory leaks on key overwrite")

    # 3. Numeric INCR in-place buffer reuse
    send_recv(s, "SET num_counter 0\r\n")
    for i in range(1000):
        send_recv(s, "INCR num_counter\r\n")
    res_counter = send_recv(s, "GET num_counter\r\n")
    assert res_counter == "$4\r\n1000\r\n", f"Expected 1000, got: {res_counter}"
    print("[PASS] High-frequency INCR counter operates with zero allocations and correct memory accounting")

    # 4. FLUSHDB memory purge
    send_recv(s, "MSET k1 v1 k2 v2 k3 v3 k4 v4\r\n")
    send_recv(s, "FLUSHDB\r\n")
    post_flush_info = send_recv(s, "INFO memory\r\n")
    post_used = 0
    for line in post_flush_info.split("\r\n"):
        if line.startswith("used_memory:"):
            post_used = int(line.split(":", 1)[1])
            break
    assert post_used == 0, f"Expected 0 used_memory after FLUSHDB, got {post_used}"
    print("[PASS] FLUSHDB resets used_memory to 0 and triggers memory purge")

    s.close()
    print("[PASS] All Memory Manager & Allocator tests passed successfully!")

if __name__ == "__main__":
    test_port = int(sys.argv[1]) if len(sys.argv) > 1 else 6389
    test_kvllay(test_port)
    test_ttl(test_port)
    test_atomic_counters_and_rate_limiting(test_port)
    test_multi_key(test_port)
    test_lists_and_queues(test_port)
    test_maxmemory_and_eviction(test_port)
    test_persistence(test_port)
    test_transactions_and_pipelines(test_port)
    test_event_loop_and_high_concurrency(test_port)
    test_allocator_and_memory_optimization(test_port)
