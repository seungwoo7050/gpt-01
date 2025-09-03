#include "read_replica_manager.h"
#include "../core/logger.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <numeric>
#include <regex>

namespace mmorpg::database {

// [SEQUENCE: 3017] Read replica implementation
ReadReplica::ReadReplica(const ReplicaConfig& config) 
    : config_(config) {
    
    connection_ = std::make_unique<DatabaseConnection>(
        config.host, config.port, config.connection_timeout_ms);
    
    stats_.last_health_check = std::chrono::system_clock::now();
}

ReadReplica::~ReadReplica() {
    Disconnect();
}

// [SEQUENCE: 3018] Connect to replica
bool ReadReplica::Connect() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (connected_) {
        return true;
    }
    
    try {
        if (connection_->Connect()) {
            connected_ = true;
            stats_.health_status = ReplicaHealth::HEALTHY;
            stats_.consecutive_failures = 0;
            
            spdlog::info("[READ_REPLICA] Connected to replica {} at {}:{}", 
                        config_.replica_id, config_.host, config_.port);
            return true;
        }
    } catch (const std::exception& e) {
        spdlog::error("[READ_REPLICA] Failed to connect to {}: {}", 
                     config_.replica_id, e.what());
    }
    
    stats_.failed_connections++;
    stats_.consecutive_failures++;
    return false;
}

// [SEQUENCE: 3019] Execute query on replica
QueryResult ReadReplica::ExecuteQuery(const std::string& query,
                                    const std::vector<std::string>& params) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!connected_) {
        throw std::runtime_error("Replica not connected");
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    
    try {
        auto result = connection_->ExecuteQuery(query, params);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double, std::milli>(end - start).count();
        
        UpdateStats(result, duration);
        
        return result;
    } catch (const std::exception& e) {
        stats_.queries_failed++;
        stats_.consecutive_failures++;
        
        if (stats_.consecutive_failures > 3) {
            stats_.health_status = ReplicaHealth::FAILED;
            connected_ = false;
        }
        
        throw;
    }
}

// [SEQUENCE: 3020] Health check implementation
bool ReadReplica::PerformHealthCheck() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        // Check connection
        if (!connected_ && !Connect()) {
            stats_.health_status = ReplicaHealth::UNREACHABLE;
            return false;
        }
        
        // Check replication lag
        uint32_t lag = GetReplicationLag();
        stats_.replication_lag_ms = lag;
        
        // Update health status based on lag
        if (lag > config_.max_allowed_lag_ms) {
            stats_.health_status = ReplicaHealth::LAGGING;
        } else if (lag > config_.lag_warning_threshold_ms) {
            stats_.health_status = ReplicaHealth::DEGRADED;
        } else {
            stats_.health_status = ReplicaHealth::HEALTHY;
        }
        
        // Reset failure counter on successful check
        stats_.consecutive_failures = 0;
        stats_.last_health_check = std::chrono::system_clock::now();
        
        return true;
    } catch (const std::exception& e) {
        spdlog::error("[READ_REPLICA] Health check failed for {}: {}", 
                     config_.replica_id, e.what());
        
        stats_.consecutive_failures++;
        if (stats_.consecutive_failures > 5) {
            stats_.health_status = ReplicaHealth::FAILED;
            connected_ = false;
        }
        
        return false;
    }
}

// [SEQUENCE: 3021] Get replication lag
uint32_t ReadReplica::GetReplicationLag() {
    // In real implementation, query replica status
    // For now, simulate with random lag
    return std::rand() % 500;  // 0-500ms lag
}

// [SEQUENCE: 3022] Update statistics
void ReadReplica::UpdateStats(const QueryResult& result, double query_time_ms) {
    stats_.queries_executed++;
    stats_.total_connections = connection_->GetActiveConnections();
    
    // Update query time tracking
    recent_query_times_.push_back(query_time_ms);
    if (recent_query_times_.size() > MAX_QUERY_TIME_SAMPLES) {
        recent_query_times_.erase(recent_query_times_.begin());
    }
    
    // Update average
    double sum = std::accumulate(recent_query_times_.begin(), 
                               recent_query_times_.end(), 0.0);
    stats_.avg_query_time_ms = sum / recent_query_times_.size();
    
    // Update percentiles
    UpdateQueryTimePercentiles();
}

// [SEQUENCE: 3023] Calculate percentiles
void ReadReplica::UpdateQueryTimePercentiles() {
    if (recent_query_times_.empty()) return;
    
    std::vector<double> sorted = recent_query_times_;
    std::sort(sorted.begin(), sorted.end());
    
    size_t p95_idx = sorted.size() * 0.95;
    size_t p99_idx = sorted.size() * 0.99;
    
    stats_.p95_query_time_ms = sorted[p95_idx];
    stats_.p99_query_time_ms = sorted[p99_idx];
}

