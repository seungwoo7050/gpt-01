#include "database/distributed_lock_manager.h"
#include "core/logger.h"

namespace mmorpg::database {

// [SEQUENCE: MVP7-46] Implements the distributed lock logic using Redis commands.
DistributedLockManager& DistributedLockManager::Instance() {
    static DistributedLockManager instance;
    return instance;
}

void DistributedLockManager::Initialize(const std::string& redis_uri) {
    try {
        m_redis = std::make_unique<sw::redis::Redis>(redis_uri);
        core::Logger::GetLogger()->info("[DistributedLock] Connected to Redis at {}", redis_uri);
    } catch (const sw::redis::Error& e) {
        core::Logger::GetLogger()->critical("[DistributedLock] Failed to connect to Redis: {}", e.what());
    }
}

bool DistributedLockManager::Lock(const std::string& key, const std::string& value, std::chrono::milliseconds ttl) {
    if (!m_redis) {
        return false;
    }
    try {
        // Use Redis's atomic SET command with options for distributed locking:
        // NX: Only set the key if it does not already exist.
        // PX: Set the specified expire time in milliseconds.
        return m_redis->set(key, value, ttl, sw::redis::UpdateType::NOT_EXIST);
    } catch (const sw::redis::Error& e) {
        core::Logger::GetLogger()->error("[DistributedLock] Failed to acquire lock for key '{}': {}", key, e.what());
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
        core::Logger::GetLogger()->error("[DistributedLock] Failed to release lock for key '{}': {}", key, e.what());
    }
}

}