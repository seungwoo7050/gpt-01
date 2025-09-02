#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <atomic>
#include <thread>
#include <functional>
#include "../core/types.h"
#include "../core/singleton.h"
#include "../network/network_manager.h"
#include "../world/world_manager.h"

namespace mmorpg::testing {

// [SEQUENCE: MVP17-39] Performance test types 성능 테스트 타입
enum class TestType {
    LOAD_TEST,              // 부하 테스트
    STRESS_TEST,            // 스트레스 테스트
    SPIKE_TEST,             // 스파이크 테스트
    ENDURANCE_TEST,         // 내구성 테스트
    SCALABILITY_TEST,       // 확장성 테스트
    LATENCY_TEST,           // 지연 시간 테스트
    THROUGHPUT_TEST,        // 처리량 테스트
    CONCURRENCY_TEST        // 동시성 테스트
};

// [SEQUENCE: MVP17-40] Test scenario definition 테스트 시나리오 정의
struct TestScenario {
    std::string name;
    TestType type;
    uint32_t duration_seconds{300};      // 5분 기본
    uint32_t target_users{1000};         // 목표 사용자 수
    uint32_t ramp_up_seconds{60};        // 증가 시간
    uint32_t ramp_down_seconds{30};      // 감소 시간
    
    // 사용자 행동 패턴
    struct UserBehavior {
        float movement_rate{0.8f};        // 80% 이동
        float combat_rate{0.3f};          // 30% 전투
        float chat_rate{0.2f};            // 20% 채팅
        float trade_rate{0.1f};           // 10% 거래
        float skill_use_rate{0.4f};       // 40% 스킬 사용
    } behavior;
    
    // 성공 기준
    struct SuccessCriteria {
        float max_response_time_ms{100.0f};     // 최대 응답 시간
        float max_error_rate{0.01f};            // 최대 오류율 1%
        float min_throughput_rps{1000.0f};      // 최소 처리량
        float max_cpu_usage{80.0f};             // 최대 CPU 사용률
        float max_memory_usage_gb{12.0f};       // 최대 메모리 사용량
    } criteria;
};

// [SEQUENCE: MVP17-41] Performance metrics 성능 메트릭
struct PerformanceMetrics {
    // 응답 시간
    struct ResponseTime {
        std::atomic<double> min_ms{999999.0};
        std::atomic<double> max_ms{0.0};
        std::atomic<double> avg_ms{0.0};
        std::atomic<double> p50_ms{0.0};     // 중앙값
        std::atomic<double> p95_ms{0.0};     // 95 백분위
        std::atomic<double> p99_ms{0.0};     // 99 백분위
        std::atomic<uint64_t> total_requests{0};
    } response_time;
    
    // 처리량
    struct Throughput {
        std::atomic<uint64_t> requests_per_second{0};
        std::atomic<uint64_t> bytes_per_second{0};
        std::atomic<uint64_t> packets_per_second{0};
        std::atomic<uint64_t> transactions_per_second{0};
    } throughput;
    
    // 에러율
    struct ErrorRate {
        std::atomic<uint64_t> total_errors{0};
        std::atomic<uint64_t> timeout_errors{0};
        std::atomic<uint64_t> connection_errors{0};
        std::atomic<uint64_t> validation_errors{0};
        std::atomic<double> error_percentage{0.0};
    } errors;
    
    // 리소스 사용량
    struct ResourceUsage {
        std::atomic<double> cpu_usage_percent{0.0};
        std::atomic<double> memory_usage_gb{0.0};
        std::atomic<double> disk_io_mbps{0.0};
        std::atomic<double> network_io_mbps{0.0};
        std::atomic<uint32_t> thread_count{0};
        std::atomic<uint32_t> connection_count{0};
    } resources;
    
    // 게임 특화 메트릭
    struct GameMetrics {
        std::atomic<uint32_t> active_players{0};
        std::atomic<uint32_t> entities_processed{0};
        std::atomic<double> tick_rate_fps{0.0};
        std::atomic<uint64_t> combat_events_per_second{0};
        std::atomic<uint64_t> movement_updates_per_second{0};
        std::atomic<double> world_update_ms{0.0};
    } game;
};

// [SEQUENCE: MVP17-42] Virtual user for load testing 부하 테스트용 가상 사용자
class VirtualUser {
public:
    VirtualUser(uint64_t user_id, const TestScenario::UserBehavior& behavior);
    ~VirtualUser();
    
