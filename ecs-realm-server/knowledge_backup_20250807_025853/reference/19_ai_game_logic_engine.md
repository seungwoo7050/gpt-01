# 21단계: AI & 게임 로직 엔진 - 살아있는 NPC와 역동적인 게임 세계 만들기
*플레이어가 "이 NPC가 진짜 생각하고 있는 것 같다"고 느끼게 만드는 마법*

> **🎯 목표**: 플레이어가 살아있다고 느낄 수 있는 지능적인 NPC AI와 동적인 게임 로직 시스템 구축

---

## 📋 문서 정보

- **난이도**: 🟡 중급→🔴 고급 (AI는 예술이다!)
- **예상 학습 시간**: 10-12시간 (개념 + 구현 + 튜닝)
- **필요 선수 지식**: 
  - ✅ [1-20단계](./01_advanced_cpp_features.md) 모든 내용 완료
  - ✅ 기본적인 AI 개념 (FSM, 행동트리)
  - ✅ 게임 플레이 경험 (RPG, RTS 게임)
  - ✅ "좋은 AI"와 "나쁜 AI"를 구분할 수 있는 감각
- **실습 환경**: C++17, Lua 스크립팅, AI 프레임워크
- **최종 결과물**: 와우급 지능형 NPC 시스템!

---

## 🤔 왜 게임 AI가 이렇게 중요할까?

### **플레이어가 게임을 계속 하는 이유**

게임에서 가장 중요한 것은 무엇일까요? 바로 **"재미"**입니다. 그리고 AI는 재미의 핵심입니다.

**😴 지루한 게임의 특징:**
```
"아, 또 그 몬스터네... 어차피 똑같이 행동할거야"
"NPC들이 다들 로봇 같아"
"패턴이 너무 뻔해서 재미없어"
```

**🤩 재미있는 게임의 특징:**
```
"이 보스는 매번 다르게 공격하네?"
"NPC가 내 행동을 기억하고 있는 것 같아"
"상황에 따라 완전히 다른 전략을 써야 해"
```

### **단순한 AI vs 지능적인 AI - 하늘과 땅 차이**

실제 예시로 비교해보겠습니다.

```cpp
// 😴 단순한 AI (90년대 게임 수준) - 로봇 같은 NPC
class SimpleMonster {
public:
    void Update() {
        if (player_nearby) {
            Attack();  // 무조건 공격 (생각 없음)
        } else {
            Patrol(); // 무조건 순찰 (단조로움)
        }
        // 📉 문제점들:
        // - 항상 똑같은 반응
        // - 상황 판단 없음
        // - 학습 능력 없음
        // - 플레이어가 금방 패턴 파악
    }
    // 😴 결과: "아 또 이거네..." 하며 플레이어가 지루해함
};

// 🧠 지능적인 AI (현대 게임 수준)
class IntelligentMonster {
private:
    BehaviorTree behavior_tree;
    AIBlackboard knowledge;
    ThreatAssessment threat_system;
    
public:
    void Update(float delta_time) {
        // 1. 상황 인식
        AnalyzeEnvironment();
        
        // 2. 위협 평가
        threat_system.EvaluateThreats();
        
        // 3. 동적 의사결정
        behavior_tree.Execute(delta_time);
        
        // 4. 학습 및 적응
        AdaptToPlayerBehavior();
    }
    // 결과: 예측 불가능하고 도전적인 적
};
```

**게임 경험의 차이:**
- **단순 AI**: 플레이어가 패턴을 파악하면 쉬워짐 → 지루함
- **지능적 AI**: 항상 새로운 도전 → 몰입감과 재미

**서버 성능 영향:**
- **좋은 AI**: CPU 사용량 5-10% → 흥미로운 게임플레이
- **나쁜 AI**: CPU 사용량 50%+ → 성능 저하 + 재미없음

---

## 🎭 1. FSM vs Behavior Tree - AI 설계 패턴 선택

### **1.1 유한 상태 기계 (FSM) - 간단하고 명확한 AI**

```cpp
#include <iostream>
#include <unordered_map>
#include <functional>
#include <chrono>

// 🎯 몬스터 상태 정의
enum class MonsterState {
    IDLE,
    PATROL,
    CHASE,
    ATTACK,
    RETREAT,
    DEAD
};

// 🎯 FSM 기반 몬스터 AI
class FSMMonster {
private:
    MonsterState current_state = MonsterState::IDLE;
    std::unordered_map<MonsterState, std::function<void(float)>> state_functions;
    std::unordered_map<MonsterState, std::function<bool()>> transition_conditions;
    
    // 몬스터 속성
    float health = 100.0f;
    float max_health = 100.0f;
    float attack_range = 50.0f;
    float chase_range = 100.0f;
    Vector3 position{0, 0, 0};
    Vector3 target_position{0, 0, 0};
    Vector3 patrol_center{0, 0, 0};
    float patrol_radius = 150.0f;
    
    // 시간 관리
    float last_attack_time = 0.0f;
    float attack_cooldown = 2.0f;
    float state_enter_time = 0.0f;

public:
    FSMMonster(Vector3 spawn_pos) : position(spawn_pos), patrol_center(spawn_pos) {
        SetupStateFunctions();
        SetupTransitions();
        ChangeState(MonsterState::IDLE);
    }
    
    void Update(float delta_time) {
        // 현재 상태 실행
        auto it = state_functions.find(current_state);
        if (it != state_functions.end()) {
            it->second(delta_time);
        }
        
        // 상태 전환 확인
        CheckStateTransitions();
        
        // 디버그 정보 출력
        if (static_cast<int>(GetCurrentTime()) % 3 == 0) {
            PrintDebugInfo();
        }
    }

private:
    void SetupStateFunctions() {
        // IDLE 상태: 가만히 있으면서 주변 감시
        state_functions[MonsterState::IDLE] = [this](float dt) {
            // 체력 회복 (천천히)
            health = std::min(max_health, health + dt * 5.0f);
            
            // 간혹 주변을 둘러보는 행동
            if (GetTimeSinceStateChange() > 3.0f) {
                ChangeState(MonsterState::PATROL);
            }
        };
        
        // PATROL 상태: 정해진 영역을 순찰
        state_functions[MonsterState::PATROL] = [this](float dt) {
            // 목표 지점까지 이동
            Vector3 direction = target_position - position;
            float distance = direction.Length();
            
            if (distance > 5.0f) {
                direction.Normalize();
                position += direction * 30.0f * dt;  // 이동 속도
            } else {
                // 새로운 순찰 지점 선택
                SelectNewPatrolPoint();
            }
        };
        
        // CHASE 상태: 플레이어 추격
        state_functions[MonsterState::CHASE] = [this](float dt) {
            Vector3 player_pos = GetNearestPlayerPosition();
            Vector3 direction = player_pos - position;
            float distance = direction.Length();
            
            if (distance > attack_range) {
                // 플레이어를 향해 빠르게 이동
                direction.Normalize();
                position += direction * 60.0f * dt;  // 추격 속도 (순찰의 2배)
                
                std::cout << "🏃 추격 중... 거리: " << distance << std::endl;
            }
        };
        
        // ATTACK 상태: 공격 수행
        state_functions[MonsterState::ATTACK] = [this](float dt) {
            float current_time = GetCurrentTime();
            
            if (current_time - last_attack_time >= attack_cooldown) {
                PerformAttack();
                last_attack_time = current_time;
                
                std::cout << "⚔️ 공격! (데미지: 25)" << std::endl;
            }
        };
        
        // RETREAT 상태: 후퇴
        state_functions[MonsterState::RETREAT] = [this](float dt) {
            Vector3 player_pos = GetNearestPlayerPosition();
            Vector3 direction = position - player_pos;  // 플레이어 반대 방향
            direction.Normalize();
            
            position += direction * 40.0f * dt;  // 후퇴 속도
            
            // 충분히 멀어지면 체력 회복 시작
            health += dt * 10.0f;  // 빠른 회복
            
            std::cout << "🏃‍♂️ 후퇴 중... 체력 회복: " << health << std::endl;
        };
        
        // DEAD 상태: 사망 처리
        state_functions[MonsterState::DEAD] = [this](float dt) {
            // 리스폰 대기 또는 제거 처리
            std::cout << "💀 사망 상태" << std::endl;
        };
    }
    
    void SetupTransitions() {
        transition_conditions[MonsterState::IDLE] = [this]() {
            Vector3 player_pos = GetNearestPlayerPosition();
            float distance = (player_pos - position).Length();
            
            if (health <= 0) {
                ChangeState(MonsterState::DEAD);
                return true;
            }
            
            if (distance <= chase_range) {
                ChangeState(MonsterState::CHASE);
                return true;
            }
            
            return false;
        };
        
        transition_conditions[MonsterState::PATROL] = [this]() {
            Vector3 player_pos = GetNearestPlayerPosition();
            float distance = (player_pos - position).Length();
            
            if (health <= 0) {
                ChangeState(MonsterState::DEAD);
                return true;
            }
            
            if (distance <= chase_range) {
                ChangeState(MonsterState::CHASE);
                return true;
            }
            
            // 순찰 영역을 너무 벗어나면 돌아가기
            float distance_from_center = (position - patrol_center).Length();
            if (distance_from_center > patrol_radius * 2.0f) {
                target_position = patrol_center;
                return false;  // 계속 순찰하면서 중앙으로
            }
            
            return false;
        };
        
        transition_conditions[MonsterState::CHASE] = [this]() {
            Vector3 player_pos = GetNearestPlayerPosition();
            float distance = (player_pos - position).Length();
            
            if (health <= 0) {
                ChangeState(MonsterState::DEAD);
                return true;
            }
            
            if (health < max_health * 0.3f) {  // 체력 30% 이하
                ChangeState(MonsterState::RETREAT);
                return true;
            }
            
            if (distance <= attack_range) {
                ChangeState(MonsterState::ATTACK);
                return true;
            }
            
            if (distance > chase_range * 1.5f) {  // 너무 멀어지면 포기
                ChangeState(MonsterState::PATROL);
                return true;
            }
            
            return false;
        };
        
        transition_conditions[MonsterState::ATTACK] = [this]() {
            Vector3 player_pos = GetNearestPlayerPosition();
            float distance = (player_pos - position).Length();
            
            if (health <= 0) {
                ChangeState(MonsterState::DEAD);
                return true;
            }
            
            if (health < max_health * 0.3f) {
                ChangeState(MonsterState::RETREAT);
                return true;
            }
            
            if (distance > attack_range * 1.2f) {  // 공격 범위를 벗어남
                ChangeState(MonsterState::CHASE);
                return true;
            }
            
            return false;
        };
        
        transition_conditions[MonsterState::RETREAT] = [this]() {
            if (health <= 0) {
                ChangeState(MonsterState::DEAD);
                return true;
            }
            
            Vector3 player_pos = GetNearestPlayerPosition();
            float distance = (player_pos - position).Length();
            
            // 충분히 회복되고 거리가 확보되면 다시 전투
            if (health > max_health * 0.7f && distance > chase_range * 2.0f) {
                ChangeState(MonsterState::PATROL);
                return true;
            }
            
            return false;
        };
    }
    
    void ChangeState(MonsterState new_state) {
        if (current_state != new_state) {
            std::cout << "🔄 상태 변경: " << StateToString(current_state) 
                      << " → " << StateToString(new_state) << std::endl;
            
            current_state = new_state;
            state_enter_time = GetCurrentTime();
            
            // 상태 진입 시 특별한 처리
            OnStateEnter(new_state);
        }
    }
    
    void CheckStateTransitions() {
        auto it = transition_conditions.find(current_state);
        if (it != transition_conditions.end()) {
            it->second();
        }
    }
    
    void OnStateEnter(MonsterState state) {
        switch (state) {
            case MonsterState::PATROL:
                SelectNewPatrolPoint();
                break;
            case MonsterState::ATTACK:
                std::cout << "🎯 공격 대상 포착!" << std::endl;
                break;
            case MonsterState::RETREAT:
                std::cout << "😰 위험하다! 후퇴!" << std::endl;
                break;
            default:
                break;
        }
    }
    
    // 유틸리티 함수들
    void SelectNewPatrolPoint() {
        // 순찰 중심점 주변의 랜덤한 지점 선택
        float angle = static_cast<float>(rand()) / RAND_MAX * 2.0f * 3.14159f;
        float distance = static_cast<float>(rand()) / RAND_MAX * patrol_radius;
        
        target_position = patrol_center + Vector3(
            cos(angle) * distance,
            0,
            sin(angle) * distance
        );
    }
    
    Vector3 GetNearestPlayerPosition() {
        // 실제로는 게임 서버에서 가장 가까운 플레이어 찾기
        // 여기서는 시뮬레이션을 위해 가상의 플레이어 위치 반환
        static Vector3 player_pos{100, 0, 100};
        
        // 플레이어가 움직이는 시뮬레이션
        float time = GetCurrentTime();
        player_pos.x = 100 + sin(time * 0.5f) * 50;
        player_pos.z = 100 + cos(time * 0.3f) * 50;
        
        return player_pos;
    }
    
    void PerformAttack() {
        // 실제 게임에서는 데미지 계산, 이펙트 등
        Vector3 player_pos = GetNearestPlayerPosition();
        std::cout << "💥 공격 실행! 목표: (" << player_pos.x << ", " << player_pos.z << ")" << std::endl;
    }
    
    float GetCurrentTime() {
        static auto start_time = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<float>(now - start_time).count();
    }
    
    float GetTimeSinceStateChange() {
        return GetCurrentTime() - state_enter_time;
    }
    
    std::string StateToString(MonsterState state) {
        switch (state) {
            case MonsterState::IDLE: return "대기";
            case MonsterState::PATROL: return "순찰";
            case MonsterState::CHASE: return "추격";
            case MonsterState::ATTACK: return "공격";
            case MonsterState::RETREAT: return "후퇴";
            case MonsterState::DEAD: return "사망";
            default: return "알수없음";
        }
    }
    
    void PrintDebugInfo() {
        Vector3 player_pos = GetNearestPlayerPosition();
        float distance = (player_pos - position).Length();
        
        std::cout << "🤖 [" << StateToString(current_state) << "] "
                  << "체력: " << static_cast<int>(health) << "/" << static_cast<int>(max_health)
                  << ", 플레이어 거리: " << static_cast<int>(distance)
                  << ", 위치: (" << static_cast<int>(position.x) << ", " << static_cast<int>(position.z) << ")"
                  << std::endl;
    }

public:
    // Getter 함수들
    MonsterState GetCurrentState() const { return current_state; }
    float GetHealth() const { return health; }
    Vector3 GetPosition() const { return position; }
    
    // 외부에서 데미지 가하기
    void TakeDamage(float damage) {
        health -= damage;
        if (health < 0) health = 0;
        
        std::cout << "💢 " << damage << " 데미지 받음! 남은 체력: " << health << std::endl;
    }
};

// Vector3 구조체 정의
struct Vector3 {
    float x, y, z;
    
    Vector3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}
    
    Vector3 operator+(const Vector3& other) const {
        return Vector3(x + other.x, y + other.y, z + other.z);
    }
    
    Vector3 operator-(const Vector3& other) const {
        return Vector3(x - other.x, y - other.y, z - other.z);
    }
    
    Vector3 operator*(float scalar) const {
        return Vector3(x * scalar, y * scalar, z * scalar);
    }
    
    Vector3& operator+=(const Vector3& other) {
        x += other.x; y += other.y; z += other.z;
        return *this;
    }
    
    float Length() const {
        return std::sqrt(x * x + y * y + z * z);
    }
    
    void Normalize() {
        float length = Length();
        if (length > 0) {
            x /= length; y /= length; z /= length;
        }
    }
};
```

