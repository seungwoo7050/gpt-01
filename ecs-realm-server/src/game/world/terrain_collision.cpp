#include "terrain_collision.h"
#include <spdlog/spdlog.h>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <queue>

namespace mmorpg::game::world {

// [SEQUENCE: 775] HeightMap - Initialize height map
void HeightMap::Initialize(uint32_t width, uint32_t height, float cell_size) {
    width_ = width;
    height_ = height;
    cell_size_ = cell_size;
    height_data_.resize(width * height, 0.0f);
}

// [SEQUENCE: 776] HeightMap - Get height at position
float HeightMap::GetHeightAt(float x, float y) const {
    // Convert world coordinates to grid coordinates
    float grid_x = x / cell_size_;
    float grid_y = y / cell_size_;
    
    // Use bilinear interpolation for smooth height
    return InterpolateHeight(grid_x, grid_y);
}

// [SEQUENCE: 777] HeightMap - Set height data
void HeightMap::SetHeightData(const std::vector<float>& heights) {
    if (heights.size() != width_ * height_) {
        spdlog::error("Height data size mismatch: expected {}, got {}", 
                     width_ * height_, heights.size());
        return;
    }
    height_data_ = heights;
}

// [SEQUENCE: 778] HeightMap - Calculate slope
float HeightMap::CalculateSlope(float x1, float y1, float x2, float y2) const {
    float h1 = GetHeightAt(x1, y1);
    float h2 = GetHeightAt(x2, y2);
    
    float horizontal_distance = std::sqrt((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
    if (horizontal_distance < 0.001f) {
        return 0.0f;
    }
    
    float height_difference = std::abs(h2 - h1);
    return std::atan(height_difference / horizontal_distance) * (180.0f / M_PI);  // Degrees
}

// [SEQUENCE: 779] HeightMap - Check valid position
bool HeightMap::IsValidPosition(float x, float y) const {
    float grid_x = x / cell_size_;
    float grid_y = y / cell_size_;
    
    return grid_x >= 0 && grid_x < width_ - 1 &&
           grid_y >= 0 && grid_y < height_ - 1;
}

// [SEQUENCE: 780] HeightMap - Bilinear interpolation
float HeightMap::InterpolateHeight(float x, float y) const {
    // Clamp to valid range
    x = std::clamp(x, 0.0f, static_cast<float>(width_ - 1));
    y = std::clamp(y, 0.0f, static_cast<float>(height_ - 1));
    
    int x0 = static_cast<int>(x);
    int y0 = static_cast<int>(y);
    int x1 = std::min(x0 + 1, static_cast<int>(width_ - 1));
    int y1 = std::min(y0 + 1, static_cast<int>(height_ - 1));
    
    float fx = x - x0;
    float fy = y - y0;
    
    // Get heights at four corners
    float h00 = height_data_[y0 * width_ + x0];
    float h10 = height_data_[y0 * width_ + x1];
    float h01 = height_data_[y1 * width_ + x0];
    float h11 = height_data_[y1 * width_ + x1];
    
    // Bilinear interpolation
    float h0 = h00 * (1 - fx) + h10 * fx;
    float h1 = h01 * (1 - fx) + h11 * fx;
    
    return h0 * (1 - fy) + h1 * fy;
}

// [SEQUENCE: 781] TerrainCollisionManager - Load collision map
bool TerrainCollisionManager::LoadCollisionMap(uint32_t map_id, const std::string& file_path) {
    auto collision_map = std::make_unique<CollisionMap>();
    
    // 실제로는 파일에서 로드하지만, 여기서는 예시 데이터 생성
    collision_map->width = 1000;
    collision_map->height = 1000;
    collision_map->cell_size = 1.0f;
    collision_map->origin_x = 0.0f;
    collision_map->origin_y = 0.0f;
    
    // Initialize cells
    collision_map->cells.resize(collision_map->width * collision_map->height);
    
    // Create height map
    collision_map->height_map = std::make_unique<HeightMap>();
    collision_map->height_map->Initialize(collision_map->width, collision_map->height, 
                                         collision_map->cell_size);
    
    // Example: Create some terrain features
    for (uint32_t y = 0; y < collision_map->height; ++y) {
        for (uint32_t x = 0; x < collision_map->width; ++x) {
            uint32_t index = y * collision_map->width + x;
            auto& cell = collision_map->cells[index];
            
            // Default walkable
            cell.properties.type = TerrainType::WALKABLE;
            
            // Create a river
            if (x >= 480 && x <= 520 && y >= 200 && y <= 800) {
                cell.properties.type = TerrainType::WATER_DEEP;
                cell.properties.requires_swimming = true;
                cell.properties.movement_modifier = 0.5f;
            }
            
            // Create some blocked areas (buildings)
            if ((x >= 100 && x <= 150 && y >= 100 && y <= 150) ||
                (x >= 300 && x <= 400 && y >= 300 && y <= 350)) {
                cell.properties.type = TerrainType::BLOCKED;
            }
            
            // Create a lava zone
            if (x >= 700 && x <= 750 && y >= 700 && y <= 750) {
                cell.properties.type = TerrainType::LAVA;
                cell.properties.damage_per_second = 10.0f;
                cell.properties.damage_type = "fire";
            }
        }
    }
    
    collision_maps_[map_id] = std::move(collision_map);
    
    spdlog::info("Loaded collision map for map_id: {}", map_id);
    return true;
}

// [SEQUENCE: 782] TerrainCollisionManager - Check movement validity
bool TerrainCollisionManager::CanMoveTo(uint32_t map_id, 
                                       float from_x, float from_y, float from_z,
                                       float to_x, float to_y, float to_z,
                                       uint32_t entity_flags) const {
    auto it = collision_maps_.find(map_id);
    if (it == collision_maps_.end()) {
        return false;
    }
    
    const auto& map = *it->second;
    
    // [SEQUENCE: 783] Convert to grid coordinates
    auto [to_grid_x, to_grid_y] = WorldToGrid(map, to_x, to_y);
    
    if (!IsValidGrid(map, to_grid_x, to_grid_y)) {
        return false;
    }
    
    // [SEQUENCE: 784] Get terrain at destination
    const auto* cell = GetCell(map, to_grid_x, to_grid_y);
    if (!cell) {
        return false;
    }
    
    // Check terrain traversability
    if (!CheckEntityFlags(entity_flags, cell->properties)) {
        return false;
    }
    
    // [SEQUENCE: 785] Check dynamic obstacles
    if (cell->has_dynamic_obstacle) {
        return false;
    }
    
    // [SEQUENCE: 786] Check slope limits (if 3D)
    if (map.height_map) {
        if (!CheckSlopeLimits(map, from_x, from_y, from_z, to_x, to_y, to_z)) {
            return false;
        }
    }
    
    // [SEQUENCE: 787] Line of sight check for large movements
    float distance = std::sqrt((to_x - from_x) * (to_x - from_x) + 
                              (to_y - from_y) * (to_y - from_y));
    if (distance > map.cell_size * 2) {
        // Check intermediate points
        int steps = static_cast<int>(distance / map.cell_size) + 1;
        for (int i = 1; i < steps; ++i) {
            float t = static_cast<float>(i) / steps;
            float check_x = from_x + (to_x - from_x) * t;
            float check_y = from_y + (to_y - from_y) * t;
            
            auto [check_grid_x, check_grid_y] = WorldToGrid(map, check_x, check_y);
            const auto* check_cell = GetCell(map, check_grid_x, check_grid_y);
            
            if (!check_cell || !CheckEntityFlags(entity_flags, check_cell->properties)) {
                return false;
            }
        }
    }
    
    return true;
}

// [SEQUENCE: 788] TerrainCollisionManager - Get terrain type
TerrainType TerrainCollisionManager::GetTerrainType(uint32_t map_id, float x, float y) const {
    auto it = collision_maps_.find(map_id);
    if (it == collision_maps_.end()) {
        return TerrainType::BLOCKED;
    }
    
    const auto& map = *it->second;
    auto [grid_x, grid_y] = WorldToGrid(map, x, y);
    
    const auto* cell = GetCell(map, grid_x, grid_y);
    return cell ? cell->properties.type : TerrainType::BLOCKED;
}

// [SEQUENCE: 789] TerrainCollisionManager - Get terrain properties
std::optional<TerrainProperties> TerrainCollisionManager::GetTerrainProperties(
    uint32_t map_id, float x, float y) const {
    auto it = collision_maps_.find(map_id);
    if (it == collision_maps_.end()) {
        return std::nullopt;
    }
    
    const auto& map = *it->second;
    auto [grid_x, grid_y] = WorldToGrid(map, x, y);
    
    const auto* cell = GetCell(map, grid_x, grid_y);
    return cell ? std::optional(cell->properties) : std::nullopt;
}

// [SEQUENCE: 790] TerrainCollisionManager - Line of sight check
bool TerrainCollisionManager::HasLineOfSight(uint32_t map_id,
                                            float x1, float y1, float z1,
                                            float x2, float y2, float z2) const {
    auto it = collision_maps_.find(map_id);
    if (it == collision_maps_.end()) {
        return false;
    }
    
    const auto& map = *it->second;
    
    // Bresenham's line algorithm for grid traversal
    auto [start_x, start_y] = WorldToGrid(map, x1, y1);
    auto [end_x, end_y] = WorldToGrid(map, x2, y2);
    
    int dx = std::abs(end_x - start_x);
    int dy = std::abs(end_y - start_y);
    int sx = start_x < end_x ? 1 : -1;
    int sy = start_y < end_y ? 1 : -1;
    int err = dx - dy;
    
    int current_x = start_x;
    int current_y = start_y;
    
    while (true) {
        const auto* cell = GetCell(map, current_x, current_y);
        if (!cell || cell->properties.type == TerrainType::BLOCKED) {
            return false;
        }
        
        if (current_x == end_x && current_y == end_y) {
            break;
        }
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            current_x += sx;
        }
        if (e2 < dx) {
            err += dx;
            current_y += sy;
        }
    }
    
    return true;
}

// [SEQUENCE: 791] TerrainCollisionManager - Find nearest walkable position
std::optional<std::tuple<float, float, float>> 
TerrainCollisionManager::FindNearestWalkablePosition(uint32_t map_id, 
                                                    float x, float y, float z,
                                                    float search_radius) const {
    auto it = collision_maps_.find(map_id);
    if (it == collision_maps_.end()) {
        return std::nullopt;
    }
    
    const auto& map = *it->second;
    auto [center_x, center_y] = WorldToGrid(map, x, y);
    
    int search_cells = static_cast<int>(search_radius / map.cell_size);
    
    // Spiral search pattern
    for (int radius = 1; radius <= search_cells; ++radius) {
        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                // Only check cells on the current radius
                if (std::abs(dx) != radius && std::abs(dy) != radius) {
                    continue;
                }
                
                int check_x = center_x + dx;
                int check_y = center_y + dy;
                
                const auto* cell = GetCell(map, check_x, check_y);
                if (cell && cell->properties.type == TerrainType::WALKABLE &&
                    !cell->has_dynamic_obstacle) {
                    float world_x = check_x * map.cell_size + map.origin_x;
                    float world_y = check_y * map.cell_size + map.origin_y;
                    float world_z = z;
                    
                    if (map.height_map) {
                        world_z = map.height_map->GetHeightAt(world_x, world_y);
                    }
                    
                    return std::make_tuple(world_x, world_y, world_z);
                }
            }
        }
    }
    
