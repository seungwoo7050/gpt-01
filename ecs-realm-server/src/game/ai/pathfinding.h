#pragma once

#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <algorithm>
#include <memory>
#include <optional>
#include <spdlog/spdlog.h>

namespace mmorpg::game::ai {

// [SEQUENCE: 2212] A* pathfinding implementation for AI navigation
// A* 길찾기 구현 - AI 네비게이션을 위한 시스템

// [SEQUENCE: 2213] Navigation node
struct NavNode {
    int32_t x, y;               // Grid coordinates
    float z;                    // Height
    bool walkable = true;       // 이동 가능 여부
    float cost_modifier = 1.0f; // 이동 비용 수정자 (물, 진흙 등)
    
    // [SEQUENCE: 2214] Node equality for hashing
    bool operator==(const NavNode& other) const {
        return x == other.x && y == other.y;
    }
};

// [SEQUENCE: 2215] Hash function for NavNode
struct NavNodeHash {
    size_t operator()(const NavNode& node) const {
        return std::hash<int32_t>()(node.x) ^ (std::hash<int32_t>()(node.y) << 1);
    }
};

// [SEQUENCE: 2216] Path node for A* algorithm
struct PathNode {
    NavNode node;
    float g_cost = 0;           // Cost from start
    float h_cost = 0;           // Heuristic cost to goal
    float f_cost = 0;           // Total cost (g + h)
    PathNode* parent = nullptr; // Parent node for path reconstruction
    
    // [SEQUENCE: 2217] Calculate f cost
    void CalculateFCost() {
        f_cost = g_cost + h_cost;
    }
};

// [SEQUENCE: 2218] Priority queue comparator for A*
struct PathNodeCompare {
    bool operator()(const PathNode* a, const PathNode* b) const {
        return a->f_cost > b->f_cost;  // Min heap
    }
};

// [SEQUENCE: 2219] Navigation grid
class NavigationGrid {
public:
    NavigationGrid(uint32_t width, uint32_t height, float cell_size = 1.0f)
        : width_(width), height_(height), cell_size_(cell_size) {
        
        // Initialize grid
        grid_.resize(width * height);
        
        // Default all cells to walkable
        for (auto& cell : grid_) {
            cell.walkable = true;
        }
    }
    
    // [SEQUENCE: 2220] Set cell walkability
    void SetWalkable(int32_t x, int32_t y, bool walkable) {
        if (IsValidCell(x, y)) {
            grid_[GetIndex(x, y)].walkable = walkable;
        }
    }
    
    // [SEQUENCE: 2221] Set cell cost modifier
    void SetCostModifier(int32_t x, int32_t y, float modifier) {
        if (IsValidCell(x, y)) {
            grid_[GetIndex(x, y)].cost_modifier = modifier;
        }
    }
    
    // [SEQUENCE: 2222] Get navigation node
    NavNode* GetNode(int32_t x, int32_t y) {
        if (IsValidCell(x, y)) {
            auto& node = grid_[GetIndex(x, y)];
            node.x = x;
            node.y = y;
            return &node;
        }
        return nullptr;
    }
    
    // [SEQUENCE: 2223] Check if cell is walkable
    bool IsWalkable(int32_t x, int32_t y) const {
        if (!IsValidCell(x, y)) return false;
        return grid_[GetIndex(x, y)].walkable;
    }
    
    // [SEQUENCE: 2224] Get neighbors of a node
    std::vector<NavNode*> GetNeighbors(int32_t x, int32_t y) {
        std::vector<NavNode*> neighbors;
        
        // 8-directional movement
        static const int32_t dx[] = {-1, -1, -1,  0,  0,  1,  1,  1};
        static const int32_t dy[] = {-1,  0,  1, -1,  1, -1,  0,  1};
        
        for (int i = 0; i < 8; ++i) {
            int32_t nx = x + dx[i];
            int32_t ny = y + dy[i];
            
            if (IsWalkable(nx, ny)) {
                // Check diagonal movement is valid (no corner cutting)
                if (i == 0 || i == 2 || i == 5 || i == 7) {  // Diagonal
                    if (!IsWalkable(x + dx[i], y) || !IsWalkable(x, y + dy[i])) {
                        continue;  // Can't cut corners
                    }
                }
                
                neighbors.push_back(GetNode(nx, ny));
            }
        }
        
        return neighbors;
    }
    