// [SEQUENCE: 3024] Calculate load score
double ReadReplica::CalculateLoadScore() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Factor in multiple metrics
    double connection_factor = static_cast<double>(stats_.active_connections) / 
                             config_.max_connections;
    double lag_factor = static_cast<double>(stats_.replication_lag_ms) / 
                       config_.max_allowed_lag_ms;
    double query_time_factor = stats_.avg_query_time_ms / 100.0;  // Normalize to 100ms
    double health_factor = (stats_.health_status == ReplicaHealth::HEALTHY) ? 1.0 : 10.0;
    
    // Weighted score (lower is better)
    return (connection_factor * 0.4 + 
            lag_factor * 0.3 + 
            query_time_factor * 0.2 + 
            health_factor * 0.1);
}

// [SEQUENCE: 3025] Add replica to pool
bool ReadReplicaPool::AddReplica(const ReplicaConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (replicas_.find(config.replica_id) != replicas_.end()) {
        spdlog::warn("[REPLICA_POOL] Replica {} already exists", config.replica_id);
        return false;
    }
    
    auto replica = std::make_unique<ReadReplica>(config);
    if (replica->Connect()) {
        replicas_[config.replica_id] = std::move(replica);
        spdlog::info("[REPLICA_POOL] Added replica {} to pool", config.replica_id);
        return true;
    }
    
    return false;
}

// [SEQUENCE: 3026] Get replica based on strategy
ReadReplica* ReadReplicaPool::GetReplica(const std::string& query_hint) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    return SelectReplica(query_hint);
}

// [SEQUENCE: 3027] Select replica implementation
ReadReplica* ReadReplicaPool::SelectReplica(const std::string& query_hint) {
    // First check for dedicated replicas if hint provided
    if (!query_hint.empty()) {
        for (auto& [id, replica] : replicas_) {
            if (replica->GetConfig().type == ReplicaType::DEDICATED) {
                for (const auto& pattern : replica->GetConfig().dedicated_patterns) {
                    if (query_hint.find(pattern) != std::string::npos) {
                        return replica.get();
                    }
                }
            }
        }
    }
    
    // Use configured strategy
    switch (strategy_) {
        case LoadBalancingStrategy::ROUND_ROBIN:
            return SelectRoundRobin();
        case LoadBalancingStrategy::LEAST_CONN:
            return SelectLeastConnections();
        case LoadBalancingStrategy::WEIGHTED:
            return SelectWeighted();
        case LoadBalancingStrategy::LATENCY_BASED:
            return SelectByLatency();
        case LoadBalancingStrategy::RANDOM:
            return SelectRandom();
        case LoadBalancingStrategy::CONSISTENT_HASH:
            return SelectConsistentHash(query_hint);
        default:
            return SelectRoundRobin();
    }
}

// [SEQUENCE: 3028] Round robin selection
ReadReplica* ReadReplicaPool::SelectRoundRobin() {
    auto healthy = GetHealthyReplicasUnsafe();
    if (healthy.empty()) return nullptr;
    
    uint64_t index = round_robin_counter_++ % healthy.size();
    return healthy[index];
}

// [SEQUENCE: 3029] Least connections selection
ReadReplica* ReadReplicaPool::SelectLeastConnections() {
    auto healthy = GetHealthyReplicasUnsafe();
    if (healthy.empty()) return nullptr;
    
    return *std::min_element(healthy.begin(), healthy.end(),
        [](const ReadReplica* a, const ReadReplica* b) {
            return a->GetStats().active_connections < b->GetStats().active_connections;
        });
}

// [SEQUENCE: 3030] Weighted selection
ReadReplica* ReadReplicaPool::SelectWeighted() {
    auto healthy = GetHealthyReplicasUnsafe();
    if (healthy.empty()) return nullptr;
    
    // Calculate total weight
    uint32_t total_weight = 0;
    for (const auto* replica : healthy) {
        total_weight += replica->GetConfig().weight;
    }
    
    // Random selection based on weight
    std::uniform_int_distribution<uint32_t> dist(0, total_weight - 1);
    uint32_t random_weight = dist(random_gen_);
    
    uint32_t cumulative = 0;
    for (auto* replica : healthy) {
        cumulative += replica->GetConfig().weight;
        if (random_weight < cumulative) {
            return replica;
        }
    }
    
    return healthy.back();
}

