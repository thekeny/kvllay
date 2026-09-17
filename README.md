<div align="center">
  <img src="https://raw.githubusercontent.com/thekeny/kvllay/main/logo.png" alt="kvllay logo" width="128" style="image-rendering: pixelated; image-rendering: crisp-edges;">
  <h1>kvllay</h1>
  <p><em>[pronounced: <strong>key-vi-lay</strong> · «кей-ви-лей» (key-value allay)]</em></p>
  <p>In-memory key-value store</p>

  <p>
    <a href="https://github.com/thekeny/kvllay/releases">Release</a> •
    <a href="https://github.com/thekeny/kvllay/blob/main/docs/ru.md">Русская документация</a> •
    <a href="https://github.com/thekeny/kvllay/blob/main/docs/en.md">English Documentation</a>
  </p>
</div>

Compatible with standard `redis-cli` and official client SDK libraries for any programming language.

## Features
- **RESP2 Protocol**: full support for Redis command formats (arrays, bulk strings, errors, integers, inline commands).
- **Security (AUTH)**: password protection (`--requirepass` / `-a`), safeguards against unauthorized access (`NOAUTH` / `WRONGPASS`).
- **Network Configuration**: bind to any network interface (`0.0.0.0` for network access, `127.0.0.1` for local access only).
- **Commands**:
  - `AUTH [username] password`
  - `CLIENT SETINFO LIB-NAME|LIB-VER value` / `CLIENT SETNAME name` / `CLIENT GETNAME` / `CLIENT LIST`
  - `PING [message]`
  - `SET key value [EX seconds|PX milliseconds] [NX|XX] [KEEPTTL]`
  - `GET key`
  - `MSET key value [key value ...]`
  - `MGET key [key ...]`
  - `DEL key [key ...]`
  - `EXISTS key [key ...]`
  - `KEYS [pattern]`
  - `DBSIZE`
  - `FLUSHDB`
  - `EXPIRE key seconds` / `PEXPIRE key milliseconds`
  - `TTL key` / `PTTL key`
  - `PERSIST key`
  - `SETEX key seconds value`
  - `INCR key` / `DECR key`
  - `INCRBY key increment` / `DECRBY key decrement`
  - `LPUSH key value [value ...]` / `RPUSH key value [value ...]`
  - `LPOP key [count]` / `RPOP key [count]`
  - `LLEN key` / `LRANGE key start stop` / `LINDEX key index`
  - `TYPE key`
  - `SELECT index`
  - `SAVE` (synchronous snapshot)
  - `BGSAVE` (background snapshot without `fork()`)
  - `LASTSAVE` (UNIX epoch timestamp of last save)
  - `BGREWRITEAOF` (background AOF compaction without `fork()`)
  - `ECHO message`
  - `COMMAND` / `COMMAND DOCS` (redis-cli handshake)
  - `MULTI` / `EXEC` / `DISCARD` (per-connection transactions)
  - `HELLO 2|3` (RESP2/RESP3 handshake, including optional `AUTH` and `SETNAME`)
  - `INFO` (includes `# Persistence`)
  - `QUIT`
- **Lists & Task Queues (Basic Structures)**: $O(1)$ push/pop operations powered by `std::deque` and 32-way lock striping for high-throughput message buffers, job queues, and LIFO/FIFO pipelines.
- **Zero-Fork Persistence (Snapshots & AOF)**:
  - **Snapshots (`dump.kvl`)**: Compact binary format with CRC32 data integrity, atomic file rename, and zero `fork()` (no page-table pauses or Copy-On-Write memory doubling).
  - **Append-Only Log (`kvllay.aof`)**: Asynchronous double-buffered logger with configurable fsync (`always`, `everysec`, `no`), decoupling client request latency from disk I/O.
- **Atomic Counters & Rate Limiting**: thread-safe counters with overflow checks for high-throughput rate limiters.
- **Non-blocking Multi-Reactor Network Engine**: Event-driven architecture (`epoll` on Linux with `eventfd` notification, `WSAPoll` on Windows) with a fixed-size worker pool (`--threads` / `--io-threads`), scaling to 50,000+ concurrent connections with sub-millisecond latencies and zero thread churn.
- **Transactions & Pipelining**: `MULTI` queues commands with `QUEUED`, `EXEC` executes them in FIFO order and returns a RESP array, and `DISCARD` clears the per-connection queue. Ordinary pipelined commands retain their request order.
- **Thread Safety & Lock Striping**: 32-way sharded store with 64-byte alignment (`alignas(64)`) to eliminate false sharing, allowing concurrent writes and reads across worker threads without lock contention.
- **Memory Manager Optimization & High-Performance Allocators**:
  - Pluggable allocators (`jemalloc` / `mimalloc` / `libc`) to eliminate heap fragmentation under intense key updates.
  - In-place string buffer reuse avoiding heap churn on `SET`, `SETEX`, `MSET`.
  - Zero-allocation numeric counters (`INCR`, `DECR`, etc.) via stack-allocated `std::to_chars`.
  - Granular `# Memory` metrics (`mem_allocator`, `used_memory_rss`, `used_memory_peak`, `mem_fragmentation_ratio`).
  - Active page purging (`purge_freed_memory`) on flush and background key evictions.
- **Ultra-Lightweight**: Docker image under **1.8 MB** (`scratch` static binary).
- **Cross-Platform**: unified codebase for Linux (POSIX sockets) and Windows (Winsock).

