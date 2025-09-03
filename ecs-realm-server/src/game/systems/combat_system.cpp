#include "game/systems/combat_system.h"
#include "core/ecs/world.h"
#include "game/components/combat_component.h"
#include "game/components/health_component.h"
#include "game/components/transform_component.h"
#include "game/components/network_component.h"
#include <spdlog/spdlog.h>

namespace mmorpg::game::systems {

// [SEQUENCE: 1] System initialization
void CombatSystem::OnSystemInit() {
    spdlog::info("CombatSystem initialized");
}

// [SEQUENCE: 2] System shutdown
void CombatSystem::OnSystemShutdown() {
    spdlog::info("CombatSystem shutdown");
}

// [SEQUENCE: 3] Main combat update
void CombatSystem::Update(float delta_time) {
    auto* storage = GetComponentStorage();
    if (!storage) return;
    
    auto* combat_storage = storage->GetStorage<components::CombatComponent>();
    if (!combat_storage) return;
    
    // Process all entities with combat components
    for (auto& [entity, combat] : combat_storage->GetAllComponents()) {
        ProcessEntityCombat(entity, delta_time);
    }
}

// [SEQUENCE: 4] Start attack
void CombatSystem::StartAttack(core::ecs::EntityId attacker, core::ecs::EntityId target) {
    auto* combat = world_->GetComponent<components::CombatComponent>(attacker);
    if (!combat) return;
    
    combat->current_target = target;
    combat->is_attacking = true;
    
    spdlog::debug("Entity {} started attacking {}", attacker, target);
}

// [SEQUENCE: 5] Stop attack
void CombatSystem::StopAttack(core::ecs::EntityId attacker) {
    auto* combat = world_->GetComponent<components::CombatComponent>(attacker);
    if (!combat) return;
    
    combat->current_target = 0;
    combat->is_attacking = false;
    
    spdlog::debug("Entity {} stopped attacking", attacker);
}

// [SEQUENCE: 6] Process entity combat state
void CombatSystem::ProcessEntityCombat(core::ecs::EntityId entity, float delta_time) {
    auto* combat = world_->GetComponent<components::CombatComponent>(entity);
    if (!combat || !combat->is_attacking || combat->current_target == 0) {
        return;
    }
    
    // Check if target is still valid
    if (!world_->IsEntityValid(combat->current_target)) {
        StopAttack(entity);
        return;
    }
    
    // Check if target is in range
    if (!IsTargetInRange(entity, combat->current_target)) {
        // Target out of range, could trigger pursuit behavior
        return;
    }
    
    // Check if can attack (cooldown)
    if (combat->CanAttack()) {
        ExecuteAttack(entity, combat->current_target);
        combat->last_attack_time = std::chrono::steady_clock::now();
    }
}

// [SEQUENCE: 7] Execute attack
void CombatSystem::ExecuteAttack(core::ecs::EntityId attacker, core::ecs::EntityId target) {
    auto* attacker_combat = world_->GetComponent<components::CombatComponent>(attacker);
    auto* target_combat = world_->GetComponent<components::CombatComponent>(target);
    
    if (!attacker_combat) return;
    
    // Calculate damage
    float base_damage = attacker_combat->CalculateDamage();
    
    // Apply target's defense if available
    if (target_combat) {
        base_damage = target_combat->CalculateDamageReduction(base_damage);
    }
    
    // Apply damage
    ApplyDamage(target, base_damage);
    
    // Mark for network update
    auto* network = world_->GetComponent<components::NetworkComponent>(target);
    if (network) {
        network->MarkHealthDirty();
    }
    
    spdlog::debug("Entity {} dealt {} damage to {}", attacker, base_damage, target);
}

// [SEQUENCE: 8] Check attack range
bool CombatSystem::IsTargetInRange(core::ecs::EntityId attacker, core::ecs::EntityId target) {
    auto* attacker_transform = world_->GetComponent<components::TransformComponent>(attacker);
    auto* target_transform = world_->GetComponent<components::TransformComponent>(target);
    auto* attacker_combat = world_->GetComponent<components::CombatComponent>(attacker);
    
    if (!attacker_transform || !target_transform || !attacker_combat) {
        return false;
    }
    
    float distance = core::utils::Vector3::Distance(
        attacker_transform->position, 
        target_transform->position
    );
    
    return distance <= attacker_combat->attack_range;
}

// [SEQUENCE: 9] Apply damage to entity
void CombatSystem::ApplyDamage(core::ecs::EntityId target, float damage) {
    auto* health = world_->GetComponent<components::HealthComponent>(target);
    if (!health) return;
    
    float actual_damage = health->TakeDamage(damage);
    
    // Check if entity died
    if (health->is_dead) {
        spdlog::info("Entity {} died", target);
        
        // Mark for removal
        auto* network = world_->GetComponent<components::NetworkComponent>(target);
        if (network) {
            network->needs_removal = true;
        }
        
        // Could trigger death events, loot drops, etc.
    }
}

} // namespace mmorpg::game::systems