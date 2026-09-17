#ifndef KVLLAY_EVENT_LOOP_HPP
#define KVLLAY_EVENT_LOOP_HPP

#pragma once

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <cstring>
#include <algorithm>
#include <cstdint>
#include <chrono>

#include <constants.hpp>
#include <resp.hpp>
#include <commands.hpp>

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
    #define SOCKET_ERRNO WSAGetLastError()
    #define ERR_AGAIN WSAEWOULDBLOCK
    #define ERR_INTR WSAEINTR
    #ifndef MSG_NOSIGNAL
    #define MSG_NOSIGNAL 0
    #endif

    // Fallback constants for epoll compatibility stubs on Windows
    #ifndef EPOLLIN
    #define EPOLLIN 0x001
    #endif
    #ifndef EPOLLOUT
    #define EPOLLOUT 0x004
    #endif
    #ifndef EPOLLRDHUP
    #define EPOLLRDHUP 0x2000
    #endif
    #ifndef EPOLLERR
    #define EPOLLERR 0x008
    #endif
    #ifndef EPOLLHUP
    #define EPOLLHUP 0x010
    #endif
#else
    #include <sys/types.h>
    #include <sys/socket.h>
    #include <sys/epoll.h>
    #include <sys/eventfd.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <signal.h>
    using socket_t = int;
    #define INVALID_SOCKET (-1)
    #define SOCKET_ERROR   (-1)
    #define IS_VALID_SOCKET(s) ((s) >= 0)
    #define CLOSE_SOCKET(s) ::close(s)
    #define SOCKET_ERRNO errno
    #define ERR_AGAIN EAGAIN
    #define ERR_INTR EINTR
    #ifndef MSG_NOSIGNAL
    #define MSG_NOSIGNAL 0x4000
    #endif
#endif

namespace kvllay {

inline bool set_socket_nonblocking(socket_t fd) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(fd, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return false;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
#endif
}

inline std::atomic<uint64_t> g_next_client_id{1};

struct Connection {
    socket_t fd = INVALID_SOCKET;
    bool authenticated = false;
    ClientSession client;
    std::string read_buf;
    size_t read_offset = 0;
    std::string write_buf;
    size_t write_offset = 0;
    bool should_close = false;
    bool in_transaction = false;
    std::vector<std::vector<std::string>> transaction_queue;
};

class WorkerEventLoop {
public:
    WorkerEventLoop(size_t id, CommandHandler& command_handler, const std::string& password)
        : id_(id),
          command_handler_(command_handler),
          password_(password),
          running_(false)
#ifndef _WIN32
        , epoll_fd_(INVALID_SOCKET),
          wakeup_fd_(INVALID_SOCKET)
#endif
    {
        scratch_args_.reserve(32);
        scratch_out_batch_.reserve(constants::CLIENT_BUFFER_SIZE * 2);
        scratch_unescape_buf_.reserve(256);
    }

    ~WorkerEventLoop() {
        stop();
    }

    WorkerEventLoop(const WorkerEventLoop&) = delete;
    WorkerEventLoop& operator=(const WorkerEventLoop&) = delete;

    void start() {
        if (running_) return;
        running_ = true;

#ifndef _WIN32
        epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
        if (epoll_fd_ == INVALID_SOCKET) {
            std::cerr << "[kvllay] Error: failed to create epoll instance" << std::endl;
            running_ = false;
            return;
        }

        wakeup_fd_ = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        if (wakeup_fd_ == INVALID_SOCKET) {
            std::cerr << "[kvllay] Error: failed to create eventfd" << std::endl;
            CLOSE_SOCKET(epoll_fd_);
            epoll_fd_ = INVALID_SOCKET;
            running_ = false;
            return;
        }

        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = wakeup_fd_;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, wakeup_fd_, &ev) != 0) {
            std::cerr << "[kvllay] Error: failed to add wakeup_fd to epoll" << std::endl;
            CLOSE_SOCKET(wakeup_fd_);
            CLOSE_SOCKET(epoll_fd_);
            wakeup_fd_ = INVALID_SOCKET;
            epoll_fd_ = INVALID_SOCKET;
            running_ = false;
            return;
        }
#endif

