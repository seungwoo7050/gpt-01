#pragma once

#include <string>
#include <vector>
#include <queue>
#include <memory>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <unordered_map>
#include <list>
#include "core/ecs/types.h"

namespace mmorpg::database {

struct QueryResult {
    bool success{false};
    std::vector<std::vector<std::string>> rows;
    uint64_t rows_affected{0};
    std::string error_message;
};

// [SEQUENCE: 3160] Connection state
enum class ConnectionState {
    IDLE,           // 유휴 상태
    IN_USE,         // 사용 중
    VALIDATING,     // 검증 중
    BROKEN,         // 손상됨
    CLOSED          // 닫힘
};

// [SEQUENCE: 3161] Connection pool configuration
struct ConnectionPoolConfig {
    // Basic settings
    std::string host;
    uint16_t port{3306};
    std::string username;
    std::string password;
    std::string database;
    
    // Pool sizing
    uint32_t min_connections{5};        // 최소 연결 수
    uint32_t max_connections{100};      // 최대 연결 수
    uint32_t initial_connections{10};   // 초기 연결 수
    
    // Connection settings
    uint32_t connection_timeout_ms{5000};     // 연결 타임아웃
    uint32_t idle_timeout_ms{600000};         // 10분 유휴 타임아웃
    uint32_t max_lifetime_ms{3600000};        // 1시간 최대 수명
    
    // Validation settings
    bool test_on_borrow{true};                // 대여 시 검증
    bool test_on_return{false};               // 반환 시 검증
    bool test_while_idle{true};               // 유휴 시 검증
    uint32_t validation_interval_ms{30000};   // 30초 검증 주기
    std::string validation_query{"SELECT 1"};  // 검증 쿼리
    
    // Performance settings
    uint32_t acquire_timeout_ms{5000};         // 연결 획득 타임아웃
    uint32_t retry_attempts{3};                // 재시도 횟수
    uint32_t retry_delay_ms{100};              // 재시도 지연
    
    // Advanced settings
    bool enable_prepared_statements{true};     // Prepared statement 사용
    uint32_t prepared_stmt_cache_size{256};    // PS 캐시 크기
    bool enable_ssl{false};                    // SSL 사용
    std::string ssl_ca_path;                   // SSL CA 경로
};

// [SEQUENCE: 3162] Connection statistics
struct ConnectionStats {
    // Pool metrics
    std::atomic<uint32_t> total_connections{0};
    std::atomic<uint32_t> active_connections{0};
    std::atomic<uint32_t> idle_connections{0};
    
    // Usage metrics
    std::atomic<uint64_t> connections_created{0};
    std::atomic<uint64_t> connections_destroyed{0};
    std::atomic<uint64_t> connections_borrowed{0};
    std::atomic<uint64_t> connections_returned{0};
    
    // Performance metrics
    std::atomic<uint64_t> wait_time_total_ms{0};
    std::atomic<uint64_t> wait_count{0};
    std::atomic<uint64_t> timeout_count{0};
    
    // Error metrics
    std::atomic<uint64_t> connection_failures{0};
    std::atomic<uint64_t> validation_failures{0};
    std::atomic<uint64_t> borrow_failures{0};

    ConnectionStats() = default;

    ConnectionStats(const ConnectionStats& other)
        : total_connections(other.total_connections.load())
        , active_connections(other.active_connections.load())
        , idle_connections(other.idle_connections.load())
        , connections_created(other.connections_created.load())
        , connections_destroyed(other.connections_destroyed.load())
        , connections_borrowed(other.connections_borrowed.load())
        , connections_returned(other.connections_returned.load())
        , wait_time_total_ms(other.wait_time_total_ms.load())
        , wait_count(other.wait_count.load())
        , timeout_count(other.timeout_count.load())
        , connection_failures(other.connection_failures.load())
        , validation_failures(other.validation_failures.load())
        , borrow_failures(other.borrow_failures.load())
    {
    }

