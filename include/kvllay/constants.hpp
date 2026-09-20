#ifndef KVLLAY_CONSTANTS_HPP
#define KVLLAY_CONSTANTS_HPP

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstdio>

namespace kvllay {
namespace constants {

inline constexpr const char* VERSION = "1.0.0";
inline constexpr const char* SERVER_NAME = "kvllay";
inline const std::string REDIS_VERSION_STRING = std::string(SERVER_NAME) + "-" + VERSION;
inline constexpr int DEFAULT_PORT = 6379;
inline constexpr const char* DEFAULT_HOST = "0.0.0.0";

inline constexpr size_t CLIENT_BUFFER_SIZE = 65536;

inline constexpr size_t DEFAULT_IO_THREADS = 0; // 0 = auto-detect CPU cores
inline constexpr int MAX_EVENTS_PER_LOOP = 256;
inline constexpr int EPOLL_TIMEOUT_MS = 50;

inline constexpr uint64_t DEFAULT_EVICTION_INTERVAL_MS = 100;
inline constexpr size_t DEFAULT_EVICTION_BATCH_LIMIT = 100;

inline constexpr const char* CRLF = "\r\n";

inline constexpr const char* DEFAULT_SNAPSHOT_FILE = "dump.kvl";
inline constexpr const char* DEFAULT_AOF_FILE = "kvllay.aof";
inline constexpr uint64_t DEFAULT_SAVE_INTERVAL_SECS = 0;
inline constexpr uint64_t DEFAULT_SAVE_CHANGES = 100;
inline constexpr size_t AOF_BUFFER_FLUSH_INTERVAL_MS = 50;

inline constexpr size_t DEFAULT_MAXMEMORY = 0;
inline constexpr const char* DEFAULT_MAXMEMORY_POLICY = "noeviction";

enum class MaxmemoryPolicy {
    NoEviction,
    AllKeysLru,
    VolatileLru,
    AllKeysRandom,
    VolatileTtl
};

inline bool parse_maxmemory_policy(const std::string& str, MaxmemoryPolicy& policy) {
    std::string s = str;
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    if (s == "noeviction") { policy = MaxmemoryPolicy::NoEviction; return true; }
    if (s == "allkeys-lru") { policy = MaxmemoryPolicy::AllKeysLru; return true; }
    if (s == "volatile-lru") { policy = MaxmemoryPolicy::VolatileLru; return true; }
    if (s == "allkeys-random") { policy = MaxmemoryPolicy::AllKeysRandom; return true; }
    if (s == "volatile-ttl") { policy = MaxmemoryPolicy::VolatileTtl; return true; }
    return false;
}

inline MaxmemoryPolicy parse_maxmemory_policy(const std::string& str) {
    MaxmemoryPolicy p = MaxmemoryPolicy::NoEviction;
    parse_maxmemory_policy(str, p);
    return p;
}

inline std::string maxmemory_policy_to_string(MaxmemoryPolicy p) {
    switch (p) {
        case MaxmemoryPolicy::AllKeysLru: return "allkeys-lru";
        case MaxmemoryPolicy::VolatileLru: return "volatile-lru";
        case MaxmemoryPolicy::AllKeysRandom: return "allkeys-random";
        case MaxmemoryPolicy::VolatileTtl: return "volatile-ttl";
        case MaxmemoryPolicy::NoEviction:
        default: return "noeviction";
    }
}

inline bool parse_memory_string(const std::string& str, size_t& out_bytes) {
    if (str.empty()) return false;
    size_t i = 0;
    while (i < str.size() && (std::isdigit(static_cast<unsigned char>(str[i])) || str[i] == '.')) {
        i++;
    }
    if (i == 0) return false;
    double val = 0.0;
    try {
        val = std::stod(str.substr(0, i));
    } catch (...) {
        return false;
    }
    if (val < 0.0) return false;

    std::string unit = str.substr(i);
    std::transform(unit.begin(), unit.end(), unit.begin(), ::tolower);

    double multiplier = 1.0;
    if (unit.empty() || unit == "b") {
        multiplier = 1.0;
    } else if (unit == "k" || unit == "kb" || unit == "kib") {
        multiplier = 1024.0;
    } else if (unit == "m" || unit == "mb" || unit == "mib") {
        multiplier = 1024.0 * 1024.0;
    } else if (unit == "g" || unit == "gb" || unit == "gib") {
        multiplier = 1024.0 * 1024.0 * 1024.0;
    } else {
        return false;
    }

    out_bytes = static_cast<size_t>(val * multiplier);
    return true;
}

inline std::string format_memory_human(size_t bytes) {
    if (bytes < 1024) {
        return std::to_string(bytes) + "B";
    }
    char buf[32];
    if (bytes < 1024 * 1024) {
        std::snprintf(buf, sizeof(buf), "%.2fK", static_cast<double>(bytes) / 1024.0);
    } else if (bytes < 1024ULL * 1024ULL * 1024ULL) {
        std::snprintf(buf, sizeof(buf), "%.2fM", static_cast<double>(bytes) / (1024.0 * 1024.0));
    } else {
        std::snprintf(buf, sizeof(buf), "%.2fG", static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    }
    return std::string(buf);
}

inline bool glob_match(std::string_view pattern, std::string_view value) noexcept {
    size_t p = 0;
    size_t v = 0;
    size_t star = std::string_view::npos;
    size_t match = 0;
    while (v < value.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == value[v])) {
            ++p;
            ++v;
        } else if (p < pattern.size() && pattern[p] == '*') {
            star = p++;
            match = v;
        } else if (star != std::string_view::npos) {
            p = star + 1;
            v = ++match;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*') ++p;
    return p == pattern.size();
}

}
}

#endif // KVLLAY_CONSTANTS_HPP