## Download Standalone Binary

You can download ready-to-run standalone binaries from [Releases](https://github.com/thekeny/kvllay/releases) (no dependencies required):
- **Linux (x86_64)**: `chmod +x kvllay-linux-x86_64 && ./kvllay-linux-x86_64`
- **Windows (x86_64)**: `.\kvllay-windows-x86_64.exe`

## Quickstart with Docker

```bash
# Run pre-built image directly from Docker Hub (no cloning required!)
docker run -d --name kvllay -p 6379:6379 kenyka/kvllay:latest

# Or with Docker Compose
docker compose up -d
```

## Build and Run Locally

### Building
```bash
# Standard build (libc allocator)
make compile

# Build with jemalloc (recommended for high-throughput write traffic)
make compile-jemalloc
# or: make compile MALLOC=jemalloc

# Build with mimalloc
make compile-mimalloc
# or: make compile MALLOC=mimalloc
```

### Running the Server
```bash
# Basic run (port 6379, listening on all interfaces 0.0.0.0)
make run

# Run with password authentication
./build/kvllay -p 6379 -a "mypassword"

# Restrict access to localhost only
./build/kvllay -p 6379 -h 127.0.0.1

# Run with periodic background snapshot (every 60 seconds)
./build/kvllay -p 6379 --save 60

# Run with Append-Only Log (AOF) persistence (fsync every second)
./build/kvllay -p 6379 --aof kvllay.aof --appendfsync everysec

# Run with both snapshots and AOF
./build/kvllay -p 6379 --snapshot dump.kvl --aof

# Run with memory limit and LRU eviction policy
./build/kvllay -p 6379 --maxmemory 256mb --maxmemory-policy allkeys-lru

# Run with custom number of worker event loop threads (default: auto-detected CPU cores)
./build/kvllay -p 6379 --threads 8

# View all options
./build/kvllay --help
```

## Usage with `redis-cli`

### Without password
```bash
redis-cli -p 6379
127.0.0.1:6379> PING
PONG
127.0.0.1:6379> SET user "Alex"
OK
127.0.0.1:6379> GET user
"Alex"
127.0.0.1:6379> SETEX temp 60 "secret"
OK
127.0.0.1:6379> TTL temp
(integer) 60
127.0.0.1:6379> INCR hits
(integer) 1
127.0.0.1:6379> INCRBY hits 10
(integer) 11
127.0.0.1:6379> RPUSH tasks "send_email" "process_payment" "notify_user"
(integer) 3
127.0.0.1:6379> LLEN tasks
(integer) 3
127.0.0.1:6379> LPOP tasks
"send_email"
127.0.0.1:6379> LRANGE tasks 0 -1
1) "process_payment"
2) "notify_user"
```

### With password
```bash
redis-cli -p 6379 -a "mypassword"
127.0.0.1:6379> GET user
"Alex"
```

## Benchmark Kvllay vs Redis

| Workload | kvllay v1.0.0 | Redis v8.x (8.8.0) | Comparison |
| :--- | :---: | :---: | :--- |
| **Pipelined GET (P=64, 100 clients)** | **5,665,723 RPS** | 2,531,645 RPS | **kvllay 2.24x faster (+124% / ~5x baseline)** |
| **Pipelined GET (P=32, 50 clients)** | **4,000,000 RPS** | 2,057,613 RPS | **kvllay 1.94x faster (+94.4%)** |
| **Pipelined SET (P=32, 50 clients)** | **3,257,329 RPS** | 1,488,095 RPS | **kvllay 2.19x faster (+119%)** |
| **Single-Client: GET** | **100,570 RPS** | 83,764 RPS | **kvllay +20.1% faster** |
| **Single-Client: SET** | **95,116 RPS** | 75,602 RPS | **kvllay +25.8% faster** |
| **Single-Client: INCR** | **98,450 RPS** | 76,200 RPS | **kvllay +29.2% faster** |
| **Single-Client: MSET (5 keys)** | **86,500 RPS** | 61,200 RPS | **kvllay +41.3% faster** |
| **Concurrent 50 Clients: INCR** | **145,200 RPS** | 136,799 RPS | **kvllay +6.1% faster** |
| **Latency p50 (Pipelined P=64)** | **0.551 ms** | 2.359 ms | **kvllay 4.3x lower latency** |
| **Latency p50 (Single-Client)** | **0.010 ms (10 μs)** | 0.013 ms (13 μs) | **kvllay 23% lower latency** |
| **Idle RAM** | **~4.1 MB** | ~15.2 MB | **kvllay 3.7x lighter** |
| **50,000 Keys RAM** | **~11.4 MB** | ~20.0 MB | **kvllay 43% less RAM** |
| **Docker Image Size** | **~1.6 MB** | ~140 MB | **kvllay 87x smaller** |
| **Cold Start** | **~3.2 ms** | ~7.6 ms | **kvllay 2.4x faster** |

![Throughput: Single-Client RPS](docs/images/benchmark_single_client.png)

![Throughput: Multi-Threaded RPS](docs/images/benchmark_multithreaded.png)

*See full benchmarks, methodology, and visual graphs in the [Russian Documentation](docs/ru.md#6-бенчмарк-сравнение-с-redis) and [English Documentation](docs/en.md#6-benchmark-comparison-with-redis).*
