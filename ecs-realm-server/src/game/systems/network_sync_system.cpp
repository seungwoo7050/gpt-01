#include "game/systems/network_sync_system.h"
#include "core/ecs/world.h"
#include "core/network/session_manager.h"
#include "game/components/transform_component.h"
#include "game/components/health_component.h"
#include "game/components/network_component.h"
#include "game/components/tag_component.h"
#include <spdlog/spdlog.h>

namespace mmorpg::game::systems {

// [SEQUENCE: MVP2-24] Responsible for synchronizing entity state changes to clients.
void NetworkSyncSystem::OnSystemInit() {
    spdlog::info("NetworkSyncSystem initialized");
}

void NetworkSyncSystem::OnSystemShutdown() {
    spdlog::info("NetworkSyncSystem shutdown");
}

void NetworkSyncSystem::Update(float delta_time) {
    std::unordered_map<uint64_t, std::vector<mmorpg::proto::EntityUpdate>> updates_by_session;

    for (const auto& entity : entities_) {
        auto& network = world_->GetComponent<components::NetworkComponent>(entity);
        if (!network.NeedsUpdate()) continue;

        auto update = CreateEntityUpdate(entity);
        
        // This is a simplification. A real visibility system would be more complex.
        // For now, we just send the update to the owner of the entity.
        if (network.owner_session_id > 0) {
             updates_by_session[network.owner_session_id].push_back(update);
        }

        network.needs_full_update = false;
        network.needs_position_update = false;
        network.needs_health_update = false;
    }

    for (const auto& [session_id, updates] : updates_by_session) {
        SendUpdatesToClient(session_id, updates);
    }
}

mmorpg::proto::EntityUpdate NetworkSyncSystem::CreateEntityUpdate(core::ecs::EntityId entity) {
    mmorpg::proto::EntityUpdate update;
    update.set_entity_id(entity);

    auto& network = world_->GetComponent<components::NetworkComponent>(entity);

    if (network.needs_full_update || network.needs_position_update) {
        auto movement = CreateMovementUpdate(entity);
        *update.mutable_movement() = movement;
    }
    
    if (network.needs_full_update || network.needs_health_update) {
        auto health = CreateHealthUpdate(entity);
        *update.mutable_health() = health;
    }
    
    return update;
}

mmorpg::proto::MovementUpdate NetworkSyncSystem::CreateMovementUpdate(core::ecs::EntityId entity) {
    mmorpg::proto::MovementUpdate update;
    update.set_entity_id(entity);
    update.set_timestamp(std::chrono::system_clock::now().time_since_epoch().count());

    auto& transform = world_->GetComponent<components::TransformComponent>(entity);
    auto* pos = update.mutable_position();
    pos->set_x(transform.position.x);
    pos->set_y(transform.position.y);
    pos->set_z(transform.position.z);

    auto* rot = update.mutable_rotation();
    rot->set_x(transform.rotation.x);
    rot->set_y(transform.rotation.y);
    rot->set_z(transform.rotation.z);

    if (world_->HasComponent<components::VelocityComponent>(entity)) {
        auto& velocity = world_->GetComponent<components::VelocityComponent>(entity);
        auto* vel = update.mutable_velocity();
        vel->set_x(velocity.linear.x);
        vel->set_y(velocity.linear.y);
        vel->set_z(velocity.linear.z);
    }
    
    return update;
}

mmorpg::proto::HealthUpdate NetworkSyncSystem::CreateHealthUpdate(core::ecs::EntityId entity) {
    mmorpg::proto::HealthUpdate update;
    
    auto& health = world_->GetComponent<components::HealthComponent>(entity);
    update.set_current_hp(health.current_hp);
    update.set_max_hp(health.max_hp);
    update.set_shield(health.shield);
    
    return update;
}

void NetworkSyncSystem::SendUpdatesToClient(uint64_t session_id, const std::vector<mmorpg::proto::EntityUpdate>& updates) {
    // This part requires the SessionManager from MVP1.
    // For now, this is just a placeholder.
    spdlog::debug("Sending {} updates to session {}", updates.size(), session_id);
}

// The visibility methods are complex and depend on spatial partitioning (MVP3).
// I will leave them out for now to keep MVP2 focused on the ECS core.
void NetworkSyncSystem::UpdateEntityVisibility(core::ecs::EntityId observer, core::ecs::EntityId target) {}
std::vector<core::ecs::EntityId> NetworkSyncSystem::GetVisibleEntities(core::ecs::EntityId observer) { return {}; }

} // namespace mmorpg::game::systems
