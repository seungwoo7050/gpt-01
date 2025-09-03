#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <atomic>
#include <mutex>
#include <random>
#include "../core/types.h"
#include "database_connection.h"

namespace mmorpg::database {

// [SEQUENCE: 2978] Read replica types
enum class ReplicaType {
    SYNC,           // 동기 복제 (강한 일관성)
    ASYNC,          // 비동기 복제 (약한 일관성)
    DELAYED,        // 지연 복제 (분석용)
    REGIONAL,       // 지역별 복제
    DEDICATED       // 전용 복제 (특정 쿼리용)
};

// [SEQUENCE: 2979] Replica health status
enum class ReplicaHealth {
    HEALTHY,        // 정상 작동
    LAGGING,        // 복제 지연
    DEGRADED,       // 성능 저하
    UNREACHABLE,    // 연결 불가
    FAILED          // 실패
};

// [SEQUENCE: 2980] Load balancing strategy
enum class LoadBalancingStrategy {
    ROUND_ROBIN,    // 순차 분배
    LEAST_CONN,     // 최소 연결
    WEIGHTED,       // 가중치 기반
    LATENCY_BASED,  // 지연시간 기반
    RANDOM,         // 무작위
    CONSISTENT_HASH // 일관된 해시
};

// [SEQUENCE: 2981] Read replica configuration
struct ReplicaConfig {
    std::string replica_id;
    std::string host;
    uint16_t port;
    
    // Replica properties
    ReplicaType type{ReplicaType::ASYNC};
    uint32_t weight{100};  // For weighted load balancing
    
    // Regional settings
    std::string region;
    std::string availability_zone;
    
    // Performance settings
    uint32_t max_connections{100};
    uint32_t connection_timeout_ms{5000};
    uint32_t query_timeout_ms{30000};
    
    // Lag thresholds
    uint32_t max_allowed_lag_ms{1000};
    uint32_t lag_warning_threshold_ms{500};
    
    // Dedicated query patterns (if DEDICATED type)
    std::vector<std::string> dedicated_patterns;
};

// [SEQUENCE: 2982] Replica statistics
struct ReplicaStats {
    // Connection metrics
    uint32_t active_connections{0};
    uint32_t total_connections{0};
    uint32_t failed_connections{0};
    
    // Query metrics
    uint64_t queries_executed{0};
    uint64_t queries_failed{0};
    double avg_query_time_ms{0};
    double p95_query_time_ms{0};
    double p99_query_time_ms{0};
    
    // Replication metrics
    uint32_t replication_lag_ms{0};
    uint64_t bytes_behind_master{0};
    std::chrono::system_clock::time_point last_sync_time;
    
    // Health metrics
    ReplicaHealth health_status{ReplicaHealth::HEALTHY};
    double cpu_usage_percent{0};
    double memory_usage_percent{0};
    uint32_t consecutive_failures{0};
    
    // Load balancing metrics
    double current_load_score{0};
    std::chrono::system_clock::time_point last_health_check;
};

// [SEQUENCE: 2983] Read replica instance
class ReadReplica {
public:
    ReadReplica(const ReplicaConfig& config);
    ~ReadReplica();
    
    // [SEQUENCE: 2984] Connection management
    bool Connect();
    void Disconnect();
    bool IsConnected() const;
    
    // [SEQUENCE: 2985] Execute read-only query
    QueryResult ExecuteQuery(const std::string& query,
                           const std::vector<std::string>& params = {});
    
    // [SEQUENCE: 2986] Health check
    bool PerformHealthCheck();
    ReplicaHealth GetHealthStatus() const { return stats_.health_status; }
    
    // [SEQUENCE: 2987] Get replication lag
    uint32_t GetReplicationLag();
    
    // [SEQUENCE: 2988] Statistics
    const ReplicaStats& GetStats() const { return stats_; }
    void UpdateStats(const QueryResult& result, double query_time_ms);
    
    // [SEQUENCE: 2989] Load scoring for balancing
    double CalculateLoadScore() const;
    
