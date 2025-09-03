#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>
#include <chrono>
#include <optional>
#include "../combat/combat_system.h"

namespace mmorpg::game::skills {

// [SEQUENCE: 886] Skill system base
// 다양한 스킬 타입과 효과를 지원하는 확장 가능한 시스템

// [SEQUENCE: 887] Skill types
enum class SkillType {
    INSTANT,      // 즉시 시전
    CAST_TIME,    // 시전 시간 필요
    CHANNELING,   // 채널링 (지속 시전)
    TOGGLE,       // 토글 온/오프
    PASSIVE       // 패시브 (자동 적용)
};

// [SEQUENCE: 888] Resource types
enum class ResourceType {
    MANA,         // 마나
    ENERGY,       // 기력
    RAGE,         // 분노
    FOCUS,        // 집중력
    COMBO_POINTS, // 콤보 포인트
    HEALTH        // 체력 (희생 스킬용)
};

// [SEQUENCE: 889] Skill target requirements
enum class SkillTargetRequirement {
    NO_TARGET,           // 타겟 불필요
    REQUIRES_ENEMY,      // 적 타겟 필요
    REQUIRES_ALLY,       // 아군 타겟 필요
    REQUIRES_GROUND,     // 지면 지정 필요
    REQUIRES_SELF        // 자신만 가능
};

// [SEQUENCE: 890] Skill interrupt flags
namespace SkillInterruptFlags {
    constexpr uint32_t MOVEMENT = 1 << 0;      // 이동 시 중단
    constexpr uint32_t DAMAGE = 1 << 1;        // 피해 시 중단
    constexpr uint32_t STUN = 1 << 2;          // 기절 시 중단
    constexpr uint32_t SILENCE = 1 << 3;       // 침묵 시 중단
    constexpr uint32_t MANUAL = 1 << 4;        // 수동 중단 가능
}

// [SEQUENCE: 891] Skill data definition
struct SkillData {
    uint32_t skill_id = 0;
    std::string name;
    std::string description;
    
    // Basic properties
    SkillType type = SkillType::INSTANT;
    uint32_t level_required = 1;
    uint32_t max_rank = 5;
    
    // Targeting
    SkillTargetRequirement target_requirement = SkillTargetRequirement::NO_TARGET;
    combat::TargetType target_type = combat::TargetType::SINGLE_ENEMY;
    float range = 0.0f;
    float radius = 0.0f;  // For AoE skills
    
    // Resource cost
    ResourceType resource_type = ResourceType::MANA;
    float resource_cost = 0.0f;
    float resource_cost_per_second = 0.0f;  // For channeling/toggle
    
    // Timing
    float cast_time = 0.0f;           // Seconds
    float channel_duration = 0.0f;     // For channeling skills
    float cooldown = 0.0f;             // Seconds
    float global_cooldown = 1.0f;      // Seconds
    
    // Damage/Healing
    float base_damage = 0.0f;
    float damage_per_rank = 0.0f;
    combat::DamageType damage_type = combat::DamageType::PHYSICAL;
    float healing = 0.0f;
    
    // Scaling
    float attack_power_coefficient = 0.0f;
    float spell_power_coefficient = 0.0f;
    
    // Interrupt
    uint32_t interrupt_flags = 0;
    
    // Effects (will be expanded with buff/debuff system)
    std::vector<std::string> apply_effects;
    
    // Animation/Visual (클라이언트용 힌트)
    std::string animation_name;
    std::string projectile_effect;
    std::string impact_effect;
};

// [SEQUENCE: 892] Skill instance (learned skill)
struct SkillInstance {
    uint32_t skill_id = 0;
    uint32_t current_rank = 1;
    
    // Cooldown tracking
    std::chrono::steady_clock::time_point last_used;
    std::chrono::steady_clock::time_point cooldown_end;
    
    // State
    bool is_on_cooldown = false;
    bool is_casting = false;
    bool is_channeling = false;
    bool is_toggled = false;
    
