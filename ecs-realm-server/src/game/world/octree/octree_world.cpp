#include "game/world/octree/octree_world.h"
#include <spdlog/spdlog.h>
#include <cmath>
#include <algorithm>
#include <queue>

namespace mmorpg::game::world::octree {

// [SEQUENCE: MVP3-40] Implements the OctreeNode constructor.
OctreeWorld::OctreeNode::OctreeNode(const core::utils::Vector3& node_min,
                                    const core::utils::Vector3& node_max,
                                    size_t node_depth)
    : min(node_min), max(node_max), depth(node_depth) {
    
    // Pre-calculate center for efficiency
    center.x = (min.x + max.x) * 0.5f;
    center.y = (min.y + max.y) * 0.5f;
    center.z = (min.z + max.z) * 0.5f;
}

// [SEQUENCE: MVP3-41] Implements GetChildIndex to determine which octant a position belongs to.
int OctreeWorld::OctreeNode::GetChildIndex(const core::utils::Vector3& position) const {
    int index = 0;
    if (position.x > center.x) index |= 1;
    if (position.y > center.y) index |= 2;
    if (position.z > center.z) index |= 4;
    return index;
}

// [SEQUENCE: MVP3-42] Implements Contains to check if a point is within the node's bounds.
bool OctreeWorld::OctreeNode::Contains(const core::utils::Vector3& point) const {
    return point.x >= min.x && point.x <= max.x &&
           point.y >= min.y && point.y <= max.y &&
           point.z >= min.z && point.z <= max.z;
}

// [SEQUENCE: MVP3-43] Implements IntersectsSphere for sphere-box intersection tests.
bool OctreeWorld::OctreeNode::IntersectsSphere(const core::utils::Vector3& sphere_center, 
                                               float radius) const {
    // Find closest point in box to sphere center
    float closest_x = std::max(min.x, std::min(sphere_center.x, max.x));
    float closest_y = std::max(min.y, std::min(sphere_center.y, max.y));
    float closest_z = std::max(min.z, std::min(sphere_center.z, max.z));
    
    // Check if distance is less than radius
    float dx = sphere_center.x - closest_x;
    float dy = sphere_center.y - closest_y;
    float dz = sphere_center.z - closest_z;
    
    return (dx * dx + dy * dy + dz * dz) <= (radius * radius);
}

// [SEQUENCE: MVP3-44] Implements IntersectsBox for box-box intersection tests.
bool OctreeWorld::OctreeNode::IntersectsBox(const core::utils::Vector3& box_min,
                                           const core::utils::Vector3& box_max) const {
    return !(box_max.x < min.x || box_min.x > max.x ||
             box_max.y < min.y || box_min.y > max.y ||
             box_max.z < min.z || box_min.z > max.z);
}

// [SEQUENCE: MVP3-45] Implements the OctreeWorld constructor, creating the root node.
OctreeWorld::OctreeWorld(const Config& config) : config_(config) {
    // Create root node covering entire world
    root_ = std::make_unique<OctreeNode>(config.world_min, config.world_max, 0);
    
    spdlog::info("OctreeWorld initialized: bounds ({}, {}, {}) to ({}, {}, {}), max depth {}",
                 config.world_min.x, config.world_min.y, config.world_min.z,
                 config.world_max.x, config.world_max.y, config.world_max.z,
                 config.max_depth);
}

// [SEQUENCE: MVP3-46] Implements the OctreeWorld destructor.
OctreeWorld::~OctreeWorld() = default;

// [SEQUENCE: MVP3-47] Implements AddEntity, the public method to add an entity to the octree.
void OctreeWorld::AddEntity(core::ecs::EntityId entity, const core::utils::Vector3& position) {
    if (!root_->Contains(position)) {
        spdlog::warn("Entity {} position {} outside world bounds", entity, position);
        return;
    }
    
    // Insert into tree
    InsertEntity(root_.get(), entity, position);
    
    // Track entity data
    {
        std::lock_guard<std::mutex> lock(entity_map_mutex_);
        entity_data_[entity] = {position, nullptr}; // Node pointer set during insertion
    }
    
    spdlog::debug("Added entity {} to octree at position ({}, {}, {})",
                 entity, position.x, position.y, position.z);
}

// [SEQUENCE: MVP3-48] Implements RemoveEntity, the public method to remove an entity.
void OctreeWorld::RemoveEntity(core::ecs::EntityId entity) {
    OctreeNode* node = nullptr;
    
    // Find entity's node
    {
        std::lock_guard<std::mutex> lock(entity_map_mutex_);
        auto it = entity_data_.find(entity);
        if (it == entity_data_.end()) {
            return;
        }
        node = it->second.node;
        entity_data_.erase(it);
    }
    
    // Remove from node
    if (node) {
        RemoveEntity(node, entity);
    }
    
    spdlog::debug("Removed entity {} from octree", entity);
}

// [SEQUENCE: MVP3-49] Implements UpdateEntity to handle entity movement.
void OctreeWorld::UpdateEntity(core::ecs::EntityId entity,
                              [[maybe_unused]] const core::utils::Vector3& old_pos,
                              const core::utils::Vector3& new_pos) {
    
    // Check if still in same node (common case)
    OctreeNode* current_node = nullptr;
    {
        std::lock_guard<std::mutex> lock(entity_map_mutex_);
        auto it = entity_data_.find(entity);
        if (it != entity_data_.end()) {
            current_node = it->second.node;
        }
    }
    
    if (current_node && current_node->Contains(new_pos)) {
        // Still in same node, just update position
        std::lock_guard<std::mutex> lock(entity_map_mutex_);
        entity_data_[entity].position = new_pos;
        return;
    }
    
    // Need to move to different node
    RemoveEntity(entity);
    AddEntity(entity, new_pos);
}

// [SEQUENCE: MVP3-50] Implements GetEntitiesInRadius, initiating a recursive radius query.
std::vector<core::ecs::EntityId> OctreeWorld::GetEntitiesInRadius(
    const core::utils::Vector3& center, float radius) const {
    
    std::vector<core::ecs::EntityId> results;
    if (root_) {
        QueryRadius(root_.get(), center, radius, results);
    }
    return results;
}

// [SEQUENCE: MVP3-51] Implements GetEntitiesInBox, initiating a recursive box query.
std::vector<core::ecs::EntityId> OctreeWorld::GetEntitiesInBox(
    const core::utils::Vector3& box_min, const core::utils::Vector3& box_max) const {
    
    std::vector<core::ecs::EntityId> results;
    if (root_) {
        QueryBox(root_.get(), box_min, box_max, results);
    }
    return results;
}

// [SEQUENCE: MVP3-52] Placeholder implementation for frustum-based queries.
std::vector<core::ecs::EntityId> OctreeWorld::GetEntitiesInFrustum(
    [[maybe_unused]] const core::utils::Vector3& origin, [[maybe_unused]] const core::utils::Vector3& direction,
    [[maybe_unused]] float fov, [[maybe_unused]] float near_dist, [[maybe_unused]] float far_dist) const {
    // TODO: Implement frustum culling
    return {};
}

// [SEQUENCE: MVP3-53] Placeholder implementation for ray-based queries.
std::vector<core::ecs::EntityId> OctreeWorld::GetEntitiesAlongRay(
    [[maybe_unused]] const core::utils::Vector3& origin, [[maybe_unused]] const core::utils::Vector3& direction,
    [[maybe_unused]] float max_distance) const {
    // TODO: Implement ray traversal
    return {};
}

// [SEQUENCE: MVP3-54] Implements GetTreeStats to initiate recursive statistics collection.
void OctreeWorld::GetTreeStats(size_t& total_nodes, size_t& leaf_nodes, 
                              size_t& entities) const {
    total_nodes = 0;
    leaf_nodes = 0;
    entities = 0;
    
    if (root_) {
        CollectStats(root_.get(), total_nodes, leaf_nodes, entities);
    }
}

// [SEQUENCE: MVP3-55] Implements GetEntityCount for a quick entity count.
size_t OctreeWorld::GetEntityCount() const {
    std::lock_guard<std::mutex> lock(entity_map_mutex_);
    return entity_data_.size();
}

// [SEQUENCE: MVP3-56] Implements GetNodeCount to get the total number of nodes in the tree.
size_t OctreeWorld::GetNodeCount() const {
    size_t total, leaf, entities;
    GetTreeStats(total, leaf, entities);
    return total;
}

// [SEQUENCE: MVP3-57] Implements GetDepth to calculate the maximum depth of the tree.
size_t OctreeWorld::GetDepth() const {
    size_t max_depth = 0;
    if (root_) {
        std::queue<const OctreeNode*> queue;
        queue.push(root_.get());
        
        while (!queue.empty()) {
            const OctreeNode* node = queue.front();
            queue.pop();
            
            max_depth = std::max(max_depth, node->depth);
            
            if (!node->is_leaf) {
                for (int i = 0; i < 8; ++i) {
                    if (node->children[i]) {
                        queue.push(node->children[i].get());
                    }
                }
            }
        }
    }
    return max_depth;
}

// [SEQUENCE: MVP3-58] Implements GetNodeInfos to collect debug information for all nodes.
std::vector<OctreeWorld::NodeInfo> OctreeWorld::GetNodeInfos() const {
    std::vector<NodeInfo> infos;
    if (root_) {
        CollectNodeInfos(root_.get(), infos);
    }
    return infos;
}

// [SEQUENCE: MVP3-59] Private helper to recursively insert an entity into a node.
void OctreeWorld::InsertEntity(OctreeNode* node, core::ecs::EntityId entity,
                              const core::utils::Vector3& position) {
    std::lock_guard<std::mutex> lock(node->mutex);
    
    if (node->is_leaf) {
        node->entities.insert(entity);
        
        {
            std::lock_guard<std::mutex> map_lock(entity_map_mutex_);
            if (auto it = entity_data_.find(entity); it != entity_data_.end()) {
                it->second.node = node;
            }
        }
        
        if (node->entities.size() > config_.max_entities_per_node &&
            node->depth < config_.max_depth) {
            
            float node_size = std::min({node->max.x - node->min.x,
                                       node->max.y - node->min.y,
                                       node->max.z - node->min.z});
            
            if (node_size > config_.min_node_size) {
                SplitNode(node);
            }
        }
    } else {
        int child_index = node->GetChildIndex(position);
        if (!node->children[child_index]) {
            spdlog::error("Missing child node at index {}", child_index);
            return;
        }
        InsertEntity(node->children[child_index].get(), entity, position);
    }
}

// [SEQUENCE: MVP3-60] Private helper to remove an entity from a specific node.
void OctreeWorld::RemoveEntity(OctreeNode* node, core::ecs::EntityId entity) {
    std::lock_guard<std::mutex> lock(node->mutex);
    
    node->entities.erase(entity);
    
    if (!node->is_leaf) {
        TryMergeNode(node);
    }
}

// [SEQUENCE: MVP3-61] Private helper to split a node into eight children.
void OctreeWorld::SplitNode(OctreeNode* node) {
    node->is_leaf = false;
    
    for (int i = 0; i < 8; ++i) {
        core::utils::Vector3 child_min, child_max;
        
        child_min.x = (i & 1) ? node->center.x : node->min.x;
        child_max.x = (i & 1) ? node->max.x : node->center.x;
        
        child_min.y = (i & 2) ? node->center.y : node->min.y;
        child_max.y = (i & 2) ? node->max.y : node->center.y;
        
        child_min.z = (i & 4) ? node->center.z : node->min.z;
        child_max.z = (i & 4) ? node->max.z : node->center.z;
        
        node->children[i] = std::make_unique<OctreeNode>(child_min, child_max, node->depth + 1);
    }
    
    auto entities_copy = node->entities;
    node->entities.clear();
    
    for (core::ecs::EntityId entity : entities_copy) {
        core::utils::Vector3 pos;
        {
            std::lock_guard<std::mutex> map_lock(entity_map_mutex_);
            if (auto it = entity_data_.find(entity); it != entity_data_.end()) {
                pos = it->second.position;
            } else {
                continue;
            }
        }
        
        int child_index = node->GetChildIndex(pos);
        InsertEntity(node->children[child_index].get(), entity, pos);
    }
    
    spdlog::debug("Split octree node at depth {} with {} entities",
                 node->depth, entities_copy.size());
}

// [SEQUENCE: MVP3-62] Private helper to merge child nodes back into the parent.
void OctreeWorld::TryMergeNode(OctreeNode* node) {
    size_t total_entities = 0;
    for (int i = 0; i < 8; ++i) {
        if (node->children[i]) {
            if (!node->children[i]->is_leaf) {
                return; // Can't merge if any child is not a leaf
            }
            total_entities += node->children[i]->entities.size();
        }
    }
    
    if (total_entities <= config_.max_entities_per_node / 2) {
        for (int i = 0; i < 8; ++i) {
            if (node->children[i]) {
                for (core::ecs::EntityId entity : node->children[i]->entities) {
                    node->entities.insert(entity);
                    
                    std::lock_guard<std::mutex> map_lock(entity_map_mutex_);
                    if (auto it = entity_data_.find(entity); it != entity_data_.end()) {
                        it->second.node = node;
                    }
                }
                node->children[i].reset();
            }
        }
        
        node->is_leaf = true;
        spdlog::debug("Merged octree node at depth {} with {} entities",
                     node->depth, node->entities.size());
    }
}

// [SEQUENCE: MVP3-63] Private helper to recursively query for entities within a radius.
void OctreeWorld::QueryRadius(const OctreeNode* node, const core::utils::Vector3& center,
                             float radius, std::vector<core::ecs::EntityId>& results) const {
    
    if (!node->IntersectsSphere(center, radius)) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(node->mutex);
    
    if (!node->entities.empty()) {
        float radius_squared = radius * radius;
        
        for (core::ecs::EntityId entity : node->entities) {
            core::utils::Vector3 pos;
            {
                std::lock_guard<std::mutex> map_lock(entity_map_mutex_);
                if (auto it = entity_data_.find(entity); it != entity_data_.end()) {
                    pos = it->second.position;
                } else {
                    continue;
                }
            }
            
            float dx = pos.x - center.x;
            float dy = pos.y - center.y;
            float dz = pos.z - center.z;
            
            if (dx * dx + dy * dy + dz * dz <= radius_squared) {
                results.push_back(entity);
            }
        }
    }
    
    if (!node->is_leaf) {
        for (int i = 0; i < 8; ++i) {
            if (node->children[i]) {
                QueryRadius(node->children[i].get(), center, radius, results);
            }
        }
    }
}

// [SEQUENCE: MVP3-64] Private helper to recursively query for entities within a box.
void OctreeWorld::QueryBox(const OctreeNode* node, const core::utils::Vector3& box_min,
                          const core::utils::Vector3& box_max,
                          std::vector<core::ecs::EntityId>& results) const {
    
    if (!node->IntersectsBox(box_min, box_max)) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(node->mutex);
    
    for (core::ecs::EntityId entity : node->entities) {
        core::utils::Vector3 pos;
        {
            std::lock_guard<std::mutex> map_lock(entity_map_mutex_);
            if (auto it = entity_data_.find(entity); it != entity_data_.end()) {
                pos = it->second.position;
            } else {
                continue;
            }
        }
        
        if (pos.x >= box_min.x && pos.x <= box_max.x &&
            pos.y >= box_min.y && pos.y <= box_max.y &&
            pos.z >= box_min.z && pos.z <= box_max.z) {
            results.push_back(entity);
        }
    }
    
    if (!node->is_leaf) {
        for (int i = 0; i < 8; ++i) {
            if (node->children[i]) {
                QueryBox(node->children[i].get(), box_min, box_max, results);
            }
        }
    }
}

// [SEQUENCE: MVP3-65] Private helper to recursively collect tree statistics.
void OctreeWorld::CollectStats(const OctreeNode* node, size_t& total_nodes,
                              size_t& leaf_nodes, size_t& entities) const {
    std::lock_guard<std::mutex> lock(node->mutex);
    
    total_nodes++;
    if (node->is_leaf) {
        leaf_nodes++;
    }
    entities += node->entities.size();
    
    if (!node->is_leaf) {
        for (int i = 0; i < 8; ++i) {
            if (node->children[i]) {
                CollectStats(node->children[i].get(), total_nodes, leaf_nodes, entities);
            }
        }
    }
}

// [SEQUENCE: MVP3-66] Private helper to recursively collect node debug information.
void OctreeWorld::CollectNodeInfos(const OctreeNode* node,
                                  std::vector<NodeInfo>& infos) const {
    std::lock_guard<std::mutex> lock(node->mutex);
    
    NodeInfo info;
    info.min = node->min;
    info.max = node->max;
    info.depth = node->depth;
    info.entity_count = node->entities.size();
    info.is_leaf = node->is_leaf;
    infos.push_back(info);
    
    if (!node->is_leaf) {
        for (int i = 0; i < 8; ++i) {
            if (node->children[i]) {
                CollectNodeInfos(node->children[i].get(), infos);
            }
        }
    }
}

} // namespace mmorpg::game::world::octree