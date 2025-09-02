#pragma once

#include <chrono>
#include <vector>
#include <memory>
#include <iostream>
#include <boost/asio.hpp>

namespace mmorpg::testing {

// [SEQUENCE: MVP19-67] Performance comparison between callback and coroutine approaches
class CoroutinePerformanceTest {
public:
    struct BenchmarkResult {
        std::string test_name;
        double avg_latency_ms;
        double min_latency_ms;
        double max_latency_ms;
        double throughput_ops_per_sec;
        size_t total_operations;
        double total_time_ms;
        size_t memory_usage_kb;
    };
    
    // [SEQUENCE: MVP19-68] Run comprehensive performance comparison
    static std::vector<BenchmarkResult> RunPerformanceComparison();
    
    // [SEQUENCE: MVP19-69] Individual test methods
    static BenchmarkResult TestCallbackApproach(size_t num_operations);
    static BenchmarkResult TestCoroutineApproach(size_t num_operations);
    static BenchmarkResult TestMemoryUsage();
    static BenchmarkResult TestConcurrentConnections(size_t num_connections);
    
    // [SEQUENCE: MVP19-70] Simulated operations for testing
    static boost::asio::awaitable<void> SimulateAsyncDatabaseCall();
    static boost::asio::awaitable<void> SimulateAsyncNetworkOperation();
    static boost::asio::awaitable<void> SimulateComplexCoroutineWorkflow();
    static boost::asio::awaitable<void> SimulateConcurrentUser(size_t user_id);
    
    // [SEQUENCE: MVP19-71] Helper methods
    static void PrintBenchmarkResults(const std::vector<BenchmarkResult>& results);
    static double CalculatePercentageImprovement(double baseline, double improved);
    
private:
    // [SEQUENCE: MVP19-72] Callback-style simulation
    static void SimulateCallbackChain(boost::asio::io_context& io, 
                                     std::function<void()> completion_handler);
    
    // [SEQUENCE: MVP19-73] Memory monitoring
    static size_t GetCurrentMemoryUsage();
    static void WarmupMemoryPools();
};

// [SEQUENCE: MVP19-74] Detailed performance metrics collector
class PerformanceMetrics {
public:
    void StartTimer(const std::string& operation);
    void EndTimer(const std::string& operation);
    
    void RecordLatency(const std::string& operation, double latency_ms);
    void RecordThroughput(const std::string& operation, size_t operations, double time_ms);
    
    void PrintReport() const;
    void SaveToFile(const std::string& filename) const;
    
private:
    struct TimingData {
        std::chrono::high_resolution_clock::time_point start_time;
        std::vector<double> latencies;
        double total_operations = 0;
        double total_time_ms = 0;
    };
    
    std::unordered_map<std::string, TimingData> metrics_;
    mutable std::mutex metrics_mutex_;
};

// [SEQUENCE: MVP19-75] Real-world scenario simulation
class ScenarioTester {
public:
    // [SEQUENCE: MVP19-76] Login flood simulation
    static boost::asio::awaitable<void> SimulateLoginFlood(size_t concurrent_users);
    
    // [SEQUENCE: MVP19-77] Mixed workload simulation
    static boost::asio::awaitable<void> SimulateMixedWorkload();
    
    // [SEQUENCE: MVP19-78] Stress test with error conditions
    static boost::asio::awaitable<void> SimulateStressWithErrors();
    
private:
    static boost::asio::awaitable<void> SimulateUser(size_t user_id);
    static boost::asio::awaitable<void> SimulateUserLogin(size_t user_id);
    static boost::asio::awaitable<void> SimulateUserActivity(size_t user_id);
};

} // namespace mmorpg::testing