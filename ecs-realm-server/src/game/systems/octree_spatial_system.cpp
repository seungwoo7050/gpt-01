

#include "game/systems/octree_spatial_system.h"
#include "game/components/transform_component.h"
#include <spdlog/spdlog.h>

namespace mmorpg::game::systems {

// [SEQUENCE: MVP3-47] octree_spatial_system.cpp: Initialize octree spatial system
void OctreeSpatialSystem::OnSystemInit() {
    // Create octree with 3D world configuration
    world::octree::OctreeWorld::Config config;
    config.world_min = core::utils::Vector3(-5000, -5000, -1000);  // 3D bounds
    config.world_max = core::utils::Vector3(5000, 5000, 1000);
    config.max_depth = 8;
    config.max_entities_per_node = 16;
    config.min_node_size = 12.5f;
    
    octree_world_ = std::make_unique<world::octree::OctreeWorld>(config);
    
    // Cache component arrays for fast access
    if (auto* world = GetWorld()) {
        optimized_world_ = dynamic_cast<core::ecs::optimized::OptimizedWorld*>(world);
        if (optimized_world_) {
            transform_array_ = optimized_world_->GetComponentArray<components::TransformComponent>();
        }
    }
    
    spdlog::info("OctreeSpatialSystem initialized with bounds ({}, {}, {}) to ({}, {}, {})",
                 config.world_min.x, config.world_min.y, config.world_min.z,
                 config.world_max.x, config.world_max.y, config.world_max.z);
}

// [SEQUENCE: MVP3-48] octree_spatial_system.cpp: Clean up resources
void OctreeSpatialSystem::OnSystemShutdown() {
    entity_spatial_data_.clear();
    octree_world_.reset();
    spdlog::info("OctreeSpatialSystem shut down");
}

// [SEQUENCE: MVP3-49] octree_spatial_system.cpp: Update octree with entity movements
void OctreeSpatialSystem::PostUpdate(float delta_time) {
    if (!optimized_world_ || !transform_array_) {
        return;
    }
    
    size_t updates_processed = 0;
    size_t forced_updates = 0;
    
    for (const auto& entity : entities_) {
        const auto* transform = transform_array_->GetComponent(entity);
        if (!transform) continue;
        
        const core::utils::Vector3& current_pos = transform->position;
        
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
    
    static float metric_timer = 0.0f;
    metric_timer += delta_time;
    if (metric_timer > 5.0f) {
        size_t total_nodes, leaf_nodes, tree_entities, depth;
        GetOctreeStats(total_nodes, leaf_nodes, tree_entities, depth);
        
        spdlog::debug("OctreeSpatial: {} entities, {} updates ({} forced), "
                     "tree: {} nodes ({} leaves), depth {}",
                     entity_spatial_data_.size(), updates_processed, forced_updates,
                     total_nodes, leaf_nodes, depth);
        metric_timer = 0.0f;
    }
}

// [SEQUENCE: MVP3-50] octree_spatial_system.cpp: Handle new entity creation
void OctreeSpatialSystem::OnEntityCreated(core::ecs::EntityId entity) {
    if (!optimized_world_ || !transform_array_) {
        return;
    }
    
    // Check if entity has transform component
    const auto* transform = transform_array_->GetComponent(entity);
    if (transform) {
        // Add to octree
        octree_world_->AddEntity(entity, transform->position);
        
        // Initialize tracking data
        entity_spatial_data_[entity] = {
            transform->position,
            false,  // Already updated
            0.0f    // No accumulated movement
        };
        
        spdlog::debug("Added entity {} to octree at position ({}, {}, {})",
                     entity, transform->position.x, transform->position.y, transform->position.z);
    }
}

// [SEQUENCE: MVP3-51] octree_spatial_system.cpp: Handle entity destruction
void OctreeSpatialSystem::OnEntityDestroyed(core::ecs::EntityId entity) {
    // Remove from octree
    octree_world_->RemoveEntity(entity);
    
    // Remove tracking data
    entity_spatial_data_.erase(entity);
    
    spdlog::debug("Removed entity {} from octree", entity);
}


std::vector<core::ecs::EntityId> OctreeSpatialSystem::GetEntitiesInRadius(
    const core::utils::Vector3& center, float radius) const {
    
    if (!octree_world_) {
        return {};
    }
    
    // Octree already does exact sphere checks
    return octree_world_->GetEntitiesInRadius(center, radius);
}

// [SEQUENCE: MVP3-53] octree_spatial_system.cpp: Get entities in 3D box
std::vector<core::ecs::EntityId> OctreeSpatialSystem::GetEntitiesInBox(
    const core::utils::Vector3& min, const core::utils::Vector3& max) const {
    
    if (!octree_world_) {
        return {};
    }
    
    return octree_world_->GetEntitiesInBox(min, max);
}

std::vector<core::ecs::EntityId> OctreeSpatialSystem::GetEntitiesInView(
    core::ecs::EntityId observer, float view_distance) const {
    
    if (!transform_array_) {
        return {};
    }
    
    // Get observer position
    const auto* observer_transform = transform_array_->GetComponent(observer);
    if (!observer_transform) {
        return {};
    }
    
    // Get entities in sphere around observer
    auto visible = GetEntitiesInRadius(observer_transform->position, view_distance);
    
    // Remove self from results
    visible.erase(
        std::remove(visible.begin(), visible.end(), observer),
        visible.end()
    );
    
    return visible;
}

std::vector<core::ecs::EntityId> OctreeSpatialSystem::GetEntitiesAbove(
    const core::utils::Vector3& position, float height) const {
    
    // Query box extending upward
    core::utils::Vector3 box_min = position;
    core::utils::Vector3 box_max = position;
    box_max.z += height;
    
    // Extend horizontally to catch nearby entities
    float horizontal_radius = 50.0f;
    box_min.x -= horizontal_radius;
    box_min.y -= horizontal_radius;
    box_max.x += horizontal_radius;
    box_max.y += horizontal_radius;
    
    return GetEntitiesInBox(box_min, box_max);
}

std::vector<core::ecs::EntityId> OctreeSpatialSystem::GetEntitiesBelow(
    const core::utils::Vector3& position, float depth) const {
    
    // Query box extending downward
    core::utils::Vector3 box_min = position;
    core::utils::Vector3 box_max = position;
    box_min.z -= depth;
    
    // Extend horizontally to catch nearby entities
    float horizontal_radius = 50.0f;
    box_min.x -= horizontal_radius;
    box_min.y -= horizontal_radius;
    box_max.x += horizontal_radius;
    box_max.y += horizontal_radius;
    
    return GetEntitiesInBox(box_min, box_max);
}

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
tally to catch nearby entities
    float horizontal_radius = 50.0f;
    box_min.x -= horizontal_radius;
    box_min.y -= horizontal_radius;
    box_max.x += horizontal_radius;
    box_max.y += horizontal_radius;
    
    return GetEntitiesInBox(box_min, box_max);
}

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
 0;
        entities = 0;
        depth = 0;
    }
}

} // namespace mmorpg::game::systems
::game::systems
ems
 namespace mmorpg::game::systems
