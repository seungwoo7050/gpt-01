#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <optional>
#include <map>

#include <mysqlx/xdevapi.h> // [SEQUENCE: MVP7-14] Include full definition for mysqlx::Session

namespace mmorpg::database {

// [SEQUENCE: MVP7-10] Defines the state of a connection in the pool.
enum class ConnectionState {
    IDLE,
    IN_USE,
    BROKEN,
    CONNECTING
};

// [SEQUENCE: MVP7-4] Defines the configuration for a database connection pool.
struct ConnectionPoolConfig {
    std::string host;
    int port;
    std::string user;
    std::string password;
    std::string schema;
    size_t initial_size = 5;
    size_t max_size = 20;
    std::chrono::seconds connect_timeout{10};
    std::chrono::milliseconds idle_timeout_ms{1800000}; // 30 minutes
    std::chrono::milliseconds max_lifetime_ms{1800000}; // 30 minutes
    bool test_on_borrow = true;
};

// [SEQUENCE: MVP7-5] Holds statistics about the connection pool's performance.
struct ConnectionPoolStats {
    size_t pool_size = 0;
    size_t active_connections = 0;
    size_t peak_connections = 0;
    uint64_t total_acquired = 0;
    uint64_t total_released = 0;
};

// [SEQUENCE: MVP7-11] Represents a single pooled connection, wrapping the actual DB session.
class PooledConnection {
public:
    PooledConnection(uint64_t id, const ConnectionPoolConfig& config);
    ~PooledConnection();

    bool Connect();
    bool Validate();
    bool IsExpired() const;
    bool IsIdle(std::chrono::milliseconds threshold) const;

    std::shared_ptr<mysqlx::Session> GetSession() { return m_session; }
    ConnectionState GetState() const { return m_state; }
    void SetState(ConnectionState state) { m_state = state; }
    uint64_t GetId() const { return m_id; }

private:
    void UpdateLastUsed() { m_last_used_time = std::chrono::system_clock::now(); }

    uint64_t m_id;
    const ConnectionPoolConfig& m_config;
    std::shared_ptr<mysqlx::Session> m_session;
    std::atomic<ConnectionState> m_state;
    std::chrono::system_clock::time_point m_created_time;
    std::chrono::system_clock::time_point m_last_used_time;
    std::mutex m_mutex;
};

// [SEQUENCE: MVP7-6] Manages a pool of database connections.
class ConnectionPool {
public:
    explicit ConnectionPool(const ConnectionPoolConfig& config);
    ~ConnectionPool();

    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;

    bool Initialize();
    void Shutdown();

    std::shared_ptr<PooledConnection> Acquire(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000));
    void Release(std::shared_ptr<PooledConnection> connection);
    ConnectionPoolStats GetStatistics() const;

private:
    std::shared_ptr<PooledConnection> CreateConnection();
    void DestroyConnection(std::shared_ptr<PooledConnection> conn);
    void ValidateConnections();
    void EvictExpiredConnections();

    ConnectionPoolConfig m_config;
    std::vector<std::shared_ptr<PooledConnection>> m_pool;
    std::mutex m_mutex;
    std::condition_variable m_condition;
    std::atomic<size_t> m_active_connections{0};
    std::atomic<bool> m_shutting_down{false};
    
    // Stats
    mutable std::mutex m_stats_mutex;
    ConnectionPoolStats m_stats;
};

// [SEQUENCE: MVP7-12] Manages multiple named connection pools.
class ConnectionPoolManager {
public:
    static ConnectionPoolManager& Instance();

    void CreatePool(const std::string& pool_name, const ConnectionPoolConfig& config);
    std::shared_ptr<ConnectionPool> GetPool(const std::string& pool_name);
    void ShutdownAll();

private:
    ConnectionPoolManager() = default;
    ~ConnectionPoolManager() = default;
    ConnectionPoolManager(const ConnectionPoolManager&) = delete;
    ConnectionPoolManager& operator=(const ConnectionPoolManager&) = delete;

    std::mutex m_manager_mutex;
    std::map<std::string, std::shared_ptr<ConnectionPool>> m_pools;
};

}