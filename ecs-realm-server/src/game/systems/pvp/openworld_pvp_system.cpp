#include "game/systems/pvp/openworld_pvp_system.h"
#include "game/systems/grid_spatial_system.h"
#include "game/components/transform_component.h"
#include "game/components/health_component.h"
#include "game/components/combat_stats_component.h"
#include "core/ecs/world.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace mmorpg::game::systems::pvp {

// [SEQUENCE: MVP5-141] Initialize open world PvP system
void OpenWorldPvPSystem::OnSystemInit() {
    // Find spatial system
    if (auto* world = world_) {
        spatial_system_ = dynamic_cast<SpatialIndexingSystem*>(
            world->GetSystem("SpatialIndexingSystem")
        );
    }
    
    // Setup faction hostilities
    hostile_factions_[1] = {2};  // Alliance vs Horde
    hostile_factions_[2] = {1};
    hostile_factions_[3] = {1, 2};  // Pirates hostile to all
    
    spdlog::info("OpenWorldPvPSystem initialized");
}

// [SEQUENCE: MVP5-142] Cleanup
void OpenWorldPvPSystem::OnSystemShutdown() {
    player_states_.clear();
    pvp_zones_.clear();
    kill_history_.clear();
    spdlog::info("OpenWorldPvPSystem shut down");
}

// [SEQUENCE: MVP5-143] Main update loop
void OpenWorldPvPSystem::Update(float delta_time) {
    // Update player zones
    UpdatePlayerZones(delta_time);
    
    // Update zone captures
    UpdateZoneCaptures(delta_time);
    
    // Update PvP flags
    UpdatePvPFlags(delta_time);
}

// [SEQUENCE: MVP5-144] Create PvP zone
core::ecs::EntityId OpenWorldPvPSystem::CreatePvPZone(const std::string& name,
                                                      const core::utils::Vector3& min,
                                                      const core::utils::Vector3& max) {
    auto* world = world_;
    if (!world) return 0;
    
    // Create zone entity
    auto zone_entity = world->CreateEntity();
    
    // Add zone component
    PvPZoneComponent zone;
    zone.zone_id = static_cast<uint32_t>(zone_entity);
    zone.zone_name = name;
    zone.pvp_enabled = true;
    zone.faction_based = true;
    
    world->AddComponent(zone_entity, zone);
    
    // Store bounds
    zone_bounds_[zone_entity] = {min, max};
    pvp_zones_.push_back(zone_entity);
    
    spdlog::info("Created PvP zone '{}' ({})", name, zone_entity);
    return zone_entity;
}

// [SEQUENCE: MVP5-145] Check if player is PvP flagged
bool OpenWorldPvPSystem::IsPlayerPvPFlagged(core::ecs::EntityId player) const {
    auto it = player_states_.find(player);
    if (it != player_states_.end()) {
        return it->second.pvp_flagged;
    }
    return false;
}

// [SEQUENCE: MVP5-146] Check if can attack
bool OpenWorldPvPSystem::CanAttack(core::ecs::EntityId attacker, core::ecs::EntityId target) const {
    // Can't attack self
    if (attacker == target) return false;
    
    // Both must be PvP flagged
    if (!IsPlayerPvPFlagged(attacker) || !IsPlayerPvPFlagged(target)) {
        return false;
    }
    
    // Check factions
    uint32_t attacker_faction = GetPlayerFaction(attacker);
    uint32_t target_faction = GetPlayerFaction(target);
    
    // Same faction can't attack (unless FFA zone)
    if (attacker_faction == target_faction && attacker_faction != 0) {
        auto attacker_zone = GetPlayerZone(attacker);
        if (attacker_zone != 0) {
            auto* world = world_;
            if (world) {
                auto* zone = world->GetComponent<PvPZoneComponent>(attacker_zone);
                if (zone && !zone->free_for_all) {
                    return false;
                }
            }
        }
    }
    
    // Check if factions are hostile
    return AreFactionsHostile(attacker_faction, target_faction);
}

// [SEQUENCE: MVP5-147] Set player faction
void OpenWorldPvPSystem::SetPlayerFaction(core::ecs::EntityId player, uint32_t faction_id) {
    player_states_[player].faction_id = faction_id;
    spdlog::debug("Player {} joined faction {}", player, faction_id);
}