**💡 FSM의 장점:**
- **단순함**: 상태와 전환이 명확
- **디버깅**: 현재 상태를 쉽게 파악
- **성능**: 빠른 실행 속도

**⚠️ FSM의 한계:**
- **복잡성 증가**: 상태가 많아지면 관리 어려움
- **경직성**: 유연한 행동 표현 제한

### **1.2 행동 트리 (Behavior Tree) - 유연하고 확장 가능한 AI**

```cpp
#include <iostream>
#include <vector>
#include <memory>
#include <functional>

// 🎯 행동 트리 노드 기본 클래스
enum class NodeStatus {
    SUCCESS,
    FAILURE,
    RUNNING
};

class BehaviorNode {
public:
    virtual ~BehaviorNode() = default;
    virtual NodeStatus Execute(float delta_time) = 0;
    virtual void Reset() {}
    virtual std::string GetName() const = 0;
};

// 🎯 AI 블랙보드 (공유 데이터)
class AIBlackboard {
private:
    std::unordered_map<std::string, std::any> data;
    
public:
    template<typename T>
    void Set(const std::string& key, const T& value) {
        data[key] = value;
    }
    
    template<typename T>
    T Get(const std::string& key, const T& default_value = T{}) const {
        auto it = data.find(key);
        if (it != data.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (const std::bad_any_cast&) {
                return default_value;
            }
        }
        return default_value;
    }
    
    bool Has(const std::string& key) const {
        return data.find(key) != data.end();
    }
    
    void Clear(const std::string& key) {
        data.erase(key);
    }
};

// 🎯 조건 노드 (Condition)
class ConditionNode : public BehaviorNode {
private:
    std::function<bool(AIBlackboard&)> condition_func;
    std::string name;
    AIBlackboard* blackboard;
    
public:
    ConditionNode(const std::string& node_name, 
                  std::function<bool(AIBlackboard&)> func,
                  AIBlackboard* bb) 
        : name(node_name), condition_func(func), blackboard(bb) {}
    
    NodeStatus Execute(float delta_time) override {
        bool result = condition_func(*blackboard);
        return result ? NodeStatus::SUCCESS : NodeStatus::FAILURE;
    }
    
    std::string GetName() const override { return name; }
};

// 🎯 액션 노드 (Action)
class ActionNode : public BehaviorNode {
private:
    std::function<NodeStatus(AIBlackboard&, float)> action_func;
    std::string name;
    AIBlackboard* blackboard;
    
public:
    ActionNode(const std::string& node_name,
               std::function<NodeStatus(AIBlackboard&, float)> func,
               AIBlackboard* bb)
        : name(node_name), action_func(func), blackboard(bb) {}
    
    NodeStatus Execute(float delta_time) override {
        return action_func(*blackboard, delta_time);
    }
    
    std::string GetName() const override { return name; }
};

// 🎯 시퀀스 노드 (Sequence) - 모든 자식이 성공해야 성공
class SequenceNode : public BehaviorNode {
private:
    std::vector<std::unique_ptr<BehaviorNode>> children;
    size_t current_child = 0;
    
public:
    void AddChild(std::unique_ptr<BehaviorNode> child) {
        children.push_back(std::move(child));
    }
    
    NodeStatus Execute(float delta_time) override {
        while (current_child < children.size()) {
            NodeStatus status = children[current_child]->Execute(delta_time);
            
            if (status == NodeStatus::FAILURE) {
                Reset();
                return NodeStatus::FAILURE;
            }
            
            if (status == NodeStatus::RUNNING) {
                return NodeStatus::RUNNING;
            }
            
            // SUCCESS인 경우 다음 자식으로
            current_child++;
        }
        
        // 모든 자식이 성공
        Reset();
        return NodeStatus::SUCCESS;
    }
    
    void Reset() override {
        current_child = 0;
        for (auto& child : children) {
            child->Reset();
        }
    }
    
    std::string GetName() const override { return "Sequence"; }
};

// 🎯 셀렉터 노드 (Selector) - 하나의 자식이 성공하면 성공
class SelectorNode : public BehaviorNode {
private:
    std::vector<std::unique_ptr<BehaviorNode>> children;
    size_t current_child = 0;
    
public:
    void AddChild(std::unique_ptr<BehaviorNode> child) {
        children.push_back(std::move(child));
    }
    
    NodeStatus Execute(float delta_time) override {
        while (current_child < children.size()) {
            NodeStatus status = children[current_child]->Execute(delta_time);
            
            if (status == NodeStatus::SUCCESS) {
                Reset();
                return NodeStatus::SUCCESS;
            }
            
            if (status == NodeStatus::RUNNING) {
                return NodeStatus::RUNNING;
            }
            
            // FAILURE인 경우 다음 자식으로
            current_child++;
        }
        
        // 모든 자식이 실패
        Reset();
        return NodeStatus::FAILURE;
    }
    
    void Reset() override {
        current_child = 0;
        for (auto& child : children) {
            child->Reset();
        }
    }
    
    std::string GetName() const override { return "Selector"; }
};

// 🎯 행동 트리 기반 몬스터 AI
class BehaviorTreeMonster {
private:
    std::unique_ptr<BehaviorNode> root_node;
    AIBlackboard blackboard;
    
    // 몬스터 속성
    Vector3 position{0, 0, 0};
    float health = 100.0f;
    float max_health = 100.0f;
    
public:
    BehaviorTreeMonster(Vector3 spawn_pos) : position(spawn_pos) {
        InitializeBlackboard();
        BuildBehaviorTree();
    }
    
    void Update(float delta_time) {
        // 블랙보드 업데이트
        UpdateBlackboard(delta_time);
        
        // 행동 트리 실행
        NodeStatus status = root_node->Execute(delta_time);
        
        // 디버그 정보
        if (static_cast<int>(GetCurrentTime()) % 2 == 0) {
            PrintDebugInfo();
        }
    }

private:
    void InitializeBlackboard() {
        blackboard.Set<Vector3>("position", position);
        blackboard.Set<float>("health", health);
        blackboard.Set<float>("max_health", max_health);
        blackboard.Set<Vector3>("patrol_center", position);
        blackboard.Set<float>("patrol_radius", 150.0f);
        blackboard.Set<float>("chase_range", 100.0f);
        blackboard.Set<float>("attack_range", 50.0f);
        blackboard.Set<float>("last_attack_time", 0.0f);
        blackboard.Set<float>("attack_cooldown", 2.0f);
    }
    
    void UpdateBlackboard(float delta_time) {
        blackboard.Set<Vector3>("position", position);
        blackboard.Set<float>("health", health);
        
        // 플레이어 위치 업데이트 (시뮬레이션)
        Vector3 player_pos = GetNearestPlayerPosition();
        blackboard.Set<Vector3>("player_position", player_pos);
        
        float distance_to_player = (player_pos - position).Length();
        blackboard.Set<float>("distance_to_player", distance_to_player);
        
        blackboard.Set<float>("current_time", GetCurrentTime());
    }
    
    void BuildBehaviorTree() {
        // 🌳 행동 트리 구조:
        // Root (Selector)
        // ├── Combat Sequence
        // │   ├── IsPlayerNearby?
        // │   └── Combat Selector
        // │       ├── Attack Sequence
        // │       │   ├── IsInAttackRange?
        // │       │   ├── CanAttack?
        // │       │   └── Attack
        // │       ├── Chase Sequence
        // │       │   ├── IsPlayerInChaseRange?
        // │       │   └── ChasePlayer
        // │       └── Retreat Sequence
        // │           ├── IsLowHealth?
        // │           └── RetreatFromPlayer
        // └── Patrol Sequence
        //     ├── SelectPatrolPoint
        //     └── MoveToTarget
        
        auto root = std::make_unique<SelectorNode>();
        
        // === 전투 시퀀스 ===
        auto combat_sequence = std::make_unique<SequenceNode>();
        
        // 플레이어가 근처에 있는가?
        auto is_player_nearby = std::make_unique<ConditionNode>(
            "IsPlayerNearby",
            [](AIBlackboard& bb) {
                float distance = bb.Get<float>("distance_to_player", 999.0f);
                float chase_range = bb.Get<float>("chase_range", 100.0f);
                return distance <= chase_range;
            },
            &blackboard
        );
        
        // 전투 행동 선택자
        auto combat_selector = std::make_unique<SelectorNode>();
        
        // --- 공격 시퀀스 ---
        auto attack_sequence = std::make_unique<SequenceNode>();
        
        auto is_in_attack_range = std::make_unique<ConditionNode>(
            "IsInAttackRange",
            [](AIBlackboard& bb) {
                float distance = bb.Get<float>("distance_to_player", 999.0f);
                float attack_range = bb.Get<float>("attack_range", 50.0f);
                return distance <= attack_range;
            },
            &blackboard
        );
        
        auto can_attack = std::make_unique<ConditionNode>(
            "CanAttack",
            [](AIBlackboard& bb) {
                float current_time = bb.Get<float>("current_time", 0.0f);
                float last_attack = bb.Get<float>("last_attack_time", 0.0f);
                float cooldown = bb.Get<float>("attack_cooldown", 2.0f);
                return (current_time - last_attack) >= cooldown;
            },
            &blackboard
        );
        
        auto attack_action = std::make_unique<ActionNode>(
            "Attack",
            [this](AIBlackboard& bb, float dt) {
                Vector3 player_pos = bb.Get<Vector3>("player_position");
                std::cout << "⚔️ 공격! 목표: (" << player_pos.x << ", " << player_pos.z << ")" << std::endl;
                
                bb.Set<float>("last_attack_time", bb.Get<float>("current_time", 0.0f));
                return NodeStatus::SUCCESS;
            },
            &blackboard
        );
        
        attack_sequence->AddChild(std::move(is_in_attack_range));
        attack_sequence->AddChild(std::move(can_attack));
        attack_sequence->AddChild(std::move(attack_action));
        
        // --- 추격 시퀀스 ---
        auto chase_sequence = std::make_unique<SequenceNode>();
        
        auto is_in_chase_range = std::make_unique<ConditionNode>(
            "IsInChaseRange",
            [](AIBlackboard& bb) {
                float distance = bb.Get<float>("distance_to_player", 999.0f);
                float chase_range = bb.Get<float>("chase_range", 100.0f);
                float attack_range = bb.Get<float>("attack_range", 50.0f);
                return distance <= chase_range && distance > attack_range;
            },
            &blackboard
        );
        
        auto chase_action = std::make_unique<ActionNode>(
            "ChasePlayer",
            [this](AIBlackboard& bb, float dt) {
                Vector3 player_pos = bb.Get<Vector3>("player_position");
                Vector3 current_pos = bb.Get<Vector3>("position");
                
                Vector3 direction = player_pos - current_pos;
                direction.Normalize();
                
                position += direction * 60.0f * dt;  // 추격 속도
                
                std::cout << "🏃 추격 중..." << std::endl;
                return NodeStatus::RUNNING;
            },
            &blackboard
        );
        
        chase_sequence->AddChild(std::move(is_in_chase_range));
        chase_sequence->AddChild(std::move(chase_action));
        
        // --- 후퇴 시퀀스 ---
        auto retreat_sequence = std::make_unique<SequenceNode>();
        
        auto is_low_health = std::make_unique<ConditionNode>(
            "IsLowHealth",
            [](AIBlackboard& bb) {
                float health = bb.Get<float>("health", 100.0f);
                float max_health = bb.Get<float>("max_health", 100.0f);
                return health < max_health * 0.3f;  // 30% 이하
            },
            &blackboard
        );
        
        auto retreat_action = std::make_unique<ActionNode>(
            "RetreatFromPlayer",
            [this](AIBlackboard& bb, float dt) {
                Vector3 player_pos = bb.Get<Vector3>("player_position");
                Vector3 current_pos = bb.Get<Vector3>("position");
                
                Vector3 direction = current_pos - player_pos;  // 반대 방향
                direction.Normalize();
                
                position += direction * 40.0f * dt;  // 후퇴 속도
                health += dt * 10.0f;  // 회복
                
                std::cout << "🏃‍♂️ 후퇴하며 회복 중... 체력: " << health << std::endl;
                return NodeStatus::RUNNING;
            },
            &blackboard
        );
        
        retreat_sequence->AddChild(std::move(is_low_health));
        retreat_sequence->AddChild(std::move(retreat_action));
        
        // 전투 선택자에 행동들 추가
        combat_selector->AddChild(std::move(retreat_sequence));  // 우선순위: 후퇴
        combat_selector->AddChild(std::move(attack_sequence));   // 우선순위: 공격
        combat_selector->AddChild(std::move(chase_sequence));    // 우선순위: 추격
        
        // 전투 시퀀스 완성
        combat_sequence->AddChild(std::move(is_player_nearby));
        combat_sequence->AddChild(std::move(combat_selector));
        
        // === 순찰 시퀀스 ===
        auto patrol_sequence = std::make_unique<SequenceNode>();
        
        auto select_patrol_point = std::make_unique<ActionNode>(
            "SelectPatrolPoint",
            [this](AIBlackboard& bb, float dt) {
                if (!bb.Has("target_position")) {
                    Vector3 patrol_center = bb.Get<Vector3>("patrol_center");
                    float patrol_radius = bb.Get<float>("patrol_radius", 150.0f);
                    
                    // 랜덤한 순찰 지점 선택
                    float angle = static_cast<float>(rand()) / RAND_MAX * 2.0f * 3.14159f;
                    float distance = static_cast<float>(rand()) / RAND_MAX * patrol_radius;
                    
                    Vector3 target = patrol_center + Vector3(
                        cos(angle) * distance, 0, sin(angle) * distance
                    );
                    
                    bb.Set<Vector3>("target_position", target);
                    std::cout << "🎯 새 순찰 지점 선택: (" << target.x << ", " << target.z << ")" << std::endl;
                }
                return NodeStatus::SUCCESS;
            },
            &blackboard
        );
        
        auto move_to_target = std::make_unique<ActionNode>(
            "MoveToTarget",
            [this](AIBlackboard& bb, float dt) {
                Vector3 target = bb.Get<Vector3>("target_position");
                Vector3 current_pos = bb.Get<Vector3>("position");
                
                Vector3 direction = target - current_pos;
                float distance = direction.Length();
                
                if (distance > 5.0f) {
                    direction.Normalize();
                    position += direction * 30.0f * dt;  // 순찰 속도
                    return NodeStatus::RUNNING;
                } else {
                    bb.Clear("target_position");  // 목표 지점 도달
                    std::cout << "✅ 순찰 지점 도달" << std::endl;
                    return NodeStatus::SUCCESS;
                }
            },
            &blackboard
        );
        
        patrol_sequence->AddChild(std::move(select_patrol_point));
        patrol_sequence->AddChild(std::move(move_to_target));
        
        // === 루트 노드 완성 ===
        root->AddChild(std::move(combat_sequence));  // 우선순위: 전투
        root->AddChild(std::move(patrol_sequence));  // 우선순위: 순찰
        
        root_node = std::move(root);
    }
    
    Vector3 GetNearestPlayerPosition() {
        // FSM과 동일한 시뮬레이션
        static Vector3 player_pos{100, 0, 100};
        float time = GetCurrentTime();
        player_pos.x = 100 + sin(time * 0.5f) * 50;
        player_pos.z = 100 + cos(time * 0.3f) * 50;
        return player_pos;
    }
    
    float GetCurrentTime() {
        static auto start_time = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<float>(now - start_time).count();
    }
    
    void PrintDebugInfo() {
        Vector3 player_pos = blackboard.Get<Vector3>("player_position");
        float distance = blackboard.Get<float>("distance_to_player", 0.0f);
        
        std::cout << "🌳 [Behavior Tree] "
                  << "체력: " << static_cast<int>(health) << "/" << static_cast<int>(max_health)
                  << ", 플레이어 거리: " << static_cast<int>(distance)
                  << ", 위치: (" << static_cast<int>(position.x) << ", " << static_cast<int>(position.z) << ")"
                  << std::endl;
    }

public:
    void TakeDamage(float damage) {
        health -= damage;
        if (health < 0) health = 0;
        std::cout << "💢 " << damage << " 데미지 받음! 남은 체력: " << health << std::endl;
    }
    
    Vector3 GetPosition() const { return position; }
    float GetHealth() const { return health; }
};
```

