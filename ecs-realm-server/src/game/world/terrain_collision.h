#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include <optional>

namespace mmorpg::game::world {

// [SEQUENCE: 741] Terrain collision system
// 지형 충돌 검사를 위한 시스템. 실제로는 클라이언트가 상세 처리하지만,
// 서버도 기본적인 이동 검증을 수행해야 함.

// [SEQUENCE: 742] Terrain types
enum class TerrainType : uint8_t {
    WALKABLE,       // 보행 가능
    BLOCKED,        // 완전 차단
    WATER_SHALLOW,  // 얕은 물 (이동 속도 감소)
    WATER_DEEP,     // 깊은 물 (수영 능력 필요)
    LAVA,           // 용암 (지속 피해)
    CLIFF,          // 절벽
    SLOPE_MILD,     // 완만한 경사 (약간 느려짐)
    SLOPE_STEEP,    // 가파른 경사 (특수 능력 필요)
    ICE,            // 빙판 (미끄러짐)
    QUICKSAND,      // 늪 (점진적 하강)
    TELEPORTER,     // 텔레포터
    DAMAGE_ZONE     // 피해 지역
};

// [SEQUENCE: 743] Terrain properties
struct TerrainProperties {
    TerrainType type = TerrainType::WALKABLE;
    float movement_modifier = 1.0f;     // 이동 속도 배수
    float height = 0.0f;                // 지형 높이
    
    // 특수 속성
    bool requires_flying = false;       // 비행 필요
    bool requires_swimming = false;     // 수영 필요
    bool causes_sliding = false;        // 미끄러짐
    
    // 피해 정보
    float damage_per_second = 0.0f;
    std::string damage_type;            // "fire", "poison" 등
    
    // 추가 데이터 (텔레포터 목적지 등)
    std::unordered_map<std::string, std::string> metadata;
};

// [SEQUENCE: 744] Collision cell
struct CollisionCell {
    TerrainProperties properties;
    
    // 동적 장애물 (서버가 실시간으로 관리)
    bool has_dynamic_obstacle = false;
    uint32_t obstacle_id = 0;
};

// [SEQUENCE: 745] Height map for 3D terrain
class HeightMap {
public:
    // [SEQUENCE: 746] Initialize height map
    void Initialize(uint32_t width, uint32_t height, float cell_size);
    
    // [SEQUENCE: 747] Get height at position
    float GetHeightAt(float x, float y) const;
    
    // [SEQUENCE: 748] Set height data
    void SetHeightData(const std::vector<float>& heights);
    
    // [SEQUENCE: 749] Calculate slope between two points
    float CalculateSlope(float x1, float y1, float x2, float y2) const;
    
    // [SEQUENCE: 750] Check if position is on valid terrain
    bool IsValidPosition(float x, float y) const;
    
private:
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    float cell_size_ = 1.0f;
    std::vector<float> height_data_;
    
    // [SEQUENCE: 751] Bilinear interpolation for smooth height
    float InterpolateHeight(float x, float y) const;
};

// [SEQUENCE: 752] Terrain collision manager
class TerrainCollisionManager {
public:
    static TerrainCollisionManager& Instance() {
        static TerrainCollisionManager instance;
        return instance;
    }
    
    // [SEQUENCE: 753] Load collision map for a zone
    bool LoadCollisionMap(uint32_t map_id, const std::string& file_path);
    
    // [SEQUENCE: 754] Check if movement is valid
    bool CanMoveTo(uint32_t map_id, float from_x, float from_y, float from_z,
                   float to_x, float to_y, float to_z,
                   uint32_t entity_flags = 0) const;
    
    // [SEQUENCE: 755] Get terrain type at position
    TerrainType GetTerrainType(uint32_t map_id, float x, float y) const;
    
    // [SEQUENCE: 756] Get terrain properties
    std::optional<TerrainProperties> GetTerrainProperties(uint32_t map_id, 
                                                          float x, float y) const;
    
    // [SEQUENCE: 757] Ray cast for line of sight
    bool HasLineOfSight(uint32_t map_id, 
                       float x1, float y1, float z1,
                       float x2, float y2, float z2) const;
    
    // [SEQUENCE: 758] Find nearest walkable position
    std::optional<std::tuple<float, float, float>> 
    FindNearestWalkablePosition(uint32_t map_id, float x, float y, float z,
                               float search_radius = 10.0f) const;
    
    // [SEQUENCE: 759] Add/remove dynamic obstacles
    void AddDynamicObstacle(uint32_t map_id, uint32_t obstacle_id,
                           float x, float y, float width, float height);
    void RemoveDynamicObstacle(uint32_t map_id, uint32_t obstacle_id);
    
