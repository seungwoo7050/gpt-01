// [SEQUENCE: MVP19-64] Performance test runner for C++20 Coroutines
#include "coroutine_performance_test.h"
#include <spdlog/spdlog.h>
#include <iostream>

int main() {
    try {
        spdlog::set_level(spdlog::level::info);
        spdlog::info("=== Starting C++20 Coroutines Performance Analysis ===");
        
        // [SEQUENCE: MVP19-65] Run comprehensive performance comparison
        auto results = mmorpg::testing::CoroutinePerformanceTest::RunPerformanceComparison();
        
        // [SEQUENCE: MVP19-66] Summary analysis
        spdlog::info("\n=== Performance Analysis Complete ===");
        
        if (results.size() >= 2) {
            auto callback_result = results[0];
            auto coroutine_result = results[1];
            
            double latency_improvement = mmorpg::testing::CoroutinePerformanceTest::CalculatePercentageImprovement(
                callback_result.avg_latency_ms, coroutine_result.avg_latency_ms);
            
            double throughput_improvement = mmorpg::testing::CoroutinePerformanceTest::CalculatePercentageImprovement(
                callback_result.throughput_ops_per_sec, coroutine_result.throughput_ops_per_sec);
            
            spdlog::info("Key Findings:");
            spdlog::info("- Latency change: {:.1f}%", latency_improvement);
            spdlog::info("- Throughput change: {:.1f}%", throughput_improvement);
            
            if (throughput_improvement > 0) {
                spdlog::info("✅ Coroutines show performance improvement!");
            } else {
                spdlog::info("⚠️  Coroutines may have overhead in this scenario");
            }
        }
        
        spdlog::info("Performance testing completed successfully");
        return 0;
        
    } catch (const std::exception& e) {
        spdlog::error("Performance test failed: {}", e.what());
        return 1;
    }
}