**💡 Behavior Tree의 장점:**
- **유연성**: 복잡한 행동을 조합 가능
- **재사용성**: 노드들을 다른 AI에서 재활용
- **시각적**: 트리 구조로 이해하기 쉬움
- **확장성**: 새로운 노드 추가가 간단

**⚠️ Behavior Tree의 단점:**
- **복잡성**: 초기 구축 비용이 높음
- **성능**: FSM보다 약간 느림

---

## 🗺️ 2. A* 경로 찾기 최적화

### **2.1 기본 A* 알고리즘**

```cpp
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cmath>

// 🎯 그리드 기반 맵
class GridMap {
public:
    struct Node {
        int x, y;
        bool walkable;
        float cost = 1.0f;  // 이동 비용
        
        Node(int x = 0, int y = 0, bool walkable = true) 
            : x(x), y(y), walkable(walkable) {}
        
        bool operator==(const Node& other) const {
            return x == other.x && y == other.y;
        }
    };
    
private:
    std::vector<std::vector<Node>> grid;
    int width, height;
    
public:
    GridMap(int w, int h) : width(w), height(h) {
        grid.resize(height);
        for (int y = 0; y < height; ++y) {
            grid[y].resize(width);
            for (int x = 0; x < width; ++x) {
                grid[y][x] = Node(x, y, true);
            }
        }
    }
    
    void SetObstacle(int x, int y) {
        if (IsValid(x, y)) {
            grid[y][x].walkable = false;
        }
    }
    
    void SetCost(int x, int y, float cost) {
        if (IsValid(x, y)) {
            grid[y][x].cost = cost;
        }
    }
    
    bool IsValid(int x, int y) const {
        return x >= 0 && x < width && y >= 0 && y < height;
    }
    
    bool IsWalkable(int x, int y) const {
        return IsValid(x, y) && grid[y][x].walkable;
    }
    
    float GetCost(int x, int y) const {
        return IsValid(x, y) ? grid[y][x].cost : 999.0f;
    }
    
    std::vector<Node> GetNeighbors(int x, int y) const {
        std::vector<Node> neighbors;
        
        // 8방향 이동 (대각선 포함)
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                if (dx == 0 && dy == 0) continue;
                
                int nx = x + dx;
                int ny = y + dy;
                
                if (IsWalkable(nx, ny)) {
                    neighbors.push_back(grid[ny][nx]);
                }
            }
        }
        
        return neighbors;
    }
    
    void PrintMap() const {
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                if (!grid[y][x].walkable) {
                    std::cout << "█";
                } else if (grid[y][x].cost > 1.0f) {
                    std::cout << "~";  // 늪지 등
                } else {
                    std::cout << ".";
                }
            }
            std::cout << std::endl;
        }
    }
};

// 🎯 A* 경로 탐색기
class AStarPathfinder {
private:
    struct PathNode {
        GridMap::Node node;
        float g_cost = 0;     // 시작점부터의 실제 비용
        float h_cost = 0;     // 목표점까지의 추정 비용 (휴리스틱)
        float f_cost = 0;     // g_cost + h_cost
        PathNode* parent = nullptr;
        
        PathNode(const GridMap::Node& n) : node(n) {}
        
        void CalculateFCost() {
            f_cost = g_cost + h_cost;
        }
    };
    
    struct PathNodeComparator {
        bool operator()(const PathNode* a, const PathNode* b) const {
            // f_cost가 낮은 것이 우선순위 높음
            if (a->f_cost != b->f_cost) {
                return a->f_cost > b->f_cost;
            }
            // f_cost가 같으면 h_cost가 낮은 것 우선
            return a->h_cost > b->h_cost;
        }
    };
    
    GridMap* map;
    
public:
    AStarPathfinder(GridMap* grid_map) : map(grid_map) {}
    
    std::vector<GridMap::Node> FindPath(int start_x, int start_y, int goal_x, int goal_y) {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        std::priority_queue<PathNode*, std::vector<PathNode*>, PathNodeComparator> open_set;
        std::unordered_map<std::string, PathNode*> all_nodes;
        std::unordered_set<std::string> closed_set;
        
        // 시작 노드 생성
        GridMap::Node start_node(start_x, start_y);
        PathNode* start_path_node = new PathNode(start_node);
        start_path_node->g_cost = 0;
        start_path_node->h_cost = CalculateHeuristic(start_x, start_y, goal_x, goal_y);
        start_path_node->CalculateFCost();
        
        std::string start_key = NodeToKey(start_x, start_y);
        all_nodes[start_key] = start_path_node;
        open_set.push(start_path_node);
        
        int nodes_explored = 0;
        
        while (!open_set.empty()) {
            // 가장 유망한 노드 선택
            PathNode* current = open_set.top();
            open_set.pop();
            
            std::string current_key = NodeToKey(current->node.x, current->node.y);
            
            // 이미 처리된 노드면 건너뛰기
            if (closed_set.find(current_key) != closed_set.end()) {
                continue;
            }
            
            closed_set.insert(current_key);
            nodes_explored++;
            
            // 목표에 도달했는가?
            if (current->node.x == goal_x && current->node.y == goal_y) {
                auto end_time = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
                
                std::cout << "🎯 경로 찾기 완료!" << std::endl;
                std::cout << "탐색한 노드: " << nodes_explored << "개" << std::endl;
                std::cout << "소요 시간: " << duration.count() << "μs" << std::endl;
                
                auto path = ReconstructPath(current);
                
                // 메모리 정리
                for (auto& pair : all_nodes) {
                    delete pair.second;
                }
                
                return path;
            }
            
            // 인접 노드들 탐색
            auto neighbors = map->GetNeighbors(current->node.x, current->node.y);
            
            for (const auto& neighbor : neighbors) {
                std::string neighbor_key = NodeToKey(neighbor.x, neighbor.y);
                
                // 이미 처리된 노드면 건너뛰기
                if (closed_set.find(neighbor_key) != closed_set.end()) {
                    continue;
                }
                
                // 이동 비용 계산 (대각선 이동 고려)
                bool is_diagonal = (std::abs(neighbor.x - current->node.x) == 1 && 
                                   std::abs(neighbor.y - current->node.y) == 1);
                float movement_cost = is_diagonal ? 1.414f : 1.0f;  // √2 ≈ 1.414
                float terrain_cost = map->GetCost(neighbor.x, neighbor.y);
                
                float tentative_g_cost = current->g_cost + movement_cost * terrain_cost;
                
                PathNode* neighbor_node = nullptr;
                auto it = all_nodes.find(neighbor_key);
                
                if (it == all_nodes.end()) {
                    // 새로운 노드 생성
                    neighbor_node = new PathNode(neighbor);
                    all_nodes[neighbor_key] = neighbor_node;
                } else {
                    neighbor_node = it->second;
                }
                
                // 더 좋은 경로를 찾았는가?
                if (neighbor_node->g_cost == 0 || tentative_g_cost < neighbor_node->g_cost) {
                    neighbor_node->parent = current;
                    neighbor_node->g_cost = tentative_g_cost;
                    neighbor_node->h_cost = CalculateHeuristic(neighbor.x, neighbor.y, goal_x, goal_y);
                    neighbor_node->CalculateFCost();
                    
                    open_set.push(neighbor_node);
                }
            }
        }
        
        // 경로를 찾지 못함
        std::cout << "❌ 경로를 찾을 수 없습니다." << std::endl;
        
        // 메모리 정리
        for (auto& pair : all_nodes) {
            delete pair.second;
        }
        
        return {};
    }

private:
    float CalculateHeuristic(int x1, int y1, int x2, int y2) {
        // 맨하탄 거리
        // return std::abs(x2 - x1) + std::abs(y2 - y1);
        
        // 유클리드 거리
        float dx = static_cast<float>(x2 - x1);
        float dy = static_cast<float>(y2 - y1);
        return std::sqrt(dx * dx + dy * dy);
        
        // 체비셰프 거리 (대각선 이동을 허용할 때 더 정확)
        // return std::max(std::abs(x2 - x1), std::abs(y2 - y1));
    }
    
    std::string NodeToKey(int x, int y) {
        return std::to_string(x) + "," + std::to_string(y);
    }
    
    std::vector<GridMap::Node> ReconstructPath(PathNode* goal_node) {
        std::vector<GridMap::Node> path;
        PathNode* current = goal_node;
        
        while (current != nullptr) {
            path.push_back(current->node);
            current = current->parent;
        }
        
        // 경로를 뒤집어서 시작점부터 목표점까지 순서로 만들기
        std::reverse(path.begin(), path.end());
        
        std::cout << "경로 길이: " << path.size() << "노드" << std::endl;
        
        return path;
    }
};
```

