#include "game/systems/network_sync_system.h"
#include "core/ecs/world.h"
#include "core/network/session_manager.h"
#include "game/components/transform_component.h"
#include "game/components/health_component.h"
#include "game/components/network_component.h"
#include "game/components/tag_component.h"
#include <spdlog/spdlog.h>

namespace mmorpg::game::systems {

// [SEQUENCE: 1] System initialization
void NetworkSyncSystem::OnSystemInit() {
    spdlog::info("NetworkSyncSystem initialized");
}

// [SEQUENCE: 2] System shutdown
void NetworkSyncSystem::OnSystemShutdown() {
    spdlog::info("NetworkSyncSystem shutdown");
}

// [SEQUENCE: 3] Main network sync update
void NetworkSyncSystem::NetworkSync(float delta_time) {
    auto* storage = GetComponentStorage();
    if (!storage) return;
    
    auto* network_storage = storage->GetStorage<components::NetworkComponent>();
    if (!network_storage) return;
    
    // Group updates by session
    std::unordered_map<uint64_t, std::vector<mmorpg::proto::EntityUpdate>> updates_by_session;
    
    // Process all networked entities
    for (auto& [entity, network] : network_storage->GetAllComponents()) {
        if (!network.NeedsUpdate()) continue;
        
        // Create update packet
        auto update = CreateEntityUpdate(entity);
        
        // Send to all sessions that can see this entity
        auto visible_to = GetVisibleEntities(entity);
        for (auto observer : visible_to) {
            auto* observer_network = world_->GetComponent<components::NetworkComponent>(observer);
            if (observer_network && observer_network->owner_session_id > 0) {
                updates_by_session[observer_network->owner_session_id].push_back(update);
            }
        }
        
        // Clear update flags
        network.needs_full_update = false;
        network.needs_position_update = false;
        network.needs_health_update = false;
    }
    
    // Send batched updates to each session
    for (const auto& [session_id, updates] : updates_by_session) {
        SendUpdatesToClient(session_id, updates);
    }
}

// [SEQUENCE: 4] Update entity visibility
void NetworkSyncSystem::UpdateEntityVisibility(core::ecs::EntityId observer, core::ecs::EntityId target) {
    auto* observer_transform = world_->GetComponent<components::TransformComponent>(observer);
    auto* target_transform = world_->GetComponent<components::TransformComponent>(target);
    
    if (!observer_transform || !target_transform) return;
    
    float distance = core::utils::Vector3::Distance(
        observer_transform->position,
        target_transform->position
    );
    
    auto& visible_list = visible_entities_[observer];
    auto it = std::find(visible_list.begin(), visible_list.end(), target);
    
    if (distance <= visibility_range_) {
        // Add to visible list if not already there
        if (it == visible_list.end()) {
            visible_list.push_back(target);
            
            // Mark target for full update to observer
            auto* network = world_->GetComponent<components::NetworkComponent>(target);
            if (network) {
                network->needs_full_update = true;
            }
        }
    } else {
        // Remove from visible list
        if (it != visible_list.end()) {
            visible_list.erase(it);
            
            // Could send entity removal packet
        }
    }
}

// [SEQUENCE: 5] Get visible entities
std::vector<core::ecs::EntityId> NetworkSyncSystem::GetVisibleEntities(core::ecs::EntityId observer) {
    auto it = visible_entities_.find(observer);
    if (it != visible_entities_.end()) {
        return it->second;
    }
    return {};
}

// [SEQUENCE: 6] Create entity update packet
mmorpg::proto::EntityUpdate NetworkSyncSystem::CreateEntityUpdate(core::ecs::EntityId entity) {
    mmorpg::proto::EntityUpdate update;
    update.set_entity_id(entity);
    
    auto* network = world_->GetComponent<components::NetworkComponent>(entity);
    if (!network) return update;
    
    // Add appropriate updates based on flags
    if (network->needs_full_update || network->needs_position_update) {
        auto movement = CreateMovementUpdate(entity);
        *update.mutable_movement() = movement;
    }
    
    if (network->needs_full_update || network->needs_health_update) {
        auto health = CreateHealthUpdate(entity);
        *update.mutable_health() = health;
    }
    
    return update;
}

// [SEQUENCE: 7] Create movement update
mmorpg::proto::MovementUpdate NetworkSyncSystem::CreateMovementUpdate(core::ecs::EntityId entity) {
    mmorpg::proto::MovementUpdate update;
    update.set_entity_id(entity);
    update.set_timestamp(std::chrono::system_clock::now().time_since_epoch().count());
    
    auto* transform = world_->GetComponent<components::TransformComponent>(entity);
    if (transform) {
        auto* pos = update.mutable_position();
        pos->set_x(transform->position.x);
        pos->set_y(transform->position.y);
        pos->set_z(transform->position.z);
        
        auto* rot = update.mutable_rotation();
        rot->set_x(transform->rotation.x);
        rot->set_y(transform->rotation.y);
        rot->set_z(transform->rotation.z);
    }
    
    auto* velocity = world_->GetComponent<components::VelocityComponent>(entity);
    if (velocity) {
        auto* vel = update.mutable_velocity();
        vel->set_x(velocity->linear.x);
        vel->set_y(velocity->linear.y);
        vel->set_z(velocity->linear.z);
    }
    
    return update;
}

// [SEQUENCE: 8] Create health update
mmorpg::proto::HealthUpdate NetworkSyncSystem::CreateHealthUpdate(core::ecs::EntityId entity) {
    mmorpg::proto::HealthUpdate update;
    
    auto* health = world_->GetComponent<components::HealthComponent>(entity);
    if (health) {
        update.set_current_hp(health->current_hp);
        update.set_max_hp(health->max_hp);
        update.set_shield(health->shield);
    }
    
    return update;
}

// [SEQUENCE: 9] Send updates to client session
void NetworkSyncSystem::SendUpdatesToClient(uint64_t session_id, const std::vector<mmorpg::proto::EntityUpdate>& updates) {
    // TODO: Get session manager and send packet
    // This would integrate with the networking layer from MVP1
    spdlog::debug("Sending {} updates to session {}", updates.size(), session_id);
}

} // namespace mmorpg::game::systems