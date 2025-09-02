#pragma once

#include <vector>
#include "core/types.h"
#include "core/singleton.h"

namespace mmorpg::game::world {

// [SEQUENCE: MVP7-112] Terrain type flags
enum class TerrainType : uint8_t {
    WALKABLE = 0,
    BLOCKED = 1,
    WATER = 2,
    LAVA = 3,
    // ... more types
};

// [SEQUENCE: MVP7-113] Collision grid cell
struct CollisionCell {
    TerrainType type;
    float height;
};

// [SEQUENCE: MVP7-114] Terrain collision manager
class TerrainCollision : public Singleton<TerrainCollision> {
public:
    // [SEQUENCE: MVP7-115] Load collision data for a map
    void LoadMapCollision(uint32_t map_id, const std::string& file_path);
    
    // [SEQUENCE: MVP7-116] Check if a position is walkable
    bool IsWalkable(uint32_t map_id, float x, float y) const;
    
    // [SEQUENCE: MVP7-117] Get the height of the terrain at a position
    float GetHeight(uint32_t map_id, float x, float y) const;
    
    // [SEQUENCE: MVP7-118] Check for line of sight between two points
    bool HasLineOfSight(uint32_t map_id, const core::types::Vector2& start, const core::types::Vector2& end) const;

private:
    friend class Singleton<TerrainCollision>;
    TerrainCollision() = default;

    struct MapCollisionData {
        int width;
        int height;
        float grid_size;
        std::vector<CollisionCell> grid;
    };

    const CollisionCell* GetCell(uint32_t map_id, int grid_x, int grid_y) const;

    std::unordered_map<uint32_t, MapCollisionData> map_data_;
};

} // namespace mmorpg::game::world
