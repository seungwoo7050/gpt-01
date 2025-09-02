#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <atomic>
#include <thread>
#include <unordered_map>
#include "../core/types.h"
#include "../monitoring/metrics_collector.h"

namespace mmorpg::database {

// [SEQUENCE: MVP6-11] A comprehensive database monitoring system.
struct DatabaseMonitorConfig {
    std::chrono::seconds health_check_interval{30};
    std::chrono::seconds stats_collection_interval{60};
    std::chrono::seconds slow_query_check_interval{10};
    uint32_t slow_query_threshold_ms{100};
    uint32_t connection_timeout_ms{5000};
    double high_cpu_threshold{80.0};
    double high_memory_threshold{85.0};
    uint64_t max_connections_threshold{1000};
    bool enable_alerts{true};
    uint32_t alert_cooldown_minutes{15};
    std::vector<std::string> alert_endpoints;
};

// [SEQUENCE: MVP7-205] Database health status
struct DatabaseHealth {
    enum class Status { HEALTHY, WARNING, CRITICAL, OFFLINE };
    Status overall_status{Status::HEALTHY};
    std::chrono::system_clock::time_point last_check;
    bool is_connected{false};
    uint32_t active_connections{0};
    uint32_t max_connections{0};
    double connection_usage_percent{0.0};
    double queries_per_second{0.0};
    double avg_query_time_ms{0.0};
    uint64_t slow_queries_count{0};
    double cpu_usage_percent{0.0};
    double memory_usage_mb{0.0};
    double disk_usage_gb{0.0};
    double disk_io_mbps{0.0};
    bool replication_running{true};
    uint64_t replication_lag_seconds{0};
    std::vector<std::string> issues;
};

// [SEQUENCE: MVP7-206] Query performance metrics
struct QueryMetrics {
    std::string query_digest;
    uint64_t execution_count{0};
    double total_time_ms{0.0};
    double avg_time_ms{0.0};
    double min_time_ms{0.0};
    double max_time_ms{0.0};
    double p95_time_ms{0.0};
    double p99_time_ms{0.0};
    uint64_t rows_examined_total{0};
    uint64_t rows_sent_total{0};
    uint64_t tmp_tables_created{0};
    double lock_time_ms{0.0};
    uint64_t lock_wait_count{0};
};

// [SEQUENCE: MVP7-207] Table statistics
struct TableStats {
    std::string table_name;
    std::string engine;
    uint64_t row_count{0};
    uint64_t data_size_bytes{0};
    uint64_t index_size_bytes{0};
    double fragmentation_percent{0.0};
    uint64_t read_count{0};
    uint64_t write_count{0};
    uint64_t update_count{0};
    uint64_t delete_count{0};
    double avg_read_time_ms{0.0};
    double avg_write_time_ms{0.0};
    std::unordered_map<std::string, uint64_t> index_usage_count;
    std::vector<std::string> unused_indexes;
};

// [SEQUENCE: MVP7-208] Database monitor interface
class DatabaseMonitor {
public:
    explicit DatabaseMonitor(const DatabaseMonitorConfig& config);
    ~DatabaseMonitor();
    
    // [SEQUENCE: MVP7-209] Lifecycle management
    bool Start();
    void Stop();
    bool IsRunning() const { return running_; }
    
    // [SEQUENCE: MVP7-210] Health monitoring
    DatabaseHealth GetHealthStatus() const;
    void CheckHealth();
    
    // [SEQUENCE: MVP7-211] Query monitoring
    void RecordQueryExecution(const std::string& query, double execution_time_ms, uint64_t rows_examined, uint64_t rows_sent);
    std::vector<QueryMetrics> GetSlowQueries(uint32_t limit = 10) const;
    std::vector<QueryMetrics> GetTopQueries(uint32_t limit = 10) const;
    
    // [SEQUENCE: MVP7-212] Table monitoring
    void UpdateTableStats(const std::string& table_name);
    TableStats GetTableStats(const std::string& table_name) const;
    std::vector<TableStats> GetAllTableStats() const;
    
    // [SEQUENCE: MVP7-213] Performance analysis
    struct PerformanceReport {
        std::chrono::system_clock::time_point timestamp;
        double avg_qps{0.0};
        double peak_qps{0.0};
        double avg_response_time_ms{0.0};
        double avg_cpu_usage{0.0};
        double peak_cpu_usage{0.0};
        double avg_memory_usage_mb{0.0};
        double peak_memory_usage_mb{0.0};
        std::vector<std::string> bottlenecks;
        std::vector<std::string> recommendations;
    };
    PerformanceReport GeneratePerformanceReport(std::chrono::hours duration = std::chrono::hours(24)) const;
    
    // [SEQUENCE: MVP7-214] Alert management
    struct Alert {
        enum class Severity { INFO, WARNING, ERROR, CRITICAL };
        Severity severity;
        std::string category;
        std::string message;
        std::chrono::system_clock::time_point timestamp;
        std::unordered_map<std::string, std::string> tags;
    };
    void TriggerAlert(const Alert& alert);
    std::vector<Alert> GetRecentAlerts(std::chrono::minutes duration) const;
    
