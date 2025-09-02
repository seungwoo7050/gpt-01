#pragma once

#include <memory>
#include <functional>
#include "map_manager.h"
#include "core/ecs/world.h"
#include "core/network/session.h"

namespace mmorpg::game::world {

// [SEQUENCE: MVP7-130] Map transition states
enum class TransitionState {
    NONE,
    PREPARING,      // 전환 준비 중
    SAVING,         // 현재 맵 데이터 저장 중
    LOADING,        // 새 맵 로딩 중
    TRANSFERRING,   // 엔티티 전송 중
    COMPLETING      // 전환 완료 중
};

// [SEQUENCE: MVP7-131] Map transition result
struct TransitionResult {
    bool success;
    std::string error_message;
    uint32_t new_map_id;
    uint32_t new_instance_id;
    float spawn_x, spawn_y, spawn_z;
};

// [SEQUENCE: MVP7-132] Map transition handler for seamless world movement
class MapTransitionHandler {
public:
    using TransitionCallback = std::function<void(const TransitionResult&)>;
    
    MapTransitionHandler(core::ecs::World& ecs_world)
        : ecs_world_(ecs_world) {}
    
    // [SEQUENCE: MVP7-133] Initiate map transition for entity
    void InitiateTransition(core::ecs::EntityId entity_id,
                           uint32_t target_map_id,
                           const TransitionCallback& callback);
    
    // [SEQUENCE: MVP7-134] Handle seamless transition at map boundary
    void HandleSeamlessTransition(core::ecs::EntityId entity_id,
                                 const MapConfig::Connection& connection);
    
    // [SEQUENCE: MVP7-135] Force teleport to specific map/location
    void TeleportToMap(core::ecs::EntityId entity_id,
                      uint32_t map_id,
                      float x, float y, float z,
                      const TransitionCallback& callback);
    
    // [SEQUENCE: MVP7-136] Instance creation and joining
    void JoinOrCreateInstance(core::ecs::EntityId entity_id,
                             uint32_t map_id,
                             uint32_t party_id,
                             const TransitionCallback& callback);
    
    // [SEQUENCE: MVP7-137] Check if entity is in transition
    bool IsInTransition(core::ecs::EntityId entity_id) const {
        return transition_states_.find(entity_id) != transition_states_.end();
    }
    
    // [SEQUENCE: MVP7-138] Cancel ongoing transition
    void CancelTransition(core::ecs::EntityId entity_id);
    
private:
    // [SEQUENCE: MVP7-139] Transition state tracking
    struct TransitionInfo {
        TransitionState state = TransitionState::NONE;
        uint32_t source_map_id = 0;
        uint32_t target_map_id = 0;
        uint32_t target_instance_id = 0;
        float target_x = 0, target_y = 0, target_z = 0;
        std::chrono::steady_clock::time_point start_time;
        TransitionCallback callback;
    };
    
    // [SEQUENCE: MVP7-140] Save entity state before transition
    bool SaveEntityState(core::ecs::EntityId entity_id);
    
    // [SEQUENCE: MVP7-141] Load entity into new map
    bool LoadEntityToMap(core::ecs::EntityId entity_id,
                        std::shared_ptr<MapInstance> target_map,
                        float x, float y, float z);
    
    // [SEQUENCE: MVP7-142] Transfer entity components between maps
    void TransferEntityComponents(core::ecs::EntityId entity_id,
                                 std::shared_ptr<MapInstance> source_map,
                                 std::shared_ptr<MapInstance> target_map);
    
    // [SEQUENCE: MVP7-143] Notify nearby players of transition
    void NotifyNearbyPlayers(core::ecs::EntityId entity_id,
                            std::shared_ptr<MapInstance> map,
                            bool is_leaving);
    
    // [SEQUENCE: MVP7-144] Send transition packets to client
    void SendTransitionPackets(core::ecs::EntityId entity_id,
                              const TransitionInfo& info);
    
    // [SEQUENCE: MVP7-145] Validate transition permission
    bool ValidateTransition(core::ecs::EntityId entity_id,
                           uint32_t target_map_id,
                           std::string& error_message);
    
    // [SEQUENCE: MVP7-146] Get spawn position for map
    std::tuple<float, float, float> GetSpawnPosition(uint32_t map_id,
                                                     const MapConfig::Connection* connection = nullptr);
    
    // [SEQUENCE: MVP7-147] Handle transition timeout
    void CheckTransitionTimeouts();
    
    core::ecs::World& ecs_world_;
    std::unordered_map<core::ecs::EntityId, TransitionInfo> transition_states_;
    mutable std::mutex transition_mutex_;
    
    static constexpr auto TRANSITION_TIMEOUT = std::chrono::seconds(10);
};

// [SEQUENCE: MVP7-148] Map boundary detector
class MapBoundaryDetector {
public:
    // [SEQUENCE: MVP7-149] Check if position is near map boundary
    static std::optional<MapConfig::Connection> CheckBoundary(
        std::shared_ptr<MapInstance> current_map,
        float x, float y, float z) {
        
        return current_map->CheckMapTransition(x, y, z);
    }
    
    // [SEQUENCE: MVP7-150] Get distance to nearest boundary
    static float GetDistanceToBoundary(std::shared_ptr<MapInstance> current_map,
                                      float x, float y, float z) {
        const auto& config = current_map->GetConfig();
        float min_distance = std::numeric_limits<float>::max();
        
        for (const auto& conn : config.connections) {
            float dx = x - conn.x;
            float dy = y - conn.y;
            float dz = z - conn.z;
            float distance = std::sqrt(dx*dx + dy*dy + dz*dz) - conn.radius;
            min_distance = std::min(min_distance, distance);
        }
        
        return min_distance;
    }
    
    // [SEQUENCE: MVP7-151] Preload adjacent map data
    static void PreloadAdjacentMaps(uint32_t current_map_id) {
        auto& map_manager = MapManager::Instance();
        auto current_instance = map_manager.GetInstance(current_map_id);
        
        if (!current_instance) return;
        
        const auto& config = current_instance->GetConfig();
        for (const auto& conn : config.connections) {
            // Trigger map data loading if not already loaded
            map_manager.GetInstance(conn.target_map_id);
        }
    }
};

} // namespace mmorpg::game::world