### **2.2 A* 최적화 기법들**

```cpp
// 🚀 최적화된 A* (계층적 경로 찾기 + 캐싱)
class OptimizedPathfinder {
private:
    GridMap* map;
    
    // 경로 캐시
    struct PathCacheKey {
        int start_x, start_y, goal_x, goal_y;
        
        bool operator==(const PathCacheKey& other) const {
            return start_x == other.start_x && start_y == other.start_y &&
                   goal_x == other.goal_x && goal_y == other.goal_y;
        }
    };
    
    struct PathCacheKeyHash {
        size_t operator()(const PathCacheKey& key) const {
            return std::hash<int>()(key.start_x) ^ 
                   (std::hash<int>()(key.start_y) << 1) ^
                   (std::hash<int>()(key.goal_x) << 2) ^
                   (std::hash<int>()(key.goal_y) << 3);
        }
    };
    
    std::unordered_map<PathCacheKey, std::vector<GridMap::Node>, PathCacheKeyHash> path_cache;
    
    // 계층적 경로 찾기를 위한 클러스터
    struct Cluster {
        int x, y;  // 클러스터 좌표
        int size;  // 클러스터 크기
        bool walkable;
        std::vector<GridMap::Node> entrances;  // 진입점들
    };
    
    std::vector<std::vector<Cluster>> clusters;
    int cluster_size = 10;  // 10x10 클러스터
    
public:
    OptimizedPathfinder(GridMap* grid_map) : map(grid_map) {
        BuildClusters();
    }
    
    std::vector<GridMap::Node> FindPathOptimized(int start_x, int start_y, int goal_x, int goal_y) {
        // 1. 캐시 확인
        PathCacheKey cache_key{start_x, start_y, goal_x, goal_y};
        auto cached_it = path_cache.find(cache_key);
        if (cached_it != path_cache.end()) {
            std::cout << "💾 캐시에서 경로 반환" << std::endl;
            return cached_it->second;
        }
        
        // 2. 거리 확인 - 가까우면 직접 A*
        float distance = std::sqrt((goal_x - start_x) * (goal_x - start_x) + 
                                  (goal_y - start_y) * (goal_y - start_y));
        
        if (distance < cluster_size * 2) {
            auto path = FindPathDirect(start_x, start_y, goal_x, goal_y);
            
            // 캐시에 저장
            if (!path.empty()) {
                path_cache[cache_key] = path;
            }
            
            return path;
        }
        
        // 3. 계층적 경로 찾기
        auto path = FindPathHierarchical(start_x, start_y, goal_x, goal_y);
        
        // 캐시에 저장
        if (!path.empty()) {
            path_cache[cache_key] = path;
        }
        
        return path;
    }

private:
    void BuildClusters() {
        // 맵을 클러스터로 분할
        int cluster_width = (map->IsValid(100, 0) ? 100 : 50) / cluster_size;
        int cluster_height = (map->IsValid(0, 100) ? 100 : 50) / cluster_size;
        
        clusters.resize(cluster_height);
        for (int cy = 0; cy < cluster_height; ++cy) {
            clusters[cy].resize(cluster_width);
            
            for (int cx = 0; cx < cluster_width; ++cx) {
                Cluster& cluster = clusters[cy][cx];
                cluster.x = cx;
                cluster.y = cy;
                cluster.size = cluster_size;
                cluster.walkable = true;
                
                // 클러스터 내부 검사
                int obstacle_count = 0;
                int total_cells = cluster_size * cluster_size;
                
                for (int y = cy * cluster_size; y < (cy + 1) * cluster_size; ++y) {
                    for (int x = cx * cluster_size; x < (cx + 1) * cluster_size; ++x) {
                        if (!map->IsWalkable(x, y)) {
                            obstacle_count++;
                        }
                    }
                }
                
                // 절반 이상이 막혀있으면 이 클러스터는 통과 불가
                if (obstacle_count > total_cells / 2) {
                    cluster.walkable = false;
                }
                
                // 클러스터 경계에서 진입점 찾기
                FindClusterEntrances(cluster);
            }
        }
        
        std::cout << "🗺️ 클러스터 시스템 구축 완료: " 
                  << cluster_width << "x" << cluster_height << std::endl;
    }
    
    void FindClusterEntrances(Cluster& cluster) {
        int base_x = cluster.x * cluster_size;
        int base_y = cluster.y * cluster_size;
        
        // 상하 경계에서 진입점 찾기
        for (int x = base_x; x < base_x + cluster_size; ++x) {
            // 상단 경계
            if (map->IsWalkable(x, base_y)) {
                cluster.entrances.push_back(GridMap::Node(x, base_y));
            }
            // 하단 경계
            if (map->IsWalkable(x, base_y + cluster_size - 1)) {
                cluster.entrances.push_back(GridMap::Node(x, base_y + cluster_size - 1));
            }
        }
        
        // 좌우 경계에서 진입점 찾기
        for (int y = base_y; y < base_y + cluster_size; ++y) {
            // 좌측 경계
            if (map->IsWalkable(base_x, y)) {
                cluster.entrances.push_back(GridMap::Node(base_x, y));
            }
            // 우측 경계
            if (map->IsWalkable(base_x + cluster_size - 1, y)) {
                cluster.entrances.push_back(GridMap::Node(base_x + cluster_size - 1, y));
            }
        }
    }
    
    std::vector<GridMap::Node> FindPathDirect(int start_x, int start_y, int goal_x, int goal_y) {
        // 기본 A* 알고리즘 사용
        AStarPathfinder basic_pathfinder(map);
        return basic_pathfinder.FindPath(start_x, start_y, goal_x, goal_y);
    }
    
    std::vector<GridMap::Node> FindPathHierarchical(int start_x, int start_y, int goal_x, int goal_y) {
        std::cout << "🌐 계층적 경로 찾기 시작" << std::endl;
        
        // 1. 시작점과 목표점이 속한 클러스터 찾기
        int start_cluster_x = start_x / cluster_size;
        int start_cluster_y = start_y / cluster_size;
        int goal_cluster_x = goal_x / cluster_size;
        int goal_cluster_y = goal_y / cluster_size;
        
        // 2. 클러스터 수준에서 경로 찾기
        auto cluster_path = FindClusterPath(start_cluster_x, start_cluster_y, 
                                          goal_cluster_x, goal_cluster_y);
        
        if (cluster_path.empty()) {
            std::cout << "❌ 클러스터 경로를 찾을 수 없음" << std::endl;
            return {};
        }
        
        // 3. 세부 경로 구성
        std::vector<GridMap::Node> detailed_path;
        
        for (size_t i = 0; i < cluster_path.size(); ++i) {
            auto& cluster = cluster_path[i];
            
            if (i == 0) {
                // 시작 클러스터: 시작점에서 진입점까지
                if (!cluster.entrances.empty()) {
                    auto entrance = FindNearestEntrance(cluster, start_x, start_y);
                    auto sub_path = FindPathDirect(start_x, start_y, entrance.x, entrance.y);
                    detailed_path.insert(detailed_path.end(), sub_path.begin(), sub_path.end());
                }
            } else if (i == cluster_path.size() - 1) {
                // 마지막 클러스터: 진입점에서 목표점까지
                if (!cluster.entrances.empty()) {
                    auto entrance = FindNearestEntrance(cluster, goal_x, goal_y);
                    auto sub_path = FindPathDirect(entrance.x, entrance.y, goal_x, goal_y);
                    detailed_path.insert(detailed_path.end(), sub_path.begin(), sub_path.end());
                }
            } else {
                // 중간 클러스터: 진입점에서 진입점까지
                // 이 부분은 더 복잡한 로직이 필요하지만 여기서는 단순화
            }
        }
        
        return detailed_path;
    }
    
    std::vector<Cluster> FindClusterPath(int start_cx, int start_cy, int goal_cx, int goal_cy) {
        // 클러스터 수준에서 간단한 경로 찾기 (BFS)
        std::queue<std::pair<int, int>> queue;
        std::unordered_map<std::string, std::pair<int, int>> parent;
        std::unordered_set<std::string> visited;
        
        queue.push({start_cx, start_cy});
        visited.insert(std::to_string(start_cx) + "," + std::to_string(start_cy));
        
        while (!queue.empty()) {
            auto [cx, cy] = queue.front();
            queue.pop();
            
            if (cx == goal_cx && cy == goal_cy) {
                // 경로 재구성
                std::vector<Cluster> path;
                int curr_x = goal_cx, curr_y = goal_cy;
                
                while (curr_x != start_cx || curr_y != start_cy) {
                    if (curr_y < static_cast<int>(clusters.size()) && 
                        curr_x < static_cast<int>(clusters[curr_y].size())) {
                        path.push_back(clusters[curr_y][curr_x]);
                    }
                    
                    auto key = std::to_string(curr_x) + "," + std::to_string(curr_y);
                    auto parent_it = parent.find(key);
                    if (parent_it != parent.end()) {
                        auto [px, py] = parent_it->second;
                        curr_x = px;
                        curr_y = py;
                    } else {
                        break;
                    }
                }
                
                if (start_cy < static_cast<int>(clusters.size()) && 
                    start_cx < static_cast<int>(clusters[start_cy].size())) {
                    path.push_back(clusters[start_cy][start_cx]);
                }
                
                std::reverse(path.begin(), path.end());
                return path;
            }
            
            // 인접 클러스터 탐색
            for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                    if (dx == 0 && dy == 0) continue;
                    
                    int nx = cx + dx;
                    int ny = cy + dy;
                    
                    std::string key = std::to_string(nx) + "," + std::to_string(ny);
                    
                    if (ny >= 0 && ny < static_cast<int>(clusters.size()) &&
                        nx >= 0 && nx < static_cast<int>(clusters[ny].size()) &&
                        clusters[ny][nx].walkable &&
                        visited.find(key) == visited.end()) {
                        
                        visited.insert(key);
                        parent[key] = {cx, cy};
                        queue.push({nx, ny});
                    }
                }
            }
        }
        
        return {};  // 경로 없음
    }
    
    GridMap::Node FindNearestEntrance(const Cluster& cluster, int target_x, int target_y) {
        if (cluster.entrances.empty()) {
            return GridMap::Node(target_x, target_y);
        }
        
        float min_distance = 999999.0f;
        GridMap::Node nearest = cluster.entrances[0];
        
        for (const auto& entrance : cluster.entrances) {
            float distance = std::sqrt((entrance.x - target_x) * (entrance.x - target_x) +
                                     (entrance.y - target_y) * (entrance.y - target_y));
            if (distance < min_distance) {
                min_distance = distance;
                nearest = entrance;
            }
        }
        
        return nearest;
    }
};
```

