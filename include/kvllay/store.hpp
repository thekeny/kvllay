#ifndef KVLLAY_STORE_HPP
#define KVLLAY_STORE_HPP

#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <vector>
#include <deque>
#include <variant>
#include <array>
#include <algorithm>
#include <optional>
#include <shared_mutex>
#include <mutex>
#include <chrono>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <charconv>
#include <limits>
#include <functional>
#include <list>
#include <constants.hpp>
#include <allocator.hpp>
#include <resp.hpp>

namespace kvllay {

class Store {
public:
    static constexpr size_t NUM_SHARDS = 32;

    enum class EntryType : uint8_t {
        String = 0,
        List = 1,
        Hash = 2,
        Set = 3,
        ZSet = 4
    };

    using Hash = std::unordered_map<std::string, std::string>;
    using Set = std::unordered_set<std::string>;
    using ZSet = std::map<std::string, double>;

    struct Entry {
        std::variant<std::string, std::deque<std::string>, Hash, Set, ZSet> data;
        uint64_t expire_at = 0;
        mutable std::atomic<uint64_t> last_access{0};

        Entry() = default;
        Entry(std::string str, uint64_t exp = 0, uint64_t access = 0)
            : data(std::move(str)), expire_at(exp), last_access(access) {}
        Entry(const char* str, uint64_t exp = 0, uint64_t access = 0)
            : data(std::string(str)), expire_at(exp), last_access(access) {}
        Entry(std::deque<std::string> lst, uint64_t exp = 0, uint64_t access = 0)
            : data(std::move(lst)), expire_at(exp), last_access(access) {}
        Entry(Hash hash, uint64_t exp = 0, uint64_t access = 0)
            : data(std::move(hash)), expire_at(exp), last_access(access) {}
        Entry(Set set, uint64_t exp = 0, uint64_t access = 0)
            : data(std::move(set)), expire_at(exp), last_access(access) {}
        Entry(ZSet zset, uint64_t exp = 0, uint64_t access = 0)
            : data(std::move(zset)), expire_at(exp), last_access(access) {}

        Entry(const Entry& other)
            : data(other.data), expire_at(other.expire_at),
              last_access(other.last_access.load(std::memory_order_relaxed)) {}

        Entry& operator=(const Entry& other) {
            if (this != &other) {
                data = other.data;
                expire_at = other.expire_at;
                last_access.store(other.last_access.load(std::memory_order_relaxed), std::memory_order_relaxed);
            }
            return *this;
        }

        Entry(Entry&& other) noexcept
            : data(std::move(other.data)), expire_at(other.expire_at),
              last_access(other.last_access.load(std::memory_order_relaxed)) {}

        Entry& operator=(Entry&& other) noexcept {
            if (this != &other) {
                data = std::move(other.data);
                expire_at = other.expire_at;
                last_access.store(other.last_access.load(std::memory_order_relaxed), std::memory_order_relaxed);
            }
            return *this;
        }

        void touch(uint64_t now_clock) const noexcept {
            last_access.store(now_clock, std::memory_order_relaxed);
        }

        bool is_string() const noexcept {
            return std::holds_alternative<std::string>(data);
        }
        bool is_list() const noexcept {
            return std::holds_alternative<std::deque<std::string>>(data);
        }
        bool is_hash() const noexcept { return std::holds_alternative<Hash>(data); }
        bool is_set() const noexcept { return std::holds_alternative<Set>(data); }
        bool is_zset() const noexcept { return std::holds_alternative<ZSet>(data); }
        const std::string& as_string() const {
            return std::get<std::string>(data);
        }
        std::string& as_string() {
            return std::get<std::string>(data);
        }
        const std::deque<std::string>& as_list() const {
            return std::get<std::deque<std::string>>(data);
        }
        std::deque<std::string>& as_list() {
            return std::get<std::deque<std::string>>(data);
        }
        const Hash& as_hash() const { return std::get<Hash>(data); }
        Hash& as_hash() { return std::get<Hash>(data); }
        const Set& as_set() const { return std::get<Set>(data); }
        Set& as_set() { return std::get<Set>(data); }
        const ZSet& as_zset() const { return std::get<ZSet>(data); }
        ZSet& as_zset() { return std::get<ZSet>(data); }
    };

    struct DumpEntry {
        std::string key;
        EntryType type = EntryType::String;
        std::string string_val;
        std::vector<std::string> list_val;
        std::vector<std::pair<std::string, std::string>> hash_val;
        std::vector<std::string> set_val;
        std::vector<std::pair<std::string, double>> zset_val;
        uint64_t expire_at_epoch_ms = 0;
    };

    enum class KeyType {
        None,
        String,
        List,
        Hash,
        Set,
        ZSet
    };

    enum class GetStatus {
        Success,
        NotFound,
        WrongType
    };

    struct GetResult {
        GetStatus status;
        std::string value;
    };

    enum class IncrStatus {
        Success,
        NotAnInteger,
        Overflow,
        WrongType
    };

    enum class SetStatus {
        Applied,
        NotApplied
    };

    enum class ListPushStatus {
        Success,
        WrongType
    };

    enum class ListPopStatus {
        Success,
        NotFound,
        WrongType
    };

    enum class ListLenStatus {
        Success,
        WrongType
    };

    enum class ListRangeStatus {
        Success,
        NotFound,
        WrongType
    };

    enum class CollectionStatus {
        Success,
        NotFound,
        WrongType
    };

    enum class BlockingPopStatus {
        Success,
        Timeout,
        WrongType
    };

    Store(size_t max_memory = constants::DEFAULT_MAXMEMORY,
          constants::MaxmemoryPolicy policy = constants::MaxmemoryPolicy::NoEviction)
        : maxmemory_(max_memory), maxmemory_policy_(policy) {
        start_active_eviction();
    }

    ~Store() {
        stop_active_eviction();
    }

    Store(const Store&) = delete;
    Store& operator=(const Store&) = delete;

    size_t used_memory() const noexcept {
        return used_memory_.load(std::memory_order_relaxed);
    }

    size_t used_memory_peak() const noexcept {
        return used_memory_peak_.load(std::memory_order_relaxed);
    }

    void reset_peak_memory() noexcept {
        used_memory_peak_.store(used_memory_.load(std::memory_order_relaxed), std::memory_order_relaxed);
    }

    size_t maxmemory() const noexcept {
        return maxmemory_.load(std::memory_order_relaxed);
    }

    void set_maxmemory(size_t bytes) noexcept {
        maxmemory_.store(bytes, std::memory_order_relaxed);
    }

    constants::MaxmemoryPolicy maxmemory_policy() const noexcept {
        return maxmemory_policy_;
    }

    void set_maxmemory_policy(constants::MaxmemoryPolicy policy) noexcept {
        maxmemory_policy_ = policy;
    }

    size_t evicted_keys_count() const noexcept {
        return evicted_keys_count_.load(std::memory_order_relaxed);
    }

    static uint64_t current_lru_clock() noexcept {
        static std::atomic<uint64_t> clock_counter{0};
        uint64_t now_us = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
        uint64_t tick = clock_counter.fetch_add(1, std::memory_order_relaxed);
        return (now_us << 16) | (tick & 0xFFFF);
    }

    static size_t estimate_entry_memory(std::string_view key, const Entry& entry) noexcept {
        size_t mem = 112 + key.size();
        if (entry.is_string()) {
            mem += entry.as_string().size();
        } else if (entry.is_list()) {
            const auto& lst = entry.as_list();
            mem += lst.size() * 48;
            for (const auto& elem : lst) {
                mem += elem.size();
            }
        } else if (entry.is_hash()) {
            mem += entry.as_hash().size() * 64;
            for (const auto& [field, value] : entry.as_hash()) mem += field.size() + value.size();
        } else if (entry.is_set()) {
            mem += entry.as_set().size() * 48;
            for (const auto& value : entry.as_set()) mem += value.size();
        } else if (entry.is_zset()) {
            mem += entry.as_zset().size() * 64;
            for (const auto& [member, score] : entry.as_zset()) {
                (void)score;
                mem += member.size();
            }
        }
        return mem;
    }

    static size_t estimate_string_memory(std::string_view key, std::string_view val) noexcept {
        return 112 + key.size() + val.size();
    }

    void add_memory(size_t bytes) noexcept {
        size_t new_val = used_memory_.fetch_add(bytes, std::memory_order_relaxed) + bytes;
        size_t cur_peak = used_memory_peak_.load(std::memory_order_relaxed);
        while (new_val > cur_peak && !used_memory_peak_.compare_exchange_weak(cur_peak, new_val, std::memory_order_relaxed)) {
        }
    }

    void sub_memory(size_t bytes) noexcept {
        size_t current = used_memory_.load(std::memory_order_relaxed);
        while (current > 0) {
            size_t next = (bytes >= current) ? 0 : (current - bytes);
            if (used_memory_.compare_exchange_weak(current, next, std::memory_order_relaxed)) {
                break;
            }
        }
    }

    bool check_memory_and_evict(size_t needed_bytes = 0) {
        size_t max_mem = maxmemory_.load(std::memory_order_relaxed);
        if (max_mem == 0) {
            return true;
        }

        size_t current_used = used_memory_.load(std::memory_order_relaxed);
        if (current_used + needed_bytes <= max_mem) {
            return true;
        }

        if (maxmemory_policy_ == constants::MaxmemoryPolicy::NoEviction) {
            return false;
        }

        uint64_t now_ms = current_time_ms();
        size_t attempts = 0;
        constexpr size_t MAX_ATTEMPTS = 10000;

        while (used_memory_.load(std::memory_order_relaxed) + needed_bytes > max_mem && attempts < MAX_ATTEMPTS) {
            attempts++;

            std::string best_key;
            size_t best_shard_idx = 0;
            uint64_t oldest_access = UINT64_MAX;
            uint64_t shortest_ttl = UINT64_MAX;
            bool found = false;
            bool expired_found = false;

            for (size_t s = 0; s < NUM_SHARDS; ++s) {
                auto& shard = shards_[s];
                std::shared_lock<std::shared_mutex> lock(shard.mutex);
                if (shard.data.empty()) continue;

                size_t sampled = 0;
                for (const auto& [k, entry] : shard.data) {
                    if (entry.expire_at != 0 && entry.expire_at <= now_ms) {
                        best_key = k;
                        best_shard_idx = s;
                        found = true;
                        expired_found = true;
                        break;
                    }

                    if (maxmemory_policy_ == constants::MaxmemoryPolicy::VolatileLru ||
                        maxmemory_policy_ == constants::MaxmemoryPolicy::VolatileTtl) {
                        if (entry.expire_at == 0) continue;
                    }

                    if (maxmemory_policy_ == constants::MaxmemoryPolicy::AllKeysLru ||
                        maxmemory_policy_ == constants::MaxmemoryPolicy::VolatileLru) {
                        uint64_t acc = entry.last_access.load(std::memory_order_relaxed);
                        if (acc < oldest_access) {
                            oldest_access = acc;
                            best_key = k;
                            best_shard_idx = s;
                            found = true;
                        }
                    } else if (maxmemory_policy_ == constants::MaxmemoryPolicy::VolatileTtl) {
                        if (entry.expire_at < shortest_ttl) {
                            shortest_ttl = entry.expire_at;
                            best_key = k;
                            best_shard_idx = s;
                            found = true;
                        }
                    } else if (maxmemory_policy_ == constants::MaxmemoryPolicy::AllKeysRandom) {
                        best_key = k;
                        best_shard_idx = s;
                        found = true;
                        break;
                    }

                    if (++sampled >= 5) break;
                }
                if (expired_found || (found && (maxmemory_policy_ == constants::MaxmemoryPolicy::AllKeysRandom))) {
                    break;
                }
            }

            if (!found) {
                break;
            }

            auto& shard = shards_[best_shard_idx];
            std::unique_lock<std::shared_mutex> lock(shard.mutex);
            auto it = shard.data.find(best_key);
            if (it != shard.data.end()) {
                sub_memory(estimate_entry_memory(best_key, it->second));
                shard.data.erase(it);
                shard.keys_with_ttl.erase(best_key);
                evicted_keys_count_++;
                dirty_++;
            }
        }

        return (used_memory_.load(std::memory_order_relaxed) + needed_bytes <= max_mem);
    }