        thread_ = std::thread(&WorkerEventLoop::run_loop, this);
    }

    void stop() {
        if (!running_.exchange(false)) return;

        wakeup();

        if (thread_.joinable()) {
            thread_.join();
        }

#ifndef _WIN32
        if (IS_VALID_SOCKET(wakeup_fd_)) {
            CLOSE_SOCKET(wakeup_fd_);
            wakeup_fd_ = INVALID_SOCKET;
        }
        if (IS_VALID_SOCKET(epoll_fd_)) {
            CLOSE_SOCKET(epoll_fd_);
            epoll_fd_ = INVALID_SOCKET;
        }
#endif

        for (auto& pair : connections_) {
            command_handler_.unregister_client(pair.second.client.id);
            CLOSE_SOCKET(pair.first);
        }
        connections_.clear();

        std::lock_guard<std::mutex> lock(pending_mutex_);
        for (const auto& item : pending_conns_) {
            CLOSE_SOCKET(item.fd);
        }
        pending_conns_.clear();
    }

    void add_client(socket_t client_socket, bool authenticated) {
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_conns_.push_back({client_socket, authenticated});
        }
        wakeup();
    }

    size_t connection_count() const {
        return connections_.size();
    }

    size_t id() const {
        return id_;
    }

private:
    struct PendingConn {
        socket_t fd;
        bool authenticated;
    };

    size_t id_;
    CommandHandler& command_handler_;
    const std::string& password_;
    std::atomic<bool> running_;
    std::thread thread_;

    std::mutex pending_mutex_;
    std::vector<PendingConn> pending_conns_;
    std::unordered_map<socket_t, Connection> connections_;

    std::vector<std::string_view> scratch_args_;
    std::string scratch_unescape_buf_;
    std::string scratch_out_batch_;

#ifndef _WIN32
    int epoll_fd_;
    int wakeup_fd_;
#endif

    void wakeup() {
#ifndef _WIN32
        if (IS_VALID_SOCKET(wakeup_fd_)) {
            uint64_t one = 1;
            ssize_t res = ::write(wakeup_fd_, &one, sizeof(one));
            (void)res;
        }
#endif
    }

    void process_pending() {
        std::vector<PendingConn> to_add;
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            if (pending_conns_.empty()) return;
            to_add.swap(pending_conns_);
        }

        for (const auto& item : to_add) {
            set_socket_nonblocking(item.fd);
            uint64_t client_id = g_next_client_id.fetch_add(1, std::memory_order_relaxed);
#ifndef _WIN32
            epoll_event ev{};
            ev.events = EPOLLIN | EPOLLRDHUP;
            ev.data.fd = item.fd;
            if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, item.fd, &ev) == 0) {
                Connection conn;
                conn.fd = item.fd;
                conn.authenticated = item.authenticated;
                conn.client.id = client_id;
                conn.read_buf.reserve(constants::CLIENT_BUFFER_SIZE);
                connections_.emplace(item.fd, std::move(conn));
                command_handler_.register_client(connections_.at(item.fd).client);
            } else {
                CLOSE_SOCKET(item.fd);
            }
#else
            Connection conn;
            conn.fd = item.fd;
            conn.authenticated = item.authenticated;
            conn.client.id = client_id;
            conn.read_buf.reserve(constants::CLIENT_BUFFER_SIZE);
            connections_.emplace(item.fd, std::move(conn));
            command_handler_.register_client(connections_.at(item.fd).client);
#endif
        }
    }

#ifndef _WIN32
    void modify_epoll(socket_t fd, uint32_t events) {
        epoll_event ev{};
        ev.events = events;
        ev.data.fd = fd;
        epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ev);
    }

    void set_write_interest(socket_t fd, bool enable) {
        uint32_t events = EPOLLIN | EPOLLRDHUP;
        if (enable) {
            events |= EPOLLOUT;
        }
        modify_epoll(fd, events);
    }
