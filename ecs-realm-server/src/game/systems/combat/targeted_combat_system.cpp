#include "game/systems/combat/targeted_combat_system.h"
#include "game/systems/grid_spatial_system.h"
#include "core/ecs/optimized/optimized_world.h"
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>

namespace mmorpg::game::systems::combat {

// [SEQUENCE: MVP4-11] Implements the destructor.
TargetedCombatSystem::~TargetedCombatSystem() = default;

// [SEQUENCE: MVP4-12] Implements the main update loop for the combat system.
void TargetedCombatSystem::Update(float delta_time) {
    if (!m_world) return;

    total_time_ += delta_time;

    ProcessAutoAttacks(delta_time);
    ProcessSkillCasts(delta_time);
    ProcessSkillCooldowns(delta_time);
    UpdateTargetValidation(delta_time);
    CleanupInvalidTargets();

    // Process combat timeout
    for (auto it = entities_in_combat_.begin(); it != entities_in_combat_.end();) {
        auto entity = *it;
        if (last_combat_time_.find(entity) == last_combat_time_.end() || (total_time_ - last_combat_time_[entity] > config_.combat_timeout)) {
            it = entities_in_combat_.erase(it);
            last_combat_time_.erase(entity);
            OnCombatEnd(entity);
        } else {
            ++it;
        }
    }
}

// [SEQUENCE: MVP4-13] Processes auto-attacks for all relevant entities.
void TargetedCombatSystem::ProcessAutoAttacks([[maybe_unused]] float delta_time) {
    if (!m_world) return;

    for (const auto& entity : m_entities) {
        auto& target_comp = m_world->GetComponent<components::TargetComponent>(entity);
        if (!target_comp.auto_attacking || target_comp.current_target == 0) {
            continue;
        }

        if (total_time_ >= target_comp.next_auto_attack_time) {
            ExecuteAutoAttack(entity, target_comp.current_target);

            last_combat_time_[entity] = total_time_;
            last_combat_time_[target_comp.current_target] = total_time_;

            auto& stats = m_world->GetComponent<components::CombatStatsComponent>(entity);
            float attack_delay = 1.0f / stats.attack_speed;
            target_comp.next_auto_attack_time = total_time_ + attack_delay;
        }
    }
}

// [SEQUENCE: MVP4-14] Executes a single auto-attack from an attacker to a target.
void TargetedCombatSystem::ExecuteAutoAttack(core::ecs::EntityId attacker, core::ecs::EntityId target) {
    if (!m_world) return;

    if (!ValidateTarget(attacker, target)) {
        auto& target_comp = m_world->GetComponent<components::TargetComponent>(attacker);
        target_comp.auto_attacking = false;
        return;
    }

    auto& target_comp = m_world->GetComponent<components::TargetComponent>(attacker);
    if (!IsInRange(attacker, target, target_comp.auto_attack_range)) {
        return;
    }

    auto& attacker_stats = m_world->GetComponent<components::CombatStatsComponent>(attacker);
    auto& defender_stats = m_world->GetComponent<components::CombatStatsComponent>(target);

    float base_damage = 10.0f;
    float damage = CalculateDamage(attacker_stats, defender_stats, base_damage, true);

    if (ApplyDamage(target, damage, attacker)) {
        spdlog::debug("Entity {} auto-attacked {} for {} damage", attacker, target, damage);

        OnCombatStart(attacker);
        OnCombatStart(target);
    }
}

// [SEQUENCE: MVP4-15] Calculates the final damage based on attacker and defender stats.
float TargetedCombatSystem::CalculateDamage(const components::CombatStatsComponent& attacker_stats,
                                           const components::CombatStatsComponent& defender_stats,
                                           float base_damage, bool is_physical) {
    float damage = base_damage;

    if (is_physical) {
        damage *= (1.0f + attacker_stats.attack_power / 100.0f);

        float armor_reduction = defender_stats.armor * config_.armor_reduction_factor;
        armor_reduction = std::min(armor_reduction, 0.75f);
        damage *= (1.0f - armor_reduction);
    } else {
        damage *= (1.0f + attacker_stats.spell_power / 100.0f);

        float resist_reduction = defender_stats.magic_resist * config_.armor_reduction_factor;
        resist_reduction = std::min(resist_reduction, 0.75f);
        damage *= (1.0f - resist_reduction);
    }

    float crit_roll = static_cast<float>(rand()) / RAND_MAX;
    if (crit_roll < attacker_stats.critical_chance) {
        damage *= attacker_stats.critical_damage;
        spdlog::debug("Critical hit! Damage: {}", damage);
    }

    int level_diff = attacker_stats.level - defender_stats.level;
    float level_modifier = 1.0f + (level_diff * config_.level_difference_factor);
    level_modifier = std::max(0.5f, std::min(1.5f, level_modifier));
    damage *= level_modifier;

    damage *= (1.0f + attacker_stats.damage_increase);
    damage *= (1.0f - defender_stats.damage_reduction);

    return damage;
}

// [SEQUENCE: MVP4-16] Applies damage to a target's HealthComponent.
bool TargetedCombatSystem::ApplyDamage(core::ecs::EntityId target, float damage, core::ecs::EntityId attacker) {
    if (!m_world) return false;

    auto& health = m_world->GetComponent<components::HealthComponent>(target);
    if (health.is_dead) {
        return false;
    }

    health.current_hp -= damage;

    last_combat_time_[target] = total_time_;
    last_combat_time_[attacker] = total_time_;

    if (health.current_hp <= 0) {
        health.current_hp = 0;
        health.is_dead = true;
        OnDeath(target);
    }

    return true;
}

// [SEQUENCE: MVP4-17] Applies healing to a target's HealthComponent.
bool TargetedCombatSystem::ApplyHealing(core::ecs::EntityId target, float healing) {
    if (!m_world) return false;

    auto& health = m_world->GetComponent<components::HealthComponent>(target);
    if (health.is_dead) {
        return false;
    }

    health.current_hp += healing;
    if (health.current_hp > health.max_hp) {
        health.current_hp = health.max_hp;
    }

    return true;
}

// [SEQUENCE: MVP4-18] Validates if a target is still attackable.
bool TargetedCombatSystem::ValidateTarget(core::ecs::EntityId attacker, core::ecs::EntityId target) {
    if (!m_world) return false;

    if (!m_world->IsValid(target)) {
        return false;
    }

    auto& target_health = m_world->GetComponent<components::HealthComponent>(target);
    if (target_health.is_dead) {
        return false;
    }

    auto& target_comp = m_world->GetComponent<components::TargetComponent>(attacker);
    if (!IsInRange(attacker, target, target_comp.max_target_range)) {
        return false;
    }

    return true;
}

// [SEQUENCE: MVP4-19] Checks if two entities are within a specified range.
bool TargetedCombatSystem::IsInRange(core::ecs::EntityId attacker, core::ecs::EntityId target, float range) {
    if (!m_world) return false;

    auto& attacker_transform = m_world->GetComponent<components::TransformComponent>(attacker);
    auto& target_transform = m_world->GetComponent<components::TransformComponent>(target);

    float dx = attacker_transform.position.x - target_transform.position.x;
    float dy = attacker_transform.position.y - target_transform.position.y;
    float dz = attacker_transform.position.z - target_transform.position.z;

    float distance_squared = dx * dx + dy * dy + dz * dz;
    return distance_squared <= (range * range);
}

// [SEQUENCE: MVP4-20] Handles the death of an entity.
void TargetedCombatSystem::OnDeath(core::ecs::EntityId entity) {
    spdlog::info("Entity {} died", entity);

    if (!m_world) return;

    for (const auto& attacker_entity : m_entities) {
        auto& target_comp = m_world->GetComponent<components::TargetComponent>(attacker_entity);
        if (target_comp.current_target == entity) {
            ClearTarget(attacker_entity);
        }
    }

    OnCombatEnd(entity);
}

// [SEQUENCE: MVP4-21] Handles the end of combat for an entity.
void TargetedCombatSystem::OnCombatEnd(core::ecs::EntityId entity) {
    entities_in_combat_.erase(entity);
    spdlog::debug("Entity {} left combat", entity);
}

// [SEQUENCE: MVP4-22] Clears the current target of an entity.
bool TargetedCombatSystem::ClearTarget(core::ecs::EntityId attacker) {
    if (!m_world) return false;

    auto& target_comp = m_world->GetComponent<components::TargetComponent>(attacker);
    target_comp.current_target = 0;
    target_comp.target_type = components::TargetType::NONE;
    target_comp.auto_attacking = false;
    return true;
}

// [SEQUENCE: MVP4-23] Handles the use of a skill by an entity.
bool TargetedCombatSystem::UseSkill(core::ecs::EntityId caster, uint32_t skill_id) {
    if (!m_world) return false;

    auto& skill_comp = m_world->GetComponent<components::SkillComponent>(caster);
    auto it = skill_comp.skills.find(skill_id);
    if (it == skill_comp.skills.end()) {
        return false; // Skill not found
    }

    auto& skill = it->second;
    if (skill.on_cooldown) {
        return false; // Skill on cooldown
    }

    skill.on_cooldown = true;
    skill.cooldown_timer = skill.cooldown;

    // TODO: Implement skill execution
    return true;
}

// [SEQUENCE: MVP4-24] Placeholder for processing skill casts.
void TargetedCombatSystem::ProcessSkillCasts(float) {}

// [SEQUENCE: MVP4-25] Processes skill cooldowns for all entities.
void TargetedCombatSystem::ProcessSkillCooldowns(float delta_time) {
    if (!m_world) return;

    for (auto& entity : m_entities) {
        if (!m_world->HasComponent<components::SkillComponent>(entity)) continue;
        auto& skill_comp = m_world->GetComponent<components::SkillComponent>(entity);
        for (auto& it : skill_comp.skills) {
            auto& skill = it.second;
            if (skill.on_cooldown) {
                skill.cooldown_timer -= delta_time;
                if (skill.cooldown_timer <= 0) {
                    skill.on_cooldown = false;
                }
            }
        }
    }
}

// [SEQUENCE: MVP4-26] Placeholder for updating target validation.
void TargetedCombatSystem::UpdateTargetValidation(float) {}

// [SEQUENCE: MVP4-27] Handles the start of combat for an entity.
void TargetedCombatSystem::OnCombatStart(core::ecs::EntityId entity) {
    entities_in_combat_.insert(entity);
    last_combat_time_[entity] = total_time_;
    spdlog::debug("Entity {} entered combat", entity);
}

// [SEQUENCE: MVP4-28] Placeholder for cleaning up invalid targets.
void TargetedCombatSystem::CleanupInvalidTargets() {}

// [SEQUENCE: MVP4-29] Checks if an entity is currently in combat.
bool TargetedCombatSystem::IsInCombat(core::ecs::EntityId entity) {
    return entities_in_combat_.count(entity);
}

}
