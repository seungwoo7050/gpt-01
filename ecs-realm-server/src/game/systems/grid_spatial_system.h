#pragma once

#include "core/ecs/optimized/system.h"
#include "core/ecs/optimized/optimized_world.h"
#include "game/world/grid/world_grid.h"
#include <memory>

namespace mmorpg::game::systems {

// [SEQUENCE: MVP3-8] Spatial Systems (`src/game/systems/`)
// [SEQUENCE: MVP3-9] Manages the WorldGrid. It updates the grid based on entity movements.
class GridSpatialSystem : public core::ecs::optimized::System {
public:
    GridSpatialSystem();
    
    void OnSystemInit();
    void OnSystemShutdown();
    
    void PostUpdate(float delta_time);
    
    void OnEntityCreated(core::ecs::EntityId entity);
    void OnEntityDestroyed(core::ecs::EntityId entity);
    
    std::vector<core::ecs::EntityId> GetEntitiesInRadius(
        const core::utils::Vector3& center, float radius) const;
    
    std::vector<core::ecs::EntityId> GetEntitiesInView(
        core::ecs::EntityId observer, float view_distance) const;
    
    std::vector<core::ecs::EntityId> GetNearbyEntities(
        core::ecs::EntityId entity, float distance) const;
    
    world::grid::WorldGrid* GetWorldGrid() { return world_grid_.get(); }
    const world::grid::WorldGrid* GetWorldGrid() const { return world_grid_.get(); }
    
private:
    struct EntitySpatialData {
        core::utils::Vector3 last_position;
        bool needs_update = true;
    };
    
    std::unique_ptr<world::grid::WorldGrid> world_grid_;
    std::unordered_map<core::ecs::EntityId, EntitySpatialData> entity_spatial_data_;
    
    float position_update_threshold_ = 0.1f; // Minimum movement to trigger update
    size_t batch_update_size_ = 100; // Process entities in batches
};

}