// [SEQUENCE: MVP5-148] Get player faction
uint32_t OpenWorldPvPSystem::GetPlayerFaction(core::ecs::EntityId player) const {
    auto it = player_states_.find(player);
    if (it != player_states_.end()) {
        return it->second.faction_id;
    }
    return 0;  // Neutral
}

// [SEQUENCE: MVP5-149] Check faction hostility
bool OpenWorldPvPSystem::AreFactionsHostile(uint32_t faction1, uint32_t faction2) const {
    if (faction1 == faction2) return false;
    if (faction1 == 0 || faction2 == 0) return false;  // Neutral
    
    auto it = hostile_factions_.find(faction1);
    if (it != hostile_factions_.end()) {
        return it->second.find(faction2) != it->second.end();
    }
    return false;
}

// [SEQUENCE: MVP5-150] Start capturing zone
bool OpenWorldPvPSystem::StartCapture(core::ecs::EntityId player, core::ecs::EntityId zone) {
    auto* world = world_;
    if (!world) return false;
    
    auto* zone_comp = world->GetComponent<PvPZoneComponent>(zone);
    if (!zone_comp) return false;
    
    // Check if player in capture range
    if (!IsPlayerInZone(player, zone)) {
        return false;
    }
    
    // Add to capturing players
    zone_comp->capturing_players.push_back(player);
    
    spdlog::debug("Player {} started capturing zone {}", player, zone);
    return true;
}

// [SEQUENCE: MVP5-151] Update player zones
void OpenWorldPvPSystem::UpdatePlayerZones(float delta_time) {
    static float update_timer = 0.0f;
    update_timer += delta_time;
    
    if (update_timer < config_.zone_update_interval) {
        return;
    }
    update_timer = 0.0f;
    
    auto* world = world_;
    if (!world) return;
    
    for (const auto& player : entities_) {
        auto* transform = world->GetComponent<components::TransformComponent>(player);
        if (!transform) continue;
        
        // Find which zone player is in
        core::ecs::EntityId current_zone = 0;
        
        for (auto zone_entity : pvp_zones_) {
            auto bounds_it = zone_bounds_.find(zone_entity);
            if (bounds_it != zone_bounds_.end()) {
                const ZoneBounds& bounds = bounds_it->second;
                
                if (transform->position.x >= bounds.min.x &&
                    transform->position.x <= bounds.max.x &&
                    transform->position.y >= bounds.min.y &&
                    transform->position.y <= bounds.max.y &&
                    transform->position.z >= bounds.min.z &&
                    transform->position.z <= bounds.max.z) {
                    
                    current_zone = zone_entity;
                    break;
                }
            }
        }
        
        // Update player zone
        auto* player_state = world_->GetComponent<components::PvPStateComponent>(player);
        if (player_state.current_zone != current_zone) {
            if (player_state.current_zone != 0) {
                OnPlayerLeaveZone(player, player_state.current_zone);
            }
            if (current_zone != 0) {
                OnPlayerEnterZone(player, current_zone);
            }
            player_state.current_zone = current_zone;
        }
    }
}

// [SEQUENCE: MVP5-152] Player enters PvP zone
void OpenWorldPvPSystem::OnPlayerEnterZone(core::ecs::EntityId player, core::ecs::EntityId zone) {
    auto* world = world_;
    if (!world) return;
    
    auto* zone_comp = world->GetComponent<PvPZoneComponent>(zone);
    if (!zone_comp || !zone_comp->pvp_enabled) return;
    
    // Flag player for PvP
    auto* player_state = world_->GetComponent<components::PvPStateComponent>(player);
    player_state.pvp_flagged = true;
    player_state.flag_time = std::chrono::steady_clock::now();
    
    // Apply zone buffs if controlled by player's faction
    if (zone_comp->controlling_faction == player_state.faction_id && 
        player_state.faction_id != 0) {
        
        auto* stats = world->GetComponent<CombatStatsComponent>(player);
        if (stats) {
            // Apply territory buff
            stats->damage_increase += config_.territory_buff_bonus;
            stats->damage_reduction += config_.territory_buff_bonus;
        }
    }
    
    spdlog::info("Player {} entered PvP zone '{}'", player, zone_comp->zone_name);
}

