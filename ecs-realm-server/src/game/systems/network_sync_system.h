#pragma once

#include "core/ecs/system.h"
#include "proto/game.pb.h"
#include <unordered_map>
#include <vector>

namespace mmorpg::game::systems {

// [SEQUENCE: 1] Network synchronization system
class NetworkSyncSystem : public core::ecs::System {
public:
    NetworkSyncSystem() : System("NetworkSyncSystem") {}
    
    // [SEQUENCE: 2] System lifecycle
    void OnSystemInit() override;
    void OnSystemShutdown() override;
    
    // [SEQUENCE: 3] Network sync update
    void NetworkSync(float delta_time) override;
    
    // [SEQUENCE: 4] System metadata
    core::ecs::SystemStage GetStage() const override { 
        return core::ecs::SystemStage::NETWORK_SYNC; 
    }
    int GetPriority() const override { return 1000; } // Last system
    
    // [SEQUENCE: 5] Entity visibility
    void UpdateEntityVisibility(core::ecs::EntityId observer, core::ecs::EntityId target);
    std::vector<core::ecs::EntityId> GetVisibleEntities(core::ecs::EntityId observer);
    
private:
    // [SEQUENCE: 6] Create update packets
    mmorpg::proto::EntityUpdate CreateEntityUpdate(core::ecs::EntityId entity);
    mmorpg::proto::MovementUpdate CreateMovementUpdate(core::ecs::EntityId entity);
    mmorpg::proto::HealthUpdate CreateHealthUpdate(core::ecs::EntityId entity);
    
    // [SEQUENCE: 7] Send updates to clients
    void SendUpdatesToClient(uint64_t session_id, const std::vector<mmorpg::proto::EntityUpdate>& updates);
    
    // [SEQUENCE: 8] Visibility tracking
    std::unordered_map<core::ecs::EntityId, std::vector<core::ecs::EntityId>> visible_entities_;
    
    // [SEQUENCE: 9] Configuration
    float sync_rate_ = 30.0f; // Updates per second
    float visibility_range_ = 100.0f; // Units
};

} // namespace mmorpg::game::systems