    // 사용자 액션
    void Connect(const std::string& server_address);
    void Disconnect();
    void Login(const std::string& username, const std::string& password);
    void SelectCharacter(uint32_t character_index);
    
    // 게임 액션
    void Move(const Vector3& direction);
    void Attack(uint64_t target_id);
    void UseSkill(uint32_t skill_id, uint64_t target_id);
    void SendChat(const std::string& message);
    void Trade(uint64_t target_player_id);
    
    // 자동 행동
    void StartBehaviorLoop();
    void StopBehaviorLoop();
    
    // 상태 조회
    bool IsConnected() const { return connected_; }
    uint64_t GetUserId() const { return user_id_; }
    std::chrono::milliseconds GetLatency() const { return latency_; }
    
private:
    uint64_t user_id_;
    TestScenario::UserBehavior behavior_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};
    
    // 네트워크
    std::unique_ptr<network::Connection> connection_;
    std::chrono::milliseconds latency_{0};
    
    // 상태
    Vector3 position_;
    uint32_t level_{1};
    float health_{100.0f};
    float mana_{100.0f};
    
    // 행동 루프
    std::thread behavior_thread_;
    void BehaviorLoop();
    void PerformRandomAction();
};

// [SEQUENCE: MVP17-43] Performance test framework 성능 테스트 프레임워크
class PerformanceTestFramework : public Singleton<PerformanceTestFramework> {
public:
    // [SEQUENCE: MVP17-44] Test execution 테스트 실행
    void RunTest(const TestScenario& scenario);
    void StopTest();
    bool IsTestRunning() const { return test_running_; }
    
    // 시나리오 관리
    void RegisterScenario(const TestScenario& scenario);
    void LoadScenariosFromFile(const std::string& filename);
    std::vector<TestScenario> GetAvailableScenarios() const;
    
    // 메트릭 수집
    PerformanceMetrics GetCurrentMetrics() const;
    void RecordResponseTime(double response_ms);
    void RecordError(const std::string& error_type);
    void RecordGameEvent(const std::string& event_type);
    
    // 리포트 생성
    void GenerateReport(const std::string& output_file);
    void GenerateHTMLReport(const std::string& output_file);
    void GenerateJSONReport(const std::string& output_file);
    
    // 실시간 모니터링
    void StartMetricsServer(uint16_t port = 9090);
    void StopMetricsServer();
    
private:
    friend class Singleton<PerformanceTestFramework>;
    PerformanceTestFramework() = default;
    
    // 테스트 상태
    std::atomic<bool> test_running_{false};
    TestScenario current_scenario_;
    std::chrono::steady_clock::time_point test_start_time_;
    
    // 가상 사용자
    std::vector<std::unique_ptr<VirtualUser>> virtual_users_;
    std::mutex users_mutex_;
    
    // 메트릭
    PerformanceMetrics metrics_;
    std::vector<double> response_times_;  // 백분위 계산용
    std::mutex metrics_mutex_;
    
    // 시나리오
    std::vector<TestScenario> scenarios_;
    
    // 테스트 실행
    void ExecuteLoadTest(const TestScenario& scenario);
    void ExecuteStressTest(const TestScenario& scenario);
    void ExecuteSpikeTest(const TestScenario& scenario);
    void ExecuteEnduranceTest(const TestScenario& scenario);
    
    // 사용자 관리
    void CreateVirtualUsers(uint32_t count, const TestScenario::UserBehavior& behavior);
    void RampUpUsers(uint32_t target_count, uint32_t duration_seconds);
    void RampDownUsers(uint32_t target_count, uint32_t duration_seconds);
    
    // 메트릭 수집
    void CollectSystemMetrics();
    void CalculatePercentiles();
    void UpdateThroughput();
};

// [SEQUENCE: MVP17-45] Load generator 부하 생성기
class LoadGenerator {
public:
    LoadGenerator(uint32_t thread_count = 4);
    ~LoadGenerator();
    
    // 부하 패턴
    enum class LoadPattern {
        CONSTANT,       // 일정한 부하
        RAMP_UP,        // 점진적 증가
        RAMP_DOWN,      // 점진적 감소
        SPIKE,          // 급격한 증가
        WAVE,           // 파도 패턴
        RANDOM          // 무작위 패턴
    };
    
    // 부하 생성
    void GenerateLoad(LoadPattern pattern, uint32_t target_rps, uint32_t duration_seconds);
    void StopGeneration();
    
    // 커스텀 부하 함수
    void SetLoadFunction(std::function<void()> load_func);
    
