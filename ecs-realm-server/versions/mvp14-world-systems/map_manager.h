#pragma once

#include <memory>
#include <unordered_map>
#include <string>
#include <mutex>
#include "core/ecs/world.h"
#include "game/world/grid/world_grid.h"
#include "game/world/octree/octree_world.h"

namespace mmorpg::game::world {

// [SEQUENCE: 412] Map types enumeration
enum class MapType {
    OVERWORLD,      // 오픈 월드 맵
    DUNGEON,        // 던전 (인스턴스 가능)
    ARENA,          // PvP 아레나
    CITY,           // 도시 (안전 지역)
    RAID            // 레이드 던전
};

// [SEQUENCE: 413] Map configuration
struct MapConfig {
    uint32_t map_id;
    std::string map_name;
    MapType type;
    
    // Spatial configuration
    bool use_octree = false;  // true: 3D octree, false: 2D grid
    float width = 1000.0f;
    float height = 1000.0f;
    float depth = 100.0f;     // For octree only
    
    // Instance settings
    bool is_instanced = false;
    uint32_t max_players = 100;
    uint32_t min_level = 1;
    uint32_t max_level = 60;
    
    // Spawn points
    struct SpawnPoint {
        float x, y, z;
        float radius;
    };
    std::vector<SpawnPoint> spawn_points;
    
    // Adjacent maps for seamless transition
    struct Connection {
        uint32_t target_map_id;
        float x, y, z;  // Transition point
        float radius;   // Trigger radius
    };
    std::vector<Connection> connections;
};

// [SEQUENCE: 414] Map instance representation
class MapInstance {
public:
    MapInstance(const MapConfig& config, uint32_t instance_id = 0)
        : config_(config), instance_id_(instance_id) {
        
        // [SEQUENCE: 415] Create spatial index based on config
        if (config.use_octree) {
            spatial_index_ = std::make_unique<OctreeWorld>(
                config.width, config.height, config.depth
            );
        } else {
            spatial_index_ = std::make_unique<WorldGrid>(
                config.width, config.height
            );
        }
    }
    
    uint32_t GetMapId() const { return config_.map_id; }
    uint32_t GetInstanceId() const { return instance_id_; }
    const MapConfig& GetConfig() const { return config_; }
    
    // [SEQUENCE: 416] Entity management
    void AddEntity(core::ecs::EntityId entity, float x, float y, float z = 0) {
        entities_.insert(entity);
        if (config_.use_octree) {
            static_cast<OctreeWorld*>(spatial_index_.get())->AddEntity(entity, x, y, z);
        } else {
            static_cast<WorldGrid*>(spatial_index_.get())->AddEntity(entity, x, y);
        }
    }
    
    void RemoveEntity(core::ecs::EntityId entity) {
        entities_.erase(entity);
        if (config_.use_octree) {
            static_cast<OctreeWorld*>(spatial_index_.get())->RemoveEntity(entity);
        } else {
            static_cast<WorldGrid*>(spatial_index_.get())->RemoveEntity(entity);
        }
    }
    
    void UpdateEntity(core::ecs::EntityId entity, float x, float y, float z = 0) {
        if (config_.use_octree) {
            static_cast<OctreeWorld*>(spatial_index_.get())->UpdateEntity(entity, x, y, z);
        } else {
            static_cast<WorldGrid*>(spatial_index_.get())->UpdateEntity(entity, x, y);
        }
    }
    
    // [SEQUENCE: 417] Spatial queries
    std::vector<core::ecs::EntityId> GetEntitiesInRadius(float x, float y, float z, float radius) {
        if (config_.use_octree) {
            return static_cast<OctreeWorld*>(spatial_index_.get())->GetEntitiesInRadius(x, y, z, radius);
        } else {
            return static_cast<WorldGrid*>(spatial_index_.get())->GetEntitiesInRadius(x, y, radius);
        }
    }
    
    const std::unordered_set<core::ecs::EntityId>& GetAllEntities() const {
        return entities_;
    }
    
    size_t GetPlayerCount() const { return entities_.size(); }
    bool IsFull() const { return entities_.size() >= config_.max_players; }
    
