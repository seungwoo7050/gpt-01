#include "game/systems/combat/action_combat_system.h"
#include "game/systems/spatial_indexing_system.h"
#include "core/ecs/world.h"
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>

namespace mmorpg::game::systems::combat {

// [SEQUENCE: MVP4-28] Initialize action combat system
void ActionCombatSystem::OnSystemInit() {
    // Find spatial system for collision detection
    optimized_world_ = reinterpret_cast<core::ecs::optimized::OptimizedWorld*>(world_);
    if (optimized_world_) {
        spatial_system_ = reinterpret_cast<GridSpatialSystem*>(optimized_world_->GetSystem<GridSpatialSystem>().get());
    }
    
    spdlog::info("ActionCombatSystem initialized");
}

void ActionCombatSystem::OnSystemShutdown() {
    projectiles_.clear();
    dodge_states_.clear();
    hit_records_.clear();
    spdlog::info("ActionCombatSystem shut down");
}

// [SEQUENCE: MVP4-29] Main update loop
void ActionCombatSystem::Update(float delta_time) {
    UpdateProjectiles(delta_time);
    UpdateDodgeStates(delta_time);

    for (const auto& entity : entities_) {
        // Process skill casts for each entity
        ProcessSkillCasts(delta_time, entity);
        // Process skill cooldowns for each entity
        ProcessSkillCooldowns(delta_time, entity);
    }

    // Clean expired hit records
    auto now = std::chrono::steady_clock::now();
    for (auto it = hit_records_.begin(); it != hit_records_.end();) {
        if (now > it->second.expire_time) {
            it = hit_records_.erase(it);
        } else {
            ++it;
        }
    }
}

// [SEQUENCE: MVP4-30] Use skillshot and area skills
bool ActionCombatSystem::UseSkillshot(core::ecs::EntityId caster, uint32_t skill_id,
                                     const core::utils::Vector3& direction) {
    auto* world = world_;
    if (!world) return false;
    
    // Get components
    auto* skill_comp = world->GetComponent<SkillComponent>(caster);
    auto* transform_comp = world->GetComponent<TransformComponent>(caster);
    auto* health_comp = world->GetComponent<HealthComponent>(caster);
    
    if (!skill_comp || !transform_comp || !health_comp) return false;
    
    // Find skill
    auto skill_it = skill_comp->skills.find(skill_id);
    if (skill_it == skill_comp->skills.end()) {
        return false;
    }
    
    const Skill& skill = skill_it->second;
    if (skill.type != SkillType::SKILLSHOT) {
        return false;
    }
    
    // Check cooldowns and resources (same as targeted)
    auto now = std::chrono::steady_clock::now();
    auto cd_it = skill_comp->cooldowns.find(skill_id);
    if (cd_it != skill_comp->cooldowns.end() && cd_it->second.is_on_cooldown) {
        if (now < cd_it->second.ready_time) {
            return false;
        }
    }
    
    if (now < skill_comp->global_cooldown_end) {
        return false;
    }
    
    if (skill.resource_type == ResourceType::MANA && health_comp->current_mana < skill.resource_cost) {
        return false;
    }
    
    // Normalize direction
    core::utils::Vector3 norm_dir = direction;
    float length = std::sqrt(norm_dir.x * norm_dir.x + norm_dir.y * norm_dir.y + norm_dir.z * norm_dir.z);
    if (length > 0) {
        norm_dir.x /= length;
        norm_dir.y /= length;
        norm_dir.z /= length;
    }
    
    // Create projectile
    CreateProjectile(caster, transform_comp->position, norm_dir, skill);
    
    // Deduct resources and set cooldowns
    health_comp->current_mana -= skill.resource_cost;
    
    auto& cooldown = skill_comp->cooldowns[skill_id];
    cooldown.is_on_cooldown = true;
    cooldown.ready_time = now + std::chrono::milliseconds(static_cast<int>(skill.cooldown_duration * 1000));
    
    skill_comp->global_cooldown_end = now + 
        std::chrono::milliseconds(static_cast<int>(skill_comp->global_cooldown_duration * 1000));
    
    spdlog::debug("Entity {} used skillshot {} in direction ({}, {}, {})",
                 caster, skill_id, norm_dir.x, norm_dir.y, norm_dir.z);
    
    return true;
}

bool ActionCombatSystem::UseAreaSkill(core::ecs::EntityId caster, uint32_t skill_id,
                                     const core::utils::Vector3& target_position) {
    auto* world = world_;
    if (!world) return false;
    
    // Get components
    auto* skill_comp = world->GetComponent<SkillComponent>(caster);
    auto* transform_comp = world->GetComponent<TransformComponent>(caster);
    auto* health_comp = world->GetComponent<HealthComponent>(caster);
    auto* stats_comp = world->GetComponent<CombatStatsComponent>(caster);
    
    if (!skill_comp || !transform_comp || !health_comp || !stats_comp) return false;
    
    // Find skill
    auto skill_it = skill_comp->skills.find(skill_id);
    if (skill_it == skill_comp->skills.end()) {
        return false;
    }
    
    const Skill& skill = skill_it->second;
    if (skill.type != SkillType::AREA_OF_EFFECT) {
        return false;
    }
    
    // Check range to target position
    float dx = target_position.x - transform_comp->position.x;
    float dy = target_position.y - transform_comp->position.y;
    float distance = std::sqrt(dx * dx + dy * dy);
    
    if (distance > skill.range) {
        return false;
    }
    
    // Check cooldowns and resources
    auto now = std::chrono::steady_clock::now();
    auto cd_it = skill_comp->cooldowns.find(skill_id);
    if (cd_it != skill_comp->cooldowns.end() && cd_it->second.is_on_cooldown) {
        if (now < cd_it->second.ready_time) {
            return false;
        }
    }
    
    if (now < skill_comp->global_cooldown_end) {
        return false;
    }
    
    if (skill.resource_type == ResourceType::MANA && health_comp->current_mana < skill.resource_cost) {
        return false;
    }
    
    // Execute AoE immediately
    if (spatial_system_) {
        auto targets = spatial_system_->GetEntitiesInRadius(target_position, skill.radius);
        
        for (auto target : targets) {
            if (target == caster) continue;  // Don't hit self
            
            if (!IsValidTarget(caster, target)) continue;
            
            auto* target_stats = world->GetComponent<CombatStatsComponent>(target);
            if (target_stats && skill.base_damage > 0) {
                float damage = skill.base_damage * skill.damage_coefficient;
                damage = CalculateDamage(*stats_comp, *target_stats, damage, skill.is_physical);
                ApplyDamage(target, damage, caster);
            }
        }
        
        spdlog::debug("Entity {} used AoE skill {} at ({}, {}, {}) hitting {} targets",
                     caster, skill_id, target_position.x, target_position.y, 
                     target_position.z, targets.size());
    }
    
    // Deduct resources and set cooldowns
    health_comp->current_mana -= skill.resource_cost;
    
    auto& cooldown = skill_comp->cooldowns[skill_id];
    cooldown.is_on_cooldown = true;
    cooldown.ready_time = now + std::chrono::milliseconds(static_cast<int>(skill.cooldown_duration * 1000));
    
    skill_comp->global_cooldown_end = now + 
        std::chrono::milliseconds(static_cast<int>(skill_comp->global_cooldown_duration * 1000));
    
    return true;
}

bool ActionCombatSystem::UseMeleeSwing(core::ecs::EntityId attacker,
                                      const core::utils::Vector3& direction,
                                      float arc_angle) {
    auto* world = world_;
    if (!world) return false;
    
    auto* transform_comp = world->GetComponent<TransformComponent>(attacker);
    auto* stats_comp = world->GetComponent<CombatStatsComponent>(attacker);
    
    if (!transform_comp || !stats_comp) return false;
    
    // Get entities in cone
    auto targets = GetEntitiesInCone(transform_comp->position, direction, 
                                    config_.melee_range, arc_angle);
    
    int hits = 0;
    for (auto target : targets) {
        if (target == attacker) continue;
        if (!IsValidTarget(attacker, target)) continue;
        
        auto* target_stats = world->GetComponent<CombatStatsComponent>(target);
        if (target_stats) {
            float base_damage = 15.0f;  // Base melee damage
            float damage = CalculateDamage(*stats_comp, *target_stats, base_damage, true);
            
            if (ApplyDamage(target, damage, attacker)) {
                hits++;
            }
        }
    }
    
    spdlog::debug("Entity {} performed melee swing hitting {} targets", attacker, hits);
    return hits > 0;
}

// [SEQUENCE: MVP4-31] Create and update projectiles
core::ecs::EntityId ActionCombatSystem::CreateProjectile(core::ecs::EntityId owner,
                                                        const core::utils::Vector3& origin,
                                                        const core::utils::Vector3& direction,
                                                        const components::Skill& skill) {
    auto* world = world_;
    if (!world) return 0;
    
    // Create projectile entity
    auto projectile = world->CreateEntity();
    
    // Add transform
    components::TransformComponent transform;
    transform.position = origin;
    transform.rotation = direction;  // Store direction in rotation
    world->AddComponent(projectile, transform);
    
    // Create projectile data
    components::ProjectileComponent proj_comp;
    proj_comp.owner = owner;
    proj_comp.velocity = direction;
    proj_comp.speed = 20.0f;  // Default projectile speed
    proj_comp.range = skill.range;
    proj_comp.traveled = 0.0f;
    proj_comp.damage = skill.base_damage * skill.damage_coefficient;
    proj_comp.radius = skill.radius > 0 ? skill.radius : 1.0f;  // Collision radius
    proj_comp.is_physical = skill.is_physical;
    proj_comp.piercing = false;  // TODO: Add to skill definition
    proj_comp.skill_id = skill.skill_id;
    
    // Scale velocity by speed
    proj_comp.velocity.x *= proj_comp.speed;
    proj_comp.velocity.y *= proj_comp.speed;
    proj_comp.velocity.z *= proj_comp.speed;
    
    world->AddComponent(projectile, proj_comp);
    
    // Create hit record to prevent multi-hits
    uint64_t hit_key = (static_cast<uint64_t>(projectile) << 32) | skill.skill_id;
    hit_records_[hit_key] = {
        std::unordered_set<core::ecs::EntityId>(),
        std::chrono::steady_clock::now() + std::chrono::seconds(10)
    };
    
    return projectile;
}

std::vector<core::ecs::EntityId> ActionCombatSystem::GetEntitiesInCone(
    const core::utils::Vector3& origin,
    const core::utils::Vector3& direction,
    float range, float angle) {
    
    std::vector<core::ecs::EntityId> results;
    
    if (!spatial_system_) return results;
    
    // Get all entities in range
    auto candidates = spatial_system_->GetEntitiesInRadius(origin, range);
    
    // Filter by cone angle
    float half_angle = angle * 0.5f * (M_PI / 180.0f);  // Convert to radians
    
    auto* world = world_;
    if (!world) return results;
    
    for (auto entity : candidates) {
        auto* transform = world->GetComponent<TransformComponent>(entity);
        if (!transform) continue;
        
        // Calculate direction to entity
        core::utils::Vector3 to_entity;
        to_entity.x = transform->position.x - origin.x;
        to_entity.y = transform->position.y - origin.y;
        to_entity.z = transform->position.z - origin.z;
        
        // Normalize
        float length = std::sqrt(to_entity.x * to_entity.x + 
                               to_entity.y * to_entity.y + 
                               to_entity.z * to_entity.z);
        if (length > 0) {
            to_entity.x /= length;
            to_entity.y /= length;
            to_entity.z /= length;
        }
        
        // Check angle
        float dot = direction.x * to_entity.x + 
                   direction.y * to_entity.y + 
                   direction.z * to_entity.z;
        float entity_angle = std::acos(std::clamp(dot, -1.0f, 1.0f));
        
        if (entity_angle <= half_angle) {
            results.push_back(entity);
        }
    }
    
    return results;
}

// [SEQUENCE: MVP4-32] Dodge roll implementation
bool ActionCombatSystem::DodgeRoll(core::ecs::EntityId entity, const core::utils::Vector3& direction) {
    auto* world = world_;
    if (!world) return false;
    
    if (world->HasComponent<components::DodgeComponent>(entity)) {
        // Already dodging
        return false;
    }

    // Add dodge component
    components::DodgeComponent dodge_comp;
    dodge_comp.is_dodging = true;
    dodge_comp.dodge_end_time = std::chrono::steady_clock::now() + std::chrono::milliseconds(
        static_cast<int>(config_.dodge_duration * 1000)
    );
    dodge_comp.dodge_direction = direction;
    world->AddComponent(entity, dodge_comp);
    
    // Move entity
    auto* transform = world->GetComponent<components::TransformComponent>(entity);
    if (transform) {
        transform->position.x += direction.x * config_.dodge_distance;
        transform->position.y += direction.y * config_.dodge_distance;
        // Don't change Z for ground-based dodge
    }
    
    OnDodge(entity);
    return true;
}

bool ActionCombatSystem::IsInvulnerable(core::ecs::EntityId entity) {
    auto it = dodge_states_.find(entity);
    if (it != dodge_states_.end()) {
        return it->second.is_dodging;
    }
    return false;
}

// [SEQUENCE: MVP4-33] Calculate and apply damage
float ActionCombatSystem::CalculateDamage(const CombatStatsComponent& attacker_stats,
                                         const CombatStatsComponent& defender_stats,
                                         float base_damage, bool is_physical) {
    float damage = base_damage;
    
    // Apply attack/spell power
    if (is_physical) {
        damage *= (1.0f + attacker_stats.attack_power / 100.0f);
        
        // Apply armor reduction
        float armor_reduction = defender_stats.armor * 0.01f;  // 1% per armor
        armor_reduction = std::min(armor_reduction, 0.75f);
        damage *= (1.0f - armor_reduction);
    } else {
        damage *= (1.0f + attacker_stats.spell_power / 100.0f);
        
        // Apply magic resistance
        float resist_reduction = defender_stats.magic_resist * 0.01f;
        resist_reduction = std::min(resist_reduction, 0.75f);
        damage *= (1.0f - resist_reduction);
    }
    
    // Check for dodge
    if (IsInvulnerable(defender_stats.level)) {  // Using level as entity ID (hack)
        return 0.0f;
    }
    
    // Check for critical hit
    float crit_roll = static_cast<float>(rand()) / RAND_MAX;
    if (crit_roll < attacker_stats.critical_chance) {
        damage *= attacker_stats.critical_damage;
    }
    
    // Apply modifiers
    damage *= (1.0f + attacker_stats.damage_increase);
    damage *= (1.0f - defender_stats.damage_reduction);
    
    return std::max(1.0f, damage);
}

bool ActionCombatSystem::ApplyDamage(core::ecs::EntityId target, float damage,
                                    core::ecs::EntityId source) {
    if (IsInvulnerable(target)) {
        return false;
    }
    
    auto* world = world_;
    if (!world) return false;
    
    auto* health = world->GetComponent<HealthComponent>(target);
    if (!health || health->is_dead) {
        return false;
    }
    
    health->current_health -= damage;
    health->last_damage_time = std::chrono::steady_clock::now();
    
    if (health->current_health <= 0) {
        health->current_health = 0;
        health->is_dead = true;
        OnDeath(target);
    }
    
    // Check if critical
    bool is_critical = false;  // TODO: Pass from damage calculation
    OnHit(source, target, damage, is_critical);
    
    return true;
}

void ActionCombatSystem::UpdateProjectiles(float delta_time) {
    auto* world = world_;
    if (!world) return;

    std::vector<core::ecs::EntityId> to_remove;

    for (const auto& entity : entities_) {
        if (!world->HasComponent<components::ProjectileComponent>(entity)) {
            continue;
        }

        auto& proj = world->GetComponent<components::ProjectileComponent>(entity);
        auto& transform = world->GetComponent<components::TransformComponent>(entity);

        // Move projectile
        transform.position.x += proj.velocity.x * delta_time;
        transform.position.y += proj.velocity.y * delta_time;
        transform.position.z += proj.velocity.z * delta_time;

        // Update distance traveled
        float dist = proj.speed * delta_time;
        proj.traveled += dist;

        // Check if exceeded range
        if (proj.traveled >= proj.range) {
            to_remove.push_back(entity);
            continue;
        }

        // Check collisions
        CheckProjectileCollisions(entity, proj, transform.position);
    }

    // Remove expired projectiles
    for (auto id : to_remove) {
        world->DestroyEntity(id);
    }
}


void ActionCombatSystem::CheckProjectileCollisions(core::ecs::EntityId projectile,
                                                  const ProjectileComponent& proj,
                                                  const core::utils::Vector3& position) {
    if (!spatial_system_) return;
    
    auto* world = world_;
    if (!world) return;
    
    // Get hit record
    uint64_t hit_key = (static_cast<uint64_t>(projectile) << 32) | proj.skill_id;
    auto hit_it = hit_records_.find(hit_key);
    if (hit_it == hit_records_.end()) return;
    
    // Check entities near projectile
    auto nearby = spatial_system_->GetEntitiesInRadius(position, proj.radius + config_.hitbox_padding);
    
    auto* owner_stats = world->GetComponent<CombatStatsComponent>(proj.owner);
    if (!owner_stats) return;
    
    bool hit_something = false;
    
    for (auto entity : nearby) {
        if (entity == proj.owner) continue;
        if (!IsValidTarget(proj.owner, entity)) continue;
        
        // Check if already hit
        if (hit_it->second.hit_entities.find(entity) != hit_it->second.hit_entities.end()) {
            continue;
        }
        
        // Apply damage
        auto* target_stats = world->GetComponent<CombatStatsComponent>(entity);
        if (target_stats) {
            float damage = CalculateDamage(*owner_stats, *target_stats, proj.damage, proj.is_physical);
            
            if (ApplyDamage(entity, damage, proj.owner)) {
                hit_it->second.hit_entities.insert(entity);
                hit_something = true;
                
                if (!proj.piercing) {
                    // Destroy projectile on hit
                    projectiles_.erase(projectile);
                    world->DestroyEntity(projectile);
                    return;
                }
            }
        }
    }
}

void ActionCombatSystem::ProcessSkillCasts(float delta_time, core::ecs::EntityId entity) {
    auto* world = world_;
    if (!world) return;
    
    auto now = std::chrono::steady_clock::now();
    
    auto* skill_comp = world->GetComponent<components::SkillComponent>(entity);
    
    // Check if casting
    if (skill_comp->casting_skill_id == 0) {
        return;
    }
    
    // Check if cast complete
    if (now >= skill_comp->cast_end_time) {
        // For action combat, we need direction or position stored
        // This would require extending SkillComponent
        skill_comp->casting_skill_id = 0;
    }
}

void ActionCombatSystem::ProcessSkillCooldowns(float delta_time, core::ecs::EntityId entity) {
    auto* world = world_;
    if (!world) return;
    
    auto now = std::chrono::steady_clock::now();
    
    auto* skill_comp = world->GetComponent<components::SkillComponent>(entity);
    
    // Update cooldowns
    for (auto& [skill_id, cooldown] : skill_comp->cooldowns) {
        if (cooldown.is_on_cooldown && now >= cooldown.ready_time) {
            cooldown.is_on_cooldown = false;
        }
    }
}

void ActionCombatSystem::UpdateDodgeStates(float delta_time) {
    auto now = std::chrono::steady_clock::now();
    auto* world = world_;
    if (!world) return;

    for (const auto& entity : entities_) {
        if (!world->HasComponent<components::DodgeComponent>(entity)) {
            continue;
        }

        auto& dodge = world->GetComponent<components::DodgeComponent>(entity);
        if (dodge.is_dodging && now >= dodge.dodge_end_time) {
            world->RemoveComponent<components::DodgeComponent>(entity);
            spdlog::debug("Entity {} dodge ended", entity);
        }
    }
}

void ActionCombatSystem::RechargeDodges(float delta_time) {
    static float recharge_timer = 0.0f;
    recharge_timer += delta_time;
    
    if (recharge_timer >= 1.0f) {  // Check every second
        recharge_timer = 0.0f;
        
        for (auto& [entity, dodge] : dodge_states_) {
            if (dodge.dodge_charges < 2) {  // Max 2 charges
                dodge.dodge_charges++;
                spdlog::debug("Entity {} recharged dodge (charges: {})", 
                            entity, dodge.dodge_charges);
            }
        }
    }
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
    
    // Remove from dodge tracking
    dodge_states_.erase(entity);
}

bool ActionCombatSystem::IsValidTarget(core::ecs::EntityId attacker, core::ecs::EntityId target) {
    auto* world = world_;
    if (!world) return false;
    
    if (!world->IsEntityAlive(target)) {
        return false;
    }
    
    auto* health = world->GetComponent<HealthComponent>(target);
    if (!health || health->is_dead) {
        return false;
    }
    
    // TODO: Check faction, friendly fire, etc.
    return true;
}

std::vector<core::ecs::EntityId> ActionCombatSystem::CheckProjectilePath(
    const core::utils::Vector3& origin,
    const core::utils::Vector3& direction,
    float range, float width) {
    
    // TODO: Implement ray-cast with width
    // For now, use cone approximation
    return GetEntitiesInCone(origin, direction, range, 15.0f);  // 15 degree cone
}

float ActionCombatSystem::GetAngleBetween(const core::utils::Vector3& v1, 
                                         const core::utils::Vector3& v2) {
    float dot = v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
    return std::acos(std::clamp(dot, -1.0f, 1.0f));
}

core::utils::Vector3 ActionCombatSystem::RotateVector(const core::utils::Vector3& vec, float angle) {
    // Rotate around Z axis
    float cos_a = std::cos(angle);
    float sin_a = std::sin(angle);
    
    core::utils::Vector3 result;
    result.x = vec.x * cos_a - vec.y * sin_a;
    result.y = vec.x * sin_a + vec.y * cos_a;
    result.z = vec.z;
    
    return result;
}

} // namespace mmorpg::game::systems::combat
at
    
    return result;
}

} // namespace mmorpg::game::systems::combat
