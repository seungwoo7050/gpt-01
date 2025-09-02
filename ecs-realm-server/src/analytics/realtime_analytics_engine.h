#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <queue>
#include <chrono>
#include <functional>
#include <future>
#include <cmath>
#include <numeric>
#include <variant>

namespace mmorpg::analytics {

// [SEQUENCE: MVP14-677] 실시간 분석 엔진
class RealtimeAnalyticsEngine {
public:
    struct MetricValue {
        std::variant<int64_t, double, std::string> value;
        std::chrono::steady_clock::time_point timestamp;
        std::unordered_map<std::string, std::string> tags;
        
        MetricValue(auto val) : value(val), timestamp(std::chrono::steady_clock::now()) {}
        
        template<typename T>
        T GetValue() const {
            return std::get<T>(value);
        }
    };

    struct EventData {
        std::string event_type;
        std::string source;
        std::unordered_map<std::string, MetricValue> properties;
        std::chrono::steady_clock::time_point timestamp;
        std::string session_id;
        std::string user_id;
        
        EventData(const std::string& type, const std::string& src)
            : event_type(type), source(src), timestamp(std::chrono::steady_clock::now()) {}
    };

    struct AggregatedMetric {
        std::string metric_name;
        double current_value{0.0};
        double min_value{std::numeric_limits<double>::max()};
        double max_value{std::numeric_limits<double>::lowest()};
        double sum_value{0.0};
        uint64_t count{0};
        double average{0.0};
        std::chrono::steady_clock::time_point last_updated;
        
        // 시계열 데이터 (최근 60개 값)
        std::vector<std::pair<std::chrono::steady_clock::time_point, double>> time_series;
        
        void Update(double value) {
            current_value = value;
            min_value = std::min(min_value, value);
            max_value = std::max(max_value, value);
            sum_value += value;
            count++;
            average = sum_value / count;
            last_updated = std::chrono::steady_clock::now();
            
            // 시계열 데이터 추가
            time_series.emplace_back(last_updated, value);
            if (time_series.size() > 60) {
                time_series.erase(time_series.begin());
            }
        }
    };

    struct AlertRule {
        std::string rule_id;
        std::string metric_name;
        std::string condition; // "greater_than", "less_than", "equals", "contains"
        double threshold_value;
        std::chrono::seconds evaluation_window{60};
        std::chrono::seconds cooldown_period{300}; // 5분 쿨다운
        std::function<void(const std::string&)> callback;
        std::chrono::steady_clock::time_point last_triggered;
        bool is_active{true};
    };

    explicit RealtimeAnalyticsEngine(size_t event_buffer_size = 100000)
        : max_buffer_size_(event_buffer_size), is_running_(false) {
        InitializeMetrics();
    }

    ~RealtimeAnalyticsEngine() {
        Shutdown();
    }

    // [SEQUENCE: MVP14-678] 이벤트 수집
    void RecordEvent(const EventData& event) {
        {
            std::lock_guard<std::mutex> lock(events_mutex_);
            
            event_buffer_.push(event);
            
            // 버퍼 크기 제한
            if (event_buffer_.size() > max_buffer_size_) {
                event_buffer_.pop();
            }
        }
        
        // 실시간 메트릭 업데이트
        UpdateMetricsFromEvent(event);
        
        // 알림 규칙 평가
        EvaluateAlertRules(event);
    }

    // [SEQUENCE: MVP14-679] 게임 메트릭 기록
    void RecordPlayerAction(const std::string& user_id, const std::string& action,
                           const std::unordered_map<std::string, std::variant<int64_t, double, std::string>>& properties = {}) {
        EventData event("player_action", "game_server");
        event.user_id = user_id;
        event.properties["action"] = MetricValue(action);
        
        for (const auto& [key, value] : properties) {
            event.properties[key] = MetricValue(value);
        }
        
        RecordEvent(event);
        
        // 플레이어별 활동 추적
        UpdatePlayerActivity(user_id, action);
    }

    void RecordServerMetric(const std::string& metric_name, double value, 
                           const std::unordered_map<std::string, std::string>& tags = {}) {
        EventData event("server_metric", "system");
        event.properties["metric"] = MetricValue(metric_name);
        event.properties["value"] = MetricValue(value);
        
        for (const auto& [key, tag_value] : tags) {
            event.properties[key] = MetricValue(tag_value);
        }
        
        RecordEvent(event);
        
        // 집계 메트릭 업데이트
        UpdateAggregatedMetric(metric_name, value);
    }

