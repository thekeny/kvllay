# AGENTS.md - Technical Guide for kvllay

## Overview
`kvllay` (pronounced *key-vi-lay* / «кей-ви-лей», from *key-value allay*) is a lightweight, in-memory key-value database written in C++17. It implements a subset of the Redis RESP2 protocol and supports raw inline text commands. It is compatible with `redis-cli` and Redis client libraries.

---

## Directory Structure
```
.
├── include/
│   └── kvllay/
│       ├── kvllay.hpp      # Umbrella header including all module headers
│       ├── constants.hpp   # Centralized version, network, store, and protocol constants
│       ├── allocator.hpp   # Memory allocator integration (jemalloc / mimalloc / libc, RSS, purge)
│       ├── resp.hpp        # RESP2 serialization and streaming command parser
│       ├── store.hpp       # In-memory key-value storage engine (thread-safe, TTL, GC)
│       ├── snapshot.hpp    # Binary point-in-time snapshot manager (CRC32, atomic rename, no fork)
│       ├── aof.hpp         # Append-Only Log manager (double-buffering, async fsync, no-fork rewrite)
│       ├── commands.hpp    # Command dispatcher and individual command handlers
│       ├── event_loop.hpp  # Non-blocking Event Loop & Worker Pool (epoll / WSAPoll Multi-Reactor)
│       └── server.hpp      # Cross-platform TCP socket server (POSIX / Winsock)
├── src/
│   ├── main.cpp            # Entry point, CLI argument parsing, server bootstrap
│   └── resources.rc        # Windows PE resource script with application icon
├── scripts/
│   └── release.sh          # Git tag and release automation script
├── logo.ico                # Multi-resolution icon for Windows executable
├── logo.png                # High-resolution (256x256) logo
├── logo-16x16.png         # Pixel-art (16x16) source logo
├── docs/
│   ├── ru.md               # Comprehensive Russian documentation and Redis benchmarks
│   └── en.md               # Comprehensive English documentation and Redis benchmarks
├── Dockerfile              # Multi-stage scratch build for ultra-minimal (< 2 MB) container
├── docker-compose.yml      # Local container orchestration
├── build/                  # Build artifact directory (ignored by git)
├── makefile                # Platform-detecting Makefile (Linux, Windows, Docker)
├── tasks.md                # Feature backlog and development roadmap
├── README.md               # User-facing project documentation
└── CLAUDE.md               # Agent reference pointer (@AGENTS.md)
```

---

## Core Components & Architecture

### 1. Storage Engine (`include/kvllay/store.hpp`)
- **Class**: `kvllay::Store`
- **Underlying Storage**: 32-way sharded storage (`std::array<Shard, 32> shards_`) with 64-bit FNV-1a `hash(key) & 31` dispatch over `std::string_view`.
- **Synchronization & Lock Striping**:
  - Each `Shard` is 64-byte aligned (`alignas(64)`) to eliminate CPU cache-line False Sharing.
  - Each shard has its own independent `mutable std::shared_mutex mutex`.
  - Single-key read operations (`get`, `ttl`) acquire `std::shared_lock` on only their specific shard.
  - Single-key write operations (`set`, `expire`, `incr`) acquire `std::unique_lock` on only their specific shard without blocking other shards.
  - Multi-key operations (`mget`, `mset`, `del`, `exists`) sort target shard indices in ascending order to guarantee deadlock-free multi-lock acquisition.
- **Pattern Matching (`keys`)**:
  - `*`: scans all shards in parallel/sequence.
  - Exact match: direct single-shard lookup (`shard_index(key)`).
  - Prefix/suffix/substring: scans shards under shared read lock.
- **Copy Semantics**: Non-copyable (copy constructor and copy assignment are deleted).

