#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>
#include <string>
#include <optional>

namespace mmorpg::game::ai {

// [SEQUENCE: 1018] AI behavior system
// NPC와 몬스터의 행동을 제어하는 AI 시스템

// [SEQUENCE: 1019] AI states
enum class AIState {
    IDLE,           // 대기
    PATROL,         // 순찰
    ALERT,          // 경계
    COMBAT,         // 전투
    FLEEING,        // 도주
    RETURNING,      // 귀환
    DEAD            // 사망
};

// [SEQUENCE: 1020] AI personality types
enum class AIPersonality {
    PASSIVE,        // 선공 안 함
    AGGRESSIVE,     // 시야 내 적 공격
    DEFENSIVE,      // 특정 지역 방어
    SUPPORT,        // 아군 지원
    COWARDLY,       // 낮은 체력 시 도주
    BERSERK         // 체력 낮을수록 공격적
};

// [SEQUENCE: 1021] Behavior tree node types
enum class BehaviorNodeType {
    SEQUENCE,       // 순차 실행 (하나라도 실패 시 중단)
    SELECTOR,       // 선택 실행 (하나라도 성공 시 중단)
    PARALLEL,       // 병렬 실행
    DECORATOR,      // 조건부 실행
    ACTION          // 실제 행동
};

// [SEQUENCE: 1022] Behavior node status
enum class BehaviorStatus {
    RUNNING,        // 실행 중
    SUCCESS,        // 성공
    FAILURE         // 실패
};

// [SEQUENCE: 1023] AI perception data
struct AIPerception {
    // Visible entities
    std::vector<uint64_t> visible_enemies;
    std::vector<uint64_t> visible_allies;
    std::vector<uint64_t> visible_neutrals;
    
    // Threat information
    uint64_t highest_threat_target = 0;
    float highest_threat_value = 0.0f;
    
    // Environmental awareness
    float distance_to_spawn = 0.0f;
    float distance_to_leader = 0.0f;
    bool is_in_combat_area = true;
    
    // Health status
    float health_percentage = 1.0f;
    float mana_percentage = 1.0f;
    uint32_t allies_nearby = 0;
    uint32_t enemies_nearby = 0;
};

// [SEQUENCE: 1024] AI memory
struct AIMemory {
    // Last known positions
    std::unordered_map<uint64_t, std::pair<float, float>> last_enemy_positions;
    
    // Combat history
    uint64_t last_attacker_id = 0;
    std::chrono::steady_clock::time_point last_combat_time;
    
    // Patrol data
    size_t current_patrol_point = 0;
    bool patrol_forward = true;
    
    // Behavior history
    std::vector<std::string> recent_actions;
    
    // Custom flags
    std::unordered_map<std::string, bool> flags;
    std::unordered_map<std::string, float> values;
};

// [SEQUENCE: 1025] Behavior tree node interface
class IBehaviorNode {
public:
    virtual ~IBehaviorNode() = default;
    
    // [SEQUENCE: 1026] Execute node
    virtual BehaviorStatus Execute(uint64_t entity_id, 
                                  const AIPerception& perception,
                                  AIMemory& memory,
                                  float delta_time) = 0;
    
    // [SEQUENCE: 1027] Reset node state
    virtual void Reset() = 0;
    
    // [SEQUENCE: 1028] Get node type
    virtual BehaviorNodeType GetType() const = 0;
};

// [SEQUENCE: 1029] Composite node (Sequence/Selector)
class CompositeNode : public IBehaviorNode {
public:
    CompositeNode(BehaviorNodeType type) : type_(type) {}
    
    // [SEQUENCE: 1030] Add child node
    void AddChild(std::shared_ptr<IBehaviorNode> child) {
        children_.push_back(child);
    }
    
    BehaviorStatus Execute(uint64_t entity_id, 
                          const AIPerception& perception,
                          AIMemory& memory,
                          float delta_time) override;
    