// [SEQUENCE: MVP5-153] Player leaves PvP zone
void OpenWorldPvPSystem::OnPlayerLeaveZone(core::ecs::EntityId player, core::ecs::EntityId zone) {
    auto* world = world_;
    if (!world) return;
    
    auto* zone_comp = world->GetComponent<PvPZoneComponent>(zone);
    if (!zone_comp) return;
    
    // Remove from capturing players
    zone_comp->capturing_players.erase(
        std::remove(zone_comp->capturing_players.begin(),
                   zone_comp->capturing_players.end(), player),
        zone_comp->capturing_players.end()
    );
    
    // Remove zone buffs
    auto* player_state = world_->GetComponent<components::PvPStateComponent>(player);
    if (zone_comp->controlling_faction == player_state.faction_id) {
        auto* stats = world->GetComponent<CombatStatsComponent>(player);
        if (stats) {
            stats->damage_increase -= config_.territory_buff_bonus;
            stats->damage_reduction -= config_.territory_buff_bonus;
        }
    }
    
    spdlog::info("Player {} left PvP zone '{}'", player, zone_comp->zone_name);
}

// [SEQUENCE: MVP5-154] Update zone captures
void OpenWorldPvPSystem::UpdateZoneCaptures(float delta_time) {
    auto* world = world_;
    if (!world) return;
    
    for (auto zone_entity : pvp_zones_) {
        auto* zone = world->GetComponent<PvPZoneComponent>(zone_entity);
        if (!zone || zone->capturing_players.empty()) continue;
        
        // Count capturing players by faction
        std::unordered_map<uint32_t, uint32_t> faction_counts;
        
        for (auto player : zone->capturing_players) {
            uint32_t faction = GetPlayerFaction(player);
            if (faction != 0) {
                faction_counts[faction]++;
            }
        }
        
        // Find dominant faction
        uint32_t dominant_faction = 0;
        uint32_t max_count = 0;
        
        for (const auto& [faction, count] : faction_counts) {
            if (count > max_count) {
                max_count = count;
                dominant_faction = faction;
            }
        }
        
        // Update capture progress
        if (dominant_faction != 0 && dominant_faction != zone->controlling_faction) {
            zone->capture_progress += config_.capture_tick_rate * delta_time * max_count;
            
            if (zone->capture_progress >= 100.0f) {
                OnZoneCaptured(zone_entity, dominant_faction);
            }
        } else if (dominant_faction == zone->controlling_faction) {
            // Defend - reduce enemy progress
            zone->capture_progress = std::max(0.0f, zone->capture_progress - 
                                            config_.capture_tick_rate * delta_time);
        }
    }
}

// [SEQUENCE: MVP5-155] Zone captured
void OpenWorldPvPSystem::OnZoneCaptured(core::ecs::EntityId zone_entity, uint32_t faction_id) {
    auto* world = world_;
    if (!world) return;
    
    auto* zone = world->GetComponent<PvPZoneComponent>(zone_entity);
    if (!zone) return;
    
    uint32_t old_faction = zone->controlling_faction;
    zone->controlling_faction = faction_id;
    zone->capture_progress = 0.0f;
    
    // Clear capturing players
    zone->capturing_players.clear();
    
    // Update stats
    stats_.zones_flipped++;
    stats_.faction_territories[faction_id]++;
    if (old_faction != 0) {
        stats_.faction_territories[old_faction]--;
    }
    
    // Grant rewards to capturing faction players in zone
    for (const auto& entity : entities_) {
        if (!world_->HasComponent<components::PvPStateComponent>(entity)) continue;
        auto* state = world_->GetComponent<components::PvPStateComponent>(entity);
        if (state.faction_id == faction_id && state.current_zone == zone_entity) {
            GrantObjectiveReward(player, 1);  // Zone capture
        }
    }
    
    spdlog::info("Zone '{}' captured by faction {} (was {})",
                zone->zone_name, faction_id, old_faction);
}

// [SEQUENCE: MVP5-156] Player killed player
void OpenWorldPvPSystem::OnPlayerKilledPlayer(core::ecs::EntityId killer, core::ecs::EntityId victim) {
    // Check if valid PvP kill
    if (!CanAttack(killer, victim)) {
        return;
    }
    
    // Update stats
    auto* world = world_;
    if (world) {
        auto* killer_stats = world->GetComponent<PvPStatsComponent>(killer);
        auto* victim_stats = world->GetComponent<PvPStatsComponent>(victim);
        
        if (killer_stats && victim_stats) {
            killer_stats->world_pvp_kills++;
            killer_stats->kills++;
            victim_stats->deaths++;
            
            // Update kill streak
            UpdateKillStreak(killer);
            victim_stats->current_streak = 0;
            
            // Grant honor
            GrantHonorKill(killer, victim);
        }
    }
    
    // Update faction kills
    uint32_t killer_faction = GetPlayerFaction(killer);
    if (killer_faction != 0) {
        stats_.faction_kills[killer_faction]++;
    }
    
    stats_.total_kills++;
}

