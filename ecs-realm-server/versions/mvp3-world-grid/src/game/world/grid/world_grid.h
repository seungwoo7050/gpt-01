#pragma once

#include <vector>
#include <unordered_set>
#include <array>
#include <mutex>
#include "core/ecs/types.h"
#include "core/utils/vector3.h"

namespace mmorpg::game::world::grid {

// [SEQUENCE: 1] Grid-based spatial partitioning for 2D worlds
class WorldGrid {
public:
    // [SEQUENCE: 2] Configuration
    struct Config {
        float cell_size = 100.0f;          // Size of each grid cell
        int grid_width = 100;              // Number of cells horizontally
        int grid_height = 100;             // Number of cells vertically
        float world_min_x = 0.0f;          // World boundaries
        float world_min_y = 0.0f;
        bool wrap_around = false;          // Wrap at world edges
    };
    
    // [SEQUENCE: 3] Constructor with configuration
    explicit WorldGrid(const Config& config);
    ~WorldGrid() = default;
    
    // [SEQUENCE: 4] Entity management
    void AddEntity(core::ecs::EntityId entity, const core::utils::Vector3& position);
    void RemoveEntity(core::ecs::EntityId entity);
    void UpdateEntity(core::ecs::EntityId entity, const core::utils::Vector3& old_pos, 
                     const core::utils::Vector3& new_pos);
    
    // [SEQUENCE: 5] Spatial queries
    std::vector<core::ecs::EntityId> GetEntitiesInRadius(
        const core::utils::Vector3& center, float radius) const;
    
    std::vector<core::ecs::EntityId> GetEntitiesInBox(
        const core::utils::Vector3& min, const core::utils::Vector3& max) const;
    
    std::vector<core::ecs::EntityId> GetEntitiesInCell(int x, int y) const;
    
    // [SEQUENCE: 6] Neighbor queries
    std::vector<core::ecs::EntityId> GetEntitiesInAdjacentCells(
        const core::utils::Vector3& position, int range = 1) const;
    
    // [SEQUENCE: 7] Utility functions
    std::pair<int, int> GetCellCoordinates(const core::utils::Vector3& position) const;
    bool IsValidCell(int x, int y) const;
    size_t GetEntityCount() const;
    size_t GetOccupiedCellCount() const;
    
    // [SEQUENCE: 8] Debug/visualization
    void GetCellBounds(int x, int y, core::utils::Vector3& min, core::utils::Vector3& max) const;
    std::vector<std::pair<int, int>> GetOccupiedCells() const;
    
private:
    // [SEQUENCE: 9] Grid cell containing entities
    struct GridCell {
        std::unordered_set<core::ecs::EntityId> entities;
        mutable std::mutex mutex;  // Per-cell locking for thread safety
    };
    
    // [SEQUENCE: 10] Grid storage
    Config config_;
    std::vector<std::vector<GridCell>> grid_;
    
    // [SEQUENCE: 11] Entity to cell mapping for fast updates
    std::unordered_map<core::ecs::EntityId, std::pair<int, int>> entity_cells_;
    mutable std::mutex entity_map_mutex_;
    
    // [SEQUENCE: 12] Helper methods
    int GetCellIndex(float world_coord, float world_min, int grid_size, float cell_size) const;
    void GetCellsInRadius(const core::utils::Vector3& center, float radius, 
                         std::vector<std::pair<int, int>>& cells) const;
    float DistanceSquaredToCell(const core::utils::Vector3& point, int cell_x, int cell_y) const;
};

} // namespace mmorpg::game::world::grid