    // [SEQUENCE: 2225] Convert world position to grid coordinates
    void WorldToGrid(float world_x, float world_y, int32_t& grid_x, int32_t& grid_y) const {
        grid_x = static_cast<int32_t>(world_x / cell_size_);
        grid_y = static_cast<int32_t>(world_y / cell_size_);
    }
    
    // [SEQUENCE: 2226] Convert grid coordinates to world position
    void GridToWorld(int32_t grid_x, int32_t grid_y, float& world_x, float& world_y) const {
        world_x = (grid_x + 0.5f) * cell_size_;
        world_y = (grid_y + 0.5f) * cell_size_;
    }
    
    // [SEQUENCE: 2227] Create from collision data
    void GenerateFromCollisionMap(const std::vector<bool>& collision_map) {
        if (collision_map.size() != grid_.size()) {
            spdlog::error("Collision map size mismatch");
            return;
        }
        
        for (size_t i = 0; i < collision_map.size(); ++i) {
            grid_[i].walkable = !collision_map[i];
        }
    }
    
private:
    uint32_t width_, height_;
    float cell_size_;
    std::vector<NavNode> grid_;
    
    size_t GetIndex(int32_t x, int32_t y) const {
        return y * width_ + x;
    }
    
    bool IsValidCell(int32_t x, int32_t y) const {
        return x >= 0 && x < static_cast<int32_t>(width_) && 
               y >= 0 && y < static_cast<int32_t>(height_);
    }
};

// [SEQUENCE: 2228] A* pathfinding algorithm
class AStarPathfinder {
public:
    AStarPathfinder(NavigationGrid* nav_grid) : nav_grid_(nav_grid) {}
    
    // [SEQUENCE: 2229] Find path from start to goal
    std::vector<std::pair<float, float>> FindPath(
        float start_x, float start_y, float goal_x, float goal_y) {
        
        // Convert to grid coordinates
        int32_t start_gx, start_gy, goal_gx, goal_gy;
        nav_grid_->WorldToGrid(start_x, start_y, start_gx, start_gy);
        nav_grid_->WorldToGrid(goal_x, goal_y, goal_gx, goal_gy);
        
        // Validate start and goal
        if (!nav_grid_->IsWalkable(start_gx, start_gy) || 
            !nav_grid_->IsWalkable(goal_gx, goal_gy)) {
            spdlog::warn("Start or goal position is not walkable");
            return {};
        }
        
        // A* algorithm
        std::priority_queue<PathNode*, std::vector<PathNode*>, PathNodeCompare> open_set;
        std::unordered_map<NavNode, PathNode*, NavNodeHash> all_nodes;
        std::unordered_set<NavNode, NavNodeHash> closed_set;
        
        // Create start node
        auto start_nav = nav_grid_->GetNode(start_gx, start_gy);
        if (!start_nav) return {};
        
        PathNode* start_node = GetOrCreateNode(*start_nav, all_nodes);
        start_node->g_cost = 0;
        start_node->h_cost = HeuristicCost(start_gx, start_gy, goal_gx, goal_gy);
        start_node->CalculateFCost();
        open_set.push(start_node);
        
        // Path found flag
        PathNode* goal_node = nullptr;
        
        // [SEQUENCE: 2230] Main A* loop
        while (!open_set.empty()) {
            // Get node with lowest f cost
            PathNode* current = open_set.top();
            open_set.pop();
            
            // Skip if already processed
            if (closed_set.count(current->node) > 0) {
                continue;
            }
            
            // Add to closed set
            closed_set.insert(current->node);
            
            // Check if reached goal
            if (current->node.x == goal_gx && current->node.y == goal_gy) {
                goal_node = current;
                break;
            }
            
            // Process neighbors
            auto neighbors = nav_grid_->GetNeighbors(current->node.x, current->node.y);
            for (NavNode* neighbor_nav : neighbors) {
                // Skip if in closed set
                if (closed_set.count(*neighbor_nav) > 0) {
                    continue;
                }
                
                // Calculate tentative g cost
                float tentative_g = current->g_cost + 
                    CalculateMoveCost(current->node, *neighbor_nav);
                
                // Get or create neighbor node
                PathNode* neighbor = GetOrCreateNode(*neighbor_nav, all_nodes);
                
                // Update if better path found
                if (tentative_g < neighbor->g_cost || neighbor->g_cost == 0) {
                    neighbor->parent = current;
                    neighbor->g_cost = tentative_g;
                    neighbor->h_cost = HeuristicCost(
                        neighbor_nav->x, neighbor_nav->y, goal_gx, goal_gy);
                    neighbor->CalculateFCost();
                    
                    // Add to open set
                    open_set.push(neighbor);
                }
            }
        }
        
        // [SEQUENCE: 2231] Reconstruct path if found
        if (goal_node) {
            return ReconstructPath(goal_node);
        }
        
        // Cleanup
        for (auto& pair : all_nodes) {
            delete pair.second;
        }
        
        return {};  // No path found
    }
    
