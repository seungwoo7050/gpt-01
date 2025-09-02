#include "combat_system.h"
#include "core/ecs/world.h"
#include "game/components/transform_component.h"
#include "game/components/combat_stats_component.h"
#include "game/components/health_component.h"
#include "game/components/target_component.h"

namespace mmorpg::game::systems {

using namespace core::ecs;
using namespace game::components;

// [SEQUENCE: MVP9-10] System initialization
void CombatSystem::OnSystemInit() {
    // Subscribe to events if needed
}

// [SEQUENCE: MVP9-11] System shutdown
void CombatSystem::OnSystemShutdown() {
    // Cleanup
}

// [SEQUENCE: MVP9-12] Update all entities with combat logic
void CombatSystem::Update(float delta_time) {
    auto& world = World::Instance();
    // Iterate over all entities that have a TargetComponent
    for (EntityId entity : world.GetEntitiesWith<TargetComponent>()) {
        ProcessEntityCombat(entity, delta_time);
    }
}

// [SEQUENCE: MVP9-13] Process combat for a single entity
void CombatSystem::ProcessEntityCombat(EntityId entity, float delta_time) {
    auto& world = World::Instance();
    auto& target_comp = world.GetComponent<TargetComponent>(entity);

    if (target_comp.auto_attacking && target_comp.current_target != 0) {
        // Update auto-attack timer
        target_comp.next_auto_attack_time -= std::chrono::duration<float>(delta_time);
        if (target_comp.next_auto_attack_time.count() <= 0) {
            ExecuteAttack(entity, target_comp.current_target);
            
            // Reset timer
            auto& stats = world.GetComponent<CombatStatsComponent>(entity);
            target_comp.next_auto_attack_time = std::chrono::duration<float>(1.0f / stats.attack_speed);
        }
    }
}

// [SEQUENCE: MVP9-14] Start an attack
void CombatSystem::StartAttack(EntityId attacker, EntityId target) {
    auto& world = World::Instance();
    if (world.HasComponent<TargetComponent>(attacker)) {
        auto& target_comp = world.GetComponent<TargetComponent>(attacker);
        target_comp.current_target = target;
        target_comp.auto_attacking = true;
    }
}

// [SEQUENCE: MVP9-15] Stop an attack
void CombatSystem::StopAttack(EntityId attacker) {
    auto& world = World::Instance();
    if (world.HasComponent<TargetComponent>(attacker)) {
        auto& target_comp = world.GetComponent<TargetComponent>(attacker);
        target_comp.auto_attacking = false;
    }
}

// [SEQUENCE: MVP9-16] Execute a single attack
void CombatSystem::ExecuteAttack(EntityId attacker, EntityId target) {
    if (!IsTargetInRange(attacker, target)) {
        return;
    }
    
    auto& world = World::Instance();
    auto& attacker_stats = world.GetComponent<CombatStatsComponent>(attacker);
    
    // Simplified damage calculation for now
    float damage = attacker_stats.attack_power;
    
    ApplyDamage(target, damage);
}

// [SEQUENCE: MVP9-17] Check attack range
bool CombatSystem::IsTargetInRange(EntityId attacker, EntityId target) {
    auto& world = World::Instance();
    auto& t1 = world.GetComponent<TransformComponent>(attacker);
    auto& t2 = world.GetComponent<TransformComponent>(target);
    
    float dx = t1.x - t2.x;
    float dy = t1.y - t2.y;
    float dist_sq = dx * dx + dy * dy;
    
    auto& target_comp = world.GetComponent<TargetComponent>(attacker);
    return dist_sq <= (target_comp.auto_attack_range * target_comp.auto_attack_range);
}

// [SEQUENCE: MVP9-18] Apply damage to a target
void CombatSystem::ApplyDamage(EntityId target, float damage) {
    auto& world = World::Instance();
    if (world.HasComponent<HealthComponent>(target)) {
        auto& health = world.GetComponent<HealthComponent>(target);
        health.current_health -= damage;
        
        if (health.current_health <= 0) {
            // Handle death
            health.current_health = 0;
        }
    }
}

} // namespace mmorpg::game::systems