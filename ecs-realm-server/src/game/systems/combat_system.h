#pragma once

#include "core/ecs/system.h"

namespace mmorpg::game::systems {

// [SEQUENCE: 1] Combat system handles attack resolution and damage
class CombatSystem : public core::ecs::System {
public:
    CombatSystem() : System("CombatSystem") {}
    
    // [SEQUENCE: 2] System lifecycle
    void OnSystemInit() override;
    void OnSystemShutdown() override;
    
    // [SEQUENCE: 3] Update combat state
    void Update(float delta_time) override;
    
    // [SEQUENCE: 4] System metadata
    core::ecs::SystemStage GetStage() const override { 
        return core::ecs::SystemStage::UPDATE; 
    }
    int GetPriority() const override { return 200; } // After movement
    
    // [SEQUENCE: 5] Combat actions
    void StartAttack(core::ecs::EntityId attacker, core::ecs::EntityId target);
    void StopAttack(core::ecs::EntityId attacker);
    
private:
    // [SEQUENCE: 6] Process single entity combat
    void ProcessEntityCombat(core::ecs::EntityId entity, float delta_time);
    
    // [SEQUENCE: 7] Execute attack
    void ExecuteAttack(core::ecs::EntityId attacker, core::ecs::EntityId target);
    
    // [SEQUENCE: 8] Check if target is in range
    bool IsTargetInRange(core::ecs::EntityId attacker, core::ecs::EntityId target);
    
    // [SEQUENCE: 9] Apply damage to target
    void ApplyDamage(core::ecs::EntityId target, float damage);
};

} // namespace mmorpg::game::systems