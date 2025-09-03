#pragma once

#include "core/ecs/system.h"
#include "game/components/transform_component.h"
#include "game/components/velocity_component.h"

namespace mmorpg::game::systems {

// [SEQUENCE: 1] Movement system updates entity positions based on velocity
class MovementSystem : public core::ecs::System {
public:
    MovementSystem() : System("MovementSystem") {}
    
    // [SEQUENCE: 2] System lifecycle
    void OnSystemInit() override;
    void OnSystemShutdown() override;
    
    // [SEQUENCE: 3] Main update logic
    void Update(float delta_time) override;
    
    // [SEQUENCE: 4] System metadata
    core::ecs::SystemStage GetStage() const override { 
        return core::ecs::SystemStage::UPDATE; 
    }
    int GetPriority() const override { return 100; } // Early in update
    
private:
    // [SEQUENCE: 5] Update single entity movement
    void UpdateEntityMovement(
        core::ecs::EntityId entity,
        components::TransformComponent& transform,
        components::VelocityComponent& velocity,
        float delta_time
    );
    
    // [SEQUENCE: 6] Apply velocity limits
    void ClampVelocity(components::VelocityComponent& velocity);
};

} // namespace mmorpg::game::systems