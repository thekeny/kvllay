#ifndef KVLLAY_AOF_HPP
#define KVLLAY_AOF_HPP

#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <algorithm>
#include <cstdint>
#include <limits>
#include <cstdio>
#include <constants.hpp>
#include <resp.hpp>
#include <store.hpp>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
    #include <io.h>
    #include <process.h>
    #define AOF_GETPID() _getpid()
#else
    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/types.h>
    #define AOF_GETPID() getpid()
#endif

namespace kvllay {

enum class FsyncPolicy {
    Always,
    EverySec,
    No
};

inline FsyncPolicy parse_fsync_policy(const std::string& str) {
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s == "always") return FsyncPolicy::Always;
    if (s == "no") return FsyncPolicy::No;
    return FsyncPolicy::EverySec;
}

inline std::string fsync_policy_to_string(FsyncPolicy policy) {
    switch (policy) {
        case FsyncPolicy::Always: return "always";
        case FsyncPolicy::No: return "no";
        case FsyncPolicy::EverySec:
        default: return "everysec";
    }
}

class AofManager {
public:
    explicit AofManager(std::string aof_path = constants::DEFAULT_AOF_FILE,
                        bool enabled = false,
                        FsyncPolicy fsync_policy = FsyncPolicy::EverySec)
        : aof_path_(std::move(aof_path)),
          enabled_(enabled),
          fsync_policy_(fsync_policy),
          file_handle_(nullptr),
          running_(false),
          rewrite_in_progress_(false) {
        last_fsync_time_ = std::chrono::steady_clock::now();
    }

    ~AofManager() {
        stop();
    }

    AofManager(const AofManager&) = delete;
    AofManager& operator=(const AofManager&) = delete;

    bool is_enabled() const {
        return enabled_;
    }

    void set_enabled(bool enabled) {
        enabled_ = enabled;
    }

    const std::string& aof_path() const {
        return aof_path_;
    }

    void set_aof_path(const std::string& path) {
        aof_path_ = path;
    }

    FsyncPolicy fsync_policy() const {
        return fsync_policy_;
    }

    void set_fsync_policy(FsyncPolicy policy) {
        fsync_policy_ = policy;
    }

    bool is_rewriting() const {
        return rewrite_in_progress_.load();
    }

    bool start() {
        if (!enabled_) {
            return true;
        }

        std::lock_guard<std::mutex> lifecycle_lock(lifecycle_mutex_);
        if (running_) {
            return true;
        }

        bool opened = false;
        {
            std::lock_guard<std::mutex> lock(buffer_mutex_);
            file_handle_ = fopen(aof_path_.c_str(), "ab");
            opened = file_handle_ != nullptr;
        }
        if (!opened) {
            std::cerr << "[kvllay] Failed to open AOF file " << aof_path_ << std::endl;
            return false;
        }

        running_ = true;
        writer_thread_ = std::thread(&AofManager::background_writer_loop, this);
        return true;
    }

    void stop() {
        std::lock_guard<std::mutex> lifecycle_lock(lifecycle_mutex_);
        running_.store(false);

        {
            std::lock_guard<std::mutex> lock(buffer_mutex_);
            cv_.notify_all();
        }

        if (writer_thread_.joinable()) {
            writer_thread_.join();
        }

        // A rewrite uses this object and may replace file_handle_.  It must
        // finish before the manager (and its FILE*) can be destroyed.
        {
            std::lock_guard<std::mutex> lock(rewrite_mutex_);
            if (rewrite_thread_.joinable()) {
                rewrite_thread_.join();
            }
        }

        flush_sync();

        {
            std::lock_guard<std::mutex> lock(buffer_mutex_);
            if (file_handle_) {
                sync_file_locked();
                fclose(file_handle_);
                file_handle_ = nullptr;
            }
        }
    }

    void append(const std::vector<std::string>& args) {
        if (!enabled_ || args.empty()) {
            return;
        }

        std::string serialized = Resp::array(args);

        {
            std::lock_guard<std::mutex> lock(buffer_mutex_);
            active_buffer_.append(serialized);
        }

        if (fsync_policy_ == FsyncPolicy::Always) {
            flush_sync();
        } else {
            cv_.notify_one();
        }
    }

    void append(const std::vector<std::string_view>& args) {
        if (!enabled_ || args.empty()) {
            return;
        }

        std::string serialized;
        Resp::append_array_header(serialized, args.size());
        for (const auto& a : args) {
            Resp::append_bulk_string(serialized, a);
        }

        {
            std::lock_guard<std::mutex> lock(buffer_mutex_);
            active_buffer_.append(serialized);
        }

        if (fsync_policy_ == FsyncPolicy::Always) {
            flush_sync();
        } else {
            cv_.notify_one();
        }
    }

