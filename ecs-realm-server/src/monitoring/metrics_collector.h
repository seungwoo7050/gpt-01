#pragma once

#include <string>
#include <unordered_map>
#include <mutex>

namespace mmorpg::monitoring {

// [SEQUENCE: MVP1-46] Metrics Collector (`metrics_collector.h` & `.cpp`)
class MetricsCollector {
public:
    // [SEQUENCE: MVP1-47] `MetricsCollector::RecordGauge()`: 특정 시점의 값을 기록하는 게이지 타입 메트릭을 수집합니다.
    void RecordGauge(const std::string& name, double value);
    // [SEQUENCE: MVP1-48] `MetricsCollector::RecordCounter()`: 누적 값을 기록하는 카운터 타입 메트릭을 수집합니다.
    void RecordCounter(const std::string& name, uint64_t value);

private:
    std::unordered_map<std::string, double> gauges_;
    std::unordered_map<std::string, uint64_t> counters_;
    mutable std::mutex mutex_;
};

}