    void RecordPerformanceMetric(const std::string& component, double latency_ms,
                                double cpu_usage, double memory_usage) {
        EventData event("performance_metric", component);
        event.properties["latency_ms"] = MetricValue(latency_ms);
        event.properties["cpu_usage"] = MetricValue(cpu_usage);
        event.properties["memory_usage"] = MetricValue(memory_usage);
        
        RecordEvent(event);
        
        // 성능 지표 업데이트
        UpdateAggregatedMetric(component + "_latency", latency_ms);
        UpdateAggregatedMetric(component + "_cpu", cpu_usage);
        UpdateAggregatedMetric(component + "_memory", memory_usage);
    }

    // [SEQUENCE: MVP14-680] 실시간 대시보드 데이터
    struct DashboardData {
        // 실시간 플레이어 통계
        uint64_t active_players{0};
        uint64_t peak_concurrent_players{0};
        double average_session_duration_minutes{0.0};
        
        // 서버 성능
        double server_cpu_usage{0.0};
        double server_memory_usage{0.0};
        double average_latency_ms{0.0};
        uint64_t requests_per_second{0};
        
        // 게임 통계
        uint64_t total_battles_today{0};
        uint64_t total_logins_today{0};
        uint64_t revenue_today{0};
        std::unordered_map<std::string, uint64_t> popular_activities;
        
        // 시계열 차트 데이터
        std::vector<std::pair<std::chrono::steady_clock::time_point, double>> player_count_series;
        std::vector<std::pair<std::chrono::steady_clock::time_point, double>> latency_series;
        std::vector<std::pair<std::chrono::steady_clock::time_point, double>> cpu_usage_series;
        
        std::chrono::steady_clock::time_point last_updated;
    };

    DashboardData GetRealtimeDashboard() {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        
        DashboardData dashboard;
        dashboard.last_updated = std::chrono::steady_clock::now();
        
        // 기본 메트릭 수집
        if (auto it = aggregated_metrics_.find("active_players"); it != aggregated_metrics_.end()) {
            dashboard.active_players = static_cast<uint64_t>(it->second.current_value);
            dashboard.peak_concurrent_players = static_cast<uint64_t>(it->second.max_value);
            dashboard.player_count_series = it->second.time_series;
        }
        
        if (auto it = aggregated_metrics_.find("server_cpu"); it != aggregated_metrics_.end()) {
            dashboard.server_cpu_usage = it->second.current_value;
            dashboard.cpu_usage_series = it->second.time_series;
        }
        
        if (auto it = aggregated_metrics_.find("server_memory"); it != aggregated_metrics_.end()) {
            dashboard.server_memory_usage = it->second.current_value;
        }
        
        if (auto it = aggregated_metrics_.find("game_server_latency"); it != aggregated_metrics_.end()) {
            dashboard.average_latency_ms = it->second.average;
            dashboard.latency_series = it->second.time_series;
        }
        
        // 플레이어 활동 통계
        dashboard.popular_activities = GetPopularActivities();
        dashboard.total_battles_today = GetDailyMetric("battles");
        dashboard.total_logins_today = GetDailyMetric("logins");
        
        return dashboard;
    }

    // [SEQUENCE: MVP14-681] 알림 및 임계값 모니터링
    void AddAlertRule(const std::string& rule_id, const std::string& metric_name,
                     const std::string& condition, double threshold,
                     std::function<void(const std::string&)> callback) {
        std::lock_guard<std::mutex> lock(alerts_mutex_);
        
        AlertRule rule;
        rule.rule_id = rule_id;
        rule.metric_name = metric_name;
        rule.condition = condition;
        rule.threshold_value = threshold;
        rule.callback = std::move(callback);
        rule.last_triggered = std::chrono::steady_clock::now() - std::chrono::hours(1); // 초기값
        
        alert_rules_[rule_id] = rule;
    }

    void RemoveAlertRule(const std::string& rule_id) {
        std::lock_guard<std::mutex> lock(alerts_mutex_);
        alert_rules_.erase(rule_id);
    }

    // [SEQUENCE: MVP14-682] 고급 분석 기능
    struct TrendAnalysis {
        std::string metric_name;
        double trend_coefficient; // 양수: 증가 추세, 음수: 감소 추세
        double correlation_score; // -1 ~ 1
        std::string trend_description;
        std::vector<double> forecasted_values; // 다음 N개 값 예측
    };

