#include "database/cache_manager.h"
#include <spdlog/spdlog.h>

namespace mmorpg::database {

// --- Cache Implementation ---

void Cache::Put(const std::string& key, const std::string& value, std::chrono::seconds ttl) {
    std::lock_guard lock(m_mutex);
    m_data[key] = {value, std::chrono::system_clock::now(), ttl};
}

std::optional<std::string> Cache::Get(const std::string& key) {
    std::lock_guard lock(m_mutex);
    auto it = m_data.find(key);
    if (it != m_data.end()) {
        it->second.last_access_time = std::chrono::system_clock::now();
        return it->second.value;
    }
    return std::nullopt;
}

void Cache::Remove(const std::string& key) {
    std::lock_guard lock(m_mutex);
    m_data.erase(key);
}

void Cache::EvictExpired() {
    std::lock_guard lock(m_mutex);
    auto now = std::chrono::system_clock::now();
    for (auto it = m_data.begin(); it != m_data.end(); ) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - it->second.last_access_time);
        if (elapsed > it->second.ttl) {
            it = m_data.erase(it);
        } else {
            ++it;
        }
    }
}

// --- CacheManager Implementation ---

CacheManager& CacheManager::Instance() {
    static CacheManager instance;
    return instance;
}

CacheManager::CacheManager() {
    m_eviction_thread = std::thread(&CacheManager::EvictionLoop, this);
}

CacheManager::~CacheManager() {
    Shutdown();
}

void CacheManager::Shutdown() {
    if (!m_shutting_down.exchange(true)) {
        if (m_eviction_thread.joinable()) {
            m_eviction_thread.join();
        }
    }
}

std::shared_ptr<Cache> CacheManager::GetOrCreateCache(const std::string& cache_name) {
    std::lock_guard lock(m_mutex);
    auto it = m_caches.find(cache_name);
    if (it != m_caches.end()) {
        return it->second;
    }
    auto new_cache = std::make_shared<Cache>();
    m_caches[cache_name] = new_cache;
    return new_cache;
}

void CacheManager::EvictionLoop() {
    while (!m_shutting_down) {
        {
            std::lock_guard lock(m_mutex);
            for (const auto& pair : m_caches) {
                pair.second->EvictExpired();
            }
        }
        std::this_thread::sleep_for(std::chrono::seconds(5)); // Eviction check interval
    }
}

}