    bool load(Store& store) {
        if (!enabled_) {
            return true;
        }

        FILE* fp = fopen(aof_path_.c_str(), "rb");
        if (!fp) {
            return true;
        }

        constexpr size_t CHUNK_SIZE = 65536;
        std::vector<char> read_buf(CHUNK_SIZE);
        std::string parse_buffer;

        while (true) {
            size_t bytes_read = fread(read_buf.data(), 1, CHUNK_SIZE, fp);
            if (bytes_read > 0) {
                parse_buffer.append(read_buf.data(), bytes_read);
            }

            while (!parse_buffer.empty()) {
                std::vector<std::string> args;
                size_t consumed = 0;
                ParseStatus status = Resp::parse_command(parse_buffer, args, consumed);

                if (status == ParseStatus::Success) {
                    parse_buffer.erase(0, consumed);
                    replay_command(store, args);
                } else if (status == ParseStatus::Incomplete) {
                    if (bytes_read == 0) {
                        parse_buffer.clear();
                        break;
                    }
                    break;
                } else {
                    std::cerr << "[kvllay] Warning: Corrupt command encountered in AOF, stopping replay" << std::endl;
                    parse_buffer.clear();
                    break;
                }
            }

            if (bytes_read == 0) {
                break;
            }
        }

        fclose(fp);
        return true;
    }

    bool rewrite_async(const Store& store) {
        if (!enabled_) {
            return false;
        }

        bool expected = false;
        if (!rewrite_in_progress_.compare_exchange_strong(expected, true)) {
            return false;
        }

        auto entries = store.get_all_entries();

        {
            std::lock_guard<std::mutex> lock(rewrite_mutex_);
            if (!running_) {
                rewrite_in_progress_.store(false);
                return false;
            }

            // A completed rewrite remains joinable until it is collected.
            // Reap it before starting the next rewrite.
            if (rewrite_thread_.joinable()) {
                std::thread previous = std::move(rewrite_thread_);
                previous.join();
            }

            rewrite_thread_ = std::thread([this, entries = std::move(entries)]() mutable {
                perform_rewrite(entries);
                rewrite_in_progress_.store(false);
            });
        }

        return true;
    }

private:
    void background_writer_loop() {
        while (running_) {
            {
                std::unique_lock<std::mutex> lock(buffer_mutex_);
                cv_.wait_for(lock, std::chrono::milliseconds(constants::AOF_BUFFER_FLUSH_INTERVAL_MS), [this]() {
                    return !running_ || !active_buffer_.empty();
                });

                if (!active_buffer_.empty()) {
                    flushing_buffer_.swap(active_buffer_);
                }

                // Keep the mutex for the entire FILE* operation.  A rewrite
                // closes and replaces file_handle_ while holding this same
                // mutex, so it cannot invalidate the pointer mid-write or
                // race with flushing_buffer_.
                if (!flushing_buffer_.empty() && file_handle_) {
                    fwrite(flushing_buffer_.data(), 1, flushing_buffer_.size(), file_handle_);
                    fflush(file_handle_);
                    flushing_buffer_.clear();
                }

                if (fsync_policy_ == FsyncPolicy::EverySec && file_handle_) {
                    auto now = std::chrono::steady_clock::now();
                    if (now - last_fsync_time_ >= std::chrono::seconds(1)) {
                        sync_file_locked();
                        last_fsync_time_ = now;
                    }
                }
            }
        }
    }

    void flush_sync() {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        if (!active_buffer_.empty() && file_handle_) {
            fwrite(active_buffer_.data(), 1, active_buffer_.size(), file_handle_);
            active_buffer_.clear();
        }
        if (!flushing_buffer_.empty() && file_handle_) {
            fwrite(flushing_buffer_.data(), 1, flushing_buffer_.size(), file_handle_);
            flushing_buffer_.clear();
        }
        if (file_handle_) {
            fflush(file_handle_);
            if (fsync_policy_ == FsyncPolicy::Always || fsync_policy_ == FsyncPolicy::EverySec) {
                sync_file_locked();
            }
        }
    }

    // Must be called while buffer_mutex_ is held.  Keeping the FILE* lifetime
    // and every stdio operation under the same mutex prevents rewrite from
    // closing the stream while another thread is using it.
    void sync_file_locked() {
        if (!file_handle_) return;
#ifdef _WIN32
        int fd = _fileno(file_handle_);
        HANDLE h = (HANDLE)_get_osfhandle(fd);
        if (h != INVALID_HANDLE_VALUE) {
            FlushFileBuffers(h);
        }
#else
        int fd = fileno(file_handle_);
        #if defined(__APPLE__) || defined(__FreeBSD__)
        fsync(fd);
        #else
        fdatasync(fd);
        #endif
#endif
    }

