#pragma once

#include <vector>
#include <unordered_set>
#include <memory>
#include <mutex>
#include "core/ecs/types.h"
#include "core/utils/vector3.h"

#pragma once

#include <vector>
#include <unordered_set>
#include <memory>
#include <mutex>
#include "core/ecs/types.h"
#include "game/world/ispatial_index.h"
#include "core/utils/vector3.h"

// [SEQUENCE: MVP3-18] octree_world.h: Octree-based spatial partitioning for 3D worlds
class OctreeWorld : public ISpatialIndex {
public:
    // [SEQUENCE: MVP3-19] octree_world.h: Configuration
    struct Config {
        core::utils::Vector3 world_min;      // World boundaries min
        core::utils::Vector3 world_max;      // World boundaries max
        size_t max_depth = 8;                // Maximum tree depth
        size_t max_entities_per_node = 16;   // Split threshold
        float min_node_size = 12.5f;         // Minimum node dimension
    };
    
    // [SEQUENCE: MVP3-20] octree_world.h: Constructor with configuration
    explicit OctreeWorld(const Config& config);
    ~OctreeWorld();
    
    // [SEQUENCE: MVP3-21] octree_world.h: Entity management
    void AddEntity(core::ecs::EntityId entity, const core::utils::Vector3& position);
    void RemoveEntity(core::ecs::EntityId entity);
    void UpdateEntity(core::ecs::EntityId entity, const core::utils::Vector3& old_pos, 
                     const core::utils::Vector3& new_pos);
    
    // [SEQUENCE: MVP3-22] octree_world.h: 3D Spatial queries
    std::vector<core::ecs::EntityId> GetEntitiesInRadius(
        const core::utils::Vector3& center, float radius) const;
    
    std::vector<core::ecs::EntityId> GetEntitiesInBox(
        const core::utils::Vector3& min, const core::utils::Vector3& max) const;
    
    std::vector<core::ecs::EntityId> GetEntitiesInFrustum(
        const core::utils::Vector3& origin, const core::utils::Vector3& direction,
        float fov, float near_dist, float far_dist) const;
    
    std::vector<core::ecs::EntityId> GetEntitiesAlongRay(
        const core::utils::Vector3& origin, const core::utils::Vector3& direction,
        float max_distance) const;
    
    size_t GetEntityCount() const;
    size_t GetNodeCount() const;
    size_t GetDepth() const;
    void GetTreeStats(size_t& total_nodes, size_t& leaf_nodes, size_t& entities) const;
    
    struct NodeInfo {
        core::utils::Vector3 min;
        core::utils::Vector3 max;
        size_t depth;
        size_t entity_count;
        bool is_leaf;
    };
    std::vector<NodeInfo> GetNodeInfos() const;
    
private:
    
    struct OctreeNode {
        core::utils::Vector3 min;            // Node boundaries
        core::utils::Vector3 max;
        core::utils::Vector3 center;         // Pre-calculated center
        
        std::unordered_set<core::ecs::EntityId> entities;  // Entities in this node
        std::unique_ptr<OctreeNode> children[8];           // Child nodes (nullptr if leaf)
        
        size_t depth;                        // Current depth in tree
        bool is_leaf = true;                 // Leaf node flag
        
        mutable std::mutex mutex;            // Thread safety
        
        OctreeNode(const core::utils::Vector3& node_min, 
                  const core::utils::Vector3& node_max,
                  size_t node_depth);
        
        int GetChildIndex(const core::utils::Vector3& position) const;
        
        bool Contains(const core::utils::Vector3& point) const;
        
        bool IntersectsSphere(const core::utils::Vector3& center, float radius) const;
        
        bool IntersectsBox(const core::utils::Vector3& box_min, 
                          const core::utils::Vector3& box_max) const;
    };
    
    Config config_;
    std::unique_ptr<OctreeNode> root_;
    
    struct EntityData {
        core::utils::Vector3 position;
        OctreeNode* node;  // Current node containing entity
    };
    std::unordered_map<core::ecs::EntityId, EntityData> entity_data_;
    mutable std::mutex entity_map_mutex_;
    
    void InsertEntity(OctreeNode* node, core::ecs::EntityId entity, 
                     const core::utils::Vector3& position);
    void RemoveEntity(OctreeNode* node, core::ecs::EntityId entity);
    void SplitNode(OctreeNode* node);
    void TryMergeNode(OctreeNode* node);
    
    void QueryRadius(const OctreeNode* node, const core::utils::Vector3& center, 
                    float radius, std::vector<core::ecs::EntityId>& results) const;
    void QueryBox(const OctreeNode* node, const core::utils::Vector3& box_min,
                 const core::utils::Vector3& box_max, 
                 std::vector<core::ecs::EntityId>& results) const;
    void QueryRay(const OctreeNode* node, const core::utils::Vector3& origin,
                 const core::utils::Vector3& direction, float max_distance,
                 std::vector<core::ecs::EntityId>& results) const;
    
    void CollectStats(const OctreeNode* node, size_t& total_nodes, 
                     size_t& leaf_nodes, size_t& entities) const;
    void CollectNodeInfos(const OctreeNode* node, 
                         std::vector<NodeInfo>& infos) const;
};

} // namespace mmorpg::game::world::octree
} // namespace mmorpg::game::world::octreereectree::game::world::octree