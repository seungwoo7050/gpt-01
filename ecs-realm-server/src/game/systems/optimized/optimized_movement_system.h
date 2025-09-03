#pragma once

#include "core/ecs/system.h"
#include "core/ecs/optimized/optimized_world.h"
#include "game/components/transform_component.h"
#include "game/components/velocity_component.h"

namespace mmorpg::game::systems::optimized {

// [SEQUENCE: 1] Optimized movement system with cache-efficient iteration
class OptimizedMovementSystem : public core::ecs::System {
public:
    OptimizedMovementSystem() : System("OptimizedMovementSystem") {}
    
    // [SEQUENCE: 2] System lifecycle
    void OnSystemInit() override;
    void OnSystemShutdown() override;
    
    // [SEQUENCE: 3] Optimized update with direct array access
    void Update(float delta_time) override;
    
    // [SEQUENCE: 4] System metadata
    core::ecs::SystemStage GetStage() const override { 
        return core::ecs::SystemStage::UPDATE; 
    }
    int GetPriority() const override { return 100; }
    
private:
    // [SEQUENCE: 5] Cache pointers to component arrays
    core::ecs::optimized::ComponentArray<components::TransformComponent>* transform_array_ = nullptr;
    core::ecs::optimized::ComponentArray<components::VelocityComponent>* velocity_array_ = nullptr;
    core::ecs::optimized::OptimizedWorld* optimized_world_ = nullptr;
    
    // [SEQUENCE: 6] Process entities in batches for cache efficiency
    void ProcessBatch(size_t start, size_t end, float delta_time);
    
    // [SEQUENCE: 7] SIMD-friendly velocity clamping
    void ClampVelocityBatch(components::VelocityComponent* velocities, size_t count);
};

} // namespace mmorpg::game::systems::optimized