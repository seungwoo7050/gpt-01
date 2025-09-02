#include "game/systems/grid_spatial_system.h"
#include "game/components/transform_component.h"
#include <spdlog/spdlog.h>

namespace mmorpg::game::systems {

// [SEQUENCE: MVP3-37] spatial_indexing_system.cpp: Initialize spatial indexing system
void GridSpatialSystem::OnSystemInit() {
    // Create world grid with default configuration
    world::grid::WorldGrid::Config config;
    config.cell_size = 100.0f;
    config.grid_width = 100;
    config.grid_height = 100;
    config.world_min_x = -5000.0f;
    config.world_min_y = -5000.0f;
    config.wrap_around = false;
    
    world_grid_ = std::make_unique<world::grid::WorldGrid>(config);
    
    // Cache component arrays for fast access
    if (auto* world = GetWorld()) {
        optimized_world_ = dynamic_cast<core::ecs::optimized::OptimizedWorld*>(world);
        if (optimized_world_) {
            transform_array_ = optimized_world_->GetComponentArray<components::TransformComponent>();
        }
    }
    
    spdlog::info("GridSpatialSystem initialized with {}x{} grid", 
                 config.grid_width, config.grid_height);
}

// [SEQUENCE: MVP3-38] spatial_indexing_system.cpp: Clean up resources
void GridSpatialSystem::OnSystemShutdown() {
    entity_spatial_data_.clear();
    world_grid_.reset();
    spdlog::info("GridSpatialSystem shut down");
}

// [SEQUENCE: MVP3-39] spatial_indexing_system.cpp: Update spatial indices after movement
void GridSpatialSystem::PostUpdate(float delta_time) {
    if (!optimized_world_ || !transform_array_) {
        return;
    }
    
    size_t updates_processed = 0;
    
    for (const auto& entity : entities_) {
        const auto* transform = transform_array_->GetComponent(entity);
        if (!transform) continue;
        
        const core::utils::Vector3& current_pos = transform->position;
        
        auto& spatial_data = entity_spatial_data_[entity];
        
        float distance_squared = 
            (current_pos.x - spatial_data.last_position.x) * 
            (current_pos.x - spatial_data.last_position.x) +
            (current_pos.y - spatial_data.last_position.y) * 
            (current_pos.y - spatial_data.last_position.y) +
            (current_pos.z - spatial_data.last_position.z) * 
            (current_pos.z - spatial_data.last_position.z);
        
        if (spatial_data.needs_update || 
            distance_squared > position_update_threshold_ * position_update_threshold_) {
            
            world_grid_->UpdateEntity(entity, spatial_data.last_position, current_pos);
            spatial_data.last_position = current_pos;
            spatial_data.needs_update = false;
            updates_processed++;
        }
    }
    
    static float metric_timer = 0.0f;
    metric_timer += delta_time;
    if (metric_timer > 5.0f) {
        spdlog::debug("SpatialIndexing: {} entities, {} updates, {} occupied cells",
                     entity_spatial_data_.size(), updates_processed,
                     world_grid_->GetOccupiedCellCount());
        metric_timer = 0.0f;
    }
}

// [SEQUENCE: MVP3-40] spatial_indexing_system.cpp: Handle new entity creation
void GridSpatialSystem::OnEntityCreated(core::ecs::EntityId entity) {
    if (!optimized_world_ || !transform_array_) {
        return;
    }
    
    // Check if entity has transform component
    const auto* transform = transform_array_->GetComponent(entity);
    if (transform) {
        // Add to spatial index
        world_grid_->AddEntity(entity, transform->position);
        
        // Initialize tracking data
        entity_spatial_data_[entity] = {
            transform->position,
            false  // Already updated
        };
        
        spdlog::debug("Added entity {} to spatial index at position ({}, {}, {})",
                     entity, transform->position.x, transform->position.y, transform->position.z);
    }
}

// [SEQUENCE: MVP3-41] spatial_indexing_system.cpp: Handle entity destruction
void GridSpatialSystem::OnEntityDestroyed(core::ecs::EntityId entity) {
    // Remove from spatial index
    world_grid_->RemoveEntity(entity);
    
    // Remove tracking data
    entity_spatial_data_.erase(entity);
    
    spdlog::debug("Removed entity {} from spatial index", entity);
}

// [SEQUENCE: MVP3-42] spatial_indexing_system.cpp: Get entities within radius of point
std::vector<core::ecs::EntityId> GridSpatialSystem::GetEntitiesInRadius(
    const core::utils::Vector3& center, float radius) const {
    
    if (!world_grid_) {
        return {};
    }
    
    // Get initial candidates from grid
    auto candidates = world_grid_->GetEntitiesInRadius(center, radius);
    
    // Filter by exact distance if needed
    std::vector<core::ecs::EntityId> result;
    result.reserve(candidates.size());
    
    float radius_squared = radius * radius;
    
    for (core::ecs::EntityId entity : candidates) {
        // Get entity position for exact check
        if (transform_array_) {
            const auto* transform = transform_array_->GetComponent(entity);
            if (transform) {
                float dx = transform->position.x - center.x;
                float dy = transform->position.y - center.y;
                float dz = transform->position.z - center.z;
                float dist_squared = dx * dx + dy * dy + dz * dz;
                
                if (dist_squared <= radius_squared) {
                    result.push_back(entity);
                }
            }
        }
    }
    
    return result;
}


std::vector<core::ecs::EntityId> GridSpatialSystem::GetEntitiesInView(
    core::ecs::EntityId observer, float view_distance) const {
    
    if (!transform_array_) {
        return {};
    }
    
    // Get observer position
    const auto* observer_transform = transform_array_->GetComponent(observer);
    if (!observer_transform) {
        return {};
    }
    
    // Get entities in view radius
    auto visible = GetEntitiesInRadius(observer_transform->position, view_distance);
    
    // Remove self from results
    visible.erase(
        std::remove(visible.begin(), visible.end(), observer),
        visible.end()
    );
    
    return visible;
}

std::vector<core::ecs::EntityId> GridSpatialSystem::GetNearbyEntities(
    core::ecs::EntityId entity, float distance) const {
    
    if (!transform_array_) {
        return {};
    }
    
    // Get entity position
    const auto* transform = transform_array_->GetComponent(entity);
    if (!transform) {
        return {};
    }
    
    // Get entities in radius
    auto nearby = GetEntitiesInRadius(transform->position, distance);
    
    // Remove self from results
    nearby.erase(
        std::remove(nearby.begin(), nearby.end(), entity),
        nearby.end()
    );
    
    return nearby;
}

} // namespace mmorpg::game::systems
e.end(), observer),
        visible.end()
    );
    
    return visible;
}

std::vector<core::ecs::EntityId> GridSpatialSystem::GetNearbyEntities(
    core::ecs::EntityId entity, float distance) const {
    
    if (!transform_array_) {
        return {};
    }
    
    // Get entity position
    const auto* transform = transform_array_->GetComponent(entity);
    if (!transform) {
        return {};
    }
    
    // Get entities in radius
    auto nearby = GetEntitiesInRadius(transform->position, distance);
    
    // Remove self from results
    nearby.erase(
        std::remove(nearby.begin(), nearby.end(), entity),
        nearby.end()
    );
    
    return nearby;
}

} // namespace mmorpg::game::systems
 );
    
    return nearby;
}

} // namespace mmorpg::game::systems
ystems
ystems
space mmorpg::game::systems
