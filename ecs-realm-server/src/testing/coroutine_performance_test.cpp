#include "coroutine_performance_test.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <numeric>
#include <algorithm>

namespace mmorpg::testing {

// [SEQUENCE: MVP19-79] Main performance comparison runner
std::vector<CoroutinePerformanceTest::BenchmarkResult> 
CoroutinePerformanceTest::RunPerformanceComparison() {
    
    spdlog::info("Starting C++20 Coroutines vs Callbacks Performance Comparison");
    
    std::vector<BenchmarkResult> results;
    
    // [SEQUENCE: MVP19-80] Warmup
    spdlog::info("Warming up memory pools...");
    WarmupMemoryPools();
    
    // [SEQUENCE: MVP19-81] Test 1: Basic operations comparison
    spdlog::info("Test 1: Basic Operations (10,000 ops)");
    results.push_back(TestCallbackApproach(10000));
    results.push_back(TestCoroutineApproach(10000));
    
    // [SEQUENCE: MVP19-82] Test 2: Memory usage comparison
    spdlog::info("Test 2: Memory Usage Comparison");
    results.push_back(TestMemoryUsage());
    
    // [SEQUENCE: MVP19-83] Test 3: Concurrent connections
    spdlog::info("Test 3: Concurrent Connections (1,000 connections)");
    results.push_back(TestConcurrentConnections(1000));
    
    // [SEQUENCE: MVP19-84] Print comprehensive results
    PrintBenchmarkResults(results);
    
    return results;
}

// [SEQUENCE: MVP19-85] Callback approach benchmark
CoroutinePerformanceTest::BenchmarkResult 
CoroutinePerformanceTest::TestCallbackApproach(size_t num_operations) {
    
    BenchmarkResult result;
    result.test_name = "Callback Approach";
    result.total_operations = num_operations;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    size_t completed_operations = 0;
    std::vector<double> latencies;
    
    boost::asio::io_context io;
    
    // [SEQUENCE: MVP19-86] Simulate callback-based operations
    for (size_t i = 0; i < num_operations; ++i) {
        auto op_start = std::chrono::high_resolution_clock::now();
        
        SimulateCallbackChain(io, [&]() {
            auto op_end = std::chrono::high_resolution_clock::now();
            auto latency = std::chrono::duration<double, std::milli>(op_end - op_start).count();
            latencies.push_back(latency);
            completed_operations++;
        });
    }
    
    // [SEQUENCE: MVP19-87] Run the io_context
    io.run();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.total_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    // [SEQUENCE: MVP19-88] Calculate statistics
    if (!latencies.empty()) {
        result.avg_latency_ms = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
        result.min_latency_ms = *std::min_element(latencies.begin(), latencies.end());
        result.max_latency_ms = *std::max_element(latencies.begin(), latencies.end());
    }
    
    result.throughput_ops_per_sec = (completed_operations * 1000.0) / result.total_time_ms;
    result.memory_usage_kb = GetCurrentMemoryUsage();
    
    spdlog::info("Callback test completed: {} ops in {:.2f}ms", 
                completed_operations, result.total_time_ms);
    
    return result;
}

// [SEQUENCE: MVP19-89] Coroutine approach benchmark
CoroutinePerformanceTest::BenchmarkResult 
CoroutinePerformanceTest::TestCoroutineApproach(size_t num_operations) {
    
    BenchmarkResult result;
    result.test_name = "Coroutine Approach";
    result.total_operations = num_operations;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    std::vector<double> latencies;
    
    boost::asio::io_context io;
    
    // [SEQUENCE: MVP19-90] Launch coroutine operations
    auto task = [&]() -> boost::asio::awaitable<void> {
        for (size_t i = 0; i < num_operations; ++i) {
            auto op_start = std::chrono::high_resolution_clock::now();
            
            // [SEQUENCE: MVP19-91] Simulate complex async workflow
            co_await SimulateComplexCoroutineWorkflow();
            
            auto op_end = std::chrono::high_resolution_clock::now();
            auto latency = std::chrono::duration<double, std::milli>(op_end - op_start).count();
            latencies.push_back(latency);
        }
    };
    
    boost::asio::co_spawn(io, task(), boost::asio::detached);
    io.run();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.total_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    // [SEQUENCE: MVP19-92] Calculate statistics
    if (!latencies.empty()) {
        result.avg_latency_ms = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
        result.min_latency_ms = *std::min_element(latencies.begin(), latencies.end());
        result.max_latency_ms = *std::max_element(latencies.begin(), latencies.end());
    }
    
    result.throughput_ops_per_sec = (num_operations * 1000.0) / result.total_time_ms;
    result.memory_usage_kb = GetCurrentMemoryUsage();
    
    spdlog::info("Coroutine test completed: {} ops in {:.2f}ms", 
                num_operations, result.total_time_ms);
    
    return result;
}

// [SEQUENCE: MVP19-93] Memory usage benchmark
CoroutinePerformanceTest::BenchmarkResult 
CoroutinePerformanceTest::TestMemoryUsage() {
    
    BenchmarkResult result;
    result.test_name = "Memory Usage Comparison";
    
    // [SEQUENCE: MVP19-94] Measure memory before test
    size_t baseline_memory = GetCurrentMemoryUsage();
    
    boost::asio::io_context io;
    
    // [SEQUENCE: MVP19-95] Create many coroutines to test memory overhead
    auto memory_test = [&]() -> boost::asio::awaitable<void> {
        std::vector<boost::asio::awaitable<void>> tasks;
        
        for (int i = 0; i < 1000; ++i) {
            tasks.push_back(SimulateAsyncDatabaseCall());
        }
        
        // Wait for all tasks (simulating many concurrent operations)
        for (auto& task : tasks) {
            co_await std::move(task);
        }
    };
    
    auto start_time = std::chrono::high_resolution_clock::now();
    boost::asio::co_spawn(io, memory_test(), boost::asio::detached);
    io.run();
    auto end_time = std::chrono::high_resolution_clock::now();
    
    // [SEQUENCE: MVP19-96] Calculate memory difference
    size_t final_memory = GetCurrentMemoryUsage();
    result.memory_usage_kb = final_memory - baseline_memory;
    result.total_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    result.total_operations = 1000;
    result.throughput_ops_per_sec = (1000 * 1000.0) / result.total_time_ms;
    
    spdlog::info("Memory test completed: {}KB additional memory used", 
                result.memory_usage_kb);
    
    return result;
}

// [SEQUENCE: MVP19-97] Simulated async database call
boost::asio::awaitable<void> CoroutinePerformanceTest::SimulateAsyncDatabaseCall() {
    auto executor = co_await boost::asio::this_coro::executor;
    auto timer = boost::asio::steady_timer(executor);
    
    // [SEQUENCE: MVP19-98] Simulate realistic database latency
    timer.expires_after(std::chrono::microseconds(100 + (rand() % 200))); // 100-300μs
    co_await timer.async_wait(boost::asio::use_awaitable);
}

// [SEQUENCE: MVP19-99] Simulated network operation
boost::asio::awaitable<void> CoroutinePerformanceTest::SimulateAsyncNetworkOperation() {
    auto executor = co_await boost::asio::this_coro::executor;
    auto timer = boost::asio::steady_timer(executor);
    
    // [SEQUENCE: MVP19-100] Simulate network I/O latency
    timer.expires_after(std::chrono::microseconds(50 + (rand() % 100))); // 50-150μs
    co_await timer.async_wait(boost::asio::use_awaitable);
}

// [SEQUENCE: MVP19-101] Complex coroutine workflow simulation
boost::asio::awaitable<void> CoroutinePerformanceTest::SimulateComplexCoroutineWorkflow() {
    // [SEQUENCE: MVP19-102] Simulate a complex authentication workflow
    co_await SimulateAsyncDatabaseCall();    // User lookup
    co_await SimulateAsyncNetworkOperation(); // Rate limit check
    co_await SimulateAsyncDatabaseCall();    // Session creation
    co_await SimulateAsyncNetworkOperation(); // Response send
}

// [SEQUENCE: MVP19-103] Callback chain simulation
void CoroutinePerformanceTest::SimulateCallbackChain(
    boost::asio::io_context& io, 
    std::function<void()> completion_handler) {
    
    // [SEQUENCE: MVP19-104] Simulate nested callbacks (callback hell)
    auto timer1 = std::make_shared<boost::asio::steady_timer>(io);
    timer1->expires_after(std::chrono::microseconds(100));
    
    timer1->async_wait([timer1, &io, completion_handler](boost::system::error_code ec) {
        if (!ec) {
            auto timer2 = std::make_shared<boost::asio::steady_timer>(io);
            timer2->expires_after(std::chrono::microseconds(50));
            
            timer2->async_wait([timer2, &io, completion_handler](boost::system::error_code ec) {
                if (!ec) {
                    auto timer3 = std::make_shared<boost::asio::steady_timer>(io);
                    timer3->expires_after(std::chrono::microseconds(100));
                    
                    timer3->async_wait([timer3, completion_handler](boost::system::error_code ec) {
                        if (!ec) {
                            completion_handler();
                        }
                    });
                }
            });
        }
    });
}

// [SEQUENCE: MVP19-105] Print benchmark results
void CoroutinePerformanceTest::PrintBenchmarkResults(
    const std::vector<BenchmarkResult>& results) {
    
    spdlog::info("=== C++20 Coroutines Performance Test Results ===");
    
    for (const auto& result : results) {
        spdlog::info("Test: {}", result.test_name);
        spdlog::info("  Operations: {}", result.total_operations);
        spdlog::info("  Total Time: {:.2f} ms", result.total_time_ms);
        spdlog::info("  Avg Latency: {:.3f} ms", result.avg_latency_ms);
        spdlog::info("  Min Latency: {:.3f} ms", result.min_latency_ms);
        spdlog::info("  Max Latency: {:.3f} ms", result.max_latency_ms);
        spdlog::info("  Throughput: {:.1f} ops/sec", result.throughput_ops_per_sec);
        spdlog::info("  Memory: {} KB", result.memory_usage_kb);
        spdlog::info("---");
    }
    
    // [SEQUENCE: MVP19-106] Calculate improvements
    if (results.size() >= 2) {
        auto callback_result = results[0];
        auto coroutine_result = results[1];
        
        double latency_improvement = CalculatePercentageImprovement(
            callback_result.avg_latency_ms, coroutine_result.avg_latency_ms);
        
        double throughput_improvement = CalculatePercentageImprovement(
            callback_result.throughput_ops_per_sec, coroutine_result.throughput_ops_per_sec);
        
        spdlog::info("=== Performance Comparison ===");
        spdlog::info("Latency improvement: {:.1f}%", latency_improvement);
        spdlog::info("Throughput improvement: {:.1f}%", throughput_improvement);
    }
}

// [SEQUENCE: MVP19-107] Memory usage estimation (simplified)
size_t CoroutinePerformanceTest::GetCurrentMemoryUsage() {
    // [SEQUENCE: MVP19-108] Simple memory estimation
    // In real implementation, this would use platform-specific APIs
    static size_t simulated_memory = 1024; // Start with 1MB baseline
    simulated_memory += rand() % 100;      // Add some variation
    return simulated_memory;
}

// [SEQUENCE: MVP19-109] Memory pool warmup
void CoroutinePerformanceTest::WarmupMemoryPools() {
    // [SEQUENCE: MVP19-110] Simulate memory pool warmup
    boost::asio::io_context io;
    auto warmup_task = [&]() -> boost::asio::awaitable<void> {
        for (int i = 0; i < 100; ++i) {
            co_await SimulateAsyncDatabaseCall();
        }
    };
    
    boost::asio::co_spawn(io, warmup_task(), boost::asio::detached);
    io.run();
}

// [SEQUENCE: MVP19-111] Concurrent connections test
CoroutinePerformanceTest::BenchmarkResult 
CoroutinePerformanceTest::TestConcurrentConnections(size_t num_connections) {
    
    BenchmarkResult result;
    result.test_name = "Concurrent Connections";
    result.total_operations = num_connections;
    
    boost::asio::io_context io;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // [SEQUENCE: MVP19-112] Launch multiple concurrent coroutines
    auto concurrent_test = [&]() -> boost::asio::awaitable<void> {
        std::vector<boost::asio::awaitable<void>> tasks;
        
        for (size_t i = 0; i < num_connections; ++i) {
            tasks.push_back(SimulateConcurrentUser(i));
        }
        
        // [SEQUENCE: MVP19-113] Wait for all connections to complete
        for (auto& task : tasks) {
            co_await std::move(task);
        }
    };
    
    boost::asio::co_spawn(io, concurrent_test(), boost::asio::detached);
    io.run();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.total_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    result.throughput_ops_per_sec = (num_connections * 1000.0) / result.total_time_ms;
    result.memory_usage_kb = GetCurrentMemoryUsage();
    
    spdlog::info("Concurrent connections test completed: {} connections in {:.2f}ms", 
                num_connections, result.total_time_ms);
    
    return result;
}

// [SEQUENCE: MVP19-114] Simulate concurrent user activity
boost::asio::awaitable<void> CoroutinePerformanceTest::SimulateConcurrentUser(size_t user_id) {
    // [SEQUENCE: MVP19-115] Simulate user login process
    co_await SimulateAsyncDatabaseCall();    // User authentication
    co_await SimulateAsyncNetworkOperation(); // Session creation
    
    // [SEQUENCE: MVP19-116] Simulate some user activity
    for (int i = 0; i < 5; ++i) {
        co_await SimulateAsyncNetworkOperation(); // Various user actions
        
        auto executor = co_await boost::asio::this_coro::executor;
        auto timer = boost::asio::steady_timer(executor);
        timer.expires_after(std::chrono::microseconds(10 + (rand() % 20)));
        co_await timer.async_wait(boost::asio::use_awaitable);
    }
}

// [SEQUENCE: MVP19-117] Percentage improvement calculation
double CoroutinePerformanceTest::CalculatePercentageImprovement(
    double baseline, double improved) {
    
    if (baseline == 0) return 0;
    return ((improved - baseline) / baseline) * 100.0;
}

} // namespace mmorpg::testing