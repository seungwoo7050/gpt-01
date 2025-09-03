#include "game/systems/pvp/openworld_pvp_system.h"
#include "game/systems/spatial_indexing_system.h"
#include "game/components/transform_component.h"
#include "game/components/health_component.h"
#include "game/components/combat_stats_component.h"
#include "game/components/pvp_state_component.h"
#include "game/components/pvp_zone_component.h"
#include "core/ecs/optimized/optimized_world.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace mmorpg::game::systems::pvp {

using namespace mmorpg::game::components;

// [SEQUENCE: 3] Main update loop
void OpenWorldPvPSystem::Update(float delta_time) {
    // Update player zones
    UpdatePlayerZones(delta_time);
    
    // Update zone captures
    UpdateZoneCaptures(delta_time);
    
    // Update PvP flags
    UpdatePvPFlags(delta_time);
}

// [SEQUENCE: 4] Create PvP zone
core::ecs::EntityId OpenWorldPvPSystem::CreatePvPZone(const std::string& name,
                                                      const core::utils::Vector3& min,
                                                      const core::utils::Vector3& max) {
    if (!world_) return 0;
    
    // Create zone entity
    auto zone_entity = world_->CreateEntity();
    
    // Add zone component
    PvPZoneComponent zone;
    zone.zone_id = static_cast<uint32_t>(zone_entity);
    zone.zone_name = name;
    zone.pvp_enabled = true;
    zone.faction_based = true;
    
    world_->AddComponent(zone_entity, zone);
    
    // Store bounds
    zone_bounds_[zone_entity] = {min, max};
    pvp_zones_.push_back(zone_entity);
    
    spdlog::info("Created PvP zone '{}' ({})", name, zone_entity);
    return zone_entity;
}

// [SEQUENCE: 5] Check if player is PvP flagged
bool OpenWorldPvPSystem::IsPlayerPvPFlagged(core::ecs::EntityId player) const {
    if (!world_->HasComponent<PvPStateComponent>(player)) return false;
    return world_->GetComponent<PvPStateComponent>(player).pvp_flagged;
}

// [SEQUENCE: 6] Check if can attack
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
            if (world_) {
                auto& zone = world_->GetComponent<PvPZoneComponent>(attacker_zone);
                if (!zone.free_for_all) {
                    return false;
                }
            }
        }
    }
    
    // Check if factions are hostile
    return AreFactionsHostile(attacker_faction, target_faction);
}

// [SEQUENCE: 7] Set player faction
void OpenWorldPvPSystem::SetPlayerFaction(core::ecs::EntityId player, uint32_t faction_id) {
    if (!world_->HasComponent<PvPStateComponent>(player)) world_->AddComponent(player, PvPStateComponent{});
    world_->GetComponent<PvPStateComponent>(player).faction_id = faction_id;
    spdlog::debug("Player {} joined faction {}", player, faction_id);
}

// [SEQUENCE: 8] Get player faction
uint32_t OpenWorldPvPSystem::GetPlayerFaction(core::ecs::EntityId player) const {
    if (!world_->HasComponent<PvPStateComponent>(player)) return 0;
    return world_->GetComponent<PvPStateComponent>(player).faction_id;
}

// [SEQUENCE: 9] Check faction hostility
bool OpenWorldPvPSystem::AreFactionsHostile(uint32_t faction1, uint32_t faction2) const {
    if (faction1 == faction2) return false;
    if (faction1 == 0 || faction2 == 0) return false;  // Neutral
    
    auto it = hostile_factions_.find(faction1);
    if (it != hostile_factions_.end()) {
        return it->second.find(faction2) != it->second.end();
    }
    return false;
}

// [SEQUENCE: 10] Start capturing zone
bool OpenWorldPvPSystem::StartCapture(core::ecs::EntityId player, core::ecs::EntityId zone) {
    if (!world_) return false;
    
    auto& zone_comp = world_->GetComponent<PvPZoneComponent>(zone);
    
    // Check if player in capture range
    if (!IsPlayerInZone(player, zone)) {
        return false;
    }
    
    // Add to capturing players
    zone_comp.capturing_players.push_back(player);
    
    spdlog::debug("Player {} started capturing zone {}", player, zone);
    return true;
}