#else
    void modify_epoll(socket_t /*fd*/, uint32_t /*events*/) {}
    void set_write_interest(socket_t /*fd*/, bool /*enable*/) {}
#endif

    void remove_connection(socket_t fd) {
#ifndef _WIN32
        epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
#endif
        auto it = connections_.find(fd);
        if (it != connections_.end()) {
            command_handler_.unregister_client(it->second.client.id);
        }
        CLOSE_SOCKET(fd);
        connections_.erase(fd);
    }

    bool handle_write(Connection& conn) {
        if (conn.write_buf.empty()) {
            set_write_interest(conn.fd, false);
            return true;
        }

        while (conn.write_offset < conn.write_buf.size()) {
            size_t remaining = conn.write_buf.size() - conn.write_offset;
            int sent = send(conn.fd, conn.write_buf.data() + conn.write_offset, static_cast<int>(remaining), MSG_NOSIGNAL);
            if (sent > 0) {
                conn.write_offset += sent;
            } else if (sent < 0) {
                int err = SOCKET_ERRNO;
                if (err == ERR_AGAIN || err == EWOULDBLOCK) {
                    return true;
                } else if (err == ERR_INTR) {
                    continue;
                } else {
                    return false;
                }
            }
        }

        conn.write_buf.clear();
        conn.write_offset = 0;
        set_write_interest(conn.fd, false);
        return true;
    }

    static bool is_command(std::string_view command, std::string_view expected) {
        return CommandHandler::iequals(command, expected);
    }

    void dispatch_connection_command(Connection& conn,
                                     const std::vector<std::string_view>& args,
                                     std::string& out) {
        if (args.empty()) {
            Resp::append_error(out, "empty command");
            return;
        }

        const std::string_view command = args[0];

        if (is_command(command, "QUIT")) {
            Resp::append_ok(out);
            conn.should_close = true;
            conn.in_transaction = false;
            conn.transaction_queue.clear();
            return;
        }

        // Authentication is checked before transaction handling. AUTH and HELLO
        // must remain usable before MULTI, while all other commands are rejected.
        if (!password_.empty() && !conn.authenticated &&
            !is_command(command, "AUTH") && !is_command(command, "HELLO")) {
            Resp::append_error(out, "NOAUTH Authentication required.");
            return;
        }

        if (is_command(command, "MULTI")) {
            if (args.size() != 1) {
                Resp::append_error(out, "wrong number of arguments for 'multi' command");
            } else if (conn.in_transaction) {
                Resp::append_error(out, "MULTI calls can not be nested");
            } else {
                conn.in_transaction = true;
                conn.transaction_queue.clear();
                Resp::append_ok(out);
            }
            return;
        }

        if (is_command(command, "DISCARD")) {
            if (args.size() != 1) {
                Resp::append_error(out, "wrong number of arguments for 'discard' command");
            } else if (!conn.in_transaction) {
                Resp::append_error(out, "DISCARD without MULTI");
            } else {
                conn.in_transaction = false;
                conn.transaction_queue.clear();
                Resp::append_ok(out);
            }
            return;
        }

        if (is_command(command, "EXEC")) {
            if (args.size() != 1) {
                Resp::append_error(out, "wrong number of arguments for 'exec' command");
            } else if (!conn.in_transaction) {
                Resp::append_error(out, "EXEC without MULTI");
            } else {
                conn.in_transaction = false;
                Resp::append_array_header(out, conn.transaction_queue.size());
                for (const auto& queued : conn.transaction_queue) {
                    scratch_args_.clear();
                    scratch_args_.reserve(queued.size());
                    for (const auto& argument : queued) {
                        scratch_args_.emplace_back(argument);
                    }
                    command_handler_.dispatch(scratch_args_, out, conn.authenticated,
                                              password_, conn.should_close, conn.client);
                }
                conn.transaction_queue.clear();
            }
            return;
        }

        if (conn.in_transaction) {
            constexpr size_t MAX_QUEUED_COMMANDS = 32768;
            if (conn.transaction_queue.size() >= MAX_QUEUED_COMMANDS) {
                Resp::append_error(out, "ERR transaction queue limit reached");
                return;
            }
            std::vector<std::string> queued;
            queued.reserve(args.size());
            for (const auto argument : args) {
                queued.emplace_back(argument);
            }
            conn.transaction_queue.emplace_back(std::move(queued));
            Resp::append_simple_string(out, "QUEUED");
            return;
        }

        command_handler_.dispatch(args, out, conn.authenticated, password_,
                                   conn.should_close, conn.client);
    }

    bool handle_read(Connection& conn) {
        constexpr size_t READ_CHUNK = 8192;
        char buf[READ_CHUNK];

        while (true) {
            int n = recv(conn.fd, buf, static_cast<int>(sizeof(buf)), 0);
            if (n > 0) {
                conn.read_buf.append(buf, n);
                if (static_cast<size_t>(n) < sizeof(buf)) {
                    break;
                }
            } else if (n == 0) {
                return false;
            } else {
                int err = SOCKET_ERRNO;
                if (err == ERR_AGAIN || err == EWOULDBLOCK) {
                    break;
                } else if (err == ERR_INTR) {
                    continue;
                } else {
                    return false;
                }
            }
        }

        scratch_out_batch_.clear();

        while (conn.read_offset < conn.read_buf.size()) {
            size_t consumed = 0;
            std::string_view sv(conn.read_buf.data() + conn.read_offset, conn.read_buf.size() - conn.read_offset);
            scratch_args_.clear();
            scratch_unescape_buf_.clear();

            ParseStatus status = Resp::parse_command(sv, scratch_args_, consumed, scratch_unescape_buf_);
            if (status == ParseStatus::Success) {
                conn.read_offset += consumed;
                dispatch_connection_command(conn, scratch_args_, scratch_out_batch_);
                if (conn.should_close) {
                    break;
                }
            } else if (status == ParseStatus::Incomplete) {
                break;
            } else {
                Resp::append_error(scratch_out_batch_, "Protocol error");
                conn.should_close = true;
                break;
            }
        }

        if (conn.read_offset > 0) {
            if (conn.read_offset >= conn.read_buf.size()) {
                conn.read_buf.clear();
                conn.read_offset = 0;
            } else if (conn.read_offset >= constants::CLIENT_BUFFER_SIZE || conn.read_buf.size() > constants::CLIENT_BUFFER_SIZE * 4) {
                conn.read_buf.erase(0, conn.read_offset);
                conn.read_offset = 0;
            }
        }

        if (!scratch_out_batch_.empty()) {
            if (!conn.write_buf.empty()) {
                conn.write_buf.append(scratch_out_batch_);
            } else {
                size_t total = scratch_out_batch_.size();
                int sent = send(conn.fd, scratch_out_batch_.data(), static_cast<int>(total), MSG_NOSIGNAL);
                if (sent > 0) {
                    if (static_cast<size_t>(sent) < total) {
                        conn.write_buf.assign(scratch_out_batch_.data() + sent, total - sent);
                        conn.write_offset = 0;
                        set_write_interest(conn.fd, true);
                    }
                } else if (sent < 0) {
                    int err = SOCKET_ERRNO;
                    if (err == ERR_AGAIN || err == EWOULDBLOCK) {
                        conn.write_buf = std::move(scratch_out_batch_);
                        conn.write_offset = 0;
                        set_write_interest(conn.fd, true);
                    } else if (err != ERR_INTR) {
                        return false;
                    }
                }
            }
        }

        return true;
    }

