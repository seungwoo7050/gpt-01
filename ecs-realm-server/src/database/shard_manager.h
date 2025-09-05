#pragma once

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace mmorpg::database {

class ConnectionPool;

// [SEQUENCE: MVP7-42] Manages database sharding logic, routing keys to the correct database server.
class ShardManager {
public:
    static ShardManager& Instance();

    void Initialize(size_t num_shards);
    std::shared_ptr<ConnectionPool> GetPoolForKey(uint64_t key);
    std::shared_ptr<ConnectionPool> GetPoolForShard(size_t shard_index);

private:
    ShardManager() = default;
    ~ShardManager() = default;
    ShardManager(const ShardManager&) = delete;
    ShardManager& operator=(const ShardManager&) = delete;

    size_t GetShardIndexForKey(uint64_t key);

    size_t m_num_shards = 0;
};

}