    // [SEQUENCE: 2232] Find partial path (best effort)
    std::vector<std::pair<float, float>> FindPartialPath(
        float start_x, float start_y, float goal_x, float goal_y, 
        uint32_t max_iterations = 1000) {
        
        // Similar to FindPath but with iteration limit
        // Returns path to closest reachable point to goal
        // Implementation details omitted for brevity
        return FindPath(start_x, start_y, goal_x, goal_y);
    }
    
    // [SEQUENCE: 2233] Smooth path using line of sight
    std::vector<std::pair<float, float>> SmoothPath(
        const std::vector<std::pair<float, float>>& path) {
        
        if (path.size() <= 2) return path;
        
        std::vector<std::pair<float, float>> smoothed;
        smoothed.push_back(path[0]);
        
        size_t current = 0;
        while (current < path.size() - 1) {
            size_t farthest = current + 1;
            
            // Find farthest visible point
            for (size_t i = current + 2; i < path.size(); ++i) {
                if (HasLineOfSight(path[current].first, path[current].second,
                                   path[i].first, path[i].second)) {
                    farthest = i;
                } else {
                    break;
                }
            }
            
            smoothed.push_back(path[farthest]);
            current = farthest;
        }
        
        return smoothed;
    }
    
private:
    NavigationGrid* nav_grid_;
    
    // [SEQUENCE: 2234] Heuristic function (Euclidean distance)
    float HeuristicCost(int32_t x1, int32_t y1, int32_t x2, int32_t y2) const {
        float dx = static_cast<float>(x2 - x1);
        float dy = static_cast<float>(y2 - y1);
        return std::sqrt(dx * dx + dy * dy);
    }
    
    // [SEQUENCE: 2235] Calculate movement cost
    float CalculateMoveCost(const NavNode& from, const NavNode& to) const {
        float base_cost = 1.0f;
        
        // Diagonal movement costs more
        if (from.x != to.x && from.y != to.y) {
            base_cost = 1.414f;  // sqrt(2)
        }
        
        // Apply terrain cost modifier
        return base_cost * to.cost_modifier;
    }
    
    // [SEQUENCE: 2236] Get or create path node
    PathNode* GetOrCreateNode(const NavNode& nav_node, 
                              std::unordered_map<NavNode, PathNode*, NavNodeHash>& all_nodes) {
        auto it = all_nodes.find(nav_node);
        if (it != all_nodes.end()) {
            return it->second;
        }
        
        PathNode* new_node = new PathNode();
        new_node->node = nav_node;
        all_nodes[nav_node] = new_node;
        return new_node;
    }
    
    // [SEQUENCE: 2237] Reconstruct path from goal to start
    std::vector<std::pair<float, float>> ReconstructPath(PathNode* goal_node) {
        std::vector<std::pair<float, float>> path;
        
        PathNode* current = goal_node;
        while (current != nullptr) {
            float world_x, world_y;
            nav_grid_->GridToWorld(current->node.x, current->node.y, world_x, world_y);
            path.emplace_back(world_x, world_y);
            current = current->parent;
        }
        
        // Reverse to get start to goal
        std::reverse(path.begin(), path.end());
        
        return path;
    }
    