    TrendAnalysis AnalyzeTrend(const std::string& metric_name, size_t window_size = 30) {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        
        TrendAnalysis analysis;
        analysis.metric_name = metric_name;
        
        auto it = aggregated_metrics_.find(metric_name);
        if (it == aggregated_metrics_.end() || it->second.time_series.size() < window_size) {
            analysis.trend_coefficient = 0.0;
            analysis.correlation_score = 0.0;
            analysis.trend_description = "Insufficient data";
            return analysis;
        }
        
        const auto& series = it->second.time_series;
        size_t start_idx = series.size() - window_size;
        
        // 선형 회귀로 추세 분석
        std::vector<double> x_values, y_values;
        for (size_t i = start_idx; i < series.size(); ++i) {
            x_values.push_back(static_cast<double>(i - start_idx));
            y_values.push_back(series[i].second);
        }
        
        auto [slope, intercept, correlation] = CalculateLinearRegression(x_values, y_values);
        
        analysis.trend_coefficient = slope;
        analysis.correlation_score = correlation;
        
        // 추세 설명
        if (std::abs(slope) < 0.01) {
            analysis.trend_description = "Stable";
        } else if (slope > 0) {
            analysis.trend_description = "Increasing (" + std::to_string(slope) + "/unit)";
        } else {
            analysis.trend_description = "Decreasing (" + std::to_string(slope) + "/unit)";
        }
        
        // 간단한 예측 (선형 외삽)
        for (int i = 1; i <= 10; ++i) {
            double predicted = intercept + slope * (window_size + i);
            analysis.forecasted_values.push_back(predicted);
        }
        
        return analysis;
    }

    // [SEQUENCE: MVP14-683] 이상 탐지
    struct AnomalyDetection {
        std::string metric_name;
        bool is_anomaly{false};
        double anomaly_score{0.0}; // 0-1, 1에 가까울수록 이상
        double expected_value{0.0};
        double actual_value{0.0};
        std::string anomaly_type; // "spike", "drop", "pattern_break"
        std::chrono::steady_clock::time_point detected_at;
    };

    AnomalyDetection DetectAnomaly(const std::string& metric_name) {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        
        AnomalyDetection result;
        result.metric_name = metric_name;
        result.detected_at = std::chrono::steady_clock::now();
        
        auto it = aggregated_metrics_.find(metric_name);
        if (it == aggregated_metrics_.end() || it->second.time_series.size() < 10) {
            return result;
        }
        
        const auto& series = it->second.time_series;
        result.actual_value = series.back().second;
        
        // 최근 값들의 평균과 표준편차 계산
        std::vector<double> recent_values;
        for (size_t i = std::max(0, static_cast<int>(series.size()) - 20); i < series.size() - 1; ++i) {
            recent_values.push_back(series[i].second);
        }
        
        double mean = std::accumulate(recent_values.begin(), recent_values.end(), 0.0) / recent_values.size();
        double variance = 0.0;
        for (double val : recent_values) {
            variance += (val - mean) * (val - mean);
        }
        variance /= recent_values.size();
        double std_dev = std::sqrt(variance);
        
        result.expected_value = mean;
        
        // Z-score 기반 이상 탐지
        double z_score = std::abs(result.actual_value - mean) / (std_dev + 1e-8);
        result.anomaly_score = std::min(1.0, z_score / 3.0); // 3-sigma 규칙 기준
        
        if (z_score > 2.5) { // 2.5 표준편차 이상
            result.is_anomaly = true;
            
            if (result.actual_value > mean + 2.5 * std_dev) {
                result.anomaly_type = "spike";
            } else if (result.actual_value < mean - 2.5 * std_dev) {
                result.anomaly_type = "drop";
            } else {
                result.anomaly_type = "pattern_break";
            }
        }
        
        return result;
    }

    // [SEQUENCE: MVP14-684] 데이터 내보내기 및 보고서
    std::string GenerateReport(const std::string& format = "json", 
                              std::chrono::hours time_range = std::chrono::hours(24)) {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        
        auto cutoff_time = std::chrono::steady_clock::now() - time_range;
        
        if (format == "json") {
            return GenerateJSONReport(cutoff_time);
        } else if (format == "csv") {
            return GenerateCSVReport(cutoff_time);
        } else {
            return GenerateTextReport(cutoff_time);
        }
    }

