#ifndef KVLLAY_SERVER_HPP
#define KVLLAY_SERVER_HPP

#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstring>
#include <constants.hpp>
#include <resp.hpp>
#include <store.hpp>
#include <snapshot.hpp>
#include <aof.hpp>
#include <commands.hpp>
#include <allocator.hpp>
#include <event_loop.hpp>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #ifdef _MSC_VER
        #pragma comment(lib, "ws2_32.lib")
    #endif
    using socket_t = SOCKET;
    #define IS_VALID_SOCKET(s) ((s) != INVALID_SOCKET)
    #define CLOSE_SOCKET(s) closesocket(s)
#else
    #include <signal.h>
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    using socket_t = int;
    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR   (-1)
    #define IS_VALID_SOCKET(s) ((s) >= 0)
    #define CLOSE_SOCKET(s) ::close(s)
#endif

namespace kvllay {

// Signal handlers may only perform async-signal-safe operations.  The handler
// therefore sets a sig_atomic_t flag and leaves all shutdown work to the main
// server loop.
namespace shutdown_signal {

inline volatile std::sig_atomic_t requested = 0;

inline void handler(int) noexcept {
    requested = 1;
}

inline void install() noexcept {
    requested = 0;

#ifdef _WIN32
    std::signal(SIGINT, handler);
    #ifdef SIGTERM
    std::signal(SIGTERM, handler);
    #endif
#else
    struct sigaction action{};
    sigemptyset(&action.sa_mask);
    action.sa_handler = handler;
    // Deliberately do not use SA_RESTART: if a platform still blocks in a
    // socket call, SIGINT/SIGTERM must be allowed to wake it up.
    action.sa_flags = 0;
    sigaction(SIGINT, &action, nullptr);
    sigaction(SIGTERM, &action, nullptr);
#endif
}

} // namespace shutdown_signal

struct ServerConfig {
    int port = constants::DEFAULT_PORT;
    std::string host = constants::DEFAULT_HOST;
    std::string password = "";

    bool snapshot_enabled = true;
    std::string snapshot_path = constants::DEFAULT_SNAPSHOT_FILE;
    uint64_t save_interval_secs = constants::DEFAULT_SAVE_INTERVAL_SECS;
    uint64_t save_changes = constants::DEFAULT_SAVE_CHANGES;

    bool aof_enabled = false;
    std::string aof_path = constants::DEFAULT_AOF_FILE;
    FsyncPolicy aof_fsync_policy = FsyncPolicy::EverySec;

    size_t maxmemory = constants::DEFAULT_MAXMEMORY;
    constants::MaxmemoryPolicy maxmemory_policy = constants::MaxmemoryPolicy::NoEviction;

    size_t io_threads = constants::DEFAULT_IO_THREADS;
};

class Server {
public:
    explicit Server(int port = constants::DEFAULT_PORT, std::string host = constants::DEFAULT_HOST, std::string password = "")
        : Server(ServerConfig{port, std::move(host), std::move(password)}) {}

    explicit Server(ServerConfig config)
        : config_(std::move(config)),
          running_(false),
          server_socket_(INVALID_SOCKET),
          store_(config_.maxmemory, config_.maxmemory_policy),
          snapshot_mgr_(config_.snapshot_path, config_.save_interval_secs, config_.save_changes),
          aof_mgr_(config_.aof_path, config_.aof_enabled, config_.aof_fsync_policy),
          command_handler_(store_, config_.snapshot_enabled ? &snapshot_mgr_ : nullptr, config_.aof_enabled ? &aof_mgr_ : nullptr),
          worker_pool_(config_.io_threads, command_handler_, config_.password) {
        init_network();
    }

