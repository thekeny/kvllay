import sys
import time
import threading
import redis

def test_redis_py(port=6389):
    print(f"=== Running redis-py Integration Suite on 127.0.0.1:{port} ===")

    # 1. Basic Connection & Ping
    r = redis.Redis(host="127.0.0.1", port=port, decode_responses=True)
    assert r.ping() is True, "PING failed"
    print("[PASS] Connection and PING")

    # 2. SELECT tests
    r_db0 = redis.Redis(host="127.0.0.1", port=port, db=0, decode_responses=True)
    assert r_db0.ping() is True, "SELECT 0 failed"
    
    try:
        r_db1 = redis.Redis(host="127.0.0.1", port=port, db=1, decode_responses=True)
        r_db1.ping()
        assert False, "SELECT 1 should have failed"
    except redis.exceptions.ResponseError as e:
        assert "DB index is out of range" in str(e), f"Unexpected error: {e}"
    print("[PASS] SELECT 0 and out-of-range rejection")

    # 3. CLIENT commands
    client_id = r.client_id()
    assert isinstance(client_id, int) and client_id > 0, f"CLIENT ID failed: {client_id}"
    
    assert r.client_getname() is None, "Initial client name should be None"
    assert r.client_setname("redis-py-worker") is True, "CLIENT SETNAME failed"
    assert r.client_getname() == "redis-py-worker", "CLIENT GETNAME mismatch"

    # Reject whitespace in CLIENT SETNAME
    try:
        r.client_setname("bad name")
        assert False, "CLIENT SETNAME with space should fail"
    except redis.exceptions.ResponseError as e:
        assert "client name cannot contain spaces" in str(e), f"Unexpected error: {e}"

    clients = r.client_list()
    assert len(clients) >= 1, "CLIENT LIST should contain at least 1 client"
    matching = [c for c in clients if c.get("name") == "redis-py-worker"]
    assert len(matching) == 1, f"Expected client in list: {clients}"
    assert "lib-name" in matching[0] and "lib-ver" in matching[0]
    print("[PASS] CLIENT ID, SETNAME, GETNAME, and LIST")

    # 4. SET options (EX, PX, NX, XX, KEEPTTL)
    r.delete("opt_k")
    # Basic SET & GET
    assert r.set("opt_k", "val") is True
    assert r.get("opt_k") == "val"

    # NX: Should not apply on existing key
    assert r.set("opt_k", "new_val", nx=True) is None
    assert r.get("opt_k") == "val"

    # XX: Should apply on existing key
    assert r.set("opt_k", "updated_val", xx=True) is True
    assert r.get("opt_k") == "updated_val"

    # NX on non-existing key
    r.delete("opt_k_nx")
    assert r.set("opt_k_nx", "created", nx=True) is True
    assert r.get("opt_k_nx") == "created"

    # XX on non-existing key
    assert r.set("nonexistent_xx", "foo", xx=True) is None
    assert r.get("nonexistent_xx") is None

    # EX: Expiration in seconds
    assert r.set("opt_k_ex", "exp_val", ex=2) is True
    ttl = r.ttl("opt_k_ex")
    assert 0 < ttl <= 2, f"Expected TTL between 1 and 2, got {ttl}"

    # PX: Expiration in milliseconds
    assert r.set("opt_k_px", "px_val", px=1500) is True
    pttl = r.pttl("opt_k_px")
    assert 0 < pttl <= 1500, f"Expected PTTL <= 1500, got {pttl}"

    # KEEPTTL: Preserves expiration on update
    r.set("opt_k_ttl", "initial", ex=5)
    r.set("opt_k_ttl", "modified", keepttl=True)
    assert r.get("opt_k_ttl") == "modified"
    assert r.ttl("opt_k_ttl") > 0, "KEEPTTL failed to preserve TTL"

    # Overwrite without KEEPTTL clears expiration
    r.set("opt_k_ttl", "permanent")
    assert r.ttl("opt_k_ttl") == -1, "SET without options should clear TTL"
    print("[PASS] SET options (EX, PX, NX, XX, KEEPTTL)")

    # 5. KEYS command pattern matching & strict argument count
    try:
        r.execute_command("KEYS")
        assert False, "KEYS without pattern should fail"
    except redis.exceptions.ResponseError as e:
        assert "wrong number of arguments" in str(e), f"Unexpected error: {e}"

    keys = r.keys("opt_k*")
    assert len(keys) >= 2, f"KEYS pattern match failed: {keys}"
    print("[PASS] KEYS validation and pattern search")

    # 6. Transactions (MULTI, EXEC, DISCARD) via pipeline(transaction=True)
    r.delete("tx_a", "tx_b")
    pipe = r.pipeline(transaction=True)
    pipe.set("tx_a", "100")
    pipe.set("tx_b", "200")
    pipe.get("tx_a")
    pipe.get("tx_b")
    results = pipe.execute()
    assert results == [True, True, "100", "200"], f"Transaction results mismatch: {results}"
    assert r.get("tx_a") == "100" and r.get("tx_b") == "200"

    # Transaction discard / reset
    pipe = r.pipeline(transaction=True)
    pipe.set("tx_discard", "should_not_exist")
    pipe.reset()
    assert r.get("tx_discard") is None
    print("[PASS] Transactions MULTI / EXEC / DISCARD")

    # 7. Non-transactional Pipelining
    pipe = r.pipeline(transaction=False)
    for i in range(50):
        pipe.set(f"bulk_pipe_{i}", f"val_{i}")
    for i in range(50):
        pipe.get(f"bulk_pipe_{i}")
    pipe_res = pipe.execute()
    assert len(pipe_res) == 100
    assert all(res is True for res in pipe_res[:50])
    assert [pipe_res[50 + i] for i in range(50)] == [f"val_{i}" for i in range(50)]
    print("[PASS] High-throughput non-transactional pipelining (100 commands)")

    # 8. Lists & Queues
    r.delete("py_list")
    assert r.rpush("py_list", "a", "b", "c") == 3
    assert r.lpush("py_list", "z") == 4
    assert r.lrange("py_list", 0, -1) == ["z", "a", "b", "c"]
    assert r.lindex("py_list", 0) == "z"
    assert r.lpop("py_list") == "z"
    assert r.rpop("py_list") == "c"
    assert r.llen("py_list") == 2
    print("[PASS] List operations (LPUSH, RPUSH, LPOP, RPOP, LRANGE, LLEN, LINDEX)")

    # 9. Counters (INCR, DECR, INCRBY, DECRBY)
    r.delete("py_counter")
    assert r.incr("py_counter") == 1
    assert r.incrby("py_counter", 5) == 6
    assert r.decr("py_counter") == 5
    assert r.decrby("py_counter", 10) == -5
    print("[PASS] Counter operations (INCR, DECR, INCRBY, DECRBY)")

    # 10. Multi-Key (MSET, MGET)
    assert r.mset({"mk1": "mv1", "mk2": "mv2"}) is True
    assert r.mget("mk1", "mk2", "nonexistent") == ["mv1", "mv2", None]
    print("[PASS] Multi-key operations (MSET, MGET)")

    # 11. Multithreaded Concurrency Stress Test with ConnectionPool
    print("Testing multithreaded ConnectionPool stress test (20 threads x 50 ops)...")
    pool = redis.ConnectionPool(host="127.0.0.1", port=port, max_connections=25, decode_responses=True)
    threads = []
    thread_errors = []

    def concurrent_worker(worker_id):
        try:
            client = redis.Redis(connection_pool=pool)
            for j in range(50):
                k = f"pool_thread_{worker_id}_{j}"
                v = f"v_{worker_id}_{j}"
                client.set(k, v, ex=30)
                got = client.get(k)
                if got != v:
                    thread_errors.append(f"Worker {worker_id} mismatch on {k}: expected {v}, got {got}")
                # Atomic counter
                client.incr("pool_shared_counter")
        except Exception as exc:
            thread_errors.append(f"Worker {worker_id} exception: {exc}")

    r.delete("pool_shared_counter")
    for wid in range(20):
        t = threading.Thread(target=concurrent_worker, args=(wid,))
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    assert not thread_errors, f"Concurrency errors occurred: {thread_errors}"
    final_counter = int(r.get("pool_shared_counter"))
    assert final_counter == 1000, f"Expected counter 1000, got {final_counter}"
    print(f"[PASS] 20 concurrent threads x 50 ops = 1000 operations completed with 0 errors (shared counter = {final_counter})")

    r.close()
    pool.disconnect()
    print("\n[ALL PASS] redis-py integration test suite passed successfully!")

if __name__ == "__main__":
    test_port = int(sys.argv[1]) if len(sys.argv) > 1 else 6389
    test_redis_py(test_port)