### 2. Protocol Engine (`include/kvllay/resp.hpp`)
- **Class**: `kvllay::Resp`
- **Parser Return Type**: `kvllay::ParseStatus` (`Success`, `Incomplete`, `Error`)
- **Zero-Copy Parser**: Parses commands directly into `std::vector<std::string_view>& args` without heap allocations for arguments, maintaining an unescape scratch buffer only when backslash escapes are encountered.
- **Protocol Formats Handled**:
  - **RESP2 Array**: Format `*<count>\r\n$<len>\r\n<data>\r\n...`. Handles negative lengths (null values) and tracks bytes consumed via `consumed_bytes`.
  - **Inline Text**: Space-delimited string terminated by `\r\n` or `\n`. Supports single and double-quoted arguments with backslash escaping (`\"`, `\'`).
- **Zero-Allocation Stream Serialization Helpers**:
  - `append_ok(out)`: appends `+OK\r\n`
  - `append_pong(out)`: appends `+PONG\r\n`
  - `append_error(out, err)`: appends `-ERR <err>\r\n`
  - `append_integer(out, val)`: appends `:<val>\r\n` using `std::to_chars`
  - `append_bulk_string(out, val)`: appends `$<len>\r\n<val>\r\n` using `std::to_chars`
  - `append_null_bulk_string(out)`: appends `$-1\r\n`
  - `append_empty_array(out)`: appends `*0\r\n`
  - `append_array_header(out, count)`: appends `*<count>\r\n`

### 3. Command Dispatcher (`include/kvllay/commands.hpp`)
- **Classes / Structs**:
  - `kvllay::CommandResult`: `{ std::string response; bool should_close; }` (legacy/standalone helper).
  - `kvllay::CommandHandler`: Dispatches tokenized command arguments to internal member functions via zero-alloc output buffering `dispatch(const std::vector<std::string_view>& args, bool& authenticated, std::string& out, bool& should_close)`.
- **Command Routing**:
  - Fast $O(1)$ jump-table dispatch based on `cmd.size()` and character tags, eliminating string allocations and cascading string comparisons.
  - `QUIT` appends `+OK\r\n` with `should_close = true` without checking auth.
  - `AUTH` evaluates credentials before global auth enforcement.
  - If server password is configured and client is unauthenticated, all other commands return `-NOAUTH Authentication required.\r\n`.
  - Unknown commands return `-ERR unknown command '<name>'\r\n`.
