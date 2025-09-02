#include "game/systems/combat/targeted_combat_system.h"
#include "game/systems/spatial_indexing_system.h"
#include "core/ecs/world.h"
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>

namespace mmorpg::game::systems::combat {

// [SEQUENCE: 1] Initialize combat system
void TargetedCombatSystem::OnSystemInit() {
    // Find spatial system for range checks
    if (auto* world = GetWorld()) {
        spatial_system_ = dynamic_cast<SpatialIndexingSystem*>(
            world->GetSystem("SpatialIndexingSystem")
        );
    }
    
    spdlog::info("TargetedCombatSystem initialized");
}

// [SEQUENCE: 2] Cleanup
void TargetedCombatSystem::OnSystemShutdown() {
    entities_in_combat_.clear();
    last_combat_time_.clear();
    spdlog::info("TargetedCombatSystem shut down");
}

// [SEQUENCE: 3] Main combat update loop
void TargetedCombatSystem::Update(float delta_time) {
    // Process auto-attacks
    ProcessAutoAttacks(delta_time);
    
    // Process skill casts
    ProcessSkillCasts(delta_time);
    
    // Update cooldowns
    ProcessSkillCooldowns(delta_time);
    
    // Validate targets
    UpdateTargetValidation(delta_time);
    
    // Clean up invalid targets
    CleanupInvalidTargets();
    
    // Update combat states
    auto now = std::chrono::steady_clock::now();
    auto timeout = std::chrono::seconds(static_cast<int>(config_.combat_timeout));
    
    for (auto it = last_combat_time_.begin(); it != last_combat_time_.end();) {
        if (now - it->second > timeout) {
            OnCombatEnd(it->first);
            it = last_combat_time_.erase(it);
        } else {
            ++it;
        }
    }
}

// [SEQUENCE: 4] Set combat target
bool TargetedCombatSystem::SetTarget(core::ecs::EntityId attacker, core::ecs::EntityId target) {
    auto* world = GetWorld();
    if (!world) return false;
    
    // Get target component
    auto* target_comp = world->GetComponent<TargetComponent>(attacker);
    if (!target_comp) return false;
    
    // Validate target
    if (!ValidateTarget(attacker, target)) {
        return false;
    }
    
    // Set target
    target_comp->current_target = target;
    target_comp->target_type = TargetType::ENEMY;  // TODO: Determine actual type
    target_comp->last_validation_time = std::chrono::steady_clock::now();
    
    // Add to target history
    target_comp->target_history.push_back(target);
    if (target_comp->target_history.size() > 10) {
        target_comp->target_history.erase(target_comp->target_history.begin());
    }
    
    spdlog::debug("Entity {} targeted entity {}", attacker, target);
    return true;
}

// [SEQUENCE: 5] Clear target
bool TargetedCombatSystem::ClearTarget(core::ecs::EntityId attacker) {
    auto* world = GetWorld();
    if (!world) return false;
    
    auto* target_comp = world->GetComponent<TargetComponent>(attacker);
    if (!target_comp) return false;
    
    target_comp->current_target = 0;
    target_comp->target_type = TargetType::NONE;
    target_comp->auto_attacking = false;
    
    return true;
}

// [SEQUENCE: 6] Start auto-attacking
bool TargetedCombatSystem::StartAutoAttack(core::ecs::EntityId attacker) {
    auto* world = GetWorld();
    if (!world) return false;
    
    auto* target_comp = world->GetComponent<TargetComponent>(attacker);
    if (!target_comp || target_comp->current_target == 0) {
        return false;
    }
    
    target_comp->auto_attacking = true;
    target_comp->next_auto_attack_time = std::chrono::steady_clock::now();
    
    OnCombatStart(attacker);
    OnCombatStart(target_comp->current_target);
    
    return true;
}

// [SEQUENCE: 7] Use skill
bool TargetedCombatSystem::UseSkill(core::ecs::EntityId caster, uint32_t skill_id) {
    auto* world = GetWorld();
    if (!world) return false;
    
    // Get components
    auto* skill_comp = world->GetComponent<SkillComponent>(caster);
    auto* target_comp = world->GetComponent<TargetComponent>(caster);
    auto* health_comp = world->GetComponent<HealthComponent>(caster);
    
    if (!skill_comp || !health_comp) return false;
    
    // Find skill
    auto skill_it = skill_comp->skills.find(skill_id);
    if (skill_it == skill_comp->skills.end()) {
        spdlog::warn("Entity {} doesn't know skill {}", caster, skill_id);
        return false;
    }
    
    const Skill& skill = skill_it->second;
    
    // Check if already casting
    if (skill_comp->casting_skill_id != 0) {
        return false;
    }
    
    // Check cooldown
    auto cd_it = skill_comp->cooldowns.find(skill_id);
    if (cd_it != skill_comp->cooldowns.end() && cd_it->second.is_on_cooldown) {
        auto now = std::chrono::steady_clock::now();
        if (now < cd_it->second.ready_time) {
            return false;
        }
    }
    
    // Check global cooldown
    auto now = std::chrono::steady_clock::now();
    if (now < skill_comp->global_cooldown_end) {
        return false;
    }
    
    // Check resource cost
    if (skill.resource_type == ResourceType::MANA && health_comp->current_mana < skill.resource_cost) {
        return false;
    }
    
    // Check target requirement
    if (skill.type == SkillType::TARGETED && (!target_comp || target_comp->current_target == 0)) {
        return false;
    }
    
    // Start casting
    if (skill.cast_time > 0) {
        skill_comp->casting_skill_id = skill_id;
        skill_comp->cast_end_time = now + std::chrono::milliseconds(
            static_cast<int>(skill.cast_time * 1000)
        );
        skill_comp->cast_target = target_comp ? target_comp->current_target : 0;
        
        spdlog::debug("Entity {} started casting skill {} ({}s cast time)",
                     caster, skill_id, skill.cast_time);
    } else {
        // Instant cast
        ExecuteSkill(caster, skill);
    }
    
    return true;
}

// [SEQUENCE: 8] Validate target
bool TargetedCombatSystem::ValidateTarget(core::ecs::EntityId attacker, core::ecs::EntityId target) {
    auto* world = GetWorld();
    if (!world) return false;
    
    // Check if target exists
    if (!world->IsEntityAlive(target)) {
        return false;
    }
    
    // Check if target has health
    auto* target_health = world->GetComponent<HealthComponent>(target);
    if (!target_health || target_health->is_dead) {
        return false;
    }
    
    // Check range
    auto* target_comp = world->GetComponent<TargetComponent>(attacker);
    if (!target_comp) return false;
    
    if (!IsInRange(attacker, target, target_comp->max_target_range)) {
        return false;
    }
    
    // TODO: Check faction, friendly fire, etc.
    
    return true;
}

// [SEQUENCE: 9] Check range between entities
bool TargetedCombatSystem::IsInRange(core::ecs::EntityId attacker, core::ecs::EntityId target, float range) {
    auto* world = GetWorld();
    if (!world) return false;
    
    auto* attacker_transform = world->GetComponent<TransformComponent>(attacker);
    auto* target_transform = world->GetComponent<TransformComponent>(target);
    
    if (!attacker_transform || !target_transform) {
        return false;
    }
    
    float dx = attacker_transform->position.x - target_transform->position.x;
    float dy = attacker_transform->position.y - target_transform->position.y;
    float dz = attacker_transform->position.z - target_transform->position.z;
    
    float distance_squared = dx * dx + dy * dy + dz * dz;
    return distance_squared <= (range * range);
}

// [SEQUENCE: 10] Calculate damage
float TargetedCombatSystem::CalculateDamage(const CombatStatsComponent& attacker_stats,
                                           const CombatStatsComponent& defender_stats,
                                           float base_damage, bool is_physical) {
    float damage = base_damage;
    
    // Apply attack/spell power
    if (is_physical) {
        damage *= (1.0f + attacker_stats.attack_power / 100.0f);
        
        // Apply armor reduction
        float armor_reduction = defender_stats.armor * config_.armor_reduction_factor;
        armor_reduction = std::min(armor_reduction, 0.75f);  // Cap at 75% reduction
        damage *= (1.0f - armor_reduction);
    } else {
        damage *= (1.0f + attacker_stats.spell_power / 100.0f);
        
        // Apply magic resistance
        float resist_reduction = defender_stats.magic_resist * config_.armor_reduction_factor;
        resist_reduction = std::min(resist_reduction, 0.75f);
        damage *= (1.0f - resist_reduction);
    }
    
    // Check for critical hit
    float crit_roll = static_cast<float>(rand()) / RAND_MAX;
    if (crit_roll < attacker_stats.critical_chance) {
        damage *= attacker_stats.critical_damage;
        spdlog::debug("Critical hit! Damage: {}", damage);
    }
    
    // Apply level difference
    int level_diff = attacker_stats.level - defender_stats.level;
    float level_modifier = 1.0f + (level_diff * config_.level_difference_factor);
    level_modifier = std::max(0.5f, std::min(1.5f, level_modifier));  // Clamp between 50-150%
    damage *= level_modifier;
    
    // Apply global modifiers
    damage *= (1.0f + attacker_stats.damage_increase);
    damage *= (1.0f - defender_stats.damage_reduction);
    
    // Minimum damage of 1
    return std::max(1.0f, damage);
}

// [SEQUENCE: 11] Apply damage to target
bool TargetedCombatSystem::ApplyDamage(core::ecs::EntityId target, float damage) {
    auto* world = GetWorld();
    if (!world) return false;
    
    auto* health = world->GetComponent<HealthComponent>(target);
    if (!health || health->is_dead) {
        return false;
    }
    
    health->current_health -= damage;
    health->last_damage_time = std::chrono::steady_clock::now();
    
    // Update combat time
    last_combat_time_[target] = std::chrono::steady_clock::now();
    
    if (health->current_health <= 0) {
        health->current_health = 0;
        health->is_dead = true;
        OnDeath(target);
    }
    
    return true;
}

// [SEQUENCE: 12] Process auto attacks
void TargetedCombatSystem::ProcessAutoAttacks(float delta_time) {
    auto* world = GetWorld();
    if (!world) return;
    
    auto now = std::chrono::steady_clock::now();
    
    // Get all entities with target components
    auto entities = world->GetEntitiesWithComponents<TargetComponent, CombatStatsComponent>();
    
    for (auto entity : entities) {
        auto* target_comp = world->GetComponent<TargetComponent>(entity);
        if (!target_comp->auto_attacking || target_comp->current_target == 0) {
            continue;
        }
        
        // Check if ready to attack
        if (now >= target_comp->next_auto_attack_time) {
            ExecuteAutoAttack(entity, target_comp->current_target);
            
            // Schedule next attack
            auto* stats = world->GetComponent<CombatStatsComponent>(entity);
            if (stats) {
                float attack_delay = 1.0f / stats->attack_speed;
                target_comp->next_auto_attack_time = now + std::chrono::milliseconds(
                    static_cast<int>(attack_delay * 1000)
                );
            }
        }
    }
}

// [SEQUENCE: 13] Execute auto attack
void TargetedCombatSystem::ExecuteAutoAttack(core::ecs::EntityId attacker, core::ecs::EntityId target) {
    auto* world = GetWorld();
    if (!world) return;
    
    // Validate target
    if (!ValidateTarget(attacker, target)) {
        auto* target_comp = world->GetComponent<TargetComponent>(attacker);
        if (target_comp) {
            target_comp->auto_attacking = false;
        }
        return;
    }
    
    // Check range
    auto* target_comp = world->GetComponent<TargetComponent>(attacker);
    if (!IsInRange(attacker, target, target_comp->auto_attack_range)) {
        return;
    }
    
    // Get combat stats
    auto* attacker_stats = world->GetComponent<CombatStatsComponent>(attacker);
    auto* defender_stats = world->GetComponent<CombatStatsComponent>(target);
    
    if (!attacker_stats || !defender_stats) {
        return;
    }
    
    // Calculate and apply damage
    float base_damage = 10.0f;  // Base auto-attack damage
    float damage = CalculateDamage(*attacker_stats, *defender_stats, base_damage, true);
    
    if (ApplyDamage(target, damage)) {
        spdlog::debug("Entity {} auto-attacked {} for {} damage", attacker, target, damage);
        
        // Update combat timers
        last_combat_time_[attacker] = std::chrono::steady_clock::now();
        entities_in_combat_.insert(attacker);
        entities_in_combat_.insert(target);
    }
}

// [SEQUENCE: 14] Process skill casts
void TargetedCombatSystem::ProcessSkillCasts(float delta_time) {
    auto* world = GetWorld();
    if (!world) return;
    
    auto now = std::chrono::steady_clock::now();
    auto entities = world->GetEntitiesWithComponent<SkillComponent>();
    
    for (auto entity : entities) {
        auto* skill_comp = world->GetComponent<SkillComponent>(entity);
        
        // Check if casting
        if (skill_comp->casting_skill_id == 0) {
            continue;
        }
        
        // Check if cast complete
        if (now >= skill_comp->cast_end_time) {
            // Find skill
            auto skill_it = skill_comp->skills.find(skill_comp->casting_skill_id);
            if (skill_it != skill_comp->skills.end()) {
                ExecuteSkill(entity, skill_it->second);
            }
            
            // Clear casting state
            skill_comp->casting_skill_id = 0;
            skill_comp->cast_target = 0;
        }
    }
}

// [SEQUENCE: 15] Execute skill
void TargetedCombatSystem::ExecuteSkill(core::ecs::EntityId caster, const Skill& skill) {
    auto* world = GetWorld();
    if (!world) return;
    
    // Deduct resource cost
    auto* health_comp = world->GetComponent<HealthComponent>(caster);
    if (health_comp && skill.resource_type == ResourceType::MANA) {
        health_comp->current_mana -= skill.resource_cost;
    }
    
    // Set cooldowns
    auto* skill_comp = world->GetComponent<SkillComponent>(caster);
    if (skill_comp) {
        // Skill cooldown
        auto& cooldown = skill_comp->cooldowns[skill.skill_id];
        cooldown.is_on_cooldown = true;
        cooldown.ready_time = std::chrono::steady_clock::now() + 
            std::chrono::milliseconds(static_cast<int>(skill.cooldown_duration * 1000));
        
        // Global cooldown
        skill_comp->global_cooldown_end = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(static_cast<int>(skill_comp->global_cooldown_duration * 1000));
        
        // Update combo state
        skill_comp->last_skill_used = skill.skill_id;
        skill_comp->combo_window_end = std::chrono::steady_clock::now() +
            std::chrono::seconds(3);  // 3 second combo window
    }
    
    // Handle different skill types
    switch (skill.type) {
        case SkillType::TARGETED: {
            auto* target_comp = world->GetComponent<TargetComponent>(caster);
            if (target_comp && target_comp->current_target != 0) {
                // Apply damage/effects to target
                auto* caster_stats = world->GetComponent<CombatStatsComponent>(caster);
                auto* target_stats = world->GetComponent<CombatStatsComponent>(target_comp->current_target);
                
                if (caster_stats && target_stats && skill.base_damage > 0) {
                    float damage = skill.base_damage * skill.damage_coefficient;
                    damage = CalculateDamage(*caster_stats, *target_stats, damage, skill.is_physical);
                    ApplyDamage(target_comp->current_target, damage);
                    
                    spdlog::debug("Entity {} used skill {} on {} for {} damage",
                                 caster, skill.skill_id, target_comp->current_target, damage);
                }
            }
            break;
        }
        
        case SkillType::AREA_OF_EFFECT: {
            // Use spatial system for AoE
            if (spatial_system_) {
                auto* transform = world->GetComponent<TransformComponent>(caster);
                if (transform) {
                    auto targets = spatial_system_->GetEntitiesInRadius(
                        transform->position, skill.radius
                    );
                    
                    auto* caster_stats = world->GetComponent<CombatStatsComponent>(caster);
                    if (caster_stats) {
                        for (auto target : targets) {
                            if (target == caster) continue;  // Don't hit self
                            
                            auto* target_stats = world->GetComponent<CombatStatsComponent>(target);
                            if (target_stats && skill.base_damage > 0) {
                                float damage = skill.base_damage * skill.damage_coefficient;
                                damage = CalculateDamage(*caster_stats, *target_stats, damage, skill.is_physical);
                                ApplyDamage(target, damage);
                            }
                        }
                    }
                    
                    spdlog::debug("Entity {} used AoE skill {} affecting {} targets",
                                 caster, skill.skill_id, targets.size());
                }
            }
            break;
        }
        
        // TODO: Implement other skill types
        default:
            break;
    }
}

// [SEQUENCE: 16] Update cooldowns
void TargetedCombatSystem::ProcessSkillCooldowns(float delta_time) {
    auto* world = GetWorld();
    if (!world) return;
    
    auto now = std::chrono::steady_clock::now();
    auto entities = world->GetEntitiesWithComponent<SkillComponent>();
    
    for (auto entity : entities) {
        auto* skill_comp = world->GetComponent<SkillComponent>(entity);
        
        // Update cooldowns
        for (auto& [skill_id, cooldown] : skill_comp->cooldowns) {
            if (cooldown.is_on_cooldown && now >= cooldown.ready_time) {
                cooldown.is_on_cooldown = false;
            }
        }
    }
}

// [SEQUENCE: 17] Combat state changes
void TargetedCombatSystem::OnCombatStart(core::ecs::EntityId entity) {
    if (entities_in_combat_.find(entity) == entities_in_combat_.end()) {
        entities_in_combat_.insert(entity);
        last_combat_time_[entity] = std::chrono::steady_clock::now();
        spdlog::debug("Entity {} entered combat", entity);
    }
}

void TargetedCombatSystem::OnCombatEnd(core::ecs::EntityId entity) {
    entities_in_combat_.erase(entity);
    spdlog::debug("Entity {} left combat", entity);
    
    // Start health regeneration
    auto* world = GetWorld();
    if (world) {
        auto* health = world->GetComponent<HealthComponent>(entity);
        if (health && !health->is_dead) {
            health->is_regenerating = true;
        }
    }
}

void TargetedCombatSystem::OnDeath(core::ecs::EntityId entity) {
    spdlog::info("Entity {} died", entity);
    
    // Clear as target for all attackers
    auto* world = GetWorld();
    if (world) {
        auto attackers = world->GetEntitiesWithComponent<TargetComponent>();
        for (auto attacker : attackers) {
            auto* target_comp = world->GetComponent<TargetComponent>(attacker);
            if (target_comp && target_comp->current_target == entity) {
                ClearTarget(attacker);
            }
        }
    }
    
    // Remove from combat
    OnCombatEnd(entity);
}

// [SEQUENCE: 18] Update target validation
void TargetedCombatSystem::UpdateTargetValidation(float delta_time) {
    static float validation_timer = 0.0f;
    validation_timer += delta_time;
    
    if (validation_timer < config_.target_validation_interval) {
        return;
    }
    validation_timer = 0.0f;
    
    auto* world = GetWorld();
    if (!world) return;
    
    auto entities = world->GetEntitiesWithComponent<TargetComponent>();
    for (auto entity : entities) {
        auto* target_comp = world->GetComponent<TargetComponent>(entity);
        if (target_comp->current_target == 0) {
            continue;
        }
        
        // Validate target
        if (!ValidateTarget(entity, target_comp->current_target)) {
            ClearTarget(entity);
        } else {
            // Update range/sight flags
            target_comp->target_in_range = IsInRange(entity, target_comp->current_target,
                                                     target_comp->max_target_range);
            target_comp->target_in_sight = HasLineOfSight(entity, target_comp->current_target);
        }
    }
}

// [SEQUENCE: 19] Cleanup invalid targets
void TargetedCombatSystem::CleanupInvalidTargets() {
    // Handled in UpdateTargetValidation for now
}

// [SEQUENCE: 20] Line of sight check (placeholder)
bool TargetedCombatSystem::HasLineOfSight(core::ecs::EntityId attacker, core::ecs::EntityId target) {
    // TODO: Implement actual line of sight checking with physics/collision
    return true;  // Always true for now
}

bool TargetedCombatSystem::StopAutoAttack(core::ecs::EntityId attacker) {
    auto* world = GetWorld();
    if (!world) return false;
    
    auto* target_comp = world->GetComponent<TargetComponent>(attacker);
    if (!target_comp) return false;
    
    target_comp->auto_attacking = false;
    return true;
}

bool TargetedCombatSystem::CancelCast(core::ecs::EntityId caster) {
    auto* world = GetWorld();
    if (!world) return false;
    
    auto* skill_comp = world->GetComponent<SkillComponent>(caster);
    if (!skill_comp) return false;
    
    if (skill_comp->casting_skill_id != 0) {
        skill_comp->casting_skill_id = 0;
        skill_comp->cast_target = 0;
        spdlog::debug("Entity {} cancelled cast", caster);
        return true;
    }
    
    return false;
}

} // namespace mmorpg::game::systems::combat