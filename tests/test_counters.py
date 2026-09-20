import socket
import threading
import time

from common import send_recv


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