**💡 A* 최적화 기법들:**
- **계층적 경로 찾기**: 큰 맵을 클러스터로 나누어 처리
- **경로 캐싱**: 자주 사용되는 경로를 메모리에 저장
- **점진적 경로 찾기**: 한 번에 전체 경로가 아닌 부분적으로 계산
- **동적 가중치**: 상황에 따라 휴리스틱 가중치 조절

---

## 🎮 3. 몬스터 AI 설계 패턴

### **3.1 적응형 AI - 플레이어 행동 학습**

```cpp
#include <map>
#include <algorithm>
#include <numeric>

// 🧠 플레이어 행동 분석기
class PlayerBehaviorAnalyzer {
private:
    struct BehaviorPattern {
        std::string action;
        float frequency = 0.0f;
        float average_timing = 0.0f;
        float success_rate = 0.0f;
    };
    
    std::map<std::string, BehaviorPattern> patterns;
    std::vector<std::pair<std::string, float>> action_history;  // 액션, 시간
    
    float learning_rate = 0.1f;
    
public:
    void RecordPlayerAction(const std::string& action, float timestamp, bool success) {
        action_history.push_back({action, timestamp});
        
        // 최근 100개 액션만 유지
        if (action_history.size() > 100) {
            action_history.erase(action_history.begin());
        }
        
        // 패턴 업데이트
        UpdatePattern(action, timestamp, success);
    }
    
    std::string PredictNextAction() {
        if (patterns.empty()) return "unknown";
        
        // 가장 빈번한 액션 예측
        std::string most_frequent_action;
        float max_frequency = 0.0f;
        
        for (const auto& pair : patterns) {
            if (pair.second.frequency > max_frequency) {
                max_frequency = pair.second.frequency;
                most_frequent_action = pair.first;
            }
        }
        
        return most_frequent_action;
    }
    
    float GetActionPredictability() {
        if (patterns.empty()) return 0.0f;
        
        // 엔트로피 계산으로 예측 가능성 측정
        float total_frequency = 0.0f;
        for (const auto& pair : patterns) {
            total_frequency += pair.second.frequency;
        }
        
        if (total_frequency == 0) return 0.0f;
        
        float entropy = 0.0f;
        for (const auto& pair : patterns) {
            float probability = pair.second.frequency / total_frequency;
            if (probability > 0) {
                entropy -= probability * std::log2(probability);
            }
        }
        
        // 정규화 (0: 완전히 예측 가능, 1: 완전히 무작위)
        float max_entropy = std::log2(static_cast<float>(patterns.size()));
        return max_entropy > 0 ? entropy / max_entropy : 0.0f;
    }
    
    void PrintAnalysis() {
        std::cout << "📊 플레이어 행동 분석:" << std::endl;
        std::cout << "예측 가능성: " << (1.0f - GetActionPredictability()) * 100 << "%" << std::endl;
        std::cout << "예상 다음 액션: " << PredictNextAction() << std::endl;
        
        std::cout << "행동 패턴:" << std::endl;
        for (const auto& pair : patterns) {
            std::cout << "  " << pair.first 
                      << " - 빈도: " << pair.second.frequency
                      << ", 성공률: " << pair.second.success_rate * 100 << "%"
                      << std::endl;
        }
    }

private:
    void UpdatePattern(const std::string& action, float timestamp, bool success) {
        auto& pattern = patterns[action];
        
        // 빈도 업데이트 (지수 이동 평균)
        pattern.frequency = pattern.frequency * (1.0f - learning_rate) + learning_rate;
        
        // 성공률 업데이트
        pattern.success_rate = pattern.success_rate * (1.0f - learning_rate) + 
                              (success ? learning_rate : 0.0f);
        
        // 타이밍 업데이트
        if (!action_history.empty() && action_history.size() > 1) {
            float time_since_last = timestamp - action_history[action_history.size() - 2].second;
            pattern.average_timing = pattern.average_timing * (1.0f - learning_rate) + 
                                   time_since_last * learning_rate;
        }
    }
};

// 🎯 적응형 몬스터 AI
class AdaptiveMonsterAI {
private:
    PlayerBehaviorAnalyzer behavior_analyzer;
    
    // AI 전략들
    enum class Strategy {
        AGGRESSIVE,   // 공격적
        DEFENSIVE,    // 방어적  
        UNPREDICTABLE,// 예측 불가능
        COUNTER       // 카운터 전략
    };
    
    Strategy current_strategy = Strategy::AGGRESSIVE;
    float strategy_confidence = 0.5f;
    
    // 몬스터 상태
    Vector3 position{0, 0, 0};
    float health = 100.0f;
    float max_health = 100.0f;
    float aggression_level = 0.5f;  // 0.0 = 소극적, 1.0 = 매우 공격적
    
public:
    void Update(float delta_time) {
        // 1. 플레이어 행동 분석
        AnalyzePlayerBehavior();
        
        // 2. 전략 적응
        AdaptStrategy();
        
        // 3. 전략에 따른 행동 실행
        ExecuteStrategy(delta_time);
        
        // 4. 학습 결과 출력 (주기적으로)
        static float last_analysis_time = 0.0f;
        float current_time = GetCurrentTime();
        if (current_time - last_analysis_time > 5.0f) {
            behavior_analyzer.PrintAnalysis();
            PrintStrategyInfo();
            last_analysis_time = current_time;
        }
    }
    
    void OnPlayerAction(const std::string& action, bool action_successful) {
        behavior_analyzer.RecordPlayerAction(action, GetCurrentTime(), action_successful);
        
        // 즉시 반응 (리액티브 AI)
        ReactToPlayerAction(action, action_successful);
    }

private:
    void AnalyzePlayerBehavior() {
        float predictability = 1.0f - behavior_analyzer.GetActionPredictability();
        std::string next_action = behavior_analyzer.PredictNextAction();
        
        // 예측 가능성에 따라 전략 조정
        if (predictability > 0.7f) {
            // 플레이어가 예측 가능하면 카운터 전략 사용
            if (current_strategy != Strategy::COUNTER) {
                std::cout << "🎯 플레이어 패턴 파악! 카운터 전략으로 전환" << std::endl;
            }
            current_strategy = Strategy::COUNTER;
            strategy_confidence = predictability;
        } else if (predictability < 0.3f) {
            // 플레이어가 예측 불가능하면 마찬가지로 예측 불가능하게
            if (current_strategy != Strategy::UNPREDICTABLE) {
                std::cout << "🎲 플레이어가 변칙적! 예측 불가 전략으로 대응" << std::endl;
            }
            current_strategy = Strategy::UNPREDICTABLE;
            strategy_confidence = 1.0f - predictability;
        }
    }
    
    void AdaptStrategy() {
        // 체력에 따른 전략 조정
        float health_ratio = health / max_health;
        
        if (health_ratio < 0.3f && current_strategy != Strategy::DEFENSIVE) {
            std::cout << "🛡️ 체력 부족! 방어 전략으로 전환" << std::endl;
            current_strategy = Strategy::DEFENSIVE;
            aggression_level = 0.2f;
        } else if (health_ratio > 0.8f && current_strategy == Strategy::DEFENSIVE) {
            std::cout << "⚔️ 체력 회복! 공격 전략으로 복귀" << std::endl;
            current_strategy = Strategy::AGGRESSIVE;
            aggression_level = 0.8f;
        }
        
        // 전략 신뢰도에 따른 공격성 조절
        aggression_level = aggression_level * 0.9f + strategy_confidence * 0.1f;
    }
    
    void ExecuteStrategy(float delta_time) {
        switch (current_strategy) {
            case Strategy::AGGRESSIVE:
                ExecuteAggressiveStrategy(delta_time);
                break;
            case Strategy::DEFENSIVE:
                ExecuteDefensiveStrategy(delta_time);
                break;
            case Strategy::UNPREDICTABLE:
                ExecuteUnpredictableStrategy(delta_time);
                break;
            case Strategy::COUNTER:
                ExecuteCounterStrategy(delta_time);
                break;
        }
    }
    
    void ExecuteAggressiveStrategy(float delta_time) {
        // 적극적으로 플레이어에게 접근
        Vector3 player_pos = GetPlayerPosition();
        Vector3 direction = player_pos - position;
        float distance = direction.Length();
        
        if (distance > 30.0f) {
            direction.Normalize();
            position += direction * 80.0f * delta_time;  // 빠른 접근
            std::cout << "💨 빠르게 접근 중..." << std::endl;
        } else {
            std::cout << "⚔️ 연속 공격!" << std::endl;
        }
    }
    
    void ExecuteDefensiveStrategy(float delta_time) {
        // 거리 유지하며 기회를 노림
        Vector3 player_pos = GetPlayerPosition();
        Vector3 direction = position - player_pos;
        float distance = (player_pos - position).Length();
        
        if (distance < 60.0f) {
            direction.Normalize();
            position += direction * 40.0f * delta_time;  // 후퇴
            std::cout << "🛡️ 거리 유지하며 방어..." << std::endl;
        }
        
        // 체력 회복
        health += delta_time * 15.0f;
        if (health > max_health) health = max_health;
    }
    
    void ExecuteUnpredictableStrategy(float delta_time) {
        // 무작위적인 행동
        static float last_random_time = 0.0f;
        float current_time = GetCurrentTime();
        
        if (current_time - last_random_time > 2.0f) {
            int random_action = rand() % 4;
            
            switch (random_action) {
                case 0:
                    std::cout << "🎲 갑작스런 돌진!" << std::endl;
                    ExecuteAggressiveStrategy(delta_time * 2.0f);
                    break;
                case 1:
                    std::cout << "🎲 예상치 못한 후퇴!" << std::endl;
                    ExecuteDefensiveStrategy(delta_time);
                    break;
                case 2:
                    std::cout << "🎲 측면 기동!" << std::endl;
                    position += Vector3(rand() % 2 ? 50.0f : -50.0f, 0, 0) * delta_time;
                    break;
                case 3:
                    std::cout << "🎲 잠시 대기..." << std::endl;
                    break;
            }
            
            last_random_time = current_time;
        }
    }
    
    void ExecuteCounterStrategy(float delta_time) {
        std::string predicted_action = behavior_analyzer.PredictNextAction();
        
        // 예측된 플레이어 액션에 대한 카운터
        if (predicted_action == "attack") {
            std::cout << "🎯 공격 예측! 회피 준비!" << std::endl;
            // 회피 행동
            Vector3 player_pos = GetPlayerPosition();
            Vector3 perpendicular = Vector3(-(player_pos.z - position.z), 0, player_pos.x - position.x);
            perpendicular.Normalize();
            position += perpendicular * 60.0f * delta_time;
            
        } else if (predicted_action == "heal") {
            std::cout << "🎯 회복 예측! 방해 공격!" << std::endl;
            // 빠른 접근으로 회복 방해
            ExecuteAggressiveStrategy(delta_time * 1.5f);
            
        } else if (predicted_action == "retreat") {
            std::cout << "🎯 후퇴 예측! 추격!" << std::endl;
            // 후퇴 경로 차단
            Vector3 player_pos = GetPlayerPosition();
            Vector3 predicted_retreat = player_pos + (player_pos - position).Normalized() * 100.0f;
            Vector3 direction = predicted_retreat - position;
            direction.Normalize();
            position += direction * 70.0f * delta_time;
        }
    }
    
    void ReactToPlayerAction(const std::string& action, bool successful) {
        if (action == "critical_hit" && successful) {
            std::cout << "😱 크리티컬 히트! 즉시 후퇴!" << std::endl;
            current_strategy = Strategy::DEFENSIVE;
            aggression_level *= 0.7f;
            
        } else if (action == "heal" && successful) {
            std::cout << "😤 회복하는군! 방해한다!" << std::endl;
            aggression_level *= 1.3f;
            
        } else if (action == "miss" || !successful) {
            std::cout << "😏 실수했군! 기회다!" << std::endl;
            aggression_level *= 1.2f;
        }
    }
    
    Vector3 GetPlayerPosition() {
        // 시뮬레이션을 위한 가상 플레이어 위치
        static Vector3 player_pos{100, 0, 100};
        float time = GetCurrentTime();
        player_pos.x = 100 + sin(time * 0.3f) * 80;
        player_pos.z = 100 + cos(time * 0.2f) * 80;
        return player_pos;
    }
    
    float GetCurrentTime() {
        static auto start_time = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration<float>(now - start_time).count();
    }
    
    void PrintStrategyInfo() {
        std::cout << "🤖 AI 전략 정보:" << std::endl;
        std::cout << "현재 전략: ";
        
        switch (current_strategy) {
            case Strategy::AGGRESSIVE: std::cout << "공격적"; break;
            case Strategy::DEFENSIVE: std::cout << "방어적"; break;
            case Strategy::UNPREDICTABLE: std::cout << "예측불가"; break;
            case Strategy::COUNTER: std::cout << "카운터"; break;
        }
        
        std::cout << std::endl;
        std::cout << "전략 신뢰도: " << strategy_confidence * 100 << "%" << std::endl;
        std::cout << "공격성 수준: " << aggression_level * 100 << "%" << std::endl;
        std::cout << "현재 체력: " << health << "/" << max_health << std::endl;
    }

public:
    void TakeDamage(float damage) {
        health -= damage;
        if (health < 0) health = 0;
    }
    
    Vector3 GetPosition() const { return position; }
    float GetHealth() const { return health; }
};
```