// [SEQUENCE: 3031] Latency-based selection
ReadReplica* ReadReplicaPool::SelectByLatency() {
    auto healthy = GetHealthyReplicasUnsafe();
    if (healthy.empty()) return nullptr;
    
    return *std::min_element(healthy.begin(), healthy.end(),
        [](const ReadReplica* a, const ReadReplica* b) {
            return a->GetStats().avg_query_time_ms < b->GetStats().avg_query_time_ms;
        });
}

// [SEQUENCE: 3032] Get healthy replicas
std::vector<ReadReplica*> ReadReplicaPool::GetHealthyReplicasUnsafe() {
    std::vector<ReadReplica*> healthy;
    
    for (auto& [id, replica] : replicas_) {
        if (replica->GetHealthStatus() == ReplicaHealth::HEALTHY ||
            replica->GetHealthStatus() == ReplicaHealth::DEGRADED) {
            healthy.push_back(replica.get());
        }
    }
    
    return healthy;
}

// [SEQUENCE: 3033] Perform health checks
void ReadReplicaPool::PerformHealthChecks() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (auto& [id, replica] : replicas_) {
        replica->PerformHealthCheck();
    }
    
    UpdatePoolMetrics();
}

// [SEQUENCE: 3034] Get pool statistics
ReadReplicaPool::PoolStats ReadReplicaPool::GetPoolStats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    PoolStats stats;
    stats.total_replicas = replicas_.size();
    
    double total_lag = 0;
    
    for (const auto& [id, replica] : replicas_) {
        const auto& replica_stats = replica->GetStats();
        
        switch (replica_stats.health_status) {
            case ReplicaHealth::HEALTHY:
                stats.healthy_replicas++;
                break;
            case ReplicaHealth::DEGRADED:
            case ReplicaHealth::LAGGING:
                stats.degraded_replicas++;
                break;
            default:
                stats.failed_replicas++;
                break;
        }
        
        stats.total_queries += replica_stats.queries_executed;
        stats.failed_queries += replica_stats.queries_failed;
        total_lag += replica_stats.replication_lag_ms;
        
        stats.replica_stats[id] = replica_stats;
    }
    
    if (stats.total_replicas > 0) {
        stats.avg_replication_lag_ms = total_lag / stats.total_replicas;
    }
    
    return stats;
}

// [SEQUENCE: 3035] Query type determination
QueryRouter::QueryType QueryRouter::DetermineQueryType(const std::string& query) {
    // Simple regex-based classification
    std::string upper_query = query;
    std::transform(upper_query.begin(), upper_query.end(), upper_query.begin(), ::toupper);
    
    if (upper_query.find("SELECT") == 0) {
        return QueryType::READ;
    } else if (upper_query.find("INSERT") == 0 || 
               upper_query.find("UPDATE") == 0 || 
               upper_query.find("DELETE") == 0) {
        return QueryType::WRITE;
    } else if (upper_query.find("BEGIN") == 0 || 
               upper_query.find("COMMIT") == 0 || 
               upper_query.find("ROLLBACK") == 0) {
        return QueryType::TRANSACTION;
    } else if (upper_query.find("CREATE") == 0 || 
               upper_query.find("ALTER") == 0 || 
               upper_query.find("DROP") == 0) {
        return QueryType::DDL;
    }
    
    return QueryType::UNKNOWN;
}

// [SEQUENCE: 3036] Routing decision
bool QueryRouter::ShouldRouteToPrimary(QueryType type, ConsistencyLevel consistency) {
    // Always route writes and DDL to primary
    if (type == QueryType::WRITE || 
        type == QueryType::DDL || 
        type == QueryType::TRANSACTION) {
        return true;
    }
    
    // Route reads based on consistency requirements
    if (type == QueryType::READ) {
        return consistency == ConsistencyLevel::STRONG ||
               consistency == ConsistencyLevel::READ_YOUR_WRITES;
    }
    
    // Unknown queries go to primary for safety
    return true;
}

// [SEQUENCE: 3037] Parse query hints
QueryRouter::QueryHints QueryRouter::ParseQueryHints(const std::string& query) {
    QueryHints hints;
    
    // Look for hints in comments: /* hint:value */
    std::regex hint_regex(R"(/\*\s*(\w+):(\w+)\s*\*/)");
    std::smatch match;
    
    std::string::const_iterator search_start(query.cbegin());
    while (std::regex_search(search_start, query.cend(), match, hint_regex)) {
        std::string hint_name = match[1];
        std::string hint_value = match[2];
        
        if (hint_name == "force_master") {
            hints.force_master = (hint_value == "true");
        } else if (hint_name == "replica") {
            hints.preferred_replica = hint_value;
        } else if (hint_name == "max_staleness") {
            hints.max_staleness_ms = std::stoi(hint_value);
        } else if (hint_name == "region") {
            hints.region_preference = hint_value;
        }
        
        search_start = match.suffix().first;
    }
    
    return hints;
}