    ConnectionStats& operator=(const ConnectionStats& other) {
        if (this == &other) {
            return *this;
        }
        total_connections = other.total_connections.load();
        active_connections = other.active_connections.load();
        idle_connections = other.idle_connections.load();
        connections_created = other.connections_created.load();
        connections_destroyed = other.connections_destroyed.load();
        connections_borrowed = other.connections_borrowed.load();
        connections_returned = other.connections_returned.load();
        wait_time_total_ms = other.wait_time_total_ms.load();
        wait_count = other.wait_count.load();
        timeout_count = other.timeout_count.load();
        connection_failures = other.connection_failures.load();
        validation_failures = other.validation_failures.load();
        borrow_failures = other.borrow_failures.load();
        return *this;
    }
    
    double GetAvgWaitTimeMs() const {
        return wait_count > 0 ? 
            static_cast<double>(wait_time_total_ms) / wait_count : 0.0;
    }
    
    double GetPoolUtilization() const {
        uint32_t total = total_connections;
        return total > 0 ? 
            static_cast<double>(active_connections) / total : 0.0;
    }
};

// [SEQUENCE: 3163] Database connection wrapper
class PooledConnection {
public:
    PooledConnection(uint64_t id, const ConnectionPoolConfig& config);
    ~PooledConnection();
    
    // [SEQUENCE: 3164] Connection lifecycle
    bool Connect();
    void Disconnect();
    bool IsConnected() const;
    bool Validate();
    
    // [SEQUENCE: 3165] Query execution
    QueryResult Execute(const std::string& query, 
                       const std::vector<std::string>& params = {});
    
    // [SEQUENCE: 3166] Prepared statements
    bool Prepare(const std::string& stmt_id, const std::string& query);
    QueryResult ExecutePrepared(const std::string& stmt_id,
                               const std::vector<std::string>& params = {});
    
    // [SEQUENCE: 3167] Transaction support
    bool BeginTransaction();
    bool Commit();
    bool Rollback();
    bool IsInTransaction() const { return in_transaction_; }
    
    // [SEQUENCE: 3168] Connection info
    uint64_t GetId() const { return id_; }
    ConnectionState GetState() const { return state_; }
    std::chrono::system_clock::time_point GetCreatedTime() const { return created_time_; }
    std::chrono::system_clock::time_point GetLastUsedTime() const { return last_used_time_; }
    
    // [SEQUENCE: 3169] State management
    void SetState(ConnectionState state) { state_ = state; }
    void UpdateLastUsed() { last_used_time_ = std::chrono::system_clock::now(); }
    
    bool IsExpired() const;
    bool IsIdle(std::chrono::milliseconds threshold) const;
    
private:
    uint64_t id_;
    ConnectionPoolConfig config_;
    ConnectionState state_{ConnectionState::CLOSED};
    
    // MySQL connection handle (would be actual MySQL type)
    void* mysql_conn_{nullptr};
    
    // Timestamps
    std::chrono::system_clock::time_point created_time_;
    std::chrono::system_clock::time_point last_used_time_;
    
    // Transaction state
    bool in_transaction_{false};
    
    // Prepared statements cache
    std::unordered_map<std::string, void*> prepared_statements_;
    
    mutable std::mutex mutex_;
};

// [SEQUENCE: MVP1-37] Connection pool implementation
class ConnectionPool {
public:
    // [SEQUENCE: MVP1-38] `ConnectionPool::ConnectionPool()`: 설정에 따라 커넥션 풀을 초기화합니다.
    ConnectionPool(const ConnectionPoolConfig& config);
    ~ConnectionPool();
    
    // [SEQUENCE: 3171] Pool lifecycle
    bool Initialize();
    void Shutdown();
    
    // [SEQUENCE: 3172] Connection management
    // [SEQUENCE: MVP1-39] `ConnectionPool::Acquire()`: 풀에서 사용 가능한 연결을 가져옵니다.
    std::shared_ptr<PooledConnection> Acquire();
    // [SEQUENCE: MVP1-40] `ConnectionPool::Release()`: 사용이 끝난 연결을 풀에 반환합니다.
    void Release(std::shared_ptr<PooledConnection> conn);
    
