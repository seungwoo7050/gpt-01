#include "connection_pool.h"
#include "../core/logger.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace mmorpg::database {

// [SEQUENCE: MVP14-500] Pooled connection implementation
PooledConnection::PooledConnection(uint64_t id, const ConnectionPoolConfig& config)
    : id_(id)
    , config_(config)
    , created_time_(std::chrono::system_clock::now())
    , last_used_time_(created_time_) {
}

PooledConnection::~PooledConnection() {
    if (IsConnected()) {
        Disconnect();
    }
}

// [SEQUENCE: MVP14-501] Connect to database
bool PooledConnection::Connect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_ == ConnectionState::IN_USE || 
        state_ == ConnectionState::IDLE) {
        return true;  // Already connected
    }
    
    try {
        // In real implementation, would use MySQL C API
        // mysql_conn_ = mysql_init(nullptr);
        // mysql_real_connect(mysql_conn_, config_.host.c_str(), ...);
        
        state_ = ConnectionState::IDLE;
        spdlog::debug("[CONNECTION_POOL] Connection {} established to {}:{}", 
                     id_, config_.host, config_.port);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("[CONNECTION_POOL] Connection {} failed: {}", id_, e.what());
        state_ = ConnectionState::BROKEN;
        return false;
    }
}

// [SEQUENCE: MVP14-502] Validate connection
bool PooledConnection::Validate() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_ != ConnectionState::IDLE && state_ != ConnectionState::IN_USE) {
        return false;
    }
    
    try {
        // Execute validation query
        // In real implementation: mysql_query(mysql_conn_, config_.validation_query.c_str());
        
        UpdateLastUsed();
        return true;
    } catch (const std::exception& e) {
        spdlog::warn("[CONNECTION_POOL] Connection {} validation failed: {}", 
                    id_, e.what());
        state_ = ConnectionState::BROKEN;
        return false;
    }
}

// [SEQUENCE: MVP14-503] Check if connection is expired
bool PooledConnection::IsExpired() const {
    auto age = std::chrono::system_clock::now() - created_time_;
    auto age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(age).count();
    return age_ms > config_.max_lifetime_ms;
}

// [SEQUENCE: MVP14-504] Check if connection is idle
bool PooledConnection::IsIdle(std::chrono::milliseconds threshold) const {
    auto idle_time = std::chrono::system_clock::now() - last_used_time_;
    return std::chrono::duration_cast<std::chrono::milliseconds>(idle_time) > threshold;
}

// [SEQUENCE: MVP14-505] Execute query
QueryResult PooledConnection::Execute(const std::string& query,
                                    const std::vector<std::string>& params) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (state_ != ConnectionState::IN_USE) {
        throw std::runtime_error("Connection not in use");
    }
    
    UpdateLastUsed();
    
    // In real implementation, would execute query
    QueryResult result;
    result.success = true;
    return result;
}

// [SEQUENCE: MVP14-506] Connection pool constructor
ConnectionPool::ConnectionPool(const ConnectionPoolConfig& config)
    : config_(config) {
}

ConnectionPool::~ConnectionPool() {
    Shutdown();
}

// [SEQUENCE: MVP14-507] Initialize connection pool
bool ConnectionPool::Initialize() {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    spdlog::info("[CONNECTION_POOL] Initializing pool with {} connections", 
                config_.initial_connections);
    
    // Create initial connections
    for (uint32_t i = 0; i < config_.initial_connections; ++i) {
        auto conn = CreateConnection();
        if (conn && conn->Connect()) {
            idle_connections_.push(conn);
            all_connections_.push_back(conn);
            stats_.total_connections++;
            stats_.idle_connections++;
        } else {
            spdlog::error("[CONNECTION_POOL] Failed to create initial connection {}", i);
        }
    }
    
    if (stats_.total_connections < config_.min_connections) {
        spdlog::error("[CONNECTION_POOL] Failed to create minimum connections");
        return false;
    }
    
    // Start background threads
    running_ = true;
    validation_thread_ = std::thread([this] { ValidationLoop(); });
    eviction_thread_ = std::thread([this] { EvictionLoop(); });
    
    spdlog::info("[CONNECTION_POOL] Pool initialized with {} connections", 
                stats_.total_connections.load());
    return true;
}