    // [SEQUENCE: 760] Path validation (simple version)
    bool ValidatePath(uint32_t map_id, 
                     const std::vector<std::pair<float, float>>& path,
                     uint32_t entity_flags = 0) const;
    
private:
    TerrainCollisionManager() = default;
    
    // [SEQUENCE: 761] Collision map data structure
    struct CollisionMap {
        uint32_t width = 0;
        uint32_t height = 0;
        float cell_size = 1.0f;
        float origin_x = 0.0f;
        float origin_y = 0.0f;
        
        std::vector<CollisionCell> cells;
        std::unique_ptr<HeightMap> height_map;
        
        // Dynamic obstacles
        struct DynamicObstacle {
            uint32_t id;
            float x, y, width, height;
        };
        std::unordered_map<uint32_t, DynamicObstacle> dynamic_obstacles;
    };
    
    // [SEQUENCE: 762] Maps storage
    std::unordered_map<uint32_t, std::unique_ptr<CollisionMap>> collision_maps_;
    
    // [SEQUENCE: 763] Helper functions
    std::pair<int, int> WorldToGrid(const CollisionMap& map, float x, float y) const;
    bool IsValidGrid(const CollisionMap& map, int grid_x, int grid_y) const;
    const CollisionCell* GetCell(const CollisionMap& map, int x, int y) const;
    
    // [SEQUENCE: 764] Movement validation helpers
    bool CheckEntityFlags(uint32_t entity_flags, const TerrainProperties& terrain) const;
    bool CheckSlopeLimits(const CollisionMap& map, 
                         float from_x, float from_y, float from_z,
                         float to_x, float to_y, float to_z) const;
};

// [SEQUENCE: 765] Entity movement flags
namespace EntityMovementFlags {
    constexpr uint32_t CAN_FLY = 1 << 0;
    constexpr uint32_t CAN_SWIM = 1 << 1;
    constexpr uint32_t CAN_CLIMB = 1 << 2;
    constexpr uint32_t IMMUNE_TO_LAVA = 1 << 3;
    constexpr uint32_t IMMUNE_TO_WATER = 1 << 4;
    constexpr uint32_t CAN_WALK_ON_WATER = 1 << 5;
    constexpr uint32_t GHOST_MODE = 1 << 6;  // 모든 지형 통과
}

// [SEQUENCE: 766] Collision query optimizer
class CollisionQueryOptimizer {
public:
    // [SEQUENCE: 767] Batch collision checks
    struct CollisionQuery {
        uint32_t entity_id;
        float from_x, from_y, from_z;
        float to_x, to_y, to_z;
        uint32_t flags;
    };
    
    struct CollisionResult {
        uint32_t entity_id;
        bool can_move;
        TerrainType terrain_type;
        float adjusted_x, adjusted_y, adjusted_z;  // 조정된 위치
    };
    
    // [SEQUENCE: 768] Process batch queries
    std::vector<CollisionResult> ProcessBatch(uint32_t map_id,
                                             const std::vector<CollisionQuery>& queries);
    
private:
    // [SEQUENCE: 769] Spatial hashing for optimization
    void BuildSpatialIndex(uint32_t map_id);
    
    struct SpatialHash {
        std::unordered_map<uint64_t, std::vector<uint32_t>> buckets;
        float bucket_size = 10.0f;
    };
    
    std::unordered_map<uint32_t, SpatialHash> spatial_indices_;
};

// [SEQUENCE: 770] Simple navigation mesh (NavMesh) support
class NavMesh {
public:
    // [SEQUENCE: 771] Navigation polygon
    struct NavPoly {
        std::vector<std::pair<float, float>> vertices;
        std::vector<uint32_t> neighbors;
        float center_x, center_y;
        TerrainType terrain_type;
    };
    
    // [SEQUENCE: 772] Build nav mesh from collision map
    void BuildFromCollisionMap(const CollisionMap& collision_map);
    
    // [SEQUENCE: 773] Find path (A* algorithm)
    std::optional<std::vector<std::pair<float, float>>> 
    FindPath(float start_x, float start_y, float end_x, float end_y,
             uint32_t movement_flags = 0) const;
    
private:
    std::vector<NavPoly> polygons_;
    
    // [SEQUENCE: 774] A* pathfinding helpers
    struct PathNode {
        uint32_t poly_id;
        float g_cost;
        float h_cost;
        float f_cost() const { return g_cost + h_cost; }
        uint32_t parent;
    };
};

} // namespace mmorpg::game::world