    return std::nullopt;
}

// [SEQUENCE: 792] TerrainCollisionManager - Add dynamic obstacle
void TerrainCollisionManager::AddDynamicObstacle(uint32_t map_id, uint32_t obstacle_id,
                                                float x, float y, float width, float height) {
    auto it = collision_maps_.find(map_id);
    if (it == collision_maps_.end()) {
        return;
    }
    
    auto& map = *it->second;
    
    // Store obstacle info
    CollisionMap::DynamicObstacle obstacle{obstacle_id, x, y, width, height};
    map.dynamic_obstacles[obstacle_id] = obstacle;
    
    // Mark affected cells
    int start_x = static_cast<int>((x - width/2 - map.origin_x) / map.cell_size);
    int start_y = static_cast<int>((y - height/2 - map.origin_y) / map.cell_size);
    int end_x = static_cast<int>((x + width/2 - map.origin_x) / map.cell_size);
    int end_y = static_cast<int>((y + height/2 - map.origin_y) / map.cell_size);
    
    for (int cy = start_y; cy <= end_y; ++cy) {
        for (int cx = start_x; cx <= end_x; ++cx) {
            if (IsValidGrid(map, cx, cy)) {
                uint32_t index = cy * map.width + cx;
                map.cells[index].has_dynamic_obstacle = true;
                map.cells[index].obstacle_id = obstacle_id;
            }
        }
    }
    
    spdlog::debug("Added dynamic obstacle {} at ({}, {}) size {}x{}", 
                  obstacle_id, x, y, width, height);
}

