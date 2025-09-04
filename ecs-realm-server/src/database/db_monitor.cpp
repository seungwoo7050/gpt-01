#include "database/db_monitor.h"
#include "database/connection_pool.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace mmorpg::database {

DatabaseMonitor::DatabaseMonitor(ConnectionPool& pool)
    : m_connection_pool(pool) {
}

DatabaseMonitor::~DatabaseMonitor() {
    Stop();
}

void DatabaseMonitor::Start(std::chrono::seconds interval) {
    if (m_running.exchange(true)) {
        return; // Already running
    }
    m_interval = interval;
    m_monitoring_thread = std::thread(&DatabaseMonitor::MonitoringLoop, this);
    spdlog::info("[DBMonitor] Started.");
}

void DatabaseMonitor::Stop() {
    if (!m_running.exchange(false)) {
        return; // Already stopped
    }
    if (m_monitoring_thread.joinable()) {
        m_monitoring_thread.join();
    }
    spdlog::info("[DBMonitor] Stopped.");
}

void DatabaseMonitor::RecordQuery(const std::string& query, std::chrono::milliseconds duration) {
    // For simplicity, we use the raw query as the key. Normalization would be better.
    std::lock_guard lock(m_metrics_mutex);
    auto& metrics = m_query_metrics[query];
    metrics.query_digest = query;
    metrics.count++;
    metrics.total_execution_time_ms += duration.count();
    metrics.avg_time_ms = metrics.total_execution_time_ms / metrics.count;
}

DBHealthStatus DatabaseMonitor::GetHealthStatus() const {
    std::lock_guard lock(m_metrics_mutex);
    return m_health_status;
}

std::vector<QueryMetrics> DatabaseMonitor::GetSlowQueries(uint32_t limit) const {
    std::lock_guard lock(m_metrics_mutex);
    std::vector<QueryMetrics> all_queries;
    for(const auto& pair : m_query_metrics) {
        all_queries.push_back(pair.second);
    }
    // Sort by average time, descending
    std::sort(all_queries.begin(), all_queries.end(), [](const auto& a, const auto& b){
        return a.avg_time_ms > b.avg_time_ms;
    });
    if (all_queries.size() > limit) {
        all_queries.resize(limit);
    }
    return all_queries;
}

std::vector<QueryMetrics> DatabaseMonitor::GetTopQueries(uint32_t limit) const {
    std::lock_guard lock(m_metrics_mutex);
    std::vector<QueryMetrics> all_queries;
    for(const auto& pair : m_query_metrics) {
        all_queries.push_back(pair.second);
    }
    // Sort by total execution count, descending
    std::sort(all_queries.begin(), all_queries.end(), [](const auto& a, const auto& b){
        return a.count > b.count;
    });
    if (all_queries.size() > limit) {
        all_queries.resize(limit);
    }
    return all_queries;
}


void DatabaseMonitor::MonitoringLoop() {
    while (m_running) {
        {
            // Update health status
            std::lock_guard lock(m_metrics_mutex);
            auto stats = m_connection_pool.GetStatistics();
            m_health_status.active_connections = stats.active_connections;
            m_health_status.pool_size = stats.pool_size;
            
            // A simple ping check could be done here by acquiring/releasing a connection
            auto start = std::chrono::steady_clock::now();
            auto conn = m_connection_pool.Acquire();
            if (conn) {
                m_health_status.is_connected = true;
                auto end = std::chrono::steady_clock::now();
                m_health_status.ping_latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                m_connection_pool.Release(conn);
            } else {
                m_health_status.is_connected = false;
                m_health_status.ping_latency_ms = -1;
            }
        }
        std::this_thread::sleep_for(m_interval);
    }
}

}