    // [SEQUENCE: 418] Check if position is near map transition
    std::optional<MapConfig::Connection> CheckMapTransition(float x, float y, float z) {
        for (const auto& conn : config_.connections) {
            float dx = x - conn.x;
            float dy = y - conn.y;
            float dz = z - conn.z;
            float dist_sq = dx*dx + dy*dy + dz*dz;
            
            if (dist_sq <= conn.radius * conn.radius) {
                return conn;
            }
        }
        return std::nullopt;
    }
    
private:
    MapConfig config_;
    uint32_t instance_id_;
    std::unique_ptr<ISpatialIndex> spatial_index_;
    std::unordered_set<core::ecs::EntityId> entities_;
};

// [SEQUENCE: 419] Map manager singleton
class MapManager {
public:
    static MapManager& Instance() {
        static MapManager instance;
        return instance;
    }
    
    // [SEQUENCE: 420] Map configuration management
    void RegisterMap(const MapConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        map_configs_[config.map_id] = config;
        
        // Create default instance for non-instanced maps
        if (!config.is_instanced) {
            CreateInstance(config.map_id, 0);
        }
    }
    
    // [SEQUENCE: 421] Instance management
    std::shared_ptr<MapInstance> CreateInstance(uint32_t map_id, uint32_t instance_id = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = map_configs_.find(map_id);
        if (it == map_configs_.end()) {
            spdlog::error("Map {} not registered", map_id);
            return nullptr;
        }
        
        // Generate unique instance ID for instanced maps
        if (it->second.is_instanced && instance_id == 0) {
            instance_id = next_instance_id_++;
        }
        
        auto instance = std::make_shared<MapInstance>(it->second, instance_id);
        
        uint64_t key = MakeInstanceKey(map_id, instance_id);
        instances_[key] = instance;
        
        spdlog::info("Created map instance: map_id={}, instance_id={}", map_id, instance_id);
        return instance;
    }
    
    // [SEQUENCE: 422] Get map instance
    std::shared_ptr<MapInstance> GetInstance(uint32_t map_id, uint32_t instance_id = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        uint64_t key = MakeInstanceKey(map_id, instance_id);
        auto it = instances_.find(key);
        
        if (it != instances_.end()) {
            return it->second;
        }
        
        return nullptr;
    }
    
    // [SEQUENCE: 423] Find available instance for instanced maps
    std::shared_ptr<MapInstance> FindAvailableInstance(uint32_t map_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto config_it = map_configs_.find(map_id);
        if (config_it == map_configs_.end() || !config_it->second.is_instanced) {
            return GetInstance(map_id, 0);
        }
        
        // Find non-full instance
        for (const auto& [key, instance] : instances_) {
            if (instance->GetMapId() == map_id && !instance->IsFull()) {
                return instance;
            }
        }
        
        // Create new instance if all are full
        return CreateInstance(map_id);
    }
    
    // [SEQUENCE: 424] Remove empty instances
    void CleanupEmptyInstances() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = instances_.begin();
        while (it != instances_.end()) {
            if (it->second->GetConfig().is_instanced && 
                it->second->GetPlayerCount() == 0) {
                spdlog::info("Removing empty instance: map_id={}, instance_id={}", 
                            it->second->GetMapId(), it->second->GetInstanceId());
                it = instances_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    std::vector<std::shared_ptr<MapInstance>> GetAllInstances() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<std::shared_ptr<MapInstance>> result;
        for (const auto& [key, instance] : instances_) {
            result.push_back(instance);
        }
        return result;
    }
    
private:
    MapManager() = default;
    
    uint64_t MakeInstanceKey(uint32_t map_id, uint32_t instance_id) {
        return (static_cast<uint64_t>(map_id) << 32) | instance_id;
    }
    
    std::mutex mutex_;
    std::unordered_map<uint32_t, MapConfig> map_configs_;
    std::unordered_map<uint64_t, std::shared_ptr<MapInstance>> instances_;
    std::atomic<uint32_t> next_instance_id_{1};
};

} // namespace mmorpg::game::world