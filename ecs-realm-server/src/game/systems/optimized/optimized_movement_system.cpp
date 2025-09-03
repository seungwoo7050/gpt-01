#include "game/systems/optimized/optimized_movement_system.h"
#include <spdlog/spdlog.h>
#include <cmath>

namespace mmorpg::game::systems::optimized {

// [SEQUENCE: 1] System initialization - cache component arrays
void OptimizedMovementSystem::OnSystemInit() {
    // Cast world to optimized version
    optimized_world_ = dynamic_cast<core::ecs::optimized::OptimizedWorld*>(world_);
    if (!optimized_world_) {
        spdlog::error("OptimizedMovementSystem requires OptimizedWorld!");
        return;
    }
    
    // Cache component arrays
    transform_array_ = optimized_world_->GetComponentArray<components::TransformComponent>();
    velocity_array_ = optimized_world_->GetComponentArray<components::VelocityComponent>();
    
    spdlog::info("OptimizedMovementSystem initialized");
}

// [SEQUENCE: 2] System shutdown
void OptimizedMovementSystem::OnSystemShutdown() {
    transform_array_ = nullptr;
    velocity_array_ = nullptr;
    optimized_world_ = nullptr;
    spdlog::info("OptimizedMovementSystem shutdown");
}

// [SEQUENCE: 3] Optimized update with direct array iteration
void OptimizedMovementSystem::Update(float delta_time) {
    if (!transform_array_ || !velocity_array_) {
        return;
    }
    
    // Get velocity component count (limiting factor)
    size_t velocity_count = velocity_array_->GetSize();
    if (velocity_count == 0) return;
    
    // Process in cache-friendly batches
    constexpr size_t BATCH_SIZE = 64; // Typical cache line count
    
    for (size_t i = 0; i < velocity_count; i += BATCH_SIZE) {
        size_t batch_end = std::min(i + BATCH_SIZE, velocity_count);
        ProcessBatch(i, batch_end, delta_time);
    }
}

// [SEQUENCE: 4] Process batch of entities
void OptimizedMovementSystem::ProcessBatch(size_t start, size_t end, float delta_time) {
    // Get raw data arrays for direct access
    auto* velocities = velocity_array_->GetDataArray();
    auto* velocity_entities = velocity_array_->GetEntityArray();
    
    // First pass: clamp velocities (can be SIMD optimized)
    ClampVelocityBatch(velocities + start, end - start);
    
    // Second pass: update positions
    for (size_t i = start; i < end; ++i) {
        EntityId entity = velocity_entities[i];
        auto* transform = transform_array_->GetComponent(entity);
        
        if (!transform) continue;
        
        // Direct memory access to velocity
        const auto& velocity = velocities[i];
        
        // Update position
        transform->position.x += velocity.linear.x * delta_time;
        transform->position.y += velocity.linear.y * delta_time;
        transform->position.z += velocity.linear.z * delta_time;
        
        // Update rotation
        transform->rotation.x += velocity.angular.x * delta_time;
        transform->rotation.y += velocity.angular.y * delta_time;
        transform->rotation.z += velocity.angular.z * delta_time;
        
        // Wrap rotation (can be vectorized)
        if (transform->rotation.x > 3.14159f) transform->rotation.x -= 6.28318f;
        if (transform->rotation.x < -3.14159f) transform->rotation.x += 6.28318f;
        if (transform->rotation.y > 3.14159f) transform->rotation.y -= 6.28318f;
        if (transform->rotation.y < -3.14159f) transform->rotation.y += 6.28318f;
        if (transform->rotation.z > 3.14159f) transform->rotation.z -= 6.28318f;
        if (transform->rotation.z < -3.14159f) transform->rotation.z += 6.28318f;
    }
}

// [SEQUENCE: 5] Batch velocity clamping (SIMD-friendly)
void OptimizedMovementSystem::ClampVelocityBatch(components::VelocityComponent* velocities, size_t count) {
    // This loop can be auto-vectorized by the compiler
    for (size_t i = 0; i < count; ++i) {
        auto& vel = velocities[i];
        
        float speed_sq = vel.linear.x * vel.linear.x + 
                        vel.linear.y * vel.linear.y + 
                        vel.linear.z * vel.linear.z;
        
        float max_speed_sq = vel.max_speed * vel.max_speed;
        
        if (speed_sq > max_speed_sq) {
            float scale = vel.max_speed / std::sqrt(speed_sq);
            vel.linear.x *= scale;
            vel.linear.y *= scale;
            vel.linear.z *= scale;
        }
    }
}

} // namespace mmorpg::game::systems::optimized