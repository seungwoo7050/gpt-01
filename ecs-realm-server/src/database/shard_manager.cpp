#include "database/shard_manager.h"
#include "database/connection_pool.h"
#include <stdexcept>

namespace mmorpg::database {

// [SEQUENCE: MVP7-43] Implements the sharding logic.
ShardManager& ShardManager::Instance() {
    static ShardManager instance;
    return instance;
}

void ShardManager::Initialize(size_t num_shards) {
    if (num_shards == 0) {
        throw std::invalid_argument("Number of shards must be greater than 0.");
    }
    m_num_shards = num_shards;
    // In a real application, you would loop here and call
    // ConnectionPoolManager::Instance().CreatePool(...) for each shard,
    // loading the configuration for each shard from a file.
}

size_t ShardManager::GetShardIndexForKey(uint64_t key) {
    if (m_num_shards == 0) {
        return 0;
    }
    // Simple modulo hashing. More advanced techniques like consistent hashing
    // could be used for easier resharding.
    return key % m_num_shards;
}

// [SEQUENCE: MVP7-44] Retrieves the correct connection pool for a given key by determining its shard.
std::shared_ptr<ConnectionPool> ShardManager::GetPoolForKey(uint64_t key) {
    size_t shard_index = GetShardIndexForKey(key);
    return GetPoolForShard(shard_index);
}

std::shared_ptr<ConnectionPool> ShardManager::GetPoolForShard(size_t shard_index) {
    if (shard_index >= m_num_shards) {
        return nullptr;
    }
    // Assumes pools are named "shard_0", "shard_1", etc.
    std::string pool_name = "shard_" + std::to_string(shard_index);
    return ConnectionPoolManager::Instance().GetPool(pool_name);
}

}