    static uint64_t current_time_ms() {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch()
            ).count()
        );
    }

    static uint64_t wall_time_ms() {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );
    }

    static bool parse_int64(const std::string& str, int64_t& out) {
        if (str.empty()) return false;
        const char* start = str.data();
        size_t len = str.size();
        if (start[0] == '+') {
            start++;
            len--;
            if (len == 0 || start[0] == '-' || start[0] == '+') return false;
        }
        auto [ptr, ec] = std::from_chars(start, start + len, out);
        return ec == std::errc() && ptr == start + len;
    }

    static bool add_overflow(int64_t a, int64_t b, int64_t& result) {
        if ((b > 0 && a > std::numeric_limits<int64_t>::max() - b) ||
            (b < 0 && a < std::numeric_limits<int64_t>::min() - b)) {
            return true;
        }
        result = a + b;
        return false;
    }

    static bool sub_overflow(int64_t a, int64_t b, int64_t& result) {
        if ((b > 0 && a < std::numeric_limits<int64_t>::min() + b) ||
            (b < 0 && a > std::numeric_limits<int64_t>::max() + b)) {
            return true;
        }
        result = a - b;
        return false;
    }

    IncrStatus incrby(std::string_view key, int64_t delta, int64_t& result_val) {
        return modify_int(key, delta, result_val, false);
    }

    IncrStatus decrby(std::string_view key, int64_t delta, int64_t& result_val) {
        return modify_int(key, delta, result_val, true);
    }

    SetStatus set_with_options(std::string_view key, std::string_view value,
                               uint64_t ttl_ms = 0, bool keep_ttl = false,
                               bool nx = false, bool xx = false) {
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        uint64_t now_ms = current_time_ms();
        uint64_t now_sec = current_lru_clock();
        std::string k(key);
        {
            std::unique_lock<std::shared_mutex> lock(shard.mutex);
            auto it = shard.data.find(k);

            // Expired entries must behave as absent for NX/XX and should not
            // retain their memory until the active eviction pass runs.
            if (it != shard.data.end() && it->second.expire_at != 0 && it->second.expire_at <= now_ms) {
                sub_memory(estimate_entry_memory(k, it->second));
                shard.data.erase(it);
                shard.keys_with_ttl.erase(k);
                it = shard.data.end();
            }

            if ((nx && it != shard.data.end()) || (xx && it == shard.data.end())) {
                return SetStatus::NotApplied;
            }

            uint64_t expire_at = 0;
            if (ttl_ms != 0) {
                expire_at = now_ms + ttl_ms;
            } else if (keep_ttl && it != shard.data.end()) {
                expire_at = it->second.expire_at;
            }
            bool had_ttl = (it != shard.data.end() && it->second.expire_at != 0);

            if (it != shard.data.end()) {
                size_t old_mem = estimate_entry_memory(it->first, it->second);
                if (it->second.is_string()) {
                    it->second.as_string().assign(value.data(), value.size());
                } else {
                    it->second.data.emplace<std::string>(value);
                }
                it->second.expire_at = expire_at;
                it->second.touch(now_sec);
                size_t new_mem = estimate_entry_memory(it->first, it->second);
                if (new_mem > old_mem) {
                    add_memory(new_mem - old_mem);
                } else if (new_mem < old_mem) {
                    sub_memory(old_mem - new_mem);
                }
            } else {
                shard.data.emplace(std::piecewise_construct,
                                   std::forward_as_tuple(std::move(k)),
                                   std::forward_as_tuple(std::string(value), expire_at, now_sec));
                add_memory(estimate_string_memory(key, value));
            }
            if (expire_at != 0) {
                shard.keys_with_ttl.insert(std::string(key));
            } else if (had_ttl && !shard.keys_with_ttl.empty()) {
                shard.keys_with_ttl.erase(std::string(key));
            }
        }
        dirty_++;
        return SetStatus::Applied;
    }

    bool set(std::string_view key, std::string_view value) {
        return set_with_options(key, value) == SetStatus::Applied;
    }

    bool mset(const std::vector<std::pair<std::string, std::string>>& kvs) {
        if (kvs.empty()) return true;

        std::vector<size_t> involved_shards;
        involved_shards.reserve(kvs.size());
        for (const auto& [k, v] : kvs) {
            involved_shards.push_back(shard_index(k));
        }
        std::sort(involved_shards.begin(), involved_shards.end());
        involved_shards.erase(std::unique(involved_shards.begin(), involved_shards.end()), involved_shards.end());

        std::vector<std::unique_lock<std::shared_mutex>> locks;
        locks.reserve(involved_shards.size());
        for (size_t s_idx : involved_shards) {
            locks.emplace_back(shards_[s_idx].mutex);
        }

        uint64_t now_sec = current_lru_clock();
        for (const auto& [key, value] : kvs) {
            size_t idx = shard_index(key);
            auto& shard = shards_[idx];
            auto it = shard.data.find(key);
            if (it != shard.data.end()) {
                size_t old_mem = estimate_entry_memory(key, it->second);
                if (it->second.is_string()) {
                    it->second.as_string().assign(value.data(), value.size());
                } else {
                    it->second.data.emplace<std::string>(value);
                }
                it->second.expire_at = 0;
                it->second.touch(now_sec);
                size_t new_mem = estimate_entry_memory(key, it->second);
                if (new_mem > old_mem) {
                    add_memory(new_mem - old_mem);
                } else if (new_mem < old_mem) {
                    sub_memory(old_mem - new_mem);
                }
            } else {
                shard.data.emplace(std::piecewise_construct,
                                   std::forward_as_tuple(key),
                                   std::forward_as_tuple(value, 0, now_sec));
                add_memory(estimate_string_memory(key, value));
            }
            shard.keys_with_ttl.erase(key);
        }
        dirty_ += kvs.size();
        return true;
    }

    bool setex(const std::string& key, uint64_t ttl_ms, const std::string& value) {
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        uint64_t expire_at = current_time_ms() + ttl_ms;
        uint64_t now_sec = current_lru_clock();
        {
            std::unique_lock<std::shared_mutex> lock(shard.mutex);
            auto it = shard.data.find(key);
            if (it != shard.data.end()) {
                size_t old_mem = estimate_entry_memory(key, it->second);
                if (it->second.is_string()) {
                    it->second.as_string().assign(value.data(), value.size());
                } else {
                    it->second.data.emplace<std::string>(value);
                }
                it->second.expire_at = expire_at;
                it->second.touch(now_sec);
                size_t new_mem = estimate_entry_memory(key, it->second);
                if (new_mem > old_mem) {
                    add_memory(new_mem - old_mem);
                } else if (new_mem < old_mem) {
                    sub_memory(old_mem - new_mem);
                }
            } else {
                shard.data.emplace(std::piecewise_construct,
                                   std::forward_as_tuple(key),
                                   std::forward_as_tuple(value, expire_at, now_sec));
                add_memory(estimate_string_memory(key, value));
            }
            shard.keys_with_ttl.insert(key);
        }
        dirty_++;
        return true;
    }

    bool get_and_append(std::string_view key, std::string& out, int protocol = 2) {
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        uint64_t now = current_time_ms();
        uint64_t now_sec = current_lru_clock();
        std::string k(key);
        {
            std::shared_lock<std::shared_mutex> lock(shard.mutex);
            auto it = shard.data.find(k);
            if (it == shard.data.end()) {
                Resp::append_null_bulk_string(out, protocol);
                return true;
            }
            if (it->second.expire_at == 0 || it->second.expire_at > now) {
                if (!it->second.is_string()) {
                    Resp::append_error(out, "WRONGTYPE Operation against a key holding the wrong kind of value");
                    return false;
                }
                it->second.touch(now_sec);
                Resp::append_bulk_string(out, it->second.as_string());
                return true;
            }
        }

        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(k);
        if (it != shard.data.end()) {
            if (it->second.expire_at != 0 && it->second.expire_at <= current_time_ms()) {
                sub_memory(estimate_entry_memory(k, it->second));
                shard.data.erase(it);
                if (!shard.keys_with_ttl.empty()) {
                    shard.keys_with_ttl.erase(k);
                }
                Resp::append_null_bulk_string(out, protocol);
                return true;
            }
            if (!it->second.is_string()) {
                Resp::append_error(out, "WRONGTYPE Operation against a key holding the wrong kind of value");
                return false;
            }
            it->second.touch(now_sec);
            Resp::append_bulk_string(out, it->second.as_string());
            return true;
        }
        Resp::append_null_bulk_string(out, protocol);
        return true;
    }

    GetResult get(const std::string& key) {
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        uint64_t now = current_time_ms();
        uint64_t now_sec = current_lru_clock();
        {
            std::shared_lock<std::shared_mutex> lock(shard.mutex);
            auto it = shard.data.find(key);
            if (it == shard.data.end()) {
                return {GetStatus::NotFound, ""};
            }
            if (it->second.expire_at == 0 || it->second.expire_at > now) {
                if (!it->second.is_string()) {
                    return {GetStatus::WrongType, ""};
                }
                it->second.touch(now_sec);
                return {GetStatus::Success, it->second.as_string()};
            }
        }

        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it != shard.data.end()) {
            if (it->second.expire_at != 0 && it->second.expire_at <= current_time_ms()) {
                sub_memory(estimate_entry_memory(key, it->second));
                shard.data.erase(it);
                shard.keys_with_ttl.erase(key);
                return {GetStatus::NotFound, ""};
            }
            if (!it->second.is_string()) {
                return {GetStatus::WrongType, ""};
            }
            it->second.touch(now_sec);
            return {GetStatus::Success, it->second.as_string()};
        }
        return {GetStatus::NotFound, ""};
    }

    void mget_and_append(const std::vector<std::string_view>& keys, std::string& out, int protocol = 2) {
        uint64_t now = current_time_ms();
        uint64_t now_sec = current_lru_clock();
        std::vector<std::string> expired_keys;

        std::vector<size_t> involved_shards;
        involved_shards.reserve(keys.size());
        for (const auto& k : keys) {
            involved_shards.push_back(shard_index(k));
        }
        std::sort(involved_shards.begin(), involved_shards.end());
        involved_shards.erase(std::unique(involved_shards.begin(), involved_shards.end()), involved_shards.end());

        std::vector<std::shared_lock<std::shared_mutex>> locks;
        locks.reserve(involved_shards.size());
        for (size_t s_idx : involved_shards) {
            locks.emplace_back(shards_[s_idx].mutex);
        }

        Resp::append_array_header(out, keys.size());
        for (const auto& key : keys) {
            size_t idx = shard_index(key);
            auto& shard = shards_[idx];
            std::string k(key);
            auto it = shard.data.find(k);
            if (it == shard.data.end()) {
                Resp::append_null_bulk_string(out, protocol);
            } else if (it->second.expire_at != 0 && it->second.expire_at <= now) {
                Resp::append_null_bulk_string(out, protocol);
                expired_keys.push_back(std::move(k));
            } else if (!it->second.is_string()) {
                Resp::append_null_bulk_string(out, protocol);
            } else {
                it->second.touch(now_sec);
                Resp::append_bulk_string(out, it->second.as_string());
            }
        }
        locks.clear();

        if (!expired_keys.empty()) {
            uint64_t cur_now = current_time_ms();
            for (const auto& key : expired_keys) {
                size_t idx = shard_index(key);
                auto& shard = shards_[idx];
                std::unique_lock<std::shared_mutex> lock(shard.mutex);
                auto it = shard.data.find(key);
                if (it != shard.data.end() && it->second.expire_at != 0 && it->second.expire_at <= cur_now) {
                    sub_memory(estimate_entry_memory(key, it->second));
                    shard.data.erase(it);
                    if (!shard.keys_with_ttl.empty()) {
                        shard.keys_with_ttl.erase(key);
                    }
                }
            }
        }
    }

    std::vector<std::optional<std::string>> mget(const std::vector<std::string>& keys) {
        uint64_t now = current_time_ms();
        uint64_t now_sec = current_lru_clock();
        std::vector<std::optional<std::string>> result;
        result.reserve(keys.size());
        std::vector<std::string> expired_keys;

        std::vector<size_t> involved_shards;
        involved_shards.reserve(keys.size());
        for (const auto& k : keys) {
            involved_shards.push_back(shard_index(k));
        }
        std::sort(involved_shards.begin(), involved_shards.end());
        involved_shards.erase(std::unique(involved_shards.begin(), involved_shards.end()), involved_shards.end());

        std::vector<std::shared_lock<std::shared_mutex>> locks;
        locks.reserve(involved_shards.size());
        for (size_t s_idx : involved_shards) {
            locks.emplace_back(shards_[s_idx].mutex);
        }

        for (const auto& key : keys) {
            size_t idx = shard_index(key);
            auto& shard = shards_[idx];
            auto it = shard.data.find(key);
            if (it == shard.data.end()) {
                result.push_back(std::nullopt);
            } else if (it->second.expire_at != 0 && it->second.expire_at <= now) {
                result.push_back(std::nullopt);
                expired_keys.push_back(key);
            } else if (!it->second.is_string()) {
                result.push_back(std::nullopt);
            } else {
                it->second.touch(now_sec);
                result.push_back(it->second.as_string());
            }
        }
        locks.clear();

        if (!expired_keys.empty()) {
            uint64_t cur_now = current_time_ms();
            for (const auto& key : expired_keys) {
                size_t idx = shard_index(key);
                auto& shard = shards_[idx];
                std::unique_lock<std::shared_mutex> lock(shard.mutex);
                auto it = shard.data.find(key);
                if (it != shard.data.end() && it->second.expire_at != 0 && it->second.expire_at <= cur_now) {
                    sub_memory(estimate_entry_memory(key, it->second));
                    shard.data.erase(it);
                    shard.keys_with_ttl.erase(key);
                }
            }
        }

        return result;
    }

    KeyType key_type(const std::string& key) {
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        uint64_t now = current_time_ms();
        {
            std::shared_lock<std::shared_mutex> lock(shard.mutex);
            auto it = shard.data.find(key);
            if (it == shard.data.end()) {
                return KeyType::None;
            }
            if (it->second.expire_at != 0 && it->second.expire_at <= now) {
            } else {
                it->second.touch(current_lru_clock());
                if (it->second.is_string()) return KeyType::String;
                if (it->second.is_list()) return KeyType::List;
                if (it->second.is_hash()) return KeyType::Hash;
                if (it->second.is_set()) return KeyType::Set;
                return KeyType::ZSet;
            }
        }

        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it != shard.data.end()) {
            if (it->second.expire_at != 0 && it->second.expire_at <= current_time_ms()) {
                sub_memory(estimate_entry_memory(key, it->second));
                shard.data.erase(it);
                shard.keys_with_ttl.erase(key);
                return KeyType::None;
            }
            it->second.touch(current_lru_clock());
            if (it->second.is_string()) return KeyType::String;
            if (it->second.is_list()) return KeyType::List;
            if (it->second.is_hash()) return KeyType::Hash;
            if (it->second.is_set()) return KeyType::Set;
            return KeyType::ZSet;
        }
        return KeyType::None;
    }

    CollectionStatus hset(const std::string& key,
                          const std::vector<std::pair<std::string, std::string>>& fields,
                          size_t& added) {
        added = 0;
        if (fields.empty()) return CollectionStatus::Success;
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it != shard.data.end() && it->second.expire_at != 0 && it->second.expire_at <= current_time_ms()) {
            sub_memory(estimate_entry_memory(key, it->second));
            shard.data.erase(it);
            shard.keys_with_ttl.erase(key);
            it = shard.data.end();
        }
        if (it == shard.data.end()) {
            it = shard.data.emplace(key, Entry{Hash{}, 0, current_lru_clock()}).first;
        } else if (!it->second.is_hash()) {
            return CollectionStatus::WrongType;
        }
        size_t old_mem = estimate_entry_memory(key, it->second);
        auto& hash = it->second.as_hash();
        for (const auto& [field, value] : fields) {
            if (hash.find(field) == hash.end()) ++added;
            hash[field] = value;
        }
        it->second.touch(current_lru_clock());
        size_t new_mem = estimate_entry_memory(key, it->second);
        if (new_mem > old_mem) add_memory(new_mem - old_mem);
        else if (old_mem > new_mem) sub_memory(old_mem - new_mem);
        dirty_ += fields.size();
        return CollectionStatus::Success;
    }

    CollectionStatus hsetnx(const std::string& key, const std::string& field,
                            const std::string& value, int& inserted) {
        inserted = 0;
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it != shard.data.end() && it->second.expire_at != 0 && it->second.expire_at <= current_time_ms()) {
            sub_memory(estimate_entry_memory(key, it->second));
            shard.data.erase(it);
            shard.keys_with_ttl.erase(key);
            it = shard.data.end();
        }
        if (it == shard.data.end()) {
            it = shard.data.emplace(key, Entry{Hash{}, 0, current_lru_clock()}).first;
        } else if (!it->second.is_hash()) {
            return CollectionStatus::WrongType;
        }
        auto& hash = it->second.as_hash();
        if (hash.find(field) != hash.end()) return CollectionStatus::Success;
        size_t old_mem = estimate_entry_memory(key, it->second);
        hash[field] = value;
        size_t new_mem = estimate_entry_memory(key, it->second);
        if (new_mem > old_mem) add_memory(new_mem - old_mem);
        it->second.touch(current_lru_clock());
        inserted = 1;
        dirty_++;
        return CollectionStatus::Success;
    }

    CollectionStatus hget(const std::string& key, const std::string& field,
                          std::string& value) {
        value.clear();
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::shared_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it == shard.data.end() || (it->second.expire_at != 0 && it->second.expire_at <= current_time_ms())) {
            return CollectionStatus::NotFound;
        }
        if (!it->second.is_hash()) return CollectionStatus::WrongType;
        auto field_it = it->second.as_hash().find(field);
        if (field_it == it->second.as_hash().end()) return CollectionStatus::NotFound;
        value = field_it->second;
        return CollectionStatus::Success;
    }

    CollectionStatus hgetall(const std::string& key, std::vector<std::pair<std::string, std::string>>& values) {
        values.clear();
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::shared_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it == shard.data.end() || (it->second.expire_at != 0 && it->second.expire_at <= current_time_ms())) {
            return CollectionStatus::NotFound;
        }
        if (!it->second.is_hash()) return CollectionStatus::WrongType;
        values.reserve(it->second.as_hash().size());
        for (const auto& pair : it->second.as_hash()) values.push_back(pair);
        return CollectionStatus::Success;
    }

    CollectionStatus hdel(const std::string& key, const std::vector<std::string>& fields, size_t& removed) {
        removed = 0;
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it == shard.data.end()) return CollectionStatus::NotFound;
        if (it->second.expire_at != 0 && it->second.expire_at <= current_time_ms()) {
            sub_memory(estimate_entry_memory(key, it->second));
            shard.data.erase(it);
            shard.keys_with_ttl.erase(key);
            return CollectionStatus::NotFound;
        }
        if (!it->second.is_hash()) return CollectionStatus::WrongType;
        size_t old_mem = estimate_entry_memory(key, it->second);
        auto& hash = it->second.as_hash();
        for (const auto& field : fields) removed += hash.erase(field);
        if (hash.empty()) {
            sub_memory(old_mem);
            shard.data.erase(it);
            shard.keys_with_ttl.erase(key);
        } else {
            size_t new_mem = estimate_entry_memory(key, it->second);
            if (old_mem > new_mem) sub_memory(old_mem - new_mem);
        }
        if (removed) dirty_ += removed;
        return CollectionStatus::Success;
    }

    CollectionStatus hlen(const std::string& key, size_t& length) {
        length = 0;
        std::string unused;
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::shared_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it == shard.data.end() || (it->second.expire_at != 0 && it->second.expire_at <= current_time_ms())) {
            return CollectionStatus::Success;
        }
        if (!it->second.is_hash()) return CollectionStatus::WrongType;
        length = it->second.as_hash().size();
        return CollectionStatus::Success;
    }

    CollectionStatus sadd(const std::string& key, const std::vector<std::string>& values, size_t& added) {
        added = 0;
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it != shard.data.end() && it->second.expire_at != 0 && it->second.expire_at <= current_time_ms()) {
            sub_memory(estimate_entry_memory(key, it->second));
            shard.data.erase(it);
            shard.keys_with_ttl.erase(key);
            it = shard.data.end();
        }
        if (it == shard.data.end()) it = shard.data.emplace(key, Entry{Set{}, 0, current_lru_clock()}).first;
        else if (!it->second.is_set()) return CollectionStatus::WrongType;
        size_t old_mem = estimate_entry_memory(key, it->second);
        auto& set = it->second.as_set();
        for (const auto& value : values) added += set.insert(value).second;
        size_t new_mem = estimate_entry_memory(key, it->second);
        if (new_mem > old_mem) add_memory(new_mem - old_mem);
        it->second.touch(current_lru_clock());
        if (added) dirty_ += added;
        return CollectionStatus::Success;
    }

    CollectionStatus srem(const std::string& key, const std::vector<std::string>& values, size_t& removed) {
        removed = 0;
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it == shard.data.end()) return CollectionStatus::NotFound;
        if (it->second.expire_at != 0 && it->second.expire_at <= current_time_ms()) return CollectionStatus::NotFound;
        if (!it->second.is_set()) return CollectionStatus::WrongType;
        size_t old_mem = estimate_entry_memory(key, it->second);
        auto& set = it->second.as_set();
        for (const auto& value : values) removed += set.erase(value);
        if (set.empty()) {
            sub_memory(old_mem);
            shard.data.erase(it);
            shard.keys_with_ttl.erase(key);
        } else {
            size_t new_mem = estimate_entry_memory(key, it->second);
            if (old_mem > new_mem) sub_memory(old_mem - new_mem);
        }
        if (removed) dirty_ += removed;
        return CollectionStatus::Success;
    }

    CollectionStatus smembers(const std::string& key, std::vector<std::string>& values) {
        values.clear();
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::shared_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it == shard.data.end() || (it->second.expire_at != 0 && it->second.expire_at <= current_time_ms())) return CollectionStatus::Success;
        if (!it->second.is_set()) return CollectionStatus::WrongType;
        values.assign(it->second.as_set().begin(), it->second.as_set().end());
        return CollectionStatus::Success;
    }

    CollectionStatus zadd(const std::string& key, const std::vector<std::pair<std::string, double>>& values, size_t& added) {
        added = 0;
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it != shard.data.end() && it->second.expire_at != 0 && it->second.expire_at <= current_time_ms()) {
            sub_memory(estimate_entry_memory(key, it->second));
            shard.data.erase(it);
            shard.keys_with_ttl.erase(key);
            it = shard.data.end();
        }
        bool is_new_entry = false;
        if (it == shard.data.end()) {
            it = shard.data.emplace(key, Entry{ZSet{}, 0, current_lru_clock()}).first;
            is_new_entry = true;
        } else if (!it->second.is_zset()) {
            return CollectionStatus::WrongType;
        }
        size_t added_bytes = is_new_entry ? (112 + key.size()) : 0;
        auto& zset = it->second.as_zset();
        for (const auto& [member, score] : values) {
            auto m_it = zset.find(member);
            if (m_it == zset.end()) {
                ++added;
                added_bytes += 64 + member.size();
                zset.emplace(member, score);
            } else {
                m_it->second = score;
            }
        }
        if (added_bytes > 0) add_memory(added_bytes);
        it->second.touch(current_lru_clock());
        if (added) dirty_ += added;
        return CollectionStatus::Success;
    }

    CollectionStatus zrem(const std::string& key, const std::vector<std::string>& values, size_t& removed) {
        removed = 0;
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it == shard.data.end()) return CollectionStatus::NotFound;
        if (it->second.expire_at != 0 && it->second.expire_at <= current_time_ms()) return CollectionStatus::NotFound;
        if (!it->second.is_zset()) return CollectionStatus::WrongType;
        auto& zset = it->second.as_zset();
        size_t removed_bytes = 0;
        for (const auto& value : values) {
            auto m_it = zset.find(value);
            if (m_it != zset.end()) {
                removed_bytes += 64 + m_it->first.size();
                zset.erase(m_it);
                ++removed;
            }
        }
        if (zset.empty()) {
            sub_memory(112 + key.size() + removed_bytes);
            shard.data.erase(it);
            shard.keys_with_ttl.erase(key);
        } else if (removed_bytes > 0) {
            sub_memory(removed_bytes);
        }
        if (removed) dirty_ += removed;
        return CollectionStatus::Success;
    }

    CollectionStatus zrangebyscore(const std::string& key, double min_score, double max_score,
                                   bool min_exclusive, bool max_exclusive,
                                   std::vector<std::string>& values) {
        values.clear();
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::shared_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it == shard.data.end() || (it->second.expire_at != 0 && it->second.expire_at <= current_time_ms())) return CollectionStatus::Success;
        if (!it->second.is_zset()) return CollectionStatus::WrongType;
        for (const auto& [member, score] : it->second.as_zset()) {
            bool lower_ok = min_exclusive ? score > min_score : score >= min_score;
            bool upper_ok = max_exclusive ? score < max_score : score <= max_score;
            if (lower_ok && upper_ok) values.push_back(member);
        }
        std::sort(values.begin(), values.end(), [&](const std::string& a, const std::string& b) {
            double sa = it->second.as_zset().at(a);
            double sb = it->second.as_zset().at(b);
            if (sa != sb) return sa < sb;
            return a < b;
        });
        return CollectionStatus::Success;
    }

    CollectionStatus zscore(const std::string& key, const std::string& member, double& score) {
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::shared_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it == shard.data.end() || (it->second.expire_at != 0 && it->second.expire_at <= current_time_ms())) {
            return CollectionStatus::NotFound;
        }
        if (!it->second.is_zset()) return CollectionStatus::WrongType;
        const auto& zset = it->second.as_zset();
        auto m_it = zset.find(member);
        if (m_it == zset.end()) return CollectionStatus::NotFound;
        score = m_it->second;
        return CollectionStatus::Success;
    }

    template <typename Iter>
    ListPushStatus lpush(std::string_view key, Iter begin, Iter end, size_t& new_len) {
        if (begin == end) return ListPushStatus::Success;
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        uint64_t now = current_time_ms();
        std::string k(key);
        auto it = shard.data.find(k);
        if (it != shard.data.end() && it->second.expire_at != 0 && it->second.expire_at <= now) {
            sub_memory(estimate_entry_memory(k, it->second));
            shard.data.erase(it);
            if (!shard.keys_with_ttl.empty()) {
                shard.keys_with_ttl.erase(k);
            }
            it = shard.data.end();
        }

        size_t count = std::distance(begin, end);
        size_t added_bytes = count * 48;
        for (auto curr = begin; curr != end; ++curr) {
            added_bytes += curr->size();
        }

        if (it != shard.data.end()) {
            if (!it->second.is_list()) {
                return ListPushStatus::WrongType;
            }
            auto& deque = it->second.as_list();
            for (auto curr = begin; curr != end; ++curr) {
                deque.emplace_front(*curr);
            }
            it->second.touch(current_lru_clock());
            new_len = deque.size();
            add_memory(added_bytes);
        } else {
            std::deque<std::string> deque;
            for (auto curr = begin; curr != end; ++curr) {
                deque.emplace_front(*curr);
            }
            new_len = deque.size();
            shard.data.emplace(std::piecewise_construct,
                               std::forward_as_tuple(std::move(k)),
                               std::forward_as_tuple(std::move(deque), 0, current_lru_clock()));
            add_memory(112 + key.size() + added_bytes);
        }

        dirty_ += count;
        list_generation_.fetch_add(1, std::memory_order_release);
        list_cv_.notify_all();
        lock.unlock();
        notify_blocked_waiter_for_key(std::string(key));
        return ListPushStatus::Success;
    }

    ListPushStatus lpush(const std::string& key, const std::vector<std::string>& values, size_t& new_len) {
        return lpush(std::string_view(key), values.begin(), values.end(), new_len);
    }

    template <typename Iter>
    ListPushStatus rpush(std::string_view key, Iter begin, Iter end, size_t& new_len) {
        if (begin == end) return ListPushStatus::Success;
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        uint64_t now = current_time_ms();
        std::string k(key);
        auto it = shard.data.find(k);
        if (it != shard.data.end() && it->second.expire_at != 0 && it->second.expire_at <= now) {
            sub_memory(estimate_entry_memory(k, it->second));
            shard.data.erase(it);
            if (!shard.keys_with_ttl.empty()) {
                shard.keys_with_ttl.erase(k);
            }
            it = shard.data.end();
        }

        size_t count = std::distance(begin, end);
        size_t added_bytes = count * 48;
        for (auto curr = begin; curr != end; ++curr) {
            added_bytes += curr->size();
        }

        if (it != shard.data.end()) {
            if (!it->second.is_list()) {
                return ListPushStatus::WrongType;
            }
            auto& deque = it->second.as_list();
            for (auto curr = begin; curr != end; ++curr) {
                deque.emplace_back(*curr);
            }
            it->second.touch(current_lru_clock());
            new_len = deque.size();
            add_memory(added_bytes);
        } else {
            std::deque<std::string> deque;
            for (auto curr = begin; curr != end; ++curr) {
                deque.emplace_back(*curr);
            }
            new_len = deque.size();
            shard.data.emplace(std::piecewise_construct,
                               std::forward_as_tuple(std::move(k)),
                               std::forward_as_tuple(std::move(deque), 0, current_lru_clock()));
            add_memory(112 + key.size() + added_bytes);
        }

        dirty_ += count;
        list_generation_.fetch_add(1, std::memory_order_release);
        list_cv_.notify_all();
        lock.unlock();
        notify_blocked_waiter_for_key(std::string(key));
        return ListPushStatus::Success;
    }

    ListPushStatus rpush(const std::string& key, const std::vector<std::string>& values, size_t& new_len) {
        return rpush(std::string_view(key), values.begin(), values.end(), new_len);
    }

    bool lpop_one(std::string_view key, std::string& out, int protocol = 2) {
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        uint64_t now = current_time_ms();
        std::string k(key);
        auto it = shard.data.find(k);
        if (it == shard.data.end()) {
            Resp::append_null_bulk_string(out, protocol);
            return true;
        }
        if (it->second.expire_at != 0 && it->second.expire_at <= now) {
            sub_memory(estimate_entry_memory(k, it->second));
            shard.data.erase(it);
            if (!shard.keys_with_ttl.empty()) {
                shard.keys_with_ttl.erase(k);
            }
            Resp::append_null_bulk_string(out, protocol);
            return true;
        }
        if (!it->second.is_list()) {
            Resp::append_error(out, "WRONGTYPE Operation against a key holding the wrong kind of value");
            return false;
        }
        auto& deque = it->second.as_list();
        if (deque.empty()) {
            sub_memory(112 + k.size());
            shard.data.erase(it);
            if (!shard.keys_with_ttl.empty()) {
                shard.keys_with_ttl.erase(k);
            }
            Resp::append_null_bulk_string(out, protocol);
            return true;
        }
        Resp::append_bulk_string(out, deque.front());
        sub_memory(48 + deque.front().size());
        deque.pop_front();
        if (deque.empty()) {
            sub_memory(112 + k.size());
            shard.data.erase(it);
            if (!shard.keys_with_ttl.empty()) {
                shard.keys_with_ttl.erase(k);
            }
        } else {
            it->second.touch(current_lru_clock());
        }
        dirty_++;
        return true;
    }

    bool rpop_one(std::string_view key, std::string& out, int protocol = 2) {
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        uint64_t now = current_time_ms();
        std::string k(key);
        auto it = shard.data.find(k);
        if (it == shard.data.end()) {
            Resp::append_null_bulk_string(out, protocol);
            return true;
        }
        if (it->second.expire_at != 0 && it->second.expire_at <= now) {
            sub_memory(estimate_entry_memory(k, it->second));
            shard.data.erase(it);
            if (!shard.keys_with_ttl.empty()) {
                shard.keys_with_ttl.erase(k);
            }
            Resp::append_null_bulk_string(out, protocol);
            return true;
        }
        if (!it->second.is_list()) {
            Resp::append_error(out, "WRONGTYPE Operation against a key holding the wrong kind of value");
            return false;
        }
        auto& deque = it->second.as_list();
        if (deque.empty()) {
            sub_memory(112 + k.size());
            shard.data.erase(it);
            if (!shard.keys_with_ttl.empty()) {
                shard.keys_with_ttl.erase(k);
            }
            Resp::append_null_bulk_string(out, protocol);
            return true;
        }
        Resp::append_bulk_string(out, deque.back());
        sub_memory(48 + deque.back().size());
        deque.pop_back();
        if (deque.empty()) {
            sub_memory(112 + k.size());
            shard.data.erase(it);
            if (!shard.keys_with_ttl.empty()) {
                shard.keys_with_ttl.erase(k);
            }
        } else {
            it->second.touch(current_lru_clock());
        }
        dirty_++;
        return true;
    }

    ListPopStatus lpop(const std::string& key, size_t count, std::vector<std::string>& out_popped) {
        out_popped.clear();
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        uint64_t now = current_time_ms();
        auto it = shard.data.find(key);
        if (it == shard.data.end()) {
            return ListPopStatus::NotFound;
        }
        if (it->second.expire_at != 0 && it->second.expire_at <= now) {
            sub_memory(estimate_entry_memory(key, it->second));
            shard.data.erase(it);
            shard.keys_with_ttl.erase(key);
            return ListPopStatus::NotFound;
        }
        if (!it->second.is_list()) {
            return ListPopStatus::WrongType;
        }

        auto& deque = it->second.as_list();
        if (deque.empty()) {
            sub_memory(112 + key.size());
            shard.data.erase(it);
            shard.keys_with_ttl.erase(key);
            return ListPopStatus::NotFound;
        }

        size_t to_pop = std::min(count, deque.size());
        out_popped.reserve(to_pop);
        size_t popped_bytes = to_pop * 48;
        for (size_t i = 0; i < to_pop; ++i) {
            popped_bytes += deque.front().size();
            out_popped.push_back(std::move(deque.front()));
            deque.pop_front();
        }
        sub_memory(popped_bytes);

        if (deque.empty()) {
            sub_memory(112 + key.size());
            shard.data.erase(it);
            shard.keys_with_ttl.erase(key);
        } else {
            it->second.touch(current_lru_clock());
        }

        dirty_ += out_popped.size();
        return ListPopStatus::Success;
    }

    ListPopStatus rpop(const std::string& key, size_t count, std::vector<std::string>& out_popped) {
        out_popped.clear();
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        uint64_t now = current_time_ms();
        auto it = shard.data.find(key);
        if (it == shard.data.end()) {
            return ListPopStatus::NotFound;
        }
        if (it->second.expire_at != 0 && it->second.expire_at <= now) {
            sub_memory(estimate_entry_memory(key, it->second));
            shard.data.erase(it);
            shard.keys_with_ttl.erase(key);
            return ListPopStatus::NotFound;
        }
        if (!it->second.is_list()) {
            return ListPopStatus::WrongType;
        }

        auto& deque = it->second.as_list();
        if (deque.empty()) {
            sub_memory(112 + key.size());
            shard.data.erase(it);
            shard.keys_with_ttl.erase(key);
            return ListPopStatus::NotFound;
        }

        size_t to_pop = std::min(count, deque.size());
        out_popped.reserve(to_pop);
        size_t popped_bytes = to_pop * 48;
        for (size_t i = 0; i < to_pop; ++i) {
            popped_bytes += deque.back().size();
            out_popped.push_back(std::move(deque.back()));
            deque.pop_back();
        }
        sub_memory(popped_bytes);

        if (deque.empty()) {
            sub_memory(112 + key.size());
            shard.data.erase(it);
            shard.keys_with_ttl.erase(key);
        } else {
            it->second.touch(current_lru_clock());
        }

        dirty_ += out_popped.size();
        return ListPopStatus::Success;
    }

    struct BlockedWaiter {
        uint64_t client_id = 0;
        int protocol = 2;
        std::vector<std::string> keys;
        std::chrono::steady_clock::time_point deadline;
        bool has_deadline = false;
        std::function<void(std::string)> sink;
    };

    BlockingPopStatus try_pop_keys(const std::vector<std::string>& keys,
                                   std::string& selected_key, std::string& selected_value) {
        selected_key.clear();
        selected_value.clear();
        for (const auto& key : keys) {
            std::vector<std::string> values;
            auto status = rpop(key, 1, values);
            if (status == ListPopStatus::WrongType) return BlockingPopStatus::WrongType;
            if (status == ListPopStatus::Success && !values.empty()) {
                selected_key = key;
                selected_value = std::move(values.front());
                return BlockingPopStatus::Success;
            }
        }
        return BlockingPopStatus::Timeout;
    }

    void register_blocked_pop(uint64_t client_id, int protocol,
                              const std::vector<std::string>& keys,
                              uint64_t timeout_seconds,
                              std::function<void(std::string)> sink) {
        std::lock_guard<std::mutex> lock(blocked_mutex_);
        for (auto it = blocked_waiters_.begin(); it != blocked_waiters_.end();) {
            if (it->client_id == client_id) it = blocked_waiters_.erase(it);
            else ++it;
        }
        BlockedWaiter waiter;
        waiter.client_id = client_id;
        waiter.protocol = protocol;
        waiter.keys = keys;
        waiter.sink = std::move(sink);
        if (timeout_seconds > 0) {
            waiter.has_deadline = true;
            waiter.deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_seconds);
        } else {
            waiter.has_deadline = false;
        }
        blocked_waiters_.push_back(std::move(waiter));
    }

    void cancel_blocked_pop(uint64_t client_id) {
        std::lock_guard<std::mutex> lock(blocked_mutex_);
        for (auto it = blocked_waiters_.begin(); it != blocked_waiters_.end();) {
            if (it->client_id == client_id) it = blocked_waiters_.erase(it);
            else ++it;
        }
    }

    void notify_blocked_waiter_for_key(const std::string& key) {
        std::function<void(std::string)> to_deliver;
        std::string response_frame;
        {
            std::lock_guard<std::mutex> lock(blocked_mutex_);
            for (auto it = blocked_waiters_.begin(); it != blocked_waiters_.end(); ++it) {
                bool watches_key = false;
                for (const auto& k : it->keys) {
                    if (k == key) { watches_key = true; break; }
                }
                if (!watches_key) continue;

                std::string popped_key, popped_val;
                auto status = try_pop_keys(it->keys, popped_key, popped_val);
                if (status == BlockingPopStatus::Success) {
                    to_deliver = std::move(it->sink);
                    Resp::append_array_header(response_frame, 2);
                    Resp::append_bulk_string(response_frame, popped_key);
                    Resp::append_bulk_string(response_frame, popped_val);
                    blocked_waiters_.erase(it);
                    break;
                }
            }
        }
        if (to_deliver && !response_frame.empty()) {
            to_deliver(std::move(response_frame));
        }
    }

    void check_blocked_pop_timeouts() {
        std::vector<std::pair<std::function<void(std::string)>, std::string>> expired_deliveries;
        auto now = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> lock(blocked_mutex_);
            for (auto it = blocked_waiters_.begin(); it != blocked_waiters_.end();) {
                if (it->has_deadline && now >= it->deadline) {
                    std::string null_frame;
                    Resp::append_null_array(null_frame, it->protocol);
                    if (it->sink) {
                        expired_deliveries.emplace_back(std::move(it->sink), std::move(null_frame));
                    }
                    it = blocked_waiters_.erase(it);
                } else {
                    ++it;
                }
            }
        }
        for (auto& delivery : expired_deliveries) {
            delivery.first(std::move(delivery.second));
        }
    }

    BlockingPopStatus brpop(const std::vector<std::string>& keys, uint64_t timeout_seconds,
                            std::string& selected_key, std::string& selected_value) {
        auto status = try_pop_keys(keys, selected_key, selected_value);
        if (status != BlockingPopStatus::Timeout) return status;

        std::unique_lock<std::mutex> wait_lock(list_cv_mutex_);
        uint64_t observed_generation = list_generation_.load(std::memory_order_acquire);
        if (timeout_seconds == 0) {
            while (true) {
                list_cv_.wait(wait_lock, [&]() {
                    return list_generation_.load(std::memory_order_acquire) != observed_generation;
                });
                observed_generation = list_generation_.load(std::memory_order_acquire);
                status = try_pop_keys(keys, selected_key, selected_value);
                if (status != BlockingPopStatus::Timeout) return status;
            }
        }

        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_seconds);
        while (std::chrono::steady_clock::now() < deadline) {
            if (!list_cv_.wait_until(wait_lock, deadline, [&]() {
                    return list_generation_.load(std::memory_order_acquire) != observed_generation;
                })) break;
            observed_generation = list_generation_.load(std::memory_order_acquire);
            status = try_pop_keys(keys, selected_key, selected_value);
            if (status != BlockingPopStatus::Timeout) return status;
        }
        return BlockingPopStatus::Timeout;
    }

    ListLenStatus llen(const std::string& key, size_t& out_len) {
        out_len = 0;
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        uint64_t now = current_time_ms();
        uint64_t now_sec = current_lru_clock();
        {
            std::shared_lock<std::shared_mutex> lock(shard.mutex);
            auto it = shard.data.find(key);
            if (it == shard.data.end()) {
                return ListLenStatus::Success;
            }
            if (it->second.expire_at != 0 && it->second.expire_at <= now) {
            } else {
                if (!it->second.is_list()) {
                    return ListLenStatus::WrongType;
                }
                it->second.touch(now_sec);
                out_len = it->second.as_list().size();
                return ListLenStatus::Success;
            }
        }

        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it != shard.data.end()) {
            if (it->second.expire_at != 0 && it->second.expire_at <= current_time_ms()) {
                sub_memory(estimate_entry_memory(key, it->second));
                shard.data.erase(it);
                shard.keys_with_ttl.erase(key);
                return ListLenStatus::Success;
            }
            if (!it->second.is_list()) {
                return ListLenStatus::WrongType;
            }
            it->second.touch(now_sec);
            out_len = it->second.as_list().size();
        }
        return ListLenStatus::Success;
    }

    ListRangeStatus lrange(const std::string& key, int64_t start, int64_t stop, std::vector<std::string>& out_elements) {
        out_elements.clear();
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        uint64_t now = current_time_ms();
        uint64_t now_sec = current_lru_clock();
        {
            std::shared_lock<std::shared_mutex> lock(shard.mutex);
            auto it = shard.data.find(key);
            if (it == shard.data.end()) {
                return ListRangeStatus::NotFound;
            }
            if (it->second.expire_at != 0 && it->second.expire_at <= now) {
            } else {
                if (!it->second.is_list()) {
                    return ListRangeStatus::WrongType;
                }
                it->second.touch(now_sec);
                const auto& deque = it->second.as_list();
                int64_t len = static_cast<int64_t>(deque.size());
                if (len == 0) return ListRangeStatus::Success;

                if (start < 0) start = len + start;
                if (stop < 0) stop = len + stop;

                if (start < 0) start = 0;
                if (start >= len || start > stop) {
                    return ListRangeStatus::Success;
                }
                if (stop >= len) stop = len - 1;

                out_elements.reserve(stop - start + 1);
                for (int64_t i = start; i <= stop; ++i) {
                    out_elements.push_back(deque[i]);
                }
                return ListRangeStatus::Success;
            }
        }

        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it != shard.data.end() && it->second.expire_at != 0 && it->second.expire_at <= current_time_ms()) {
            sub_memory(estimate_entry_memory(key, it->second));
            shard.data.erase(it);
            shard.keys_with_ttl.erase(key);
            return ListRangeStatus::NotFound;
        }
        if (it == shard.data.end()) {
            return ListRangeStatus::NotFound;
        }
        if (!it->second.is_list()) {
            return ListRangeStatus::WrongType;
        }
        it->second.touch(now_sec);
        const auto& deque = it->second.as_list();
        int64_t len = static_cast<int64_t>(deque.size());
        if (len == 0) return ListRangeStatus::Success;

        if (start < 0) start = len + start;
        if (stop < 0) stop = len + stop;

        if (start < 0) start = 0;
        if (start >= len || start > stop) {
            return ListRangeStatus::Success;
        }
        if (stop >= len) stop = len - 1;

        out_elements.reserve(stop - start + 1);
        for (int64_t i = start; i <= stop; ++i) {
            out_elements.push_back(deque[i]);
        }
        return ListRangeStatus::Success;
    }

    ListRangeStatus lindex(const std::string& key, int64_t index, std::string& out_element) {
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        uint64_t now = current_time_ms();
        uint64_t now_sec = current_lru_clock();
        {
            std::shared_lock<std::shared_mutex> lock(shard.mutex);
            auto it = shard.data.find(key);
            if (it == shard.data.end()) {
                return ListRangeStatus::NotFound;
            }
            if (it->second.expire_at != 0 && it->second.expire_at <= now) {
            } else {
                if (!it->second.is_list()) {
                    return ListRangeStatus::WrongType;
                }
                it->second.touch(now_sec);
                const auto& deque = it->second.as_list();
                int64_t len = static_cast<int64_t>(deque.size());
                if (index < 0) index = len + index;
                if (index < 0 || index >= len) {
                    return ListRangeStatus::NotFound;
                }
                out_element = deque[index];
                return ListRangeStatus::Success;
            }
        }

        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it != shard.data.end() && it->second.expire_at != 0 && it->second.expire_at <= current_time_ms()) {
            sub_memory(estimate_entry_memory(key, it->second));
            shard.data.erase(it);
            shard.keys_with_ttl.erase(key);
            return ListRangeStatus::NotFound;
        }
        if (it == shard.data.end()) {
            return ListRangeStatus::NotFound;
        }
        if (!it->second.is_list()) {
            return ListRangeStatus::WrongType;
        }
        it->second.touch(now_sec);
        const auto& deque = it->second.as_list();
        int64_t len = static_cast<int64_t>(deque.size());
        if (index < 0) index = len + index;
        if (index < 0 || index >= len) {
            return ListRangeStatus::NotFound;
        }
        out_element = deque[index];
        return ListRangeStatus::Success;
    }

    size_t del_one(std::string_view key) {
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::string k(key);
        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        uint64_t now = current_time_ms();
        auto it = shard.data.find(k);
        if (it != shard.data.end()) {
            bool is_active = (it->second.expire_at == 0 || it->second.expire_at > now);
            sub_memory(estimate_entry_memory(k, it->second));
            shard.data.erase(it);
            if (!shard.keys_with_ttl.empty()) {
                shard.keys_with_ttl.erase(k);
            }
            if (is_active) {
                dirty_++;
                return 1;
            }
        }
        return 0;
    }

    size_t del(const std::vector<std::string_view>& keys) {
        if (keys.empty()) return 0;
        if (keys.size() == 1) return del_one(keys[0]);
        std::vector<size_t> involved_shards;
        involved_shards.reserve(keys.size());
        for (const auto& k : keys) {
            involved_shards.push_back(shard_index(k));
        }
        std::sort(involved_shards.begin(), involved_shards.end());
        involved_shards.erase(std::unique(involved_shards.begin(), involved_shards.end()), involved_shards.end());

        std::vector<std::unique_lock<std::shared_mutex>> locks;
        locks.reserve(involved_shards.size());
        for (size_t s_idx : involved_shards) {
            locks.emplace_back(shards_[s_idx].mutex);
        }

        size_t count = 0;
        uint64_t now = current_time_ms();
        for (const auto& key : keys) {
            size_t idx = shard_index(key);
            auto& shard = shards_[idx];
            std::string k(key);
            auto it = shard.data.find(k);
            if (it != shard.data.end()) {
                bool is_active = (it->second.expire_at == 0 || it->second.expire_at > now);
                sub_memory(estimate_entry_memory(k, it->second));
                shard.data.erase(it);
                if (!shard.keys_with_ttl.empty()) {
                    shard.keys_with_ttl.erase(k);
                }
                if (is_active) {
                    count++;
                }
            }
        }
        if (count > 0) {
            dirty_ += count;
        }
        return count;
    }

    size_t del(const std::vector<std::string>& keys) {
        if (keys.empty()) return 0;
        if (keys.size() == 1) {
            const auto& key = keys[0];
            size_t idx = shard_index(key);
            auto& shard = shards_[idx];
            std::unique_lock<std::shared_mutex> lock(shard.mutex);
            uint64_t now = current_time_ms();
            auto it = shard.data.find(key);
            if (it != shard.data.end()) {
                bool is_active = (it->second.expire_at == 0 || it->second.expire_at > now);
                sub_memory(estimate_entry_memory(key, it->second));
                shard.data.erase(it);
                shard.keys_with_ttl.erase(key);
                if (is_active) {
                    dirty_++;
                    return 1;
                }
            }
            return 0;
        }

        std::vector<size_t> involved_shards;
        involved_shards.reserve(keys.size());
        for (const auto& k : keys) {
            involved_shards.push_back(shard_index(k));
        }
        std::sort(involved_shards.begin(), involved_shards.end());
        involved_shards.erase(std::unique(involved_shards.begin(), involved_shards.end()), involved_shards.end());

        std::vector<std::unique_lock<std::shared_mutex>> locks;
        locks.reserve(involved_shards.size());
        for (size_t s_idx : involved_shards) {
            locks.emplace_back(shards_[s_idx].mutex);
        }

        size_t count = 0;
        uint64_t now = current_time_ms();
        for (const auto& key : keys) {
            size_t idx = shard_index(key);
            auto& shard = shards_[idx];
            auto it = shard.data.find(key);
            if (it != shard.data.end()) {
                bool is_active = (it->second.expire_at == 0 || it->second.expire_at > now);
                sub_memory(estimate_entry_memory(key, it->second));
                shard.data.erase(it);
                shard.keys_with_ttl.erase(key);
                if (is_active) {
                    count++;
                }
            }
        }
        if (count > 0) {
            dirty_ += count;
        }
        return count;
    }

    size_t exists_one(std::string_view key) {
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        uint64_t now = current_time_ms();
        std::string k(key);
        {
            std::shared_lock<std::shared_mutex> lock(shard.mutex);
            auto it = shard.data.find(k);
            if (it == shard.data.end()) {
                return 0;
            }
            if (it->second.expire_at == 0 || it->second.expire_at > now) {
                return 1;
            }
        }
        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(k);
        if (it != shard.data.end()) {
            if (it->second.expire_at != 0 && it->second.expire_at <= current_time_ms()) {
                sub_memory(estimate_entry_memory(k, it->second));
                shard.data.erase(it);
                if (!shard.keys_with_ttl.empty()) {
                    shard.keys_with_ttl.erase(k);
                }
                return 0;
            }
            return 1;
        }
        return 0;
    }

    size_t exists(const std::vector<std::string_view>& keys) {
        if (keys.empty()) return 0;
        if (keys.size() == 1) return exists_one(keys[0]);
        uint64_t now = current_time_ms();
        std::vector<std::string> expired_keys;
        size_t count = 0;

        std::vector<size_t> involved_shards;
        involved_shards.reserve(keys.size());
        for (const auto& k : keys) {
            involved_shards.push_back(shard_index(k));
        }
        std::sort(involved_shards.begin(), involved_shards.end());
        involved_shards.erase(std::unique(involved_shards.begin(), involved_shards.end()), involved_shards.end());

        std::vector<std::shared_lock<std::shared_mutex>> locks;
        locks.reserve(involved_shards.size());
        for (size_t s_idx : involved_shards) {
            locks.emplace_back(shards_[s_idx].mutex);
        }

        for (const auto& key : keys) {
            size_t idx = shard_index(key);
            auto& shard = shards_[idx];
            std::string k(key);
            auto it = shard.data.find(k);
            if (it != shard.data.end()) {
                if (it->second.expire_at != 0 && it->second.expire_at <= now) {
                    expired_keys.push_back(std::move(k));
                } else {
                    count++;
                }
            }
        }
        locks.clear();

        if (!expired_keys.empty()) {
            uint64_t cur_now = current_time_ms();
            for (const auto& key : expired_keys) {
                size_t idx = shard_index(key);
                auto& shard = shards_[idx];
                std::unique_lock<std::shared_mutex> lock(shard.mutex);
                auto it = shard.data.find(key);
                if (it != shard.data.end() && it->second.expire_at != 0 && it->second.expire_at <= cur_now) {
                    sub_memory(estimate_entry_memory(key, it->second));
                    shard.data.erase(it);
                    if (!shard.keys_with_ttl.empty()) {
                        shard.keys_with_ttl.erase(key);
                    }
                }
            }
        }

        return count;
    }

    size_t exists(const std::vector<std::string>& keys) {
        if (keys.empty()) return 0;
        uint64_t now = current_time_ms();
        std::vector<std::string> expired_keys;
        size_t count = 0;

        std::vector<size_t> involved_shards;
        involved_shards.reserve(keys.size());
        for (const auto& k : keys) {
            involved_shards.push_back(shard_index(k));
        }
        std::sort(involved_shards.begin(), involved_shards.end());
        involved_shards.erase(std::unique(involved_shards.begin(), involved_shards.end()), involved_shards.end());

        std::vector<std::shared_lock<std::shared_mutex>> locks;
        locks.reserve(involved_shards.size());
        for (size_t s_idx : involved_shards) {
            locks.emplace_back(shards_[s_idx].mutex);
        }

        for (const auto& key : keys) {
            size_t idx = shard_index(key);
            auto& shard = shards_[idx];
            auto it = shard.data.find(key);
            if (it != shard.data.end()) {
                if (it->second.expire_at != 0 && it->second.expire_at <= now) {
                    expired_keys.push_back(key);
                } else {
                    count++;
                }
            }
        }
        locks.clear();

        if (!expired_keys.empty()) {
            uint64_t cur_now = current_time_ms();
            for (const auto& key : expired_keys) {
                size_t idx = shard_index(key);
                auto& shard = shards_[idx];
                std::unique_lock<std::shared_mutex> lock(shard.mutex);
                auto it = shard.data.find(key);
                if (it != shard.data.end() && it->second.expire_at != 0 && it->second.expire_at <= cur_now) {
                    sub_memory(estimate_entry_memory(key, it->second));
                    shard.data.erase(it);
                    shard.keys_with_ttl.erase(key);
                }
            }
        }

        return count;
    }

    std::vector<std::string> keys(const std::string& pattern = "*") const {
        std::vector<std::string> result;
        uint64_t now = current_time_ms();

        if (pattern == "*") {
            for (const auto& shard : shards_) {
                std::shared_lock<std::shared_mutex> lock(shard.mutex);
                for (const auto& [k, v] : shard.data) {
                    if (v.expire_at == 0 || v.expire_at > now) {
                        result.push_back(k);
                    }
                }
            }
            return result;
        }

        bool has_wildcard = (pattern.find_first_of("*?[") != std::string::npos);
        if (!has_wildcard) {
            size_t idx = shard_index(pattern);
            auto& shard = shards_[idx];
            std::shared_lock<std::shared_mutex> lock(shard.mutex);
            auto it = shard.data.find(pattern);
            if (it != shard.data.end() && (it->second.expire_at == 0 || it->second.expire_at > now)) {
                result.push_back(pattern);
            }
            return result;
        }

        // Fast path for simple prefix "foo*"
        if (pattern.back() == '*' && pattern.front() != '*' && pattern.find_first_of("*?[") == pattern.size() - 1) {
            std::string prefix = pattern.substr(0, pattern.size() - 1);
            for (const auto& shard : shards_) {
                std::shared_lock<std::shared_mutex> lock(shard.mutex);
                for (const auto& [k, v] : shard.data) {
                    if (v.expire_at != 0 && v.expire_at <= now) continue;
                    if (k.rfind(prefix, 0) == 0) result.push_back(k);
                }
            }
            return result;
        }

        // Fast path for simple suffix "*foo"
        if (pattern.front() == '*' && pattern.back() != '*' && pattern.rfind('*') == 0 && pattern.find_first_of("?[") == std::string::npos) {
            std::string suffix = pattern.substr(1);
            for (const auto& shard : shards_) {
                std::shared_lock<std::shared_mutex> lock(shard.mutex);
                for (const auto& [k, v] : shard.data) {
                    if (v.expire_at != 0 && v.expire_at <= now) continue;
                    if (k.size() >= suffix.size() && k.compare(k.size() - suffix.size(), suffix.size(), suffix) == 0) {
                        result.push_back(k);
                    }
                }
            }
            return result;
        }

        // Fast path for simple substring "*foo*"
        if (pattern.front() == '*' && pattern.back() == '*' && pattern.size() > 2 && pattern.find_first_of("*?[", 1) == pattern.size() - 1) {
            std::string substr = pattern.substr(1, pattern.size() - 2);
            for (const auto& shard : shards_) {
                std::shared_lock<std::shared_mutex> lock(shard.mutex);
                for (const auto& [k, v] : shard.data) {
                    if (v.expire_at != 0 && v.expire_at <= now) continue;
                    if (k.find(substr) != std::string::npos) result.push_back(k);
                }
            }
            return result;
        }

        // General glob matching for patterns like "user:*:profile" or "[a-z]*"
        for (const auto& shard : shards_) {
            std::shared_lock<std::shared_mutex> lock(shard.mutex);
            for (const auto& [k, v] : shard.data) {
                if (v.expire_at != 0 && v.expire_at <= now) continue;
                if (constants::glob_match(pattern, k)) {
                    result.push_back(k);
                }
            }
        }

        return result;
    }

    int expire(const std::string& key, uint64_t ttl_ms, uint64_t* expire_at_result = nullptr) {
        if (expire_at_result) {
            *expire_at_result = 0;
        }

        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it == shard.data.end()) {
            return 0;
        }
        uint64_t now = current_time_ms();
        uint64_t now_wall = wall_time_ms();
        if (it->second.expire_at != 0 && it->second.expire_at <= now) {
            sub_memory(estimate_entry_memory(key, it->second));
            shard.data.erase(it);
            shard.keys_with_ttl.erase(key);
            return 0;
        }

        if (ttl_ms == 0) {
            sub_memory(estimate_entry_memory(key, it->second));
            shard.data.erase(it);
            shard.keys_with_ttl.erase(key);
            dirty_++;
            return 1;
        }

        it->second.expire_at = now + ttl_ms;
        it->second.touch(current_lru_clock());
        shard.keys_with_ttl.insert(key);
        if (expire_at_result) {
            *expire_at_result = now_wall + ttl_ms;
        }
        dirty_++;
        return 1;
    }

    int expire_at(const std::string& key, uint64_t expire_at_epoch_ms) {
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it == shard.data.end()) {
            return 0;
        }

        uint64_t now = current_time_ms();
        uint64_t now_wall = wall_time_ms();
        if (it->second.expire_at != 0 && it->second.expire_at <= now) {
            sub_memory(estimate_entry_memory(key, it->second));
            shard.data.erase(it);
            shard.keys_with_ttl.erase(key);
            return 0;
        }

        if (expire_at_epoch_ms <= now_wall) {
            sub_memory(estimate_entry_memory(key, it->second));
            shard.data.erase(it);
            shard.keys_with_ttl.erase(key);
            dirty_++;
            return 1;
        }

        it->second.expire_at = now + (expire_at_epoch_ms - now_wall);
        it->second.touch(current_lru_clock());
        shard.keys_with_ttl.insert(key);
        dirty_++;
        return 1;
    }

    long long ttl(const std::string& key, bool in_milliseconds) {
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        uint64_t now = current_time_ms();
        {
            std::shared_lock<std::shared_mutex> lock(shard.mutex);
            auto it = shard.data.find(key);
            if (it == shard.data.end()) {
                return -2;
            }
            if (it->second.expire_at == 0) {
                return -1;
            }
            if (it->second.expire_at > now) {
                uint64_t diff = it->second.expire_at - now;
                if (in_milliseconds) {
                    return static_cast<long long>(diff);
                }
                return static_cast<long long>((diff + 999) / 1000);
            }
        }

        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it != shard.data.end() && it->second.expire_at != 0 && it->second.expire_at <= current_time_ms()) {
            sub_memory(estimate_entry_memory(key, it->second));
            shard.data.erase(it);
            shard.keys_with_ttl.erase(key);
        }
        return -2;
    }

    int persist(const std::string& key) {
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it == shard.data.end()) {
            return 0;
        }
        uint64_t now = current_time_ms();
        if (it->second.expire_at != 0 && it->second.expire_at <= now) {
            sub_memory(estimate_entry_memory(key, it->second));
            shard.data.erase(it);
            shard.keys_with_ttl.erase(key);
            return 0;
        }
        if (it->second.expire_at == 0) {
            return 0;
        }
        it->second.expire_at = 0;
        it->second.touch(current_lru_clock());
        shard.keys_with_ttl.erase(key);
        dirty_++;
        return 1;
    }

    void flushdb() {
        for (auto& shard : shards_) {
            std::unique_lock<std::shared_mutex> lock(shard.mutex);
            shard.data.clear();
            shard.keys_with_ttl.clear();
        }
        used_memory_.store(0, std::memory_order_relaxed);
        allocator::purge_freed_memory();
        dirty_++;
    }

    size_t size() const {
        uint64_t now = current_time_ms();
        size_t count = 0;
        for (const auto& shard : shards_) {
            std::shared_lock<std::shared_mutex> lock(shard.mutex);
            for (const auto& [k, v] : shard.data) {
                if (v.expire_at == 0 || v.expire_at > now) {
                    count++;
                }
            }
        }
        return count;
    }

    size_t expires_size() const {
        uint64_t now = current_time_ms();
        size_t count = 0;
        for (const auto& shard : shards_) {
            std::shared_lock<std::shared_mutex> lock(shard.mutex);
            for (const auto& k : shard.keys_with_ttl) {
                auto it = shard.data.find(k);
                if (it != shard.data.end() && (it->second.expire_at == 0 || it->second.expire_at > now)) {
                    count++;
                }
            }
        }
        return count;
    }

    void evict_expired(size_t batch_limit = constants::DEFAULT_EVICTION_BATCH_LIMIT) {
        uint64_t now = current_time_ms();
        size_t limit_per_shard = (batch_limit + NUM_SHARDS - 1) / NUM_SHARDS;
        for (auto& shard : shards_) {
            std::unique_lock<std::shared_mutex> lock(shard.mutex);
            if (shard.keys_with_ttl.empty()) continue;
            std::vector<std::string> to_remove;
            size_t checked = 0;
            for (auto it = shard.keys_with_ttl.begin(); it != shard.keys_with_ttl.end() && checked < limit_per_shard; ++it, ++checked) {
                auto data_it = shard.data.find(*it);
                if (data_it == shard.data.end()) {
                    to_remove.push_back(*it);
                } else if (data_it->second.expire_at != 0 && data_it->second.expire_at <= now) {
                    to_remove.push_back(*it);
                    sub_memory(estimate_entry_memory(*it, data_it->second));
                    shard.data.erase(data_it);
                }
            }
            for (const auto& k : to_remove) {
                shard.keys_with_ttl.erase(k);
            }
        }
    }

    void start_active_eviction() {
        if (active_eviction_running_.exchange(true)) {
            return;
        }
        eviction_thread_ = std::thread([this]() {
            while (active_eviction_running_) {
                {
                    std::unique_lock<std::mutex> lk(eviction_cv_mutex_);
                    eviction_cv_.wait_for(lk, std::chrono::milliseconds(50), [this]() {
                        return !active_eviction_running_.load();
                    });
                }
                if (!active_eviction_running_) {
                    break;
                }
                evict_expired();
                check_blocked_pop_timeouts();
            }
        });
    }

    void stop_active_eviction() {
        if (!active_eviction_running_.exchange(false)) {
            return;
        }
        eviction_cv_.notify_all();
        if (eviction_thread_.joinable()) {
            eviction_thread_.join();
        }
    }

    uint64_t dirty_count() const {
        return dirty_.load();
    }

    void reset_dirty() {
        dirty_.store(0);
    }

    std::vector<DumpEntry> get_all_entries() const {
        uint64_t now_steady = current_time_ms();
        uint64_t now_wall = wall_time_ms();
        std::vector<DumpEntry> result;

        std::vector<std::shared_lock<std::shared_mutex>> locks;
        locks.reserve(NUM_SHARDS);
        for (const auto& shard : shards_) {
            locks.emplace_back(shard.mutex);
        }

        for (const auto& shard : shards_) {
            for (const auto& [k, v] : shard.data) {
                if (v.expire_at != 0 && v.expire_at <= now_steady) {
                    continue;
                }
                uint64_t expire_at_wall = 0;
                if (v.expire_at > now_steady) {
                    uint64_t remaining_ms = v.expire_at - now_steady;
                    expire_at_wall = now_wall + remaining_ms;
                }
                if (v.is_string()) {
                    DumpEntry dump;
                    dump.key = k;
                    dump.type = EntryType::String;
                    dump.string_val = v.as_string();
                    dump.expire_at_epoch_ms = expire_at_wall;
                    result.push_back(std::move(dump));
                } else if (v.is_list()) {
                    std::vector<std::string> elements(v.as_list().begin(), v.as_list().end());
                    DumpEntry dump;
                    dump.key = k;
                    dump.type = EntryType::List;
                    dump.list_val = std::move(elements);
                    dump.expire_at_epoch_ms = expire_at_wall;
                    result.push_back(std::move(dump));
                } else if (v.is_hash()) {
                    DumpEntry dump;
                    dump.key = k;
                    dump.type = EntryType::Hash;
                    dump.hash_val.assign(v.as_hash().begin(), v.as_hash().end());
                    dump.expire_at_epoch_ms = expire_at_wall;
                    result.push_back(std::move(dump));
                } else if (v.is_set()) {
                    DumpEntry dump;
                    dump.key = k;
                    dump.type = EntryType::Set;
                    dump.set_val.assign(v.as_set().begin(), v.as_set().end());
                    dump.expire_at_epoch_ms = expire_at_wall;
                    result.push_back(std::move(dump));
                } else if (v.is_zset()) {
                    DumpEntry dump;
                    dump.key = k;
                    dump.type = EntryType::ZSet;
                    dump.zset_val.assign(v.as_zset().begin(), v.as_zset().end());
                    dump.expire_at_epoch_ms = expire_at_wall;
                    result.push_back(std::move(dump));
                }
            }
        }
        return result;
    }

    void restore_string_entry(const std::string& key, const std::string& value, uint64_t expire_at_epoch_ms) {
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it != shard.data.end()) {
            sub_memory(estimate_entry_memory(key, it->second));
        }
        if (expire_at_epoch_ms == 0) {
            shard.data[key] = Entry{value, 0, current_lru_clock()};
            shard.keys_with_ttl.erase(key);
            add_memory(estimate_string_memory(key, value));
        } else {
            uint64_t now_wall = wall_time_ms();
            if (expire_at_epoch_ms <= now_wall) {
                shard.data.erase(key);
                shard.keys_with_ttl.erase(key);
            } else {
                uint64_t remaining_ms = expire_at_epoch_ms - now_wall;
                shard.data[key] = Entry{value, current_time_ms() + remaining_ms, current_lru_clock()};
                shard.keys_with_ttl.insert(key);
                add_memory(estimate_string_memory(key, value));
            }
        }
    }

    void restore_list_entry(const std::string& key, const std::vector<std::string>& elements, uint64_t expire_at_epoch_ms) {
        if (elements.empty()) return;
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it != shard.data.end()) {
            sub_memory(estimate_entry_memory(key, it->second));
        }
        std::deque<std::string> deque(elements.begin(), elements.end());
        size_t elem_mem = elements.size() * 48;
        for (const auto& el : elements) {
            elem_mem += el.size();
        }
        if (expire_at_epoch_ms == 0) {
            shard.data[key] = Entry{std::move(deque), 0, current_lru_clock()};
            shard.keys_with_ttl.erase(key);
            add_memory(112 + key.size() + elem_mem);
        } else {
            uint64_t now_wall = wall_time_ms();
            if (expire_at_epoch_ms <= now_wall) {
                shard.data.erase(key);
                shard.keys_with_ttl.erase(key);
            } else {
                uint64_t remaining_ms = expire_at_epoch_ms - now_wall;
                shard.data[key] = Entry{std::move(deque), current_time_ms() + remaining_ms, current_lru_clock()};
                shard.keys_with_ttl.insert(key);
                add_memory(112 + key.size() + elem_mem);
            }
        }
    }

    void restore_hash_entry(const std::string& key, const std::vector<std::pair<std::string, std::string>>& fields,
                            uint64_t expire_at_epoch_ms) {
        if (fields.empty()) return;
        Hash hash;
        for (const auto& pair : fields) hash.emplace(pair);
        restore_collection_entry(key, Entry{std::move(hash)}, expire_at_epoch_ms);
    }

    void restore_set_entry(const std::string& key, const std::vector<std::string>& values,
                           uint64_t expire_at_epoch_ms) {
        if (values.empty()) return;
        Set set(values.begin(), values.end());
        restore_collection_entry(key, Entry{std::move(set)}, expire_at_epoch_ms);
    }

    void restore_zset_entry(const std::string& key, const std::vector<std::pair<std::string, double>>& values,
                            uint64_t expire_at_epoch_ms) {
        if (values.empty()) return;
        ZSet zset;
        for (const auto& pair : values) zset[pair.first] = pair.second;
        restore_collection_entry(key, Entry{std::move(zset)}, expire_at_epoch_ms);
    }

    void restore_entry(const std::string& key, const std::string& value, uint64_t expire_at_epoch_ms) {
        restore_string_entry(key, value, expire_at_epoch_ms);
    }

