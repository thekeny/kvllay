import argparse
import os
import socket
import tempfile
from contextlib import contextmanager

from test_basic import test_kvllay
from test_ttl import test_ttl
from test_counters import test_atomic_counters_and_rate_limiting
from test_multi_key import test_multi_key
from test_lists import test_lists_and_queues
from test_maxmemory import test_maxmemory_and_eviction
from test_persistence import test_persistence
from test_transactions import test_transactions_and_pipelines
from test_concurrency import test_event_loop_and_high_concurrency
from test_allocator import test_allocator_and_memory_optimization
from test_auth import test_auth
from common import running_server, send_recv, wait_for_server


def reset_server(port):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.settimeout(5)
        sock.connect(("127.0.0.1", port))
        response = send_recv(sock, "FLUSHDB\r\n")
        if response != "+OK\r\n":
            raise RuntimeError(
                f"Cannot reset server on port {port}; expected +OK, got {response!r}"
            )
    finally:
        sock.close()


@contextmanager
def test_server(port):
    try:
        wait_for_server(port, timeout=0.25)
    except RuntimeError:
        snapshot = os.path.join(tempfile.gettempdir(), f"kvllay-test-{port}.kvl")
        with running_server(
            port,
            ["--snapshot", snapshot, "--no-aof"],
        ) as process:
            yield process
        if os.path.exists(snapshot):
            os.remove(snapshot)
        return
    yield None


def run_all(port=6389):
    reset_server(port)
    test_kvllay(port)
    test_auth(port + 1)
    test_ttl(port)
    test_atomic_counters_and_rate_limiting(port)
    test_multi_key(port)
    test_lists_and_queues(port)
    test_maxmemory_and_eviction(port)
    test_persistence(port)
    test_transactions_and_pipelines(port)
    test_event_loop_and_high_concurrency(port)
    test_allocator_and_memory_optimization(port)
    try:
        from test_redis_py import test_redis_py
    except ImportError:
        print("[SKIP] redis-py is not installed")
    else:
        test_redis_py(port)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("port", nargs="?", type=int, default=6389)
    parser.add_argument(
        "--start-server",
        action="store_true",
        help="start a temporary server when the test port is not already in use",
    )
    options = parser.parse_args()
    if options.start_server:
        with test_server(options.port):
            run_all(options.port)
    else:
        run_all(options.port)
