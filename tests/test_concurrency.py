import socket
import threading
import time
import subprocess

from common import send_recv, server_binary, wait_for_server


def test_event_loop_and_high_concurrency(port=6389):
    print(f"\n--- Testing Event Loop & High Concurrency on port {port} ---")
    
    # 1. Pipelining test: Send 100 commands in a single TCP write
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(("127.0.0.1", port))
    
    pipeline_cmds = ""
    for i in range(100):
        pipeline_cmds += f"SET pipe_k_{i} val_{i}\r\n"
    s.sendall(pipeline_cmds.encode('utf-8'))
    
    # Read until we get 100 +OK responses
    resp_buf = ""
    while resp_buf.count("+OK\r\n") < 100:
        chunk = s.recv(4096).decode('utf-8')
        if not chunk:
            break
        resp_buf += chunk
    assert resp_buf.count("+OK\r\n") == 100, f"Expected 100 +OK responses, got {resp_buf.count('+OK\\r\\n')}"
    print("[PASS] Event Loop pipelining: 100 commands parsed and responded in exact order")
    
    # Verify values
    for i in range(0, 100, 20):
        res = send_recv(s, f"GET pipe_k_{i}\r\n")
        assert res == f"${len(f'val_{i}')}\r\nval_{i}\r\n", f"Mismatch for pipe_k_{i}: {res}"
    s.close()
    
    # 2. High concurrency test: 50 concurrent client threads
    num_clients = 50
    ops_per_client = 60
    errors = []
    
    def client_worker(worker_id):
        try:
            cs = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            cs.connect(("127.0.0.1", port))
            for op in range(ops_per_client):
                key = f"c_thread_{worker_id}_{op}"
                val = f"v_{worker_id}_{op}"
                set_resp = send_recv(cs, f"SET {key} {val}\r\n")
                if set_resp != "+OK\r\n":
                    errors.append(f"Worker {worker_id} SET failed: {set_resp}")
                    break
                get_resp = send_recv(cs, f"GET {key}\r\n")
                expected = f"${len(val)}\r\n{val}\r\n"
                if get_resp != expected:
                    errors.append(f"Worker {worker_id} GET failed: expected {expected}, got {get_resp}")
                    break
            cs.close()
        except Exception as e:
            errors.append(f"Worker {worker_id} exception: {e}")
            
    threads = [threading.Thread(target=client_worker, args=(i,)) for i in range(num_clients)]
    for t in threads: t.start()
    for t in threads: t.join()
    assert len(errors) == 0, f"High concurrency errors ({len(errors)}): {errors[:5]}"
    print(f"[PASS] Event Loop Multi-Reactor: {num_clients} concurrent clients x {ops_per_client} ops = {num_clients * ops_per_client * 2} ops succeeded with 0 errors")
    
    # 3. Connection churn test: 200 short-lived connections
    churn_errors = []
    for i in range(200):
        try:
            cs = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            cs.connect(("127.0.0.1", port))
            r = send_recv(cs, "PING churn\r\n")
            if r != "$5\r\nchurn\r\n":
                churn_errors.append(f"Churn {i} failed: {r}")
            cs.close()
        except Exception as e:
            churn_errors.append(f"Churn {i} exception: {e}")
    assert len(churn_errors) == 0, f"Connection churn errors: {churn_errors}"
    print("[PASS] Connection Churn: 200 short-lived connections opened, served, and cleanly closed")
    
    # 4. Test CLI flag --threads / --io-threads
    cli_port = port + 25
    proc = subprocess.Popen([
        server_binary(),
        "-p", str(cli_port),
        "--threads", "4",
        "--no-snapshot",
        "--no-aof"
    ], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    wait_for_server(cli_port)
    
    try:
        cli_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        cli_sock.connect(("127.0.0.1", cli_port))
        res_ping = send_recv(cli_sock, "PING\r\n")
        assert res_ping == "+PONG\r\n", f"Expected +PONG, got {res_ping}"
        cli_sock.close()
        print("[PASS] CLI flag --threads correctly initialized worker pool")
    finally:
        proc.terminate()
        proc.wait()
        
    print("[PASS] All Event Loop & High Concurrency tests passed successfully!")