private:
    void restore_collection_entry(const std::string& key, Entry entry, uint64_t expire_at_epoch_ms) {
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        auto it = shard.data.find(key);
        if (it != shard.data.end()) sub_memory(estimate_entry_memory(key, it->second));
        if (expire_at_epoch_ms == 0) {
            entry.expire_at = 0;
            entry.touch(current_lru_clock());
            shard.data[key] = std::move(entry);
            shard.keys_with_ttl.erase(key);
        } else {
            uint64_t now_wall = wall_time_ms();
            if (expire_at_epoch_ms <= now_wall) {
                shard.data.erase(key);
                shard.keys_with_ttl.erase(key);
                return;
            }
            entry.expire_at = current_time_ms() + (expire_at_epoch_ms - now_wall);
            entry.touch(current_lru_clock());
            shard.data[key] = std::move(entry);
            shard.keys_with_ttl.insert(key);
        }
        add_memory(estimate_entry_memory(key, shard.data.at(key)));
    }

    IncrStatus modify_int(std::string_view key, int64_t delta, int64_t& result_val, bool is_decrement) {
        size_t idx = shard_index(key);
        auto& shard = shards_[idx];
        std::unique_lock<std::shared_mutex> lock(shard.mutex);
        uint64_t now = current_time_ms();
        std::string k(key);
        auto it = shard.data.find(k);
        if (it != shard.data.end() && it->second.expire_at != 0 && it->second.expire_at <= now) {
            sub_memory(estimate_entry_memory(k, it->second));
            shard.data.erase(it);
            if (!shard.keys_with_ttl.empty()) {
                shard.keys_with_ttl.erase(k);
            }
            it = shard.data.end();
        }

        int64_t current_val = 0;
        if (it != shard.data.end()) {
            if (!it->second.is_string()) {
                return IncrStatus::WrongType;
            }
            if (!parse_int64(it->second.as_string(), current_val)) {
                return IncrStatus::NotAnInteger;
            }
        }

        int64_t new_val = 0;
        bool overflow = is_decrement ? sub_overflow(current_val, delta, new_val)
                                     : add_overflow(current_val, delta, new_val);
        if (overflow) {
            return IncrStatus::Overflow;
        }

        char num_buf[32];
        auto [ptr, ec] = std::to_chars(num_buf, num_buf + sizeof(num_buf), new_val);
        std::string_view new_sv(num_buf, ptr - num_buf);
        uint64_t now_sec = current_lru_clock();
        if (it != shard.data.end()) {
            size_t old_size = it->second.as_string().size();
            size_t new_size = new_sv.size();
            if (new_size > old_size) {
                add_memory(new_size - old_size);
            } else if (old_size > new_size) {
                sub_memory(old_size - new_size);
            }
            it->second.as_string().assign(new_sv.data(), new_sv.size());
            it->second.touch(now_sec);
        } else {
            shard.data.emplace(std::piecewise_construct,
                               std::forward_as_tuple(std::move(k)),
                               std::forward_as_tuple(std::string(new_sv), 0, now_sec));
            add_memory(estimate_string_memory(key, new_sv));
        }

        dirty_++;
        result_val = new_val;
        return IncrStatus::Success;
    }

    struct alignas(64) Shard {
        mutable std::shared_mutex mutex;
        std::unordered_map<std::string, Entry> data;
        std::unordered_set<std::string> keys_with_ttl;
    };

    std::array<Shard, NUM_SHARDS> shards_;

    inline size_t shard_index(std::string_view key) const noexcept {
        uint64_t hash = 14695981039346656037ULL;
        for (char c : key) {
            hash ^= static_cast<uint8_t>(c);
            hash *= 1099511628211ULL;
        }
        return static_cast<size_t>(hash) & (NUM_SHARDS - 1);
    }

    std::atomic<uint64_t> dirty_{0};
    std::atomic<bool> active_eviction_running_{false};
    std::thread eviction_thread_;
    std::mutex eviction_cv_mutex_;
    std::condition_variable eviction_cv_;
    std::mutex list_cv_mutex_;
    std::condition_variable list_cv_;
    std::atomic<uint64_t> list_generation_{0};
    std::mutex blocked_mutex_;
    std::list<BlockedWaiter> blocked_waiters_;
    std::atomic<size_t> used_memory_{0};
    std::atomic<size_t> used_memory_peak_{0};
    std::atomic<size_t> maxmemory_{0};
    constants::MaxmemoryPolicy maxmemory_policy_{constants::MaxmemoryPolicy::NoEviction};
    std::atomic<size_t> evicted_keys_count_{0};
};

}

#endif // KVLLAY_STORE_HPP