    // 통계
    struct LoadStats {
        uint64_t total_requests{0};
        uint64_t successful_requests{0};
        uint64_t failed_requests{0};
        double average_latency_ms{0.0};
    };
    
    LoadStats GetStats() const { return stats_; }
    
private:
    std::vector<std::thread> worker_threads_;
    std::atomic<bool> running_{false};
    LoadStats stats_;
    std::function<void()> load_function_;
    
    void WorkerLoop(uint32_t target_rps);
    uint32_t CalculateDelay(LoadPattern pattern, uint32_t elapsed_seconds);
};

// [SEQUENCE: MVP17-46] Benchmark suite 벤치마크 스위트
class BenchmarkSuite {
public:
    // 개별 벤치마크
    struct Benchmark {
        std::string name;
        std::function<void()> test_function;
        uint32_t iterations{1000};
        uint32_t warmup_iterations{100};
    };
    
    // 벤치마크 등록
    void RegisterBenchmark(const Benchmark& benchmark);
    
    // 실행
    void RunAll();
    void RunBenchmark(const std::string& name);
    
    // 결과
    struct BenchmarkResult {
        std::string name;
        double min_time_us;
        double max_time_us;
        double avg_time_us;
        double std_deviation_us;
        uint32_t iterations;
    };
    
    std::vector<BenchmarkResult> GetResults() const { return results_; }
    void PrintResults() const;
    void SaveResults(const std::string& filename) const;
    
private:
    std::vector<Benchmark> benchmarks_;
    std::vector<BenchmarkResult> results_;
    
    BenchmarkResult RunSingleBenchmark(const Benchmark& benchmark);
    double CalculateStandardDeviation(const std::vector<double>& times, double avg);
};

// [SEQUENCE: MVP17-47] Stress test scenarios 스트레스 테스트 시나리오
namespace StressTestScenarios {
    // 대규모 전투
    TestScenario CreateMassiveCombatScenario();
    
    // 동시 로그인
    TestScenario CreateLoginStormScenario();
    
    // 지역 과밀
    TestScenario CreateZoneCongestionScenario();
    
    // 시장 거래 폭주
    TestScenario CreateMarketCrashScenario();
    
    // 길드전
    TestScenario CreateGuildWarScenario();
    
    // 월드 이벤트
    TestScenario CreateWorldEventScenario();
}

// [SEQUENCE: MVP17-48] Performance monitoring 성능 모니터링
class PerformanceMonitor {
public:
    // 메트릭 수집 시작
    void StartMonitoring(std::chrono::milliseconds interval = std::chrono::milliseconds(1000));
    void StopMonitoring();
    
    // 실시간 대시보드
    void StartDashboard(uint16_t port = 8080);
    void StopDashboard();
    
    // 경고 설정
    struct AlertThreshold {
        double cpu_usage_percent{90.0};
        double memory_usage_gb{14.0};
        double response_time_ms{200.0};
        double error_rate_percent{5.0};
    };
    
    void SetAlertThresholds(const AlertThreshold& thresholds);
    void SetAlertCallback(std::function<void(const std::string&)> callback);
    
private:
    std::thread monitor_thread_;
    std::atomic<bool> monitoring_{false};
    AlertThreshold thresholds_;
    std::function<void(const std::string&)> alert_callback_;
    
    void MonitorLoop(std::chrono::milliseconds interval);
    void CheckThresholds(const PerformanceMetrics& metrics);
};

// [SEQUENCE: MVP17-49] Test utilities 테스트 유틸리티
namespace TestUtils {
    // 테스트 데이터 생성
    std::vector<std::string> GenerateRandomUsernames(uint32_t count);
    std::vector<Vector3> GenerateRandomPositions(uint32_t count, float range);
    std::vector<uint32_t> GenerateRandomSkillSequence(uint32_t count);
    
    // 통계 계산
    double CalculatePercentile(std::vector<double>& values, double percentile);
    double CalculateStandardDeviation(const std::vector<double>& values);
    double CalculateThroughput(uint64_t operations, double duration_seconds);
    
    // 리포트 포맷팅
    std::string FormatMetrics(const PerformanceMetrics& metrics);
    std::string FormatDuration(std::chrono::seconds duration);
    std::string FormatBytes(uint64_t bytes);
    
    // 검증
    bool ValidateTestResults(const PerformanceMetrics& metrics, 
                           const TestScenario::SuccessCriteria& criteria);
}

} // namespace mmorpg::testing