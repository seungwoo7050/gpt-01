#include "metrics_collector.h"

namespace mmorpg::monitoring {

// [SEQUENCE: MVP1-47] `MetricsCollector::RecordGauge()`: 특정 시점의 값을 기록하는 게이지 타입 메트릭을 수집합니다.
void MetricsCollector::RecordGauge(const std::string& name, double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    gauges_[name] = value;
}

// [SEQUENCE: MVP1-48] `MetricsCollector::RecordCounter()`: 누적 값을 기록하는 카운터 타입 메트릭을 수집합니다.
void MetricsCollector::RecordCounter(const std::string& name, uint64_t value) {
    std::lock_guard<std::mutex> lock(mutex_);
    counters_[name] = value;
}

}