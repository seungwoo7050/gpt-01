// [SEQUENCE: MVP4-13] This system handles non-targeting, action-based combat.
#pragma once

#include "core/ecs/system.h"
#include "game/components/health_component.h"
#include "game/components/combat_stats_component.h"
#include "game/components/skill_component.h"
#include "game/components/transform_component.h"
#include "game/components/projectile_component.h"
#include "game/components/dodge_component.h"
#include "core/utils/vector3.h"
#include <memory>

namespace mmorpg::game::systems::combat {

class ActionCombatSystem : public core::ecs::System {
public:
    ActionCombatSystem() = default;
    
    void OnSystemInit() override;
    void OnSystemShutdown() override;
    void Update(float delta_time) override;
    
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
    float CalculateDamage(const components::CombatStatsComponent& attacker_stats,
                         const components::CombatStatsComponent& defender_stats,
                         float base_damage, bool is_physical);
    
    bool ApplyDamage(core::ecs::EntityId target, float damage,
                    core::ecs::EntityId source);
    
    void UpdateProjectiles(float delta_time);
    void CheckProjectileCollisions(core::ecs::EntityId projectile,
                                  const components::ProjectileComponent& proj,
                                  const core::utils::Vector3& position);
    
    void ProcessSkillCasts(float delta_time, core::ecs::EntityId entity);
    void ProcessSkillCooldowns(float delta_time, core::ecs::EntityId entity);
    
    void UpdateDodgeStates(float delta_time);
    void RechargeDodges(float delta_time);
    
    void OnHit(core::ecs::EntityId attacker, core::ecs::EntityId target,
              float damage, bool is_critical);
    void OnDodge(core::ecs::EntityId entity);
    void OnDeath(core::ecs::EntityId entity);
    
    class GridSpatialSystem* spatial_system_ = nullptr;
    
    struct ActionCombatConfig {
        float projectile_update_rate = 60.0f;
        float melee_range = 5.0f;
        float dodge_distance = 10.0f;
        float dodge_duration = 0.5f;
        float combo_window = 2.0f;
        float hitbox_padding = 0.5f;
    } config_;
    
    std::unordered_map<core::ecs::EntityId, components::DodgeComponent> dodge_states_;
    
    struct HitRecord {
        std::unordered_set<core::ecs::EntityId> hit_entities;
        std::chrono::steady_clock::time_point expire_time;
    };
    std::unordered_map<uint64_t, HitRecord> hit_records_;
    
    bool IsValidTarget(core::ecs::EntityId attacker, core::ecs::EntityId target);
    float GetAngleBetween(const core::utils::Vector3& v1, const core::utils::Vector3& v2);
    core::utils::Vector3 RotateVector(const core::utils::Vector3& vec, float angle);
};

} // namespace mmorpg::game::systems::combat