#ifndef KVLLAY_ALLOCATOR_HPP
#define KVLLAY_ALLOCATOR_HPP

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <cstdio>
#include <charconv>

#if defined(USE_JEMALLOC)
  #include <jemalloc/jemalloc.h>
#elif defined(USE_MIMALLOC)
  #include <mimalloc.h>
  #include <mimalloc-new-delete.h>
#endif

#if defined(_WIN32)
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif
  #include <windows.h>
  #include <psapi.h>
#else
  #include <unistd.h>
  #include <sys/resource.h>
  #if defined(__GLIBC__)
    #include <malloc.h>
  #endif
#endif

namespace kvllay {
namespace allocator {

inline std::string get_allocator_name() {
#if defined(USE_JEMALLOC)
    #if defined(JEMALLOC_VERSION_MAJOR) && defined(JEMALLOC_VERSION_MINOR) && defined(JEMALLOC_VERSION_BUGFIX)
    return "jemalloc-" + std::to_string(JEMALLOC_VERSION_MAJOR) + "." +
           std::to_string(JEMALLOC_VERSION_MINOR) + "." +
           std::to_string(JEMALLOC_VERSION_BUGFIX);
    #elif defined(JEMALLOC_VERSION)
    std::string ver = JEMALLOC_VERSION;
    auto dash = ver.find('-');
    if (dash != std::string::npos) {
        ver = ver.substr(0, dash);
    }
    return "jemalloc-" + ver;
    #else
    return "jemalloc";
    #endif
#elif defined(USE_MIMALLOC)
    #if defined(MI_MALLOC_VERSION)
    int v = mi_version();
    int major = v / 10000;
    int minor = (v % 10000) / 100;
    int patch = v % 100;
    return "mimalloc-" + std::to_string(major) + "." +
           std::to_string(minor) + "." + std::to_string(patch);
    #else
    return "mimalloc";
    #endif
#else
    return "libc";
#endif
}

inline size_t get_rss_bytes() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<size_t>(pmc.WorkingSetSize);
    }
    return 0;
#else
    FILE* fp = std::fopen("/proc/self/statm", "r");
    if (fp) {
        long total_pages = 0;
        long resident_pages = 0;
        if (std::fscanf(fp, "%ld %ld", &total_pages, &resident_pages) == 2) {
            std::fclose(fp);
            long page_size = sysconf(_SC_PAGESIZE);
            if (page_size > 0 && resident_pages > 0) {
                return static_cast<size_t>(resident_pages) * static_cast<size_t>(page_size);
            }
        } else {
            std::fclose(fp);
        }
    }

    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
#if defined(__APPLE__)
        return static_cast<size_t>(usage.ru_maxrss);
#else
        return static_cast<size_t>(usage.ru_maxrss) * 1024ULL;
#endif
    }
    return 0;
#endif
}

inline size_t get_peak_rss_bytes() {
#if defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<size_t>(pmc.PeakWorkingSetSize);
    }
    return 0;
#else
    struct rusage usage;
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
#if defined(__APPLE__)
        return static_cast<size_t>(usage.ru_maxrss);
#else
        return static_cast<size_t>(usage.ru_maxrss) * 1024ULL;
#endif
    }
    return 0;
#endif
}

inline void purge_freed_memory() {
#if defined(USE_JEMALLOC)
    mallctl("arena.4096.purge", nullptr, nullptr, nullptr, 0);
#elif defined(USE_MIMALLOC)
    mi_collect(true);
#elif defined(__GLIBC__) && !defined(_WIN32)
    malloc_trim(0);
#endif
}

inline double get_fragmentation_ratio(size_t used_memory_bytes, size_t rss_bytes) {
    if (used_memory_bytes == 0) {
        return 0.0;
    }
    return static_cast<double>(rss_bytes) / static_cast<double>(used_memory_bytes);
}

}
}

#endif
