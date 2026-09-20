import socket
import threading
import time

from common import send_recv


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

