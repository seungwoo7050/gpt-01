#pragma once

#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <string>
#include <vector>
#include <map>

namespace mmorpg::database {

class ConnectionPool;

// [SEQUENCE: MVP7-7] Defines the health status of the database.
struct DBHealthStatus {
    bool is_connected = false;
    long long ping_latency_ms = -1;
    size_t active_connections = 0;
    size_t pool_size = 0;
    double queries_per_second = 0.0;
    double avg_query_time_ms = 0.0;
};

// [SEQUENCE: MVP7-8] Holds metrics for a specific type of query.
struct QueryMetrics {
    std::string query_digest;
    long long count = 0;
    long long total_execution_time_ms = 0;
    long long avg_time_ms = 0;
};

// [SEQUENCE: MVP7-15] Defines an alert for critical database events.
struct Alert {
    enum class Severity { WARNING, ERROR, CRITICAL };
    Severity severity;
    std::string message;
    std::string category;
    std::chrono::system_clock::time_point timestamp;
    std::map<std::string, std::string> tags;
};

// [SEQUENCE: MVP7-9] Monitors database health and query performance.
class DatabaseMonitor {
public:
    explicit DatabaseMonitor(ConnectionPool& pool);
    ~DatabaseMonitor();

    DatabaseMonitor(const DatabaseMonitor&) = delete;
    DatabaseMonitor& operator=(const DatabaseMonitor&) = delete;

    void Start(std::chrono::seconds interval);
    void Stop();
    void RecordQuery(const std::string& query, std::chrono::milliseconds duration);
    DBHealthStatus GetHealthStatus() const;
    std::vector<QueryMetrics> GetSlowQueries(uint32_t limit) const;
    std::vector<QueryMetrics> GetTopQueries(uint32_t limit) const;

private:
    void MonitoringLoop();

    std::atomic<bool> m_running{false};
    std::thread m_monitoring_thread;
    std::chrono::seconds m_interval;
    
    ConnectionPool& m_connection_pool;

    mutable std::mutex m_metrics_mutex;
    std::map<std::string, QueryMetrics> m_query_metrics;
    DBHealthStatus m_health_status;
};

}