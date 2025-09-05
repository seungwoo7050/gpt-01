#include "database/connection_pool.h"
#include "core/logger.h"
#include <mysqlx/xdevapi.h>

namespace mmorpg::database {

// --- PooledConnection Implementation ---

// [SEQUENCE: MVP7-8] Constructor for a pooled connection.
PooledConnection::PooledConnection(uint64_t id, const ConnectionPoolConfig& config)
    : m_id(id),
      m_config(config),
      m_state(ConnectionState::CONNECTING),
      m_created_time(std::chrono::system_clock::now()) {
    UpdateLastUsedTime();
}

PooledConnection::~PooledConnection() {
    if (m_session) {
        try {
            m_session->close();
        } catch (...) {
            // Ignore exceptions on close
        }
    }
}

// [SEQUENCE: MVP7-9] Establishes a connection to the database.
bool PooledConnection::Connect() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state == ConnectionState::IN_USE || m_state == ConnectionState::IDLE) {
        return true;
    }
    try {
        std::string url = "mysqlx://" + m_config.user + ":" + m_config.password + "@" + m_config.host + ":" + std::to_string(m_config.port) + "/" + m_config.schema;
        m_session = std::make_shared<mysqlx::Session>(url);
        m_state = ConnectionState::IDLE;
        core::Logger::GetLogger()->info("[DB] Connection {} established.", m_id);
        return true;
    } catch (const std::exception& e) {
        core::Logger::GetLogger()->error("[DB] Connection {} failed: {}", m_id, e.what());
        m_state = ConnectionState::BROKEN;
        return false;
    }
}

// [SEQUENCE: MVP7-10] Validates if the connection is still alive by executing a simple query.
bool PooledConnection::Validate() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_state != ConnectionState::IDLE && m_state != ConnectionState::IN_USE) {
        return false;
    }
    try {
        if (m_session) {
            m_session->sql("SELECT 1").execute();
            return true;
        }
    } catch (const std::exception& e) {
        core::Logger::GetLogger()->warn("[DB] Connection {} validation failed: {}", m_id, e.what());
        m_state = ConnectionState::BROKEN;
    }
    return false;
}

bool PooledConnection::IsExpired() const {
    auto age = std::chrono::system_clock::now() - m_created_time;
    return age > m_config.max_lifetime_ms;
}

void PooledConnection::SetState(ConnectionState state) {
    m_state = state;
    if (state == ConnectionState::IN_USE) {
        UpdateLastUsedTime();
    }
}

// --- ConnectionPool Implementation ---

// [SEQUENCE: MVP7-12] Initializes the connection pool, creating the initial set of connections.
ConnectionPool::ConnectionPool(const ConnectionPoolConfig& config)
    : m_config(config) {}

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
            core::Logger::GetLogger()->error("[DB] Failed to create initial connection.");
        }
    }
    m_stats.pool_size = m_pool.size();
    if (m_pool.size() < m_config.initial_size) {
        core::Logger::GetLogger()->error("[DB] Pool initialized with {} connections, less than required {}.", m_pool.size(), m_config.initial_size);
        return false;
    }
    return true;
}

// [SEQUENCE: MVP7-13] Acquires a connection from the pool, waiting if necessary.
std::shared_ptr<PooledConnection> ConnectionPool::Acquire(std::chrono::milliseconds timeout) {
    std::unique_lock lock(m_mutex);
    if (m_shutting_down) return nullptr;

    // Wait until a connection is available or the pool is shutting down.
    if (!m_condition.wait_for(lock, timeout, [this] { return !m_pool.empty() || m_shutting_down; })) {
        return nullptr; // Timeout
    }

    if (m_shutting_down) return nullptr;

    // Get a connection from the back of the vector.
    auto conn = m_pool.back();
    m_pool.pop_back();
    lock.unlock(); // Unlock before validating to allow other threads to access the pool.

    // Validate the connection before returning it.
    if (m_config.test_on_borrow && !conn->Validate()) {
        // If validation fails, destroy the connection and try to acquire a new one.
        // This is a simple recursive call; a more robust implementation might use a loop.
        return Acquire(timeout);
    }

    conn->SetState(ConnectionState::IN_USE);
    m_active_connections++;
    
    // Update stats
    std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
    m_stats.active_connections = m_active_connections;
    m_stats.total_acquired++;
    if (m_stats.active_connections > m_stats.peak_connections) {
        m_stats.peak_connections = m_stats.active_connections;
    }

    return conn;
}

// [SEQUENCE: MVP7-14] Releases a connection back to the pool.
void ConnectionPool::Release(std::shared_ptr<PooledConnection> connection) {
    if (!connection) return;

    if (connection->IsExpired() || connection->GetState() == ConnectionState::BROKEN) {
        // Don't return expired or broken connections to the pool.
        return;
    }

    {
        std::unique_lock lock(m_mutex);
        if (m_shutting_down) return;
        connection->SetState(ConnectionState::IDLE);
        m_pool.push_back(std::move(connection));
    }

    m_active_connections--;
    m_condition.notify_one(); // Notify one waiting thread that a connection is available.
    
    std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
    m_stats.total_released++;
    m_stats.active_connections = m_active_connections;
}

// [SEQUENCE: MVP7-15] Shuts down the connection pool, closing all connections.
void ConnectionPool::Shutdown() {
    m_shutting_down = true;
    m_condition.notify_all();
    std::unique_lock lock(m_mutex);
    for (auto& conn : m_pool) {
        conn.reset();
    }
    m_pool.clear();
}

ConnectionPoolStats ConnectionPool::GetStatistics() const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    return m_stats;
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

// [SEQUENCE: MVP7-17] Gets the singleton instance of the ConnectionPoolManager.
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

// [SEQUENCE: MVP7-18] Retrieves a named connection pool.
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