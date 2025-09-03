#pragma once

#include <chrono>
#include <vector>
#include <memory>
#include <iostream>
#include <boost/asio.hpp>

namespace mmorpg::testing {

// [SEQUENCE: 545] Performance comparison between callback and coroutine approaches
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
    
    // [SEQUENCE: 546] Run comprehensive performance comparison
    static std::vector<BenchmarkResult> RunPerformanceComparison();
    
    // [SEQUENCE: 547] Individual test methods
    static BenchmarkResult TestCallbackApproach(size_t num_operations);
    static BenchmarkResult TestCoroutineApproach(size_t num_operations);
    static BenchmarkResult TestMemoryUsage();
    static BenchmarkResult TestConcurrentConnections(size_t num_connections);
    
    // [SEQUENCE: 548] Simulated operations for testing
    static boost::asio::awaitable<void> SimulateAsyncDatabaseCall();
    static boost::asio::awaitable<void> SimulateAsyncNetworkOperation();
    static boost::asio::awaitable<void> SimulateComplexCoroutineWorkflow();
    static boost::asio::awaitable<void> SimulateConcurrentUser(size_t user_id);
    
    // [SEQUENCE: 549] Helper methods
    static void PrintBenchmarkResults(const std::vector<BenchmarkResult>& results);
    static double CalculatePercentageImprovement(double baseline, double improved);
    
private:
    // [SEQUENCE: 550] Callback-style simulation
    static void SimulateCallbackChain(boost::asio::io_context& io, 
                                     std::function<void()> completion_handler);
    
    // [SEQUENCE: 551] Memory monitoring
    static size_t GetCurrentMemoryUsage();
    static void WarmupMemoryPools();
};

// [SEQUENCE: 552] Detailed performance metrics collector
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

// [SEQUENCE: 553] Real-world scenario simulation
class ScenarioTester {
public:
    // [SEQUENCE: 554] Login flood simulation
    static boost::asio::awaitable<void> SimulateLoginFlood(size_t concurrent_users);
    
    // [SEQUENCE: 555] Mixed workload simulation
    static boost::asio::awaitable<void> SimulateMixedWorkload();
    
    // [SEQUENCE: 556] Stress test with error conditions
    static boost::asio::awaitable<void> SimulateStressWithErrors();
    
private:
    static boost::asio::awaitable<void> SimulateUser(size_t user_id);
    static boost::asio::awaitable<void> SimulateUserLogin(size_t user_id);
    static boost::asio::awaitable<void> SimulateUserActivity(size_t user_id);
};

} // namespace mmorpg::testing