// [SEQUENCE: MVP14-508] Shutdown connection pool
void ConnectionPool::Shutdown() {
    spdlog::info("[CONNECTION_POOL] Shutting down connection pool");
    
    running_ = false;
    pool_cv_.notify_all();
    
    // Wait for background threads
    if (validation_thread_.joinable()) {
        validation_thread_.join();
    }
    if (eviction_thread_.joinable()) {
        eviction_thread_.join();
    }
    
    // Close all connections
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    while (!idle_connections_.empty()) {
        idle_connections_.pop();
    }
    
    for (auto& conn : all_connections_) {
        conn->Disconnect();
    }
    
    all_connections_.clear();
    stats_ = ConnectionStats();
}

// [SEQUENCE: MVP14-509] Acquire connection from pool
std::shared_ptr<PooledConnection> ConnectionPool::Acquire() {
    auto start = std::chrono::high_resolution_clock::now();
    
    std::unique_lock<std::mutex> lock(pool_mutex_);
    
    // Try to get idle connection
    if (!idle_connections_.empty()) {
        auto conn = idle_connections_.front();
        idle_connections_.pop();
        
        // Validate if configured
        if (config_.test_on_borrow && !conn->Validate()) {
            stats_.validation_failures++;
            DestroyConnection(conn);
            // Try again
            return Acquire();
        }
        
        conn->SetState(ConnectionState::IN_USE);
        stats_.idle_connections--;
        stats_.active_connections++;
        stats_.connections_borrowed++;
        
        return conn;
    }
    
    // Create new connection if under limit
    if (stats_.total_connections < config_.max_connections) {
        auto conn = CreateConnection();
        if (conn && conn->Connect()) {
            all_connections_.push_back(conn);
            stats_.total_connections++;
            
            conn->SetState(ConnectionState::IN_USE);
            stats_.active_connections++;
            stats_.connections_borrowed++;
            
            return conn;
        } else {
            stats_.connection_failures++;
        }
    }
    
    // Wait for available connection
    auto wait_timeout = std::chrono::milliseconds(config_.acquire_timeout_ms);
    auto conn = WaitForConnection(wait_timeout);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto wait_time = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    stats_.wait_time_total_ms += wait_time;
    stats_.wait_count++;
    
    if (!conn) {
        stats_.timeout_count++;
        stats_.borrow_failures++;
        throw std::runtime_error("Failed to acquire connection: timeout");
    }
    
    return conn;
}

// [SEQUENCE: MVP14-510] Wait for available connection
std::shared_ptr<PooledConnection> ConnectionPool::WaitForConnection(
    std::chrono::milliseconds timeout) {
    
    std::unique_lock<std::mutex> lock(pool_mutex_);
    
    if (pool_cv_.wait_for(lock, timeout, [this] {
        return !idle_connections_.empty() || !running_;
    })) {
        if (!running_) {
            return nullptr;
        }
        
        if (!idle_connections_.empty()) {
            auto conn = idle_connections_.front();
            idle_connections_.pop();
            
            conn->SetState(ConnectionState::IN_USE);
            stats_.idle_connections--;
            stats_.active_connections++;
            stats_.connections_borrowed++;
            
            return conn;
        }
    }
    
    return nullptr;
}

// [SEQUENCE: MVP14-511] Release connection back to pool
void ConnectionPool::Release(std::shared_ptr<PooledConnection> conn) {
    if (!conn) return;
    
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    // Validate on return if configured
    if (config_.test_on_return && !conn->Validate()) {
        stats_.validation_failures++;
        DestroyConnection(conn);
        return;
    }
    
    // Check if connection is still valid
    if (conn->IsExpired() || conn->GetState() == ConnectionState::BROKEN) {
        DestroyConnection(conn);
        return;
    }
    
    // Return to pool
    conn->SetState(ConnectionState::IDLE);
    idle_connections_.push(conn);
    
    stats_.active_connections--;
    stats_.idle_connections++;
    stats_.connections_returned++;
    
    // Notify waiters
    pool_cv_.notify_one();
}

// [SEQUENCE: MVP14-512] Create new connection
std::shared_ptr<PooledConnection> ConnectionPool::CreateConnection() {
    uint64_t id = next_connection_id_++;
    auto conn = std::make_shared<PooledConnection>(id, config_);
    
    stats_.connections_created++;
    
    spdlog::debug("[CONNECTION_POOL] Created connection {}", id);
    return conn;
}

