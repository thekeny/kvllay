import os
import socket
import subprocess
import time
from contextlib import contextmanager
from pathlib import Path


def send_recv(sock, data):
    sock.sendall(data.encode('utf-8') if isinstance(data, str) else data)
    return sock.recv(4096).decode('utf-8')


def wait_for_server(port, timeout=5.0):
    deadline = time.monotonic() + timeout
    last_error = None
    while time.monotonic() < deadline:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(0.25)
        try:
            sock.connect(("127.0.0.1", port))
            sock.close()
            return
        except OSError as exc:
            last_error = exc
            sock.close()
            time.sleep(0.05)
    raise RuntimeError(f"Server did not start on port {port}: {last_error}")


def server_binary():
    name = "kvllay.exe" if os.name == "nt" else "kvllay"
    return str(Path(__file__).resolve().parent.parent / "build" / name)


@contextmanager
def running_server(port, extra_args=None):
    args = [server_binary(), "-p", str(port), "--no-snapshot", "--no-aof"]
    if extra_args:
        args.extend(extra_args)
    process = subprocess.Popen(args)
    try:
        wait_for_server(port)
        yield process
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
