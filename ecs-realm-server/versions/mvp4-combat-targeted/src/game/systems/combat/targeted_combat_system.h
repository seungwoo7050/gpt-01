#pragma once

#include "core/ecs/system.h"
#include "game/components/health_component.h"
#include "game/components/combat_stats_component.h"
#include "game/components/skill_component.h"
#include "game/components/target_component.h"
#include "game/components/transform_component.h"
#include <memory>

namespace mmorpg::game::systems::combat {

// [SEQUENCE: 1] Traditional target-based combat system
class TargetedCombatSystem : public core::ecs::System {
public:
    TargetedCombatSystem() : System("TargetedCombatSystem") {}
    
    // [SEQUENCE: 2] System lifecycle
    void OnSystemInit() override;
    void OnSystemShutdown() override;
    
    // [SEQUENCE: 3] Combat updates
    void Update(float delta_time) override;
    
    // [SEQUENCE: 4] System metadata
    core::ecs::SystemStage GetStage() const override {
        return core::ecs::SystemStage::UPDATE;
    }
    int GetPriority() const override { return 100; }
    
    // [SEQUENCE: 5] Combat actions
    bool SetTarget(core::ecs::EntityId attacker, core::ecs::EntityId target);
    bool ClearTarget(core::ecs::EntityId attacker);
    bool StartAutoAttack(core::ecs::EntityId attacker);
    bool StopAutoAttack(core::ecs::EntityId attacker);
    
    // [SEQUENCE: 6] Skill usage
    bool UseSkill(core::ecs::EntityId caster, uint32_t skill_id);
    bool CancelCast(core::ecs::EntityId caster);
    
    // [SEQUENCE: 7] Target validation
    bool ValidateTarget(core::ecs::EntityId attacker, core::ecs::EntityId target);
    bool IsInRange(core::ecs::EntityId attacker, core::ecs::EntityId target, float range);
    bool HasLineOfSight(core::ecs::EntityId attacker, core::ecs::EntityId target);
    
private:
    // [SEQUENCE: 8] Combat calculations
    float CalculateDamage(const CombatStatsComponent& attacker_stats,
                         const CombatStatsComponent& defender_stats,
                         float base_damage, bool is_physical);
    
    bool ApplyDamage(core::ecs::EntityId target, float damage);
    bool ApplyHealing(core::ecs::EntityId target, float healing);
    
    // [SEQUENCE: 9] Auto-attack processing
    void ProcessAutoAttacks(float delta_time);
    void ExecuteAutoAttack(core::ecs::EntityId attacker, core::ecs::EntityId target);
    
    // [SEQUENCE: 10] Skill processing
    void ProcessSkillCasts(float delta_time);
    void ProcessSkillCooldowns(float delta_time);
    void ExecuteSkill(core::ecs::EntityId caster, const Skill& skill);
    
    // [SEQUENCE: 11] Target management
    void UpdateTargetValidation(float delta_time);
    void CleanupInvalidTargets();
    
    // [SEQUENCE: 12] Combat events
    void OnCombatStart(core::ecs::EntityId entity);
    void OnCombatEnd(core::ecs::EntityId entity);
    void OnDeath(core::ecs::EntityId entity);
    
    // [SEQUENCE: 13] Spatial integration
    class SpatialIndexingSystem* spatial_system_ = nullptr;
    
    // [SEQUENCE: 14] Combat configuration
    struct CombatConfig {
        float target_validation_interval = 0.5f;  // Check targets every 0.5s
        float max_combat_range = 50.0f;          // Maximum engagement range
        float combat_timeout = 5.0f;             // Exit combat after 5s
        float armor_reduction_factor = 0.01f;    // 1 armor = 1% reduction
        float level_difference_factor = 0.05f;   // 5% per level difference
    } config_;
    
    // [SEQUENCE: 15] Combat state tracking
    std::unordered_set<core::ecs::EntityId> entities_in_combat_;
    std::unordered_map<core::ecs::EntityId, std::chrono::steady_clock::time_point> last_combat_time_;
};

} // namespace mmorpg::game::systems::combat