    void Reset() override;
    BehaviorNodeType GetType() const override { return type_; }
    
protected:
    BehaviorNodeType type_;
    std::vector<std::shared_ptr<IBehaviorNode>> children_;
    size_t current_child_ = 0;
};

// [SEQUENCE: 1031] Decorator node
class DecoratorNode : public IBehaviorNode {
public:
    DecoratorNode(std::shared_ptr<IBehaviorNode> child,
                  std::function<bool(const AIPerception&, const AIMemory&)> condition)
        : child_(child), condition_(condition) {}
    
    BehaviorStatus Execute(uint64_t entity_id,
                          const AIPerception& perception,
                          AIMemory& memory,
                          float delta_time) override;
    
    void Reset() override { if (child_) child_->Reset(); }
    BehaviorNodeType GetType() const override { return BehaviorNodeType::DECORATOR; }
    
private:
    std::shared_ptr<IBehaviorNode> child_;
    std::function<bool(const AIPerception&, const AIMemory&)> condition_;
};

// [SEQUENCE: 1032] Action node base
class ActionNode : public IBehaviorNode {
public:
    BehaviorNodeType GetType() const override { return BehaviorNodeType::ACTION; }
    void Reset() override {}
};

// [SEQUENCE: 1033] Common action nodes
class AttackTargetAction : public ActionNode {
public:
    BehaviorStatus Execute(uint64_t entity_id,
                          const AIPerception& perception,
                          AIMemory& memory,
                          float delta_time) override;
};

class MoveToTargetAction : public ActionNode {
public:
    MoveToTargetAction(float acceptable_distance = 5.0f) 
        : acceptable_distance_(acceptable_distance) {}
    
    BehaviorStatus Execute(uint64_t entity_id,
                          const AIPerception& perception,
                          AIMemory& memory,
                          float delta_time) override;
                          
private:
    float acceptable_distance_;
};

class FleeAction : public ActionNode {
public:
    FleeAction(float flee_distance = 20.0f) : flee_distance_(flee_distance) {}
    
    BehaviorStatus Execute(uint64_t entity_id,
                          const AIPerception& perception,
                          AIMemory& memory,
                          float delta_time) override;
                          
private:
    float flee_distance_;
};

class PatrolAction : public ActionNode {
public:
    PatrolAction(const std::vector<std::pair<float, float>>& patrol_points)
        : patrol_points_(patrol_points) {}
    
    BehaviorStatus Execute(uint64_t entity_id,
                          const AIPerception& perception,
                          AIMemory& memory,
                          float delta_time) override;
                          
private:
    std::vector<std::pair<float, float>> patrol_points_;
};

class UseSkillAction : public ActionNode {
public:
    UseSkillAction(uint32_t skill_id) : skill_id_(skill_id) {}
    
    BehaviorStatus Execute(uint64_t entity_id,
                          const AIPerception& perception,
                          AIMemory& memory,
                          float delta_time) override;
                          
private:
    uint32_t skill_id_;
};

// [SEQUENCE: 1034] AI controller
class AIController {
public:
    AIController(uint64_t entity_id, AIPersonality personality)
        : entity_id_(entity_id), personality_(personality) {}
    
    // [SEQUENCE: 1035] Set behavior tree
    void SetBehaviorTree(std::shared_ptr<IBehaviorNode> root) {
        behavior_tree_ = root;
    }
    
    // [SEQUENCE: 1036] Update AI
    void Update(float delta_time);
    
    // [SEQUENCE: 1037] State management
    AIState GetState() const { return current_state_; }
    void SetState(AIState state);
    
    // [SEQUENCE: 1038] Perception update
    void UpdatePerception();
    
    // [SEQUENCE: 1039] Event handlers
    void OnDamaged(uint64_t attacker_id, float damage);
    void OnAllyDamaged(uint64_t ally_id, uint64_t attacker_id);
    void OnTargetDied(uint64_t target_id);
    void OnSpellCast(uint64_t caster_id, uint32_t spell_id);
    
