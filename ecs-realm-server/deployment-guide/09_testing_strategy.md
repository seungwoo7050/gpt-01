# 09. Testing Strategy - 테스팅 전략 가이드

## 🎯 목표
개발부터 프로덕션까지 모든 단계에서 품질을 보장하는 포괄적인 테스팅 전략을 수립합니다.

## 📋 Prerequisites
- 테스트 프레임워크 (Google Test, Catch2)
- 부하 테스트 도구 (K6, Locust, JMeter)
- 모의 객체 라이브러리 (GMock)
- CI/CD 파이프라인
- 테스트 환경

---

## 1. 테스트 피라미드

### 1.1 테스트 레벨 분포
```
         /\
        /  \  E2E Tests (5%)
       /    \  - Full user scenarios
      /      \  - Production-like environment
     /--------\
    /          \ Integration Tests (15%)
   /            \ - API tests
  /              \ - Database tests
 /                \ - Service integration
/------------------\
      Unit Tests (80%)
   - Business logic
   - Utility functions
   - Component isolation
```

### 1.2 테스트 전략 매트릭스
| 테스트 유형 | 목적 | 빈도 | 소요 시간 | 환경 |
|------------|------|------|-----------|------|
| Unit | 개별 함수 검증 | 커밋마다 | <1분 | Local/CI |
| Integration | 컴포넌트 통합 | PR마다 | 5분 | CI |
| E2E | 전체 플로우 | 일일 | 30분 | Staging |
| Performance | 성능 검증 | 주간 | 2시간 | Staging |
| Security | 보안 취약점 | 릴리스 전 | 1시간 | Staging |
| Chaos | 장애 복구 | 월간 | 4시간 | Production-like |

---

## 2. 단위 테스트

### 2.1 C++ 단위 테스트 구현
**tests/unit/test_combat_system.cpp**
```cpp
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "game/combat/combat_system.h"

using ::testing::_;
using ::testing::Return;
using ::testing::AtLeast;

// Mock 객체
class MockPlayer : public IPlayer {
public:
    MOCK_METHOD(int, GetLevel, (), (const, override));
    MOCK_METHOD(int, GetAttackPower, (), (const, override));
    MOCK_METHOD(int, GetDefense, (), (const, override));
    MOCK_METHOD(void, TakeDamage, (int damage), (override));
    MOCK_METHOD(bool, IsAlive, (), (const, override));
};

class MockRandomGenerator : public IRandomGenerator {
public:
    MOCK_METHOD(int, Generate, (int min, int max), (override));
    MOCK_METHOD(float, GenerateFloat, (float min, float max), (override));
};

// Test Fixture
class CombatSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        attacker = std::make_unique<MockPlayer>();
        defender = std::make_unique<MockPlayer>();
        rng = std::make_unique<MockRandomGenerator>();
        combat_system = std::make_unique<CombatSystem>(rng.get());
    }
    
    std::unique_ptr<MockPlayer> attacker;
    std::unique_ptr<MockPlayer> defender;
    std::unique_ptr<MockRandomGenerator> rng;
    std::unique_ptr<CombatSystem> combat_system;
};

// 테스트 케이스
TEST_F(CombatSystemTest, BasicAttackDamageCalculation) {
    // Given
    EXPECT_CALL(*attacker, GetAttackPower()).WillOnce(Return(100));
    EXPECT_CALL(*defender, GetDefense()).WillOnce(Return(50));
    EXPECT_CALL(*rng, GenerateFloat(0.9f, 1.1f)).WillOnce(Return(1.0f));
    
    // When
    int damage = combat_system->CalculateDamage(attacker.get(), defender.get());
    
    // Then
    EXPECT_EQ(damage, 50); // 100 - 50 = 50
}

TEST_F(CombatSystemTest, CriticalHitDoublesDamage) {
    // Given
    EXPECT_CALL(*attacker, GetAttackPower()).WillOnce(Return(100));
    EXPECT_CALL(*defender, GetDefense()).WillOnce(Return(20));
    EXPECT_CALL(*rng, Generate(1, 100)).WillOnce(Return(5)); // 5% crit chance
    EXPECT_CALL(*rng, GenerateFloat(0.9f, 1.1f)).WillOnce(Return(1.0f));
    
    // When
    int damage = combat_system->CalculateDamageWithCrit(attacker.get(), defender.get());
    
    // Then
    EXPECT_EQ(damage, 160); // (100 - 20) * 2 = 160
}

TEST_F(CombatSystemTest, MinimumDamageIsOne) {
    // Given
    EXPECT_CALL(*attacker, GetAttackPower()).WillOnce(Return(10));
    EXPECT_CALL(*defender, GetDefense()).WillOnce(Return(100));
    
    // When
    int damage = combat_system->CalculateDamage(attacker.get(), defender.get());
    
    // Then
    EXPECT_EQ(damage, 1); // Minimum damage
}

// Parameterized Tests
class DamageCalculationTest : public ::testing::TestWithParam<std::tuple<int, int, int>> {};

TEST_P(DamageCalculationTest, VariousDamageScenarios) {
    int attack = std::get<0>(GetParam());
    int defense = std::get<1>(GetParam());
    int expected = std::get<2>(GetParam());
    
    CombatSystem combat;
    EXPECT_EQ(combat.CalculateBaseDamage(attack, defense), expected);
}

INSTANTIATE_TEST_SUITE_P(
    DamageScenarios,
    DamageCalculationTest,
    ::testing::Values(
        std::make_tuple(100, 50, 50),
        std::make_tuple(50, 100, 1),
        std::make_tuple(200, 50, 150),
        std::make_tuple(0, 50, 1)
    )
);

// Property-based testing
TEST(CombatPropertyTest, DamageIsAlwaysPositive) {
    CombatSystem combat;
    
    for (int i = 0; i < 1000; ++i) {
        int attack = rand() % 1000;
        int defense = rand() % 1000;
        
        int damage = combat.CalculateBaseDamage(attack, defense);
        ASSERT_GT(damage, 0) << "Attack: " << attack << ", Defense: " << defense;
    }
}
```

