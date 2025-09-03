#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <atomic>
#include <thread>
#include <unordered_map>
#include "core/types.h"
#include "../monitoring/metrics_collector.h"

namespace mmorpg::database {

// [SEQUENCE: 3216] Database monitoring configuration
struct DatabaseMonitorConfig {
    // Monitoring intervals
    std::chrono::seconds health_check_interval{30};      // 헬스 체크 주기
    std::chrono::seconds stats_collection_interval{60};  // 통계 수집 주기
    std::chrono::seconds slow_query_check_interval{10};  // 슬로우 쿼리 체크
    
    // Thresholds
    uint32_t slow_query_threshold_ms{100};       // 슬로우 쿼리 임계값
    uint32_t connection_timeout_ms{5000};        // 연결 타임아웃
    double high_cpu_threshold{80.0};             // CPU 사용률 임계값
    double high_memory_threshold{85.0};          // 메모리 사용률 임계값
    uint64_t max_connections_threshold{1000};    // 최대 연결 수 임계값
    
    // Alert settings
    bool enable_alerts{true};                    // 알림 활성화
    uint32_t alert_cooldown_minutes{15};         // 알림 쿨다운
    std::vector<std::string> alert_endpoints;    // 알림 엔드포인트
};

// [SEQUENCE: 3217] Database health status
struct DatabaseHealth {
    enum class Status {
        HEALTHY,        // 정상
        WARNING,        // 경고
        CRITICAL,       // 위험
        OFFLINE        // 오프라인
    };
    
    Status overall_status{Status::HEALTHY};
    std::chrono::system_clock::time_point last_check;
    
    // Connection health
    bool is_connected{false};
    uint32_t active_connections{0};
    uint32_t max_connections{0};
    double connection_usage_percent{0.0};
    
    // Performance metrics
    double queries_per_second{0.0};
    double avg_query_time_ms{0.0};
    uint64_t slow_queries_count{0};
    
    // Resource usage
    double cpu_usage_percent{0.0};
    double memory_usage_mb{0.0};
    double disk_usage_gb{0.0};
    double disk_io_mbps{0.0};
    
    // Replication status
    bool replication_running{true};
    uint64_t replication_lag_seconds{0};
    
    // Issues detected
    std::vector<std::string> issues;
};

// [SEQUENCE: 3218] Query performance metrics
struct QueryMetrics {
    std::string query_digest;          // 정규화된 쿼리
    uint64_t execution_count{0};       // 실행 횟수
    
    // Timing
    double total_time_ms{0.0};         // 총 실행 시간
    double avg_time_ms{0.0};           // 평균 실행 시간
    double min_time_ms{0.0};           // 최소 실행 시간
    double max_time_ms{0.0};           // 최대 실행 시간
    double p95_time_ms{0.0};           // 95 퍼센타일
    double p99_time_ms{0.0};           // 99 퍼센타일
    
    // Resources
    uint64_t rows_examined_total{0};   // 검사된 총 행 수
    uint64_t rows_sent_total{0};       // 반환된 총 행 수
    uint64_t tmp_tables_created{0};    // 생성된 임시 테이블
    
    // Lock statistics
    double lock_time_ms{0.0};          // 락 대기 시간
    uint64_t lock_wait_count{0};       // 락 대기 횟수
};

// [SEQUENCE: 3219] Table statistics
struct TableStats {
    std::string table_name;
    std::string engine;                // Storage engine (InnoDB, MyISAM)
    
    // Size metrics
    uint64_t row_count{0};             // 행 수
    uint64_t data_size_bytes{0};       // 데이터 크기
    uint64_t index_size_bytes{0};      // 인덱스 크기
    double fragmentation_percent{0.0}; // 단편화 비율
    
    // Access patterns
    uint64_t read_count{0};            // 읽기 횟수
    uint64_t write_count{0};           // 쓰기 횟수
    uint64_t update_count{0};          // 업데이트 횟수
    uint64_t delete_count{0};          // 삭제 횟수
    