    ~Server() {
        stop();
        cleanup_network();
    }

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    bool start() {
        if (config_.snapshot_enabled) {
            if (snapshot_mgr_.load(store_)) {
                std::cout << "[kvllay] Snapshot loaded from " << config_.snapshot_path << std::endl;
            }
        }

        if (config_.aof_enabled) {
            aof_mgr_.load(store_);
            if (!aof_mgr_.start()) {
                std::cerr << "[kvllay] Warning: Failed to start AOF background writer" << std::endl;
            } else {
                std::cout << "[kvllay] AOF persistence enabled (" << config_.aof_path << ", fsync: " 
                          << fsync_policy_to_string(config_.aof_fsync_policy) << ")" << std::endl;
            }
        }

        if (config_.maxmemory > 0) {
            std::cout << "[kvllay] Maxmemory limit: " << constants::format_memory_human(config_.maxmemory)
                      << " (policy: " << constants::maxmemory_policy_to_string(config_.maxmemory_policy) << ")" << std::endl;
        }

        if (config_.snapshot_enabled && config_.save_interval_secs > 0) {
            snapshot_mgr_.start_auto_save(store_);
            std::cout << "[kvllay] Auto-save enabled (every " << config_.save_interval_secs << "s if >= " 
                      << config_.save_changes << " changes)" << std::endl;
        }

        server_socket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (!IS_VALID_SOCKET(server_socket_)) {
            std::cerr << "[kvllay] Failed to create socket" << std::endl;
            return false;
        }

        int opt = 1;
#ifdef _WIN32
        setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
        setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(static_cast<uint16_t>(config_.port));
        if (config_.host == "0.0.0.0" || config_.host.empty()) {
            server_addr.sin_addr.s_addr = INADDR_ANY;
        } else {
            inet_pton(AF_INET, config_.host.c_str(), &server_addr.sin_addr);
        }

        if (bind(server_socket_, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
            std::cerr << "[kvllay] Failed to bind to " << config_.host << ":" << config_.port << std::endl;
            CLOSE_SOCKET(server_socket_);
            server_socket_ = INVALID_SOCKET;
            return false;
        }

        if (listen(server_socket_, SOMAXCONN) == SOCKET_ERROR) {
            std::cerr << "[kvllay] Failed to listen on socket" << std::endl;
            CLOSE_SOCKET(server_socket_);
            server_socket_ = INVALID_SOCKET;
            return false;
        }

        if (!set_socket_nonblocking(server_socket_)) {
            std::cerr << "[kvllay] Failed to make listening socket non-blocking" << std::endl;
            CLOSE_SOCKET(server_socket_);
            server_socket_ = INVALID_SOCKET;
            return false;
        }

        running_ = true;
        std::cout << "[kvllay] Server started on " << config_.host << ":" << config_.port;
        if (!config_.password.empty()) {
            std::cout << " (password protected)";
        }
        std::cout << " (io-threads: " << worker_pool_.thread_count()
                  << ", allocator: " << allocator::get_allocator_name() << ")" << std::endl;
        return true;
    }

    void run() {
        shutdown_signal::install();

        if (!running_ && !start()) {
            return;
        }

        worker_pool_.start();

        while (running_) {
            if (shutdown_signal::requested) {
                std::cout << "[kvllay] Shutdown signal received, stopping gracefully" << std::endl;
                break;
            }

            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            socket_t client_socket = accept(server_socket_, (sockaddr*)&client_addr, &client_len);

            if (!IS_VALID_SOCKET(client_socket)) {
                int err = SOCKET_ERRNO;
                if (err == ERR_INTR) {
                    continue;
                }

                if (err == ERR_AGAIN
#ifndef _WIN32
                    || err == EWOULDBLOCK
#endif
                ) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                    continue;
                }

                if (running_) {
                    std::cerr << "[kvllay] accept() failed, stopping server" << std::endl;
                }
                break;
            }

            int nodelay = 1;
#ifdef _WIN32
            setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, (const char*)&nodelay, sizeof(nodelay));
            int bufsize = 262144;
            setsockopt(client_socket, SOL_SOCKET, SO_RCVBUF, (const char*)&bufsize, sizeof(bufsize));
            setsockopt(client_socket, SOL_SOCKET, SO_SNDBUF, (const char*)&bufsize, sizeof(bufsize));
#else
            setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
            int bufsize = 262144;
            setsockopt(client_socket, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));
            setsockopt(client_socket, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize));
#endif

            worker_pool_.dispatch_connection(client_socket, config_.password.empty());
        }

        // stop() is idempotent, so this also covers an external stop() call
        // racing with the accept loop.
        stop();
    }

    void stop() {
        if (!running_.exchange(false)) {
            return;
        }

        if (IS_VALID_SOCKET(server_socket_)) {
#ifdef _WIN32
            shutdown(server_socket_, SD_BOTH);
#else
            shutdown(server_socket_, SHUT_RDWR);
#endif
            CLOSE_SOCKET(server_socket_);
            server_socket_ = INVALID_SOCKET;
        }

        worker_pool_.stop();

        if (config_.snapshot_enabled) {
            snapshot_mgr_.stop_auto_save();
            if (store_.dirty_count() > 0) {
                snapshot_mgr_.save_sync(store_);
            }
        }

        if (config_.aof_enabled) {
            aof_mgr_.stop();
        }
    }

    Store& store() {
        return store_;
    }

    SnapshotManager& snapshot_manager() {
        return snapshot_mgr_;
    }

    AofManager& aof_manager() {
        return aof_mgr_;
    }

    WorkerPool& worker_pool() {
        return worker_pool_;
    }

    const ServerConfig& config() const {
        return config_;
    }

private:
    ServerConfig config_;
    std::atomic<bool> running_;
    socket_t server_socket_;
    Store store_;
    SnapshotManager snapshot_mgr_;
    AofManager aof_mgr_;
    CommandHandler command_handler_;
    WorkerPool worker_pool_;

    void init_network() {
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#else
        signal(SIGPIPE, SIG_IGN);
#endif
    }

    void cleanup_network() {
#ifdef _WIN32
        WSACleanup();
#endif
    }
};

}

#endif // KVLLAY_SERVER_HPP