// [SEQUENCE: 11] Update player zones
void OpenWorldPvPSystem::UpdatePlayerZones(float delta_time) {
    static float update_timer = 0.0f;
    update_timer += delta_time;
    
    if (update_timer < config_.zone_update_interval) {
        return;
    }
    update_timer = 0.0f;
    
    if (!world_) return;
    
    // Check all players with transforms
    for (auto player : world_->GetEntitiesWith<TransformComponent>()) {
        auto& transform = world_->GetComponent<TransformComponent>(player);
        
        // Find which zone player is in
        core::ecs::EntityId current_zone = 0;
        
        for (auto zone_entity : pvp_zones_) {
            auto bounds_it = zone_bounds_.find(zone_entity);
            if (bounds_it != zone_bounds_.end()) {
                const ZoneBounds& bounds = bounds_it->second;
                
                if (transform.position.x >= bounds.min.x &&
                    transform.position.x <= bounds.max.x &&
                    transform.position.y >= bounds.min.y &&
                    transform.position.y <= bounds.max.y &&
                    transform.position.z >= bounds.min.z &&
                    transform.position.z <= bounds.max.z) {
                    
                    current_zone = zone_entity;
                    break;
                }
            }
        }
        
        // Update player zone
        if (!world_->HasComponent<PvPStateComponent>(player)) world_->AddComponent(player, PvPStateComponent{});
        auto& player_state = world_->GetComponent<PvPStateComponent>(player);
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

// [SEQUENCE: 12] Player enters PvP zone
void OpenWorldPvPSystem::OnPlayerEnterZone(core::ecs::EntityId player, core::ecs::EntityId zone) {
    if (!world_) return;
    
    auto& zone_comp = world_->GetComponent<PvPZoneComponent>(zone);
    if (!zone_comp.pvp_enabled) return;
    
    // Flag player for PvP
    if (!world_->HasComponent<PvPStateComponent>(player)) world_->AddComponent(player, PvPStateComponent{});
    auto& player_state = world_->GetComponent<PvPStateComponent>(player);
    player_state.pvp_flagged = true;
    player_state.flag_time = std::chrono::steady_clock::now();
    
    // Apply zone buffs if controlled by player's faction
    if (zone_comp.controlling_faction == player_state.faction_id && 
        player_state.faction_id != 0) {
        
        auto& stats = world_->GetComponent<CombatStatsComponent>(player);
        // Apply territory buff
        stats.damage_increase += config_.territory_buff_bonus;
        stats.damage_reduction += config_.territory_buff_bonus;
    }
    
    spdlog::info("Player {} entered PvP zone '{}'", player, zone_comp.zone_name);
}

// [SEQUENCE: 13] Player leaves PvP zone
void OpenWorldPvPSystem::OnPlayerLeaveZone(core::ecs::EntityId player, core::ecs::EntityId zone) {
    if (!world_) return;
    
    auto& zone_comp = world_->GetComponent<PvPZoneComponent>(zone);
    
    // Remove from capturing players
    zone_comp.capturing_players.erase(
        std::remove(zone_comp.capturing_players.begin(),
                   zone_comp.capturing_players.end(), player),
        zone_comp.capturing_players.end()
    );
    
    // Remove zone buffs
    if (!world_->HasComponent<PvPStateComponent>(player)) return;
    auto& player_state = world_->GetComponent<PvPStateComponent>(player);
    if (zone_comp.controlling_faction == player_state.faction_id) {
        auto& stats = world_->GetComponent<CombatStatsComponent>(player);
        stats.damage_increase -= config_.territory_buff_bonus;
        stats.damage_reduction -= config_.territory_buff_bonus;
    }
    
    spdlog::info("Player {} left PvP zone '{}'", player, zone_comp.zone_name);
}

// [SEQUENCE: 14] Update zone captures
void OpenWorldPvPSystem::UpdateZoneCaptures(float delta_time) {
    if (!world_) return;
    
    for (auto zone_entity : pvp_zones_) {
        auto& zone = world_->GetComponent<PvPZoneComponent>(zone_entity);
        if (zone.capturing_players.empty()) continue;
        
        // Count capturing players by faction
        std::unordered_map<uint32_t, uint32_t> faction_counts;
        
        for (auto player : zone.capturing_players) {
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
        if (dominant_faction != 0 && dominant_faction != zone.controlling_faction) {
            zone.capture_progress += config_.capture_tick_rate * delta_time * max_count;
            
            if (zone.capture_progress >= 100.0f) {
                OnZoneCaptured(zone_entity, dominant_faction);
            }
        } else if (dominant_faction == zone.controlling_faction) {
            // Defend - reduce enemy progress
            zone.capture_progress = std::max(0.0f, zone.capture_progress - 
                                            config_.capture_tick_rate * delta_time);
        }
    }
}

// [SEQUENCE: 15] Zone captured
void OpenWorldPvPSystem::OnZoneCaptured(core::ecs::EntityId zone_entity, uint32_t faction_id) {
    if (!world_) return;
    
    auto& zone = world_->GetComponent<PvPZoneComponent>(zone_entity);
    
    uint32_t old_faction = zone.controlling_faction;
    zone.controlling_faction = faction_id;
    zone.capture_progress = 0.0f;
    
    // Clear capturing players
    zone.capturing_players.clear();
    
    // Update stats
    stats_.zones_flipped++;
    stats_.faction_territories[faction_id]++;
    if (old_faction != 0) {
        stats_.faction_territories[old_faction]--;
    }
    
    // Grant rewards to capturing faction players in zone
    for (auto player : world_->GetEntitiesWith<PvPStateComponent>()) {
        auto& state = world_->GetComponent<PvPStateComponent>(player);
        if (state.faction_id == faction_id && state.current_zone == zone_entity) {
            GrantObjectiveReward(player, 1);  // Zone capture
        }
    }
    
    spdlog::info("Zone '{}' captured by faction {} (was {})",
                zone.zone_name, faction_id, old_faction);
}

// [SEQUENCE: 16] Player killed player
void OpenWorldPvPSystem::OnPlayerKilledPlayer(core::ecs::EntityId killer, core::ecs::EntityId victim) {
    // Check if valid PvP kill
    if (!CanAttack(killer, victim)) {
        return;
    }
    
    // Update stats
    if (world_) {
        auto& killer_stats = world_->GetComponent<PvPStatsComponent>(killer);
        auto& victim_stats = world_->GetComponent<PvPStatsComponent>(victim);
        
        killer_stats.world_pvp_kills++;
        killer_stats.kills++;
        victim_stats.deaths++;
        
        // Update kill streak
        UpdateKillStreak(killer);
        victim_stats.current_streak = 0;
        
        // Grant honor
        GrantHonorKill(killer, victim);
    }
    
    // Update faction kills
    uint32_t killer_faction = GetPlayerFaction(killer);
    if (killer_faction != 0) {
        stats_.faction_kills[killer_faction]++;
    }
    
    stats_.total_kills++;
}

// [SEQUENCE: 17] Grant honor for kill
void OpenWorldPvPSystem::GrantHonorKill(core::ecs::EntityId killer, core::ecs::EntityId victim) {
    if (!world_) return;
    
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
        auto& zone = world_->GetComponent<PvPZoneComponent>(killer_zone);
        if (zone.controlling_faction != GetPlayerFaction(killer)) {
            honor = static_cast<uint32_t>(honor * 1.5f);  // 50% bonus
        }
    }
    
    // Grant honor
    auto& pvp_stats = world_->GetComponent<PvPStatsComponent>(killer);
    pvp_stats.honor_points += honor;
    spdlog::debug("Player {} gained {} honor for killing {}", killer, honor, victim);
}

// [SEQUENCE: 18] Update PvP flags
void OpenWorldPvPSystem::UpdatePvPFlags([[maybe_unused]] float delta_time) {
    auto now = std::chrono::steady_clock::now();
    
    for (auto player : world_->GetEntitiesWith<PvPStateComponent>()) {
        auto& state = world_->GetComponent<PvPStateComponent>(player);
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

// [SEQUENCE: 19] Capture objective
bool OpenWorldPvPSystem::CaptureObjective(core::ecs::EntityId player, 
                                         core::ecs::EntityId zone_entity,
                                         uint32_t objective_id) {
    if (!world_) return false;
    
    auto& zone = world_->GetComponent<PvPZoneComponent>(zone_entity);
    
    // Find objective
    for (auto& obj : zone.objectives) {
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

// [SEQUENCE: 20] Objective captured
void OpenWorldPvPSystem::OnObjectiveCaptured(core::ecs::EntityId zone_entity,
                                            uint32_t objective_id,
                                            uint32_t faction_id) {
    stats_.objectives_captured++;
    
    // Find all faction players in zone for rewards
    for (auto player : world_->GetEntitiesWith<PvPStateComponent>()) {
        auto& state = world_->GetComponent<PvPStateComponent>(player);
        if (state.faction_id == faction_id && state.current_zone == zone_entity) {
            GrantObjectiveReward(player, 2);  // Objective capture
        }
    }
    
    spdlog::info("Objective {} in zone {} captured by faction {}",
                objective_id, zone_entity, faction_id);
}

void OpenWorldPvPSystem::GrantObjectiveReward(core::ecs::EntityId player, uint32_t) {
    if (!world_) return;
    
    auto& pvp_stats = world_->GetComponent<PvPStatsComponent>(player);
    pvp_stats.honor_points += config_.honor_per_objective;
    pvp_stats.objectives_completed++;
}

void OpenWorldPvPSystem::UpdateKillStreak(core::ecs::EntityId player) {
    if (!world_) return;
    
    auto& pvp_stats = world_->GetComponent<PvPStatsComponent>(player);
    pvp_stats.current_streak++;
    pvp_stats.max_killing_spree = std::max(pvp_stats.max_killing_spree,
                                            pvp_stats.current_streak);
    
    // Announce sprees
    if (pvp_stats.current_streak == 3) {
        spdlog::info("Player {} is on a killing spree!", player);
    } else if (pvp_stats.current_streak == 5) {
        spdlog::info("Player {} is dominating!", player);
    } else if (pvp_stats.current_streak == 10) {
        spdlog::info("Player {} is UNSTOPPABLE!", player);
    }
}

bool OpenWorldPvPSystem::IsPlayerInZone(core::ecs::EntityId player, core::ecs::EntityId zone) const {
    if (!world_->HasComponent<PvPStateComponent>(player)) return false;
    return world_->GetComponent<PvPStateComponent>(player).current_zone == zone;
}

core::ecs::EntityId OpenWorldPvPSystem::GetPlayerZone(core::ecs::EntityId player) const {
    if (!world_->HasComponent<PvPStateComponent>(player)) return 0;
    return world_->GetComponent<PvPStateComponent>(player).current_zone;
}

void OpenWorldPvPSystem::AddObjective(core::ecs::EntityId zone, uint32_t objective_id,
                                     const core::utils::Vector3& position) {
    if (!world_) return;
    
    auto& zone_comp = world_->GetComponent<PvPZoneComponent>(zone);
    PvPZoneComponent::Objective obj;
    obj.objective_id = objective_id;
    obj.position = position;
    obj.controlling_team = 0;
    obj.capture_radius = 10.0f;
    obj.point_value = 1;
    
    zone_comp.objectives.push_back(obj);
}

bool OpenWorldPvPSystem::SetZonePvPEnabled(core::ecs::EntityId zone, bool enabled) {
    if (!world_) return false;
    
    auto& zone_comp = world_->GetComponent<PvPZoneComponent>(zone);
    zone_comp.pvp_enabled = enabled;
    return true;
}

std::vector<core::ecs::EntityId> OpenWorldPvPSystem::GetPvPEnabledPlayers() const {
    std::vector<core::ecs::EntityId> result;
    for (auto player : world_->GetEntitiesWith<PvPStateComponent>()) {
        if (world_->GetComponent<PvPStateComponent>(player).pvp_flagged) {
            result.push_back(player);
        }
    }
    return result;
}

bool OpenWorldPvPSystem::StopCapture(core::ecs::EntityId player, core::ecs::EntityId zone) {
    if (!world_) return false;
    
    auto& zone_comp = world_->GetComponent<PvPZoneComponent>(zone);
    zone_comp.capturing_players.erase(
        std::remove(zone_comp.capturing_players.begin(),
                   zone_comp.capturing_players.end(), player),
        zone_comp.capturing_players.end()
    );
    
    return true;
}

float OpenWorldPvPSystem::GetCaptureProgress(core::ecs::EntityId zone) const {
    if (!world_) return 0.0f;
    
    auto& zone_comp = world_->GetComponent<PvPZoneComponent>(zone);
    return zone_comp.capture_progress;
}

void OpenWorldPvPSystem::OnPlayerAssist(core::ecs::EntityId assister, [[maybe_unused]] core::ecs::EntityId victim) {
    if (!world_) return;
    
    auto& pvp_stats = world_->GetComponent<PvPStatsComponent>(assister);
    pvp_stats.assists++;
    pvp_stats.honor_points += config_.honor_per_assist;
}

} // namespace mmorpg::game::systems::pvp
