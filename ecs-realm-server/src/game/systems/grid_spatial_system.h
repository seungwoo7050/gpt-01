#pragma once

#include "core/ecs/optimized/system.h"
#include "core/ecs/optimized/optimized_world.h"
#include "game/world/grid/world_grid.h"
#include <memory>

namespace mmorpg::game::systems {

// [SEQUENCE: MVP3-67] Defines the GridSpatialSystem, an ECS system to manage the WorldGrid.
class GridSpatialSystem : public core::ecs::optimized::System {
public:
    // [SEQUENCE: MVP3-68] Constructor.
    GridSpatialSystem();
    
    // [SEQUENCE: MVP3-69] System lifecycle methods.
    void OnSystemInit();
    void OnSystemShutdown();
    void PostUpdate(float delta_time);
    
    // [SEQUENCE: MVP3-70] Entity lifecycle methods to keep the grid synchronized.
    void OnEntityCreated(core::ecs::EntityId entity);
    void OnEntityDestroyed(core::ecs::EntityId entity);
    
    // [SEQUENCE: MVP3-71] Public API for spatial queries, with precise, narrow-phase filtering.
    std::vector<core::ecs::EntityId> GetEntitiesInRadius(
        const core::utils::Vector3& center, float radius) const;
    
    std::vector<core::ecs::EntityId> GetEntitiesInView(
        core::ecs::EntityId observer, float view_distance) const;
    
    std::vector<core::ecs::EntityId> GetNearbyEntities(
        core::ecs::EntityId entity, float distance) const;
    
    // [SEQUENCE: MVP3-72] Getter for direct access to the underlying WorldGrid.
    world::grid::WorldGrid* GetWorldGrid() { return world_grid_.get(); }
    const world::grid::WorldGrid* GetWorldGrid() const { return world_grid_.get(); }
    
private:
    // [SEQUENCE: MVP3-73] Internal struct to track an entity's spatial state.
    struct EntitySpatialData {
        core::utils::Vector3 last_position;
        bool needs_update = true;
    };
    
    // [SEQUENCE: MVP3-74] Private member variables for the system.
    std::unique_ptr<world::grid::WorldGrid> world_grid_;
    std::unordered_map<core::ecs::EntityId, EntitySpatialData> entity_spatial_data_;
    
    float position_update_threshold_ = 0.1f; // Minimum movement to trigger update
    size_t batch_update_size_ = 100; // Process entities in batches
};

}