- **Current Implemented Commands**:
  - `AUTH [username] password`: Authenticates connection. Accepts optional username for Redis 6+ compatibility. Returns `-WRONGPASS ...` on mismatch.
  - `PING [message]`: Returns `+PONG\r\n` or the message as bulk string.
  - `SET key value [EX seconds|PX milliseconds] [NX|XX] [KEEPTTL]`: Sets a string value with optional expiration, conditional creation/update, or TTL preservation. Returns `+OK\r\n`, or `$-1\r\n` when `NX`/`XX` is not satisfied.
  - `GET key`: Returns bulk string or null bulk string (`$-1\r\n`).
  - `DEL key [key ...]`: Deletes keys, returns integer count of removed keys.
  - `EXISTS key [key ...]`: Returns integer count of existing keys.
  - `KEYS [pattern]`: Returns RESP array of matching keys. Default pattern is `*`.
  - `FLUSHDB` / `FLUSHALL`: Clears store, returns `+OK\r\n`.
  - `DBSIZE`: Returns integer count of total keys.
  - `ECHO message`: Returns argument as bulk string.
  - `COMMAND` / `COMMAND DOCS`: Returns `*0\r\n` to pass redis-cli handshake.
  - `INFO`: Returns server version, kvllay version, uptime in seconds, and keyspace count.
  - `EXPIRE key seconds`: Sets key TTL in seconds.
  - `PEXPIRE key milliseconds`: Sets key TTL in milliseconds.
  - `TTL key`: Returns remaining TTL in seconds (-1: no TTL, -2: key not found).
  - `PTTL key`: Returns remaining TTL in milliseconds.
  - `PERSIST key`: Clears expiration timer on key.
  - `SETEX key seconds value`: Sets key value with expiration in seconds atomically.
  - `INCR key`: Atomically increments integer value by 1.
  - `DECR key`: Atomically decrements integer value by 1.
  - `INCRBY key increment`: Atomically increments integer value by given delta.
  - `DECRBY key decrement`: Atomically decrements integer value by given delta.
  - `MGET key [key ...]`: Atomically retrieves multiple keys in a single roundtrip, returning array of bulk strings or nulls (`$-1`).
  - `MSET key value [key value ...]`: Atomically sets multiple key-value pairs in a single operation, clearing any existing TTLs.
  - `LPUSH key value [value ...]`: Prepends one or multiple values to the head of a list, returns length.
  - `RPUSH key value [value ...]`: Appends one or multiple values to the tail of a list, returns length.
  - `LPOP key [count]`: Removes and returns the first element(s) of a list. Auto-deletes key when empty.
  - `RPOP key [count]`: Removes and returns the last element(s) of a list. Auto-deletes key when empty.
  - `LLEN key`: Returns length of the list, or 0 if nonexistent.
  - `LRANGE key start stop`: Returns elements from start to stop (supports negative indexes).
  - `LINDEX key index`: Returns element by 0-based or negative index.
  - `TYPE key`: Returns type of key (`string`, `list`, or `none`).
  - `SELECT index`: Selects logical database (supports default DB 0, returns `-ERR DB index is out of range` for out-of-range indexes).
  - `CONFIG GET parameter`: Retrieves configuration parameter (`maxmemory`, `maxmemory-policy`, or `*`).
  - `CONFIG SET parameter value`: Dynamically sets configuration (`maxmemory`, `maxmemory-policy`).
  - `SAVE`: Synchronously dumps memory state to binary snapshot file (`dump.kvl`).
  - `BGSAVE`: Asynchronously dumps memory state to snapshot in a background thread without `fork()`.
  - `LASTSAVE`: Returns UNIX epoch timestamp of the most recent successful snapshot save.
  - `BGREWRITEAOF`: Compacts and rewrites AOF log in background from current in-memory state without `fork()`.

  - `MULTI`: Starts a transaction on the current connection; subsequent commands return `QUEUED`.
  - `EXEC`: Executes the queued commands in FIFO order and returns a RESP array of their results.
  - `DISCARD`: Clears the current connection's transaction queue.

### 4. Binary Snapshot Engine (`include/kvllay/snapshot.hpp`)
- **Class**: `kvllay::SnapshotManager`
- **Why it is better than Redis**:
  - Redis relies on Linux `fork()`, which causes page-table copying latency spikes (up to hundreds of milliseconds) and Copy-On-Write memory explosion (up to 2x RAM usage under write traffic, risking OOM kills). Redis snapshots are also not natively supported on Windows.
  - `kvllay` creates point-in-time snapshots in a background thread using a brief `std::shared_lock` read-lock (readers are never blocked, writes are paused for microseconds to extract references). Zero kernel COW page table bloat, zero memory doubling, and fully cross-platform (Linux & Windows).
- **Format**:
  - Header: `"KVLLAYS2"` (or legacy `"KVLLAYS1"`) (8 bytes) + timestamp (8 bytes) + record count (8 bytes).
  - Records (V2): `type` (1B: 0 for String, 1 for List) + `key_len` (4B) + `key` + [if String: `val_len` (4B) + `val`; if List: `count` (4B) + for each elem: `elem_len` (4B) + `elem`] + `expire_at_epoch_ms` (8B).
  - Footer: 32-bit CRC32 checksum verifying data integrity.
- **Atomic File Swapping**: Writes to `<file>.tmp.<pid>_<ts>`, flushes & fsyncs, then executes atomic `rename()` / `MoveFileExA`.

### 5. Append-Only Log Engine (`include/kvllay/aof.hpp`)
- **Class**: `kvllay::AofManager`
- **Why it is better than Redis**:
  - Redis event loop can stall when fsync disk operations backlog in background threads.
  - `kvllay` utilizes a double-buffered architecture: client threads write mutating commands (`SET`, `DEL`, `INCR`, `MSET`, etc.) into an active memory buffer in nanoseconds without blocking on disk I/O.
  - Dedicated background writer thread swaps active and flushing buffers, streams to disk sequentially, and executes periodic `fdatasync()` / `FlushFileBuffers()` every second (`everysec`) or per command (`always`).