#ifndef _WIN32
    void run_loop() {
        constexpr int MAX_EVENTS = constants::MAX_EVENTS_PER_LOOP;
        epoll_event events[MAX_EVENTS];

        while (running_) {
            int nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, constants::EPOLL_TIMEOUT_MS);
            if (nfds < 0) {
                if (errno == EINTR) continue;
                if (!running_) break;
            }

            for (int i = 0; i < nfds; ++i) {
                int fd = events[i].data.fd;
                uint32_t ev = events[i].events;

                if (fd == wakeup_fd_) {
                    uint64_t val = 0;
                    ssize_t r = ::read(wakeup_fd_, &val, sizeof(val));
                    (void)r;
                    process_pending();
                    continue;
                }

                auto it = connections_.find(fd);
                if (it == connections_.end()) {
                    continue;
                }

                Connection& conn = it->second;
                bool closed = false;

                if (ev & EPOLLOUT) {
                    if (!handle_write(conn)) {
                        closed = true;
                    }
                }

                if (!closed && (ev & EPOLLIN)) {
                    if (!handle_read(conn)) {
                        closed = true;
                    }
                }

                if (!closed && (ev & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))) {
                    if (conn.write_buf.empty()) {
                        closed = true;
                    }
                }

                if (closed || (conn.should_close && conn.write_buf.empty())) {
                    remove_connection(fd);
                }
            }

            process_pending();
        }
    }
