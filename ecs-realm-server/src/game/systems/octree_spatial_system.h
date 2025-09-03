#pragma once

#include "core/ecs/optimized/system.h"
#include "core/ecs/optimized/optimized_world.h"
#include "game/world/octree/octree_world.h"
#include <memory>

namespace mmorpg::game::systems {

// [SEQUENCE: MVP3-10] System that maintains octree spatial index for 3D queries
class OctreeSpatialSystem : public core::ecs::optimized::System {
public:
    OctreeSpatialSystem();
    
    // [SEQUENCE: 2] Initialize with world configuration
    void OnSystemInit();
    void OnSystemShutdown();
    
    // [SEQUENCE: 3] Update spatial indices
    void PostUpdate(float delta_time);
    
    // [SEQUENCE: 5] Entity lifecycle
    void OnEntityCreated(core::ecs::EntityId entity);
    void OnEntityDestroyed(core::ecs::EntityId entity);
    
    // [SEQUENCE: 6] 3D spatial queries
    std::vector<core::ecs::EntityId> GetEntitiesInRadius(
        const core::utils::Vector3& center, float radius) const;
    
    std::vector<core::ecs::EntityId> GetEntitiesInBox(
        const core::utils::Vector3& min, const core::utils::Vector3& max) const;
    
    std::vector<core::ecs::EntityId> GetEntitiesInView(
        core::ecs::EntityId observer, float view_distance) const;
    
    std::vector<core::ecs::EntityId> GetEntitiesAbove(
        const core::utils::Vector3& position, float height) const;
    
    std::vector<core::ecs::EntityId> GetEntitiesBelow(
        const core::utils::Vector3& position, float depth) const;
    
    // [SEQUENCE: 7] Get octree for advanced queries
    world::octree::OctreeWorld* GetOctree() { return octree_world_.get(); }
    const world::octree::OctreeWorld* GetOctree() const { return octree_world_.get(); }
    
    // [SEQUENCE: 8] Get tree statistics
    void GetOctreeStats(size_t& total_nodes, size_t& leaf_nodes, 
                       size_t& entities, size_t& depth) const;
    
private:
    // [SEQUENCE: 9] Track entity positions for updates
    struct EntitySpatialData {
        core::utils::Vector3 last_position;
        bool needs_update = true;
        float accumulated_movement = 0.0f; // Track total movement
    };
    
    // [SEQUENCE: 10] Octree and tracking
    std::unique_ptr<world::octree::OctreeWorld> octree_world_;
    std::unordered_map<core::ecs::EntityId, EntitySpatialData> entity_spatial_data_;
    
    // [SEQUENCE: 12] Update configuration
    float position_update_threshold_ = 0.5f;  // Larger threshold for 3D
    float force_update_distance_ = 10.0f;     // Force update after this movement
    size_t batch_update_size_ = 64;           // Smaller batches for complex updates
};

} // namespace mmorpg::game::systems