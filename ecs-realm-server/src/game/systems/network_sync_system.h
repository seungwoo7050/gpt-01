#pragma once

#include "core/ecs/system.h"
#include "proto/game.pb.h"
#include <unordered_map>
#include <vector>

namespace mmorpg::game::systems {

// [SEQUENCE: MVP2-24] Responsible for synchronizing entity state changes to clients.
class NetworkSyncSystem : public core::ecs::System {
public:
    NetworkSyncSystem() = default;
    
    void OnSystemInit() override;
    void OnSystemShutdown() override;
    void Update(float delta_time) override;
    
private:
    void UpdateEntityVisibility(core::ecs::EntityId observer, core::ecs::EntityId target);
    std::vector<core::ecs::EntityId> GetVisibleEntities(core::ecs::EntityId observer);
    
    mmorpg::proto::EntityUpdate CreateEntityUpdate(core::ecs::EntityId entity);
    mmorpg::proto::MovementUpdate CreateMovementUpdate(core::ecs::EntityId entity);
    mmorpg::proto::HealthUpdate CreateHealthUpdate(core::ecs::EntityId entity);
    void SendUpdatesToClient(uint64_t session_id, const std::vector<mmorpg::proto::EntityUpdate>& updates);
    
    std::unordered_map<core::ecs::EntityId, std::vector<core::ecs::EntityId>> visible_entities_;
    float sync_rate_ = 30.0f; // Updates per second
    float visibility_range_ = 100.0f; // Units
};

} // namespace mmorpg::game::systems