    // [SEQUENCE: 2238] Check line of sight between two points
    bool HasLineOfSight(float x1, float y1, float x2, float y2) const {
        // Bresenham's line algorithm to check if path is clear
        int32_t gx1, gy1, gx2, gy2;
        nav_grid_->WorldToGrid(x1, y1, gx1, gy1);
        nav_grid_->WorldToGrid(x2, y2, gx2, gy2);
        
        int32_t dx = std::abs(gx2 - gx1);
        int32_t dy = std::abs(gy2 - gy1);
        int32_t sx = (gx1 < gx2) ? 1 : -1;
        int32_t sy = (gy1 < gy2) ? 1 : -1;
        int32_t err = dx - dy;
        
        while (true) {
            if (!nav_grid_->IsWalkable(gx1, gy1)) {
                return false;
            }
            
            if (gx1 == gx2 && gy1 == gy2) {
                break;
            }
            
            int32_t e2 = 2 * err;
            if (e2 > -dy) {
                err -= dy;
                gx1 += sx;
            }
            if (e2 < dx) {
                err += dx;
                gy1 += sy;
            }
        }
        
        return true;
    }
};

// [SEQUENCE: 2239] Path request for async pathfinding
struct PathRequest {
    uint64_t request_id;
    uint64_t entity_id;
    float start_x, start_y;
    float goal_x, goal_y;
    std::function<void(const std::vector<std::pair<float, float>>&)> callback;
    bool smooth_path = true;
    uint32_t priority = 0;  // Higher priority processed first
};

// [SEQUENCE: 2240] Pathfinding manager (singleton)
class PathfindingManager {
public:
    static PathfindingManager& Instance() {
        static PathfindingManager instance;
        return instance;
    }
    
    // [SEQUENCE: 2241] Initialize navigation grid
    void InitializeGrid(uint32_t width, uint32_t height, float cell_size = 1.0f) {
        nav_grid_ = std::make_unique<NavigationGrid>(width, height, cell_size);
        pathfinder_ = std::make_unique<AStarPathfinder>(nav_grid_.get());
        
        spdlog::info("Initialized navigation grid: {}x{}, cell size: {}", 
                    width, height, cell_size);
    }
    
    // [SEQUENCE: 2242] Load navigation data from file
    void LoadNavigationData(const std::string& filename) {
        // Load pre-computed navigation mesh or grid
        // This would typically load from a file generated by level tools
        spdlog::info("Loading navigation data from: {}", filename);
    }
    
    // [SEQUENCE: 2243] Request path (synchronous)
    std::vector<std::pair<float, float>> FindPath(
        float start_x, float start_y, float goal_x, float goal_y, bool smooth = true) {
        
        if (!pathfinder_) {
            spdlog::error("Pathfinder not initialized");
            return {};
        }
        
        auto path = pathfinder_->FindPath(start_x, start_y, goal_x, goal_y);
        
        if (smooth && !path.empty()) {
            path = pathfinder_->SmoothPath(path);
        }
        
        return path;
    }
    
    // [SEQUENCE: 2244] Request path (asynchronous)
    uint64_t RequestPath(uint64_t entity_id, float start_x, float start_y, 
                        float goal_x, float goal_y, 
                        std::function<void(const std::vector<std::pair<float, float>>&)> callback,
                        uint32_t priority = 0) {
        
        PathRequest request;
        request.request_id = next_request_id_++;
        request.entity_id = entity_id;
        request.start_x = start_x;
        request.start_y = start_y;
        request.goal_x = goal_x;
        request.goal_y = goal_y;
        request.callback = callback;
        request.priority = priority;
        
        // In a real implementation, this would be processed on a worker thread
        // For now, process immediately
        ProcessPathRequest(request);
        
        return request.request_id;
    }
    
    // [SEQUENCE: 2245] Update navigation grid
    void UpdateNavigation(int32_t x, int32_t y, bool walkable) {
        if (nav_grid_) {
            nav_grid_->SetWalkable(x, y, walkable);
        }
    }
    
    // [SEQUENCE: 2246] Set terrain cost
    void SetTerrainCost(int32_t x, int32_t y, float cost_modifier) {
        if (nav_grid_) {
            nav_grid_->SetCostModifier(x, y, cost_modifier);
        }
    }
    