    // Configuration
    const ReplicaConfig& GetConfig() const { return config_; }
    const std::string& GetId() const { return config_.replica_id; }
    
private:
    ReplicaConfig config_;
    ReplicaStats stats_;
    std::unique_ptr<DatabaseConnection> connection_;
    
    mutable std::mutex mutex_;
    std::atomic<bool> connected_{false};
    
    // Query time tracking for percentiles
    std::vector<double> recent_query_times_;
    static constexpr size_t MAX_QUERY_TIME_SAMPLES = 1000;
    
    void UpdateQueryTimePercentiles();
    bool CheckReplicationStatus();
};

// [SEQUENCE: 2990] Read replica pool
class ReadReplicaPool {
public:
    // [SEQUENCE: 2991] Initialize pool
    ReadReplicaPool(LoadBalancingStrategy strategy = LoadBalancingStrategy::LEAST_CONN)
        : strategy_(strategy) {}
    
    // [SEQUENCE: 2992] Add replica to pool
    bool AddReplica(const ReplicaConfig& config);
    
    // [SEQUENCE: 2993] Remove replica from pool
    bool RemoveReplica(const std::string& replica_id);
    
    // [SEQUENCE: 2994] Get replica for query
    ReadReplica* GetReplica(const std::string& query_hint = "");
    
    // [SEQUENCE: 2995] Get replica by criteria
    ReadReplica* GetReplicaByCriteria(
        std::function<bool(const ReadReplica*)> criteria);
    
    // [SEQUENCE: 2996] Get all healthy replicas
    std::vector<ReadReplica*> GetHealthyReplicas();
    
    // [SEQUENCE: 2997] Health check all replicas
    void PerformHealthChecks();
    
    // [SEQUENCE: 2998] Get pool statistics
    struct PoolStats {
        uint32_t total_replicas{0};
        uint32_t healthy_replicas{0};
        uint32_t degraded_replicas{0};
        uint32_t failed_replicas{0};
        
        uint64_t total_queries{0};
        uint64_t failed_queries{0};
        double avg_replication_lag_ms{0};
        
        std::unordered_map<std::string, ReplicaStats> replica_stats;
    };
    
    PoolStats GetPoolStats() const;
    
    // [SEQUENCE: 2999] Rebalance connections
    void RebalanceConnections();
    
private:
    std::unordered_map<std::string, std::unique_ptr<ReadReplica>> replicas_;
    LoadBalancingStrategy strategy_;
    
    mutable std::mutex mutex_;
    std::atomic<uint64_t> round_robin_counter_{0};
    std::mt19937 random_gen_{std::random_device{}()};
    
    // [SEQUENCE: 3000] Select replica based on strategy
    ReadReplica* SelectReplica(const std::string& query_hint);
    
    ReadReplica* SelectRoundRobin();
    ReadReplica* SelectLeastConnections();
    ReadReplica* SelectWeighted();
    ReadReplica* SelectByLatency();
    ReadReplica* SelectRandom();
    ReadReplica* SelectConsistentHash(const std::string& key);
    
    // Helper methods
    std::vector<ReadReplica*> GetHealthyReplicasUnsafe();
    void UpdatePoolMetrics();
};

// [SEQUENCE: 3001] Query router for read/write splitting
class QueryRouter {
public:
    QueryRouter() = default;
    
    // [SEQUENCE: 3002] Route query to appropriate server
    enum class QueryType {
        READ,           // SELECT queries
        WRITE,          // INSERT/UPDATE/DELETE
        TRANSACTION,    // BEGIN/COMMIT/ROLLBACK
        DDL,            // CREATE/ALTER/DROP
        UNKNOWN
    };
    
    static QueryType DetermineQueryType(const std::string& query);
    
    // [SEQUENCE: 3003] Route based on consistency requirements
    enum class ConsistencyLevel {
        STRONG,         // Must read from master
        BOUNDED_STALENESS,  // Max lag allowed
        EVENTUAL,       // Any replica OK
        READ_YOUR_WRITES    // Session consistency
    };
    
