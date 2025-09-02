#pragma once

#include "core/ecs/system.h"
#include "core/ecs/optimized/optimized_world.h"
#include "game/world/grid/world_grid.h"
#include <memory>

namespace mmorpg::game::systems {

// [SEQUENCE: 1] System that maintains spatial indices for efficient queries
class SpatialIndexingSystem : public core::ecs::System {
public:
    SpatialIndexingSystem() : System("SpatialIndexingSystem") {}
    
    // [SEQUENCE: 2] Initialize with world configuration
    void OnSystemInit() override;
    void OnSystemShutdown() override;
    
    // [SEQUENCE: 3] Update spatial indices
    void PostUpdate(float delta_time) override;
    
    // [SEQUENCE: 4] System metadata
    core::ecs::SystemStage GetStage() const override { 
        return core::ecs::SystemStage::POST_UPDATE; 
    }
    int GetPriority() const override { return 500; } // After movement
    
    // [SEQUENCE: 5] Entity lifecycle
    void OnEntityCreated(core::ecs::EntityId entity) override;
    void OnEntityDestroyed(core::ecs::EntityId entity) override;
    
    // [SEQUENCE: 6] Spatial queries
    std::vector<core::ecs::EntityId> GetEntitiesInRadius(
        const core::utils::Vector3& center, float radius) const;
    
    std::vector<core::ecs::EntityId> GetEntitiesInView(
        core::ecs::EntityId observer, float view_distance) const;
    
    std::vector<core::ecs::EntityId> GetNearbyEntities(
        core::ecs::EntityId entity, float distance) const;
    
    // [SEQUENCE: 7] Get spatial index (for other systems)
    world::grid::WorldGrid* GetWorldGrid() { return world_grid_.get(); }
    const world::grid::WorldGrid* GetWorldGrid() const { return world_grid_.get(); }
    
private:
    // [SEQUENCE: 8] Track entity positions for updates
    struct EntitySpatialData {
        core::utils::Vector3 last_position;
        bool needs_update = true;
    };
    
    // [SEQUENCE: 9] Spatial index and tracking
    std::unique_ptr<world::grid::WorldGrid> world_grid_;
    std::unordered_map<core::ecs::EntityId, EntitySpatialData> entity_spatial_data_;
    
    // [SEQUENCE: 10] Cached component arrays
    core::ecs::optimized::ComponentArray<components::TransformComponent>* transform_array_ = nullptr;
    core::ecs::optimized::OptimizedWorld* optimized_world_ = nullptr;
    
    // [SEQUENCE: 11] Update configuration
    float position_update_threshold_ = 0.1f; // Minimum movement to trigger update
    size_t batch_update_size_ = 100; // Process entities in batches
};

} // namespace mmorpg::game::systems