### 2.2 테스트 커버리지 설정
**CMakeLists.txt**
```cmake
# 테스트 커버리지 옵션
option(ENABLE_COVERAGE "Enable coverage reporting" OFF)

if(ENABLE_COVERAGE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()

# 테스트 타겟
enable_testing()
add_subdirectory(tests)

# 커버리지 리포트 생성
add_custom_target(coverage
    COMMAND lcov --capture --directory . --output-file coverage.info
    COMMAND lcov --remove coverage.info '/usr/*' --output-file coverage.info
    COMMAND lcov --list coverage.info
    COMMAND genhtml coverage.info --output-directory coverage_report
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
```

---

## 3. 통합 테스트

### 3.1 API 통합 테스트
**tests/integration/test_api.cpp**
```cpp
#include <gtest/gtest.h>
#include <httplib.h>
#include "test_fixtures.h"

class APIIntegrationTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        // 테스트 서버 시작
        server_process = StartTestServer();
        WaitForServerReady();
        
        // 테스트 데이터베이스 초기화
        InitTestDatabase();
    }
    
    static void TearDownTestSuite() {
        StopTestServer(server_process);
        CleanupTestDatabase();
    }
    
    void SetUp() override {
        client = std::make_unique<httplib::Client>("localhost", 8080);
        client->set_connection_timeout(5);
    }
    
    std::unique_ptr<httplib::Client> client;
    static pid_t server_process;
};

TEST_F(APIIntegrationTest, PlayerLoginFlow) {
    // 1. 회원가입
    nlohmann::json signup_data = {
        {"username", "testplayer"},
        {"password", "Test123!@#"},
        {"email", "test@example.com"}
    };
    
    auto signup_res = client->Post("/api/v1/signup", 
                                   signup_data.dump(), 
                                   "application/json");
    ASSERT_EQ(signup_res->status, 201);
    
    // 2. 로그인
    nlohmann::json login_data = {
        {"username", "testplayer"},
        {"password", "Test123!@#"}
    };
    
    auto login_res = client->Post("/api/v1/login",
                                  login_data.dump(),
                                  "application/json");
    ASSERT_EQ(login_res->status, 200);
    
    auto response = nlohmann::json::parse(login_res->body);
    ASSERT_TRUE(response.contains("token"));
    
    std::string token = response["token"];
    
    // 3. 인증된 요청
    httplib::Headers headers = {
        {"Authorization", "Bearer " + token}
    };
    
    auto profile_res = client->Get("/api/v1/player/profile", headers);
    ASSERT_EQ(profile_res->status, 200);
    
    auto profile = nlohmann::json::parse(profile_res->body);
    EXPECT_EQ(profile["username"], "testplayer");
}

TEST_F(APIIntegrationTest, CombatFlow) {
    // 로그인
    std::string token = LoginTestUser("player1");
    httplib::Headers headers = {{"Authorization", "Bearer " + token}};
    
    // 전투 시작
    nlohmann::json combat_data = {
        {"target_id", "npc_123"},
        {"skill_id", "basic_attack"}
    };
    
    auto combat_res = client->Post("/api/v1/combat/attack",
                                   combat_data.dump(),
                                   "application/json",
                                   headers);
    ASSERT_EQ(combat_res->status, 200);
    
    auto result = nlohmann::json::parse(combat_res->body);
    EXPECT_GT(result["damage"], 0);
    EXPECT_TRUE(result.contains("target_hp"));
}

TEST_F(APIIntegrationTest, ConcurrentRequests) {
    const int num_threads = 10;
    const int requests_per_thread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> success_count(0);
    std::atomic<int> error_count(0);
    
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() {
            httplib::Client thread_client("localhost", 8080);
            
            for (int j = 0; j < requests_per_thread; ++j) {
                auto res = thread_client.Get("/api/v1/status");
                if (res && res->status == 200) {
                    success_count++;
                } else {
                    error_count++;
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    EXPECT_EQ(success_count, num_threads * requests_per_thread);
    EXPECT_EQ(error_count, 0);
}
```

