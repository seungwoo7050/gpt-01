#include "terrain_collision.h"
#include <fstream>
#include <nlohmann/json.hpp>

namespace mmorpg::game::world {

using json = nlohmann::json;

// [SEQUENCE: MVP7-119] Load map collision data from file
void TerrainCollision::LoadMapCollision(uint32_t map_id, const std::string& file_path) {
    std::ifstream f(file_path);
    json data = json::parse(f);
    
    MapCollisionData collision_data;
    collision_data.width = data.at("width").get<int>();
    collision_data.height = data.at("height").get<int>();
    collision_data.grid_size = data.at("grid_size").get<float>();
    
    const auto& grid_data = data.at("grid");
    collision_data.grid.reserve(grid_data.size());
    
    for (const auto& cell_item : grid_data) {
        CollisionCell cell;
        cell.type = static_cast<TerrainType>(cell_item.at("type").get<uint8_t>());
        cell.height = cell_item.at("height").get<float>();
        collision_data.grid.push_back(cell);
    }
    
    map_data_[map_id] = std::move(collision_data);
}

// [SEQUENCE: MVP7-120] Get a specific cell from the collision grid
const CollisionCell* TerrainCollision::GetCell(uint32_t map_id, int grid_x, int grid_y) const {
    auto it = map_data_.find(map_id);
    if (it == map_data_.end()) {
        return nullptr;
    }
    
    const auto& data = it->second;
    if (grid_x < 0 || grid_x >= data.width || grid_y < 0 || grid_y >= data.height) {
        return nullptr;
    }
    
    return &data.grid[grid_y * data.width + grid_x];
}

// [SEQUENCE: MVP7-121] Check if a world position is walkable
bool TerrainCollision::IsWalkable(uint32_t map_id, float x, float y) const {
    auto it = map_data_.find(map_id);
    if (it == map_data_.end()) {
        return false; // Assume non-walkable if map data not loaded
    }
    
    const auto& data = it->second;
    int grid_x = static_cast<int>(x / data.grid_size);
    int grid_y = static_cast<int>(y / data.grid_size);
    
    const auto* cell = GetCell(map_id, grid_x, grid_y);
    return cell && cell->type == TerrainType::WALKABLE;
}

// [SEQUENCE: MVP7-122] Get terrain height at a world position
float TerrainCollision::GetHeight(uint32_t map_id, float x, float y) const {
    auto it = map_data_.find(map_id);
    if (it == map_data_.end()) {
        return 0.0f;
    }
    
    const auto& data = it->second;
    int grid_x = static_cast<int>(x / data.grid_size);
    int grid_y = static_cast<int>(y / data.grid_size);
    
    // Implement bilinear interpolation for smoother height transitions
    const auto* cell = GetCell(map_id, grid_x, grid_y);
    return cell ? cell->height : 0.0f;
}

// [SEQUENCE: MVP7-123] Check line of sight using Bresenham's algorithm
bool TerrainCollision::HasLineOfSight(uint32_t map_id, const core::types::Vector2& start, const core::types::Vector2& end) const {
    auto it = map_data_.find(map_id);
    if (it == map_data_.end()) {
        return false;
    }
    
    const auto& data = it->second;
    int x0 = static_cast<int>(start.x / data.grid_size);
    int y0 = static_cast<int>(start.y / data.grid_size);
    int x1 = static_cast<int>(end.x / data.grid_size);
    int y1 = static_cast<int>(end.y / data.grid_size);
    
    // Bresenham's line algorithm implementation
    // ...
    
    return true; // Placeholder
}

} // namespace mmorpg::game::world