**💡 적응형 AI의 핵심:**
- **행동 패턴 학습**: 플레이어의 습관과 전략 분석
- **동적 전략 변경**: 상황에 따른 유연한 대응
- **예측과 카운터**: 플레이어 행동 예측 및 대응책 실행
- **감정적 반응**: 성공/실패에 따른 즉시 반응

---

## 🐍 4. 스크립팅 시스템 (Lua 통합)

### **4.1 C++과 Lua 연동 기본**

```cpp
extern "C" {
    #include "lua.h"
    #include "lauxlib.h"
    #include "lualib.h"
}

#include <iostream>
#include <string>
#include <functional>

// 🐍 Lua 스크립트 엔진
class LuaScriptEngine {
private:
    lua_State* L;
    
public:
    LuaScriptEngine() {
        L = luaL_newstate();
        luaL_openlibs(L);  // 표준 라이브러리 로드
        
        // C++ 함수들을 Lua에 등록
        RegisterCppFunctions();
    }
    
    ~LuaScriptEngine() {
        lua_close(L);
    }
    
    bool ExecuteScript(const std::string& script) {
        int result = luaL_dostring(L, script.c_str());
        
        if (result != LUA_OK) {
            std::cerr << "❌ Lua 스크립트 오류: " << lua_tostring(L, -1) << std::endl;
            lua_pop(L, 1);
            return false;
        }
        
        return true;
    }
    
    bool LoadScriptFile(const std::string& filename) {
        int result = luaL_dofile(L, filename.c_str());
        
        if (result != LUA_OK) {
            std::cerr << "❌ Lua 파일 로드 오류: " << lua_tostring(L, -1) << std::endl;
            lua_pop(L, 1);
            return false;
        }
        
        return true;
    }
    
    // Lua 함수 호출
    template<typename... Args>
    bool CallLuaFunction(const std::string& function_name, Args... args) {
        lua_getglobal(L, function_name.c_str());
        
        if (!lua_isfunction(L, -1)) {
            std::cerr << "❌ Lua 함수를 찾을 수 없음: " << function_name << std::endl;
            lua_pop(L, 1);
            return false;
        }
        
        // 인자들을 스택에 푸시
        int arg_count = 0;
        PushArgs(arg_count, args...);
        
        // 함수 호출
        int result = lua_pcall(L, arg_count, 0, 0);
        
        if (result != LUA_OK) {
            std::cerr << "❌ Lua 함수 호출 오류: " << lua_tostring(L, -1) << std::endl;
            lua_pop(L, 1);
            return false;
        }
        
        return true;
    }
    
    // Lua 함수 호출하고 결과 받기
    template<typename ReturnType>
    ReturnType CallLuaFunctionWithReturn(const std::string& function_name) {
        lua_getglobal(L, function_name.c_str());
        
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 1);
            return ReturnType{};
        }
        
        int result = lua_pcall(L, 0, 1, 0);
        
        if (result != LUA_OK) {
            lua_pop(L, 1);
            return ReturnType{};
        }
        
        ReturnType ret = GetValue<ReturnType>(-1);
        lua_pop(L, 1);
        
        return ret;
    }
    
    // C++ 객체를 Lua에 등록
    void RegisterMonster(class ScriptableMonster* monster) {
        lua_pushlightuserdata(L, monster);
        lua_setglobal(L, "current_monster");
    }

private:
    void RegisterCppFunctions() {
        // 로그 출력 함수
        lua_register(L, "print_debug", [](lua_State* L) -> int {
            const char* message = luaL_checkstring(L, 1);
            std::cout << "🐍 [Lua] " << message << std::endl;
            return 0;
        });
        
        // 랜덤 함수
        lua_register(L, "random", [](lua_State* L) -> int {
            int min = luaL_checkinteger(L, 1);
            int max = luaL_checkinteger(L, 2);
            int result = min + rand() % (max - min + 1);
            lua_pushinteger(L, result);
            return 1;
        });
        
        // 거리 계산 함수
        lua_register(L, "distance", [](lua_State* L) -> int {
            double x1 = luaL_checknumber(L, 1);
            double y1 = luaL_checknumber(L, 2);
            double x2 = luaL_checknumber(L, 3);
            double y2 = luaL_checknumber(L, 4);
            
            double dx = x2 - x1;
            double dy = y2 - y1;
            double dist = std::sqrt(dx * dx + dy * dy);
            
            lua_pushnumber(L, dist);
            return 1;
        });
        
        // 몬스터 정보 접근 함수들
        lua_register(L, "get_health", [](lua_State* L) -> int {
            lua_getglobal(L, "current_monster");
            ScriptableMonster* monster = static_cast<ScriptableMonster*>(lua_touserdata(L, -1));
            if (monster) {
                lua_pushnumber(L, monster->GetHealth());
                return 1;
            }
            lua_pushnumber(L, 0);
            return 1;
        });
        
        lua_register(L, "get_position", [](lua_State* L) -> int {
            lua_getglobal(L, "current_monster");
            ScriptableMonster* monster = static_cast<ScriptableMonster*>(lua_touserdata(L, -1));
            if (monster) {
                auto pos = monster->GetPosition();
                lua_pushnumber(L, pos.x);
                lua_pushnumber(L, pos.z);
                return 2;
            }
            lua_pushnumber(L, 0);
            lua_pushnumber(L, 0);
            return 2;
        });
        
        lua_register(L, "move_to", [](lua_State* L) -> int {
            double x = luaL_checknumber(L, 1);
            double z = luaL_checknumber(L, 2);
            double speed = luaL_checknumber(L, 3);
            
            lua_getglobal(L, "current_monster");
            ScriptableMonster* monster = static_cast<ScriptableMonster*>(lua_touserdata(L, -1));
            if (monster) {
                monster->MoveTo(Vector3(static_cast<float>(x), 0, static_cast<float>(z)), static_cast<float>(speed));
            }
            return 0;
        });
        
        lua_register(L, "attack", [](lua_State* L) -> int {
            double damage = luaL_checknumber(L, 1);
            
            lua_getglobal(L, "current_monster");
            ScriptableMonster* monster = static_cast<ScriptableMonster*>(lua_touserdata(L, -1));
            if (monster) {
                monster->Attack(static_cast<float>(damage));
            }
            return 0;
        });
    }
    
    // 가변 인자 처리
    void PushArgs(int& count) {}
    
    template<typename T, typename... Args>
    void PushArgs(int& count, T&& first, Args&&... args) {
        PushValue(first);
        count++;
        PushArgs(count, args...);
    }
    
    void PushValue(int value) { lua_pushinteger(L, value); }
    void PushValue(float value) { lua_pushnumber(L, value); }
    void PushValue(double value) { lua_pushnumber(L, value); }
    void PushValue(const std::string& value) { lua_pushstring(L, value.c_str()); }
    void PushValue(const char* value) { lua_pushstring(L, value); }
    void PushValue(bool value) { lua_pushboolean(L, value); }
    
    template<typename T>
    T GetValue(int index);
};

// 특수화
template<>
int LuaScriptEngine::GetValue<int>(int index) {
    return static_cast<int>(lua_tointeger(L, index));
}

template<>
float LuaScriptEngine::GetValue<float>(int index) {
    return static_cast<float>(lua_tonumber(L, index));
}

template<>
std::string LuaScriptEngine::GetValue<std::string>(int index) {
    return std::string(lua_tostring(L, index));
}

template<>
bool LuaScriptEngine::GetValue<bool>(int index) {
    return lua_toboolean(L, index);
}
```

