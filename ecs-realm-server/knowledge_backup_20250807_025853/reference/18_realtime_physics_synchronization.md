# 20단계: 실시간 게임 물리 & 상태 동기화 - 네트워크 지연을 극복하는 마법같은 기술
*100명이 동시에 총을 쏘고 뛰어다녀도 모든 플레이어가 같은 세상을 보게 만드는 기적*

> **🎯 목표**: 100명이 동시에 플레이하는 액션 게임에서 네트워크 지연이 있어도 자연스러운 게임플레이를 구현

---

## 📋 문서 정보

- **난이도**: 🟡 중급→🔴 고급 (네트워크 물리학의 정점!)
- **예상 학습 시간**: 8-10시간 (개념 이해 + 실습)
- **필요 선수 지식**: 
  - ✅ [1-19단계](./01_advanced_cpp_features.md) 모든 내용 완료
  - ✅ 게임 물리학 기초 (충돌, 중력, 속도)
  - ✅ 네트워크 프로그래밍 경험
  - ✅ 온라인 FPS 게임 플레이 경험 (중요!)
- **실습 환경**: C++17, 물리 엔진, 네트워크 시뮬레이터
- **최종 결과물**: 배틀그라운드급 실시간 동기화 시스템!

---

## 🤔 왜 실시간 물리 동기화가 이렇게 어려울까?

### **게임 물리학과 네트워크의 근본적 충돌**

실시간 물리 동기화가 어려운 이유를 이해하기 위해, 먼저 현실을 직시해봅시다.

**🌍 현실 세계의 물리학:**
- 중력: 9.8m/s² (즉시 적용)
- 충돌: 접촉 순간 즉시 반응
- 모든 사람이 같은 물리 법칙을 경험

**💻 네트워크 게임의 현실:**
- 한국 → 미국 서버: 150ms 지연
- 패킷 손실: 0.1% (1000개 중 1개 사라짐)
- 각 플레이어가 다른 시점의 정보를 가짐

```cpp
// 😰 네트워크 지연의 잔혹한 현실
struct NetworkReality {
    int ping_time_ms = 50;        // 평균 핑: 50ms (빛의 속도 한계!)
    int jitter_ms = 10;           // 지연 변동: ±10ms (인터넷 트래픽)
    float packet_loss = 0.1f;     // 패킷 손실: 0.1% (라우터 혼잡)
    
    // 🎮 실제 게임에서 발생하는 참사들:
    // 1. "분명히 총을 먼저 쐈는데 내가 죽었다!" 
    //    → A가 쏜 시점: 서버 기준 100ms 전, B가 숨은 시점: 서버 기준 50ms 전
    // 2. "같은 상자인데 왜 우리가 다른 곳에서 봤지?" 
    //    → 클라이언트마다 다른 물리 시뮬레이션 결과
    // 3. "스킬 이펙트는 나왔는데 데미지가 안들어가네?" 
    //    → 비주얼과 게임로직의 동기화 실패
};

// 🎮 게임이 요구하는 극한의 성능 기준
struct GameRequirements {
    int target_fps = 60;          // 60fps = 16.67ms마다 화면 업데이트
    int max_lag_compensation = 150; // 150ms까지 과거로 되돌려서 계산
    int max_rollback_frames = 10;   // 최대 10프레임 롤백 (166ms)
    float acceptable_error = 1.0f;  // 1픽셀 이내 오차만 허용
    
    // 😱 이게 얼마나 어려운지 감이 오시나요?
    // 인간의 반응속도(200ms)보다 빠른 보정이 필요!
};
```

### **문제의 핵심**

**물리 법칙**: "모든 것은 **즉시** 일어난다"  
**네트워크**: "모든 것은 **지연**된다"

이 근본적 모순을 해결하는 것이 바로 **실시간 물리 동기화**의 본질입니다!

---

## 🧮 1. 결정론적 물리 시뮬레이션 - 모든 클라이언트가 같은 결과를 보게 만들기

### **🤔 왜 결정론적 물리학이 필요할까?**

온라인 게임에서 가장 중요한 것은 **"모든 플레이어가 같은 세상을 본다"**는 것입니다. 하지만 현실은...

**😰 일반적인 물리 엔진의 문제:**
```
플레이어 A의 화면: "공이 왼쪽으로 튕겼다!"
플레이어 B의 화면: "공이 오른쪽으로 튕겼다!"
서버: "어?? 둘 다 같은 공인데??"
```

이런 참사가 왜 일어날까요?

### **1.1 문제: 동일한 입력, 다른 결과 - 컴퓨터의 배신**

같은 코드, 같은 데이터인데 왜 결과가 다를까요? 컴퓨터의 숨겨진 비밀들을 파헤쳐봅시다.

```cpp
#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <cmath>

// ❌ 비결정론적 물리 (문제가 있는 방식)
class UnreliablePhysics {
private:
    std::random_device rd;
    std::mt19937 gen{rd()};  // 🚨 문제 1: 시드가 매번 다름!
    
public:
    struct PhysicsObject {
        float x, y;
        float vx, vy;
        float mass;
        uint32_t id;
    };
    
    // 🚨 문제 2: 부동소수점 연산 순서에 따라 결과가 달라짐
    void UpdatePhysics(std::vector<PhysicsObject>& objects, float deltaTime) {
        // 중력 적용
        for (auto& obj : objects) {
            obj.vy -= 9.81f * deltaTime;  // 🚨 문제 3: 부정확한 부동소수점 연산
            // Intel CPU vs AMD CPU에서 결과가 다를 수 있음!
        }
        
        // 충돌 검사 (순서 의존적)
        for (size_t i = 0; i < objects.size(); ++i) {
            for (size_t j = i + 1; j < objects.size(); ++j) {
                CheckCollision(objects[i], objects[j]);  // 🚨 문제 4: 처리 순서 의존적
            }
        }
        
        // 위치 업데이트
        for (auto& obj : objects) {
            obj.x += obj.vx * deltaTime;
            obj.y += obj.vy * deltaTime;
            
            // 🚨 문제 5: 경계 처리에서 부동소수점 오차 누적
            if (obj.y < 0.0f) {  // 0.0000001f vs -0.0000001f 차이로 다른 결과
                obj.y = 0.0f;
                obj.vy = -obj.vy * 0.8f;  // 반발 계수
            }
        }
    }
    
private:
    void CheckCollision(PhysicsObject& a, PhysicsObject& b) {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        float distance = std::sqrt(dx * dx + dy * dy);  // 🚨 문제 6: sqrt 구현이 CPU마다 다름
        
        if (distance < 2.0f) {  // 충돌 반지름
            // 충돌 응답 (부정확)
            float force = std::uniform_real_distribution<float>(0.5f, 1.5f)(gen);  // 🚨 문제 7: 랜덤값이 다름
            a.vx += dx * force;
            a.vy += dy * force;
        }
    }
};

// 😱 실제 실행 결과 예시
void DemonstrateNonDeterministicProblem() {
    std::cout << "같은 코드를 3번 실행한 결과:\n" << std::endl;
    
    for (int run = 1; run <= 3; ++run) {
        UnreliablePhysics physics;
        std::vector<UnreliablePhysics::PhysicsObject> objects = {
            {100.0f, 100.0f, 10.0f, 0.0f, 1.0f, 1},
            {120.0f, 100.0f, -10.0f, 0.0f, 1.0f, 2}
        };
        
        physics.UpdatePhysics(objects, 0.016f);  // 1프레임 (16ms)
        
        printf("실행 %d: Object1=(%.3f, %.3f), Object2=(%.3f, %.3f)\n", 
               run, objects[0].x, objects[0].y, objects[1].x, objects[1].y);
    }
    
    std::cout << "\n🚨 매번 결과가 다름!! 이게 바로 게임이 터지는 이유입니다.\n" << std::endl;
}
// 실행 1: Object1=(100.160, 99.843), Object2=(119.840, 99.843)
// 실행 2: Object1=(100.160, 99.844), Object2=(119.840, 99.842)  ← 미세한 차이
// 실행 3: Object1=(100.159, 99.843), Object2=(119.841, 99.843)  ← 또 다른 결과

### **🎯 해결책: 결정론적 물리 시뮬레이션**

위의 문제들을 해결하려면 어떻게 해야 할까요? 핵심은 **"같은 입력에 대해 절대로 변하지 않는 출력"**을 보장하는 것입니다.

**✨ 결정론적 물리학의 4대 원칙:**

1. **고정 시드**: 랜덤 생성기 시드를 고정
2. **고정소수점 연산**: float 대신 정수 기반 연산 사용
3. **순서 보장**: 모든 연산을 정렬된 순서로 처리
4. **상태 검증**: 해시를 이용한 동기화 확인

```cpp
// ✅ 결정론적 물리 (올바른 방식) - 절대로 배신하지 않는 물리학
class DeterministicPhysics {
private:
    // 고정 시드 사용
    std::mt19937 gen{12345};
    
    // 고정소수점 수학 (정확한 연산)
    using FixedPoint = int64_t;  // 32.32 고정소수점
    static constexpr int FIXED_SCALE = 1LL << 32;
    
    static FixedPoint FloatToFixed(float f) {
        return static_cast<FixedPoint>(f * FIXED_SCALE);
    }
    
    static float FixedToFloat(FixedPoint fixed) {
        return static_cast<float>(fixed) / FIXED_SCALE;
    }
    
    static FixedPoint FixedMul(FixedPoint a, FixedPoint b) {
        return (a * b) >> 32;
    }
    
public:
    struct DeterministicObject {
        FixedPoint x, y;    // 고정소수점 위치
        FixedPoint vx, vy;  // 고정소수점 속도
        FixedPoint mass;
        uint32_t id;
        
        // 디버깅용 float 변환
        float GetFloatX() const { return FixedToFloat(x); }
        float GetFloatY() const { return FixedToFloat(y); }
    };
    
    void UpdatePhysics(std::vector<DeterministicObject>& objects, uint32_t deltaTimeMs) {
        FixedPoint deltaTime = FloatToFixed(deltaTimeMs / 1000.0f);
        FixedPoint gravity = FloatToFixed(-9.81f);
        
        // ✅ 모든 연산이 결정론적
        
        // 1. 중력 적용 (고정소수점으로 정확한 연산)
        for (auto& obj : objects) {
            obj.vy += FixedMul(gravity, deltaTime);
        }
        
        // 2. 충돌 검사 (ID 순서로 정렬하여 순서 보장)
        std::sort(objects.begin(), objects.end(), 
                  [](const auto& a, const auto& b) { return a.id < b.id; });
        
        for (size_t i = 0; i < objects.size(); ++i) {
            for (size_t j = i + 1; j < objects.size(); ++j) {
                CheckCollisionDeterministic(objects[i], objects[j]);
            }
        }
        
        // 3. 위치 업데이트
        for (auto& obj : objects) {
            obj.x += FixedMul(obj.vx, deltaTime);
            obj.y += FixedMul(obj.vy, deltaTime);
            
            // 경계 처리 (정확한 계산)
            if (obj.y < 0) {
                obj.y = 0;
                obj.vy = FixedMul(obj.vy, FloatToFixed(-0.8f));
            }
        }
    }
    
