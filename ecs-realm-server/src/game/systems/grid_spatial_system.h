#pragma once

#include "core/ecs/system.h"
#include "core/ecs/optimized/optimized_world.h"
#include "game/world/grid/world_grid.h"
#include <memory>

namespace mmorpg::game::systems {

// [SEQUENCE: MVP3-34] spatial_indexing_system.h: System that maintains spatial indices for efficient queries
class GridSpatialSystem : public core::ecs::System {
public:
    SpatialIndexingSystem() : System("SpatialIndexingSystem") {}
    
    // [SEQUENCE: MVP3-35] spatial_indexing_system.h: System lifecycle and update logic
    void OnSystemInit() override;
    void OnSystemShutdown() override;
    
    
    void PostUpdate(float delta_time) override;
    
    
    core::ecs::SystemStage GetStage() const override { 
        return core::ecs::SystemStage::POST_UPDATE; 
    }
    int GetPriority() const override { return 500; } // After movement
    
    
    void OnEntityCreated(core::ecs::EntityId entity) override;
    void OnEntityDestroyed(core::ecs::EntityId entity) override;
    
    
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
    
    
    core::ecs::optimized::ComponentArray<components::TransformComponent>* transform_array_ = nullptr;
    core::ecs::optimized::OptimizedWorld* optimized_world_ = nullptr;
    
    float position_update_threshold_ = 0.1f; // Minimum movement to trigger update
    size_t batch_update_size_ = 100; // Process entities in batches
};hes
};