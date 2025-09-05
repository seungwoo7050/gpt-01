#pragma once

#include "core/ecs/optimized/system.h"
#include "game/components/health_component.h"
#include "game/components/combat_stats_component.h"
#include "game/components/skill_component.h"
#include "game/components/transform_component.h"
#include "game/components/projectile_component.h"
#include "game/components/dodge_component.h"
#include "core/utils/vector3.h"
#include "game/systems/grid_spatial_system.h"
#include <memory>
#include <unordered_set>

namespace mmorpg::game::systems::combat {

// [SEQUENCE: MVP4-30] Defines the system for handling action-oriented, non-target-based combat.
class ActionCombatSystem : public core::ecs::optimized::System {
public:
    ActionCombatSystem();
    ~ActionCombatSystem() override;
    
    // [SEQUENCE: MVP4-31] Public API for managing action combat, skills, and movement.
    void Update(float delta_time) override;
    void SetSpatialSystem(mmorpg::game::systems::GridSpatialSystem* system) { spatial_system_ = system; }
    
    bool UseSkillshot(core::ecs::EntityId caster, uint32_t skill_id,
                     const core::utils::Vector3& direction);
    
    bool UseAreaSkill(core::ecs::EntityId caster, uint32_t skill_id,
                     const core::utils::Vector3& target_position);
    
    bool UseMeleeSwing(core::ecs::EntityId attacker,
                      const core::utils::Vector3& direction,
                      float arc_angle = 90.0f);
    
    core::ecs::EntityId CreateProjectile(core::ecs::EntityId owner,
                                        const core::utils::Vector3& origin,
                                        const core::utils::Vector3& direction,
                                        const components::Skill& skill);
    
    std::vector<core::ecs::EntityId> GetEntitiesInCone(
        const core::utils::Vector3& origin,
        const core::utils::Vector3& direction,
        float range, float angle);
    
    std::vector<core::ecs::EntityId> CheckProjectilePath(
        const core::utils::Vector3& origin,
        const core::utils::Vector3& direction,
        float range, float width);
    
    bool DodgeRoll(core::ecs::EntityId entity, const core::utils::Vector3& direction);
    bool IsInvulnerable(core::ecs::EntityId entity);
    
private:
    // [SEQUENCE: MVP4-32] Private helper methods for internal action combat logic.
    float CalculateDamage(const components::CombatStatsComponent& attacker_stats,
                         const components::CombatStatsComponent& defender_stats,
                         float base_damage, bool is_physical);
    
    bool ApplyDamage(core::ecs::EntityId target, float damage,
                    core::ecs::EntityId source);
    
    void UpdateProjectiles(float delta_time);
    void CheckProjectileCollisions(core::ecs::EntityId projectile,
                                  const components::ProjectileComponent& proj,
                                  const core::utils::Vector3& position);
    
    void ProcessSkillCasts(float delta_time);
    void ProcessSkillCooldowns(float delta_time);
    
    void UpdateDodgeStates(float delta_time);
    void RechargeDodges(float delta_time);
    
    void OnHit(core::ecs::EntityId attacker, core::ecs::EntityId target,
              float damage, bool is_critical);
    void OnDodge(core::ecs::EntityId entity);
    void OnDeath(core::ecs::EntityId entity);
    
    bool IsValidTarget(core::ecs::EntityId attacker, core::ecs::EntityId target);
    float GetAngleBetween(const core::utils::Vector3& v1, const core::utils::Vector3& v2);
    core::utils::Vector3 RotateVector(const core::utils::Vector3& vec, float angle);

    // [SEQUENCE: MVP4-33] Private member variables for system state and configuration.
    class GridSpatialSystem* spatial_system_ = nullptr;
    
    struct ActionCombatConfig {
        float projectile_update_rate = 60.0f;
        float melee_range = 5.0f;
        float dodge_distance = 10.0f;
        float dodge_duration = 0.5f;
        float combo_window = 2.0f;
        float hitbox_padding = 0.5f;
    } config_;
    
    struct HitRecord {
        std::unordered_set<core::ecs::EntityId> hit_entities;
        std::chrono::steady_clock::time_point expire_time;
    };
    std::unordered_map<uint64_t, HitRecord> hit_records_;
};

} // namespace mmorpg::game::systems::combat