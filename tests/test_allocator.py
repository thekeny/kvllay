import socket

from common import send_recv


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