---

## 4. E2E 테스트

### 4.1 Selenium 기반 E2E 테스트
**tests/e2e/test_game_flow.py**
```python
import pytest
from selenium import webdriver
from selenium.webdriver.common.by import By
from selenium.webdriver.support.ui import WebDriverWait
from selenium.webdriver.support import expected_conditions as EC
import time

class TestGameFlow:
    @pytest.fixture(autouse=True)
    def setup(self):
        self.driver = webdriver.Chrome()
        self.driver.get("https://staging.gameserver.com")
        yield
        self.driver.quit()
    
    def test_complete_game_session(self):
        """완전한 게임 세션 테스트"""
        
        # 1. 로그인
        self.login("testplayer", "password123")
        
        # 2. 캐릭터 선택
        self.select_character("Warrior")
        
        # 3. 게임 월드 진입
        assert self.wait_for_element("game-world")
        
        # 4. 이동 테스트
        self.move_character(100, 200)
        position = self.get_character_position()
        assert position == (100, 200)
        
        # 5. NPC 상호작용
        self.interact_with_npc("merchant")
        assert self.wait_for_element("shop-dialog")
        
        # 6. 아이템 구매
        self.buy_item("health-potion", 5)
        inventory = self.get_inventory()
        assert inventory["health-potion"] == 5
        
        # 7. 전투 테스트
        self.engage_combat("goblin")
        self.use_skill("basic-attack")
        combat_result = self.wait_for_combat_result()
        assert combat_result["victory"] == True
        
        # 8. 로그아웃
        self.logout()
        assert self.wait_for_element("login-screen")
    
    def test_pvp_match(self):
        """PvP 매치 테스트"""
        
        # Player 1 로그인
        driver1 = webdriver.Chrome()
        self.login_with_driver(driver1, "player1", "pass1")
        self.join_pvp_queue(driver1)
        
        # Player 2 로그인
        driver2 = webdriver.Chrome()
        self.login_with_driver(driver2, "player2", "pass2")
        self.join_pvp_queue(driver2)
        
        # 매치메이킹 대기
        time.sleep(5)
        
        # 양쪽 모두 전투 화면 확인
        assert self.is_in_combat(driver1)
        assert self.is_in_combat(driver2)
        
        # 전투 진행
        self.execute_combat_turn(driver1, "attack")
        self.execute_combat_turn(driver2, "defend")
        
        # 결과 확인
        result1 = self.get_combat_result(driver1)
        result2 = self.get_combat_result(driver2)
        
        assert (result1["victory"] and not result2["victory"]) or \
               (not result1["victory"] and result2["victory"])
        
        driver1.quit()
        driver2.quit()
```

