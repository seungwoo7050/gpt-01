#include "game/systems/combat/action_combat_system.h"
#include "game/systems/grid_spatial_system.h"
#include "core/ecs/optimized/optimized_world.h"
#include "game/components/projectile_component.h"
#include "game/components/dodge_component.h"
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>

namespace mmorpg::game::systems::combat {

ActionCombatSystem::ActionCombatSystem() = default;
ActionCombatSystem::~ActionCombatSystem() = default;

void ActionCombatSystem::Update(float delta_time) {
    if (!world_) return;

    UpdateProjectiles(delta_time);
    ProcessSkillCasts(delta_time);
    ProcessSkillCooldowns(delta_time);
    UpdateDodgeStates(delta_time);
    RechargeDodges(delta_time);

    auto now = std::chrono::steady_clock::now();
    for (auto it = hit_records_.begin(); it != hit_records_.end();) {
        if (now > it->second.expire_time) {
            it = hit_records_.erase(it);
        } else {
            ++it;
        }
    }
}

void ActionCombatSystem::UpdateProjectiles(float delta_time) {
    if (!world_) return;

    std::vector<core::ecs::EntityId> to_remove;

    for (auto entity : world_->GetEntitiesWith<components::ProjectileComponent, components::TransformComponent>()) {
        auto& proj = world_->GetComponent<components::ProjectileComponent>(entity);
        auto& transform = world_->GetComponent<components::TransformComponent>(entity);

        transform.position.x += proj.velocity.x * delta_time;
        transform.position.y += proj.velocity.y * delta_time;
        transform.position.z += proj.velocity.z * delta_time;

        float dist = proj.speed * delta_time;
        proj.traveled += dist;

        if (proj.traveled >= proj.range) {
            to_remove.push_back(entity);
            continue;
        }

        CheckProjectileCollisions(entity, proj, transform.position);
    }

    for (auto id : to_remove) {
        world_->DestroyEntity(id);
    }
}

void ActionCombatSystem::CheckProjectileCollisions(core::ecs::EntityId projectile,
                                                  const components::ProjectileComponent& proj,
                                                  const core::utils::Vector3& position) {
    if (!spatial_system_) return;

    if (!world_) return;

    uint64_t hit_key = (static_cast<uint64_t>(projectile) << 32) | proj.skill_id;
    auto hit_it = hit_records_.find(hit_key);
    if (hit_it == hit_records_.end()) return;

    auto nearby = spatial_system_->GetEntitiesInRadius(position, proj.radius + config_.hitbox_padding);

    auto& owner_stats = world_->GetComponent<components::CombatStatsComponent>(proj.owner);

    for (auto entity : nearby) {
        if (entity == proj.owner) continue;
        if (!IsValidTarget(proj.owner, entity)) continue;

        if (hit_it->second.hit_entities.find(entity) != hit_it->second.hit_entities.end()) {
            continue;
        }

        auto& target_stats = world_->GetComponent<components::CombatStatsComponent>(entity);
        float damage = CalculateDamage(owner_stats, target_stats, proj.damage, proj.is_physical);

        if (ApplyDamage(entity, damage, proj.owner)) {
            hit_it->second.hit_entities.insert(entity);

            if (!proj.piercing) {
                world_->DestroyEntity(projectile);
                return;
            }
        }
    }
}

float ActionCombatSystem::CalculateDamage(const components::CombatStatsComponent& attacker_stats,
                                         const components::CombatStatsComponent& defender_stats,
                                         float base_damage, bool is_physical) {
    float damage = base_damage;

    if (is_physical) {
        damage *= (1.0f + attacker_stats.attack_power / 100.0f);

        float armor_reduction = defender_stats.armor * 0.01f;
        armor_reduction = std::min(armor_reduction, 0.75f);
        damage *= (1.0f - armor_reduction);
    } else {
        damage *= (1.0f + attacker_stats.spell_power / 100.0f);

        float resist_reduction = defender_stats.magic_resist * 0.01f;
        resist_reduction = std::min(resist_reduction, 0.75f);
        damage *= (1.0f - resist_reduction);
    }

    if (IsInvulnerable(defender_stats.level)) { 
        return 0.0f;
    }

    float crit_roll = static_cast<float>(rand()) / RAND_MAX;
    if (crit_roll < attacker_stats.critical_chance) {
        damage *= attacker_stats.critical_damage;
    }

    damage *= (1.0f + attacker_stats.damage_increase);
    damage *= (1.0f - defender_stats.damage_reduction);

    return damage;
}

bool ActionCombatSystem::ApplyDamage(core::ecs::EntityId target, float damage,
                                    core::ecs::EntityId source) {
    if (IsInvulnerable(target)) {
        return false;
    }

    if (!world_) return false;

    auto& health = world_->GetComponent<components::HealthComponent>(target);
    if (health.is_dead) {
        return false;
    }

    health.current_hp -= damage;

    if (health.current_hp <= 0) {
        health.current_hp = 0;
        health.is_dead = true;
        OnDeath(target);
    }

    bool is_critical = false;
    OnHit(source, target, damage, is_critical);

    return true;
}

bool ActionCombatSystem::IsValidTarget([[maybe_unused]] core::ecs::EntityId attacker, core::ecs::EntityId target) {
    if (!world_) return false;

    if (!world_->IsValid(target)) {
        return false;
    }

    auto& health = world_->GetComponent<components::HealthComponent>(target);
    if (health.is_dead) {
        return false;
    }

    return true;
}

void ActionCombatSystem::OnHit(core::ecs::EntityId attacker, core::ecs::EntityId target,
                              float damage, bool is_critical) {
    spdlog::debug("Entity {} hit {} for {} damage{}", 
                 attacker, target, damage, is_critical ? " (CRIT)" : "");
}

void ActionCombatSystem::OnDodge(core::ecs::EntityId entity) {
    spdlog::debug("Entity {} performed dodge roll", entity);
}

void ActionCombatSystem::OnDeath(core::ecs::EntityId entity) {
    spdlog::info("Entity {} died in action combat", entity);
}

bool ActionCombatSystem::IsInvulnerable(core::ecs::EntityId) { return false; }
void ActionCombatSystem::ProcessSkillCasts(float) {}
void ActionCombatSystem::ProcessSkillCooldowns(float) {}

void ActionCombatSystem::UpdateDodgeStates(float) {
    if (!world_) return;

    auto now = std::chrono::steady_clock::now();
    for (auto entity : world_->GetEntitiesWith<components::DodgeComponent>()) {
        auto& dodge = world_->GetComponent<components::DodgeComponent>(entity);
        if (dodge.is_dodging && now >= dodge.dodge_end_time) {
            dodge.is_dodging = false;
        }
    }
}

void ActionCombatSystem::RechargeDodges(float) {
    if (!world_) return;

    auto now = std::chrono::steady_clock::now();
    for (auto entity : world_->GetEntitiesWith<components::DodgeComponent>()) {
        auto& dodge = world_->GetComponent<components::DodgeComponent>(entity);
        if (dodge.dodge_charges < 2 && now >= dodge.next_dodge_time) {
            dodge.dodge_charges++;
            dodge.next_dodge_time = now + std::chrono::seconds(static_cast<long>(dodge.dodge_recharge_time));
        }
    }
}

bool ActionCombatSystem::UseAreaSkill(core::ecs::EntityId caster, uint32_t, const core::utils::Vector3& target_position) {
    if (!world_ || !spatial_system_) return false;

    auto nearby = spatial_system_->GetEntitiesInRadius(target_position, 5.0f); // 5.0f is skill radius
    auto& caster_stats = world_->GetComponent<components::CombatStatsComponent>(caster);

    for (auto entity : nearby) {
        if (entity == caster) continue;
        if (!IsValidTarget(caster, entity)) continue;

        auto& target_stats = world_->GetComponent<components::CombatStatsComponent>(entity);
        float damage = CalculateDamage(caster_stats, target_stats, 20.0f, true); // 20.0f is base damage
        ApplyDamage(entity, damage, caster);
    }

    return true;
}

bool ActionCombatSystem::UseMeleeSwing(core::ecs::EntityId attacker, const core::utils::Vector3&, float) {
    if (!world_ || !spatial_system_) return false;

    auto& attacker_transform = world_->GetComponent<components::TransformComponent>(attacker);
    auto nearby = spatial_system_->GetEntitiesInRadius(attacker_transform.position, 5.0f); // 5.0f is melee range
    auto& attacker_stats = world_->GetComponent<components::CombatStatsComponent>(attacker);

    for (auto entity : nearby) {
        if (entity == attacker) continue;
        if (!IsValidTarget(attacker, entity)) continue;

        auto& target_stats = world_->GetComponent<components::CombatStatsComponent>(entity);
        float damage = CalculateDamage(attacker_stats, target_stats, 15.0f, true); // 15.0f is base damage
        ApplyDamage(entity, damage, attacker);
    }

    return true;
}

}