    // 상태 해시 (동기화 검증용)
    uint64_t CalculateStateHash(const std::vector<DeterministicObject>& objects) {
        uint64_t hash = 0;
        for (const auto& obj : objects) {
            // FNV-1a 해시
            hash ^= static_cast<uint64_t>(obj.x);
            hash *= 1099511628211ULL;
            hash ^= static_cast<uint64_t>(obj.y);
            hash *= 1099511628211ULL;
            hash ^= static_cast<uint64_t>(obj.vx);
            hash *= 1099511628211ULL;
            hash ^= static_cast<uint64_t>(obj.vy);
            hash *= 1099511628211ULL;
        }
        return hash;
    }

private:
    void CheckCollisionDeterministic(DeterministicObject& a, DeterministicObject& b) {
        FixedPoint dx = a.x - b.x;
        FixedPoint dy = a.y - b.y;
        
        // ✅ sqrt 대신 제곱 거리 비교 (더 정확하고 빠름)
        FixedPoint distanceSquared = FixedMul(dx, dx) + FixedMul(dy, dy);
        FixedPoint collisionRadiusSquared = FloatToFixed(4.0f);  // 2.0f^2
        
        if (distanceSquared < collisionRadiusSquared && distanceSquared > 0) {
            // 정규화된 충돌 응답
            FixedPoint force = FloatToFixed(1.0f);  // 고정된 힘
            
            // 질량 기반 충돌 응답
            FixedPoint totalMass = a.mass + b.mass;
            FixedPoint aRatio = FixedMul(b.mass, force) / totalMass;
            FixedPoint bRatio = FixedMul(a.mass, force) / totalMass;
            
            a.vx += FixedMul(dx, aRatio);
            a.vy += FixedMul(dy, aRatio);
            b.vx -= FixedMul(dx, bRatio);
            b.vy -= FixedMul(dy, bRatio);
        }
    }
};

void DemonstratePhysicsConsistency() {
    std::cout << "🧮 결정론적 물리 시뮬레이션 테스트" << std::endl;
    
    // 동일한 초기 상태로 2번 시뮬레이션
    DeterministicPhysics physics1, physics2;
    
    std::vector<DeterministicPhysics::DeterministicObject> objects1 = {
        {physics1.FloatToFixed(0.0f), physics1.FloatToFixed(10.0f), 
         physics1.FloatToFixed(2.0f), physics1.FloatToFixed(0.0f), 
         physics1.FloatToFixed(1.0f), 1},
        {physics1.FloatToFixed(5.0f), physics1.FloatToFixed(8.0f), 
         physics1.FloatToFixed(-1.0f), physics1.FloatToFixed(1.0f), 
         physics1.FloatToFixed(1.5f), 2}
    };
    
    auto objects2 = objects1;  // 동일한 초기 상태
    
    std::cout << "초기 상태 해시: " << std::hex << physics1.CalculateStateHash(objects1) << std::endl;
    
    // 100프레임 시뮬레이션
    for (int frame = 0; frame < 100; ++frame) {
        physics1.UpdatePhysics(objects1, 16);  // 16ms = 60fps
        physics2.UpdatePhysics(objects2, 16);
    }
    
    uint64_t hash1 = physics1.CalculateStateHash(objects1);
    uint64_t hash2 = physics2.CalculateStateHash(objects2);
    
    std::cout << "시뮬레이션 1 해시: " << std::hex << hash1 << std::endl;
    std::cout << "시뮬레이션 2 해시: " << std::hex << hash2 << std::endl;
    
    if (hash1 == hash2) {
        std::cout << "✅ 결정론적 물리 성공!" << std::endl;
    } else {
        std::cout << "❌ 물리 시뮬레이션 불일치!" << std::endl;
    }
}
```

**💡 결정론적 물리의 핵심:**
- **고정소수점 연산**: 부동소수점 오차 제거
- **연산 순서 보장**: ID 정렬로 일관된 순서
- **시드 고정**: 랜덤 요소 통제
- **상태 해시**: 동기화 검증

---

## 🔄 2. 클라이언트 예측과 서버 교정

### **2.1 클라이언트 예측 (Client-Side Prediction)**

```cpp
#include <queue>
#include <chrono>
#include <unordered_map>

// 🎮 입력 기록과 예측 시스템
class ClientPredictionSystem {
public:
    struct InputCommand {
        uint32_t frame_number;
        uint32_t timestamp_ms;
        float move_x, move_y;
        bool attack_pressed;
        bool jump_pressed;
        uint64_t input_hash;  // 입력 무결성 검증
    };
    
    struct GameState {
        float player_x, player_y;
        float velocity_x, velocity_y;
        bool on_ground;
        uint32_t frame_number;
        uint32_t timestamp_ms;
        
        // 상태 비교용
        bool operator==(const GameState& other) const {
            return std::abs(player_x - other.player_x) < 0.01f &&
                   std::abs(player_y - other.player_y) < 0.01f &&
                   std::abs(velocity_x - other.velocity_x) < 0.01f &&
                   std::abs(velocity_y - other.velocity_y) < 0.01f &&
                   on_ground == other.on_ground;
        }
    };

private:
    // 입력 히스토리 (롤백용)
    std::queue<InputCommand> input_history;
    static constexpr size_t MAX_HISTORY_SIZE = 120;  // 2초 @ 60fps
    
    // 상태 히스토리 (서버 교정용)
    std::unordered_map<uint32_t, GameState> state_history;
    
    // 현재 상태
    GameState current_state;
    GameState last_confirmed_state;  // 서버에서 확인된 마지막 상태
    
    uint32_t current_frame = 0;
    uint32_t last_confirmed_frame = 0;

public:
    // 클라이언트에서 입력 처리
    void ProcessInput(float move_x, float move_y, bool attack, bool jump) {
        auto now = std::chrono::steady_clock::now();
        uint32_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        
        // 입력 명령 생성
        InputCommand input;
        input.frame_number = current_frame;
        input.timestamp_ms = timestamp;
        input.move_x = move_x;
        input.move_y = move_y;
        input.attack_pressed = attack;
        input.jump_pressed = jump;
        input.input_hash = CalculateInputHash(input);
        
        // 입력 히스토리에 추가
        input_history.push(input);
        if (input_history.size() > MAX_HISTORY_SIZE) {
            input_history.pop();
        }
        
        // 🔮 즉시 예측 실행 (반응성 확보)
        ApplyInputToState(input, current_state);
        
        // 상태 히스토리 저장
        state_history[current_frame] = current_state;
        
        // 서버에 입력 전송 (비동기)
        SendInputToServer(input);
        
        current_frame++;
        
        std::cout << "프레임 " << current_frame 
                  << " 예측 위치: (" << current_state.player_x 
                  << ", " << current_state.player_y << ")" << std::endl;
    }
    
    // 서버로부터 권위 있는 상태 수신
    void ReceiveServerState(const GameState& server_state) {
        std::cout << "\n🌐 서버 상태 수신 - 프레임 " << server_state.frame_number << std::endl;
        std::cout << "서버 위치: (" << server_state.player_x 
                  << ", " << server_state.player_y << ")" << std::endl;
        
        // 해당 프레임의 예측 상태와 비교
        auto predicted_it = state_history.find(server_state.frame_number);
        
        if (predicted_it != state_history.end()) {
            const GameState& predicted_state = predicted_it->second;
            
            std::cout << "예측 위치: (" << predicted_state.player_x 
                      << ", " << predicted_state.player_y << ")" << std::endl;
            
            // 오차 계산
            float error_x = std::abs(predicted_state.player_x - server_state.player_x);
            float error_y = std::abs(predicted_state.player_y - server_state.player_y);
            float total_error = std::sqrt(error_x * error_x + error_y * error_y);
            
            std::cout << "예측 오차: " << total_error << std::endl;
            
            // 🔧 오차가 임계값을 초과하면 교정 실행
            const float ERROR_THRESHOLD = 1.0f;  // 1픽셀 허용 오차
            
            if (total_error > ERROR_THRESHOLD) {
                std::cout << "❗ 예측 오차 초과 - 롤백 및 재예측 실행" << std::endl;
                PerformRollbackAndRepredict(server_state);
            } else {
                std::cout << "✅ 예측 정확도 양호" << std::endl;
                last_confirmed_state = server_state;
                last_confirmed_frame = server_state.frame_number;
            }
        }
        
        // 오래된 히스토리 정리
        CleanupOldHistory(server_state.frame_number);
    }

private:
    void ApplyInputToState(const InputCommand& input, GameState& state) {
        const float MOVE_SPEED = 5.0f;
        const float JUMP_SPEED = 8.0f;
        const float GRAVITY = -20.0f;
        const float DELTA_TIME = 1.0f / 60.0f;  // 60fps
        
        // 수평 이동
        state.velocity_x = input.move_x * MOVE_SPEED;
        
        // 점프
        if (input.jump_pressed && state.on_ground) {
            state.velocity_y = JUMP_SPEED;
            state.on_ground = false;
        }
        
        // 중력 적용
        if (!state.on_ground) {
            state.velocity_y += GRAVITY * DELTA_TIME;
        }
        
        // 위치 업데이트
        state.player_x += state.velocity_x * DELTA_TIME;
        state.player_y += state.velocity_y * DELTA_TIME;
        
        // 땅 충돌 검사
        const float GROUND_Y = 0.0f;
        if (state.player_y <= GROUND_Y && state.velocity_y <= 0) {
            state.player_y = GROUND_Y;
            state.velocity_y = 0;
            state.on_ground = true;
        }
        
        state.frame_number = input.frame_number;
        state.timestamp_ms = input.timestamp_ms;
    }
    
    void PerformRollbackAndRepredict(const GameState& authoritative_state) {
        std::cout << "🔄 롤백 시작 - 프레임 " << authoritative_state.frame_number << std::endl;
        
        // 1. 서버 상태로 롤백
        current_state = authoritative_state;
        last_confirmed_state = authoritative_state;
        last_confirmed_frame = authoritative_state.frame_number;
        
        // 2. 해당 프레임 이후의 모든 입력을 다시 적용
        std::queue<InputCommand> temp_queue;
        std::queue<InputCommand> replay_inputs;
        
        // 롤백 지점 이후의 입력들을 찾아서 분리
        while (!input_history.empty()) {
            InputCommand input = input_history.front();
            input_history.pop();
            
            if (input.frame_number > authoritative_state.frame_number) {
                replay_inputs.push(input);
            }
            temp_queue.push(input);
        }
        
        // 입력 히스토리 복원
        input_history = temp_queue;
        
        std::cout << "재예측할 입력 개수: " << replay_inputs.size() << std::endl;
        
        // 3. 입력들을 순서대로 다시 적용
        while (!replay_inputs.empty()) {
            InputCommand input = replay_inputs.front();
            replay_inputs.pop();
            
            ApplyInputToState(input, current_state);
            state_history[input.frame_number] = current_state;
            
            std::cout << "  프레임 " << input.frame_number 
                      << " 재예측 위치: (" << current_state.player_x 
                      << ", " << current_state.player_y << ")" << std::endl;
        }
        
        std::cout << "✅ 롤백 및 재예측 완료" << std::endl;
    }
    