// [SEQUENCE: MVP5-157] Grant honor for kill
void OpenWorldPvPSystem::GrantHonorKill(core::ecs::EntityId killer, core::ecs::EntityId victim) {
    auto* world = world_;
    if (!world) return;
    
    // Check diminishing returns
    auto kill_key = std::make_pair(killer, victim);
    auto& kill_record = kill_history_[kill_key];
    
    auto now = std::chrono::steady_clock::now();
    auto time_since_last = std::chrono::duration_cast<std::chrono::minutes>(
        now - kill_record.last_kill_time
    ).count();
    
    // Reset count if been long enough
    if (time_since_last > 60) {  // 1 hour
        kill_record.kill_count = 0;
    }
    
    kill_record.kill_count++;
    kill_record.last_kill_time = now;
    
    // Calculate honor with diminishing returns
    uint32_t honor = config_.honor_per_kill;
    if (kill_record.kill_count > config_.honor_diminishing_returns) {
        honor = honor / kill_record.kill_count;  // Less honor per repeated kill
    }
    
    // Apply faction bonus if in hostile territory
    auto killer_zone = GetPlayerZone(killer);
    if (killer_zone != 0) {
        auto* zone = world->GetComponent<PvPZoneComponent>(killer_zone);
        if (zone && zone->controlling_faction != GetPlayerFaction(killer)) {
            honor = static_cast<uint32_t>(honor * 1.5f);  // 50% bonus
        }
    }
    
    // Grant honor
    auto* pvp_stats = world->GetComponent<PvPStatsComponent>(killer);
    if (pvp_stats) {
        pvp_stats->honor_points += honor;
        spdlog::debug("Player {} gained {} honor for killing {}", killer, honor, victim);
    }
}

// [SEQUENCE: MVP5-158] Update PvP flags
void OpenWorldPvPSystem::UpdatePvPFlags(float delta_time) {
    auto now = std::chrono::steady_clock::now();
    
    for (const auto& entity : entities_) {
        if (!world_->HasComponent<components::PvPStateComponent>(entity)) continue;
        auto* state = world_->GetComponent<components::PvPStateComponent>(entity);
        if (!state.pvp_flagged) continue;
        
        // Check if should unflag
        if (state.current_zone == 0) {  // Not in PvP zone
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - state.last_pvp_action
            ).count();
            
            if (elapsed > config_.pvp_flag_duration) {
                state.pvp_flagged = false;
                state.recent_attackers.clear();
                spdlog::debug("Player {} PvP flag expired", player);
            }
        }
    }
}

// [SEQUENCE: MVP5-159] Capture objective
bool OpenWorldPvPSystem::CaptureObjective(core::ecs::EntityId player, 
                                         core::ecs::EntityId zone_entity,
                                         uint32_t objective_id) {
    auto* world = world_;
    if (!world) return false;
    
    auto* zone = world->GetComponent<PvPZoneComponent>(zone_entity);
    if (!zone) return false;
    
    // Find objective
    for (auto& obj : zone->objectives) {
        if (obj.objective_id == objective_id) {
            uint32_t player_faction = GetPlayerFaction(player);
            
            if (obj.controlling_team != player_faction) {
                obj.controlling_team = player_faction;
                OnObjectiveCaptured(zone_entity, objective_id, player_faction);
                return true;
            }
        }
    }
    
    return false;
}


void OpenWorldPvPSystem::OnObjectiveCaptured(core::ecs::EntityId zone_entity,
                                            uint32_t objective_id,
                                            uint32_t faction_id) {
    stats_.objectives_captured++;
    
    // Find all faction players in zone for rewards
    for (const auto& entity : entities_) {
        if (!world_->HasComponent<components::PvPStateComponent>(entity)) continue;
        auto* state = world_->GetComponent<components::PvPStateComponent>(entity);
        if (state.faction_id == faction_id && state.current_zone == zone_entity) {
            GrantObjectiveReward(player, 2);  // Objective capture
        }
    }
    
    spdlog::info("Objective {} in zone {} captured by faction {}",
                objective_id, zone_entity, faction_id);
}