// [SEQUENCE: MVP14-513] Destroy connection
void ConnectionPool::DestroyConnection(std::shared_ptr<PooledConnection> conn) {
    if (!conn) return;
    
    conn->Disconnect();
    
    // Remove from all_connections
    all_connections_.erase(
        std::remove(all_connections_.begin(), all_connections_.end(), conn),
        all_connections_.end()
    );
    
    stats_.total_connections--;
    stats_.connections_destroyed++;
    
    spdlog::debug("[CONNECTION_POOL] Destroyed connection {}", conn->GetId());
}

// [SEQUENCE: MVP14-514] Validation loop
void ConnectionPool::ValidationLoop() {
    while (running_) {
        std::this_thread::sleep_for(
            std::chrono::milliseconds(config_.validation_interval_ms));
        
        if (!running_) break;
        
        ValidateConnections();
    }
}

// [SEQUENCE: MVP14-515] Validate all connections
void ConnectionPool::ValidateConnections() {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    std::vector<std::shared_ptr<PooledConnection>> to_validate;
    std::queue<std::shared_ptr<PooledConnection>> new_idle_queue;
    
    // Check idle connections
    while (!idle_connections_.empty()) {
        auto conn = idle_connections_.front();
        idle_connections_.pop();
        
        if (config_.test_while_idle) {
            conn->SetState(ConnectionState::VALIDATING);
            to_validate.push_back(conn);
        } else {
            new_idle_queue.push(conn);
        }
    }
    
    idle_connections_ = new_idle_queue;
    
    // Validate connections
    for (auto& conn : to_validate) {
        if (conn->Validate()) {
            conn->SetState(ConnectionState::IDLE);
            idle_connections_.push(conn);
        } else {
            stats_.validation_failures++;
            DestroyConnection(conn);
            stats_.idle_connections--;
        }
    }
}

// [SEQUENCE: MVP14-516] Eviction loop
void ConnectionPool::EvictionLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::seconds(60));  // Check every minute
        
        if (!running_) break;
        
        EvictExpiredConnections();
    }
}

// [SEQUENCE: MVP14-517] Evict expired connections
void ConnectionPool::EvictExpiredConnections() {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    std::queue<std::shared_ptr<PooledConnection>> new_idle_queue;
    
    while (!idle_connections_.empty()) {
        auto conn = idle_connections_.front();
        idle_connections_.pop();
        
        if (conn->IsExpired() || 
            conn->IsIdle(std::chrono::milliseconds(config_.idle_timeout_ms))) {
            
            DestroyConnection(conn);
            stats_.idle_connections--;
            
            spdlog::debug("[CONNECTION_POOL] Evicted expired connection {}", 
                         conn->GetId());
        } else {
            new_idle_queue.push(conn);
        }
    }
    
    idle_connections_ = new_idle_queue;
    
    // Ensure minimum connections
    while (stats_.total_connections < config_.min_connections) {
        auto conn = CreateConnection();
        if (conn && conn->Connect()) {
            idle_connections_.push(conn);
            all_connections_.push_back(conn);
            stats_.total_connections++;
            stats_.idle_connections++;
        } else {
            break;
        }
    }
}

// [SEQUENCE: MVP14-518] Get pool health status
ConnectionPool::HealthStatus ConnectionPool::GetHealthStatus() const {
    std::lock_guard<std::mutex> lock(pool_mutex_);
    
    HealthStatus health;
    health.active_connections = stats_.active_connections;
    health.idle_connections = stats_.idle_connections;
    health.pool_utilization = stats_.GetPoolUtilization();
    health.avg_wait_time_ms = stats_.GetAvgWaitTimeMs();
    
    // Count broken connections
    for (const auto& conn : all_connections_) {
        if (conn->GetState() == ConnectionState::BROKEN) {
            health.broken_connections++;
        }
    }
    
    // Check for issues
    if (health.pool_utilization > 0.9) {
        health.issues.push_back("High pool utilization (>90%)");
        health.healthy = false;
    }
    
    if (health.broken_connections > 0) {
        health.issues.push_back("Broken connections detected");
        health.healthy = false;
    }
    
    if (health.avg_wait_time_ms > 1000) {
        health.issues.push_back("High average wait time (>1s)");
        health.healthy = false;
    }
    
    if (stats_.total_connections < config_.min_connections) {
        health.issues.push_back("Below minimum connection count");
        health.healthy = false;
    }
    
    return health;
}

