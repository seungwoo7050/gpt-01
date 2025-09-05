#include "game/systems/octree_spatial_system.h"
#include "game/components/transform_component.h"
#include "core/ecs/optimized/optimized_world.h"
#include <spdlog/spdlog.h>

namespace mmorpg::game::systems {

// [SEQUENCE: MVP3-92] Implements the constructor, initializing the underlying OctreeWorld.
OctreeSpatialSystem::OctreeSpatialSystem() {
    // Create octree with 3D world configuration
    world::octree::OctreeWorld::Config config;
    config.world_min = core::utils::Vector3(-5000, -5000, -1000);  // 3D bounds
    config.world_max = core::utils::Vector3(5000, 5000, 1000);
    config.max_depth = 8;
    config.max_entities_per_node = 16;
    config.min_node_size = 12.5f;
    
    octree_world_ = std::make_unique<world::octree::OctreeWorld>(config);
    
    spdlog::info("OctreeSpatialSystem initialized with bounds ({}, {}, {}) to ({}, {}, {})",
                 config.world_min.x, config.world_min.y, config.world_min.z,
                 config.world_max.x, config.world_max.y, config.world_max.z);
}

// [SEQUENCE: MVP3-93] Implements the system lifecycle methods (currently empty).
void OctreeSpatialSystem::OnSystemInit() {}
void OctreeSpatialSystem::OnSystemShutdown() {}

// [SEQUENCE: MVP3-94] Implements PostUpdate to process entity movements and update the octree.
void OctreeSpatialSystem::PostUpdate([[maybe_unused]] float delta_time) {
    if (!m_world) return;
    
    size_t updates_processed = 0;
    size_t forced_updates = 0;
    
    for (const auto& entity : m_entities) {
        auto& transform = m_world->GetComponent<components::TransformComponent>(entity);
        const core::utils::Vector3& current_pos = transform.position;
        
        auto& spatial_data = entity_spatial_data_[entity];
        
        float dx = current_pos.x - spatial_data.last_position.x;
        float dy = current_pos.y - spatial_data.last_position.y;
        float dz = current_pos.z - spatial_data.last_position.z;
        float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        
        spatial_data.accumulated_movement += distance;
        
        if (spatial_data.needs_update || 
            distance > position_update_threshold_ ||
            spatial_data.accumulated_movement > force_update_distance_) {
            
            octree_world_->UpdateEntity(entity, spatial_data.last_position, current_pos);
            spatial_data.last_position = current_pos;
            spatial_data.needs_update = false;
            spatial_data.accumulated_movement = 0.0f;
            updates_processed++;
            
            if (spatial_data.accumulated_movement > force_update_distance_) {
                forced_updates++;
            }
        }
    }
}

// [SEQUENCE: MVP3-95] Implements OnEntityCreated to add new entities to the octree.
void OctreeSpatialSystem::OnEntityCreated(core::ecs::EntityId entity) {
    if (!m_world) return;

    if (m_world->HasComponent<components::TransformComponent>(entity)) {
        auto& transform = m_world->GetComponent<components::TransformComponent>(entity);
        octree_world_->AddEntity(entity, transform.position);
        
        EntitySpatialData data;
        data.last_position = transform.position;
        data.needs_update = false;
        data.accumulated_movement = 0.0f;
        entity_spatial_data_[entity] = data;
        
        spdlog::debug("Added entity {} to octree at position ({}, {}, {})",
                     entity, transform.position.x, transform.position.y, transform.position.z);
    }
}

// [SEQUENCE: MVP3-96] Implements OnEntityDestroyed to remove entities from the octree.
void OctreeSpatialSystem::OnEntityDestroyed(core::ecs::EntityId entity) {
    octree_world_->RemoveEntity(entity);
    entity_spatial_data_.erase(entity);
    spdlog::debug("Removed entity {} from octree", entity);
}

// [SEQUENCE: MVP3-97] Implements GetEntitiesInRadius, delegating the query to the OctreeWorld.
std::vector<core::ecs::EntityId> OctreeSpatialSystem::GetEntitiesInRadius(
    const core::utils::Vector3& center, float radius) const {
    
    if (!octree_world_) {
        return {};
    }
    
    return octree_world_->GetEntitiesInRadius(center, radius);
}

// [SEQUENCE: MVP3-98] Implements GetEntitiesInBox, delegating the query to the OctreeWorld.
std::vector<core::ecs::EntityId> OctreeSpatialSystem::GetEntitiesInBox(
    const core::utils::Vector3& min, const core::utils::Vector3& max) const {
    
    if (!octree_world_) {
        return {};
    }
    
    return octree_world_->GetEntitiesInBox(min, max);
}

// [SEQUENCE: MVP3-99] Implements GetEntitiesInView for observer-based queries.
std::vector<core::ecs::EntityId> OctreeSpatialSystem::GetEntitiesInView(
    core::ecs::EntityId observer, float view_distance) const {
    
    if (!m_world) return {};

    auto& observer_transform = m_world->GetComponent<components::TransformComponent>(observer);
    auto visible = GetEntitiesInRadius(observer_transform.position, view_distance);
    
    visible.erase(
        std::remove(visible.begin(), visible.end(), observer),
        visible.end()
    );
    
    return visible;
}

// [SEQUENCE: MVP3-100] Implements GetEntitiesAbove for vertical queries.
std::vector<core::ecs::EntityId> OctreeSpatialSystem::GetEntitiesAbove(
    const core::utils::Vector3& position, float height) const {
    
    core::utils::Vector3 box_min = position;
    core::utils::Vector3 box_max = position;
    box_max.z += height;
    
    float horizontal_radius = 50.0f;
    box_min.x -= horizontal_radius;
    box_min.y -= horizontal_radius;
    box_max.x += horizontal_radius;
    box_max.y += horizontal_radius;
    
    return GetEntitiesInBox(box_min, box_max);
}

// [SEQUENCE: MVP3-101] Implements GetEntitiesBelow for vertical queries.
std::vector<core::ecs::EntityId> OctreeSpatialSystem::GetEntitiesBelow(
    const core::utils::Vector3& position, float depth) const {
    
    core::utils::Vector3 box_min = position;
    core::utils::Vector3 box_max = position;
    box_min.z -= depth;
    
    float horizontal_radius = 50.0f;
    box_min.x -= horizontal_radius;
    box_min.y -= horizontal_radius;
    box_max.x += horizontal_radius;
    box_max.y += horizontal_radius;
    
    return GetEntitiesInBox(box_min, box_max);
}

// [SEQUENCE: MVP3-102] Implements GetOctreeStats to retrieve statistics from the OctreeWorld.
void OctreeSpatialSystem::GetOctreeStats(size_t& total_nodes, size_t& leaf_nodes,
                                        size_t& entities, size_t& depth) const {
    if (octree_world_) {
        octree_world_->GetTreeStats(total_nodes, leaf_nodes, entities);
        depth = octree_world_->GetDepth();
    } else {
        total_nodes = 0;
        leaf_nodes = 0;
        entities = 0;
        depth = 0;
    }
}

} // namespace mmorpg::game::systems