#else
    void run_loop() {
        std::vector<WSAPOLLFD> poll_fds;

        while (running_) {
            process_pending();

            poll_fds.clear();
            poll_fds.reserve(connections_.size());
            for (const auto& pair : connections_) {
                WSAPOLLFD pfd{};
                pfd.fd = pair.first;
                pfd.events = POLLRDNORM;
                if (!pair.second.write_buf.empty()) {
                    pfd.events |= POLLWRNORM;
                }
                poll_fds.push_back(pfd);
            }

            if (poll_fds.empty()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }

            int ret = WSAPoll(poll_fds.data(), static_cast<ULONG>(poll_fds.size()), constants::EPOLL_TIMEOUT_MS);
            if (ret <= 0) {
                continue;
            }

            for (const auto& pfd : poll_fds) {
                if (pfd.revents == 0) continue;

                auto it = connections_.find(pfd.fd);
                if (it == connections_.end()) continue;

                Connection& conn = it->second;
                bool closed = false;

                if (pfd.revents & POLLWRNORM) {
                    if (!handle_write(conn)) {
                        closed = true;
                    }
                }

                if (!closed && (pfd.revents & (POLLRDNORM | POLLRDBAND))) {
                    if (!handle_read(conn)) {
                        closed = true;
                    }
                }

                if (!closed && (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))) {
                    if (conn.write_buf.empty()) {
                        closed = true;
                    }
                }

                if (closed || (conn.should_close && conn.write_buf.empty())) {
                    remove_connection(conn.fd);
                }
            }
        }
    }
#endif
};

class WorkerPool {
public:
    WorkerPool(size_t num_threads, CommandHandler& command_handler, const std::string& password)
        : num_threads_(num_threads), round_robin_index_(0) {
        if (num_threads_ == 0) {
            unsigned int hw = std::thread::hardware_concurrency();
            num_threads_ = hw > 0 ? std::clamp(hw, 1u, 16u) : 4;
        }
        workers_.reserve(num_threads_);
        for (size_t i = 0; i < num_threads_; ++i) {
            workers_.push_back(std::make_unique<WorkerEventLoop>(i, command_handler, password));
        }
    }

    ~WorkerPool() {
        stop();
    }

    WorkerPool(const WorkerPool&) = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;

    void start() {
#ifndef _WIN32
        signal(SIGPIPE, SIG_IGN);
#endif
        for (auto& worker : workers_) {
            worker->start();
        }
    }

    void stop() {
        for (auto& worker : workers_) {
            worker->stop();
        }
    }

    void dispatch_connection(socket_t client_socket, bool authenticated) {
        if (workers_.empty()) {
            CLOSE_SOCKET(client_socket);
            return;
        }
        size_t idx = round_robin_index_.fetch_add(1, std::memory_order_relaxed) % workers_.size();
        workers_[idx]->add_client(client_socket, authenticated);
    }

    size_t thread_count() const {
        return workers_.size();
    }

    size_t total_connections() const {
        size_t total = 0;
        for (const auto& worker : workers_) {
            total += worker->connection_count();
        }
        return total;
    }

private:
    size_t num_threads_;
    std::atomic<size_t> round_robin_index_;
    std::vector<std::unique_ptr<WorkerEventLoop>> workers_;
};

} // namespace kvllay

#endif // KVLLAY_EVENT_LOOP_HPP
