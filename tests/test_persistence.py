import socket
import time
import subprocess
import os

from common import send_recv, server_binary, wait_for_server


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
    snap_port = port + 3
    snap_file = "test_snapshot_lifecycle.kvl"
    if os.path.exists(snap_file):
        os.remove(snap_file)

    p1 = subprocess.Popen([server_binary(), "-p", str(snap_port), "--snapshot", snap_file, "--no-aof"])
    wait_for_server(snap_port)
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
    p2 = subprocess.Popen([server_binary(), "-p", str(snap_port), "--snapshot", snap_file, "--no-aof"])
    wait_for_server(snap_port)
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
    aof_port = port + 4
    aof_file = "test_aof_lifecycle.aof"
    if os.path.exists(aof_file):
        os.remove(aof_file)

    p_aof1 = subprocess.Popen([server_binary(), "-p", str(aof_port), "--aof", aof_file, "--no-snapshot", "--appendfsync", "always"])
    wait_for_server(aof_port)
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
    p_aof2 = subprocess.Popen([server_binary(), "-p", str(aof_port), "--aof", aof_file, "--no-snapshot"])
    wait_for_server(aof_port)
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

    # 7. Truncated AOF Crash Recovery
    print("Testing truncated AOF crash recovery...")
    trunc_aof = "test_trunc_crash.aof"
    with open(trunc_aof, "wb") as f:
        # Valid command 1
        f.write(b"*3\r\n$3\r\nSET\r\n$7\r\nvalid_k\r\n$7\r\nvalid_v\r\n")
        # Valid command 2
        f.write(b"*2\r\n$4\r\nINCR\r\n$9\r\nsaved_num\r\n")
        # Incomplete / cut-off command at crash
        f.write(b"*3\r\n$3\r\nSET\r\n$11\r\nincomplete_\r\n$20\r\ntruncated_payload_mi")

    trunc_port = port + 7
    p_trunc = subprocess.Popen([server_binary(), "-p", str(trunc_port), "--aof", trunc_aof, "--no-snapshot"])
    wait_for_server(trunc_port)
    try:
        st = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        st.connect(("127.0.0.1", trunc_port))
        assert send_recv(st, "GET valid_k\r\n") == "$7\r\nvalid_v\r\n"
        assert send_recv(st, "GET saved_num\r\n") == "$1\r\n1\r\n"
        assert send_recv(st, "GET incomplete_\r\n") == "$-1\r\n"
        st.close()
        print("[PASS] Truncated AOF cleanly recovered valid commands without hanging")
    finally:
        p_trunc.terminate()
        p_trunc.wait()
        if os.path.exists(trunc_aof):
            os.remove(trunc_aof)

    # 8. Snapshot CRC32 corruption detection
    print("Testing CRC32 snapshot corruption detection...")
    corrupt_file = "corrupt_test.kvl"
    with open(corrupt_file, "wb") as f:
        f.write(b"KVLLAYS1\x00\x00\x00\x00\x00\x00\x00\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00BADCRC")

    corrupt_port = port + 6
    p_corr = subprocess.Popen([server_binary(), "-p", str(corrupt_port), "--snapshot", corrupt_file, "--no-aof"])
    wait_for_server(corrupt_port)
    try:
        sc = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sc.connect(("127.0.0.1", corrupt_port))
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
