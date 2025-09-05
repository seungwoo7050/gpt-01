#pragma once

#include <string>
#include <vector>
#include <memory>
#include <atomic>

namespace mmorpg::database {

class ConnectionPool;

// [SEQUENCE: MVP7-36] Manages routing queries to a primary (write) or read replica databases.
// This is a key component for implementing a read/write splitting architecture.
class ReadReplicaManager {
public:
    static ReadReplicaManager& Instance();

    void Initialize(const std::string& primary_pool_name, const std::vector<std::string>& replica_pool_names);
    
    std::shared_ptr<ConnectionPool> GetWritePool();
    std::shared_ptr<ConnectionPool> GetReadPool();

private:
    ReadReplicaManager() = default;
    ~ReadReplicaManager() = default;
    ReadReplicaManager(const ReadReplicaManager&) = delete;
    ReadReplicaManager& operator=(const ReadReplicaManager&) = delete;

    std::string m_primary_pool_name;
    std::vector<std::string> m_replica_pool_names;
    std::atomic<size_t> m_next_replica_index{0};
};

}