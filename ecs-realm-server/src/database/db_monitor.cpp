#include "db_monitor.h"
#include "core/logger.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <numeric>
#include <sstream>
#include <iomanip>
#include <regex>

namespace mmorpg::database {

// [SEQUENCE: 3239] Database monitor constructor
DatabaseMonitor::DatabaseMonitor(const DatabaseMonitorConfig& config)
    : config_(config) {
}

DatabaseMonitor::~DatabaseMonitor() {
    Stop();
}

// [SEQUENCE: MVP1-42] `DatabaseMonitor::Start()`: 모니터링 백그라운드 스레드를 시작합니다.
// [SEQUENCE: 3240] Start monitoring
bool DatabaseMonitor::Start() {
    if (running_) {
        return true;
    }
    
    spdlog::info("[DB_MONITOR] Starting database monitoring");
    
    running_ = true;
    
    // Start background threads
    monitoring_thread_ = std::thread([this] { MonitoringLoop(); });
    health_check_thread_ = std::thread([this] { HealthCheckLoop(); });
    stats_thread_ = std::thread([this] { StatsCollectionLoop(); });
    slow_query_thread_ = std::thread([this] { SlowQueryCheckLoop(); });
    
    spdlog::info("[DB_MONITOR] Database monitoring started");
    return true;
}

// [SEQUENCE: MVP1-43] `DatabaseMonitor::Stop()`: 모니터링을 중지합니다.
// [SEQUENCE: 3241] Stop monitoring
void DatabaseMonitor::Stop() {
    if (!running_) {
        return;
    }
    
    spdlog::info("[DB_MONITOR] Stopping database monitoring");
    
    running_ = false;
    
    // Wait for threads to finish
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
    if (health_check_thread_.joinable()) {
        health_check_thread_.join();
    }
    if (stats_thread_.joinable()) {
        stats_thread_.join();
    }
    if (slow_query_thread_.joinable()) {
        slow_query_thread_.join();
    }
    
    spdlog::info("[DB_MONITOR] Database monitoring stopped");
}

// [SEQUENCE: 3242] Get health status
DatabaseHealth DatabaseMonitor::GetHealthStatus() const {
    std::lock_guard<std::mutex> lock(health_mutex_);
    return current_health_;
}

// [SEQUENCE: MVP1-45] `DatabaseMonitor::CheckHealth()`: DB 연결 상태 등 헬스 체크를 수행합니다.
// [SEQUENCE: 3243] Check database health
void DatabaseMonitor::CheckHealth() {
    DatabaseHealth health;
    health.last_check = std::chrono::system_clock::now();
    
    // Collect all health metrics
    CollectConnectionStats();
    CollectPerformanceStats();
    CollectResourceStats();
    CollectReplicationStats();
    
    // Determine overall status
    if (!health.is_connected) {
        health.overall_status = DatabaseHealth::Status::OFFLINE;
        health.issues.push_back("Database connection lost");
    } else if (health.connection_usage_percent > 90) {
        health.overall_status = DatabaseHealth::Status::CRITICAL;
        health.issues.push_back("Connection pool nearly exhausted");
    } else if (health.cpu_usage_percent > config_.high_cpu_threshold) {
        health.overall_status = DatabaseHealth::Status::WARNING;
        health.issues.push_back("High CPU usage detected");
    } else if (health.memory_usage_mb > config_.high_memory_threshold) {
        health.overall_status = DatabaseHealth::Status::WARNING;
        health.issues.push_back("High memory usage detected");
    } else if (health.replication_lag_seconds > 60) {
        health.overall_status = DatabaseHealth::Status::WARNING;
        health.issues.push_back("Replication lag detected");
    }
    
    // Update current health
    {
        std::lock_guard<std::mutex> lock(health_mutex_);
        current_health_ = health;
    }
    
    // Check alert conditions
    CheckAlertConditions();
}

// [SEQUENCE: MVP1-44] `DatabaseMonitor::RecordQueryExecution()`: 쿼리 실행 정보를 기록합니다.
// [SEQUENCE: 3244] Record query execution
void DatabaseMonitor::RecordQueryExecution(const std::string& query,
                                         double execution_time_ms,
                                         uint64_t rows_examined,
                                         uint64_t rows_sent) {
    // Normalize query
    std::string normalized = DatabaseMonitorUtils::NormalizeQuery(query);
    
    std::lock_guard<std::mutex> lock(query_mutex_);
    
    auto& metrics = query_metrics_[normalized];
    metrics.query_digest = normalized;
    metrics.execution_count++;
    
    // Update timing statistics
    metrics.total_time_ms += execution_time_ms;
    metrics.avg_time_ms = metrics.total_time_ms / metrics.execution_count;
    
    if (metrics.execution_count == 1) {
        metrics.min_time_ms = execution_time_ms;
        metrics.max_time_ms = execution_time_ms;
    } else {
        metrics.min_time_ms = std::min(metrics.min_time_ms, execution_time_ms);
        metrics.max_time_ms = std::max(metrics.max_time_ms, execution_time_ms);
    }
    
    // Update resource statistics
    metrics.rows_examined_total += rows_examined;
    metrics.rows_sent_total += rows_sent;
    
    // Check if slow query
    if (execution_time_ms > config_.slow_query_threshold_ms) {
        spdlog::warn("[DB_MONITOR] Slow query detected: {} ms - {}",
                    execution_time_ms, normalized);
    }
}

// [SEQUENCE: 3245] Get slow queries
std::vector<QueryMetrics> DatabaseMonitor::GetSlowQueries(uint32_t limit) const {
    std::lock_guard<std::mutex> lock(query_mutex_);
    
    std::vector<QueryMetrics> queries;
    for (const auto& [digest, metrics] : query_metrics_) {
        if (metrics.avg_time_ms > config_.slow_query_threshold_ms) {
            queries.push_back(metrics);
        }
    }
    
    // Sort by average execution time
    std::sort(queries.begin(), queries.end(),
              [](const QueryMetrics& a, const QueryMetrics& b) {
                  return a.avg_time_ms > b.avg_time_ms;
              });
    
    // Limit results
    if (queries.size() > limit) {
        queries.resize(limit);
    }
    
    return queries;
}

// [SEQUENCE: 3246] Get top queries
std::vector<QueryMetrics> DatabaseMonitor::GetTopQueries(uint32_t limit) const {
    std::lock_guard<std::mutex> lock(query_mutex_);
    
    std::vector<QueryMetrics> queries;
    for (const auto& [digest, metrics] : query_metrics_) {
        queries.push_back(metrics);
    }
    
    // Sort by total execution time
    std::sort(queries.begin(), queries.end(),
              [](const QueryMetrics& a, const QueryMetrics& b) {
                  return a.total_time_ms > b.total_time_ms;
              });
    
    // Limit results
    if (queries.size() > limit) {
        queries.resize(limit);
    }
    
    return queries;
}

// [SEQUENCE: 3247] Update table statistics
void DatabaseMonitor::UpdateTableStats(const std::string& table_name) {
    TableStats stats;
    stats.table_name = table_name;
    
    // In real implementation, would query database for stats
    // SELECT table_rows, data_length, index_length FROM information_schema.TABLES
    // WHERE table_schema = ? AND table_name = ?
    
    std::lock_guard<std::mutex> lock(table_mutex_);
    table_stats_[table_name] = stats;
}

// [SEQUENCE: 3248] Get table statistics
TableStats DatabaseMonitor::GetTableStats(const std::string& table_name) const {
    std::lock_guard<std::mutex> lock(table_mutex_);
    
    auto it = table_stats_.find(table_name);
    if (it != table_stats_.end()) {
        return it->second;
    }
    
    return TableStats();
}

// [SEQUENCE: 3249] Get all table statistics
std::vector<TableStats> DatabaseMonitor::GetAllTableStats() const {
    std::lock_guard<std::mutex> lock(table_mutex_);
    
    std::vector<TableStats> stats;
    for (const auto& [name, table_stats] : table_stats_) {
        stats.push_back(table_stats);
    }
    
    // Sort by data size
    std::sort(stats.begin(), stats.end(),
              [](const TableStats& a, const TableStats& b) {
                  return a.data_size_bytes > b.data_size_bytes;
              });
    
    return stats;
}

// [SEQUENCE: 3250] Generate performance report
DatabaseMonitor::PerformanceReport DatabaseMonitor::GeneratePerformanceReport(
    std::chrono::hours duration) const {
    
    PerformanceReport report;
    report.timestamp = std::chrono::system_clock::now();
    
    auto cutoff_time = report.timestamp - duration;
    
    std::lock_guard<std::mutex> lock(history_mutex_);
    
    // Calculate averages from historical data
    double total_qps = 0;
    double total_response_time = 0;
    double total_cpu = 0;
    double total_memory = 0;
    uint32_t sample_count = 0;
    
    for (const auto& point : historical_data_) {
        if (point.timestamp < cutoff_time) {
            continue;
        }
        
        total_qps += point.qps;
        total_response_time += point.avg_response_time_ms;
        total_cpu += point.cpu_usage;
        total_memory += point.memory_usage_mb;
        sample_count++;
        
        // Track peaks
        report.peak_qps = std::max(report.peak_qps, point.qps);
        report.peak_cpu_usage = std::max(report.peak_cpu_usage, point.cpu_usage);
        report.peak_memory_usage_mb = std::max(report.peak_memory_usage_mb, 
                                              point.memory_usage_mb);
    }
    
    if (sample_count > 0) {
        report.avg_qps = total_qps / sample_count;
        report.avg_response_time_ms = total_response_time / sample_count;
        report.avg_cpu_usage = total_cpu / sample_count;
        report.avg_memory_usage_mb = total_memory / sample_count;
    }
    
    // Identify bottlenecks
    if (report.avg_cpu_usage > 80) {
        report.bottlenecks.push_back("CPU bottleneck detected");
        report.recommendations.push_back("Consider query optimization or hardware upgrade");
    }
    
    if (report.avg_response_time_ms > 100) {
        report.bottlenecks.push_back("High query response time");
        report.recommendations.push_back("Review slow query log and add indexes");
    }
    
    return report;
}

// [SEQUENCE: 3251] Trigger alert
void DatabaseMonitor::TriggerAlert(const Alert& alert) {
    std::lock_guard<std::mutex> lock(alert_mutex_);
    
    // Check cooldown
    std::string alert_key = alert.category + "_" + alert.message;
    if (ShouldTriggerAlert(alert_key)) {
        alerts_.push_back(alert);
        last_alert_time_[alert_key] = alert.timestamp;
        
        // Log alert
        switch (alert.severity) {
            case Alert::Severity::CRITICAL:
                spdlog::critical("[DB_MONITOR] {}: {}", alert.category, alert.message);
                break;
            case Alert::Severity::ERROR:
                spdlog::error("[DB_MONITOR] {}: {}", alert.category, alert.message);
                break;
            case Alert::Severity::WARNING:
                spdlog::warn("[DB_MONITOR] {}: {}", alert.category, alert.message);
                break;
            default:
                spdlog::info("[DB_MONITOR] {}: {}", alert.category, alert.message);
        }
        
        // Send to alert endpoints if configured
        if (config_.enable_alerts) {
            // Would send to configured endpoints (webhook, email, etc.)
        }
    }
}

// [SEQUENCE: 3252] Get recent alerts
std::vector<DatabaseMonitor::Alert> DatabaseMonitor::GetRecentAlerts(
    std::chrono::minutes duration) const {
    
    std::lock_guard<std::mutex> lock(alert_mutex_);
    
    auto cutoff_time = std::chrono::system_clock::now() - duration;
    std::vector<Alert> recent_alerts;
    
    for (const auto& alert : alerts_) {
        if (alert.timestamp > cutoff_time) {
            recent_alerts.push_back(alert);
        }
    }
    
    return recent_alerts;
}

// [SEQUENCE: 3253] Export metrics
void DatabaseMonitor::ExportMetrics(monitoring::MetricsCollector& collector) const {
    auto health = GetHealthStatus();
    
    // Connection metrics
    collector.RecordGauge("db_connections_active", health.active_connections);
    collector.RecordGauge("db_connections_max", health.max_connections);
    collector.RecordGauge("db_connection_usage_percent", health.connection_usage_percent);
    
    // Performance metrics
    collector.RecordGauge("db_queries_per_second", health.queries_per_second);
    collector.RecordGauge("db_avg_query_time_ms", health.avg_query_time_ms);
    collector.RecordCounter("db_slow_queries_total", health.slow_queries_count);
    
    // Resource metrics
    collector.RecordGauge("db_cpu_usage_percent", health.cpu_usage_percent);
    collector.RecordGauge("db_memory_usage_mb", health.memory_usage_mb);
    collector.RecordGauge("db_disk_usage_gb", health.disk_usage_gb);
    
    // Replication metrics
    collector.RecordGauge("db_replication_lag_seconds", health.replication_lag_seconds);
}

// [SEQUENCE: 3254] Export dashboard
std::string DatabaseMonitor::ExportDashboard(const std::string& format) const {
    if (format == "json") {
        std::ostringstream oss;
        oss << "{\n";
        
        // Health status
        auto health = GetHealthStatus();
        oss << "  \"health\": {\n";
        oss << "    \"status\": \"" << 
            (health.overall_status == DatabaseHealth::Status::HEALTHY ? "healthy" :
             health.overall_status == DatabaseHealth::Status::WARNING ? "warning" :
             health.overall_status == DatabaseHealth::Status::CRITICAL ? "critical" : 
             "offline") << "\",\n";
        oss << "    \"connections\": " << health.active_connections << ",\n";
        oss << "    \"qps\": " << health.queries_per_second << ",\n";
        oss << "    \"avg_query_time_ms\": " << health.avg_query_time_ms << "\n";
        oss << "  },\n";
        
        // Slow queries
        oss << "  \"slow_queries\": [\n";
        auto slow_queries = GetSlowQueries(5);
        for (size_t i = 0; i < slow_queries.size(); ++i) {
            const auto& query = slow_queries[i];
            oss << "    {\n";
            oss << "      \"query\": \"" << query.query_digest << "\",\n";
            oss << "      \"avg_time_ms\": " << query.avg_time_ms << ",\n";
            oss << "      \"count\": " << query.execution_count << "\n";
            oss << "    }" << (i < slow_queries.size() - 1 ? "," : "") << "\n";
        }
        oss << "  ]\n";
        oss << "}\n";
        
        return oss.str();
    }
    
    return "";
}

// [SEQUENCE: 3255] Monitoring loop
void DatabaseMonitor::MonitoringLoop() {
    while (running_) {
        // Record current metrics
        auto health = GetHealthStatus();
        
        HistoricalPoint point;
        point.timestamp = std::chrono::system_clock::now();
        point.qps = health.queries_per_second;
        point.avg_response_time_ms = health.avg_query_time_ms;
        point.cpu_usage = health.cpu_usage_percent;
        point.memory_usage_mb = health.memory_usage_mb;
        point.connection_count = health.active_connections;
        
        {
            std::lock_guard<std::mutex> lock(history_mutex_);
            historical_data_.push_back(point);
            
            // Keep only recent data (24 hours)
            auto cutoff = point.timestamp - std::chrono::hours(24);
            historical_data_.erase(
                std::remove_if(historical_data_.begin(), historical_data_.end(),
                             [cutoff](const HistoricalPoint& p) {
                                 return p.timestamp < cutoff;
                             }),
                historical_data_.end()
            );
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(60));
    }
}

// [SEQUENCE: 3256] Health check loop
void DatabaseMonitor::HealthCheckLoop() {
    while (running_) {
        CheckHealth();
        std::this_thread::sleep_for(config_.health_check_interval);
    }
}

// [SEQUENCE: 3257] Stats collection loop
void DatabaseMonitor::StatsCollectionLoop() {
    while (running_) {
        // Update table statistics
        for (const auto& [table_name, _] : table_stats_) {
            UpdateTableStats(table_name);
        }
        
        std::this_thread::sleep_for(config_.stats_collection_interval);
    }
}

// [SEQUENCE: 3258] Slow query check loop
void DatabaseMonitor::SlowQueryCheckLoop() {
    while (running_) {
        auto slow_queries = GetSlowQueries(10);
        
        if (!slow_queries.empty()) {
            Alert alert;
            alert.severity = Alert::Severity::WARNING;
            alert.category = "performance";
            alert.message = std::to_string(slow_queries.size()) + 
                          " slow queries detected";
            alert.timestamp = std::chrono::system_clock::now();
            
            TriggerAlert(alert);
        }
        
        std::this_thread::sleep_for(config_.slow_query_check_interval);
    }
}

// [SEQUENCE: 3259] Collect connection statistics
void DatabaseMonitor::CollectConnectionStats() {
    // In real implementation, would query database
    // SHOW STATUS LIKE 'Threads_connected'
    // SHOW VARIABLES LIKE 'max_connections'
}

// [SEQUENCE: 3260] Collect performance statistics
void DatabaseMonitor::CollectPerformanceStats() {
    // In real implementation, would query database
    // SHOW GLOBAL STATUS WHERE Variable_name IN 
    // ('Questions', 'Slow_queries', 'Com_select', 'Com_insert', ...)
}

// [SEQUENCE: 3261] Collect resource statistics
void DatabaseMonitor::CollectResourceStats() {
    // In real implementation, would query system metrics
    // CPU usage, memory usage, disk I/O
}

// [SEQUENCE: 3262] Collect replication statistics
void DatabaseMonitor::CollectReplicationStats() {
    // In real implementation, would query database
    // SHOW SLAVE STATUS
}

// [SEQUENCE: 3263] Check alert conditions
void DatabaseMonitor::CheckAlertConditions() {
    auto health = GetHealthStatus();
    
    // Connection alerts
    if (health.connection_usage_percent > 90) {
        Alert alert;
        alert.severity = Alert::Severity::CRITICAL;
        alert.category = "connection";
        alert.message = "Connection pool usage above 90%";
        alert.timestamp = std::chrono::system_clock::now();
        TriggerAlert(alert);
    }
    
    // Performance alerts
    if (health.avg_query_time_ms > 1000) {
        Alert alert;
        alert.severity = Alert::Severity::ERROR;
        alert.category = "performance";
        alert.message = "Average query time exceeds 1 second";
        alert.timestamp = std::chrono::system_clock::now();
        TriggerAlert(alert);
    }
    
    // Replication alerts
    if (health.replication_lag_seconds > 300) {
        Alert alert;
        alert.severity = Alert::Severity::ERROR;
        alert.category = "replication";
        alert.message = "Replication lag exceeds 5 minutes";
        alert.timestamp = std::chrono::system_clock::now();
        TriggerAlert(alert);
    }
}

// [SEQUENCE: 3264] Should trigger alert
bool DatabaseMonitor::ShouldTriggerAlert(const std::string& alert_key) const {
    auto it = last_alert_time_.find(alert_key);
    if (it == last_alert_time_.end()) {
        return true;  // First time
    }
    
    auto elapsed = std::chrono::system_clock::now() - it->second;
    auto cooldown = std::chrono::minutes(config_.alert_cooldown_minutes);
    
    return elapsed > cooldown;
}

// [SEQUENCE: 3265] MySQL monitor implementation
MySQLMonitor::MySQLMonitor(const DatabaseMonitorConfig& config)
    : DatabaseMonitor(config) {
}

MySQLMonitor::InnoDBStatus MySQLMonitor::GetInnoDBStatus() const {
    InnoDBStatus status;
    
    // In real implementation:
    // SHOW ENGINE INNODB STATUS
    // Parse output for various metrics
    
    return status;
}

MySQLMonitor::BinlogStatus MySQLMonitor::GetBinlogStatus() const {
    BinlogStatus status;
    
    // In real implementation:
    // SHOW MASTER STATUS
    // SHOW BINARY LOGS
    
    return status;
}

std::vector<MySQLMonitor::ProcessInfo> MySQLMonitor::GetProcessList() const {
    std::vector<ProcessInfo> processes;
    
    // In real implementation:
    // SHOW FULL PROCESSLIST
    
    return processes;
}

void MySQLMonitor::KillSlowQueries(uint32_t threshold_seconds) {
    auto processes = GetProcessList();
    
    for (const auto& proc : processes) {
        if (proc.time_seconds > threshold_seconds && 
            proc.command == "Query") {
            // KILL QUERY <id>
            spdlog::warn("[MYSQL_MONITOR] Killing slow query: {} ({}s)",
                        proc.id, proc.time_seconds);
        }
    }
}

// [SEQUENCE: 3266] Redis monitor implementation
RedisMonitor::RedisMonitor(const DatabaseMonitorConfig& config)
    : DatabaseMonitor(config) {
}

RedisMonitor::RedisInfo RedisMonitor::GetRedisInfo() const {
    RedisInfo info;
    
    // In real implementation:
    // INFO command to Redis
    // Parse response
    
    return info;
}

RedisMonitor::KeyspaceAnalysis RedisMonitor::AnalyzeKeyspace() const {
    KeyspaceAnalysis analysis;
    
    // In real implementation:
    // SCAN through keys
    // TYPE command for each key
    // MEMORY USAGE for large keys
    
    return analysis;
}

// [SEQUENCE: 3267] Database monitor utilities
namespace DatabaseMonitorUtils {

std::string NormalizeQuery(const std::string& query) {
    std::string normalized = query;
    
    // Remove extra whitespace
    normalized = std::regex_replace(normalized, std::regex("\\s+"), " ");
    
    // Replace values with placeholders
    normalized = std::regex_replace(normalized, std::regex("\\b\\d+\\b"), "?");
    normalized = std::regex_replace(normalized, std::regex("'[^']*'"), "'?'");
    
    // Convert to uppercase
    std::transform(normalized.begin(), normalized.end(), 
                  normalized.begin(), ::toupper);
    
    return normalized;
}

double CalculatePercentile(const std::vector<double>& values, double percentile) {
    if (values.empty()) {
        return 0.0;
    }
    
    std::vector<double> sorted = values;
    std::sort(sorted.begin(), sorted.end());
    
    size_t index = static_cast<size_t>(percentile / 100.0 * sorted.size());
    return sorted[std::min(index, sorted.size() - 1)];
}

std::string FormatAlert(const DatabaseMonitor::Alert& alert) {
    std::ostringstream oss;
    
    // Severity
    switch (alert.severity) {
        case DatabaseMonitor::Alert::Severity::CRITICAL:
            oss << "[CRITICAL]";
            break;
        case DatabaseMonitor::Alert::Severity::ERROR:
            oss << "[ERROR]";
            break;
        case DatabaseMonitor::Alert::Severity::WARNING:
            oss << "[WARNING]";
            break;
        default:
            oss << "[INFO]";
    }
    
    // Timestamp
    auto time_t = std::chrono::system_clock::to_time_t(alert.timestamp);
    oss << " " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    
    // Category and message
    oss << " [" << alert.category << "] " << alert.message;
    
    // Tags
    if (!alert.tags.empty()) {
        oss << " {";
        bool first = true;
        for (const auto& [key, value] : alert.tags) {
            if (!first) oss << ", ";
            oss << key << "=" << value;
            first = false;
        }
        oss << "}";
    }
    
    return oss.str();
}

std::string GenerateHTMLDashboard(const DatabaseHealth& health,
                                 const std::vector<QueryMetrics>& slow_queries,
                                 [[maybe_unused]] const std::vector<TableStats>& table_stats) {
    std::ostringstream html;
    
    html << "<!DOCTYPE html>\n";
    html << "<html>\n";
    html << "<head>\n";
    html << "  <title>Database Monitor Dashboard</title>\n";
    html << "  <style>\n";
    html << "    body { font-family: Arial, sans-serif; margin: 20px; }\n";
    html << "    .status-healthy { color: green; }\n";
    html << "    .status-warning { color: orange; }\n";
    html << "    .status-critical { color: red; }\n";
    html << "    table { border-collapse: collapse; width: 100%; margin: 20px 0; }\n";
    html << "    th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }\n";
    html << "    th { background-color: #f2f2f2; }\n";
    html << "  </style>\n";
    html << "</head>\n";
    html << "<body>\n";
    
    // Health status
    html << "<h1>Database Health Status</h1>\n";
    html << "<div class=\"status-" << 
        (health.overall_status == DatabaseHealth::Status::HEALTHY ? "healthy" :
         health.overall_status == DatabaseHealth::Status::WARNING ? "warning" : 
         "critical") << "\">\n";
    html << "  Status: " << 
        (health.overall_status == DatabaseHealth::Status::HEALTHY ? "HEALTHY" :
         health.overall_status == DatabaseHealth::Status::WARNING ? "WARNING" :
         health.overall_status == DatabaseHealth::Status::CRITICAL ? "CRITICAL" :
         "OFFLINE") << "\n";
    html << "</div>\n";
    
    // Metrics
    html << "<h2>Performance Metrics</h2>\n";
    html << "<ul>\n";
    html << "  <li>Queries per second: " << health.queries_per_second << "</li>\n";
    html << "  <li>Average query time: " << health.avg_query_time_ms << " ms</li>\n";
    html << "  <li>Active connections: " << health.active_connections << "/" 
         << health.max_connections << "</li>\n";
    html << "  <li>CPU usage: " << health.cpu_usage_percent << "%</li>\n";
    html << "  <li>Memory usage: " << health.memory_usage_mb << " MB</li>\n";
    html << "</ul>\n";
    
    // Slow queries
    html << "<h2>Slow Queries</h2>\n";
    html << "<table>\n";
    html << "  <tr><th>Query</th><th>Avg Time (ms)</th><th>Count</th></tr>\n";
    for (const auto& query : slow_queries) {
        html << "  <tr>\n";
        html << "    <td>" << query.query_digest << "</td>\n";
        html << "    <td>" << query.avg_time_ms << "</td>\n";
        html << "    <td>" << query.execution_count << "</td>\n";
        html << "  </tr>\n";
    }
    html << "</table>\n";
    
    html << "</body>\n";
    html << "</html>\n";
    
    return html.str();
}

} // namespace DatabaseMonitorUtils

} // namespace mmorpg::database
