import socket
import time
import subprocess

from common import send_recv, server_binary, wait_for_server


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
        server_binary(),
        "-p", str(cli_port),
        "--maxmemory", "64mb",
        "--maxmemory-policy", "allkeys-lru",
        "--no-snapshot",
        "--no-aof"
    ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    wait_for_server(cli_port)

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
