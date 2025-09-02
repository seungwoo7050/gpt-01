#include "game/systems/optimized/optimized_movement_system.h"
#include <spdlog/spdlog.h>
#include <cmath>

namespace mmorpg::game::systems::optimized {

// [SEQUENCE: MVP2-26] A data-oriented version of the movement system.
void OptimizedMovementSystem::OnSystemInit() {
    optimized_world_ = reinterpret_cast<core::ecs::optimized::OptimizedWorld*>(world_);
    if (!optimized_world_) {
        spdlog::error("OptimizedMovementSystem requires OptimizedWorld!");
        return;
    }
    
    transform_array_ = optimized_world_->GetComponentArray<components::TransformComponent>().get();
    velocity_array_ = optimized_world_->GetComponentArray<components::VelocityComponent>().get();
    
    spdlog::info("OptimizedMovementSystem initialized");
}

void OptimizedMovementSystem::OnSystemShutdown() {
    transform_array_ = nullptr;
    velocity_array_ = nullptr;
    optimized_world_ = nullptr;
    spdlog::info("OptimizedMovementSystem shutdown");
}

void OptimizedMovementSystem::Update(float delta_time) {
    if (!transform_array_ || !velocity_array_) {
        return;
    }
    
    size_t count = velocity_array_->GetSize();
    if (count == 0) return;

    auto& velocities = velocity_array_->GetComponentDataArray();
    auto& entity_map = velocity_array_->GetEntityMap();

    for (size_t i = 0; i < count; ++i) {
        auto entity = entity_map[i];
        if (transform_array_->HasEntity(entity)) {
            auto& velocity = velocities[i];
            auto& transform = transform_array_->GetData(entity);

            // Clamp velocity
            float speed_sq = velocity.linear.x * velocity.linear.x + 
                            velocity.linear.y * velocity.linear.y + 
                            velocity.linear.z * velocity.linear.z;
            float max_speed_sq = velocity.max_speed * velocity.max_speed;
            if (speed_sq > max_speed_sq) {
                float scale = velocity.max_speed / std::sqrt(speed_sq);
                velocity.linear.x *= scale;
                velocity.linear.y *= scale;
                velocity.linear.z *= scale;
            }

            // Update position
            transform.position = transform.position + (velocity.linear * delta_time);
            transform.rotation = transform.rotation + (velocity.angular * delta_time);
        }
    }
}

// The batch processing methods are not needed with this simpler approach
void OptimizedMovementSystem::ProcessBatch(size_t start, size_t end, float delta_time) {}
void OptimizedMovementSystem::ClampVelocityBatch(components::VelocityComponent* velocities, size_t count) {}

} // namespace mmorpg::game::systems::optimized