void OpenWorldPvPSystem::GrantObjectiveReward(core::ecs::EntityId player, uint32_t objective_type) {
    auto* world = world_;
    if (!world) return;
    
    auto* pvp_stats = world->GetComponent<PvPStatsComponent>(player);
    if (pvp_stats) {
        pvp_stats->honor_points += config_.honor_per_objective;
        pvp_stats->objectives_completed++;
    }
}

void OpenWorldPvPSystem::UpdateKillStreak(core::ecs::EntityId player) {
    auto* world = world_;
    if (!world) return;
    
    auto* pvp_stats = world->GetComponent<PvPStatsComponent>(player);
    if (pvp_stats) {
        pvp_stats->current_streak++;
        pvp_stats->max_killing_spree = std::max(pvp_stats->max_killing_spree,
                                               pvp_stats->current_streak);
        
        // Announce sprees
        if (pvp_stats->current_streak == 3) {
            spdlog::info("Player {} is on a killing spree!", player);
        } else if (pvp_stats->current_streak == 5) {
            spdlog::info("Player {} is dominating!", player);
        } else if (pvp_stats->current_streak == 10) {
            spdlog::info("Player {} is UNSTOPPABLE!", player);
        }
    }
}

bool OpenWorldPvPSystem::IsPlayerInZone(core::ecs::EntityId player, core::ecs::EntityId zone) const {
    auto it = player_states_.find(player);
    if (it != player_states_.end()) {
        return it->second.current_zone == zone;
    }
    return false;
}

core::ecs::EntityId OpenWorldPvPSystem::GetPlayerZone(core::ecs::EntityId player) const {
    auto* player_state = world_->GetComponent<components::PvPStateComponent>(player);
    if (player_state) {
        return player_state->current_zone;
    }
    return 0;
}

void OpenWorldPvPSystem::AddObjective(core::ecs::EntityId zone, uint32_t objective_id,
                                     const core::utils::Vector3& position) {
    auto* world = world_;
    if (!world) return;
    
    auto* zone_comp = world->GetComponent<PvPZoneComponent>(zone);
    if (zone_comp) {
        PvPZoneComponent::Objective obj;
        obj.objective_id = objective_id;
        obj.position = position;
        obj.controlling_team = 0;
        obj.capture_radius = 10.0f;
        obj.point_value = 1;
        
        zone_comp->objectives.push_back(obj);
    }
}

bool OpenWorldPvPSystem::SetZonePvPEnabled(core::ecs::EntityId zone, bool enabled) {
    auto* world = world_;
    if (!world) return false;
    
    auto* zone_comp = world->GetComponent<PvPZoneComponent>(zone);
    if (zone_comp) {
        zone_comp->pvp_enabled = enabled;
        return true;
    }
    return false;
}

std::vector<core::ecs::EntityId> OpenWorldPvPSystem::GetPvPEnabledPlayers() const {
    std::vector<core::ecs::EntityId> result;
    for (const auto& [player, state] : player_states_) {
        if (state.pvp_flagged) {
            result.push_back(player);
        }
    }
    return result;
}

bool OpenWorldPvPSystem::StopCapture(core::ecs::EntityId player, core::ecs::EntityId zone) {
    auto* world = world_;
    if (!world) return false;
    
    auto* zone_comp = world->GetComponent<PvPZoneComponent>(zone);
    if (!zone_comp) return false;
    
    zone_comp->capturing_players.erase(
        std::remove(zone_comp->capturing_players.begin(),
                   zone_comp->capturing_players.end(), player),
        zone_comp->capturing_players.end()
    );
    
    return true;
}

float OpenWorldPvPSystem::GetCaptureProgress(core::ecs::EntityId zone) const {
    auto* world = world_;
    if (!world) return 0.0f;
    
    auto* zone_comp = world->GetComponent<PvPZoneComponent>(zone);
    if (zone_comp) {
        return zone_comp->capture_progress;
    }
    return 0.0f;
}

void OpenWorldPvPSystem::OnPlayerAssist(core::ecs::EntityId assister, core::ecs::EntityId victim) {
    auto* world = world_;
    if (!world) return;
    
    auto* pvp_stats = world->GetComponent<PvPStatsComponent>(assister);
    if (pvp_stats) {
        pvp_stats->assists++;
        pvp_stats->honor_points += config_.honor_per_assist;
    }
}

} // namespace mmorpg::game::systems::pvp      pvp_stats->assists++;
        pvp_stats->honor_points += config_.honor_per_assist;
    }
}

} // namespace mmorpg::game::systems::pvppppvp mmorpg::game::systems::pvp