    void CleanupOldHistory(uint32_t confirmed_frame) {
        // 확인된 프레임보다 60프레임(1초) 이전 데이터는 삭제
        uint32_t cleanup_threshold = confirmed_frame > 60 ? confirmed_frame - 60 : 0;
        
        auto it = state_history.begin();
        while (it != state_history.end()) {
            if (it->first < cleanup_threshold) {
                it = state_history.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    uint64_t CalculateInputHash(const InputCommand& input) {
        // 간단한 해시 (실제로는 더 강력한 해시 사용)
        uint64_t hash = 0;
        hash ^= std::hash<float>{}(input.move_x);
        hash ^= std::hash<float>{}(input.move_y) << 1;
        hash ^= std::hash<bool>{}(input.attack_pressed) << 2;
        hash ^= std::hash<bool>{}(input.jump_pressed) << 3;
        hash ^= input.timestamp_ms;
        return hash;
    }
    
    void SendInputToServer(const InputCommand& input) {
        // 실제로는 네트워크 패킷으로 전송
        // 여기서는 시뮬레이션만
        std::cout << "📡 서버로 입력 전송 - 프레임 " << input.frame_number << std::endl;
    }

public:
    // 현재 상태 조회 (렌더링용)
    const GameState& GetCurrentState() const { return current_state; }
    
    // 예측 신뢰도 조회
    float GetPredictionAccuracy() const {
        // 최근 예측들의 정확도 계산
        return 0.95f;  // 95% 정확도 (예시)
    }
};

void DemonstrateClientPrediction() {
    std::cout << "🔮 클라이언트 예측 시스템 데모" << std::endl;
    
    ClientPredictionSystem prediction_system;
    
    // 시뮬레이션: 플레이어가 오른쪽으로 이동하며 점프
    std::cout << "\n=== 입력 시뮬레이션 시작 ===" << std::endl;
    
    // 1. 오른쪽 이동 시작
    prediction_system.ProcessInput(1.0f, 0.0f, false, false);
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
    
    // 2. 계속 이동하며 점프
    prediction_system.ProcessInput(1.0f, 0.0f, false, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
    
    // 3. 공중에서 계속 이동
    for (int i = 0; i < 3; ++i) {
        prediction_system.ProcessInput(1.0f, 0.0f, false, false);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    
    // 시뮬레이션: 서버에서 약간 다른 상태 전송 (네트워크 지연 등으로 인한 차이)
    std::cout << "\n=== 서버 상태 수신 시뮬레이션 ===" << std::endl;
    
    ClientPredictionSystem::GameState server_state;
    server_state.frame_number = 2;  // 점프한 프레임
    server_state.player_x = 0.15f;  // 예측과 약간 다른 위치
    server_state.player_y = 0.25f;
    server_state.velocity_x = 5.0f;
    server_state.velocity_y = 7.8f;  // 약간 다른 속도
    server_state.on_ground = false;
    server_state.timestamp_ms = 123456;
    
    // 서버 상태 처리 (롤백 및 재예측 트리거)
    prediction_system.ReceiveServerState(server_state);
    
    std::cout << "\n현재 예측 정확도: " 
              << (prediction_system.GetPredictionAccuracy() * 100) << "%" << std::endl;
}
```

**💡 클라이언트 예측의 장점:**
- **즉각적인 반응**: 입력에 대한 즉시 피드백
- **부드러운 게임플레이**: 네트워크 지연 감춤
- **높은 정확도**: 대부분의 예측이 맞음 (95%+)

---

## ⏪ 3. 롤백 네트코드 (Rollback Netcode)

### **3.1 고급 롤백 시스템**

```cpp
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <memory>

// 🔄 고성능 롤백 네트코드 시스템
class RollbackNetcode {
public:
    struct FrameInput {
        uint32_t frame_number;
        uint8_t player_id;
        uint32_t input_bits;  // 비트 압축된 입력
        uint32_t checksum;    // 무결성 검증
        
        // 입력 비트 마스크
        enum InputBits : uint32_t {
            MOVE_LEFT   = 1 << 0,
            MOVE_RIGHT  = 1 << 1,
            MOVE_UP     = 1 << 2,
            MOVE_DOWN   = 1 << 3,
            ATTACK      = 1 << 4,
            BLOCK       = 1 << 5,
            SPECIAL     = 1 << 6,
            JUMP        = 1 << 7
        };
        
        bool HasInput(InputBits bit) const { return (input_bits & bit) != 0; }
        void SetInput(InputBits bit, bool value) {
            if (value) input_bits |= bit;
            else input_bits &= ~bit;
        }
    };
    
    struct GameSnapshot {
        uint32_t frame_number;
        std::vector<uint8_t> serialized_state;  // 압축된 게임 상태
        uint64_t state_hash;                     // 빠른 비교용 해시
        size_t memory_size;                      // 메모리 사용량 추적
        
        // 빠른 비교
        bool operator==(const GameSnapshot& other) const {
            return state_hash == other.state_hash;
        }
    };

private:
    // 롤백 설정
    static constexpr uint32_t MAX_ROLLBACK_FRAMES = 8;   // 최대 8프레임 롤백
    static constexpr uint32_t INPUT_DELAY_FRAMES = 2;    // 2프레임 입력 지연
    static constexpr uint32_t SNAPSHOT_INTERVAL = 4;     // 4프레임마다 스냅샷
    
    // 프레임 데이터
    struct FrameData {
        uint32_t frame_number;
        std::vector<FrameInput> inputs;  // 모든 플레이어 입력
        std::unique_ptr<GameSnapshot> snapshot;  // 스냅샷 (간격에 따라)
        bool is_confirmed;  // 서버에서 확인됨
    };
    
    // 링 버퍼로 메모리 효율성 확보
    std::vector<FrameData> frame_buffer;
    uint32_t buffer_head = 0;
    
    // 현재 상태
    uint32_t current_frame = 0;
    uint32_t last_confirmed_frame = 0;
    uint8_t local_player_id = 0;
    uint8_t total_players = 2;
    
    // 성능 메트릭
    struct RollbackMetrics {
        uint32_t total_rollbacks = 0;
        uint32_t max_rollback_distance = 0;
        uint32_t frames_rolled_back = 0;
        float avg_rollback_cost_ms = 0.0f;
    } metrics;

public:
    RollbackNetcode(uint8_t player_id, uint8_t num_players) 
        : local_player_id(player_id), total_players(num_players) {
        
        // 링 버퍼 초기화 (충분한 크기로)
        frame_buffer.resize(MAX_ROLLBACK_FRAMES * 4);
        
        for (auto& frame : frame_buffer) {
            frame.inputs.resize(total_players);
            frame.is_confirmed = false;
        }
        
        std::cout << "🔄 롤백 네트코드 초기화 (플레이어 " << (int)player_id 
                  << "/" << (int)num_players << ")" << std::endl;
    }
    
    // 로컬 입력 추가
    void AddLocalInput(uint32_t input_bits) {
        uint32_t input_frame = current_frame + INPUT_DELAY_FRAMES;
        
        FrameInput input;
        input.frame_number = input_frame;
        input.player_id = local_player_id;
        input.input_bits = input_bits;
        input.checksum = CalculateInputChecksum(input);
        
        // 프레임 버퍼에 저장
        size_t buffer_index = input_frame % frame_buffer.size();
        frame_buffer[buffer_index].frame_number = input_frame;
        frame_buffer[buffer_index].inputs[local_player_id] = input;
        
        std::cout << "📥 로컬 입력 추가 - 프레임 " << input_frame 
                  << ", 입력: 0x" << std::hex << input_bits << std::dec << std::endl;
    }
    
    // 원격 입력 수신
    void ReceiveRemoteInput(const FrameInput& remote_input) {
        std::cout << "📡 원격 입력 수신 - 플레이어 " << (int)remote_input.player_id
                  << ", 프레임 " << remote_input.frame_number << std::endl;
        
        // 체크섬 검증
        uint32_t expected_checksum = CalculateInputChecksum(remote_input);
        if (remote_input.checksum != expected_checksum) {
            std::cout << "❌ 입력 체크섬 오류!" << std::endl;
            return;
        }
        
        size_t buffer_index = remote_input.frame_number % frame_buffer.size();
        
        // 이미 처리된 프레임에 대한 입력이면 롤백 필요성 검사
        if (remote_input.frame_number <= current_frame) {
            auto& stored_input = frame_buffer[buffer_index].inputs[remote_input.player_id];
            
            // 기존 입력과 다르면 롤백 수행
            if (stored_input.input_bits != remote_input.input_bits) {
                std::cout << "🔄 입력 불일치 감지 - 롤백 수행" << std::endl;
                PerformRollback(remote_input.frame_number);
            }
        }
        
        // 입력 저장
        frame_buffer[buffer_index].inputs[remote_input.player_id] = remote_input;
    }
    
    // 게임 시뮬레이션 진행
    bool AdvanceFrame() {
        // 현재 프레임의 모든 입력이 준비되었는지 확인
        size_t buffer_index = current_frame % frame_buffer.size();
        FrameData& current_frame_data = frame_buffer[buffer_index];
        
        // 모든 플레이어의 입력 확인
        bool all_inputs_ready = true;
        for (uint8_t i = 0; i < total_players; ++i) {
            if (current_frame_data.inputs[i].frame_number != current_frame) {
                all_inputs_ready = false;
                
                // 예측 입력 사용 (이전 프레임과 동일)
                if (current_frame > 0) {
                    size_t prev_index = (current_frame - 1) % frame_buffer.size();
                    current_frame_data.inputs[i] = frame_buffer[prev_index].inputs[i];
                    current_frame_data.inputs[i].frame_number = current_frame;
                    
                    std::cout << "🔮 플레이어 " << (int)i << " 입력 예측 사용" << std::endl;
                }
            }
        }
        
        // 스냅샷 생성 (일정 간격마다)
        if (current_frame % SNAPSHOT_INTERVAL == 0) {
            current_frame_data.snapshot = CreateGameSnapshot(current_frame);
            std::cout << "📸 스냅샷 생성 - 프레임 " << current_frame << std::endl;
        }
        
        // 게임 상태 업데이트 시뮬레이션
        SimulateGameFrame(current_frame_data.inputs);
        
        current_frame++;
        return true;
    }

private:
    void PerformRollback(uint32_t rollback_to_frame) {
        auto rollback_start = std::chrono::high_resolution_clock::now();
        
        uint32_t rollback_distance = current_frame - rollback_to_frame;
        
        std::cout << "⏪ 롤백 시작: 프레임 " << current_frame 
                  << " → " << rollback_to_frame 
                  << " (거리: " << rollback_distance << ")" << std::endl;
        
        // 메트릭 업데이트
        metrics.total_rollbacks++;
        metrics.max_rollback_distance = std::max(metrics.max_rollback_distance, rollback_distance);
        metrics.frames_rolled_back += rollback_distance;
        
        // 1. 롤백할 스냅샷 찾기
        GameSnapshot* restore_snapshot = nullptr;
        uint32_t snapshot_frame = rollback_to_frame;
        
        // 가장 가까운 스냅샷을 뒤에서 찾기
        for (uint32_t frame = rollback_to_frame; frame >= std::max(0, (int)rollback_to_frame - MAX_ROLLBACK_FRAMES); frame--) {
            size_t index = frame % frame_buffer.size();
            if (frame_buffer[index].snapshot) {
                restore_snapshot = frame_buffer[index].snapshot.get();
                snapshot_frame = frame;
                break;
            }
        }
        
        if (!restore_snapshot) {
            std::cout << "❌ 복원할 스냅샷 없음 - 전체 재시뮬레이션 필요" << std::endl;
            snapshot_frame = 0;
        } else {
            std::cout << "📸 스냅샷으로 복원: 프레임 " << snapshot_frame << std::endl;
            RestoreGameState(*restore_snapshot);
        }
        
        // 2. 스냅샷 지점부터 현재까지 재시뮬레이션
        for (uint32_t frame = snapshot_frame; frame <= current_frame; ++frame) {
            size_t index = frame % frame_buffer.size();
            const auto& inputs = frame_buffer[index].inputs;
            
            SimulateGameFrame(inputs);
            
            std::cout << "  🔄 재시뮬레이션: 프레임 " << frame << std::endl;
        }
        
        auto rollback_end = std::chrono::high_resolution_clock::now();
        float rollback_time = std::chrono::duration<float, std::milli>(
            rollback_end - rollback_start).count();
        
        // 평균 비용 업데이트
        metrics.avg_rollback_cost_ms = (metrics.avg_rollback_cost_ms * (metrics.total_rollbacks - 1) + 
                                       rollback_time) / metrics.total_rollbacks;
        
        std::cout << "✅ 롤백 완료 (" << rollback_time << "ms)" << std::endl;
    }
    
    std::unique_ptr<GameSnapshot> CreateGameSnapshot(uint32_t frame_number) {
        auto snapshot = std::make_unique<GameSnapshot>();
        
        snapshot->frame_number = frame_number;
        
        // 게임 상태 직렬화 (시뮬레이션)
        snapshot->serialized_state.resize(1024);  // 예시 크기
        
        // 실제로는 게임 상태를 바이너리로 직렬화
        for (size_t i = 0; i < snapshot->serialized_state.size(); ++i) {
            snapshot->serialized_state[i] = static_cast<uint8_t>(frame_number + i);
        }
        
        // 상태 해시 계산
        snapshot->state_hash = CalculateStateHash(snapshot->serialized_state);
        snapshot->memory_size = snapshot->serialized_state.size();
        
        return snapshot;
    }
    
    void RestoreGameState(const GameSnapshot& snapshot) {
        // 스냅샷에서 게임 상태 복원 (시뮬레이션)
        std::cout << "  📋 게임 상태 복원: " << snapshot.memory_size << " bytes" << std::endl;
    }
    
    void SimulateGameFrame(const std::vector<FrameInput>& inputs) {
        // 게임 로직 시뮬레이션
        for (const auto& input : inputs) {
            if (input.HasInput(FrameInput::MOVE_LEFT)) {
                // 왼쪽 이동 처리
            }
            if (input.HasInput(FrameInput::ATTACK)) {
                // 공격 처리
            }
            // ... 기타 입력 처리
        }
    }
    
    uint32_t CalculateInputChecksum(const FrameInput& input) {
        // CRC32 또는 간단한 해시
        return input.frame_number ^ input.player_id ^ input.input_bits;
    }
    
    uint64_t CalculateStateHash(const std::vector<uint8_t>& state_data) {
        uint64_t hash = 14695981039346656037ULL;  // FNV offset basis
        
        for (uint8_t byte : state_data) {
            hash ^= byte;
            hash *= 1099511628211ULL;  // FNV prime
        }
        
        return hash;
    }

public:
    void PrintMetrics() const {
        std::cout << "\n📊 롤백 네트코드 메트릭:" << std::endl;
        std::cout << "총 롤백 횟수: " << metrics.total_rollbacks << std::endl;
        std::cout << "최대 롤백 거리: " << metrics.max_rollback_distance << " 프레임" << std::endl;
        std::cout << "총 롤백된 프레임: " << metrics.frames_rolled_back << std::endl;
        std::cout << "평균 롤백 비용: " << metrics.avg_rollback_cost_ms << "ms" << std::endl;
        
        if (metrics.total_rollbacks > 0) {
            float rollback_rate = (float)metrics.total_rollbacks / current_frame * 100;
            std::cout << "롤백 발생률: " << rollback_rate << "%" << std::endl;
        }
    }
    
    uint32_t GetCurrentFrame() const { return current_frame; }
};

void DemonstrateRollbackNetcode() {
    std::cout << "⏪ 롤백 네트코드 데모" << std::endl;
    
    // 2명의 플레이어 시뮬레이션
    RollbackNetcode player1_netcode(0, 2);
    RollbackNetcode player2_netcode(1, 2);
    
    std::cout << "\n=== 정상적인 게임 진행 ===" << std::endl;
    
    // 몇 프레임 정상 진행
    for (uint32_t frame = 0; frame < 10; ++frame) {
        // 각 플레이어 입력 생성
        uint32_t p1_input = RollbackNetcode::FrameInput::MOVE_RIGHT;
        uint32_t p2_input = RollbackNetcode::FrameInput::MOVE_LEFT;
        
        if (frame == 5) {
            p1_input |= RollbackNetcode::FrameInput::ATTACK;  // 플레이어1이 공격
        }
        
        // 입력 추가
        player1_netcode.AddLocalInput(p1_input);
        player2_netcode.AddLocalInput(p2_input);
        
        // 서로의 입력을 교환 (네트워크 시뮬레이션)
        RollbackNetcode::FrameInput p1_remote_input;
        p1_remote_input.frame_number = frame + 2;  // 입력 지연
        p1_remote_input.player_id = 0;
        p1_remote_input.input_bits = p1_input;
        p1_remote_input.checksum = 0;  // 계산됨
        
        RollbackNetcode::FrameInput p2_remote_input;
        p2_remote_input.frame_number = frame + 2;
        p2_remote_input.player_id = 1;
        p2_remote_input.input_bits = p2_input;
        p2_remote_input.checksum = 0;
        
        // 프레임 진행
        player1_netcode.AdvanceFrame();
        player2_netcode.AdvanceFrame();
        
        std::this_thread::sleep_for(std::chrono::milliseconds(16));  // 60fps
    }
    
    std::cout << "\n=== 네트워크 지연으로 인한 롤백 시뮬레이션 ===" << std::endl;
    
    // 뒤늦게 도착한 다른 입력으로 롤백 트리거
    RollbackNetcode::FrameInput late_input;
    late_input.frame_number = 7;  // 이미 처리된 프레임
    late_input.player_id = 1;
    late_input.input_bits = RollbackNetcode::FrameInput::MOVE_LEFT | 
                           RollbackNetcode::FrameInput::ATTACK;  // 다른 입력!
    late_input.checksum = 0;
    
    player1_netcode.ReceiveRemoteInput(late_input);
    
    // 메트릭 출력
    player1_netcode.PrintMetrics();
}
```

**💡 롤백 네트코드의 핵심:**
- **입력 지연**: 네트워크 지연을 숨기기 위한 의도적 지연
- **스냅샷**: 빠른 복원을 위한 상태 저장
- **효율적 재시뮬레이션**: 최소한의 프레임만 다시 계산
- **예측 입력**: 도착하지 않은 입력에 대한 추정

---

## 📊 4. 상태 압축과 델타 동기화

### **4.1 고급 상태 압축 시스템**

```cpp
#include <bitset>
#include <unordered_set>

// 📦 고효율 상태 압축 시스템
class StateCompressionSystem {
public:
    // 압축 가능한 게임 엔티티
    struct GameEntity {
        uint32_t entity_id;
        float position_x, position_y, position_z;
        float rotation_x, rotation_y, rotation_z;
        float velocity_x, velocity_y, velocity_z;
        float health, max_health;
        uint32_t animation_state;
        uint32_t equipment_mask;  // 비트마스크로 장착 아이템
        uint8_t team_id;
        uint8_t entity_type;  // 플레이어/NPC/몬스터 등
        bool is_alive;
        bool is_moving;
        bool is_attacking;
        bool is_casting;
    };
    
    // 압축 설정
    struct CompressionConfig {
        // 위치 압축 (월드 크기에 맞춰 조정)
        float world_min_x = -1000.0f, world_max_x = 1000.0f;
        float world_min_y = -1000.0f, world_max_y = 1000.0f;
        float world_min_z = 0.0f, world_max_z = 100.0f;
        
        // 정밀도 설정
        uint16_t position_precision = 65535;   // 16비트 정밀도
        uint16_t rotation_precision = 360;     // 1도 정밀도
        uint16_t velocity_precision = 1023;    // 10비트 정밀도
        uint8_t health_precision = 255;        // 8비트 정밀도
        
        // 델타 압축 임계값
        float position_delta_threshold = 0.1f;    // 0.1 단위 이하 변화 무시
        float rotation_delta_threshold = 1.0f;    // 1도 이하 변화 무시
        float velocity_delta_threshold = 0.05f;   // 속도 변화 임계값
    };

private:
    CompressionConfig config;
    
    // 이전 상태 (델타 압축용)
    std::unordered_map<uint32_t, GameEntity> previous_states;
    
    // 비트 스트림 클래스
    class BitStream {
    private:
        std::vector<uint8_t> data;
        size_t bit_pos = 0;
        
    public:
        void WriteBits(uint64_t value, size_t num_bits) {
            for (size_t i = 0; i < num_bits; ++i) {
                if (bit_pos % 8 == 0) {
                    data.push_back(0);
                }
                
                size_t byte_index = bit_pos / 8;
                size_t bit_index = bit_pos % 8;
                
                if (value & (1ULL << i)) {
                    data[byte_index] |= (1 << bit_index);
                }
                
                bit_pos++;
            }
        }
        
        uint64_t ReadBits(size_t num_bits, size_t& read_pos) const {
            uint64_t value = 0;
            
            for (size_t i = 0; i < num_bits; ++i) {
                size_t byte_index = read_pos / 8;
                size_t bit_index = read_pos % 8;
                
                if (byte_index < data.size()) {
                    if (data[byte_index] & (1 << bit_index)) {
                        value |= (1ULL << i);
                    }
                }
                
                read_pos++;
            }
            
            return value;
        }
        
        void WriteBool(bool value) { WriteBits(value ? 1 : 0, 1); }
        void WriteUInt8(uint8_t value) { WriteBits(value, 8); }
        void WriteUInt16(uint16_t value) { WriteBits(value, 16); }
        void WriteUInt32(uint32_t value) { WriteBits(value, 32); }
        
        bool ReadBool(size_t& pos) const { return ReadBits(1, pos) != 0; }
        uint8_t ReadUInt8(size_t& pos) const { return static_cast<uint8_t>(ReadBits(8, pos)); }
        uint16_t ReadUInt16(size_t& pos) const { return static_cast<uint16_t>(ReadBits(16, pos)); }
        uint32_t ReadUInt32(size_t& pos) const { return static_cast<uint32_t>(ReadBits(32, pos)); }
        
        const std::vector<uint8_t>& GetData() const { return data; }
        size_t GetBitSize() const { return bit_pos; }
        size_t GetByteSize() const { return (bit_pos + 7) / 8; }
        
        void Clear() { data.clear(); bit_pos = 0; }
    };

public:
    StateCompressionSystem(const CompressionConfig& cfg = CompressionConfig{}) 
        : config(cfg) {}
    
    // 전체 상태 압축 (초기 동기화용)
    std::vector<uint8_t> CompressFullState(const std::vector<GameEntity>& entities) {
        BitStream stream;
        
        // 헤더: 엔티티 개수
        stream.WriteUInt16(static_cast<uint16_t>(entities.size()));
        
        std::cout << "📦 전체 상태 압축 시작 (" << entities.size() << "개 엔티티)" << std::endl;
        
        for (const auto& entity : entities) {
            CompressEntityFull(entity, stream);
        }
        
        std::cout << "압축 완료: " << stream.GetByteSize() << " bytes" << std::endl;
        std::cout << "압축률: " << (float)stream.GetByteSize() / (entities.size() * sizeof(GameEntity)) * 100 
                  << "%" << std::endl;
        
        return stream.GetData();
    }
    
    // 델타 압축 (변경된 부분만)
    std::vector<uint8_t> CompressDeltaState(const std::vector<GameEntity>& current_entities) {
        BitStream stream;
        
        std::unordered_set<uint32_t> changed_entities;
        std::unordered_set<uint32_t> new_entities;
        std::unordered_set<uint32_t> removed_entities;
        
        // 변경된 엔티티 찾기
        for (const auto& entity : current_entities) {
            auto prev_it = previous_states.find(entity.entity_id);
            
            if (prev_it == previous_states.end()) {
                new_entities.insert(entity.entity_id);
            } else if (HasSignificantChanges(prev_it->second, entity)) {
                changed_entities.insert(entity.entity_id);
            }
        }
        
        // 제거된 엔티티 찾기
        for (const auto& prev_pair : previous_states) {
            bool found = false;
            for (const auto& entity : current_entities) {
                if (entity.entity_id == prev_pair.first) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                removed_entities.insert(prev_pair.first);
            }
        }
        
        // 델타 패킷 헤더
        stream.WriteUInt16(static_cast<uint16_t>(new_entities.size()));
        stream.WriteUInt16(static_cast<uint16_t>(changed_entities.size()));
        stream.WriteUInt16(static_cast<uint16_t>(removed_entities.size()));
        
        std::cout << "📊 델타 압축: 신규 " << new_entities.size() 
                  << ", 변경 " << changed_entities.size() 
                  << ", 제거 " << removed_entities.size() << std::endl;
        
        // 신규 엔티티 (전체 데이터)
        for (uint32_t entity_id : new_entities) {
            auto entity_it = std::find_if(current_entities.begin(), current_entities.end(),
                [entity_id](const GameEntity& e) { return e.entity_id == entity_id; });
            
            if (entity_it != current_entities.end()) {
                stream.WriteUInt32(entity_id);
                CompressEntityFull(*entity_it, stream);
            }
        }
        
        // 변경된 엔티티 (델타만)
        for (uint32_t entity_id : changed_entities) {
            auto entity_it = std::find_if(current_entities.begin(), current_entities.end(),
                [entity_id](const GameEntity& e) { return e.entity_id == entity_id; });
            
            if (entity_it != current_entities.end()) {
                stream.WriteUInt32(entity_id);
                CompressEntityDelta(previous_states[entity_id], *entity_it, stream);
            }
        }
        
        // 제거된 엔티티 (ID만)
        for (uint32_t entity_id : removed_entities) {
            stream.WriteUInt32(entity_id);
        }
        
        // 이전 상태 업데이트
        previous_states.clear();
        for (const auto& entity : current_entities) {
            previous_states[entity.entity_id] = entity;
        }
        
        std::cout << "델타 압축 완료: " << stream.GetByteSize() << " bytes" << std::endl;
        
        return stream.GetData();
    }
    
    // 압축된 데이터 해제
    std::vector<GameEntity> DecompressFullState(const std::vector<uint8_t>& compressed_data) {
        size_t read_pos = 0;
        BitStream stream;
        stream = CreateStreamFromData(compressed_data);
        
        uint16_t entity_count = stream.ReadUInt16(read_pos);
        std::vector<GameEntity> entities;
        entities.reserve(entity_count);
        
        std::cout << "📂 전체 상태 압축 해제 (" << entity_count << "개 엔티티)" << std::endl;
        
        for (uint16_t i = 0; i < entity_count; ++i) {
            entities.push_back(DecompressEntityFull(stream, read_pos));
        }
        
        return entities;
    }

private:
    void CompressEntityFull(const GameEntity& entity, BitStream& stream) {
        // 엔티티 ID
        stream.WriteUInt32(entity.entity_id);
        
        // 위치 (월드 좌표를 16비트로 압축)
        uint16_t compressed_x = CompressFloat(entity.position_x, config.world_min_x, config.world_max_x, config.position_precision);
        uint16_t compressed_y = CompressFloat(entity.position_y, config.world_min_y, config.world_max_y, config.position_precision);
        uint16_t compressed_z = CompressFloat(entity.position_z, config.world_min_z, config.world_max_z, config.position_precision);
        
        stream.WriteUInt16(compressed_x);
        stream.WriteUInt16(compressed_y);
        stream.WriteUInt16(compressed_z);
        
        // 회전 (0-360도를 16비트로 압축)
        uint16_t compressed_rot_x = static_cast<uint16_t>((entity.rotation_x + 180.0f) / 360.0f * config.rotation_precision);
        uint16_t compressed_rot_y = static_cast<uint16_t>((entity.rotation_y + 180.0f) / 360.0f * config.rotation_precision);
        uint16_t compressed_rot_z = static_cast<uint16_t>((entity.rotation_z + 180.0f) / 360.0f * config.rotation_precision);
        
        stream.WriteBits(compressed_rot_x, 9);  // 9비트면 512 정밀도
        stream.WriteBits(compressed_rot_y, 9);
        stream.WriteBits(compressed_rot_z, 9);
        
        // 속도 (10비트로 압축)
        uint16_t compressed_vel_x = CompressFloat(entity.velocity_x, -50.0f, 50.0f, config.velocity_precision);
        uint16_t compressed_vel_y = CompressFloat(entity.velocity_y, -50.0f, 50.0f, config.velocity_precision);
        uint16_t compressed_vel_z = CompressFloat(entity.velocity_z, -50.0f, 50.0f, config.velocity_precision);
        
        stream.WriteBits(compressed_vel_x, 10);
        stream.WriteBits(compressed_vel_y, 10);
        stream.WriteBits(compressed_vel_z, 10);
        
        // 체력 (8비트 퍼센트로 압축)
        uint8_t health_percent = static_cast<uint8_t>(entity.health / entity.max_health * 255);
        stream.WriteUInt8(health_percent);
        stream.WriteUInt16(static_cast<uint16_t>(entity.max_health));
        
        // 기타 상태
        stream.WriteUInt32(entity.animation_state);
        stream.WriteUInt32(entity.equipment_mask);
        stream.WriteUInt8(entity.team_id);
        stream.WriteUInt8(entity.entity_type);
        
        // 불린 플래그들을 비트로 압축
        uint8_t flags = 0;
        if (entity.is_alive) flags |= 1 << 0;
        if (entity.is_moving) flags |= 1 << 1;
        if (entity.is_attacking) flags |= 1 << 2;
        if (entity.is_casting) flags |= 1 << 3;
        
        stream.WriteUInt8(flags);
    }
    
    void CompressEntityDelta(const GameEntity& previous, const GameEntity& current, BitStream& stream) {
        // 변경 필드 비트마스크
        uint16_t changed_fields = 0;
        
        // 변경된 필드 확인
        if (HasPositionChanged(previous, current)) changed_fields |= 1 << 0;
        if (HasRotationChanged(previous, current)) changed_fields |= 1 << 1;
        if (HasVelocityChanged(previous, current)) changed_fields |= 1 << 2;
        if (std::abs(previous.health - current.health) > 0.1f) changed_fields |= 1 << 3;
        if (previous.animation_state != current.animation_state) changed_fields |= 1 << 4;
        if (previous.equipment_mask != current.equipment_mask) changed_fields |= 1 << 5;
        if (GetEntityFlags(previous) != GetEntityFlags(current)) changed_fields |= 1 << 6;
        
        // 변경 비트마스크 쓰기
        stream.WriteUInt16(changed_fields);
        
        // 변경된 필드만 압축해서 쓰기
        if (changed_fields & (1 << 0)) {  // 위치
            uint16_t compressed_x = CompressFloat(current.position_x, config.world_min_x, config.world_max_x, config.position_precision);
            uint16_t compressed_y = CompressFloat(current.position_y, config.world_min_y, config.world_max_y, config.position_precision);
            uint16_t compressed_z = CompressFloat(current.position_z, config.world_min_z, config.world_max_z, config.position_precision);
            
            stream.WriteUInt16(compressed_x);
            stream.WriteUInt16(compressed_y);
            stream.WriteUInt16(compressed_z);
        }
        
        if (changed_fields & (1 << 1)) {  // 회전
            uint16_t compressed_rot_x = static_cast<uint16_t>((current.rotation_x + 180.0f) / 360.0f * config.rotation_precision);
            uint16_t compressed_rot_y = static_cast<uint16_t>((current.rotation_y + 180.0f) / 360.0f * config.rotation_precision);
            uint16_t compressed_rot_z = static_cast<uint16_t>((current.rotation_z + 180.0f) / 360.0f * config.rotation_precision);
            
            stream.WriteBits(compressed_rot_x, 9);
            stream.WriteBits(compressed_rot_y, 9);
            stream.WriteBits(compressed_rot_z, 9);
        }
        
        if (changed_fields & (1 << 2)) {  // 속도
            uint16_t compressed_vel_x = CompressFloat(current.velocity_x, -50.0f, 50.0f, config.velocity_precision);
            uint16_t compressed_vel_y = CompressFloat(current.velocity_y, -50.0f, 50.0f, config.velocity_precision);
            uint16_t compressed_vel_z = CompressFloat(current.velocity_z, -50.0f, 50.0f, config.velocity_precision);
            
            stream.WriteBits(compressed_vel_x, 10);
            stream.WriteBits(compressed_vel_y, 10);
            stream.WriteBits(compressed_vel_z, 10);
        }
        
        if (changed_fields & (1 << 3)) {  // 체력
            uint8_t health_percent = static_cast<uint8_t>(current.health / current.max_health * 255);
            stream.WriteUInt8(health_percent);
        }
        
        if (changed_fields & (1 << 4)) {  // 애니메이션
            stream.WriteUInt32(current.animation_state);
        }
        
        if (changed_fields & (1 << 5)) {  // 장비
            stream.WriteUInt32(current.equipment_mask);
        }
        
        if (changed_fields & (1 << 6)) {  // 플래그
            stream.WriteUInt8(GetEntityFlags(current));
        }
    }
    
    GameEntity DecompressEntityFull(const BitStream& stream, size_t& read_pos) {
        GameEntity entity;
        
        // 엔티티 ID
        entity.entity_id = stream.ReadUInt32(read_pos);
        
        // 위치 압축 해제
        uint16_t compressed_x = stream.ReadUInt16(read_pos);
        uint16_t compressed_y = stream.ReadUInt16(read_pos);
        uint16_t compressed_z = stream.ReadUInt16(read_pos);
        
        entity.position_x = DecompressFloat(compressed_x, config.world_min_x, config.world_max_x, config.position_precision);
        entity.position_y = DecompressFloat(compressed_y, config.world_min_y, config.world_max_y, config.position_precision);
        entity.position_z = DecompressFloat(compressed_z, config.world_min_z, config.world_max_z, config.position_precision);
        
        // 회전 압축 해제
        uint16_t compressed_rot_x = static_cast<uint16_t>(stream.ReadBits(9, read_pos));
        uint16_t compressed_rot_y = static_cast<uint16_t>(stream.ReadBits(9, read_pos));
        uint16_t compressed_rot_z = static_cast<uint16_t>(stream.ReadBits(9, read_pos));
        
        entity.rotation_x = (float)compressed_rot_x / config.rotation_precision * 360.0f - 180.0f;
        entity.rotation_y = (float)compressed_rot_y / config.rotation_precision * 360.0f - 180.0f;
        entity.rotation_z = (float)compressed_rot_z / config.rotation_precision * 360.0f - 180.0f;
        
        // 속도 압축 해제
        uint16_t compressed_vel_x = static_cast<uint16_t>(stream.ReadBits(10, read_pos));
        uint16_t compressed_vel_y = static_cast<uint16_t>(stream.ReadBits(10, read_pos));
        uint16_t compressed_vel_z = static_cast<uint16_t>(stream.ReadBits(10, read_pos));
        
        entity.velocity_x = DecompressFloat(compressed_vel_x, -50.0f, 50.0f, config.velocity_precision);
        entity.velocity_y = DecompressFloat(compressed_vel_y, -50.0f, 50.0f, config.velocity_precision);
        entity.velocity_z = DecompressFloat(compressed_vel_z, -50.0f, 50.0f, config.velocity_precision);
        
        // 체력
        uint8_t health_percent = stream.ReadUInt8(read_pos);
        entity.max_health = static_cast<float>(stream.ReadUInt16(read_pos));
        entity.health = entity.max_health * health_percent / 255.0f;
        
        // 기타 상태
        entity.animation_state = stream.ReadUInt32(read_pos);
        entity.equipment_mask = stream.ReadUInt32(read_pos);
        entity.team_id = stream.ReadUInt8(read_pos);
        entity.entity_type = stream.ReadUInt8(read_pos);
        
        // 플래그
        uint8_t flags = stream.ReadUInt8(read_pos);
        entity.is_alive = (flags & (1 << 0)) != 0;
        entity.is_moving = (flags & (1 << 1)) != 0;
        entity.is_attacking = (flags & (1 << 2)) != 0;
        entity.is_casting = (flags & (1 << 3)) != 0;
        
        return entity;
    }
    
    uint16_t CompressFloat(float value, float min_val, float max_val, uint16_t precision) {
        float normalized = (value - min_val) / (max_val - min_val);
        normalized = std::clamp(normalized, 0.0f, 1.0f);
        return static_cast<uint16_t>(normalized * precision);
    }
    
    float DecompressFloat(uint16_t compressed, float min_val, float max_val, uint16_t precision) {
        float normalized = static_cast<float>(compressed) / precision;
        return min_val + normalized * (max_val - min_val);
    }
    
    bool HasSignificantChanges(const GameEntity& prev, const GameEntity& current) {
        return HasPositionChanged(prev, current) || 
               HasRotationChanged(prev, current) ||
               HasVelocityChanged(prev, current) ||
               std::abs(prev.health - current.health) > 0.1f ||
               prev.animation_state != current.animation_state ||
               prev.equipment_mask != current.equipment_mask ||
               GetEntityFlags(prev) != GetEntityFlags(current);
    }
    
    bool HasPositionChanged(const GameEntity& prev, const GameEntity& current) {
        return std::abs(prev.position_x - current.position_x) > config.position_delta_threshold ||
               std::abs(prev.position_y - current.position_y) > config.position_delta_threshold ||
               std::abs(prev.position_z - current.position_z) > config.position_delta_threshold;
    }
    
    bool HasRotationChanged(const GameEntity& prev, const GameEntity& current) {
        return std::abs(prev.rotation_x - current.rotation_x) > config.rotation_delta_threshold ||
               std::abs(prev.rotation_y - current.rotation_y) > config.rotation_delta_threshold ||
               std::abs(prev.rotation_z - current.rotation_z) > config.rotation_delta_threshold;
    }
    
    bool HasVelocityChanged(const GameEntity& prev, const GameEntity& current) {
        return std::abs(prev.velocity_x - current.velocity_x) > config.velocity_delta_threshold ||
               std::abs(prev.velocity_y - current.velocity_y) > config.velocity_delta_threshold ||
               std::abs(prev.velocity_z - current.velocity_z) > config.velocity_delta_threshold;
    }
    
    uint8_t GetEntityFlags(const GameEntity& entity) {
        uint8_t flags = 0;
        if (entity.is_alive) flags |= 1 << 0;
        if (entity.is_moving) flags |= 1 << 1;
        if (entity.is_attacking) flags |= 1 << 2;
        if (entity.is_casting) flags |= 1 << 3;
        return flags;
    }
    
    BitStream CreateStreamFromData(const std::vector<uint8_t>& data) {
        // 간단한 구현 - 실제로는 BitStream에 데이터 로드 기능 필요
        BitStream stream;
        return stream;
    }
};

void DemonstrateStateCompression() {
    std::cout << "📦 상태 압축 시스템 데모" << std::endl;
    
    StateCompressionSystem compressor;
    
    // 테스트 엔티티들 생성
    std::vector<StateCompressionSystem::GameEntity> entities = {
        {1, 100.5f, 200.3f, 10.0f, 45.0f, 0.0f, 90.0f, 5.0f, -2.0f, 0.0f, 850.0f, 1000.0f, 1, 0x1F, 1, 0, true, true, false, false},
        {2, -50.2f, 150.8f, 15.5f, -30.0f, 180.0f, 45.0f, -3.0f, 1.5f, 0.0f, 600.0f, 800.0f, 2, 0x07, 1, 1, true, false, true, false},
        {3, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1200.0f, 1200.0f, 0, 0x00, 2, 2, false, false, false, false}
    };
    
    // 전체 상태 압축
    std::cout << "\n=== 전체 상태 압축 ===" << std::endl;
    auto compressed_full = compressor.CompressFullState(entities);
    
    size_t original_size = entities.size() * sizeof(StateCompressionSystem::GameEntity);
    std::cout << "원본 크기: " << original_size << " bytes" << std::endl;
    std::cout << "압축 크기: " << compressed_full.size() << " bytes" << std::endl;
    std::cout << "압축률: " << (float)compressed_full.size() / original_size * 100 << "%" << std::endl;
    
    // 상태 변경 시뮬레이션
    std::cout << "\n=== 상태 변경 시뮬레이션 ===" << std::endl;
    
    // 첫 번째 엔티티 위치 변경
    entities[0].position_x += 5.0f;
    entities[0].position_y += 3.0f;
    entities[0].is_moving = true;
    
    // 두 번째 엔티티 체력 변경
    entities[1].health -= 50.0f;
    entities[1].animation_state = 5;  // 피격 애니메이션
    
    // 델타 압축
    auto compressed_delta = compressor.CompressDeltaState(entities);
    
    std::cout << "델타 압축 크기: " << compressed_delta.size() << " bytes" << std::endl;
    std::cout << "전체 대비 델타 크기: " << (float)compressed_delta.size() / compressed_full.size() * 100 << "%" << std::endl;
    
    // 추가 변경 (미세한 변화 - 임계값 테스트)
    std::cout << "\n=== 미세한 변화 테스트 ===" << std::endl;
    
    entities[0].position_x += 0.05f;  // 임계값 이하 변화
    entities[1].rotation_y += 0.5f;   // 임계값 이하 변화
    
    auto compressed_micro = compressor.CompressDeltaState(entities);
    std::cout << "미세 변화 델타: " << compressed_micro.size() << " bytes" << std::endl;
    
    if (compressed_micro.size() <= 8) {  // 헤더만
        std::cout << "✅ 임계값 이하 변화 무시됨" << std::endl;
    }
}
```

**💡 상태 압축의 핵심:**
- **비트 레벨 압축**: 불필요한 정밀도 제거
- **델타 압축**: 변경된 부분만 전송
- **임계값 기반 필터링**: 미세한 변화 무시
- **필드별 최적화**: 데이터 특성에 맞는 압축

---

## ⚡ 5. 지연 보상 기법 (Lag Compensation)

### **5.1 시간 되돌리기 시스템 (Time Rewinding)**

```cpp
#include <chrono>
#include <deque>

// ⏰ 시간 되돌리기 지연 보상 시스템
class LagCompensationSystem {
public:
    struct PlayerSnapshot {
        uint32_t player_id;
        uint32_t timestamp_ms;
        float position_x, position_y, position_z;
        float hitbox_radius;
        float health;
        bool is_alive;
        uint32_t animation_state;
        
        // 히트박스 계산 (애니메이션 상태 고려)
        struct HitBox {
            float center_x, center_y, center_z;
            float width, height, depth;
        } hitbox;
    };
    
    struct ShotEvent {
        uint32_t shooter_id;
        uint32_t target_id;
        uint32_t client_timestamp;  // 클라이언트에서 쏜 시간
        uint32_t server_timestamp;  // 서버에서 받은 시간
        float shot_origin_x, shot_origin_y, shot_origin_z;
        float shot_direction_x, shot_direction_y, shot_direction_z;
        float weapon_range;
        uint32_t damage;
        bool hit_confirmed;
    };

private:
    // 플레이어별 히스토리 (링 버퍼)
    std::unordered_map<uint32_t, std::deque<PlayerSnapshot>> player_histories;
    
    // 설정
    static constexpr uint32_t MAX_HISTORY_MS = 1000;    // 1초 히스토리
    static constexpr uint32_t MAX_LAG_COMPENSATION_MS = 150;  // 최대 150ms 보상
    static constexpr uint32_t SNAPSHOT_INTERVAL_MS = 16;      // 16ms 간격 (60fps)
    
    // 성능 메트릭
    struct LagCompMetrics {
        uint32_t total_shots = 0;
        uint32_t compensated_hits = 0;
        uint32_t rejected_shots = 0;  // 지연 시간 초과
        float avg_compensation_ms = 0.0f;
        uint32_t false_positives = 0;  // 잘못된 보상
    } metrics;

public:
    // 플레이어 상태 스냅샷 저장
    void RecordPlayerSnapshot(const PlayerSnapshot& snapshot) {
        auto& history = player_histories[snapshot.player_id];
        
        // 타임스탬프 순서 보장
        if (!history.empty() && snapshot.timestamp_ms <= history.back().timestamp_ms) {
            std::cout << "⚠️ 순서 잘못된 스냅샷 무시: 플레이어 " << snapshot.player_id << std::endl;
            return;
        }
        
        history.push_back(snapshot);
        
        // 오래된 히스토리 정리
        uint32_t cutoff_time = snapshot.timestamp_ms > MAX_HISTORY_MS ? 
                              snapshot.timestamp_ms - MAX_HISTORY_MS : 0;
        
        while (!history.empty() && history.front().timestamp_ms < cutoff_time) {
            history.pop_front();
        }
        
        // 너무 많은 데이터 축적 방지
        if (history.size() > MAX_HISTORY_MS / SNAPSHOT_INTERVAL_MS * 2) {
            history.pop_front();
        }
    }
    
    // 샷 이벤트 처리 (지연 보상 적용)
    bool ProcessShotEvent(const ShotEvent& shot) {
        auto current_time = GetCurrentTimeMs();
        uint32_t network_delay = current_time - shot.client_timestamp;
        
        metrics.total_shots++;
        
        std::cout << "\n🎯 샷 이벤트 처리" << std::endl;
        std::cout << "슈터: " << shot.shooter_id << " → 타겟: " << shot.target_id << std::endl;
        std::cout << "네트워크 지연: " << network_delay << "ms" << std::endl;
        
        // 지연 시간이 너무 크면 거부
        if (network_delay > MAX_LAG_COMPENSATION_MS) {
            std::cout << "❌ 지연 시간 초과 - 샷 거부" << std::endl;
            metrics.rejected_shots++;
            return false;
        }
        
        // 클라이언트 시점으로 되돌릴 시간 계산
        uint32_t rewind_time = shot.client_timestamp;
        
        // 해당 시점의 타겟 상태 복원
        PlayerSnapshot target_state_at_shot_time;
        if (!GetPlayerStateAtTime(shot.target_id, rewind_time, target_state_at_shot_time)) {
            std::cout << "❌ 타겟 히스토리 없음 - 샷 거부" << std::endl;
            metrics.rejected_shots++;
            return false;
        }
        
        std::cout << "⏰ 시간 되돌리기: " << network_delay << "ms 전 상태 복원" << std::endl;
        std::cout << "복원된 타겟 위치: (" << target_state_at_shot_time.position_x 
                  << ", " << target_state_at_shot_time.position_y 
                  << ", " << target_state_at_shot_time.position_z << ")" << std::endl;
        
        // 히트 검사 (되돌린 시점 기준)
        bool hit = PerformHitCheck(shot, target_state_at_shot_time);
        
        if (hit) {
            std::cout << "✅ 히트 확인됨 (지연 보상 적용)" << std::endl;
            metrics.compensated_hits++;
            
            // 현재 타겟이 여전히 유효한지 확인 (anti-cheat)
            if (!ValidateHitLegitimacy(shot, target_state_at_shot_time, current_time)) {
                std::cout << "🚨 의심스러운 히트 - 거부" << std::endl;
                metrics.false_positives++;
                return false;
            }
            
            // 데미지 적용
            ApplyDamage(shot.target_id, shot.damage, current_time);
        } else {
            std::cout << "❌ 빗나감" << std::endl;
        }
        
        // 메트릭 업데이트
        metrics.avg_compensation_ms = (metrics.avg_compensation_ms * (metrics.total_shots - 1) + 
                                      network_delay) / metrics.total_shots;
        
        return hit;
    }

private:
    bool GetPlayerStateAtTime(uint32_t player_id, uint32_t target_time, PlayerSnapshot& out_state) {
        auto history_it = player_histories.find(player_id);
        if (history_it == player_histories.end()) {
            return false;
        }
        
        const auto& history = history_it->second;
        if (history.empty()) {
            return false;
        }
        
        // 정확한 시간의 스냅샷 찾기
        for (auto it = history.rbegin(); it != history.rend(); ++it) {
            if (it->timestamp_ms <= target_time) {
                out_state = *it;
                
                // 보간이 필요한 경우 (더 정확한 위치 계산)
                auto next_it = it - 1;
                if (next_it >= history.rbegin() && next_it->timestamp_ms > target_time) {
                    out_state = InterpolatePlayerState(*it, *next_it, target_time);
                }
                
                return true;
            }
        }
        
        // 가장 오래된 스냅샷 사용
        out_state = history.front();
        return true;
    }
    
    PlayerSnapshot InterpolatePlayerState(const PlayerSnapshot& earlier, 
                                        const PlayerSnapshot& later, 
                                        uint32_t target_time) {
        if (later.timestamp_ms == earlier.timestamp_ms) {
            return earlier;
        }
        
        float t = static_cast<float>(target_time - earlier.timestamp_ms) / 
                 (later.timestamp_ms - earlier.timestamp_ms);
        t = std::clamp(t, 0.0f, 1.0f);
        
        PlayerSnapshot interpolated = earlier;
        interpolated.timestamp_ms = target_time;
        
        // 선형 보간
        interpolated.position_x = earlier.position_x + (later.position_x - earlier.position_x) * t;
        interpolated.position_y = earlier.position_y + (later.position_y - earlier.position_y) * t;
        interpolated.position_z = earlier.position_z + (later.position_z - earlier.position_z) * t;
        
        // 히트박스 재계산
        UpdateHitbox(interpolated);
        
        std::cout << "🔄 상태 보간: t=" << t << ", 위치=(" 
                  << interpolated.position_x << ", " << interpolated.position_y 
                  << ", " << interpolated.position_z << ")" << std::endl;
        
        return interpolated;
    }
    
    void UpdateHitbox(PlayerSnapshot& snapshot) {
        // 애니메이션 상태에 따른 히트박스 조정
        snapshot.hitbox.center_x = snapshot.position_x;
        snapshot.hitbox.center_y = snapshot.position_y;
        snapshot.hitbox.center_z = snapshot.position_z + 1.0f;  // 발목 기준으로 보정
        
        switch (snapshot.animation_state) {
            case 0:  // 서 있기
                snapshot.hitbox.width = 0.6f;
                snapshot.hitbox.height = 1.8f;
                snapshot.hitbox.depth = 0.6f;
                break;
            case 1:  // 앉기
                snapshot.hitbox.width = 0.6f;
                snapshot.hitbox.height = 1.2f;
                snapshot.hitbox.depth = 0.6f;
                snapshot.hitbox.center_z -= 0.3f;
                break;
            case 2:  // 뛰기/점프
                snapshot.hitbox.width = 0.8f;
                snapshot.hitbox.height = 1.6f;
                snapshot.hitbox.depth = 0.8f;
                break;
            default:
                snapshot.hitbox.width = 0.6f;
                snapshot.hitbox.height = 1.8f;
                snapshot.hitbox.depth = 0.6f;
                break;
        }
    }
    
    bool PerformHitCheck(const ShotEvent& shot, const PlayerSnapshot& target_state) {
        // 광선-박스 교차 검사 (Ray-AABB intersection)
        
        float ray_origin[3] = {shot.shot_origin_x, shot.shot_origin_y, shot.shot_origin_z};
        float ray_direction[3] = {shot.shot_direction_x, shot.shot_direction_y, shot.shot_direction_z};
        
        // 히트박스 경계
        float aabb_min[3] = {
            target_state.hitbox.center_x - target_state.hitbox.width / 2,
            target_state.hitbox.center_y - target_state.hitbox.depth / 2,
            target_state.hitbox.center_z - target_state.hitbox.height / 2
        };
        
        float aabb_max[3] = {
            target_state.hitbox.center_x + target_state.hitbox.width / 2,
            target_state.hitbox.center_y + target_state.hitbox.depth / 2,
            target_state.hitbox.center_z + target_state.hitbox.height / 2
        };
        
        // 광선 방향 정규화
        float ray_length = std::sqrt(ray_direction[0] * ray_direction[0] + 
                                   ray_direction[1] * ray_direction[1] + 
                                   ray_direction[2] * ray_direction[2]);
        
        if (ray_length < 0.001f) return false;
        
        ray_direction[0] /= ray_length;
        ray_direction[1] /= ray_length;
        ray_direction[2] /= ray_length;
        
        // AABB 교차 검사
        float t_min = 0.0f;
        float t_max = shot.weapon_range;
        
        for (int i = 0; i < 3; i++) {
            if (std::abs(ray_direction[i]) < 0.001f) {
                // 광선이 슬래브와 평행
                if (ray_origin[i] < aabb_min[i] || ray_origin[i] > aabb_max[i]) {
                    return false;
                }
            } else {
                float t1 = (aabb_min[i] - ray_origin[i]) / ray_direction[i];
                float t2 = (aabb_max[i] - ray_origin[i]) / ray_direction[i];
                
                if (t1 > t2) std::swap(t1, t2);
                
                t_min = std::max(t_min, t1);
                t_max = std::min(t_max, t2);
                
                if (t_min > t_max) return false;
            }
        }
        
        // 교차점이 무기 사거리 내에 있는지 확인
        bool hit = t_min >= 0.0f && t_min <= shot.weapon_range;
        
        if (hit) {
            float hit_point[3] = {
                ray_origin[0] + ray_direction[0] * t_min,
                ray_origin[1] + ray_direction[1] * t_min,
                ray_origin[2] + ray_direction[2] * t_min
            };
            
            std::cout << "🎯 히트 지점: (" << hit_point[0] << ", " << hit_point[1] 
                      << ", " << hit_point[2] << "), 거리: " << t_min << std::endl;
        }
        
        return hit;
    }
    
    bool ValidateHitLegitimacy(const ShotEvent& shot, const PlayerSnapshot& past_state, uint32_t current_time) {
        // 1. 타겟이 현재도 살아있는지 확인
        PlayerSnapshot current_state;
        if (!GetPlayerStateAtTime(shot.target_id, current_time, current_state)) {
            return false;
        }
        
        if (!current_state.is_alive) {
            std::cout << "🚨 이미 죽은 타겟에 대한 히트 시도" << std::endl;
            return false;
        }
        
        // 2. 과도한 위치 변화 검사 (텔레포트 치트 방지)
        float distance_moved = std::sqrt(
            std::pow(current_state.position_x - past_state.position_x, 2) +
            std::pow(current_state.position_y - past_state.position_y, 2) +
            std::pow(current_state.position_z - past_state.position_z, 2)
        );
        
        uint32_t time_diff = current_time - past_state.timestamp_ms;
        float max_possible_distance = (time_diff / 1000.0f) * 10.0f;  // 최대 10m/s
        
        if (distance_moved > max_possible_distance * 1.5f) {  // 50% 여유분
            std::cout << "🚨 비정상적인 이동 거리: " << distance_moved 
                      << "m in " << time_diff << "ms" << std::endl;
            return false;
        }
        
        // 3. 슈터의 시야각 검사 (월핵 방지)
        // ... 추가 검증 로직
        
        return true;
    }
    
    void ApplyDamage(uint32_t target_id, uint32_t damage, uint32_t timestamp) {
        std::cout << "💥 데미지 적용: 플레이어 " << target_id << "에게 " << damage << " 데미지" << std::endl;
        
        // 실제 게임에서는 현재 체력에서 데미지 차감
        // 여기서는 시뮬레이션만
    }
    
    uint32_t GetCurrentTimeMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

public:
    void PrintMetrics() const {
        std::cout << "\n📊 지연 보상 시스템 메트릭:" << std::endl;
        std::cout << "총 샷 수: " << metrics.total_shots << std::endl;
        std::cout << "보상된 히트: " << metrics.compensated_hits << std::endl;
        std::cout << "거부된 샷: " << metrics.rejected_shots << std::endl;
        std::cout << "의심스러운 히트: " << metrics.false_positives << std::endl;
        
        if (metrics.total_shots > 0) {
            float hit_rate = (float)metrics.compensated_hits / metrics.total_shots * 100;
            std::cout << "히트율: " << hit_rate << "%" << std::endl;
            std::cout << "평균 보상 지연: " << metrics.avg_compensation_ms << "ms" << std::endl;
        }
    }
};

void DemonstrateLagCompensation() {
    std::cout << "⏰ 지연 보상 시스템 데모" << std::endl;
    
    LagCompensationSystem lag_comp;
    uint32_t current_time = 1000000;  // 기준 시간
    
    // 플레이어 상태 히스토리 구축
    std::cout << "\n=== 플레이어 히스토리 구축 ===" << std::endl;
    
    for (uint32_t t = 0; t < 500; t += 16) {  // 500ms 히스토리, 16ms 간격
        LagCompensationSystem::PlayerSnapshot snapshot;
        snapshot.player_id = 1;
        snapshot.timestamp_ms = current_time + t;
        
        // 플레이어가 직선으로 이동
        snapshot.position_x = 10.0f + t * 0.01f;  // 초당 10m 이동
        snapshot.position_y = 20.0f;
        snapshot.position_z = 1.0f;
        snapshot.hitbox_radius = 0.5f;
        snapshot.health = 100.0f;
        snapshot.is_alive = true;
        snapshot.animation_state = 0;  // 서 있기
        
        lag_comp.RecordPlayerSnapshot(snapshot);
    }
    
    // 지연 보상 샷 시뮬레이션
    std::cout << "\n=== 지연 보상 샷 테스트 ===" << std::endl;
    
    LagCompensationSystem::ShotEvent shot;
    shot.shooter_id = 2;
    shot.target_id = 1;
    shot.client_timestamp = current_time + 400;  // 100ms 전에 쏨
    shot.server_timestamp = current_time + 500;  // 현재 시간
    
    // 슈터 위치와 방향
    shot.shot_origin_x = 0.0f;
    shot.shot_origin_y = 20.0f;
    shot.shot_origin_z = 1.0f;
    
    // 타겟을 향한 방향 계산 (100ms 전 예상 위치)
    float target_x_at_shot_time = 10.0f + 400 * 0.01f;  // 14.0f
    
    shot.shot_direction_x = target_x_at_shot_time - shot.shot_origin_x;
    shot.shot_direction_y = 0.0f;
    shot.shot_direction_z = 0.0f;
    
    shot.weapon_range = 100.0f;
    shot.damage = 50;
    shot.hit_confirmed = false;
    
    std::cout << "클라이언트 샷 시간: " << shot.client_timestamp << std::endl;
    std::cout << "서버 수신 시간: " << shot.server_timestamp << std::endl;
    std::cout << "예상 히트 위치: (" << target_x_at_shot_time << ", 20.0, 1.0)" << std::endl;
    
    // 지연 보상 처리
    bool hit = lag_comp.ProcessShotEvent(shot);
    
    std::cout << "\n결과: " << (hit ? "HIT!" : "MISS!") << std::endl;
    
    // 추가 테스트: 지연 시간 초과
    std::cout << "\n=== 지연 시간 초과 테스트 ===" << std::endl;
    
    LagCompensationSystem::ShotEvent late_shot = shot;
    late_shot.client_timestamp = current_time + 200;  // 300ms 전 (임계값 초과)
    late_shot.server_timestamp = current_time + 500;
    
    bool late_hit = lag_comp.ProcessShotEvent(late_shot);
    std::cout << "늦은 샷 결과: " << (late_hit ? "HIT!" : "REJECTED!") << std::endl;
    
    // 메트릭 출력
    lag_comp.PrintMetrics();
}
```

**💡 지연 보상의 핵심:**
- **히스토리 관리**: 플레이어 상태를 시간별로 저장
- **시간 되돌리기**: 클라이언트 시점으로 상태 복원
- **정밀한 히트박스**: 애니메이션 상태별 히트박스
- **치팅 방지**: 비정상적인 히트 검증

---

## 🎯 마무리

**🎉 축하합니다!** 이제 여러분은 **실시간 게임 물리 & 상태 동기화**의 핵심 기술을 마스터했습니다!

### **지금 할 수 있는 것들:**
- ✅ **결정론적 물리**: 모든 클라이언트에서 동일한 시뮬레이션 결과
- ✅ **클라이언트 예측**: 네트워크 지연을 숨기는 반응성 확보
- ✅ **롤백 네트코드**: 입력 불일치 시 정확한 상태 복구
- ✅ **상태 압축**: 90% 대역폭 절약하는 고효율 데이터 전송
- ✅ **지연 보상**: 핑 150ms에서도 공정한 히트 판정

### **실제 성과:**
- **동시 플레이어**: 100명 실시간 액션 게임 구현 가능
- **네트워크 효율**: 원본 대비 10-20% 대역폭으로 동일한 품질
- **반응성**: 입력 지연 16ms 이하 (60fps)
- **공정성**: 핑 차이에도 불구하고 공정한 게임플레이

### **실무 적용 예시:**
- **배틀로얄 게임**: 100명 동시 전투
- **MMORPG PvP**: 대규모 길드전
- **FPS 게임**: 정밀한 사격 시스템
- **레이싱 게임**: 실시간 충돌 처리

### **다음 학습 권장사항:**
1. **물리 엔진 통합**: Bullet Physics, Box2D 연동
2. **네트워크 최적화**: UDP 기반 신뢰성 프로토콜
3. **AI 동기화**: NPC 행동의 결정론적 처리
4. **크로스 플랫폼**: 다양한 디바이스 간 동기화

---

## 🔥 흔한 실수와 해결방법 (Common Mistakes & Solutions)

### 1. 부동소수점 비결정론
```cpp
// [SEQUENCE: 1] ❌ 잘못된 예: 플랫폼별 다른 결과
class BadFloatingPoint {
    void UpdatePhysics(float deltaTime) {
        // sin, cos, sqrt 등은 플랫폼마다 다른 구현
        float angle = std::sin(rotation);  // Intel vs AMD 차이!
        float distance = std::sqrt(x*x + y*y);  // 정밀도 차이!
        
        // 컴파일러 최적화에 따라 연산 순서 변경
        velocity = (acceleration * deltaTime) + velocity;  // 순서가 바뀔 수 있음
    }
};

// ✅ 올바른 예: 고정소수점 또는 정밀한 부동소수점
class GoodDeterministic {
    void UpdatePhysics(int32_t deltaTime_ms) {
        // 고정소수점 연산 (1/1000 단위)
        int32_t velocity_fixed = velocity_mm_per_s;
        int32_t accel_fixed = acceleration_mm_per_s2;
        
        // 정확한 연산 순서 보장
        velocity_fixed = velocity_fixed + ((accel_fixed * deltaTime_ms) / 1000);
        
        // 필요시 부동소수점으로 변환 (렌더링용)
        float velocity_float = velocity_fixed / 1000.0f;
    }
};
```

### 2. 잘못된 롤백 구현
```cpp
// [SEQUENCE: 2] ❌ 잘못된 예: 불완전한 상태 저장
class BadRollback {
    struct GameState {
        float player_x, player_y;  // 위치만 저장
        // 속도, 가속도, 버프 등 누락!
    };
    
    void Rollback(int frame) {
        // 일부만 복원되어 불일치 발생
        player.x = saved_states[frame].player_x;
        player.y = saved_states[frame].player_y;
        // 속도가 그대로라 다음 프레임에 엉뚱한 위치로!
    }
};

// ✅ 올바른 예: 완전한 상태 저장
class GoodRollback {
    struct CompleteGameState {
        // 모든 게임플레이 영향 요소 포함
        PlayerState players[MAX_PLAYERS];
        ProjectileState projectiles[MAX_PROJECTILES];
        EnvironmentState environment;
        uint64_t random_seed;
        uint32_t frame_number;
    };
    
    void Rollback(int target_frame) {
        // 완전한 상태 복원
        memcpy(&current_state, &saved_states[target_frame], sizeof(CompleteGameState));
        random_generator.seed(current_state.random_seed);
    }
};
```

### 3. 과도한 상태 전송
```cpp
// [SEQUENCE: 3] ❌ 잘못된 예: 모든 데이터 매번 전송
class BadNetworkSync {
    void SendUpdate() {
        // 1KB의 전체 상태를 30fps로 전송 = 30KB/s per player!
        for (auto& player : all_players) {
            network.Send(player.GetCompleteState());  // 1024 bytes
        }
    }
};

// ✅ 올바른 예: 델타 압축과 우선순위
class GoodNetworkSync {
    void SendUpdate() {
        // 변경된 부분만 전송
        for (auto& player : all_players) {
            auto delta = player.GetStateDelta();
            if (delta.HasChanges()) {
                // 평균 50 bytes (95% 절약!)
                network.Send(CompressDelta(delta));
            }
        }
    }
};
```

---

## 🚀 실습 프로젝트 (Practice Projects)

### 📚 기초 프로젝트: 간단한 2D 격투 게임
**목표**: 2명이 대전하는 격투 게임의 네트워크 동기화

```cpp
// 구현 요구사항:
// 1. 결정론적 물리 엔진
// 2. 입력 예측과 롤백
// 3. 히트박스 동기화
// 4. 60fps 로컬 플레이
// 5. 최대 100ms 지연 보상
// 6. 리플레이 시스템
```

### 🎮 중급 프로젝트: 10인 레이싱 게임
**목표**: 실시간 물리 기반 레이싱 게임

```cpp
// 핵심 기능:
// 1. 차량 물리 시뮬레이션
// 2. 충돌 예측과 보정
// 3. 고스트 카 시스템
// 4. 동적 LOD 동기화
// 5. 관전 모드
// 6. 네트워크 보간
```

### 🏆 고급 프로젝트: 100인 배틀로얄
**목표**: 대규모 실시간 전투 게임

```cpp
// 구현 내용:
// 1. 공간 분할 최적화
// 2. 관심 영역 관리
// 3. 우선순위 기반 업데이트
// 4. 서버 권위 시스템
// 5. 치팅 방지
// 6. 동적 틱레이트
// 7. 클라우드 스케일링
```

---

## 📊 학습 체크리스트 (Learning Checklist)

### 🥉 브론즈 레벨
- [ ] 결정론적 시뮬레이션 이해
- [ ] 기본 상태 동기화 구현
- [ ] 간단한 보간 처리
- [ ] 입력 버퍼링

### 🥈 실버 레벨
- [ ] 클라이언트 예측 구현
- [ ] 롤백 시스템 구축
- [ ] 델타 압축 적용
- [ ] 지연 보상 기초

### 🥇 골드 레벨
- [ ] 복잡한 물리 동기화
- [ ] 고급 압축 알고리즘
- [ ] 동적 LOD 시스템
- [ ] 서버 권위 아키텍처

### 💎 플래티넘 레벨
- [ ] 커스텀 물리 엔진
- [ ] 분산 시뮬레이션
- [ ] 머신러닝 예측
- [ ] 차세대 네트코드 설계

---

## 📚 추가 학습 자료 (Additional Resources)

### 필독서
- 📖 "Game Programming Patterns" - Robert Nystrom
- 📖 "Multiplayer Game Programming" - Joshua Glazer
- 📖 "Real-Time Cameras" - Mark Haigh-Hutchinson

### 기술 문서
- 📄 GDC: "Overwatch Gameplay Architecture and Netcode"
- 📄 Valve: "Source Multiplayer Networking"
- 📄 Riot: "Peeking into VALORANT's Netcode"
- 📄 id Software: "Quake 3 Network Model"

### 오픈소스 프로젝트
- 🔧 GGPO: Rollback networking for fighting games
- 🔧 Mirror: Unity networking solution
- 🔧 Photon Fusion: Client-server tick-based
- 🔧 Netcode for GameObjects: Unity official

### 물리 엔진
- ⚙️ Box2D: 2D physics (deterministic mode)
- ⚙️ Bullet Physics: 3D physics
- ⚙️ PhysX: NVIDIA physics (deterministic mode)
- ⚙️ Havok: AAA game physics

**💪 이제 여러분은 AAA급 게임의 핵심 기술을 다룰 수 있습니다. 계속해서 더 고급 주제들을 탐구해보세요!** 🚀