- **Format & Interoperability**: Formatted as standard Redis RESP2 commands (`*<count>\r\n...`), allowing direct inspection, debugging, and pipe loading into standard Redis tools.
- **Background Rewrite**: Non-blocking `BGREWRITEAOF` dumps in-memory state to a temporary rewrite file, appends newly arriving mutations, and atomically replaces the active AOF log.

### 6. Networking & Server (`include/kvllay/server.hpp`, `include/kvllay/event_loop.hpp`)
- **Classes**: `kvllay::Server`, `kvllay::WorkerPool`, `kvllay::WorkerEventLoop`, `kvllay::Connection`.
- **Architecture (Multi-Reactor / Reactor-per-Thread)**:
  - Replaces old blocking «1 thread per socket» model with non-blocking event-driven reactors.
  - Main Acceptor thread binds and listens on `server_socket_`, accepting connections and setting `O_NONBLOCK` / `FIONBIO`, `TCP_NODELAY`, and expanded socket buffers (`SO_RCVBUF`/`SO_SNDBUF` 256 KB).
  - Fixed-size `WorkerPool` (by default `std::clamp(hardware_concurrency(), 1u, 16u)` threads, or configured via `--threads` / `--io-threads`) manages independent `WorkerEventLoop` instances.
  - Accepted sockets are dispatched across worker threads via lock-free round-robin (`fetch_add`).
  - **Linux Multiplexing**: Native `epoll` (`epoll_create1`, `epoll_ctl`, `epoll_wait`) with instant inter-thread notification via `eventfd`.
  - **Windows Multiplexing**: Native `WSAPoll` array handling.
  - **Non-blocking Write Buffering**: If a socket's send buffer fills up during a large response, residual bytes are queued in `Connection::write_buf` and `EPOLLOUT` is registered until fully flushed, preventing thread stalls.
  - **Strict Ordering**: Because each connection is pinned to exactly one worker event loop, pipelined RESP commands execute in strict FIFO order without mutex contention.
  - **Zero Allocations in Hot Path**: Worker threads reuse thread-local scratch vectors (`scratch_args_`, `scratch_unescape_buf_`, `scratch_out_batch_`).

### 7. CLI Entrypoint (`src/main.cpp`)
- Parses CLI flags and positional arguments:
  - `-p`, `--port <port>`: Port to listen on (default: `6379`).
  - `-h`, `--bind`, `--host <host>`: Bind IP (default: `0.0.0.0`).
  - `-a`, `--requirepass`, `--password <pass>`: Server auth password.
  - `--threads`, `--io-threads <n>`: Number of worker event loop threads (default: auto-detected CPU cores).
  - `--save <secs> [changes]`: Auto-save snapshot every `<secs>` if `[changes]` occurred.
  - `--snapshot`, `--save-file <file>`: Snapshot path (default: `dump.kvl`).
  - `--no-snapshot`: Disables snapshot file loading and saving.
  - `--aof [file]`: Enables Append-Only Log persistence (default: `kvllay.aof`).
  - `--no-aof`: Explicitly disables AOF persistence.
  - `--appendfsync <always|everysec|no>`: AOF fsync policy (default: `everysec`).
  - `--maxmemory <bytes|mb>`: Maximum memory limit (default: 0 = unlimited).
  - `--maxmemory-policy <policy>`: Eviction policy (`noeviction`, `allkeys-lru`, `volatile-lru`, `allkeys-random`, `volatile-ttl`).
  - Positional fallback: `kvllay [port] [host] [password]`.
  - `--help`: Prints usage options.
- Instantiates `kvllay::Server` and executes `server.run()`.

---

## Build and Run

### Compiler Requirements
- C++17 compatible compiler (`g++`, `clang++`, or `MSVC`).
- POSIX threads (`-pthread`) on Linux.
- Winsock (`-lws2_32`) on Windows.