    // [SEQUENCE: 2247] Debug visualization
    void GetNavigationDebugData(std::vector<std::pair<int32_t, int32_t>>& walkable_cells,
                               std::vector<std::pair<int32_t, int32_t>>& blocked_cells) {
        // Return grid data for visualization
        // Useful for debugging pathfinding issues
    }
    
private:
    PathfindingManager() = default;
    
    std::unique_ptr<NavigationGrid> nav_grid_;
    std::unique_ptr<AStarPathfinder> pathfinder_;
    std::atomic<uint64_t> next_request_id_{1};
    
    // [SEQUENCE: 2248] Process path request
    void ProcessPathRequest(const PathRequest& request) {
        auto path = FindPath(request.start_x, request.start_y, 
                           request.goal_x, request.goal_y, 
                           request.smooth_path);
        
        // Call callback with result
        if (request.callback) {
            request.callback(path);
        }
    }
};

// [SEQUENCE: 2249] Path follower component for smooth movement
class PathFollower {
public:
    PathFollower(uint64_t entity_id) : entity_id_(entity_id) {}
    
    // [SEQUENCE: 2250] Set path to follow
    void SetPath(const std::vector<std::pair<float, float>>& path) {
        path_ = path;
        current_waypoint_ = 0;
        
        if (!path_.empty()) {
            is_following_ = true;
        }
    }
    
    // [SEQUENCE: 2251] Update movement along path
    bool UpdateMovement(float& out_x, float& out_y, float current_x, float current_y, 
                       float move_speed, float delta_time) {
        
        if (!is_following_ || path_.empty()) {
            return false;
        }
        
        // Check if reached current waypoint
        while (current_waypoint_ < path_.size()) {
            float dx = path_[current_waypoint_].first - current_x;
            float dy = path_[current_waypoint_].second - current_y;
            float distance = std::sqrt(dx * dx + dy * dy);
            
            if (distance < waypoint_threshold_) {
                // Reached waypoint, move to next
                current_waypoint_++;
                
                if (current_waypoint_ >= path_.size()) {
                    // Path complete
                    is_following_ = false;
                    OnPathComplete();
                    return false;
                }
            } else {
                // Move towards current waypoint
                float move_distance = move_speed * delta_time;
                
                if (move_distance >= distance) {
                    // Will reach waypoint this frame
                    out_x = path_[current_waypoint_].first;
                    out_y = path_[current_waypoint_].second;
                } else {
                    // Move towards waypoint
                    float ratio = move_distance / distance;
                    out_x = current_x + dx * ratio;
                    out_y = current_y + dy * ratio;
                }
                
                return true;
            }
        }
        
        return false;
    }
    
    // [SEQUENCE: 2252] Stop following path
    void StopFollowing() {
        is_following_ = false;
        path_.clear();
        current_waypoint_ = 0;
    }
    
    // [SEQUENCE: 2253] Check if following path
    bool IsFollowingPath() const {
        return is_following_;
    }
    
    // [SEQUENCE: 2254] Get remaining path distance
    float GetRemainingDistance(float current_x, float current_y) const {
        if (!is_following_ || current_waypoint_ >= path_.size()) {
            return 0.0f;
        }
        
        float total_distance = 0.0f;
        
        // Distance to current waypoint
        float dx = path_[current_waypoint_].first - current_x;
        float dy = path_[current_waypoint_].second - current_y;
        total_distance += std::sqrt(dx * dx + dy * dy);
        
        // Distance between remaining waypoints
        for (size_t i = current_waypoint_; i < path_.size() - 1; ++i) {
            dx = path_[i + 1].first - path_[i].first;
            dy = path_[i + 1].second - path_[i].second;
            total_distance += std::sqrt(dx * dx + dy * dy);
        }
        
        return total_distance;
    }
    
private:
    uint64_t entity_id_;
    std::vector<std::pair<float, float>> path_;
    size_t current_waypoint_ = 0;
    bool is_following_ = false;
    float waypoint_threshold_ = 1.0f;  // Distance to consider waypoint reached
    
    // [SEQUENCE: 2255] Path completion callback
    void OnPathComplete() {
        spdlog::debug("Entity {} completed path", entity_id_);
        // Could trigger events or callbacks here
    }
};

} // namespace mmorpg::game::ai