    void StartAnalyticsEngine() {
        if (is_running_) return;
        
        is_running_ = true;
        
        // 주기적 집계 스레드
        aggregation_thread_ = std::thread([this]() {
            while (is_running_) {
                PerformPeriodicAggregation();
                std::this_thread::sleep_for(std::chrono::seconds(10));
            }
        });
        
        // 이상 탐지 스레드
        anomaly_thread_ = std::thread([this]() {
            while (is_running_) {
                PerformAnomalyDetection();
                std::this_thread::sleep_for(std::chrono::seconds(30));
            }
        });
    }

    void Shutdown() {
        is_running_ = false;
        
        if (aggregation_thread_.joinable()) {
            aggregation_thread_.join();
        }
        
        if (anomaly_thread_.joinable()) {
            anomaly_thread_.join();
        }
    }

private:
    size_t max_buffer_size_;
    std::atomic<bool> is_running_;
    
    // 이벤트 저장
    std::queue<EventData> event_buffer_;
    std::mutex events_mutex_;
    
    // 집계 메트릭
    std::unordered_map<std::string, AggregatedMetric> aggregated_metrics_;
    std::mutex metrics_mutex_;
    
    // 플레이어 활동 추적
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> player_last_activity_;
    std::unordered_map<std::string, uint64_t> daily_metrics_;
    std::unordered_map<std::string, uint64_t> activity_counts_;
    
    // 알림 규칙
    std::unordered_map<std::string, AlertRule> alert_rules_;
    std::mutex alerts_mutex_;
    
    // 백그라운드 스레드
    std::thread aggregation_thread_;
    std::thread anomaly_thread_;

    // [SEQUENCE: MVP14-685] 내부 유틸리티 함수
    void InitializeMetrics() {
        std::vector<std::string> default_metrics = {
            "active_players", "server_cpu", "server_memory", 
            "game_server_latency", "database_latency", "cache_hit_rate"
        };
        
        for (const auto& metric : default_metrics) {
            aggregated_metrics_[metric] = AggregatedMetric{};
            aggregated_metrics_[metric].metric_name = metric;
        }
    }

    void UpdateMetricsFromEvent(const EventData& event) {
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        
        if (event.event_type == "player_action") {
            // 활성 플레이어 수 업데이트
            player_last_activity_[event.user_id] = event.timestamp;
            
            // 현재 활성 플레이어 수 계산
            auto now = std::chrono::steady_clock::now();
            uint64_t active_count = 0;
            
            for (auto it = player_last_activity_.begin(); it != player_last_activity_.end();) {
                if (now - it->second > std::chrono::minutes(5)) {
                    it = player_last_activity_.erase(it);
                } else {
                    active_count++;
                    ++it;
                }
            }
            
            UpdateAggregatedMetric("active_players", static_cast<double>(active_count));
            
        } else if (event.event_type == "server_metric") {
            auto metric_it = event.properties.find("metric");
            auto value_it = event.properties.find("value");
            
            if (metric_it != event.properties.end() && value_it != event.properties.end()) {
                std::string metric_name = metric_it->second.GetValue<std::string>();
                double value = value_it->second.GetValue<double>();
                UpdateAggregatedMetric(metric_name, value);
            }
        }
    }

    void UpdateAggregatedMetric(const std::string& name, double value) {
        auto& metric = aggregated_metrics_[name];
        metric.metric_name = name;
        metric.Update(value);
    }

    void UpdatePlayerActivity(const std::string& user_id, const std::string& action) {
        activity_counts_[action]++;
        
        if (action == "login") {
            daily_metrics_["logins"]++;
        } else if (action == "battle_start") {
            daily_metrics_["battles"]++;
        }
    }

    void EvaluateAlertRules(const EventData& event) {
        std::lock_guard<std::mutex> lock(alerts_mutex_);
        
        auto now = std::chrono::steady_clock::now();
        
        for (auto& [rule_id, rule] : alert_rules_) {
            if (!rule.is_active) continue;
            
            // 쿨다운 체크
            if (now - rule.last_triggered < rule.cooldown_period) {
                continue;
            }
            
            // 메트릭 값 확인
            auto metric_it = aggregated_metrics_.find(rule.metric_name);
            if (metric_it == aggregated_metrics_.end()) {
                continue;
            }
            
            double current_value = metric_it->second.current_value;
            bool should_trigger = false;
            
            if (rule.condition == "greater_than" && current_value > rule.threshold_value) {
                should_trigger = true;
            } else if (rule.condition == "less_than" && current_value < rule.threshold_value) {
                should_trigger = true;
            } else if (rule.condition == "equals" && std::abs(current_value - rule.threshold_value) < 0.001) {
                should_trigger = true;
            }
            
            if (should_trigger && rule.callback) {
                std::string alert_message = "Alert [" + rule_id + "]: " + 
                                          rule.metric_name + " = " + std::to_string(current_value) +
                                          " (threshold: " + std::to_string(rule.threshold_value) + ")";
                
                rule.callback(alert_message);
                rule.last_triggered = now;
            }
        }
    }

