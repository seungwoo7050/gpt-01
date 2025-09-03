#include "game/world/grid/world_grid.h"
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>

namespace mmorpg::game::world::grid {

// [SEQUENCE: 1] Constructor - initialize grid
WorldGrid::WorldGrid(const Config& config) : config_(config) {
    // Allocate grid cells
    grid_.resize(config_.grid_width);
    for (auto& column : grid_) {
        column.resize(config_.grid_height);
        for (auto& cell : column) {
            cell = std::make_unique<GridCell>();
        }
    }
    
    spdlog::info("WorldGrid initialized: {}x{} cells of size {}", 
                 config_.grid_width, config_.grid_height, config_.cell_size);
}

// [SEQUENCE: 2] Add entity to grid
void WorldGrid::AddEntity(core::ecs::EntityId entity, const core::utils::Vector3& position) {
    auto [cell_x, cell_y] = GetCellCoordinates(position);
    
    if (!IsValidCell(cell_x, cell_y)) {
        spdlog::warn("Entity {} position {} outside grid bounds", entity, position);
        return;
    }
    
    // Add to cell
    {
        std::lock_guard<std::mutex> lock(grid_[cell_x][cell_y]->mutex);
        grid_[cell_x][cell_y]->entities.insert(entity);
    }
    
    // Update entity mapping
    {
        std::lock_guard<std::mutex> lock(entity_map_mutex_);
        entity_cells_[entity] = {cell_x, cell_y};
    }
    
    spdlog::debug("Added entity {} to cell ({}, {})", entity, cell_x, cell_y);
}

// [SEQUENCE: 3] Remove entity from grid
void WorldGrid::RemoveEntity(core::ecs::EntityId entity) {
    std::pair<int, int> cell_coords;
    
    // Find entity's cell
    {
        std::lock_guard<std::mutex> lock(entity_map_mutex_);
        auto it = entity_cells_.find(entity);
        if (it == entity_cells_.end()) {
            return;
        }
        cell_coords = it->second;
        entity_cells_.erase(it);
    }
    
    // Remove from cell
    {
        std::lock_guard<std::mutex> lock(grid_[cell_coords.first][cell_coords.second]->mutex);
        grid_[cell_coords.first][cell_coords.second]->entities.erase(entity);
    }
    
    spdlog::debug("Removed entity {} from cell ({}, {})", entity, cell_coords.first, cell_coords.second);
}

// [SEQUENCE: 4] Update entity position
void WorldGrid::UpdateEntity(core::ecs::EntityId entity, 
                           const core::utils::Vector3& old_pos,
                           const core::utils::Vector3& new_pos) {
    auto [old_x, old_y] = GetCellCoordinates(old_pos);
    auto [new_x, new_y] = GetCellCoordinates(new_pos);
    
    // If still in same cell, no update needed
    if (old_x == new_x && old_y == new_y) {
        return;
    }
    
    // Remove from old cell
    if (IsValidCell(old_x, old_y)) {
        std::lock_guard<std::mutex> lock(grid_[old_x][old_y]->mutex);
        grid_[old_x][old_y]->entities.erase(entity);
    }
    
    // Add to new cell
    if (IsValidCell(new_x, new_y)) {
        std::lock_guard<std::mutex> lock(grid_[new_x][new_y]->mutex);
        grid_[new_x][new_y]->entities.insert(entity);
        
        // Update mapping
        std::lock_guard<std::mutex> map_lock(entity_map_mutex_);
        entity_cells_[entity] = {new_x, new_y};
    } else {
        // Entity moved outside grid
        std::lock_guard<std::mutex> lock(entity_map_mutex_);
        entity_cells_.erase(entity);
    }
}

// [SEQUENCE: 5] Get entities within radius
std::vector<core::ecs::EntityId> WorldGrid::GetEntitiesInRadius(
    const core::utils::Vector3& center, float radius) const {
    
    std::vector<core::ecs::EntityId> result;
    std::vector<std::pair<int, int>> cells_to_check;
    
    // Get all cells that might contain entities within radius
    GetCellsInRadius(center, radius, cells_to_check);
    
    // float radius_squared = radius * radius; // This is not used, but might be useful for precise filtering later
    
    // Check each cell
    for (const auto& [cell_x, cell_y] : cells_to_check) {
        if (!IsValidCell(cell_x, cell_y)) continue;
        
        std::lock_guard<std::mutex> lock(grid_[cell_x][cell_y]->mutex);
        
        // For each entity in cell, check actual distance
        for (core::ecs::EntityId entity : grid_[cell_x][cell_y]->entities) {
            // Note: Would need entity position here for exact check
            // For now, include all entities in relevant cells
            result.push_back(entity);
        }
    }
    
    return result;
}

// [SEQUENCE: 6] Get entities in box
std::vector<core::ecs::EntityId> WorldGrid::GetEntitiesInBox(
    const core::utils::Vector3& min, const core::utils::Vector3& max) const {
    
    std::vector<core::ecs::EntityId> result;
    
    // Get cell range
    auto [min_x, min_y] = GetCellCoordinates(min);
    auto [max_x, max_y] = GetCellCoordinates(max);
    
    // Clamp to grid bounds
    min_x = std::max(0, min_x);
    min_y = std::max(0, min_y);
    max_x = std::min(config_.grid_width - 1, max_x);
    max_y = std::min(config_.grid_height - 1, max_y);
    
    // Collect entities from all cells in range
    for (int x = min_x; x <= max_x; ++x) {
        for (int y = min_y; y <= max_y; ++y) {
            std::lock_guard<std::mutex> lock(grid_[x][y]->mutex);
            result.insert(result.end(), 
                         grid_[x][y]->entities.begin(), 
                         grid_[x][y]->entities.end());
        }
    }
    
    return result;
}

// [SEQUENCE: 7] Get entities in specific cell
std::vector<core::ecs::EntityId> WorldGrid::GetEntitiesInCell(int x, int y) const {
    if (!IsValidCell(x, y)) {
        return {};
    }
    
    std::lock_guard<std::mutex> lock(grid_[x][y]->mutex);
    return std::vector<core::ecs::EntityId>(
        grid_[x][y]->entities.begin(), 
        grid_[x][y]->entities.end()
    );
}

// [SEQUENCE: 8] Get entities in adjacent cells
std::vector<core::ecs::EntityId> WorldGrid::GetEntitiesInAdjacentCells(
    const core::utils::Vector3& position, int range) const {
    
    std::vector<core::ecs::EntityId> result;
    auto [center_x, center_y] = GetCellCoordinates(position);
    
    // Check all cells within range
    for (int dx = -range; dx <= range; ++dx) {
        for (int dy = -range; dy <= range; ++dy) {
            int cell_x = center_x + dx;
            int cell_y = center_y + dy;
            
            // Handle wrap-around if enabled
            if (config_.wrap_around) {
                cell_x = (cell_x + config_.grid_width) % config_.grid_width;
                cell_y = (cell_y + config_.grid_height) % config_.grid_height;
            }
            
            if (IsValidCell(cell_x, cell_y)) {
                std::lock_guard<std::mutex> lock(grid_[cell_x][cell_y]->mutex);
                result.insert(result.end(),
                            grid_[cell_x][cell_y]->entities.begin(),
                            grid_[cell_x][cell_y]->entities.end());
            }
        }
    }
    
    return result;
}

// [SEQUENCE: 9] Convert world position to cell coordinates
std::pair<int, int> WorldGrid::GetCellCoordinates(const core::utils::Vector3& position) const {
    int x = GetCellIndex(position.x, config_.world_min_x, config_.grid_width, config_.cell_size);
    int y = GetCellIndex(position.y, config_.world_min_y, config_.grid_height, config_.cell_size);
    return {x, y};
}

// [SEQUENCE: 10] Check if cell coordinates are valid
bool WorldGrid::IsValidCell(int x, int y) const {
    return x >= 0 && x < config_.grid_width && y >= 0 && y < config_.grid_height;
}

// [SEQUENCE: 11] Get total entity count
size_t WorldGrid::GetEntityCount() const {
    std::lock_guard<std::mutex> lock(entity_map_mutex_);
    return entity_cells_.size();
}

// [SEQUENCE: 12] Get occupied cell count
size_t WorldGrid::GetOccupiedCellCount() const {
    size_t count = 0;
    for (const auto& column : grid_) {
        for (const auto& cell : column) {
            std::lock_guard<std::mutex> lock(cell->mutex);
            if (!cell->entities.empty()) {
                count++;
            }
        }
    }
    return count;
}

// [SEQUENCE: 13] Get cell bounds for visualization
void WorldGrid::GetCellBounds(int x, int y, core::utils::Vector3& min, core::utils::Vector3& max) const {
    min.x = config_.world_min_x + x * config_.cell_size;
    min.y = config_.world_min_y + y * config_.cell_size;
    min.z = 0;
    
    max.x = min.x + config_.cell_size;
    max.y = min.y + config_.cell_size;
    max.z = 0;
}

// [SEQUENCE: 14] Get all occupied cells
std::vector<std::pair<int, int>> WorldGrid::GetOccupiedCells() const {
    std::vector<std::pair<int, int>> occupied;
    
    for (int x = 0; x < config_.grid_width; ++x) {
        for (int y = 0; y < config_.grid_height; ++y) {
            std::lock_guard<std::mutex> lock(grid_[x][y]->mutex);
            if (!grid_[x][y]->entities.empty()) {
                occupied.emplace_back(x, y);
            }
        }
    }
    
    return occupied;
}

// [SEQUENCE: 15] Helper: Convert world coordinate to cell index
int WorldGrid::GetCellIndex(float world_coord, float world_min, [[maybe_unused]] int grid_size, float cell_size) const {
    return static_cast<int>((world_coord - world_min) / cell_size);
}

// [SEQUENCE: 16] Helper: Get cells potentially containing entities within radius
void WorldGrid::GetCellsInRadius(const core::utils::Vector3& center, float radius,
                               std::vector<std::pair<int, int>>& cells) const {
    // Calculate cell range
    int min_x = GetCellIndex(center.x - radius, config_.world_min_x, config_.grid_width, config_.cell_size);
    int max_x = GetCellIndex(center.x + radius, config_.world_min_x, config_.grid_width, config_.cell_size);
    int min_y = GetCellIndex(center.y - radius, config_.world_min_y, config_.grid_height, config_.cell_size);
    int max_y = GetCellIndex(center.y + radius, config_.world_min_y, config_.grid_height, config_.cell_size);
    
    // Clamp to grid bounds
    min_x = std::max(0, min_x);
    max_x = std::min(config_.grid_width - 1, max_x);
    min_y = std::max(0, min_y);
    max_y = std::min(config_.grid_height - 1, max_y);
    
    // Add all cells in range
    for (int x = min_x; x <= max_x; ++x) {
        for (int y = min_y; y <= max_y; ++y) {
            // Optional: Check if cell actually intersects circle
            if (DistanceSquaredToCell(center, x, y) <= radius * radius) {
                cells.emplace_back(x, y);
            }
        }
    }
}

// [SEQUENCE: 17] Helper: Calculate minimum distance squared from point to cell
float WorldGrid::DistanceSquaredToCell(const core::utils::Vector3& point, int cell_x, int cell_y) const {
    core::utils::Vector3 cell_min, cell_max;
    GetCellBounds(cell_x, cell_y, cell_min, cell_max);
    
    // Find closest point in cell to the given point
    float closest_x = std::max(cell_min.x, std::min(point.x, cell_max.x));
    float closest_y = std::max(cell_min.y, std::min(point.y, cell_max.y));
    
    float dx = point.x - closest_x;
    float dy = point.y - closest_y;
    
    return dx * dx + dy * dy;
}

} // namespace mmorpg::game::world::grid