// [SEQUENCE: MVP14-519] Connection pool manager implementation
ConnectionPoolManager::~ConnectionPoolManager() {
    ShutdownAll();
}

void ConnectionPoolManager::CreatePool(const std::string& pool_name,
                                     const ConnectionPoolConfig& config) {
    std::lock_guard<std::mutex> lock(manager_mutex_);
    
    if (pools_.find(pool_name) != pools_.end()) {
        spdlog::warn("[CONNECTION_POOL] Pool {} already exists", pool_name);
        return;
    }
    
    auto pool = std::make_shared<ConnectionPool>(config);
    if (pool->Initialize()) {
        pools_[pool_name] = pool;
        spdlog::info("[CONNECTION_POOL] Created pool: {}", pool_name);
    } else {
        spdlog::error("[CONNECTION_POOL] Failed to create pool: {}", pool_name);
    }
}

// [SEQUENCE: MVP14-520] Get pool by name
std::shared_ptr<ConnectionPool> ConnectionPoolManager::GetPool(
    const std::string& pool_name) {
    
    std::lock_guard<std::mutex> lock(manager_mutex_);
    
    auto it = pools_.find(pool_name);
    if (it != pools_.end()) {
        return it->second;
    }
    
    return nullptr;
}

// [SEQUENCE: MVP14-521] Shutdown all pools
void ConnectionPoolManager::ShutdownAll() {
    std::lock_guard<std::mutex> lock(manager_mutex_);
    
    spdlog::info("[CONNECTION_POOL] Shutting down all connection pools");
    
    for (auto& [name, pool] : pools_) {
        pool->Shutdown();
    }
    
    pools_.clear();
}

// [SEQUENCE: MVP14-522] Get global health status
ConnectionPoolManager::GlobalHealthStatus ConnectionPoolManager::GetGlobalHealth() const {
    std::lock_guard<std::mutex> lock(manager_mutex_);
    
    GlobalHealthStatus global;
    
    for (const auto& [name, pool] : pools_) {
        auto health = pool->GetHealthStatus();
        global.pool_health[name] = health;
        
        global.total_active_connections += health.active_connections;
        global.total_idle_connections += health.idle_connections;
        
        if (!health.healthy) {
            global.all_healthy = false;
        }
    }
    
    return global;
}

// [SEQUENCE: MVP14-523] Connection pool utilities
namespace ConnectionPoolUtils {

ConnectionPoolConfig CreateDefaultConfig(const std::string& host,
                                       uint16_t port,
                                       const std::string& database) {
    ConnectionPoolConfig config;
    config.host = host;
    config.port = port;
    config.database = database;
    
    // Default settings
    config.min_connections = 5;
    config.max_connections = 100;
    config.initial_connections = 10;
    
    return config;
}

ConnectionPoolConfig CreateReadHeavyConfig(const std::string& host,
                                         uint16_t port,
                                         const std::string& database) {
    auto config = CreateDefaultConfig(host, port, database);
    
    // More connections for read-heavy workloads
    config.min_connections = 20;
    config.max_connections = 200;
    config.initial_connections = 50;
    
    // Longer timeouts for read queries
    config.connection_timeout_ms = 10000;
    config.idle_timeout_ms = 1800000;  // 30 minutes
    
    return config;
}

ConnectionPoolConfig CreateWriteHeavyConfig(const std::string& host,
                                          uint16_t port,
                                          const std::string& database) {
    auto config = CreateDefaultConfig(host, port, database);
    
    // Fewer but fresher connections for writes
    config.min_connections = 10;
    config.max_connections = 50;
    config.initial_connections = 20;
    
    // Shorter timeouts for write queries
    config.idle_timeout_ms = 300000;     // 5 minutes
    config.max_lifetime_ms = 1800000;    // 30 minutes
    
    // More aggressive validation
    config.test_on_borrow = true;
    config.test_on_return = true;
    config.validation_interval_ms = 15000;  // 15 seconds
    
    return config;
}

} // namespace ConnectionPoolUtils

} // namespace mmorpg::database
