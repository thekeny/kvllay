import socket
import time

from common import send_recv


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

