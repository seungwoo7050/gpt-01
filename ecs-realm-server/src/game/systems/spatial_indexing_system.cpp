#include "game/systems/spatial_indexing_system.h"
#include "game/components/transform_component.h"
#include "core/ecs/optimized/optimized_world.h"
#include <spdlog/spdlog.h>
#include <cmath>

namespace mmorpg::game::systems {

using namespace mmorpg::game::components;

void SpatialIndexingSystem::Update(float delta_time) {
    if (!world_) return;
    
    size_t updates_processed = 0;
    
    for (const auto& entity : entities_) {
        auto& transform = world_->GetComponent<TransformComponent>(entity);
        
        const core::utils::Vector3& current_pos = transform.position;
        
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
}

std::vector<core::ecs::EntityId> SpatialIndexingSystem::GetEntitiesInRadius(
    const core::utils::Vector3& center, float radius) const {
    
    if (!world_grid_) {
        return {};
    }
    
    auto candidates = world_grid_->GetEntitiesInRadius(center, radius);
    
    std::vector<core::ecs::EntityId> result;
    if (!world_) return result;

    for (auto entity : candidates) {
        auto& transform = world_->GetComponent<TransformComponent>(entity);
        float dx = center.x - transform.position.x;
        float dy = center.y - transform.position.y;
        float dz = center.z - transform.position.z;
        if (dx * dx + dy * dy + dz * dz <= radius * radius) {
            result.push_back(entity);
        }
    }

    return result;
}


std::vector<core::ecs::EntityId> SpatialIndexingSystem::GetEntitiesInView(
    core::ecs::EntityId observer, float view_distance) const {
    
    if (!world_) return {};
    
    auto& observer_transform = world_->GetComponent<TransformComponent>(observer);
    
    auto visible = GetEntitiesInRadius(observer_transform.position, view_distance);
    
    visible.erase(
        std::remove(visible.begin(), visible.end(), observer),
        visible.end()
    );
    
    return visible;
}

std::vector<core::ecs::EntityId> SpatialIndexingSystem::GetNearbyEntities(
    core::ecs::EntityId entity, float distance) const {
    
    if (!world_) return {};
    
    auto& transform = world_->GetComponent<TransformComponent>(entity);
    
    auto nearby = GetEntitiesInRadius(transform.position, distance);
    
    nearby.erase(
        std::remove(nearby.begin(), nearby.end(), entity),
        nearby.end()
    );
    
    return nearby;
}

} // namespace mmorpg::game::systems