### **4.2 스크립트 기반 몬스터 AI**

```cpp
// 🎭 스크립트로 제어되는 몬스터
class ScriptableMonster {
private:
    LuaScriptEngine script_engine;
    Vector3 position{0, 0, 0};
    float health = 100.0f;
    float max_health = 100.0f;
    std::string monster_type;
    
    // 스크립트 파일들
    std::string ai_script_file;
    std::string behavior_script_file;
    
public:
    ScriptableMonster(const std::string& type, Vector3 spawn_pos) 
        : monster_type(type), position(spawn_pos) {
        
        // 이 몬스터를 Lua에 등록
        script_engine.RegisterMonster(this);
        
        // 몬스터 타입별 스크립트 로드
        LoadScriptsForType(type);
        
        // 초기화 스크립트 실행
        script_engine.CallLuaFunction("on_spawn", spawn_pos.x, spawn_pos.z);
    }
    
    void Update(float delta_time) {
        // Lua의 업데이트 함수 호출
        script_engine.CallLuaFunction("on_update", delta_time);
        
        // 스크립트가 변경되었는지 확인하고 핫 리로딩
        CheckAndReloadScripts();
    }
    
    void OnPlayerNearby(Vector3 player_pos) {
        script_engine.CallLuaFunction("on_player_nearby", player_pos.x, player_pos.z);
    }
    
    void OnTakeDamage(float damage, Vector3 attacker_pos) {
        health -= damage;
        if (health < 0) health = 0;
        
        script_engine.CallLuaFunction("on_take_damage", damage, attacker_pos.x, attacker_pos.z);
    }
    
    void OnPlayerAction(const std::string& action) {
        script_engine.CallLuaFunction("on_player_action", action);
    }

private:
    void LoadScriptsForType(const std::string& type) {
        // 몬스터 타입별 스크립트 파일 경로
        ai_script_file = "scripts/monsters/" + type + "_ai.lua";
        behavior_script_file = "scripts/monsters/" + type + "_behavior.lua";
        
        // 기본 AI 스크립트 생성 (파일이 없으면)
        CreateDefaultScript(type);
        
        // 스크립트 로드
        if (!script_engine.LoadScriptFile(ai_script_file)) {
            std::cerr << "❌ AI 스크립트 로드 실패: " << ai_script_file << std::endl;
        }
        
        if (!script_engine.LoadScriptFile(behavior_script_file)) {
            std::cerr << "❌ 행동 스크립트 로드 실패: " << behavior_script_file << std::endl;
        }
    }
    
    void CreateDefaultScript(const std::string& type) {
        // 고블린 AI 스크립트 예제
        if (type == "goblin") {
            std::string goblin_ai = R"(
-- 고블린 AI 스크립트
local goblin = {}

-- 상태 변수
goblin.state = "idle"
goblin.last_attack_time = 0
goblin.attack_cooldown = 2.0
goblin.chase_range = 80
goblin.attack_range = 30
goblin.health_threshold = 0.3

function on_spawn(x, z)
    print_debug("고블린이 (" .. x .. ", " .. z .. ")에 스폰되었습니다!")
    goblin.spawn_x = x
    goblin.spawn_z = z
    goblin.state = "idle"
end

function on_update(delta_time)
    local current_time = os.clock()
    local health = get_health()
    local pos_x, pos_z = get_position()
    
    -- 플레이어 위치 (시뮬레이션)
    local player_x = 100 + math.sin(current_time * 0.3) * 50
    local player_z = 100 + math.cos(current_time * 0.2) * 50
    
    local dist_to_player = distance(pos_x, pos_z, player_x, player_z)
    
    -- 상태 기계
    if goblin.state == "idle" then
        if dist_to_player <= goblin.chase_range then
            goblin.state = "chase"
            print_debug("플레이어 발견! 추격 시작!")
        else
            -- 가끔 주변 순찰
            if random(1, 100) <= 5 then  -- 5% 확률
                local patrol_x = goblin.spawn_x + random(-30, 30)
                local patrol_z = goblin.spawn_z + random(-30, 30)
                move_to(patrol_x, patrol_z, 20)
                print_debug("순찰 중...")
            end
        end
        
    elseif goblin.state == "chase" then
        if dist_to_player > goblin.chase_range * 1.5 then
            goblin.state = "idle"
            print_debug("추격 포기. 원래 위치로 돌아감.")
            move_to(goblin.spawn_x, goblin.spawn_z, 25)
        elseif dist_to_player <= goblin.attack_range then
            goblin.state = "attack"
            print_debug("공격 범위 진입!")
        else
            -- 플레이어를 향해 이동
            move_to(player_x, player_z, 50)
        end
        
    elseif goblin.state == "attack" then
        if dist_to_player > goblin.attack_range * 1.2 then
            goblin.state = "chase"
            print_debug("공격 범위를 벗어남. 다시 추격.")
        else
            -- 공격 실행
            if current_time - goblin.last_attack_time >= goblin.attack_cooldown then
                local damage = random(15, 25)
                attack(damage)
                goblin.last_attack_time = current_time
                print_debug("공격! 데미지: " .. damage)
                
                -- 공격 후 잠시 후퇴 (전술적 이동)
                if random(1, 100) <= 30 then  -- 30% 확률
                    local retreat_x = pos_x + random(-20, 20)
                    local retreat_z = pos_z + random(-20, 20)
                    move_to(retreat_x, retreat_z, 40)
                    print_debug("전술적 후퇴!")
                end
            end
        end
        
    elseif goblin.state == "retreat" then
        -- 체력이 낮을 때 후퇴
        if health / 100 > goblin.health_threshold then
            goblin.state = "idle"
            print_debug("체력 회복. 전투 재개 준비.")
        else
            -- 스폰 지점으로 도망
            move_to(goblin.spawn_x, goblin.spawn_z, 60)
            print_debug("스폰 지점으로 도망\!")
        end
    end
    
    -- 체력이 낮으면 후퇴
    if health / 100 <= goblin.health_threshold and goblin.state ~= "retreat" then
        goblin.state = "retreat"
        print_debug("체력이 낮음\! 후퇴\!")
    end
end

function on_take_damage(damage, attacker_x, attacker_z)
    local health = get_health()
    print_debug("피해를 입음\! 데미지: " .. damage .. ", 남은 체력: " .. health)
    
    -- 반격 결정
    if goblin.state == "idle" then
        goblin.state = "chase"
        print_debug("공격받음\! 반격\!")
    end
    
    -- 크리티컬 히트 반응
    if damage > 30 then
        say("크윽\! 아프다\!")
        -- 일시적으로 스턴
        set_stunned(0.5)
    end
end

function on_player_action(action)
    if action == "taunt" then
        say("감히 날 도발해?\!")
        goblin.state = "chase"
        -- 도발당하면 공격력 증가
        set_buff("enraged", 10)  -- 10초간 분노
    elseif action == "stealth" then
        if goblin.state == "chase" then
            goblin.state = "idle"
            say("어디 갔지...?")
        end
    end
end

return goblin
)";
            SaveScriptToFile("scripts/monsters/goblin_ai.lua", goblin_ai);
        }
    }
};

---

## 🔥 흔한 실수와 해결방법 (Common Mistakes & Solutions)

### 1. AI 업데이트 최적화 실패
```cpp
// [SEQUENCE: 1] ❌ 잘못된 예: 모든 AI를 매 프레임 업데이트
class BadAIManager {
    void Update(float delta_time) {
        for (auto& ai : all_npcs) {  // 1000개의 NPC
            ai->FullUpdate(delta_time);  // 복잡한 계산 매번 실행
        }
        // CPU 사용률 80%+\!
    }
};