// [SEQUENCE: 793] TerrainCollisionManager - Remove dynamic obstacle
void TerrainCollisionManager::RemoveDynamicObstacle(uint32_t map_id, uint32_t obstacle_id) {
    auto it = collision_maps_.find(map_id);
    if (it == collision_maps_.end()) {
        return;
    }
    
    auto& map = *it->second;
    auto obstacle_it = map.dynamic_obstacles.find(obstacle_id);
    if (obstacle_it == map.dynamic_obstacles.end()) {
        return;
    }
    
    // Clear affected cells
    for (auto& cell : map.cells) {
        if (cell.obstacle_id == obstacle_id) {
            cell.has_dynamic_obstacle = false;
            cell.obstacle_id = 0;
        }
    }
    
    map.dynamic_obstacles.erase(obstacle_it);
    spdlog::debug("Removed dynamic obstacle {}", obstacle_id);
}

// [SEQUENCE: 794] TerrainCollisionManager - Path validation
bool TerrainCollisionManager::ValidatePath(uint32_t map_id,
                                          const std::vector<std::pair<float, float>>& path,
                                          uint32_t entity_flags) const {
    if (path.size() < 2) {
        return true;
    }
    
    // Check each segment
    for (size_t i = 0; i < path.size() - 1; ++i) {
        float from_x = path[i].first;
        float from_y = path[i].second;
        float to_x = path[i + 1].first;
        float to_y = path[i + 1].second;
        
        if (!CanMoveTo(map_id, from_x, from_y, 0, to_x, to_y, 0, entity_flags)) {
            return false;
        }
    }
    
    return true;
}