    // [SEQUENCE: 3173] Pool operations
    void ValidateConnections();
    void EvictExpiredConnections();
    void ExpandPool(uint32_t additional_connections);
    void ShrinkPool(uint32_t target_size);
    
    // [SEQUENCE: 3174] Statistics
    ConnectionStats GetStats() const { return stats_; }
    uint32_t GetActiveCount() const { return stats_.active_connections; }
    uint32_t GetIdleCount() const { return stats_.idle_connections; }
    uint32_t GetTotalCount() const { return stats_.total_connections; }
    
    // [SEQUENCE: 3175] Health check
    struct HealthStatus {
        bool healthy{true};
        uint32_t active_connections{0};
        uint32_t idle_connections{0};
        uint32_t broken_connections{0};
        double pool_utilization{0.0};
        double avg_wait_time_ms{0.0};
        std::vector<std::string> issues;
    };
    
    HealthStatus GetHealthStatus() const;
    
private:
    ConnectionPoolConfig config_;
    ConnectionStats stats_;
    
    // Connection storage
    std::queue<std::shared_ptr<PooledConnection>> idle_connections_;
    std::vector<std::shared_ptr<PooledConnection>> all_connections_;
    
    // Synchronization
    mutable std::mutex pool_mutex_;
    std::condition_variable pool_cv_;
    
    // Background threads
    std::thread validation_thread_;
    std::thread eviction_thread_;
    std::atomic<bool> running_{false};
    
    // Connection ID generator
    std::atomic<uint64_t> next_connection_id_{1};
    
    // [SEQUENCE: 3176] Create new connection
    std::shared_ptr<PooledConnection> CreateConnection();
    
    // [SEQUENCE: 3177] Destroy connection
    void DestroyConnection(std::shared_ptr<PooledConnection> conn);
    
    // [SEQUENCE: 3178] Background tasks
    void ValidationLoop();
    void EvictionLoop();
    
    // [SEQUENCE: 3179] Wait for available connection
    std::shared_ptr<PooledConnection> WaitForConnection(
        std::chrono::milliseconds timeout);
};

// [SEQUENCE: 3180] Connection pool manager (manages multiple pools)
class ConnectionPoolManager {
public:
    static ConnectionPoolManager& Instance() {
        static ConnectionPoolManager instance;
        return instance;
    }
    
    // [SEQUENCE: 3181] Pool management
    void CreatePool(const std::string& pool_name, 
                   const ConnectionPoolConfig& config);
    
    std::shared_ptr<ConnectionPool> GetPool(const std::string& pool_name);
    
    void DestroyPool(const std::string& pool_name);
    
    // [SEQUENCE: 3182] Global operations
    void ShutdownAll();
    
    std::unordered_map<std::string, ConnectionStats> GetAllStats() const;
    
    // [SEQUENCE: 3183] Health monitoring
    struct GlobalHealthStatus {
        std::unordered_map<std::string, ConnectionPool::HealthStatus> pool_health;
        uint32_t total_active_connections{0};
        uint32_t total_idle_connections{0};
        bool all_healthy{true};
    };
    
    GlobalHealthStatus GetGlobalHealth() const;
    
private:
    ConnectionPoolManager() = default;
    ~ConnectionPoolManager();
    
    std::unordered_map<std::string, std::shared_ptr<ConnectionPool>> pools_;
    mutable std::mutex manager_mutex_;
};

// [SEQUENCE: 3184] Connection guard (RAII for automatic release)
class ConnectionGuard {
public:
    ConnectionGuard(std::shared_ptr<ConnectionPool> pool)
        : pool_(pool), conn_(pool->Acquire()) {}
    
    ~ConnectionGuard() {
        if (conn_ && pool_) {
            pool_->Release(conn_);
        }
    }
    
    // Disable copy
    ConnectionGuard(const ConnectionGuard&) = delete;
    ConnectionGuard& operator=(const ConnectionGuard&) = delete;
    
