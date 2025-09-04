#include "database/connection_pool.h"
#include <spdlog/spdlog.h>
#include <mysqlx/xdevapi.h>

namespace mmorpg::database {

// --- PooledConnection Implementation ---

PooledConnection::PooledConnection(uint64_t id, const ConnectionPoolConfig& config)
    : m_id(id),
      m_config(config),
      m_state(ConnectionState::CONNECTING),
      m_created_time(std::chrono::system_clock::now()) {
    UpdateLastUsed();
}

PooledConnection::~PooledConnection() {
    if (m_session) {
        try {
            m_session->close();
        } catch (...) {
            // Ignore exceptions on close, as we are destructing anyway.
        }
    }
}

bool PooledConnection::Connect() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state == ConnectionState::IN_USE || m_state == ConnectionState::IDLE) {
        return true;
    }
    try {
        std::string url = "mysqlx://" + m_config.user + ":" + m_config.password + "@" + m_config.host + ":" + std::to_string(m_config.port) + "/" + m_config.schema;
        m_session = std::make_shared<mysqlx::Session>(url);
        m_state = ConnectionState::IDLE;
        spdlog::debug("[ConnectionPool] Connection {} established.", m_id);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("[ConnectionPool] Connection {} failed: {}", m_id, e.what());
        m_state = ConnectionState::BROKEN;
        return false;
    }
}

bool PooledConnection::Validate() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != ConnectionState::IDLE && m_state != ConnectionState::IN_USE) {
        return false;
    }
    try {
        if (m_session) { // Just check if session object exists
            m_session->sql("SELECT 1").execute();
            UpdateLastUsed();
            return true;
        }
    } catch (const std::exception& e) {
        spdlog::warn("[ConnectionPool] Connection {} validation failed: {}", m_id, e.what());
        m_state = ConnectionState::BROKEN;
    }
    return false;
}

bool PooledConnection::IsExpired() const {
    auto age = std::chrono::system_clock::now() - m_created_time;
    return age > m_config.max_lifetime_ms;
}

bool PooledConnection::IsIdle(std::chrono::milliseconds threshold) const {
    auto idle_time = std::chrono::system_clock::now() - m_last_used_time;
    return idle_time > threshold;
}


// --- ConnectionPool Implementation ---

ConnectionPool::ConnectionPool(const ConnectionPoolConfig& config)
    : m_config(config) {
}

ConnectionPool::~ConnectionPool() {
    if (!m_shutting_down) {
        Shutdown();
    }
}

bool ConnectionPool::Initialize() {
    std::unique_lock lock(m_mutex);
    for (size_t i = 0; i < m_config.initial_size; ++i) {
        auto conn = CreateConnection();
        if (conn) {
            m_pool.push_back(conn);
        } else {
            spdlog::error("[ConnectionPool] Failed to create initial connection.");
        }
    }
    if (m_pool.size() < m_config.initial_size) {
        spdlog::error("[ConnectionPool] Pool initialized with fewer connections than required.");
        return false;
    }
    return true;
}

void ConnectionPool::Shutdown() {
    m_shutting_down = true;
    m_condition.notify_all();
    std::unique_lock lock(m_mutex);
    m_pool.clear();
}

std::shared_ptr<PooledConnection> ConnectionPool::Acquire(std::chrono::milliseconds timeout) {
    std::unique_lock lock(m_mutex);
    if (m_condition.wait_for(lock, timeout, [this] { return !m_pool.empty() || m_shutting_down; })) {
        if (m_shutting_down) {
            return nullptr;
        }
        if (!m_pool.empty()) {
            auto conn = m_pool.back();
            m_pool.pop_back();
            
            if (m_config.test_on_borrow && !conn->Validate()) {
                return Acquire(timeout);
            }
            
            m_active_connections++;
            return conn;
        }
    }
    return nullptr;
}

void ConnectionPool::Release(std::shared_ptr<PooledConnection> connection) {
    if (!connection) return;
    std::unique_lock lock(m_mutex);
    if (m_shutting_down) return;
    
    if (connection->IsExpired() || connection->GetState() == ConnectionState::BROKEN) {
        // Don't return to pool
    } else {
        m_pool.push_back(connection);
    }
    m_active_connections--;
    m_condition.notify_one();
}

ConnectionPoolStats ConnectionPool::GetStatistics() const {
    return {};
}

std::shared_ptr<PooledConnection> ConnectionPool::CreateConnection() {
    static uint64_t next_id = 0;
    auto conn = std::make_shared<PooledConnection>(next_id++, m_config);
    if (conn->Connect()) {
        return conn;
    }
    return nullptr;
}


// --- ConnectionPoolManager Implementation ---

ConnectionPoolManager& ConnectionPoolManager::Instance() {
    static ConnectionPoolManager instance;
    return instance;
}

void ConnectionPoolManager::CreatePool(const std::string& pool_name, const ConnectionPoolConfig& config) {
    std::lock_guard<std::mutex> lock(m_manager_mutex);
    if (m_pools.find(pool_name) == m_pools.end()) {
        auto pool = std::make_shared<ConnectionPool>(config);
        if (pool->Initialize()) {
            m_pools[pool_name] = pool;
        }
    }
}

std::shared_ptr<ConnectionPool> ConnectionPoolManager::GetPool(const std::string& pool_name) {
    std::lock_guard<std::mutex> lock(m_manager_mutex);
    auto it = m_pools.find(pool_name);
    return (it != m_pools.end()) ? it->second : nullptr;
}

void ConnectionPoolManager::ShutdownAll() {
    std::lock_guard<std::mutex> lock(m_manager_mutex);
    for (auto const& [key, val] : m_pools) {
        val->Shutdown();
    }
    m_pools.clear();
}

}
