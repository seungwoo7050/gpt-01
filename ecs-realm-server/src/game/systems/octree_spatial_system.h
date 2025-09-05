#pragma once

#include "core/ecs/optimized/system.h"
#include "core/ecs/optimized/optimized_world.h"
#include "game/world/octree/octree_world.h"
#include <memory>

namespace mmorpg::game::systems {

// [SEQUENCE: MVP3-83] Defines the OctreeSpatialSystem, an ECS system to manage the OctreeWorld.
class OctreeSpatialSystem : public core::ecs::optimized::System {
public:
    // [SEQUENCE: MVP3-84] Constructor.
    OctreeSpatialSystem();
    
    // [SEQUENCE: MVP3-85] System lifecycle methods.
    void OnSystemInit();
    void OnSystemShutdown();
    void PostUpdate(float delta_time);
    
    // [SEQUENCE: MVP3-86] Entity lifecycle methods to keep the octree synchronized.
    void OnEntityCreated(core::ecs::EntityId entity);
    void OnEntityDestroyed(core::ecs::EntityId entity);
    
    // [SEQUENCE: MVP3-87] Public API for 3D spatial queries.
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
    
    // [SEQUENCE: MVP3-88] Getter for direct access to the underlying OctreeWorld.
    world::octree::OctreeWorld* GetOctree() { return octree_world_.get(); }
    const world::octree::OctreeWorld* GetOctree() const { return octree_world_.get(); }
    
    // [SEQUENCE: MVP3-89] Public method to retrieve statistics about the octree.
    void GetOctreeStats(size_t& total_nodes, size_t& leaf_nodes, 
                       size_t& entities, size_t& depth) const;
    
private:
    // [SEQUENCE: MVP3-90] Internal struct to track an entity's spatial state.
    struct EntitySpatialData {
        core::utils::Vector3 last_position;
        bool needs_update = true;
        float accumulated_movement = 0.0f; // Track total movement
    };
    
    // [SEQUENCE: MVP3-91] Private member variables for the system.
    std::unique_ptr<world::octree::OctreeWorld> octree_world_;
    std::unordered_map<core::ecs::EntityId, EntitySpatialData> entity_spatial_data_;
    
    float position_update_threshold_ = 0.5f;  // Larger threshold for 3D
    float force_update_distance_ = 10.0f;     // Force update after this movement
    size_t batch_update_size_ = 64;           // Smaller batches for complex updates
};

} // namespace mmorpg::game::systems