    static bool ShouldRouteToPrimary(QueryType type, 
                                    ConsistencyLevel consistency);
    
    // [SEQUENCE: 3004] Parse query hints
    struct QueryHints {
        bool force_master{false};
        std::string preferred_replica;
        uint32_t max_staleness_ms{0};
        std::string region_preference;
    };
    
    static QueryHints ParseQueryHints(const std::string& query);
};

// [SEQUENCE: 3005] Read replica manager (singleton)
class ReadReplicaManager {
public:
    // [SEQUENCE: 3006] Get singleton instance
    static ReadReplicaManager& Instance() {
        static ReadReplicaManager instance;
        return instance;
    }
    
    // [SEQUENCE: 3007] Initialize with configuration
    void Initialize(const std::vector<ReplicaConfig>& configs,
                   LoadBalancingStrategy strategy = LoadBalancingStrategy::LEAST_CONN);
    
    // [SEQUENCE: 3008] Execute query with automatic routing
    QueryResult ExecuteQuery(const std::string& query,
                           const std::vector<std::string>& params = {},
                           QueryRouter::ConsistencyLevel consistency = 
                               QueryRouter::ConsistencyLevel::EVENTUAL);
    
    // [SEQUENCE: 3009] Get specific pool
    ReadReplicaPool* GetPool(const std::string& pool_name = "default");
    
    // [SEQUENCE: 3010] Create named pool
    void CreatePool(const std::string& pool_name,
                   LoadBalancingStrategy strategy = LoadBalancingStrategy::LEAST_CONN);
    
    // [SEQUENCE: 3011] Add replica to pool
    bool AddReplicaToPool(const std::string& pool_name,
                         const ReplicaConfig& config);
    
    // [SEQUENCE: 3012] Start health monitoring
    void StartHealthMonitoring(std::chrono::seconds interval = std::chrono::seconds(30));
    
    // [SEQUENCE: 3013] Stop health monitoring
    void StopHealthMonitoring();
    
    // [SEQUENCE: 3014] Get manager statistics
    struct ManagerStats {
        std::unordered_map<std::string, ReadReplicaPool::PoolStats> pool_stats;
        uint64_t total_queries_routed{0};
        uint64_t queries_to_primary{0};
        uint64_t queries_to_replicas{0};
        
        // Routing decisions
        uint64_t strong_consistency_reads{0};
        uint64_t eventual_consistency_reads{0};
        uint64_t write_operations{0};
    };
    
    ManagerStats GetStats() const;
    
private:
    ReadReplicaManager() = default;
    ~ReadReplicaManager();
    
    // Prevent copying
    ReadReplicaManager(const ReadReplicaManager&) = delete;
    ReadReplicaManager& operator=(const ReadReplicaManager&) = delete;
    
    std::unordered_map<std::string, std::unique_ptr<ReadReplicaPool>> pools_;
    std::unique_ptr<DatabaseConnection> primary_connection_;
    
    mutable std::mutex mutex_;
    std::atomic<bool> monitoring_active_{false};
    std::thread monitoring_thread_;
    
    // Statistics
    mutable ManagerStats stats_;
    
    void MonitoringLoop(std::chrono::seconds interval);
    void UpdateRoutingStats(QueryRouter::QueryType type, bool to_primary);
};

// [SEQUENCE: 3015] Utility functions
std::string GetReplicaHealthString(ReplicaHealth health);
std::string GetLoadBalancingStrategyString(LoadBalancingStrategy strategy);

// [SEQUENCE: 3016] Configuration helpers
ReplicaConfig CreateSyncReplica(const std::string& host, uint16_t port,
                               const std::string& region = "");

ReplicaConfig CreateAsyncReplica(const std::string& host, uint16_t port,
                                const std::string& region = "",
                                uint32_t max_lag_ms = 1000);

ReplicaConfig CreateAnalyticsReplica(const std::string& host, uint16_t port,
                                   uint32_t delay_minutes = 60);

} // namespace mmorpg::database