    // Performance
    double avg_read_time_ms{0.0};      // 평균 읽기 시간
    double avg_write_time_ms{0.0};     // 평균 쓰기 시간
    
    // Index usage
    std::unordered_map<std::string, uint64_t> index_usage_count;
    std::vector<std::string> unused_indexes;
};

// [SEQUENCE: MVP1-41] Database monitor interface
class DatabaseMonitor {
public:
    explicit DatabaseMonitor(const DatabaseMonitorConfig& config);
    ~DatabaseMonitor();
    
    // [SEQUENCE: 3221] Lifecycle management
    // [SEQUENCE: MVP1-42] `DatabaseMonitor::Start()`: 모니터링 백그라운드 스레드를 시작합니다.
    bool Start();
    // [SEQUENCE: MVP1-43] `DatabaseMonitor::Stop()`: 모니터링을 중지합니다.
    void Stop();
    bool IsRunning() const { return running_; }
    
    // [SEQUENCE: 3222] Health monitoring
    DatabaseHealth GetHealthStatus() const;
    // [SEQUENCE: MVP1-45] `DatabaseMonitor::CheckHealth()`: DB 연결 상태 등 헬스 체크를 수행합니다.
    void CheckHealth();
    
    // [SEQUENCE: 3223] Query monitoring
    // [SEQUENCE: MVP1-44] `DatabaseMonitor::RecordQueryExecution()`: 쿼리 실행 정보를 기록합니다.
    void RecordQueryExecution(const std::string& query,
                            double execution_time_ms,
                            uint64_t rows_examined,
                            uint64_t rows_sent);
    
    std::vector<QueryMetrics> GetSlowQueries(uint32_t limit = 10) const;
    std::vector<QueryMetrics> GetTopQueries(uint32_t limit = 10) const;
    
    // [SEQUENCE: 3224] Table monitoring
    void UpdateTableStats(const std::string& table_name);
    TableStats GetTableStats(const std::string& table_name) const;
    std::vector<TableStats> GetAllTableStats() const;
    
    // [SEQUENCE: 3225] Performance analysis
    struct PerformanceReport {
        std::chrono::system_clock::time_point timestamp;
        
        // Overall metrics
        double avg_qps{0.0};                      // Queries per second
        double peak_qps{0.0};                     // Peak QPS
        double avg_response_time_ms{0.0};         // Average response time
        
        // Resource utilization
        double avg_cpu_usage{0.0};                // Average CPU usage
        double peak_cpu_usage{0.0};               // Peak CPU usage
        double avg_memory_usage_mb{0.0};          // Average memory usage
        double peak_memory_usage_mb{0.0};         // Peak memory usage
        
        // Top issues
        std::vector<std::string> bottlenecks;     // Identified bottlenecks
        std::vector<std::string> recommendations; // Optimization suggestions
    };
    
    PerformanceReport GeneratePerformanceReport(
        std::chrono::hours duration = std::chrono::hours(24)) const;
    
    // [SEQUENCE: 3226] Alert management
    struct Alert {
        enum class Severity {
            INFO,
            WARNING,
            ERROR,
            CRITICAL
        };
        
        Severity severity;
        std::string category;      // "connection", "performance", "replication"
        std::string message;
        std::chrono::system_clock::time_point timestamp;
        
        // Additional context
        std::unordered_map<std::string, std::string> tags;
    };
    
    void TriggerAlert(const Alert& alert);
    std::vector<Alert> GetRecentAlerts(std::chrono::minutes duration) const;
    
    // [SEQUENCE: 3227] Metrics export
    void ExportMetrics(monitoring::MetricsCollector& collector) const;
    std::string ExportDashboard(const std::string& format = "json") const;
    
protected:
    // [SEQUENCE: 3228] Background monitoring tasks
    void MonitoringLoop();
    void HealthCheckLoop();
    void StatsCollectionLoop();
    void SlowQueryCheckLoop();
    
    // [SEQUENCE: 3229] Data collection
    void CollectConnectionStats();
    void CollectPerformanceStats();
    void CollectResourceStats();
    void CollectReplicationStats();
    