    // Cast/Channel progress
    float cast_progress = 0.0f;        // 0.0 to 1.0
    float channel_progress = 0.0f;     // 0.0 to 1.0
    uint64_t current_target = 0;
    float target_x = 0, target_y = 0, target_z = 0;
};

// [SEQUENCE: 893] Skill cast result
struct SkillCastResult {
    bool success = false;
    std::string failure_reason;
    
    // Results
    std::vector<combat::DamageInfo> damage_results;
    std::vector<uint64_t> affected_targets;
    float resource_consumed = 0.0f;
};

// [SEQUENCE: 894] Skill effect interface
class ISkillEffect {
public:
    virtual ~ISkillEffect() = default;
    
    // [SEQUENCE: 895] Apply immediate effect
    virtual void OnApply(uint64_t caster_id, uint64_t target_id, 
                        uint32_t skill_rank) = 0;
    
    // [SEQUENCE: 896] Update channeling effect
    virtual void OnChannelTick(uint64_t caster_id, uint64_t target_id,
                              float delta_time, uint32_t skill_rank) = 0;
    
    // [SEQUENCE: 897] Remove effect
    virtual void OnRemove(uint64_t caster_id, uint64_t target_id) = 0;
};

// [SEQUENCE: 898] Skill manager
class SkillManager {
public:
    static SkillManager& Instance() {
        static SkillManager instance;
        return instance;
    }
    
    // [SEQUENCE: 899] Skill data management
    void RegisterSkill(const SkillData& skill_data);
    const SkillData* GetSkillData(uint32_t skill_id) const;
    
    // [SEQUENCE: 900] Skill learning
    bool LearnSkill(uint64_t entity_id, uint32_t skill_id);
    bool UpgradeSkill(uint64_t entity_id, uint32_t skill_id);
    bool HasSkill(uint64_t entity_id, uint32_t skill_id) const;
    
    // [SEQUENCE: 901] Skill casting
    SkillCastResult StartCast(uint64_t caster_id, uint32_t skill_id,
                             uint64_t target_id = 0,
                             float target_x = 0, float target_y = 0, float target_z = 0);
    
    // [SEQUENCE: 902] Cancel casting
    void CancelCast(uint64_t caster_id);
    void InterruptCast(uint64_t caster_id, uint32_t interrupt_flags);
    
    // [SEQUENCE: 903] Toggle skills
    bool ToggleSkill(uint64_t caster_id, uint32_t skill_id);
    
    // [SEQUENCE: 904] Update system
    void Update(float delta_time);
    
    // [SEQUENCE: 905] Cooldown management
    bool IsOnCooldown(uint64_t entity_id, uint32_t skill_id) const;
    float GetCooldownRemaining(uint64_t entity_id, uint32_t skill_id) const;
    void ResetCooldown(uint64_t entity_id, uint32_t skill_id);
    void ResetAllCooldowns(uint64_t entity_id);
    
    // [SEQUENCE: 906] Skill state queries
    bool IsCasting(uint64_t entity_id) const;
    bool IsChanneling(uint64_t entity_id) const;
    std::optional<uint32_t> GetCastingSkill(uint64_t entity_id) const;
    float GetCastProgress(uint64_t entity_id) const;
    
    // [SEQUENCE: 907] Register skill effects
    void RegisterSkillEffect(uint32_t skill_id, std::shared_ptr<ISkillEffect> effect);
    
private:
    SkillManager() = default;
    
    // [SEQUENCE: 908] Skill database
    std::unordered_map<uint32_t, SkillData> skill_database_;
    
    // [SEQUENCE: 909] Learned skills per entity
    std::unordered_map<uint64_t, std::unordered_map<uint32_t, SkillInstance>> entity_skills_;
    
