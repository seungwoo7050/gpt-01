#include "performance_testing.h"
#include "../core/logger.h"
#include "../optimization/final_optimization.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <numeric>
#include <fstream>
#include <iomanip>
#include <random>
#include <cmath>

namespace mmorpg::testing {

// [SEQUENCE: 3812] Virtual user implementation 가상 사용자 구현
VirtualUser::VirtualUser(uint64_t user_id, const TestScenario::UserBehavior& behavior)
    : user_id_(user_id), behavior_(behavior) {
    
    // 초기 위치 설정
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(-100.0f, 100.0f);
    
    position_ = Vector3(dist(gen), 0.0f, dist(gen));
    level_ = 1 + (user_id % 50);  // 1-50 레벨
    
    spdlog::debug("[VirtualUser] Created user {} at position ({}, {}, {})",
                  user_id_, position_.x, position_.y, position_.z);
}

VirtualUser::~VirtualUser() {
    if (running_) {
        StopBehaviorLoop();
    }
    if (connected_) {
        Disconnect();
    }
}

void VirtualUser::Connect(const std::string& server_address) {
    try {
        connection_ = std::make_unique<network::Connection>();
        connection_->Connect(server_address);
        connected_ = true;
        
        spdlog::debug("[VirtualUser] User {} connected to {}", user_id_, server_address);
    } catch (const std::exception& e) {
        spdlog::error("[VirtualUser] User {} connection failed: {}", user_id_, e.what());
        throw;
    }
}

void VirtualUser::Disconnect() {
    if (connected_ && connection_) {
        connection_->Disconnect();
        connected_ = false;
        spdlog::debug("[VirtualUser] User {} disconnected", user_id_);
    }
}

void VirtualUser::Login(const std::string& username, const std::string& password) {
    if (!connected_) {
        throw std::runtime_error("Not connected");
    }
    
    // 로그인 패킷 전송
    network::Packet login_packet(network::PacketType::LOGIN_REQUEST);
    login_packet.WriteString(username);
    login_packet.WriteString(password);
    
    auto start = std::chrono::steady_clock::now();
    connection_->Send(login_packet);
    
    // 응답 대기
    auto response = connection_->Receive(std::chrono::seconds(5));
    auto end = std::chrono::steady_clock::now();
    
    latency_ = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    spdlog::debug("[VirtualUser] User {} logged in, latency: {}ms", 
                  user_id_, latency_.count());
}

void VirtualUser::Move(const Vector3& direction) {
    if (!connected_) return;
    
    // 위치 업데이트
    position_ += direction;
    
    // 이동 패킷 전송
    network::Packet move_packet(network::PacketType::MOVEMENT_UPDATE);
    move_packet.Write(position_);
    move_packet.Write(direction);
    
    connection_->Send(move_packet);
}

void VirtualUser::Attack(uint64_t target_id) {
    if (!connected_) return;
    
    network::Packet attack_packet(network::PacketType::COMBAT_ACTION);
    attack_packet.Write(static_cast<uint8_t>(0));  // 기본 공격
    attack_packet.Write(target_id);
    
    connection_->Send(attack_packet);
}

void VirtualUser::UseSkill(uint32_t skill_id, uint64_t target_id) {
    if (!connected_) return;
    
    network::Packet skill_packet(network::PacketType::SKILL_USE);
    skill_packet.Write(skill_id);
    skill_packet.Write(target_id);
    skill_packet.Write(position_);
    
    connection_->Send(skill_packet);
}

void VirtualUser::StartBehaviorLoop() {
    running_ = true;
    behavior_thread_ = std::thread(&VirtualUser::BehaviorLoop, this);
}

void VirtualUser::StopBehaviorLoop() {
    running_ = false;
    if (behavior_thread_.joinable()) {
        behavior_thread_.join();
    }
}

void VirtualUser::BehaviorLoop() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    
    while (running_ && connected_) {
        // 무작위 행동 수행
        PerformRandomAction();
        
        // 100-500ms 대기
        std::this_thread::sleep_for(std::chrono::milliseconds(100 + gen() % 400));
    }
}

