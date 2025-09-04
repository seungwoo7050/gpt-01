#include "database/distributed_lock_manager.h"
#include <spdlog/spdlog.h>

namespace mmorpg::database {

DistributedLockManager& DistributedLockManager::Instance() {
    static DistributedLockManager instance;
    return instance;
}

void DistributedLockManager::Initialize(const std::string& redis_uri) {
    try {
        m_redis = std::make_unique<sw::redis::Redis>(redis_uri);
        spdlog::info("[DistributedLock] Connected to Redis at {}", redis_uri);
    } catch (const sw::redis::Error& e) {
        spdlog::critical("[DistributedLock] Failed to connect to Redis: {}", e.what());
        // In a real application, you might want to retry or handle this more gracefully.
    }
}

bool DistributedLockManager::Lock(const std::string& key, const std::string& value, std::chrono::milliseconds ttl) {
    if (!m_redis) {
        return false;
    }
    try {
        // SET key value PX ttl NX
        // PX -- Set the specified expire time, in milliseconds.
        // NX -- Only set the key if it does not already exist.
        return m_redis->set(key, value, ttl, sw::redis::UpdateType::NOT_EXIST);
    } catch (const sw::redis::Error& e) {
        spdlog::error("[DistributedLock] Failed to acquire lock for key '{}': {}", key, e.what());
        return false;
    }
}

void DistributedLockManager::Unlock(const std::string& key) {
    if (!m_redis) {
        return;
    }
    try {
        m_redis->del(key);
    } catch (const sw::redis::Error& e) {
        spdlog::error("[DistributedLock] Failed to release lock for key '{}': {}", key, e.what());
    }
}

}
