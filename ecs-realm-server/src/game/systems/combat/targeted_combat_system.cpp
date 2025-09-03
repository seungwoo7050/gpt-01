#include "game/systems/combat/targeted_combat_system.h"
#include "game/systems/grid_spatial_system.h"
#include "core/ecs/optimized/optimized_world.h"
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>

namespace mmorpg::game::systems::combat {

TargetedCombatSystem::~TargetedCombatSystem() = default;

// [SEQUENCE: MVP4-12] Traditional target-based combat system

void TargetedCombatSystem::Update(float delta_time) {
    if (!world_) return;

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
            // Mark for removal and call OnCombatEnd
            it = entities_in_combat_.erase(it);
            last_combat_time_.erase(entity);
            OnCombatEnd(entity);
        } else {
            ++it;
        }
    }
}

void TargetedCombatSystem::ProcessAutoAttacks([[maybe_unused]] float delta_time) {
    if (!world_) return;

    for (const auto& entity : entities_) {
        auto& target_comp = world_->GetComponent<components::TargetComponent>(entity);
        if (!target_comp.auto_attacking || target_comp.current_target == 0) {
            continue;
        }

        if (total_time_ >= target_comp.next_auto_attack_time) {
            ExecuteAutoAttack(entity, target_comp.current_target);

            last_combat_time_[entity] = total_time_;
            last_combat_time_[target_comp.current_target] = total_time_;

            auto& stats = world_->GetComponent<components::CombatStatsComponent>(entity);
            float attack_delay = 1.0f / stats.attack_speed;
            target_comp.next_auto_attack_time = total_time_ + attack_delay;
        }
    }
}

void TargetedCombatSystem::ExecuteAutoAttack(core::ecs::EntityId attacker, core::ecs::EntityId target) {
    if (!world_) return;

    if (!ValidateTarget(attacker, target)) {
        auto& target_comp = world_->GetComponent<components::TargetComponent>(attacker);
        target_comp.auto_attacking = false;
        return;
    }

    auto& target_comp = world_->GetComponent<components::TargetComponent>(attacker);
    if (!IsInRange(attacker, target, target_comp.auto_attack_range)) {
        return;
    }

    auto& attacker_stats = world_->GetComponent<components::CombatStatsComponent>(attacker);
    auto& defender_stats = world_->GetComponent<components::CombatStatsComponent>(target);

    float base_damage = 10.0f;
    float damage = CalculateDamage(attacker_stats, defender_stats, base_damage, true);

    if (ApplyDamage(target, damage, attacker)) {
        spdlog::debug("Entity {} auto-attacked {} for {} damage", attacker, target, damage);

        OnCombatStart(attacker);
        OnCombatStart(target);
    }
}

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

bool TargetedCombatSystem::ApplyDamage(core::ecs::EntityId target, float damage, core::ecs::EntityId attacker) {
    if (!world_) return false;

    auto& health = world_->GetComponent<components::HealthComponent>(target);
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

bool TargetedCombatSystem::ApplyHealing(core::ecs::EntityId target, float healing) {
    if (!world_) return false;

    auto& health = world_->GetComponent<components::HealthComponent>(target);
    if (health.is_dead) {
        return false;
    }

    health.current_hp += healing;
    if (health.current_hp > health.max_hp) {
        health.current_hp = health.max_hp;
    }

    return true;
}

bool TargetedCombatSystem::ValidateTarget(core::ecs::EntityId attacker, core::ecs::EntityId target) {
    if (!world_) return false;

    if (!world_->IsValid(target)) {
        return false;
    }

    auto& target_health = world_->GetComponent<components::HealthComponent>(target);
    if (target_health.is_dead) {
        return false;
    }

    auto& target_comp = world_->GetComponent<components::TargetComponent>(attacker);
    if (!IsInRange(attacker, target, target_comp.max_target_range)) {
        return false;
    }

    return true;
}

bool TargetedCombatSystem::IsInRange(core::ecs::EntityId attacker, core::ecs::EntityId target, float range) {
    if (!world_) return false;

    auto& attacker_transform = world_->GetComponent<components::TransformComponent>(attacker);
    auto& target_transform = world_->GetComponent<components::TransformComponent>(target);

    float dx = attacker_transform.position.x - target_transform.position.x;
    float dy = attacker_transform.position.y - target_transform.position.y;
    float dz = attacker_transform.position.z - target_transform.position.z;

    float distance_squared = dx * dx + dy * dy + dz * dz;
    return distance_squared <= (range * range);
}

void TargetedCombatSystem::OnDeath(core::ecs::EntityId entity) {
    spdlog::info("Entity {} died", entity);

    if (!world_) return;

    for (const auto& attacker_entity : entities_) {
        auto& target_comp = world_->GetComponent<components::TargetComponent>(attacker_entity);
        if (target_comp.current_target == entity) {
            ClearTarget(attacker_entity);
        }
    }

    OnCombatEnd(entity);
}

void TargetedCombatSystem::OnCombatEnd(core::ecs::EntityId entity) {
    entities_in_combat_.erase(entity);
    spdlog::debug("Entity {} left combat", entity);
}

bool TargetedCombatSystem::ClearTarget(core::ecs::EntityId attacker) {
    if (!world_) return false;

    auto& target_comp = world_->GetComponent<components::TargetComponent>(attacker);
    target_comp.current_target = 0;
    target_comp.target_type = components::TargetType::NONE;
    target_comp.auto_attacking = false;
    return true;
}

bool TargetedCombatSystem::UseSkill(core::ecs::EntityId caster, uint32_t skill_id) {
    if (!world_) return false;

    auto& skill_comp = world_->GetComponent<components::SkillComponent>(caster);
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

void TargetedCombatSystem::ProcessSkillCasts(float) {}
void TargetedCombatSystem::ProcessSkillCooldowns(float delta_time) {
    if (!world_) return;

    for (auto& entity : entities_) {
        if (!world_->HasComponent<components::SkillComponent>(entity)) continue;
        auto& skill_comp = world_->GetComponent<components::SkillComponent>(entity);
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
void TargetedCombatSystem::UpdateTargetValidation(float) {}
void TargetedCombatSystem::OnCombatStart(core::ecs::EntityId entity) {
    entities_in_combat_.insert(entity);
    last_combat_time_[entity] = total_time_;
    spdlog::debug("Entity {} entered combat", entity);
}

void TargetedCombatSystem::CleanupInvalidTargets() {}

bool TargetedCombatSystem::IsInCombat(core::ecs::EntityId entity) {
    return entities_in_combat_.count(entity);
}

}
