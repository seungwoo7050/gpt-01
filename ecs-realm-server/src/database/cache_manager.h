#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <optional>

namespace mmorpg::database {

// [SEQUENCE: MVP7-26] Represents a single entry in the cache, storing the value and metadata like TTL.
struct CacheEntry {
    std::string value; // Storing serialized data as a string for simplicity
    std::chrono::system_clock::time_point last_access_time;
    std::chrono::seconds ttl;
};

// [SEQUENCE: MVP7-27] A thread-safe, in-memory key-value cache.
class Cache {
public:
    void Put(const std::string& key, const std::string& value, std::chrono::seconds ttl);
    std::optional<std::string> Get(const std::string& key);
    void Remove(const std::string& key);
    void EvictExpired();

private:
    mutable std::mutex m_mutex;
    std::unordered_map<std::string, CacheEntry> m_data;
};

// [SEQUENCE: MVP7-30] A singleton manager for all named caches in the system.
class CacheManager {
public:
    static CacheManager& Instance();

    std::shared_ptr<Cache> GetOrCreateCache(const std::string& cache_name);
    void Shutdown();

private:
    CacheManager();
    ~CacheManager();
    CacheManager(const CacheManager&) = delete;
    CacheManager& operator=(const CacheManager&) = delete;

    void EvictionLoop();

    std::mutex m_mutex;
    std::unordered_map<std::string, std::shared_ptr<Cache>> m_caches;
    std::thread m_eviction_thread;
    std::atomic<bool> m_shutting_down{false};
};

}