void VirtualUser::PerformRandomAction() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    
    float rand = dist(gen);
    
    if (rand < behavior_.movement_rate) {
        // 무작위 이동
        Vector3 direction(dist(gen) - 0.5f, 0, dist(gen) - 0.5f);
        direction.Normalize();
        Move(direction * 5.0f);
    } else if (rand < behavior_.movement_rate + behavior_.combat_rate) {
        // 전투
        uint64_t target_id = 1000000 + (gen() % 1000);  // 가상 타겟
        Attack(target_id);
    } else if (rand < behavior_.movement_rate + behavior_.combat_rate + behavior_.skill_use_rate) {
        // 스킬 사용
        uint32_t skill_id = 1 + (gen() % 10);
        uint64_t target_id = 1000000 + (gen() % 1000);
        UseSkill(skill_id, target_id);
    } else if (rand < behavior_.movement_rate + behavior_.combat_rate + 
                      behavior_.skill_use_rate + behavior_.chat_rate) {
        // 채팅
        SendChat("Test message " + std::to_string(gen() % 1000));
    }
}

void VirtualUser::SendChat(const std::string& message) {
    if (!connected_) return;
    
    network::Packet chat_packet(network::PacketType::CHAT_MESSAGE);
    chat_packet.Write(static_cast<uint8_t>(0));  // 일반 채팅
    chat_packet.WriteString(message);
    
    connection_->Send(chat_packet);
}

// [SEQUENCE: 3813] Performance test framework implementation 성능 테스트 프레임워크 구현
void PerformanceTestFramework::RunTest(const TestScenario& scenario) {
    if (test_running_) {
        spdlog::warn("[PerformanceTest] Test already running");
        return;
    }
    
    spdlog::info("[PerformanceTest] Starting test: {}", scenario.name);
    
    test_running_ = true;
    current_scenario_ = scenario;
    test_start_time_ = std::chrono::steady_clock::now();
    
    // 메트릭 초기화
    metrics_ = PerformanceMetrics();
    response_times_.clear();
    response_times_.reserve(1000000);  // 백만개 예약
    
    // 테스트 타입별 실행
    switch (scenario.type) {
        case TestType::LOAD_TEST:
            ExecuteLoadTest(scenario);
            break;
        case TestType::STRESS_TEST:
            ExecuteStressTest(scenario);
            break;
        case TestType::SPIKE_TEST:
            ExecuteSpikeTest(scenario);
            break;
        case TestType::ENDURANCE_TEST:
            ExecuteEnduranceTest(scenario);
            break;
        default:
            spdlog::error("[PerformanceTest] Unknown test type");
            test_running_ = false;
            return;
    }
}

void PerformanceTestFramework::StopTest() {
    if (!test_running_) {
        return;
    }
    
    spdlog::info("[PerformanceTest] Stopping test");
    test_running_ = false;
    
    // 모든 가상 사용자 정지
    {
        std::lock_guard lock(users_mutex_);
        for (auto& user : virtual_users_) {
            user->StopBehaviorLoop();
            user->Disconnect();
        }
        virtual_users_.clear();
    }
    
    // 최종 메트릭 계산
    CalculatePercentiles();
    
    // 테스트 결과 로그
    auto duration = std::chrono::steady_clock::now() - test_start_time_;
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    
    spdlog::info("[PerformanceTest] Test completed. Duration: {}s", seconds);
    spdlog::info("[PerformanceTest] Average response time: {:.2f}ms", 
                 metrics_.response_time.avg_ms.load());
    spdlog::info("[PerformanceTest] P95 response time: {:.2f}ms", 
                 metrics_.response_time.p95_ms.load());
    spdlog::info("[PerformanceTest] Error rate: {:.2f}%", 
                 metrics_.errors.error_percentage.load());
}