// [SEQUENCE: 795] TerrainCollisionManager - World to grid conversion
std::pair<int, int> TerrainCollisionManager::WorldToGrid(const CollisionMap& map, 
                                                         float x, float y) const {
    int grid_x = static_cast<int>((x - map.origin_x) / map.cell_size);
    int grid_y = static_cast<int>((y - map.origin_y) / map.cell_size);
    return {grid_x, grid_y};
}

// [SEQUENCE: 796] TerrainCollisionManager - Grid validation
bool TerrainCollisionManager::IsValidGrid(const CollisionMap& map, int grid_x, int grid_y) const {
    return grid_x >= 0 && grid_x < static_cast<int>(map.width) &&
           grid_y >= 0 && grid_y < static_cast<int>(map.height);
}

// [SEQUENCE: 797] TerrainCollisionManager - Get cell
const CollisionCell* TerrainCollisionManager::GetCell(const CollisionMap& map, 
                                                      int x, int y) const {
    if (!IsValidGrid(map, x, y)) {
        return nullptr;
    }
    return &map.cells[y * map.width + x];
}

// [SEQUENCE: 798] TerrainCollisionManager - Check entity movement capabilities
bool TerrainCollisionManager::CheckEntityFlags(uint32_t entity_flags, 
                                              const TerrainProperties& terrain) const {
    // Ghost mode can pass through anything
    if (entity_flags & EntityMovementFlags::GHOST_MODE) {
        return true;
    }
    
    switch (terrain.type) {
        case TerrainType::BLOCKED:
            return false;
            
        case TerrainType::WATER_SHALLOW:
            return true;  // Anyone can walk in shallow water
            
        case TerrainType::WATER_DEEP:
            return (entity_flags & EntityMovementFlags::CAN_SWIM) ||
                   (entity_flags & EntityMovementFlags::CAN_FLY) ||
                   (entity_flags & EntityMovementFlags::CAN_WALK_ON_WATER);
                   
        case TerrainType::LAVA:
            return (entity_flags & EntityMovementFlags::IMMUNE_TO_LAVA) ||
                   (entity_flags & EntityMovementFlags::CAN_FLY);
                   
        case TerrainType::CLIFF:
            return (entity_flags & EntityMovementFlags::CAN_FLY) ||
                   (entity_flags & EntityMovementFlags::CAN_CLIMB);
                   
        case TerrainType::SLOPE_STEEP:
            return (entity_flags & EntityMovementFlags::CAN_CLIMB) ||
                   (entity_flags & EntityMovementFlags::CAN_FLY);
                   
        default:
            return true;
    }
}