    // [SEQUENCE: 910] Active casts
    struct ActiveCast {
        uint32_t skill_id;
        uint64_t target_id;
        float target_x, target_y, target_z;
        std::chrono::steady_clock::time_point start_time;
        float cast_time;
        bool is_channeling;
        float channel_time_remaining;
    };
    std::unordered_map<uint64_t, ActiveCast> active_casts_;
    
    // [SEQUENCE: 911] Skill effects
    std::unordered_map<uint32_t, std::vector<std::shared_ptr<ISkillEffect>>> skill_effects_;
    
    // [SEQUENCE: 912] Toggle states
    std::unordered_map<uint64_t, std::vector<uint32_t>> active_toggles_;
    
    // [SEQUENCE: 913] Helper functions
    bool ValidateCast(uint64_t caster_id, const SkillData& skill, 
                     uint64_t target_id, SkillCastResult& result);
    bool CheckResourceCost(uint64_t caster_id, const SkillData& skill);
    void ConsumeResource(uint64_t caster_id, const SkillData& skill);
    void ExecuteSkill(uint64_t caster_id, const SkillData& skill,
                     const SkillInstance& instance, const ActiveCast& cast);
    void ApplyGlobalCooldown(uint64_t caster_id, float gcd_duration);
    void UpdateCastProgress(uint64_t caster_id, ActiveCast& cast, float delta_time);
    void UpdateChanneling(uint64_t caster_id, ActiveCast& cast, float delta_time);
    void UpdateToggles(float delta_time);
};

// [SEQUENCE: 914] Skill factory for common skill types
class SkillFactory {
public:
    // [SEQUENCE: 915] Create damage skill
    static SkillData CreateDamageSkill(
        uint32_t skill_id,
        const std::string& name,
        float base_damage,
        combat::DamageType damage_type,
        float range,
        float cooldown,
        float mana_cost
    );
    
    // [SEQUENCE: 916] Create healing skill
    static SkillData CreateHealingSkill(
        uint32_t skill_id,
        const std::string& name,
        float base_healing,
        float range,
        float cast_time,
        float mana_cost
    );
    
    // [SEQUENCE: 917] Create AoE skill
    static SkillData CreateAoESkill(
        uint32_t skill_id,
        const std::string& name,
        float base_damage,
        float radius,
        float cooldown,
        float mana_cost
    );
    
    // [SEQUENCE: 918] Create buff skill
    static SkillData CreateBuffSkill(
        uint32_t skill_id,
        const std::string& name,
        const std::vector<std::string>& effects,
        float duration,
        float cooldown,
        float mana_cost
    );
};

// [SEQUENCE: 919] Common skill effects
class DamageSkillEffect : public ISkillEffect {
public:
    DamageSkillEffect(float base_damage, float damage_per_rank, 
                     combat::DamageType damage_type)
        : base_damage_(base_damage), damage_per_rank_(damage_per_rank),
          damage_type_(damage_type) {}
    
    void OnApply(uint64_t caster_id, uint64_t target_id, uint32_t skill_rank) override;
    void OnChannelTick(uint64_t caster_id, uint64_t target_id, 
                      float delta_time, uint32_t skill_rank) override {}
    void OnRemove(uint64_t caster_id, uint64_t target_id) override {}
    
private:
    float base_damage_;
    float damage_per_rank_;
    combat::DamageType damage_type_;
};

// [SEQUENCE: 920] Skill utilities
namespace SkillUtils {
    // Calculate final skill damage including modifiers
    float CalculateSkillDamage(const SkillData& skill, uint32_t rank,
                              const combat::CombatStats& caster_stats);
    
    // Check if target is valid for skill
    bool IsValidSkillTarget(uint64_t caster_id, uint64_t target_id,
                           const SkillData& skill);
    
    // Get all entities affected by AoE skill
    std::vector<uint64_t> GetAoETargets(uint64_t caster_id,
                                       float center_x, float center_y, float center_z,
                                       float radius, combat::TargetType target_type);
}

} // namespace mmorpg::game::skills