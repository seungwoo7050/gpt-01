#pragma once

#include "core/ecs/system.h"
#include "game/components/transform_component.h"
#include "game/components/velocity_component.h"

namespace mmorpg::game::systems {

// [SEQUENCE: MVP2-22] Updates entity positions based on their TransformComponent and VelocityComponent.
class MovementSystem : public core::ecs::System {
public:
    MovementSystem() = default;
    
    void OnSystemInit() override;
    void OnSystemShutdown() override;
    void Update(float delta_time) override;
    
private:
    void UpdateEntityMovement(
        core::ecs::EntityId entity,
        components::TransformComponent& transform,
        components::VelocityComponent& velocity,
        float delta_time
    );
    
    void ClampVelocity(components::VelocityComponent& velocity);
};

} // namespace mmorpg::game::systems