// [SEQUENCE: 799] TerrainCollisionManager - Check slope limits
bool TerrainCollisionManager::CheckSlopeLimits(const CollisionMap& map,
                                              float from_x, float from_y, float from_z,
                                              float to_x, float to_y, float to_z) const {
    if (!map.height_map) {
        return true;
    }
    
    float slope = map.height_map->CalculateSlope(from_x, from_y, to_x, to_y);
    
    // Maximum walkable slope is 45 degrees
    const float MAX_WALKABLE_SLOPE = 45.0f;
    
    return slope <= MAX_WALKABLE_SLOPE;
}

// [SEQUENCE: 800] CollisionQueryOptimizer - Process batch queries
std::vector<CollisionQueryOptimizer::CollisionResult> 
CollisionQueryOptimizer::ProcessBatch(uint32_t map_id,
                                     const std::vector<CollisionQuery>& queries) {
    std::vector<CollisionResult> results;
    results.reserve(queries.size());
    
    auto& terrain_mgr = TerrainCollisionManager::Instance();
    
    for (const auto& query : queries) {
        CollisionResult result;
        result.entity_id = query.entity_id;
        
        // Check movement
        result.can_move = terrain_mgr.CanMoveTo(map_id, 
                                               query.from_x, query.from_y, query.from_z,
                                               query.to_x, query.to_y, query.to_z,
                                               query.flags);
        
        if (result.can_move) {
            result.adjusted_x = query.to_x;
            result.adjusted_y = query.to_y;
            result.adjusted_z = query.to_z;
            result.terrain_type = terrain_mgr.GetTerrainType(map_id, query.to_x, query.to_y);
        } else {
            // Find nearest valid position
            auto nearest = terrain_mgr.FindNearestWalkablePosition(map_id, 
                                                                  query.to_x, query.to_y, query.to_z);
            if (nearest) {
                std::tie(result.adjusted_x, result.adjusted_y, result.adjusted_z) = *nearest;
                result.terrain_type = TerrainType::WALKABLE;
            } else {
                result.adjusted_x = query.from_x;
                result.adjusted_y = query.from_y;
                result.adjusted_z = query.from_z;
                result.terrain_type = terrain_mgr.GetTerrainType(map_id, query.from_x, query.from_y);
            }
        }
        
        results.push_back(result);
    }
    
    return results;
}

} // namespace mmorpg::game::world