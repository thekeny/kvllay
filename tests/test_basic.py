import socket
import threading

from common import send_recv


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

