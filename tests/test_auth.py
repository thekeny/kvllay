import socket

from common import running_server, send_recv


def test_auth(port=6390, password="secret123"):
    print(f"\n--- Testing Password Protection on port {port} ---")
    with running_server(port, ["--requirepass", password]) as _:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(("127.0.0.1", port))

        # Try command without AUTH -> expect NOAUTH
        res = send_recv(s, "PING\r\n")
        assert "NOAUTH" in res, f"Expected NOAUTH, got: {repr(res)}"
        print("[PASS] Rejected command without auth (NOAUTH)")

        # Try AUTH with wrong password -> expect WRONGPASS
        res = send_recv(s, "AUTH wrongpass\r\n")
        assert "WRONGPASS" in res, f"Expected WRONGPASS, got: {repr(res)}"
        print("[PASS] Rejected wrong password (WRONGPASS)")

        # Still unauthorized
        res = send_recv(s, "SET a 1\r\n")
        assert "NOAUTH" in res, f"Expected NOAUTH, got: {repr(res)}"

        # Try AUTH with correct password -> expect +OK
        res = send_recv(s, f"AUTH {password}\r\n")
        assert res == "+OK\r\n", f"Expected +OK, got: {repr(res)}"
        print("[PASS] Authenticated successfully with password")

        # Commands now succeed
        res = send_recv(s, "SET a 123\r\n")
        assert res == "+OK\r\n", f"Expected +OK, got: {repr(res)}"
        res = send_recv(s, "GET a\r\n")
        assert res == "$3\r\n123\r\n", f"Expected $3\\r\\n123\\r\\n, got: {repr(res)}"
        print("[PASS] Commands work after authentication")

        s.close()
    print("[PASS] All AUTH tests passed!")
