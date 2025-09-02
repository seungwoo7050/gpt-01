#pragma once

#include "core/ecs/system.h"
#include "game/components/health_component.h"
#include "game/components/combat_stats_component.h"
#include "game/components/skill_component.h"
#include "game/components/target_component.h"
#include "game/components/transform_component.h"
#include <memory>

namespace mmorpg::game::systems::combat {


// [SEQUENCE: MVP4-12] Traditional target-based combat system definition
class TargetedCombatSystem : public core::ecs::System {
public:
    TargetedCombatSystem() : System("TargetedCombatSystem") {}
    
    
    // [SEQUENCE: MVP4-13] System lifecycle and update logic
    void OnSystemInit() override;
    void OnSystemShutdown() override;
    
    
    void Update(float delta_time) override;
    
    
    core::ecs::SystemStage GetStage() const override {
        return core::ecs::SystemStage::UPDATE;
    }
    int GetPriority() const override { return 100; }
    
    
    // [SEQUENCE: MVP4-14] Combat action and skill usage methods
    bool SetTarget(core::ecs::EntityId attacker, core::ecs::EntityId target);
    bool ClearTarget(core::ecs::EntityId attacker);
    bool StartAutoAttack(core::ecs::EntityId attacker);
    bool StopAutoAttack(core::ecs::EntityId attacker);
    
    
    bool UseSkill(core::ecs::EntityId caster, uint32_t skill_id);
    bool CancelCast(core::ecs::EntityId caster);
    
    
    
    bool ValidateTarget(core::ecs::EntityId attacker, core::ecs::EntityId target);
    bool IsInRange(core::ecs::EntityId attacker, core::ecs::EntityId target, float range);
    bool HasLineOfSight(core::ecs::EntityId attacker, core::ecs::EntityId target);
    
private:
    
    float CalculateDamage(const CombatStatsComponent& attacker_stats,
                         const CombatStatsComponent& defender_stats,
                         float base_damage, bool is_physical);
    
    bool ApplyDamage(core::ecs::EntityId target, float damage);
    bool ApplyHealing(core::ecs::EntityId target, float healing);
    
    
    void ProcessAutoAttacks(float delta_time, core::ecs::EntityId entity);
    void ExecuteAutoAttack(core::ecs::EntityId attacker, core::ecs::EntityId target);
    
    
    void ProcessSkillCasts(float delta_time, core::ecs::EntityId entity);
    void ProcessSkillCooldowns(float delta_time);
    void ExecuteSkill(core::ecs::EntityId caster, const Skill& skill);
    
    
    void UpdateTargetValidation(float delta_time);
    void CleanupInvalidTargets();
    
    
    void OnCombatStart(core::ecs::EntityId entity);
    void OnCombatEnd(core::ecs::EntityId entity);
    void OnDeath(core::ecs::EntityId entity);
    
    
    class GridSpatialSystem* spatial_system_ = nullptr;
    
    
    struct CombatConfig {
        float target_validation_interval = 0.5f;  // Check targets every 0.5s
        float max_combat_range = 50.0f;          // Maximum engagement range
        float combat_timeout = 5.0f;             // Exit combat after 5s
        float armor_reduction_factor = 0.01f;    // 1 armor = 1% reduction
        float level_difference_factor = 0.05f;   // 5% per level difference
    } config_;
    
    
    std::unordered_set<core::ecs::EntityId> entities_in_combat_;
    std::unordered_map<core::ecs::EntityId, std::chrono::steady_clock::time_point> last_combat_time_;
};

} // namespace mmorpg::game::systems::combat
time_;
};

} // namespace mmorpg::game::systems::combat
