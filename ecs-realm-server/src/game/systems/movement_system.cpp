#include "game/systems/movement_system.h"
#include "core/ecs/world.h"
#include <spdlog/spdlog.h>

namespace mmorpg::game::systems {

// [SEQUENCE: MVP2-22] Updates entity positions based on their TransformComponent and VelocityComponent.
void MovementSystem::OnSystemInit() {
    spdlog::info("MovementSystem initialized");
}

void MovementSystem::OnSystemShutdown() {
    spdlog::info("MovementSystem shutdown");
}

void MovementSystem::Update(float delta_time) {
    for (const auto& entity : entities_) {
        auto& transform = world_->GetComponent<components::TransformComponent>(entity);
        auto& velocity = world_->GetComponent<components::VelocityComponent>(entity);

        UpdateEntityMovement(entity, transform, velocity, delta_time);
    }
}

void MovementSystem::UpdateEntityMovement(
    core::ecs::EntityId entity,
    components::TransformComponent& transform,
    components::VelocityComponent& velocity,
    float delta_time) {
    
    // Clamp velocity to max speed
    ClampVelocity(velocity);
    
    // Update position based on velocity
    transform.position = transform.position + (velocity.linear * delta_time);
    
    // Update rotation based on angular velocity
    transform.rotation = transform.rotation + (velocity.angular * delta_time);
}

void MovementSystem::ClampVelocity(components::VelocityComponent& velocity) {
    float speed = velocity.linear.Length();
    if (speed > velocity.max_speed) {
        velocity.linear = velocity.linear.Normalized() * velocity.max_speed;
    }
}

} // namespace mmorpg::game::systems