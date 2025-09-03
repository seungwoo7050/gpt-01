#include "game/systems/movement_system.h"
#include "core/ecs/world.h"
#include <spdlog/spdlog.h>

namespace mmorpg::game::systems {

// [SEQUENCE: 1] System initialization
void MovementSystem::OnSystemInit() {
    spdlog::info("MovementSystem initialized");
}

// [SEQUENCE: 2] System shutdown
void MovementSystem::OnSystemShutdown() {
    spdlog::info("MovementSystem shutdown");
}

// [SEQUENCE: 3] Main update loop
void MovementSystem::Update(float delta_time) {
    auto* storage = GetComponentStorage();
    if (!storage) return;
    
    // Get transform storage
    auto* transform_storage = storage->GetStorage<components::TransformComponent>();
    auto* velocity_storage = storage->GetStorage<components::VelocityComponent>();
    
    if (!transform_storage || !velocity_storage) return;
    
    // Process entities with both components
    for (auto& [entity, velocity] : velocity_storage->GetAllComponents()) {
        auto* transform = transform_storage->GetComponent(entity);
        if (transform) {
            UpdateEntityMovement(entity, *transform, velocity, delta_time);
        }
    }
}

// [SEQUENCE: 4] Update individual entity movement
void MovementSystem::UpdateEntityMovement(
    core::ecs::EntityId entity,
    components::TransformComponent& transform,
    components::VelocityComponent& velocity,
    float delta_time) {
    
    // Clamp velocity to max speed
    ClampVelocity(velocity);
    
    // Update position based on velocity
    core::utils::Vector3 delta_pos = velocity.linear * delta_time;
    transform.position += delta_pos;
    
    // Update rotation based on angular velocity
    core::utils::Vector3 delta_rot = velocity.angular * delta_time;
    transform.rotation += delta_rot;
    
    // Keep rotation in valid range
    if (transform.rotation.x > 3.14159f) transform.rotation.x -= 6.28318f;
    if (transform.rotation.x < -3.14159f) transform.rotation.x += 6.28318f;
    if (transform.rotation.y > 3.14159f) transform.rotation.y -= 6.28318f;
    if (transform.rotation.y < -3.14159f) transform.rotation.y += 6.28318f;
    if (transform.rotation.z > 3.14159f) transform.rotation.z -= 6.28318f;
    if (transform.rotation.z < -3.14159f) transform.rotation.z += 6.28318f;
}

// [SEQUENCE: 5] Apply velocity constraints
void MovementSystem::ClampVelocity(components::VelocityComponent& velocity) {
    float speed = velocity.linear.Length();
    if (speed > velocity.max_speed) {
        velocity.linear = velocity.linear.Normalized() * velocity.max_speed;
    }
}

} // namespace mmorpg::game::systems