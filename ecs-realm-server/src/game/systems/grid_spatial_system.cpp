#include "game/systems/grid_spatial_system.h"
#include "game/components/transform_component.h"
#include "core/ecs/optimized/optimized_world.h"
#include <spdlog/spdlog.h>

namespace mmorpg::game::systems {

GridSpatialSystem::GridSpatialSystem() {
    world::grid::WorldGrid::Config config;
    config.cell_size = 100.0f;
    config.grid_width = 100;
    config.grid_height = 100;
    config.world_min_x = -5000.0f;
    config.world_min_y = -5000.0f;
    config.wrap_around = false;
    
    world_grid_ = std::make_unique<world::grid::WorldGrid>(config);
}

void GridSpatialSystem::OnSystemInit() {}
void GridSpatialSystem::OnSystemShutdown() {}

void GridSpatialSystem::PostUpdate([[maybe_unused]] float delta_time) {
    if (!world_) return;
    
    size_t updates_processed = 0;
    
    for (const auto& entity : entities_) {
        auto& transform = world_->GetComponent<components::TransformComponent>(entity);
        
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

void GridSpatialSystem::OnEntityCreated(core::ecs::EntityId entity) {
    if (!world_) return;

    if (world_->HasComponent<components::TransformComponent>(entity)){
        auto& transform = world_->GetComponent<components::TransformComponent>(entity);
        world_grid_->AddEntity(entity, transform.position);
        EntitySpatialData data;
        data.last_position = transform.position;
        data.needs_update = false;
        entity_spatial_data_[entity] = data;
    }
}

void GridSpatialSystem::OnEntityDestroyed(core::ecs::EntityId entity) {
    world_grid_->RemoveEntity(entity);
    entity_spatial_data_.erase(entity);
}

std::vector<core::ecs::EntityId> GridSpatialSystem::GetEntitiesInRadius(
    const core::utils::Vector3& center, float radius) const {
    
    if (!world_grid_) {
        return {};
    }
    
    auto candidates = world_grid_->GetEntitiesInRadius(center, radius);

    std::vector<core::ecs::EntityId> result;
    if (!world_) return result;

    for (auto entity : candidates) {
        auto& transform = world_->GetComponent<components::TransformComponent>(entity);
        float dx = center.x - transform.position.x;
        float dy = center.y - transform.position.y;
        float dz = center.z - transform.position.z;
        if (dx * dx + dy * dy + dz * dz <= radius * radius) {
            result.push_back(entity);
        }
    }

    return result;
}


std::vector<core::ecs::EntityId> GridSpatialSystem::GetEntitiesInView(
    core::ecs::EntityId observer, float view_distance) const {
    
    if (!world_) return {};
    
    auto& observer_transform = world_->GetComponent<components::TransformComponent>(observer);
    
    auto visible = GetEntitiesInRadius(observer_transform.position, view_distance);
    
    visible.erase(
        std::remove(visible.begin(), visible.end(), observer),
        visible.end()
    );
    
    return visible;
}

std::vector<core::ecs::EntityId> GridSpatialSystem::GetNearbyEntities(
    core::ecs::EntityId entity, float distance) const {
    
    if (!world_) return {};
    
    auto& transform = world_->GetComponent<components::TransformComponent>(entity);
    
    auto nearby = GetEntitiesInRadius(transform.position, distance);
    
    nearby.erase(
        std::remove(nearby.begin(), nearby.end(), entity),
        nearby.end()
    );
    
    return nearby;
}

} // namespace mmorpg::game::systems