void PerformanceTestFramework::ExecuteLoadTest(const TestScenario& scenario) {
    spdlog::info("[PerformanceTest] Executing load test with {} users", 
                 scenario.target_users);
    
    // 메트릭 수집 스레드 시작
    std::thread metrics_thread([this]() {
        while (test_running_) {
            CollectSystemMetrics();
            UpdateThroughput();
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    });
    
    // Ramp-up 단계
    RampUpUsers(scenario.target_users, scenario.ramp_up_seconds);
    
    // 유지 단계
    auto sustain_duration = scenario.duration_seconds - 
                           scenario.ramp_up_seconds - 
                           scenario.ramp_down_seconds;
    std::this_thread::sleep_for(std::chrono::seconds(sustain_duration));
    
    // Ramp-down 단계
    RampDownUsers(0, scenario.ramp_down_seconds);
    
    // 메트릭 스레드 종료
    test_running_ = false;
    if (metrics_thread.joinable()) {
        metrics_thread.join();
    }
}

void PerformanceTestFramework::ExecuteStressTest(const TestScenario& scenario) {
    spdlog::info("[PerformanceTest] Executing stress test");
    
    // 최대 부하까지 점진적 증가
    uint32_t current_users = 100;
    while (test_running_ && current_users <= scenario.target_users * 2) {
        CreateVirtualUsers(100, scenario.behavior);
        current_users += 100;
        
        // 30초 대기 후 성능 확인
        std::this_thread::sleep_for(std::chrono::seconds(30));
        
        // 실패 조건 확인
        if (metrics_.errors.error_percentage > 10.0 || 
            metrics_.response_time.avg_ms > 1000.0) {
            spdlog::warn("[PerformanceTest] System reaching limits at {} users", 
                        current_users);
            break;
        }
    }
}

void PerformanceTestFramework::CreateVirtualUsers(uint32_t count, 
                                                 const TestScenario::UserBehavior& behavior) {
    std::lock_guard lock(users_mutex_);
    
    for (uint32_t i = 0; i < count; ++i) {
        uint64_t user_id = virtual_users_.size() + 1;
        auto user = std::make_unique<VirtualUser>(user_id, behavior);
        
        try {
            user->Connect("localhost:8080");
            user->Login("user" + std::to_string(user_id), "password");
            user->SelectCharacter(0);
            user->StartBehaviorLoop();
            
            virtual_users_.push_back(std::move(user));
            metrics_.game.active_players++;
        } catch (const std::exception& e) {
            spdlog::error("[PerformanceTest] Failed to create user {}: {}", 
                         user_id, e.what());
            metrics_.errors.connection_errors++;
        }
    }
    
    spdlog::info("[PerformanceTest] Created {} virtual users, total: {}", 
                 count, virtual_users_.size());
}

void PerformanceTestFramework::RampUpUsers(uint32_t target_count, uint32_t duration_seconds) {
    if (duration_seconds == 0) {
        CreateVirtualUsers(target_count, current_scenario_.behavior);
        return;
    }
    
    uint32_t users_per_second = target_count / duration_seconds;
    if (users_per_second == 0) users_per_second = 1;
    
    for (uint32_t i = 0; i < duration_seconds && test_running_; ++i) {
        uint32_t users_to_create = users_per_second;
        if (i == duration_seconds - 1) {
            // 마지막 초에 나머지 생성
            users_to_create = target_count - (users_per_second * i);
        }
        
        CreateVirtualUsers(users_to_create, current_scenario_.behavior);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void PerformanceTestFramework::RampDownUsers(uint32_t target_count, uint32_t duration_seconds) {
    std::lock_guard lock(users_mutex_);
    
    uint32_t current_count = virtual_users_.size();
    if (current_count <= target_count) {
        return;
    }
    
    uint32_t users_to_remove = current_count - target_count;
    uint32_t users_per_second = users_to_remove / duration_seconds;
    if (users_per_second == 0) users_per_second = 1;
    
    for (uint32_t i = 0; i < duration_seconds && test_running_; ++i) {
        for (uint32_t j = 0; j < users_per_second && !virtual_users_.empty(); ++j) {
            auto& user = virtual_users_.back();
            user->StopBehaviorLoop();
            user->Disconnect();
            virtual_users_.pop_back();
            metrics_.game.active_players--;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

void PerformanceTestFramework::RecordResponseTime(double response_ms) {
    std::lock_guard lock(metrics_mutex_);
    
    response_times_.push_back(response_ms);
    
    // 실시간 통계 업데이트
    metrics_.response_time.total_requests++;
    
    if (response_ms < metrics_.response_time.min_ms) {
        metrics_.response_time.min_ms = response_ms;
    }
    if (response_ms > metrics_.response_time.max_ms) {
        metrics_.response_time.max_ms = response_ms;
    }
    
    // 이동 평균 계산
    double current_avg = metrics_.response_time.avg_ms;
    uint64_t count = metrics_.response_time.total_requests;
    metrics_.response_time.avg_ms = (current_avg * (count - 1) + response_ms) / count;
}

void PerformanceTestFramework::RecordError(const std::string& error_type) {
    metrics_.errors.total_errors++;
    
    if (error_type == "timeout") {
        metrics_.errors.timeout_errors++;
    } else if (error_type == "connection") {
        metrics_.errors.connection_errors++;
    } else if (error_type == "validation") {
        metrics_.errors.validation_errors++;
    }
    
    // 에러율 계산
    uint64_t total_requests = metrics_.response_time.total_requests;
    if (total_requests > 0) {
        metrics_.errors.error_percentage = 
            (double)metrics_.errors.total_errors / total_requests * 100.0;
    }
}

void PerformanceTestFramework::CollectSystemMetrics() {
    // CPU 사용률
    metrics_.resources.cpu_usage_percent = 
        optimization::OptimizationUtils::GetCPUUsage();
    
    // 메모리 사용량
    metrics_.resources.memory_usage_gb = 
        optimization::OptimizationUtils::GetMemoryUsage() / (1024.0 * 1024.0 * 1024.0);
    
    // 활성 연결 수
    metrics_.resources.connection_count = 
        network::NetworkManager::Instance().GetActiveConnections();
    
    // 게임 메트릭
    metrics_.game.entities_processed = 
        world::WorldManager::Instance().GetEntityCount();
    
    metrics_.game.tick_rate_fps = 
        world::WorldManager::Instance().GetTickRate();
}

void PerformanceTestFramework::CalculatePercentiles() {
    std::lock_guard lock(metrics_mutex_);
    
    if (response_times_.empty()) {
        return;
    }
    
    // 정렬
    std::sort(response_times_.begin(), response_times_.end());
    
    size_t size = response_times_.size();
    metrics_.response_time.p50_ms = response_times_[size * 0.50];
    metrics_.response_time.p95_ms = response_times_[size * 0.95];
    metrics_.response_time.p99_ms = response_times_[size * 0.99];
}

void PerformanceTestFramework::UpdateThroughput() {
    static uint64_t last_requests = 0;
    static auto last_time = std::chrono::steady_clock::now();
    
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration<double>(now - last_time).count();
    
    if (duration >= 1.0) {
        uint64_t current_requests = metrics_.response_time.total_requests;
        metrics_.throughput.requests_per_second = 
            (current_requests - last_requests) / duration;
        
        last_requests = current_requests;
        last_time = now;
    }
}

void PerformanceTestFramework::GenerateReport(const std::string& output_file) {
    std::ofstream file(output_file);
    if (!file.is_open()) {
        spdlog::error("[PerformanceTest] Failed to open report file: {}", output_file);
        return;
    }
    
    file << "Performance Test Report\n";
    file << "=====================\n\n";
    
    file << "Test Scenario: " << current_scenario_.name << "\n";
    file << "Test Type: " << static_cast<int>(current_scenario_.type) << "\n";
    file << "Duration: " << current_scenario_.duration_seconds << " seconds\n";
    file << "Target Users: " << current_scenario_.target_users << "\n\n";
    
    file << "Response Time Metrics:\n";
    file << "  Min: " << std::fixed << std::setprecision(2) 
         << metrics_.response_time.min_ms << " ms\n";
    file << "  Max: " << metrics_.response_time.max_ms << " ms\n";
    file << "  Average: " << metrics_.response_time.avg_ms << " ms\n";
    file << "  P50: " << metrics_.response_time.p50_ms << " ms\n";
    file << "  P95: " << metrics_.response_time.p95_ms << " ms\n";
    file << "  P99: " << metrics_.response_time.p99_ms << " ms\n\n";
    
    file << "Throughput:\n";
    file << "  Requests/sec: " << metrics_.throughput.requests_per_second << "\n\n";
    
    file << "Error Rates:\n";
    file << "  Total Errors: " << metrics_.errors.total_errors << "\n";
    file << "  Error Rate: " << metrics_.errors.error_percentage << "%\n\n";
    
    file << "Resource Usage:\n";
    file << "  CPU: " << metrics_.resources.cpu_usage_percent << "%\n";
    file << "  Memory: " << metrics_.resources.memory_usage_gb << " GB\n";
    file << "  Connections: " << metrics_.resources.connection_count << "\n\n";
    
    file << "Success Criteria:\n";
    bool passed = TestUtils::ValidateTestResults(metrics_, current_scenario_.criteria);
    file << "  Overall Result: " << (passed ? "PASSED" : "FAILED") << "\n";
    
    file.close();
    spdlog::info("[PerformanceTest] Report generated: {}", output_file);
}

// [SEQUENCE: 3814] Load generator implementation 부하 생성기 구현
LoadGenerator::LoadGenerator(uint32_t thread_count) {
    for (uint32_t i = 0; i < thread_count; ++i) {
        worker_threads_.emplace_back();
    }
}

LoadGenerator::~LoadGenerator() {
    StopGeneration();
}

void LoadGenerator::GenerateLoad(LoadPattern pattern, uint32_t target_rps, 
                               uint32_t duration_seconds) {
    running_ = true;
    
    for (size_t i = 0; i < worker_threads_.size(); ++i) {
        worker_threads_[i] = std::thread(
            &LoadGenerator::WorkerLoop, this, 
            target_rps / worker_threads_.size());
    }
    
    // 지정된 시간 동안 실행
    std::this_thread::sleep_for(std::chrono::seconds(duration_seconds));
    
    StopGeneration();
}

void LoadGenerator::StopGeneration() {
    running_ = false;
    
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void LoadGenerator::WorkerLoop(uint32_t target_rps) {
    auto interval = std::chrono::microseconds(1000000 / target_rps);
    auto next_time = std::chrono::steady_clock::now();
    
    while (running_) {
        // 부하 함수 실행
        if (load_function_) {
            auto start = std::chrono::steady_clock::now();
            
            try {
                load_function_();
                stats_.successful_requests++;
            } catch (...) {
                stats_.failed_requests++;
            }
            
            auto end = std::chrono::steady_clock::now();
            auto latency = std::chrono::duration<double, std::milli>(end - start).count();
            
            // 평균 지연 시간 업데이트
            stats_.total_requests++;
            stats_.average_latency_ms = 
                (stats_.average_latency_ms * (stats_.total_requests - 1) + latency) / 
                stats_.total_requests;
        }
        
        // 다음 실행 시간까지 대기
        next_time += interval;
        std::this_thread::sleep_until(next_time);
    }
}

// [SEQUENCE: 3815] Benchmark suite implementation 벤치마크 스위트 구현
void BenchmarkSuite::RegisterBenchmark(const Benchmark& benchmark) {
    benchmarks_.push_back(benchmark);
}

void BenchmarkSuite::RunAll() {
    spdlog::info("[Benchmark] Running {} benchmarks", benchmarks_.size());
    
    for (const auto& benchmark : benchmarks_) {
        auto result = RunSingleBenchmark(benchmark);
        results_.push_back(result);
    }
    
    PrintResults();
}

void BenchmarkSuite::RunBenchmark(const std::string& name) {
    auto it = std::find_if(benchmarks_.begin(), benchmarks_.end(),
        [&name](const Benchmark& b) { return b.name == name; });
    
    if (it != benchmarks_.end()) {
        auto result = RunSingleBenchmark(*it);
        results_.push_back(result);
        PrintResults();
    } else {
        spdlog::error("[Benchmark] Benchmark '{}' not found", name);
    }
}

BenchmarkSuite::BenchmarkResult BenchmarkSuite::RunSingleBenchmark(const Benchmark& benchmark) {
    spdlog::info("[Benchmark] Running: {}", benchmark.name);
    
    BenchmarkResult result;
    result.name = benchmark.name;
    result.iterations = benchmark.iterations;
    
    std::vector<double> times;
    times.reserve(benchmark.iterations);
    
    // Warmup
    for (uint32_t i = 0; i < benchmark.warmup_iterations; ++i) {
        benchmark.test_function();
    }
    
    // 실제 테스트
    for (uint32_t i = 0; i < benchmark.iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        benchmark.test_function();
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration<double, std::micro>(end - start).count();
        times.push_back(duration);
    }
    
    // 통계 계산
    result.min_time_us = *std::min_element(times.begin(), times.end());
    result.max_time_us = *std::max_element(times.begin(), times.end());
    result.avg_time_us = std::accumulate(times.begin(), times.end(), 0.0) / times.size();
    result.std_deviation_us = CalculateStandardDeviation(times, result.avg_time_us);
    
    return result;
}

double BenchmarkSuite::CalculateStandardDeviation(const std::vector<double>& times, double avg) {
    double sum_sq = 0.0;
    for (double time : times) {
        double diff = time - avg;
        sum_sq += diff * diff;
    }
    return std::sqrt(sum_sq / times.size());
}

void BenchmarkSuite::PrintResults() const {
    spdlog::info("[Benchmark] Results:");
    spdlog::info("{:<30} {:>10} {:>10} {:>10} {:>10}", 
                 "Benchmark", "Min (us)", "Max (us)", "Avg (us)", "StdDev");
    spdlog::info("{:-<70}", "");
    
    for (const auto& result : results_) {
        spdlog::info("{:<30} {:>10.2f} {:>10.2f} {:>10.2f} {:>10.2f}",
                     result.name,
                     result.min_time_us,
                     result.max_time_us,
                     result.avg_time_us,
                     result.std_deviation_us);
    }
}

// [SEQUENCE: 3816] Stress test scenarios 스트레스 테스트 시나리오
namespace StressTestScenarios {

TestScenario CreateMassiveCombatScenario() {
    TestScenario scenario;
    scenario.name = "Massive Combat Test";
    scenario.type = TestType::STRESS_TEST;
    scenario.duration_seconds = 600;  // 10분
    scenario.target_users = 500;
    scenario.ramp_up_seconds = 120;
    scenario.ramp_down_seconds = 60;
    
    // 전투 중심 행동
    scenario.behavior.movement_rate = 0.3f;
    scenario.behavior.combat_rate = 0.8f;
    scenario.behavior.skill_use_rate = 0.7f;
    scenario.behavior.chat_rate = 0.1f;
    scenario.behavior.trade_rate = 0.0f;
    
    scenario.criteria.max_response_time_ms = 150.0f;
    scenario.criteria.max_error_rate = 0.05f;
    scenario.criteria.min_throughput_rps = 5000.0f;
    
    return scenario;
}

TestScenario CreateLoginStormScenario() {
    TestScenario scenario;
    scenario.name = "Login Storm Test";
    scenario.type = TestType::SPIKE_TEST;
    scenario.duration_seconds = 300;  // 5분
    scenario.target_users = 1000;
    scenario.ramp_up_seconds = 10;   // 급격한 증가
    scenario.ramp_down_seconds = 30;
    
    // 로그인 후 기본 행동
    scenario.behavior.movement_rate = 0.5f;
    scenario.behavior.combat_rate = 0.1f;
    scenario.behavior.skill_use_rate = 0.1f;
    scenario.behavior.chat_rate = 0.3f;
    scenario.behavior.trade_rate = 0.2f;
    
    scenario.criteria.max_response_time_ms = 200.0f;
    scenario.criteria.max_error_rate = 0.02f;
    
    return scenario;
}

TestScenario CreateGuildWarScenario() {
    TestScenario scenario;
    scenario.name = "Guild War Test";
    scenario.type = TestType::LOAD_TEST;
    scenario.duration_seconds = 1800;  // 30분
    scenario.target_users = 200;      // 100 vs 100
    scenario.ramp_up_seconds = 180;
    scenario.ramp_down_seconds = 60;
    
    // 길드전 중심
    scenario.behavior.movement_rate = 0.6f;
    scenario.behavior.combat_rate = 0.9f;
    scenario.behavior.skill_use_rate = 0.8f;
    scenario.behavior.chat_rate = 0.4f;  // 전술 지시
    scenario.behavior.trade_rate = 0.0f;
    
    scenario.criteria.max_response_time_ms = 100.0f;
    scenario.criteria.max_error_rate = 0.01f;
    scenario.criteria.max_cpu_usage = 70.0f;
    
    return scenario;
}

} // namespace StressTestScenarios

// [SEQUENCE: 3817] Performance monitor implementation 성능 모니터 구현
void PerformanceMonitor::StartMonitoring(std::chrono::milliseconds interval) {
    if (monitoring_) {
        return;
    }
    
    monitoring_ = true;
    monitor_thread_ = std::thread(&PerformanceMonitor::MonitorLoop, this, interval);
    
    spdlog::info("[PerformanceMonitor] Started monitoring with {}ms interval", 
                 interval.count());
}

void PerformanceMonitor::StopMonitoring() {
    monitoring_ = false;
    
    if (monitor_thread_.joinable()) {
        monitor_thread_.join();
    }
    
    spdlog::info("[PerformanceMonitor] Stopped monitoring");
}

void PerformanceMonitor::SetAlertThresholds(const AlertThreshold& thresholds) {
    thresholds_ = thresholds;
}

void PerformanceMonitor::SetAlertCallback(std::function<void(const std::string&)> callback) {
    alert_callback_ = callback;
}

void PerformanceMonitor::MonitorLoop(std::chrono::milliseconds interval) {
    while (monitoring_) {
        auto metrics = PerformanceTestFramework::Instance().GetCurrentMetrics();
        CheckThresholds(metrics);
        
        std::this_thread::sleep_for(interval);
    }
}

void PerformanceMonitor::CheckThresholds(const PerformanceMetrics& metrics) {
    std::vector<std::string> alerts;
    
    if (metrics.resources.cpu_usage_percent > thresholds_.cpu_usage_percent) {
        alerts.push_back("CPU usage exceeded: " + 
                        std::to_string(metrics.resources.cpu_usage_percent.load()) + "%");
    }
    
    if (metrics.resources.memory_usage_gb > thresholds_.memory_usage_gb) {
        alerts.push_back("Memory usage exceeded: " + 
                        std::to_string(metrics.resources.memory_usage_gb.load()) + " GB");
    }
    
    if (metrics.response_time.avg_ms > thresholds_.response_time_ms) {
        alerts.push_back("Response time exceeded: " + 
                        std::to_string(metrics.response_time.avg_ms.load()) + " ms");
    }
    
    if (metrics.errors.error_percentage > thresholds_.error_rate_percent) {
        alerts.push_back("Error rate exceeded: " + 
                        std::to_string(metrics.errors.error_percentage.load()) + "%");
    }
    
    // 경고 콜백 호출
    if (!alerts.empty() && alert_callback_) {
        for (const auto& alert : alerts) {
            alert_callback_(alert);
        }
    }
}

// [SEQUENCE: 3818] Test utilities implementation 테스트 유틸리티 구현
namespace TestUtils {

std::vector<std::string> GenerateRandomUsernames(uint32_t count) {
    std::vector<std::string> usernames;
    usernames.reserve(count);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(1000, 9999);
    
    for (uint32_t i = 0; i < count; ++i) {
        usernames.push_back("player_" + std::to_string(dist(gen)));
    }
    
    return usernames;
}

std::vector<Vector3> GenerateRandomPositions(uint32_t count, float range) {
    std::vector<Vector3> positions;
    positions.reserve(count);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(-range, range);
    
    for (uint32_t i = 0; i < count; ++i) {
        positions.emplace_back(dist(gen), 0.0f, dist(gen));
    }
    
    return positions;
}

double CalculatePercentile(std::vector<double>& values, double percentile) {
    if (values.empty()) {
        return 0.0;
    }
    
    std::sort(values.begin(), values.end());
    
    size_t index = static_cast<size_t>(values.size() * percentile / 100.0);
    if (index >= values.size()) {
        index = values.size() - 1;
    }
    
    return values[index];
}

double CalculateStandardDeviation(const std::vector<double>& values) {
    if (values.empty()) {
        return 0.0;
    }
    
    double mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    double sum_sq = 0.0;
    
    for (double value : values) {
        double diff = value - mean;
        sum_sq += diff * diff;
    }
    
    return std::sqrt(sum_sq / values.size());
}

bool ValidateTestResults(const PerformanceMetrics& metrics, 
                        const TestScenario::SuccessCriteria& criteria) {
    
    bool passed = true;
    
    if (metrics.response_time.avg_ms > criteria.max_response_time_ms) {
        spdlog::warn("[Validation] Response time failed: {:.2f}ms > {:.2f}ms",
                    metrics.response_time.avg_ms.load(), 
                    criteria.max_response_time_ms);
        passed = false;
    }
    
    if (metrics.errors.error_percentage > criteria.max_error_rate * 100) {
        spdlog::warn("[Validation] Error rate failed: {:.2f}% > {:.2f}%",
                    metrics.errors.error_percentage.load(), 
                    criteria.max_error_rate * 100);
        passed = false;
    }
    
    if (metrics.throughput.requests_per_second < criteria.min_throughput_rps) {
        spdlog::warn("[Validation] Throughput failed: {} < {} rps",
                    metrics.throughput.requests_per_second.load(), 
                    criteria.min_throughput_rps);
        passed = false;
    }
    
    if (metrics.resources.cpu_usage_percent > criteria.max_cpu_usage) {
        spdlog::warn("[Validation] CPU usage failed: {:.2f}% > {:.2f}%",
                    metrics.resources.cpu_usage_percent.load(), 
                    criteria.max_cpu_usage);
        passed = false;
    }
    
    if (metrics.resources.memory_usage_gb > criteria.max_memory_usage_gb) {
        spdlog::warn("[Validation] Memory usage failed: {:.2f}GB > {:.2f}GB",
                    metrics.resources.memory_usage_gb.load(), 
                    criteria.max_memory_usage_gb);
        passed = false;
    }
    
    return passed;
}

std::string FormatMetrics(const PerformanceMetrics& metrics) {
    std::stringstream ss;
    
    ss << "Performance Metrics:\n";
    ss << "  Response Time: " << std::fixed << std::setprecision(2) 
       << metrics.response_time.avg_ms << " ms (P95: "
       << metrics.response_time.p95_ms << " ms)\n";
    ss << "  Throughput: " << metrics.throughput.requests_per_second << " rps\n";
    ss << "  Error Rate: " << metrics.errors.error_percentage << "%\n";
    ss << "  CPU Usage: " << metrics.resources.cpu_usage_percent << "%\n";
    ss << "  Memory: " << metrics.resources.memory_usage_gb << " GB\n";
    ss << "  Active Players: " << metrics.game.active_players << "\n";
    ss << "  Server Tick: " << metrics.game.tick_rate_fps << " FPS\n";
    
    return ss.str();
}

} // namespace TestUtils

} // namespace mmorpg::testing