    void perform_rewrite(const std::vector<Store::DumpEntry>& entries) {
        uint64_t now_epoch = static_cast<uint64_t>(time(nullptr));
        std::string tmp_path = aof_path_ + ".rewrite.tmp." + std::to_string(AOF_GETPID()) + "_" + std::to_string(now_epoch);

        FILE* tmp_fp = fopen(tmp_path.c_str(), "wb");
        if (!tmp_fp) {
            std::cerr << "[kvllay] Failed to open temporary rewrite AOF file " << tmp_path << std::endl;
            return;
        }

        uint64_t now_wall = Store::wall_time_ms();

        for (const auto& entry : entries) {
            std::string serialized;
            if (entry.type == Store::EntryType::String) {
                if (entry.expire_at_epoch_ms == 0) {
                    serialized = Resp::array({"SET", entry.key, entry.string_val});
                } else if (entry.expire_at_epoch_ms > now_wall) {
                    uint64_t rem_ms = entry.expire_at_epoch_ms - now_wall;
                    uint64_t rem_sec = (rem_ms + 999) / 1000;
                    if (rem_sec == 0) rem_sec = 1;
                    serialized = Resp::array({"SETEX", entry.key, std::to_string(rem_sec), entry.string_val});
                } else {
                    continue;
                }
            } else if (entry.type == Store::EntryType::List) {
                if (entry.list_val.empty()) continue;
                if (entry.expire_at_epoch_ms != 0 && entry.expire_at_epoch_ms <= now_wall) {
                    continue;
                }
                std::vector<std::string> rpush_args;
                rpush_args.reserve(2 + entry.list_val.size());
                rpush_args.push_back("RPUSH");
                rpush_args.push_back(entry.key);
                for (const auto& elem : entry.list_val) {
                    rpush_args.push_back(elem);
                }
                serialized = Resp::array(rpush_args);
                if (entry.expire_at_epoch_ms > now_wall) {
                    uint64_t rem_ms = entry.expire_at_epoch_ms - now_wall;
                    serialized += Resp::array({"PEXPIRE", entry.key, std::to_string(rem_ms)});
                }
            } else {
                continue;
            }

            if (fwrite(serialized.data(), 1, serialized.size(), tmp_fp) != serialized.size()) {
                fclose(tmp_fp);
                remove(tmp_path.c_str());
                return;
            }
        }

        fflush(tmp_fp);

        {
            std::lock_guard<std::mutex> lock(buffer_mutex_);

            if (!flushing_buffer_.empty()) {
                fwrite(flushing_buffer_.data(), 1, flushing_buffer_.size(), tmp_fp);
                flushing_buffer_.clear();
            }
            if (!active_buffer_.empty()) {
                fwrite(active_buffer_.data(), 1, active_buffer_.size(), tmp_fp);
                active_buffer_.clear();
            }

            fflush(tmp_fp);
            fclose(tmp_fp);
            tmp_fp = nullptr;

            if (file_handle_) {
                fclose(file_handle_);
                file_handle_ = nullptr;
            }

#ifdef _WIN32
            MoveFileExA(tmp_path.c_str(), aof_path_.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
#else
            ::rename(tmp_path.c_str(), aof_path_.c_str());
#endif

            file_handle_ = fopen(aof_path_.c_str(), "ab");
        }

    }

    void replay_command(Store& store, const std::vector<std::string>& args) {
        if (args.empty()) return;
        std::string cmd = args[0];
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

        if (cmd == "SET" && args.size() >= 3) {
            uint64_t ttl_ms = 0;
            bool has_expiry = false;
            bool keep_ttl = false;
            bool nx = false;
            bool xx = false;
            bool valid = true;

            for (size_t i = 3; i < args.size() && valid; ++i) {
                std::string option = args[i];
                std::transform(option.begin(), option.end(), option.begin(),
                               [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
                if (option == "NX") {
                    if (nx || xx) valid = false;
                    nx = true;
                } else if (option == "XX") {
                    if (nx || xx) valid = false;
                    xx = true;
                } else if (option == "KEEPTTL") {
                    if (keep_ttl) valid = false;
                    keep_ttl = true;
                } else if (option == "EX" || option == "PX") {
                    if (has_expiry || i + 1 >= args.size()) {
                        valid = false;
                        break;
                    }
                    try {
                        long long duration = std::stoll(args[++i]);
                        if (duration <= 0) {
                            valid = false;
                            break;
                        }
                        ttl_ms = static_cast<uint64_t>(duration);
                        if (option == "EX") {
                            if (ttl_ms > std::numeric_limits<uint64_t>::max() / 1000) {
                                valid = false;
                                break;
                            }
                            ttl_ms *= 1000;
                        }
                        has_expiry = true;
                    } catch (...) {
                        valid = false;
                    }
                } else {
                    valid = false;
                }
            }

            if (valid && !(keep_ttl && has_expiry)) {
                store.set_with_options(args[1], args[2], ttl_ms, keep_ttl, nx, xx);
            }
        } else if (cmd == "SETEX" && args.size() >= 4) {
            try {
                long long sec = std::stoll(args[2]);
                if (sec > 0) {
                    store.setex(args[1], static_cast<uint64_t>(sec) * 1000, args[3]);
                }
            } catch (...) {}
        } else if (cmd == "DEL" && args.size() >= 2) {
            std::vector<std::string> keys(args.begin() + 1, args.end());
            store.del(keys);
        } else if (cmd == "EXPIRE" && args.size() >= 3) {
            try {
                long long sec = std::stoll(args[2]);
                store.expire(args[1], sec > 0 ? static_cast<uint64_t>(sec) * 1000 : 0);
            } catch (...) {}
        } else if (cmd == "PEXPIRE" && args.size() >= 3) {
            try {
                long long ms = std::stoll(args[2]);
                store.expire(args[1], ms > 0 ? static_cast<uint64_t>(ms) : 0);
            } catch (...) {}
        } else if (cmd == "PERSIST" && args.size() >= 2) {
            store.persist(args[1]);
        } else if ((cmd == "FLUSHDB" || cmd == "FLUSHALL")) {
            store.flushdb();
        } else if (cmd == "INCR" && args.size() >= 2) {
            int64_t res = 0;
            store.incrby(args[1], 1, res);
        } else if (cmd == "DECR" && args.size() >= 2) {
            int64_t res = 0;
            store.decrby(args[1], 1, res);
        } else if (cmd == "INCRBY" && args.size() >= 3) {
            int64_t delta = 0;
            if (Store::parse_int64(args[2], delta)) {
                int64_t res = 0;
                store.incrby(args[1], delta, res);
            }
        } else if (cmd == "DECRBY" && args.size() >= 3) {
            int64_t delta = 0;
            if (Store::parse_int64(args[2], delta)) {
                int64_t res = 0;
                store.decrby(args[1], delta, res);
            }
        } else if (cmd == "MSET" && args.size() >= 3 && (args.size() - 1) % 2 == 0) {
            std::vector<std::pair<std::string, std::string>> kvs;
            for (size_t i = 1; i < args.size(); i += 2) {
                kvs.emplace_back(args[i], args[i + 1]);
            }
            store.mset(kvs);
        } else if (cmd == "LPUSH" && args.size() >= 3) {
            std::vector<std::string> values(args.begin() + 2, args.end());
            size_t new_len = 0;
            store.lpush(args[1], values, new_len);
        } else if (cmd == "RPUSH" && args.size() >= 3) {
            std::vector<std::string> values(args.begin() + 2, args.end());
            size_t new_len = 0;
            store.rpush(args[1], values, new_len);
        } else if (cmd == "LPOP" && args.size() >= 2) {
            size_t count = 1;
            if (args.size() >= 3) {
                try {
                    long long c = std::stoll(args[2]);
                    if (c > 0) count = static_cast<size_t>(c);
                } catch (...) {}
            }
            std::vector<std::string> popped;
            store.lpop(args[1], count, popped);
        } else if (cmd == "RPOP" && args.size() >= 2) {
            size_t count = 1;
            if (args.size() >= 3) {
                try {
                    long long c = std::stoll(args[2]);
                    if (c > 0) count = static_cast<size_t>(c);
                } catch (...) {}
            }
            std::vector<std::string> popped;
            store.rpop(args[1], count, popped);
        }
    }

    std::string aof_path_;
    bool enabled_;
    FsyncPolicy fsync_policy_;
    FILE* file_handle_;

    std::mutex lifecycle_mutex_;

    std::string active_buffer_;
    std::string flushing_buffer_;
    std::mutex buffer_mutex_;
    std::condition_variable cv_;

    // Protects the lifetime of the rewrite thread.  The operation still
    // serializes FILE*/buffer access through buffer_mutex_, while stop() joins
    // this thread before destroying the manager underneath it.
    std::mutex rewrite_mutex_;

    std::atomic<bool> running_;
    std::atomic<bool> rewrite_in_progress_;
    std::thread writer_thread_;
    std::thread rewrite_thread_;
    std::chrono::steady_clock::time_point last_fsync_time_;
};

}

#endif // KVLLAY_AOF_HPP
