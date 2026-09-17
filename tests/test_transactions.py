import socket

from common import send_recv


def test_transactions_and_pipelines(port=6389):
    print(f"\n--- Testing Transactions & Pipelines on port {port} ---")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(("127.0.0.1", port))

    res = send_recv(s, "MULTI\r\nSET transaction_key value\r\nGET transaction_key\r\nEXEC\r\n")
    expected = "+OK\r\n+QUEUED\r\n+QUEUED\r\n*2\r\n+OK\r\n$5\r\nvalue\r\n"
    assert res == expected, f"MULTI/EXEC failed: {res!r}"

    res = send_recv(s, "MULTI\r\nSET discarded value\r\nDISCARD\r\nGET discarded\r\n")
    expected = "+OK\r\n+QUEUED\r\n+OK\r\n$-1\r\n"
    assert res == expected, f"DISCARD failed: {res!r}"

    res = send_recv(s, "SET pipeline_one 1\r\nGET pipeline_one\r\n")
    assert res == "+OK\r\n$1\r\n1\r\n", f"ordinary pipeline failed: {res!r}"

    assert send_recv(s, "EXEC\r\n") == "-ERR EXEC without MULTI\r\n"
    assert send_recv(s, "DISCARD\r\n") == "-ERR DISCARD without MULTI\r\n"
    s.close()

    # QUIT inside MULTI
    s_quit = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s_quit.connect(("127.0.0.1", port))
    res = send_recv(s_quit, "MULTI\r\nSET qk qv\r\nQUIT\r\n")
    assert res == "+OK\r\n+QUEUED\r\n+OK\r\n", f"QUIT in MULTI failed: {res!r}"
    assert len(s_quit.recv(1024)) == 0, "Expected socket close after QUIT in MULTI"
    s_quit.close()
    print("[PASS] MULTI/EXEC/DISCARD, pipeline, and QUIT in MULTI")