---

## 5. 성능 테스트

### 5.1 K6 부하 테스트
**tests/performance/load-test.js**
```javascript
import http from 'k6/http';
import { check, sleep, group } from 'k6';
import { Rate, Trend } from 'k6/metrics';

// 커스텀 메트릭
const errorRate = new Rate('errors');
const loginDuration = new Trend('login_duration');
const combatDuration = new Trend('combat_duration');

// 테스트 시나리오
export let options = {
    scenarios: {
        // 점진적 부하 증가
        ramp_up: {
            executor: 'ramping-vus',
            startVUs: 0,
            stages: [
                { duration: '2m', target: 100 },
                { duration: '5m', target: 500 },
                { duration: '10m', target: 1000 },
                { duration: '5m', target: 5000 },
                { duration: '5m', target: 0 },
            ],
        },
        // 스파이크 테스트
        spike: {
            executor: 'constant-vus',
            vus: 2000,
            duration: '1m',
            startTime: '10m',
        },
        // 지속 부하
        steady: {
            executor: 'constant-arrival-rate',
            rate: 1000,
            timeUnit: '1s',
            duration: '30m',
            preAllocatedVUs: 2000,
        }
    },
    thresholds: {
        http_req_duration: ['p(95)<500', 'p(99)<1000'],
        http_req_failed: ['rate<0.01'],
        errors: ['rate<0.05'],
        login_duration: ['p(95)<2000'],
        combat_duration: ['p(95)<100'],
    },
};

const BASE_URL = 'https://staging.gameserver.com';

// 테스트 데이터
const users = JSON.parse(open('./test-users.json'));

export default function() {
    // 사용자 선택
    const user = users[Math.floor(Math.random() * users.length)];
    
    group('User Journey', function() {
        // 1. 로그인
        group('Login', function() {
            const loginStart = Date.now();
            
            const loginRes = http.post(`${BASE_URL}/api/v1/login`, {
                username: user.username,
                password: user.password,
            });
            
            loginDuration.add(Date.now() - loginStart);
            
            check(loginRes, {
                'login successful': (r) => r.status === 200,
                'token received': (r) => r.json('token') !== '',
            }) || errorRate.add(1);
            
            const token = loginRes.json('token');
            const headers = { Authorization: `Bearer ${token}` };
            
            // 2. 게임 월드 진입
            group('Enter World', function() {
                const worldRes = http.post(`${BASE_URL}/api/v1/world/enter`, {
                    character_id: user.character_id,
                }, { headers });
                
                check(worldRes, {
                    'entered world': (r) => r.status === 200,
                });
            });
            
            // 3. 게임 활동 시뮬레이션
            group('Game Activities', function() {
                // 이동
                for (let i = 0; i < 5; i++) {
                    const moveRes = http.post(`${BASE_URL}/api/v1/move`, {
                        x: Math.random() * 1000,
                        y: Math.random() * 1000,
                    }, { headers });
                    
                    check(moveRes, {
                        'move successful': (r) => r.status === 200,
                    });
                    
                    sleep(0.5);
                }
                
                // 전투
                const combatStart = Date.now();
                
                const combatRes = http.post(`${BASE_URL}/api/v1/combat/attack`, {
                    target: 'npc_' + Math.floor(Math.random() * 100),
                    skill: 'basic_attack',
                }, { headers });
                
                combatDuration.add(Date.now() - combatStart);
                
                check(combatRes, {
                    'combat successful': (r) => r.status === 200,
                });
                
                // 채팅
                const chatRes = http.post(`${BASE_URL}/api/v1/chat/send`, {
                    channel: 'global',
                    message: `Test message ${Date.now()}`,
                }, { headers });
                
                check(chatRes, {
                    'chat sent': (r) => r.status === 200,
                });
            });
            
            // 4. 로그아웃
            group('Logout', function() {
                const logoutRes = http.post(`${BASE_URL}/api/v1/logout`, {}, { headers });
                
                check(logoutRes, {
                    'logout successful': (r) => r.status === 200,
                });
            });
        });
    });
    
    sleep(1);
}

// 테스트 후 처리
export function handleSummary(data) {
    return {
        'summary.json': JSON.stringify(data),
        'summary.html': htmlReport(data),
        stdout: textSummary(data, { indent: ' ', enableColors: true }),
    };
}
```

