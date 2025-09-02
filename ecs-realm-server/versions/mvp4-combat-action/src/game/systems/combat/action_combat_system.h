#pragma once

#include "core/ecs/system.h"
#include "game/components/health_component.h"
#include "game/components/combat_stats_component.h"
#include "game/components/skill_component.h"
#include "game/components/transform_component.h"
#include "core/utils/vector3.h"
#include <memory>

namespace mmorpg::game::systems::combat {

// [SEQUENCE: 1] Action-oriented combat without target locking
class ActionCombatSystem : public core::ecs::System {
public:
    ActionCombatSystem() : System("ActionCombatSystem") {}
    
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
    
    // [SEQUENCE: 5] Action combat abilities
    bool UseSkillshot(core::ecs::EntityId caster, uint32_t skill_id,
                     const core::utils::Vector3& direction);
    
    bool UseAreaSkill(core::ecs::EntityId caster, uint32_t skill_id,
                     const core::utils::Vector3& target_position);
    
    bool UseMeleeSwing(core::ecs::EntityId attacker,
                      const core::utils::Vector3& direction,
                      float arc_angle = 90.0f);
    
    // [SEQUENCE: 6] Projectile management
    core::ecs::EntityId CreateProjectile(core::ecs::EntityId owner,
                                        const core::utils::Vector3& origin,
                                        const core::utils::Vector3& direction,
                                        const Skill& skill);
    
    // [SEQUENCE: 7] Collision detection
    std::vector<core::ecs::EntityId> GetEntitiesInCone(
        const core::utils::Vector3& origin,
        const core::utils::Vector3& direction,
        float range, float angle);
    
    std::vector<core::ecs::EntityId> CheckProjectilePath(
        const core::utils::Vector3& origin,
        const core::utils::Vector3& direction,
        float range, float width);
    
    // [SEQUENCE: 8] Dodge mechanics
    bool DodgeRoll(core::ecs::EntityId entity, const core::utils::Vector3& direction);
    bool IsInvulnerable(core::ecs::EntityId entity);
    
private:
    // [SEQUENCE: 9] Projectile component (internal)
    struct ProjectileComponent {
        core::ecs::EntityId owner;
        core::utils::Vector3 velocity;
        float speed;
        float range;
        float traveled;
        float damage;
        float radius;  // Collision radius
        bool is_physical;
        bool piercing;  // Goes through enemies
        uint32_t skill_id;
    };
    
    // [SEQUENCE: 10] Dodge state component (internal)
    struct DodgeComponent {
        std::chrono::steady_clock::time_point dodge_end_time;
        std::chrono::steady_clock::time_point next_dodge_time;
        core::utils::Vector3 dodge_direction;
        bool is_dodging = false;
        int dodge_charges = 2;
        float dodge_recharge_time = 5.0f;
    };
    
    // [SEQUENCE: 11] Combat calculations
    float CalculateDamage(const CombatStatsComponent& attacker_stats,
                         const CombatStatsComponent& defender_stats,
                         float base_damage, bool is_physical);
    
    bool ApplyDamage(core::ecs::EntityId target, float damage,
                    core::ecs::EntityId source);
    
    // [SEQUENCE: 12] Projectile processing
    void UpdateProjectiles(float delta_time);
    void CheckProjectileCollisions(core::ecs::EntityId projectile,
                                  const ProjectileComponent& proj,
                                  const core::utils::Vector3& position);
    
    // [SEQUENCE: 13] Skill processing
    void ProcessSkillCasts(float delta_time);
    void ProcessSkillCooldowns(float delta_time);
    
    // [SEQUENCE: 14] Dodge processing
    void UpdateDodgeStates(float delta_time);
    void RechargeDodges(float delta_time);
    
    // [SEQUENCE: 15] Combat events
    void OnHit(core::ecs::EntityId attacker, core::ecs::EntityId target,
              float damage, bool is_critical);
    void OnDodge(core::ecs::EntityId entity);
    void OnDeath(core::ecs::EntityId entity);
    
    // [SEQUENCE: 16] Spatial integration
    class SpatialIndexingSystem* spatial_system_ = nullptr;
    
    // [SEQUENCE: 17] Combat configuration
    struct ActionCombatConfig {
        float projectile_update_rate = 60.0f;    // Updates per second
        float melee_range = 5.0f;                // Default melee range
        float dodge_distance = 10.0f;            // Roll distance
        float dodge_duration = 0.5f;             // Invulnerability time
        float combo_window = 2.0f;               // Time to chain abilities
        float hitbox_padding = 0.5f;             // Extra collision size
    } config_;
    
    // [SEQUENCE: 18] Projectile tracking
    std::unordered_map<core::ecs::EntityId, ProjectileComponent> projectiles_;
    std::unordered_map<core::ecs::EntityId, DodgeComponent> dodge_states_;
    
    // [SEQUENCE: 19] Hit tracking (prevent multi-hits)
    struct HitRecord {
        std::unordered_set<core::ecs::EntityId> hit_entities;
        std::chrono::steady_clock::time_point expire_time;
    };
    std::unordered_map<uint64_t, HitRecord> hit_records_;  // Key: projectile_id << 32 | skill_id
    
    // [SEQUENCE: 20] Helper methods
    bool IsValidTarget(core::ecs::EntityId attacker, core::ecs::EntityId target);
    float GetAngleBetween(const core::utils::Vector3& v1, const core::utils::Vector3& v2);
    core::utils::Vector3 RotateVector(const core::utils::Vector3& vec, float angle);
};

} // namespace mmorpg::game::systems::combat