    // [SEQUENCE: 1040] Memory access
    AIMemory& GetMemory() { return memory_; }
    const AIPerception& GetPerception() const { return perception_; }
    
    // [SEQUENCE: 1041] Configuration
    void SetAggroRange(float range) { aggro_range_ = range; }
    void SetLeashRange(float range) { leash_range_ = range; }
    void SetRespawnPosition(float x, float y, float z);
    
private:
    uint64_t entity_id_;
    AIPersonality personality_;
    AIState current_state_ = AIState::IDLE;
    
    // Behavior tree
    std::shared_ptr<IBehaviorNode> behavior_tree_;
    
    // Perception and memory
    AIPerception perception_;
    AIMemory memory_;
    
    // Configuration
    float aggro_range_ = 15.0f;
    float leash_range_ = 30.0f;
    float spawn_x_ = 0, spawn_y_ = 0, spawn_z_ = 0;
    
    // Update timers
    float perception_update_timer_ = 0.0f;
    float behavior_update_timer_ = 0.0f;
    
    // Constants
    static constexpr float PERCEPTION_UPDATE_INTERVAL = 0.2f;
    static constexpr float BEHAVIOR_UPDATE_INTERVAL = 0.1f;
};

// [SEQUENCE: 1042] AI manager
class AIManager {
public:
    static AIManager& Instance() {
        static AIManager instance;
        return instance;
    }
    
    // [SEQUENCE: 1043] Create AI controller
    std::shared_ptr<AIController> CreateAI(uint64_t entity_id, 
                                          AIPersonality personality);
    
    // [SEQUENCE: 1044] Remove AI
    void RemoveAI(uint64_t entity_id);
    
    // [SEQUENCE: 1045] Get AI controller
    std::shared_ptr<AIController> GetAI(uint64_t entity_id) const;
    
    // [SEQUENCE: 1046] Update all AIs
    void Update(float delta_time);
    
    // [SEQUENCE: 1047] Global AI events
    void NotifyDamage(uint64_t victim_id, uint64_t attacker_id, float damage);
    void NotifyDeath(uint64_t entity_id);
    void NotifySpellCast(uint64_t caster_id, uint32_t spell_id, float x, float y);
    
    // [SEQUENCE: 1048] AI templates
    void RegisterAITemplate(const std::string& template_name,
                           std::function<std::shared_ptr<IBehaviorNode>()> factory);
    std::shared_ptr<IBehaviorNode> CreateFromTemplate(const std::string& template_name);
    
private:
    AIManager() = default;
    
    // Active AI controllers
    std::unordered_map<uint64_t, std::shared_ptr<AIController>> ai_controllers_;
    
    // AI templates
    std::unordered_map<std::string, std::function<std::shared_ptr<IBehaviorNode>()>> ai_templates_;
};

// [SEQUENCE: 1049] Behavior tree builder
class BehaviorTreeBuilder {
public:
    // [SEQUENCE: 1050] Create common AI behaviors
    static std::shared_ptr<IBehaviorNode> CreateAggressiveMelee();
    static std::shared_ptr<IBehaviorNode> CreateDefensiveTank();
    static std::shared_ptr<IBehaviorNode> CreateRangedDPS();
    static std::shared_ptr<IBehaviorNode> CreateHealer();
    static std::shared_ptr<IBehaviorNode> CreatePatrolGuard();
    
    // [SEQUENCE: 1051] Condition helpers
    static std::function<bool(const AIPerception&, const AIMemory&)> 
        HasTarget();
    static std::function<bool(const AIPerception&, const AIMemory&)> 
        HealthBelow(float percentage);
    static std::function<bool(const AIPerception&, const AIMemory&)> 
        EnemiesNearby(uint32_t count);
    static std::function<bool(const AIPerception&, const AIMemory&)> 
        AlliesNearby(uint32_t count);
};

} // namespace mmorpg::game::ai