// ✅ 올바른 예: LOD와 우선순위 기반 업데이트
class GoodAIManager {
    void Update(float delta_time) {
        // 플레이어 근처 AI만 자주 업데이트
        for (auto& ai : nearby_npcs) {
            ai->FullUpdate(delta_time);
        }
        
        // 멀리 있는 AI는 간단히 업데이트
        static int frame_counter = 0;
        if (frame_counter++ % 10 == 0) {  // 10프레임마다
            for (auto& ai : distant_npcs) {
                ai->SimpleUpdate(delta_time * 10);
            }
        }
    }
};
```

### 2. 경로 찾기 병목
```cpp
// [SEQUENCE: 2] ❌ 잘못된 예: 매번 전체 경로 재계산
class BadPathfinding {
    void MoveToTarget(Vector3 target) {
        // 매 프레임 A* 실행 - 너무 비쌈\!
        auto path = AStar::FindPath(position, target);
        FollowPath(path);
    }
};

// ✅ 올바른 예: 경로 캐싱과 부분 업데이트
class GoodPathfinding {
    std::vector<Vector3> cached_path;
    float path_update_timer = 0;
    
    void MoveToTarget(Vector3 target) {
        path_update_timer += delta_time;
        
        // 1초마다 경로 재계산
        if (path_update_timer > 1.0f || cached_path.empty()) {
            cached_path = AStar::FindPath(position, target);
            path_update_timer = 0;
        }
        
        // 동적 장애물 회피는 스티어링으로
        Vector3 avoidance = CalculateDynamicAvoidance();
        FollowPathWithAvoidance(cached_path, avoidance);
    }
};
```

### 3. 스크립트 메모리 누수
```cpp
// [SEQUENCE: 3] ❌ 잘못된 예: Lua 참조 관리 실패
class BadScriptManager {
    void CreateNPC(const std::string& script) {
        lua_State* L = luaL_newstate();
        luaL_dofile(L, script.c_str());
        // L을 닫지 않음 - 메모리 누수\!
    }
};

// ✅ 올바른 예: 적절한 리소스 관리
class GoodScriptManager {
    std::unique_ptr<lua_State, decltype(&lua_close)> L{luaL_newstate(), lua_close};
    
    void CreateNPC(const std::string& script) {
        // 스크립트 샌드박싱
        lua_newtable(L.get());  // 새 환경
        lua_setfenv(L.get(), -2);
        
        // 안전한 실행
        if (luaL_dofile(L.get(), script.c_str()) \!= 0) {
            HandleScriptError();
        }
        
        // 주기적 가비지 컬렉션
        lua_gc(L.get(), LUA_GCCOLLECT, 0);
    }
};
```

---

## 🚀 실습 프로젝트 (Practice Projects)

### 📚 기초 프로젝트: 간단한 몬스터 AI 시스템
**목표**: FSM 기반 몬스터 AI 구현

```cpp
// 구현 요구사항:
// 1. 3가지 몬스터 타입 (고블린, 늑대, 오크)
// 2. 기본 상태 머신 (순찰, 추격, 공격, 도망)
// 3. 시야 시스템
// 4. 기본 전투 AI
// 5. 그룹 행동 (늑대 무리)
// 6. 난이도 조절
```

### 🎮 중급 프로젝트: 행동 트리 기반 보스 AI
**목표**: 복잡한 보스 전투 시스템

```cpp
// 핵심 기능:
// 1. 다단계 보스 페이즈
// 2. 패턴 학습 방지
// 3. 플레이어 행동 분석
// 4. 동적 난이도 조절
// 5. 특수 스킬 콤보
// 6. 환경 상호작용
```

### 🏆 고급 프로젝트: 오픈월드 NPC 생태계
**목표**: 살아있는 게임 세계 구현

```cpp
// 구현 내용:
// 1. 일일 루틴 시스템
// 2. NPC 간 관계 시뮬레이션
// 3. 동적 퀘스트 생성
// 4. 경제 시뮬레이션
// 5. 날씨/시간 반응
// 6. 플레이어 평판 시스템
// 7. 이머전트 스토리텔링
```

---

## 📊 학습 체크리스트 (Learning Checklist)

### 🥉 브론즈 레벨
- [ ] FSM 구현 능력
- [ ] 기본 AI 패턴 이해
- [ ] 간단한 경로 찾기
- [ ] 기초 스크립팅

### 🥈 실버 레벨
- [ ] 행동 트리 설계
- [ ] A* 알고리즘 구현
- [ ] Lua 바인딩
- [ ] AI 디버깅 도구

### 🥇 골드 레벨
- [ ] GOAP 시스템
- [ ] 군집 AI
- [ ] 고급 스티어링
- [ ] 머신러닝 통합

### 💎 플래티넘 레벨
- [ ] 이머전트 행동
- [ ] 대규모 시뮬레이션
- [ ] AI 디렉터 시스템
- [ ] 프로시저럴 AI

---

## 📚 추가 학습 자료 (Additional Resources)

### 필독서
- 📖 "AI for Games" - Ian Millington
- 📖 "Programming Game AI By Example" - Mat Buckland
- 📖 "Behavioral Mathematics for Game AI" - Dave Mark

### GDC 강연
- 🎥 "Building a Better Centaur: AI in Halo"
- 🎥 "AI in The Last of Us"
- 🎥 "Horizon Zero Dawn: The AI of the Machines"
- 🎥 "The AI Systems of Left 4 Dead"

### 오픈소스 프로젝트
- 🔧 BehaviorTree.CPP: C++ 행동 트리 라이브러리
- 🔧 Recast/Detour: 네비게이션 메쉬
- 🔧 GOAP: Goal Oriented Action Planning
- 🔧 Unity ML-Agents: 강화학습 AI

### AI 프레임워크
- 🤖 TensorFlow C++ API
- 🤖 PyTorch C++ Frontend
- 🤖 ONNX Runtime
- 🤖 OpenAI Gym (강화학습)
