#include "database/read_replica_manager.h"
#include "database/connection_pool.h"

namespace mmorpg::database {

// [SEQUENCE: MVP7-37] Implements the singleton and connection pool retrieval logic.
ReadReplicaManager& ReadReplicaManager::Instance() {
    static ReadReplicaManager instance;
    return instance;
}

void ReadReplicaManager::Initialize(const std::string& primary_pool_name, const std::vector<std::string>& replica_pool_names) {
    m_primary_pool_name = primary_pool_name;
    m_replica_pool_names = replica_pool_names;
    m_next_replica_index = 0;
}

std::shared_ptr<ConnectionPool> ReadReplicaManager::GetWritePool() {
    return ConnectionPoolManager::Instance().GetPool(m_primary_pool_name);
}

// GetReadPool uses a simple atomic round-robin to distribute read queries among replica pools.
std::shared_ptr<ConnectionPool> ReadReplicaManager::GetReadPool() {
    if (m_replica_pool_names.empty()) {
        // Fallback to primary if no replicas are configured
        return GetWritePool();
    }

    size_t index = m_next_replica_index.fetch_add(1, std::memory_order_relaxed) % m_replica_pool_names.size();
    return ConnectionPoolManager::Instance().GetPool(m_replica_pool_names[index]);
}

}