    // Enable move
    ConnectionGuard(ConnectionGuard&& other) noexcept
        : pool_(std::move(other.pool_)), conn_(std::move(other.conn_)) {
        other.conn_ = nullptr;
    }
    
    PooledConnection* operator->() { return conn_.get(); }
    PooledConnection& operator*() { return *conn_; }
    
    bool IsValid() const { return conn_ != nullptr; }
    
private:
    std::shared_ptr<ConnectionPool> pool_;
    std::shared_ptr<PooledConnection> conn_;
};

// [SEQUENCE: 3185] Connection pool monitoring
class ConnectionPoolMonitor {
public:
    // [SEQUENCE: 3186] Monitor configuration
    struct MonitorConfig {
        MonitorConfig() = default;
        std::chrono::seconds check_interval{60};      // 1분 체크 주기
        double high_utilization_threshold{0.8};       // 80% 이상 경고
        double low_utilization_threshold{0.1};        // 10% 이하 경고
        uint32_t max_wait_time_ms{1000};             // 1초 이상 대기 경고
        uint32_t max_connection_age_minutes{60};      // 1시간 이상 연결 경고
    };
    
    // [SEQUENCE: 3187] Start monitoring
    void StartMonitoring(const MonitorConfig& config);
    void StopMonitoring();
    
    // [SEQUENCE: 3188] Get alerts
    struct Alert {
        enum Type {
            HIGH_UTILIZATION,      // 높은 사용률
            LOW_UTILIZATION,       // 낮은 사용률
            LONG_WAIT_TIME,        // 긴 대기 시간
            CONNECTION_LEAK,       // 연결 누수 가능성
            VALIDATION_FAILURES,   // 검증 실패 다수
            POOL_EXHAUSTED        // 풀 고갈
        } type;
        
        std::string pool_name;
        std::string message;
        std::chrono::system_clock::time_point timestamp;
    };
    
    std::vector<Alert> GetRecentAlerts(
        std::chrono::minutes duration = std::chrono::minutes(60)) const;
    
private:
    void MonitoringLoop();
    void CheckPoolHealth(const std::string& pool_name, 
                        const ConnectionPool::HealthStatus& health);
    
    MonitorConfig config_;
    std::thread monitor_thread_;
    std::atomic<bool> monitoring_{false};
    
    std::vector<Alert> alerts_;
    mutable std::mutex alerts_mutex_;
};

// [SEQUENCE: 3189] Prepared statement cache
class PreparedStatementCache {
public:
    PreparedStatementCache(size_t max_size = 256) : max_size_(max_size) {}
    
    // [SEQUENCE: 3190] Cache operations
    bool Add(const std::string& query, void* stmt);
    void* Get(const std::string& query);
    void Remove(const std::string& query);
    void Clear();
    
    size_t Size() const { return cache_.size(); }
    
private:
    struct CacheEntry {
        void* statement;
        std::chrono::system_clock::time_point last_used;
        uint64_t use_count{0};
    };
    
    std::unordered_map<std::string, CacheEntry> cache_;
    std::list<std::string> lru_list_;
    size_t max_size_;
    
    mutable std::mutex mutex_;
    
    void EvictLRU();
};

// [SEQUENCE: 3191] Connection pool utilities
namespace ConnectionPoolUtils {
    // Create default configurations
    ConnectionPoolConfig CreateDefaultConfig(const std::string& host,
                                           uint16_t port,
                                           const std::string& database);
    
    // Create read-heavy configuration
    ConnectionPoolConfig CreateReadHeavyConfig(const std::string& host,
                                             uint16_t port,
                                             const std::string& database);
    
    // Create write-heavy configuration
    ConnectionPoolConfig CreateWriteHeavyConfig(const std::string& host,
                                              uint16_t port,
                                              const std::string& database);
    
    // Validate configuration
    bool ValidateConfig(const ConnectionPoolConfig& config,
                       std::vector<std::string>& errors);
}

} // namespace mmorpg::database