### 5.2 메모리/CPU 프로파일링
**tests/performance/profiling.cpp**
```cpp
#include <gperftools/profiler.h>
#include <gperftools/heap-profiler.h>
#include <chrono>
#include <thread>

class PerformanceProfiler {
public:
    void ProfileCPU(const std::string& test_name, 
                    std::function<void()> test_func) {
        std::string filename = test_name + "_cpu.prof";
        
        ProfilerStart(filename.c_str());
        
        auto start = std::chrono::high_resolution_clock::now();
        test_func();
        auto end = std::chrono::high_resolution_clock::now();
        
        ProfilerStop();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << test_name << " took " << duration.count() << "ms\n";
        
        // 분석
        system(("pprof --text " + filename).c_str());
    }
    
    void ProfileMemory(const std::string& test_name,
                      std::function<void()> test_func) {
        std::string filename = test_name + "_heap.prof";
        
        HeapProfilerStart(filename.c_str());
        
        // 초기 메모리
        size_t initial_memory = GetCurrentMemoryUsage();
        
        test_func();
        
        // 최종 메모리
        size_t final_memory = GetCurrentMemoryUsage();
        
        HeapProfilerStop();
        
        std::cout << test_name << " memory delta: " 
                  << (final_memory - initial_memory) / 1024 / 1024 << "MB\n";
        
        // 메모리 누수 체크
        CheckForLeaks(filename);
    }
    
    void BenchmarkFunction(const std::string& name,
                          std::function<void()> func,
                          int iterations = 1000000) {
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            func();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        
        double ns_per_op = duration.count() / static_cast<double>(iterations);
        std::cout << name << ": " << ns_per_op << "ns per operation\n";
    }
};

// 성능 테스트 예시
TEST(PerformanceTest, CombatSystemPerformance) {
    PerformanceProfiler profiler;
    
    // CPU 프로파일링
    profiler.ProfileCPU("CombatCalculation", []() {
        CombatSystem combat;
        for (int i = 0; i < 1000000; ++i) {
            combat.CalculateDamage(100, 50);
        }
    });
    
    // 메모리 프로파일링
    profiler.ProfileMemory("EntityCreation", []() {
        std::vector<std::unique_ptr<Entity>> entities;
        for (int i = 0; i < 10000; ++i) {
            entities.push_back(std::make_unique<Entity>());
        }
    });
    
    // 벤치마크
    profiler.BenchmarkFunction("VectorMath", []() {
        Vector3 a(1, 2, 3);
        Vector3 b(4, 5, 6);
        auto c = a + b;
        auto d = c.Normalize();
    });
}
```

---

## 6. 카오스 엔지니어링

### 6.1 Chaos Monkey 설정
**chaos/chaos-experiments.yaml**
```yaml
apiVersion: chaos-mesh.org/v1alpha1
kind: PodChaos
metadata:
  name: pod-failure
  namespace: game-production
spec:
  action: pod-failure
  mode: random-max-percent
  value: "30"
  duration: "60s"
  selector:
    namespaces:
      - game-production
    labelSelectors:
      app: game-server
  scheduler:
    cron: "@hourly"
---
apiVersion: chaos-mesh.org/v1alpha1
kind: NetworkChaos
metadata:
  name: network-delay
  namespace: game-production
spec:
  action: delay
  mode: all
  selector:
    namespaces:
      - game-production
  delay:
    latency: "100ms"
    jitter: "50ms"
  duration: "5m"
---
apiVersion: chaos-mesh.org/v1alpha1
kind: StressChaos
metadata:
  name: memory-stress
  namespace: game-production
spec:
  mode: one
  selector:
    namespaces:
      - game-production
  stressors:
    memory:
      workers: 4
      size: "256MB"
  duration: "10m"
```