    // [SEQUENCE: MVP7-215] Metrics export
    void ExportMetrics(monitoring::MetricsCollector& collector) const;
    std::string ExportDashboard(const std::string& format = "json") const;
    
protected:
    // [SEQUENCE: MVP7-216] Background monitoring tasks
    void MonitoringLoop();
    void HealthCheckLoop();
    void StatsCollectionLoop();
    void SlowQueryCheckLoop();
    
    // [SEQUENCE: MVP7-217] Data collection
    void CollectConnectionStats();
    void CollectPerformanceStats();
    void CollectResourceStats();
    void CollectReplicationStats();
    
    // [SEQUENCE: MVP7-218] Alert checking
    void CheckAlertConditions();
    bool ShouldTriggerAlert(const std::string& alert_key) const;
    
private:
    DatabaseMonitorConfig config_;
    std::atomic<bool> running_{false};
    mutable std::mutex health_mutex_;
    DatabaseHealth current_health_;
    mutable std::mutex query_mutex_;
    std::unordered_map<std::string, QueryMetrics> query_metrics_;
    mutable std::mutex table_mutex_;
    std::unordered_map<std::string, TableStats> table_stats_;
    struct HistoricalPoint {
        std::chrono::system_clock::time_point timestamp;
        double qps;
        double avg_response_time_ms;
        double cpu_usage;
        double memory_usage_mb;
        uint32_t connection_count;
    };
    mutable std::mutex history_mutex_;
    std::vector<HistoricalPoint> historical_data_;
    mutable std::mutex alert_mutex_;
    std::vector<Alert> alerts_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> last_alert_time_;
    std::thread monitoring_thread_;
    std::thread health_check_thread_;
    std::thread stats_thread_;
    std::thread slow_query_thread_;
};

// [SEQUENCE: MVP7-219] Specialized monitors
class MySQLMonitor : public DatabaseMonitor {
public:
    explicit MySQLMonitor(const DatabaseMonitorConfig& config);
    
    // [SEQUENCE: MVP7-220] MySQL-specific monitoring
    struct InnoDBStatus {
        uint64_t buffer_pool_size_bytes;
        double buffer_pool_hit_rate;
        uint64_t buffer_pool_dirty_pages;
        uint64_t log_sequence_number;
        uint64_t log_flushed_to;
        uint64_t pages_flushed;
        uint64_t row_lock_waits;
        double avg_row_lock_wait_ms;
        uint64_t deadlocks;
    };
    InnoDBStatus GetInnoDBStatus() const;
    
    // [SEQUENCE: MVP7-221] Binary log monitoring
    struct BinlogStatus {
        bool enabled;
        std::string current_file;
        uint64_t position;
        uint64_t total_size_bytes;
        std::vector<std::string> files;
    };
    BinlogStatus GetBinlogStatus() const;
    
    // [SEQUENCE: MVP7-222] Process list analysis
    struct ProcessInfo {
        uint64_t id;
        std::string user;
        std::string host;
        std::string db;
        std::string command;
        uint32_t time_seconds;
        std::string state;
        std::string info;
    };
    std::vector<ProcessInfo> GetProcessList() const;
    void KillSlowQueries(uint32_t threshold_seconds);
};

// [SEQUENCE: MVP7-223] Redis monitor
class RedisMonitor : public DatabaseMonitor {
public:
    explicit RedisMonitor(const DatabaseMonitorConfig& config);
    
    // [SEQUENCE: MVP7-224] Redis-specific monitoring
    struct RedisInfo {
        std::string version;
        uint64_t uptime_seconds;
        uint32_t connected_clients;
        uint32_t blocked_clients;
        uint64_t used_memory_bytes;
        uint64_t used_memory_peak_bytes;
        double memory_fragmentation_ratio;
        uint64_t total_commands_processed;
        uint64_t instantaneous_ops_per_sec;
        std::chrono::system_clock::time_point last_save_time;
        uint64_t rdb_changes_since_last_save;
        std::string role;
        uint32_t connected_slaves;
    };
    RedisInfo GetRedisInfo() const;
    
    // [SEQUENCE: MVP7-225] Key analysis
    struct KeyspaceAnalysis {
        uint64_t total_keys;
        std::unordered_map<std::string, uint64_t> keys_by_type;
        std::unordered_map<std::string, uint64_t> keys_by_prefix;
        std::vector<std::string> large_keys;
        std::vector<std::string> hot_keys;
    };
    KeyspaceAnalysis AnalyzeKeyspace() const;
};

// [SEQUENCE: MVP7-226] Database monitoring utilities
namespace DatabaseMonitorUtils {
    std::string NormalizeQuery(const std::string& query);
    double CalculatePercentile(const std::vector<double>& values, double percentile);
    std::string FormatAlert(const DatabaseMonitor::Alert& alert);
    std::string GenerateHTMLDashboard(const DatabaseHealth& health,
                                     const std::vector<QueryMetrics>& slow_queries,
                                     const std::vector<TableStats>& table_stats);
}

} // namespace mmorpg::database
