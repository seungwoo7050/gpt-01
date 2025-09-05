#pragma once

#include "core/ecs/optimized/system.h"
#include "game/components/health_component.h"
#include "game/components/combat_stats_component.h"
#include "game/components/skill_component.h"
#include "game/components/target_component.h"
#include "game/components/transform_component.h"
#include "game/systems/grid_spatial_system.h"
#include <memory>
#include <unordered_set>

namespace mmorpg::game::systems::combat {

// [SEQUENCE: MVP4-7] Defines the system for handling traditional, target-based combat mechanics.
class TargetedCombatSystem : public core::ecs::optimized::System {
public:
    TargetedCombatSystem() = default;
    ~TargetedCombatSystem() override;

    // [SEQUENCE: MVP4-8] Public API for managing combat state, skills, and targets.
    void Update(float delta_time) override;
    void SetSpatialSystem(mmorpg::game::systems::GridSpatialSystem* system) { spatial_system_ = system; }
    
    bool SetTarget(core::ecs::EntityId attacker, core::ecs::EntityId target);
    bool ClearTarget(core::ecs::EntityId attacker);
    bool StartAutoAttack(core::ecs::EntityId attacker);
    bool StopAutoAttack(core::ecs::EntityId attacker);
    
    bool UseSkill(core::ecs::EntityId caster, uint32_t skill_id);
    bool CancelCast(core::ecs::EntityId caster);
    
    bool ValidateTarget(core::ecs::EntityId attacker, core::ecs::EntityId target);
    bool IsInRange(core::ecs::EntityId attacker, core::ecs::EntityId target, float range);
    bool HasLineOfSight(core::ecs::EntityId attacker, core::ecs::EntityId target);
    bool IsInCombat(core::ecs::EntityId entity);
    bool ApplyHealing(core::ecs::EntityId target, float healing);

    void OnCombatStart(core::ecs::EntityId entity);
    void OnCombatEnd(core::ecs::EntityId entity);
    void OnDeath(core::ecs::EntityId entity);

private:
    // [SEQUENCE: MVP4-9] Private helper methods for internal combat logic.
    float CalculateDamage(const components::CombatStatsComponent& attacker_stats,
                         const components::CombatStatsComponent& defender_stats,
                         float base_damage, bool is_physical);
    
    bool ApplyDamage(core::ecs::EntityId target, float damage, core::ecs::EntityId attacker);
    
    void ProcessAutoAttacks(float delta_time);
    void ExecuteAutoAttack(core::ecs::EntityId attacker, core::ecs::EntityId target);
    
    void ProcessSkillCasts(float delta_time);
    void ProcessSkillCooldowns(float delta_time);
    void ExecuteSkill(core::ecs::EntityId caster, const components::Skill& skill);
    
    void UpdateTargetValidation(float delta_time);
    void CleanupInvalidTargets();

    // [SEQUENCE: MVP4-10] Private member variables for system state and configuration.
    class GridSpatialSystem* spatial_system_ = nullptr;
    
    struct CombatConfig {
        float target_validation_interval = 0.5f;
        float max_combat_range = 50.0f;
        float combat_timeout = 5.0f;
        float armor_reduction_factor = 0.01f;
        float level_difference_factor = 0.05f;
    } config_;
    
    std::unordered_set<core::ecs::EntityId> entities_in_combat_;
    std::unordered_map<core::ecs::EntityId, float> last_combat_time_;
    float total_time_ = 0.0f;
};

} // namespace mmorpg::game::systems::combat