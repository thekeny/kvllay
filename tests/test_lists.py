import socket
import threading
import time

from common import send_recv


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