// [SEQUENCE: 3038] Manager initialization
void ReadReplicaManager::Initialize(const std::vector<ReplicaConfig>& configs,
                                  LoadBalancingStrategy strategy) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Create default pool
    CreatePool("default", strategy);
    
    // Add all replicas to default pool
    for (const auto& config : configs) {
        AddReplicaToPool("default", config);
    }
    
    spdlog::info("[REPLICA_MANAGER] Initialized with {} replicas", configs.size());
}

// [SEQUENCE: 3039] Execute query with routing
QueryResult ReadReplicaManager::ExecuteQuery(const std::string& query,
                                           const std::vector<std::string>& params,
                                           QueryRouter::ConsistencyLevel consistency) {
    // Determine query type
    auto query_type = QueryRouter::DetermineQueryType(query);
    
    // Parse hints
    auto hints = QueryRouter::ParseQueryHints(query);
    
    // Routing decision
    bool use_primary = hints.force_master || 
                      QueryRouter::ShouldRouteToPrimary(query_type, consistency);
    
    UpdateRoutingStats(query_type, use_primary);
    
    if (use_primary) {
        // Route to primary
        return primary_connection_->ExecuteQuery(query, params);
    } else {
        // Route to replica
        auto* pool = GetPool("default");
        if (!pool) {
            throw std::runtime_error("No replica pool available");
        }
        
        auto* replica = pool->GetReplica(query);
        if (!replica) {
            // Fallback to primary if no healthy replicas
            spdlog::warn("[REPLICA_MANAGER] No healthy replicas, falling back to primary");
            return primary_connection_->ExecuteQuery(query, params);
        }
        
        try {
            return replica->ExecuteQuery(query, params);
        } catch (const std::exception& e) {
            // Fallback to primary on replica failure
            spdlog::warn("[REPLICA_MANAGER] Replica query failed, falling back to primary: {}", 
                        e.what());
            return primary_connection_->ExecuteQuery(query, params);
        }
    }
}

// [SEQUENCE: 3040] Start health monitoring
void ReadReplicaManager::StartHealthMonitoring(std::chrono::seconds interval) {
    if (monitoring_active_) return;
    
    monitoring_active_ = true;
    monitoring_thread_ = std::thread([this, interval] {
        MonitoringLoop(interval);
    });
    
    spdlog::info("[REPLICA_MANAGER] Started health monitoring with {}s interval", 
                interval.count());
}

// [SEQUENCE: 3041] Monitoring loop
void ReadReplicaManager::MonitoringLoop(std::chrono::seconds interval) {
    while (monitoring_active_) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            for (auto& [name, pool] : pools_) {
                pool->PerformHealthChecks();
            }
        }
        
        std::this_thread::sleep_for(interval);
    }
}

// [SEQUENCE: 3042] Configuration helpers
ReplicaConfig CreateSyncReplica(const std::string& host, uint16_t port,
                               const std::string& region) {
    ReplicaConfig config;
    config.replica_id = host + ":" + std::to_string(port);
    config.host = host;
    config.port = port;
    config.type = ReplicaType::SYNC;
    config.region = region;
    config.max_allowed_lag_ms = 10;  // Very low lag for sync replicas
    return config;
}

ReplicaConfig CreateAsyncReplica(const std::string& host, uint16_t port,
                                const std::string& region,
                                uint32_t max_lag_ms) {
    ReplicaConfig config;
    config.replica_id = host + ":" + std::to_string(port);
    config.host = host;
    config.port = port;
    config.type = ReplicaType::ASYNC;
    config.region = region;
    config.max_allowed_lag_ms = max_lag_ms;
    return config;
}

ReplicaConfig CreateAnalyticsReplica(const std::string& host, uint16_t port,
                                   uint32_t delay_minutes) {
    ReplicaConfig config;
    config.replica_id = host + ":" + std::to_string(port) + "_analytics";
    config.host = host;
    config.port = port;
    config.type = ReplicaType::DELAYED;
    config.max_allowed_lag_ms = delay_minutes * 60 * 1000;  // Convert to ms
    config.dedicated_patterns = {"analytics", "report", "aggregate"};
    return config;
}

} // namespace mmorpg::database