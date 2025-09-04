#pragma once

#include <sw/redis++/redis++.h>
#include <string>
#include <memory>
#include <chrono>

namespace mmorpg::database {

// [SEQUENCE: MVP7-34] Manages distributed locks using Redis.
class DistributedLockManager {
public:
    static DistributedLockManager& Instance();

    void Initialize(const std::string& redis_uri);
    bool Lock(const std::string& key, const std::string& value, std::chrono::milliseconds ttl);
    void Unlock(const std::string& key);

private:
    DistributedLockManager() = default;
    ~DistributedLockManager() = default;
    DistributedLockManager(const DistributedLockManager&) = delete;
    DistributedLockManager& operator=(const DistributedLockManager&) = delete;

    std::unique_ptr<sw::redis::Redis> m_redis;
};

}