### Make Targets
- `make compile`: Compiles binary into `build/kvllay` (`build/kvllay.exe` on Windows) with default allocator (`libc`).
- `make compile MALLOC=jemalloc` or `make compile-jemalloc`: Compiles with high-performance `jemalloc`.
- `make compile MALLOC=mimalloc` or `make compile-mimalloc`: Compiles with high-performance `mimalloc`.
- `make run`: Compiles and runs binary with default settings (`0.0.0.0:6379`).
- `make clean`: Removes binary from `build/`.
- `make release`: Interactive release tagging and GitHub push (or `make release VERSION=v1.0.0`).

### Manual Compilation
```bash
# Linux
g++ -std=c++17 -Wall -Wextra -O2 -I header -I include -I include/kvllay src/main.cpp -o build/kvllay -pthread

# Windows (MinGW)
windres -I . src/resources.rc -O coff -o build/resources.o
g++ -std=c++17 -Wall -Wextra -O2 -I header -I include -I include/kvllay -D _WIN32_WINNT=0x0A00 src/main.cpp build/resources.o -o build/kvllay.exe -lws2_32
```

---

## Protocol Verification & Interaction

Interact with and verify the running server using standard CLI tools:

### redis-cli
```bash
# Connect without authentication
redis-cli -p 6379 PING
redis-cli -p 6379 SET key value
redis-cli -p 6379 GET key

# Connect with authentication
redis-cli -p 6379 -a "mypassword" GET key
```

### Raw TCP (Netcat / Telnet)
```bash
# Inline command format
echo -e "PING\r\n" | nc 127.0.0.1 6379

# RESP2 array format
echo -e "*3\r\n\$3\r\nSET\r\n\$4\r\nname\r\n\$4\r\njohn\r\n" | nc 127.0.0.1 6379
```

---

## Guide for Extending the Codebase

When modifying or expanding `kvllay`, adhere to the following architecture patterns:

### 1. Adding a New Command
1. **Declare and implement logic in `include/kvllay/store.hpp`** (if storage mutation or lookup is required):
   - Add thread-safe method using `std::shared_lock` for read-only operations or `std::unique_lock` for mutations.
2. **Add handler method in `include/kvllay/commands.hpp`**:
   - Signature: `CommandResult handle_<name>(const std::vector<std::string>& args)`.
   - Validate argument count (`args.size()`). Return `Resp::error(...)` on invalid count or format.
   - Call `store_` method and serialize return value using `Resp::*` helper functions.
3. **Route in `CommandHandler::dispatch`**:
   - Check normalized `cmd == "<NAME>"`.
   - Invoke `handle_<name>(args)`.
4. **Verification**:
   - Verify command handling against expected RESP2 responses using `redis-cli` or raw TCP sockets.
5. **Update documentation**:
   - Add the command to `README.md`, `tasks.md`, and update this file (`AGENTS.md`).

### 2. Implementing Expiration (TTL)
- Storage representation: Change `data_` value type or add secondary index in `Store` (e.g., `std::unordered_map<std::string, uint64_t> expires_`).
- Expired key eviction:
  - Passive: Check expiration timestamp inside `get()`, `exists()`, `keys()`. If expired, erase key and treat as nonexistent.
  - Active: Background thread running periodic sweep with `unique_lock`.

### 3. Implementing Persistence (AOF / Snapshot)
- AOF: Append mutating command raw RESP strings to file on successful execution in `CommandHandler` or dedicated worker thread.
- On startup, read AOF file and feed through `Resp::parse_command` directly into `CommandHandler`.

---

## Coding Conventions and Invariants
- **Language**: C++17 standard features only. Avoid external dependencies.
- **Header Structure**: All core logic resides in headers (`include/kvllay/*.hpp`). Header inclusion is guarded with `#ifndef` and `#pragma once`.
- **Namespace**: All core types are inside `namespace kvllay`.
- **Thread Safety**: Storage modifications MUST synchronize via `mutex_` in `Store`. Handlers and network layers must not bypass the storage mutex.
- **Protocol Precision**: All responses must strictly adhere to the Redis RESP2 format (`\r\n` line endings, valid integer formats, null representation).
- **Transaction State**: Transaction queues are connection-local and must never be stored in the shared `CommandHandler`; ordinary pipelining outside `MULTI` must continue to preserve FIFO response order.
