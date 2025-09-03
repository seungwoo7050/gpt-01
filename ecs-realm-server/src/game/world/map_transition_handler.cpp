#include "map_transition_handler.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/components/network_component.h"
#include "game/components/player_component.h"
#include "proto/game_messages.pb.h"
#include <spdlog/spdlog.h>

namespace mmorpg::game::world {

// [SEQUENCE: 483] Initiate map transition process
void MapTransitionHandler::InitiateTransition(core::ecs::EntityId entity_id,
                                             uint32_t target_map_id,
                                             const TransitionCallback& callback) {
    std::lock_guard<std::mutex> lock(transition_mutex_);
    
    // [SEQUENCE: 484] Check if already in transition
    if (IsInTransition(entity_id)) {
        callback({false, "Already in transition", 0, 0, 0, 0, 0});
        return;
    }
    
    // [SEQUENCE: 485] Validate transition
    std::string error_message;
    if (!ValidateTransition(entity_id, target_map_id, error_message)) {
        callback({false, error_message, 0, 0, 0, 0, 0});
        return;
    }
    
    // [SEQUENCE: 486] Create transition info
    TransitionInfo info;
    info.state = TransitionState::PREPARING;
    info.target_map_id = target_map_id;
    info.start_time = std::chrono::steady_clock::now();
    info.callback = callback;
    
    // Get current map
    auto* transform = ecs_world_.GetComponent<core::ecs::TransformComponent>(entity_id);
    if (transform) {
        info.source_map_id = transform->map_id;
    }
    
    transition_states_[entity_id] = std::move(info);
    
    // [SEQUENCE: 487] Start transition process
    spdlog::info("Starting map transition for entity {} from map {} to map {}", 
                 entity_id, info.source_map_id, target_map_id);
    
    // Save entity state
    if (SaveEntityState(entity_id)) {
        transition_states_[entity_id].state = TransitionState::LOADING;
        
        // Find or create target map instance
        auto& map_manager = MapManager::Instance();
        auto target_map = map_manager.FindAvailableInstance(target_map_id);
        
        if (!target_map) {
            callback({false, "Failed to find target map", 0, 0, 0, 0, 0});
            transition_states_.erase(entity_id);
            return;
        }
        
        // Get spawn position
        auto [x, y, z] = GetSpawnPosition(target_map_id);
        
        // Load entity to new map
        if (LoadEntityToMap(entity_id, target_map, x, y, z)) {
            transition_states_[entity_id].state = TransitionState::COMPLETING;
            callback({true, "", target_map_id, target_map->GetInstanceId(), x, y, z});
            transition_states_.erase(entity_id);
        } else {
            callback({false, "Failed to load entity to map", 0, 0, 0, 0, 0});
            transition_states_.erase(entity_id);
        }
    } else {
        callback({false, "Failed to save entity state", 0, 0, 0, 0, 0});
        transition_states_.erase(entity_id);
    }
}

// [SEQUENCE: 488] Handle seamless transition at map boundary
void MapTransitionHandler::HandleSeamlessTransition(core::ecs::EntityId entity_id,
                                                   const MapConfig::Connection& connection) {
    // [SEQUENCE: 489] Seamless transition doesn't show loading screen
    InitiateTransition(entity_id, connection.target_map_id,
        [entity_id, connection](const TransitionResult& result) {
            if (result.success) {
                spdlog::info("Seamless transition completed for entity {} to map {}", 
                            entity_id, result.new_map_id);
            } else {
                spdlog::error("Seamless transition failed for entity {}: {}", 
                             entity_id, result.error_message);
            }
        });
}

// [SEQUENCE: 490] Force teleport to specific location
void MapTransitionHandler::TeleportToMap(core::ecs::EntityId entity_id,
                                        uint32_t map_id,
                                        float x, float y, float z,
                                        const TransitionCallback& callback) {
    std::lock_guard<std::mutex> lock(transition_mutex_);
    
    if (IsInTransition(entity_id)) {
        callback({false, "Already in transition", 0, 0, 0, 0, 0});
        return;
    }
    
    // [SEQUENCE: 491] Create transition with specific coordinates
    TransitionInfo info;
    info.state = TransitionState::PREPARING;
    info.target_map_id = map_id;
    info.target_x = x;
    info.target_y = y;
    info.target_z = z;
    info.start_time = std::chrono::steady_clock::now();
    info.callback = callback;
    
    transition_states_[entity_id] = std::move(info);
    
    // Perform teleport
    auto& map_manager = MapManager::Instance();
    auto target_map = map_manager.GetInstance(map_id);
    
    if (target_map && LoadEntityToMap(entity_id, target_map, x, y, z)) {
        callback({true, "", map_id, target_map->GetInstanceId(), x, y, z});
    } else {
        callback({false, "Teleport failed", 0, 0, 0, 0, 0});
    }
    
    transition_states_.erase(entity_id);
}

// [SEQUENCE: 492] Join or create instance for party/raid
void MapTransitionHandler::JoinOrCreateInstance(core::ecs::EntityId entity_id,
                                               uint32_t map_id,
                                               uint32_t party_id,
                                               const TransitionCallback& callback) {
    auto& map_manager = MapManager::Instance();
    
    // [SEQUENCE: 493] Look for existing instance with party members
    std::shared_ptr<MapInstance> target_instance;
    
    for (auto& instance : map_manager.GetAllInstances()) {
        if (instance->GetMapId() == map_id) {
            // Check if any party member is in this instance
            for (auto& other_entity : instance->GetAllEntities()) {
                auto* player = ecs_world_.GetComponent<PlayerComponent>(other_entity);
                if (player && player->party_id == party_id) {
                    target_instance = instance;
                    break;
                }
            }
            if (target_instance) break;
        }
    }
    
    // [SEQUENCE: 494] Create new instance if none found
    if (!target_instance) {
        target_instance = map_manager.CreateInstance(map_id);
        spdlog::info("Created new instance {} for party {}", 
                    target_instance->GetInstanceId(), party_id);
    }
    
    if (target_instance) {
        auto [x, y, z] = GetSpawnPosition(map_id);
        InitiateTransition(entity_id, map_id, callback);
    } else {
        callback({false, "Failed to create instance", 0, 0, 0, 0, 0});
    }
}

// [SEQUENCE: 495] Cancel ongoing transition
void MapTransitionHandler::CancelTransition(core::ecs::EntityId entity_id) {
    std::lock_guard<std::mutex> lock(transition_mutex_);
    
    auto it = transition_states_.find(entity_id);
    if (it != transition_states_.end()) {
        spdlog::warn("Cancelling transition for entity {}", entity_id);
        if (it->second.callback) {
            it->second.callback({false, "Transition cancelled", 0, 0, 0, 0, 0});
        }
        transition_states_.erase(it);
    }
}

// [SEQUENCE: 496] Save entity state before transition
bool MapTransitionHandler::SaveEntityState(core::ecs::EntityId entity_id) {
    // In a real implementation, this would:
    // 1. Save to database
    // 2. Flush any pending changes
    // 3. Create a checkpoint
    
    auto* transform = ecs_world_.GetComponent<core::ecs::TransformComponent>(entity_id);
    if (!transform) return false;
    
    spdlog::debug("Saving entity {} state at position ({}, {}, {})", 
                  entity_id, transform->position.x, transform->position.y, transform->position.z);
    
    // TODO: Actual database save
    return true;
}

// [SEQUENCE: 497] Load entity into new map
bool MapTransitionHandler::LoadEntityToMap(core::ecs::EntityId entity_id,
                                          std::shared_ptr<MapInstance> target_map,
                                          float x, float y, float z) {
    // [SEQUENCE: 498] Remove from current map
    auto* transform = ecs_world_.GetComponent<core::ecs::TransformComponent>(entity_id);
    if (transform && transform->map_id != target_map->GetMapId()) {
        auto& map_manager = MapManager::Instance();
        auto current_map = map_manager.GetInstance(transform->map_id);
        
        if (current_map) {
            NotifyNearbyPlayers(entity_id, current_map, true);
            current_map->RemoveEntity(entity_id);
        }
    }
    
    // [SEQUENCE: 499] Update transform component
    if (transform) {
        transform->position.x = x;
        transform->position.y = y;
        transform->position.z = z;
        transform->map_id = target_map->GetMapId();
        transform->instance_id = target_map->GetInstanceId();
    }
    
    // [SEQUENCE: 500] Add to new map
    target_map->AddEntity(entity_id, x, y, z);
    NotifyNearbyPlayers(entity_id, target_map, false);
    
    // [SEQUENCE: 501] Send map change packet to client
    auto* network = ecs_world_.GetComponent<core::ecs::NetworkComponent>(entity_id);
    if (network && network->session) {
        proto::MapChangeNotification notification;
        notification.set_map_id(target_map->GetMapId());
        notification.set_instance_id(target_map->GetInstanceId());
        notification.set_x(x);
        notification.set_y(y);
        notification.set_z(z);
        
        network->session->SendPacket(proto::PACKET_MAP_CHANGE, notification);
    }
    
    return true;
}

// [SEQUENCE: 502] Notify nearby players of entity arrival/departure
void MapTransitionHandler::NotifyNearbyPlayers(core::ecs::EntityId entity_id,
                                              std::shared_ptr<MapInstance> map,
                                              bool is_leaving) {
    auto* transform = ecs_world_.GetComponent<core::ecs::TransformComponent>(entity_id);
    if (!transform) return;
    
    // [SEQUENCE: 503] Get nearby entities
    auto nearby = map->GetEntitiesInRadius(
        transform->position.x, 
        transform->position.y, 
        transform->position.z, 
        100.0f  // Notification radius
    );
    
    for (auto& other_id : nearby) {
        if (other_id == entity_id) continue;
        
        auto* other_network = ecs_world_.GetComponent<core::ecs::NetworkComponent>(other_id);
        if (other_network && other_network->session) {
            if (is_leaving) {
                proto::EntityRemoveNotification remove;
                remove.set_entity_id(entity_id);
                other_network->session->SendPacket(proto::PACKET_ENTITY_REMOVE, remove);
            } else {
                proto::EntitySpawnNotification spawn;
                spawn.set_entity_id(entity_id);
                spawn.set_x(transform->position.x);
                spawn.set_y(transform->position.y);
                spawn.set_z(transform->position.z);
                // Add other entity data...
                other_network->session->SendPacket(proto::PACKET_ENTITY_SPAWN, spawn);
            }
        }
    }
}

// [SEQUENCE: 504] Validate if transition is allowed
bool MapTransitionHandler::ValidateTransition(core::ecs::EntityId entity_id,
                                             uint32_t target_map_id,
                                             std::string& error_message) {
    // [SEQUENCE: 505] Check entity exists
    auto* player = ecs_world_.GetComponent<PlayerComponent>(entity_id);
    if (!player) {
        error_message = "Entity is not a player";
        return false;
    }
    
    // [SEQUENCE: 506] Check level requirements
    auto& map_manager = MapManager::Instance();
    auto target_config = map_manager.GetInstance(target_map_id);
    if (target_config) {
        const auto& config = target_config->GetConfig();
        if (player->level < config.min_level) {
            error_message = "Level too low for target map";
            return false;
        }
        if (player->level > config.max_level) {
            error_message = "Level too high for target map";
            return false;
        }
    }
    
    // [SEQUENCE: 507] Check combat state
    if (player->in_combat) {
        error_message = "Cannot change maps while in combat";
        return false;
    }
    
    return true;
}

// [SEQUENCE: 508] Get spawn position for map
std::tuple<float, float, float> MapTransitionHandler::GetSpawnPosition(
    uint32_t map_id, 
    const MapConfig::Connection* connection) {
    
    auto& map_manager = MapManager::Instance();
    auto map_instance = map_manager.GetInstance(map_id);
    
    if (!map_instance) {
        return {0, 0, 0};
    }
    
    const auto& config = map_instance->GetConfig();
    
    // [SEQUENCE: 509] Use connection point if available
    if (connection && !config.spawn_points.empty()) {
        // Find nearest spawn point to connection
        float min_dist = std::numeric_limits<float>::max();
        const MapConfig::SpawnPoint* best_spawn = &config.spawn_points[0];
        
        for (const auto& spawn : config.spawn_points) {
            float dx = spawn.x - connection->x;
            float dy = spawn.y - connection->y;
            float dz = spawn.z - connection->z;
            float dist = dx*dx + dy*dy + dz*dz;
            
            if (dist < min_dist) {
                min_dist = dist;
                best_spawn = &spawn;
            }
        }
        
        return {best_spawn->x, best_spawn->y, best_spawn->z};
    }
    
    // [SEQUENCE: 510] Use random spawn point
    if (!config.spawn_points.empty()) {
        size_t index = rand() % config.spawn_points.size();
        const auto& spawn = config.spawn_points[index];
        
        // Add some randomness within spawn radius
        float angle = (rand() / (float)RAND_MAX) * 2 * M_PI;
        float radius = (rand() / (float)RAND_MAX) * spawn.radius;
        
        return {
            spawn.x + radius * cos(angle),
            spawn.y + radius * sin(angle),
            spawn.z
        };
    }
    
    // Default spawn at origin
    return {0, 0, 0};
}

// [SEQUENCE: 511] Check for transition timeouts
void MapTransitionHandler::CheckTransitionTimeouts() {
    std::lock_guard<std::mutex> lock(transition_mutex_);
    
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = transition_states_.begin(); it != transition_states_.end();) {
        auto elapsed = now - it->second.start_time;
        
        if (elapsed > TRANSITION_TIMEOUT) {
            spdlog::error("Transition timeout for entity {}", it->first);
            if (it->second.callback) {
                it->second.callback({false, "Transition timeout", 0, 0, 0, 0, 0});
            }
            it = transition_states_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace mmorpg::game::world