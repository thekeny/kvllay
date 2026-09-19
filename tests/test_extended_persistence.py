import os
import subprocess
import socket
import time
from common import send_recv, server_binary, wait_for_server


def test_extended_persistence(port=6389):
    print(f"\n--- Testing Extended Persistence (Hash, Set, ZSet, TTL across Snapshot & AOF) on port {port} ---")
    import redis

    # 1. Snapshot lifecycle test for Hash, Set, ZSet
    snap_port = port + 8
    snap_file = "test_ext_snap.kvl"
    if os.path.exists(snap_file):
        os.remove(snap_file)

    p_snap1 = subprocess.Popen([server_binary(), "-p", str(snap_port), "--snapshot", snap_file, "--no-aof"])
    wait_for_server(snap_port)
    try:
        r1 = redis.Redis(host="127.0.0.1", port=snap_port, decode_responses=True, socket_timeout=4)
        r1.hset("user:100:profile", mapping={"name": "Alice", "role": "admin"})
        r1.sadd("user:100:groups", "engineers", "leads")
        r1.zadd("leaderboard", {"player1": 100.5, "player2": 250.75})
        r1.setex("temp:token", 60, "xyz123")
        r1.save()
        r1.close()
    finally:
        p_snap1.terminate()
        p_snap1.wait()

    assert os.path.exists(snap_file)
    assert os.path.getsize(snap_file) > 28

    # Restart and verify
    p_snap2 = subprocess.Popen([server_binary(), "-p", str(snap_port), "--snapshot", snap_file, "--no-aof"])
    wait_for_server(snap_port)
    try:
        r2 = redis.Redis(host="127.0.0.1", port=snap_port, decode_responses=True, socket_timeout=4)
        assert r2.hgetall("user:100:profile") == {"name": "Alice", "role": "admin"}
        assert r2.smembers("user:100:groups") == {"engineers", "leads"}
        assert r2.zscore("leaderboard", "player1") == 100.5
        assert r2.zscore("leaderboard", "player2") == 250.75
        assert r2.get("temp:token") == "xyz123"
        ttl_rem = r2.ttl("temp:token")
        assert 0 < ttl_rem <= 60
        r2.close()
        print("[PASS] Snapshot correctly saved and restored Hash, Set, ZSet with precision and TTL")
    finally:
        p_snap2.terminate()
        p_snap2.wait()
        if os.path.exists(snap_file):
            os.remove(snap_file)

    # 2. AOF lifecycle test with BGREWRITEAOF for Hash, Set, ZSet
    aof_port = port + 9
    aof_file = "test_ext_aof.aof"
    if os.path.exists(aof_file):
        os.remove(aof_file)

    p_aof1 = subprocess.Popen([
        server_binary(), "-p", str(aof_port), "--aof", aof_file, "--no-snapshot", "--appendfsync", "always"
    ])
    wait_for_server(aof_port)
    try:
        ra1 = redis.Redis(host="127.0.0.1", port=aof_port, decode_responses=True, socket_timeout=4)
        ra1.hset("call:session:aof", mapping={"state": "ringing", "caller": "user1"})
        ra1.sadd("call:tokens", "tok_a", "tok_b")
        ra1.zadd("call:timeouts", {"user1": 1500.125, "user2": 3000.5})
        ra1.expire("call:session:aof", 300)

        # Trigger rewrite while running
        ra1.bgrewriteaof()
        time.sleep(0.3)

        # Add more mutations after rewrite
        ra1.hset("call:session:aof", "state", "active")
        ra1.zrem("call:timeouts", "user1")
        ra1.close()
    finally:
        p_aof1.terminate()
        p_aof1.wait()

    assert os.path.exists(aof_file)

    # Restart and verify replay
    p_aof2 = subprocess.Popen([
        server_binary(), "-p", str(aof_port), "--aof", aof_file, "--no-snapshot"
    ])
    wait_for_server(aof_port)
    try:
        ra2 = redis.Redis(host="127.0.0.1", port=aof_port, decode_responses=True, socket_timeout=4)
        assert ra2.hgetall("call:session:aof") == {"state": "active", "caller": "user1"}
        assert ra2.smembers("call:tokens") == {"tok_a", "tok_b"}
        assert ra2.zscore("call:timeouts", "user1") is None
        assert ra2.zscore("call:timeouts", "user2") == 3000.5
        ttl_rem = ra2.ttl("call:session:aof")
        assert 0 < ttl_rem <= 300
        ra2.close()
        print("[PASS] AOF BGREWRITEAOF and replay correctly recovered Hash, Set, ZSet state")
    finally:
        p_aof2.terminate()
        p_aof2.wait()
        if os.path.exists(aof_file):
            os.remove(aof_file)

    print("[PASS] All Extended Persistence tests passed successfully!")


if __name__ == "__main__":
    test_extended_persistence()
