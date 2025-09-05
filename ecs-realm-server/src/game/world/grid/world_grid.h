#pragma once

#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <array>
#include <mutex>
#include "core/ecs/types.h"
#include "core/utils/vector3.h"

namespace mmorpg::game::world::grid {

// [SEQUENCE: MVP3-1] Defines the WorldGrid class, a spatial partitioning structure using a uniform 2D grid.
class WorldGrid {
public:
    // [SEQUENCE: MVP3-2] Configuration struct for WorldGrid initialization.
    struct Config {
        float cell_size = 100.0f;          // Size of each grid cell
        int grid_width = 100;              // Number of cells horizontally
        int grid_height = 100;             // Number of cells vertically
        float world_min_x = 0.0f;          // World boundaries
        float world_min_y = 0.0f;
        bool wrap_around = false;          // Wrap at world edges
    };
    
    // [SEQUENCE: MVP3-3] Constructor and destructor.
    explicit WorldGrid(const Config& config);
    ~WorldGrid() = default;
    
    // [SEQUENCE: MVP3-4] Public API for entity management in the grid.
    void AddEntity(core::ecs::EntityId entity, const core::utils::Vector3& position);
    void RemoveEntity(core::ecs::EntityId entity);
    void UpdateEntity(core::ecs::EntityId entity, const core::utils::Vector3& old_pos, 
                     const core::utils::Vector3& new_pos);
    
    // [SEQUENCE: MVP3-5] Public API for performing spatial queries.
    std::vector<core::ecs::EntityId> GetEntitiesInRadius(
        const core::utils::Vector3& center, float radius) const;
    
    std::vector<core::ecs::EntityId> GetEntitiesInBox(
        const core::utils::Vector3& min, const core::utils::Vector3& max) const;
    
    std::vector<core::ecs::EntityId> GetEntitiesInCell(int x, int y) const;
    
    std::vector<core::ecs::EntityId> GetEntitiesInAdjacentCells(
        const core::utils::Vector3& position, int range = 1) const;
    
    // [SEQUENCE: MVP3-6] Public API for utility and debugging functions.
    std::pair<int, int> GetCellCoordinates(const core::utils::Vector3& position) const;
    bool IsValidCell(int x, int y) const;
    size_t GetEntityCount() const;
    size_t GetOccupiedCellCount() const;
    void GetCellBounds(int x, int y, core::utils::Vector3& min, core::utils::Vector3& max) const;
    std::vector<std::pair<int, int>> GetOccupiedCells() const;
    
private:
    // [SEQUENCE: MVP3-7] Internal GridCell struct to hold entities within a cell.
    struct GridCell {
        std::unordered_set<core::ecs::EntityId> entities;
        mutable std::mutex mutex;  // Per-cell locking for thread safety
    };
    
    // [SEQUENCE: MVP3-8] Private member variables for grid storage and configuration.
    Config config_;
    std::vector<std::vector<std::unique_ptr<GridCell>>> grid_;
    std::unordered_map<core::ecs::EntityId, std::pair<int, int>> entity_cells_;
    mutable std::mutex entity_map_mutex_;
    
    // [SEQUENCE: MVP3-9] Private helper methods for internal grid calculations.
    int GetCellIndex(float world_coord, float world_min, int grid_size, float cell_size) const;
    void GetCellsInRadius(const core::utils::Vector3& center, float radius, 
                         std::vector<std::pair<int, int>>& cells) const;
    float DistanceSquaredToCell(const core::utils::Vector3& point, int cell_x, int cell_y) const;
};

} // namespace mmorpg::game::world::grid