    // [SEQUENCE: 3230] Alert checking
    void CheckAlertConditions();
    bool ShouldTriggerAlert(const std::string& alert_key) const;
    
private:
    DatabaseMonitorConfig config_;
    std::atomic<bool> running_{false};
    
    // Current state
    mutable std::mutex health_mutex_;
    DatabaseHealth current_health_;
    
    // Query metrics
    mutable std::mutex query_mutex_;
    std::unordered_map<std::string, QueryMetrics> query_metrics_;
    
    // Table statistics
    mutable std::mutex table_mutex_;
    std::unordered_map<std::string, TableStats> table_stats_;
    
    // Historical data
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
    
    // Alerts
    mutable std::mutex alert_mutex_;
    std::vector<Alert> alerts_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> 
        last_alert_time_;
    
    // Background threads
    std::thread monitoring_thread_;
    std::thread health_check_thread_;
    std::thread stats_thread_;
    std::thread slow_query_thread_;
};

// [SEQUENCE: 3231] Specialized monitors
class MySQLMonitor : public DatabaseMonitor {
public:
    explicit MySQLMonitor(const DatabaseMonitorConfig& config);
    
    // [SEQUENCE: 3232] MySQL-specific monitoring
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
    
    // [SEQUENCE: 3233] Binary log monitoring
    struct BinlogStatus {
        bool enabled;
        std::string current_file;
        uint64_t position;
        uint64_t total_size_bytes;
        std::vector<std::string> files;
    };
    
    BinlogStatus GetBinlogStatus() const;
    
    // [SEQUENCE: 3234] Process list analysis
    struct ProcessInfo {
        uint64_t id;
        std::string user;
        std::string host;
        std::string db;
        std::string command;
        uint32_t time_seconds;
        std::string state;
        std::string info;  // Current query
    };
    
    std::vector<ProcessInfo> GetProcessList() const;
    void KillSlowQueries(uint32_t threshold_seconds);
};

// [SEQUENCE: 3235] Redis monitor
class RedisMonitor : public DatabaseMonitor {
public:
    explicit RedisMonitor(const DatabaseMonitorConfig& config);
    
    // [SEQUENCE: 3236] Redis-specific monitoring
    struct RedisInfo {
        // Server
        std::string version;
        uint64_t uptime_seconds;
        
        // Clients
        uint32_t connected_clients;
        uint32_t blocked_clients;
        
        // Memory
        uint64_t used_memory_bytes;
        uint64_t used_memory_peak_bytes;
        double memory_fragmentation_ratio;
        
        // Stats
        uint64_t total_commands_processed;
        uint64_t instantaneous_ops_per_sec;
        
        // Persistence
        std::chrono::system_clock::time_point last_save_time;
        uint64_t rdb_changes_since_last_save;
        
        // Replication
        std::string role;  // master/slave
        uint32_t connected_slaves;
    };
    
    RedisInfo GetRedisInfo() const;
    
    // [SEQUENCE: 3237] Key analysis
    struct KeyspaceAnalysis {
        uint64_t total_keys;
        std::unordered_map<std::string, uint64_t> keys_by_type;
        std::unordered_map<std::string, uint64_t> keys_by_prefix;
        std::vector<std::string> large_keys;
        std::vector<std::string> hot_keys;
    };
    
    KeyspaceAnalysis AnalyzeKeyspace() const;
};

// [SEQUENCE: 3238] Database monitoring utilities
namespace DatabaseMonitorUtils {
    // Query normalization
    std::string NormalizeQuery(const std::string& query);
    
    // Performance calculations
    double CalculatePercentile(const std::vector<double>& values, 
                              double percentile);
    
    // Alert formatting
    std::string FormatAlert(const DatabaseMonitor::Alert& alert);
    
    // Dashboard generation
    std::string GenerateHTMLDashboard(const DatabaseHealth& health,
                                     const std::vector<QueryMetrics>& slow_queries,
                                     const std::vector<TableStats>& table_stats);
}

} // namespace mmorpg::database