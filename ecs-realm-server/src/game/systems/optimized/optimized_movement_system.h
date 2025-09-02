#pragma once

#include "core/ecs/system.h"
#include "core/ecs/optimized/optimized_world.h"
#include "game/components/transform_component.h"
#include "game/components/velocity_component.h"

namespace mmorpg::game::systems::optimized {

// [SEQUENCE: MVP2-26] A data-oriented version of the movement system.
class OptimizedMovementSystem : public core::ecs::System {
public:
    OptimizedMovementSystem() = default;
    
    void OnSystemInit() override;
    void OnSystemShutdown() override;
    void Update(float delta_time) override;
    
private:
    core::ecs::optimized::ComponentArray<components::TransformComponent>* transform_array_ = nullptr;
    core::ecs::optimized::ComponentArray<components::VelocityComponent>* velocity_array_ = nullptr;
    core::ecs::optimized::OptimizedWorld* optimized_world_ = nullptr;
    
    void ProcessBatch(size_t start, size_t end, float delta_time);
    void ClampVelocityBatch(components::VelocityComponent* velocities, size_t count);
};

} // namespace mmorpg::game::systems::optimized