---

## 7. 테스트 자동화

### 7.1 CI/CD 테스트 파이프라인
**.github/workflows/test-pipeline.yml**
```yaml
name: Test Pipeline

on:
  pull_request:
  push:
    branches: [main, develop]

jobs:
  unit-tests:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v3
    
    - name: Build and Test
      run: |
        mkdir build && cd build
        cmake .. -DBUILD_TESTS=ON -DENABLE_COVERAGE=ON
        make -j$(nproc)
        ctest --output-on-failure
    
    - name: Coverage Report
      run: |
        cd build
        lcov --capture --directory . --output-file coverage.info
        bash <(curl -s https://codecov.io/bash)
  
  integration-tests:
    runs-on: ubuntu-latest
    services:
      mysql:
        image: mysql:8.0
        env:
          MYSQL_ROOT_PASSWORD: root
        options: >-
          --health-cmd="mysqladmin ping"
          --health-interval=10s
      redis:
        image: redis:7
        options: >-
          --health-cmd="redis-cli ping"
          --health-interval=10s
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Run Integration Tests
      run: |
        docker-compose -f docker-compose.test.yml up --abort-on-container-exit
  
  performance-tests:
    runs-on: ubuntu-latest
    if: github.event_name == 'push' && github.ref == 'refs/heads/main'
    steps:
    - uses: actions/checkout@v3
    
    - name: Run Performance Tests
      run: |
        docker run -i loadimpact/k6 run -e BASE_URL=${{ secrets.STAGING_URL }} - <tests/performance/load-test.js
    
    - name: Analyze Results
      run: |
        python scripts/analyze-performance.py --baseline previous --current current
```

---

## 8. 테스트 리포트

### 8.1 테스트 대시보드
```html
<!DOCTYPE html>
<html>
<head>
    <title>Test Dashboard</title>
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
</head>
<body>
    <h1>Game Server Test Dashboard</h1>
    
    <div class="metrics">
        <div class="metric">
            <h3>Test Coverage</h3>
            <canvas id="coverageChart"></canvas>
            <p>Current: 82%</p>
        </div>
        
        <div class="metric">
            <h3>Test Pass Rate</h3>
            <canvas id="passRateChart"></canvas>
            <p>Last 7 days: 98.5%</p>
        </div>
        
        <div class="metric">
            <h3>Performance Trend</h3>
            <canvas id="performanceChart"></canvas>
            <p>P95 Latency: 45ms</p>
        </div>
    </div>
    
    <div class="test-results">
        <h2>Recent Test Runs</h2>
        <table>
            <tr>
                <th>Build</th>
                <th>Unit</th>
                <th>Integration</th>
                <th>E2E</th>
                <th>Performance</th>
                <th>Status</th>
            </tr>
            <tr>
                <td>#1234</td>
                <td>✅ 156/156</td>
                <td>✅ 48/48</td>
                <td>✅ 12/12</td>
                <td>✅ Pass</td>
                <td>Success</td>
            </tr>
        </table>
    </div>
</body>
</html>
```

---

## ✅ 체크리스트

### 필수 테스트
- [ ] 단위 테스트 (>80% 커버리지)
- [ ] API 통합 테스트
- [ ] 데이터베이스 테스트
- [ ] 부하 테스트 (5000 동시 사용자)
- [ ] 보안 테스트
- [ ] 회귀 테스트

### 권장 테스트
- [ ] E2E 테스트
- [ ] 성능 프로파일링
- [ ] 카오스 테스트
- [ ] 접근성 테스트
- [ ] 국제화 테스트

## 🎯 다음 단계
→ [12_operation_runbook.md](12_operation_runbook.md) - 운영 런북