    std::unordered_map<std::string, uint64_t> GetPopularActivities() {
        std::unordered_map<std::string, uint64_t> result;
        
        // 최대 5개 인기 활동 반환
        std::vector<std::pair<std::string, uint64_t>> sorted_activities(activity_counts_.begin(), activity_counts_.end());
        std::sort(sorted_activities.begin(), sorted_activities.end(),
                 [](const auto& a, const auto& b) { return a.second > b.second; });
        
        for (size_t i = 0; i < std::min(5ul, sorted_activities.size()); ++i) {
            result[sorted_activities[i].first] = sorted_activities[i].second;
        }
        
        return result;
    }

    uint64_t GetDailyMetric(const std::string& metric_name) {
        auto it = daily_metrics_.find(metric_name);
        return (it != daily_metrics_.end()) ? it->second : 0;
    }

    // [SEQUENCE: MVP14-686] 통계 분석 함수
    std::tuple<double, double, double> CalculateLinearRegression(const std::vector<double>& x, const std::vector<double>& y) {
        size_t n = x.size();
        if (n != y.size() || n < 2) {
            return {0.0, 0.0, 0.0};
        }
        
        double sum_x = std::accumulate(x.begin(), x.end(), 0.0);
        double sum_y = std::accumulate(y.begin(), y.end(), 0.0);
        double sum_xy = 0.0, sum_xx = 0.0, sum_yy = 0.0;
        
        for (size_t i = 0; i < n; ++i) {
            sum_xy += x[i] * y[i];
            sum_xx += x[i] * x[i];
            sum_yy += y[i] * y[i];
        }
        
        double slope = (n * sum_xy - sum_x * sum_y) / (n * sum_xx - sum_x * sum_x);
        double intercept = (sum_y - slope * sum_x) / n;
        
        // 상관계수 계산
        double correlation = (n * sum_xy - sum_x * sum_y) / 
                           std::sqrt((n * sum_xx - sum_x * sum_x) * (n * sum_yy - sum_y * sum_y));
        
        return {slope, intercept, correlation};
    }

    void PerformPeriodicAggregation() {
        // 주기적으로 집계 데이터 계산 및 정리
        std::lock_guard<std::mutex> lock(metrics_mutex_);
        
        auto now = std::chrono::steady_clock::now();
        auto cutoff = now - std::chrono::hours(1);
        
        // 오래된 시계열 데이터 정리
        for (auto& [name, metric] : aggregated_metrics_) {
            auto& series = metric.time_series;
            series.erase(
                std::remove_if(series.begin(), series.end(),
                    [cutoff](const auto& point) { return point.first < cutoff; }),
                series.end()
            );
        }
    }

    void PerformAnomalyDetection() {
        std::vector<std::string> metrics_to_check = {
            "active_players", "server_cpu", "server_memory", "game_server_latency"
        };
        
        for (const auto& metric_name : metrics_to_check) {
            auto anomaly = DetectAnomaly(metric_name);
            if (anomaly.is_anomaly) {
                // 이상 탐지 시 로깅 또는 알림
                // 실제 구현에서는 로깅 시스템에 기록
            }
        }
    }

    std::string GenerateJSONReport(std::chrono::steady_clock::time_point cutoff) {
        // JSON 형태의 보고서 생성 (시뮬레이션)
        return "{ \"report\": \"analytics_summary\", \"generated_at\": \"" + 
               std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "\" }";
    }

    std::string GenerateCSVReport(std::chrono::steady_clock::time_point cutoff) {
        // CSV 형태의 보고서 생성 (시뮬레이션)
        return "metric_name,current_value,avg_value,min_value,max_value\n";
    }

    std::string GenerateTextReport(std::chrono::steady_clock::time_point cutoff) {
        // 텍스트 형태의 보고서 생성 (시뮬레이션)
        return "Analytics Report - Generated at: " + 
               std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    }
};

} // namespace mmorpg::analytics
