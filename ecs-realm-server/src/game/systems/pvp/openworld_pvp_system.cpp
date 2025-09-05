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

// [SEQUENCE: MVP5-56] Implements the main update loop for the open-world PvP system.
void OpenWorldPvPSystem::Update(float delta_time) {
    UpdatePlayerZones(delta_time);
    UpdateZoneCaptures(delta_time);
    UpdatePvPFlags(delta_time);
}

// [SEQUENCE: MVP5-57] Creates a new PvP zone entity.
core::ecs::EntityId OpenWorldPvPSystem::CreatePvPZone(const std::string& name,
                                                      const core::utils::Vector3& min,
                                                      const core::utils::Vector3& max) {
    if (!m_world) return 0;
    
    auto zone_entity = m_world->CreateEntity();
    
    PvPZoneComponent zone;
    zone.zone_id = static_cast<uint32_t>(zone_entity);
    zone.zone_name = name;
    zone.pvp_enabled = true;
    zone.faction_based = true;
    
    m_world->AddComponent(zone_entity, zone);
    
    zone_bounds_[zone_entity] = {min, max};
    pvp_zones_.push_back(zone_entity);
    
    spdlog::info("Created PvP zone '{}' ({})", name, zone_entity);
    return zone_entity;
}

// [SEQUENCE: MVP5-58] Checks if a player is currently flagged for PvP.
bool OpenWorldPvPSystem::IsPlayerPvPFlagged(core::ecs::EntityId player) const {
    if (!m_world->HasComponent<PvPStateComponent>(player)) return false;
    return m_world->GetComponent<PvPStateComponent>(player).pvp_flagged;
}

// [SEQUENCE: MVP5-59] Checks if an attacker can legally attack a target.
bool OpenWorldPvPSystem::CanAttack(core::ecs::EntityId attacker, core::ecs::EntityId target) const {
    if (attacker == target) return false;
    
    if (!IsPlayerPvPFlagged(attacker) || !IsPlayerPvPFlagged(target)) {
        return false;
    }
    
    uint32_t attacker_faction = GetPlayerFaction(attacker);
    uint32_t target_faction = GetPlayerFaction(target);
    
    if (attacker_faction == target_faction && attacker_faction != 0) {
        auto attacker_zone = GetPlayerZone(attacker);
        if (attacker_zone != 0) {
            if (m_world) {
                auto& zone = m_world->GetComponent<PvPZoneComponent>(attacker_zone);
                if (!zone.free_for_all) {
                    return false;
                }
            }
        }
    }
    
    return AreFactionsHostile(attacker_faction, target_faction);
}

// [SEQUENCE: MVP5-60] Sets the faction for a given player.
void OpenWorldPvPSystem::SetPlayerFaction(core::ecs::EntityId player, uint32_t faction_id) {
    if (!m_world->HasComponent<PvPStateComponent>(player)) m_world->AddComponent(player, PvPStateComponent{});
    m_world->GetComponent<PvPStateComponent>(player).faction_id = faction_id;
    spdlog::debug("Player {} joined faction {}", player, faction_id);
}

// [SEQUENCE: MVP5-61] Gets the faction of a given player.
uint32_t OpenWorldPvPSystem::GetPlayerFaction(core::ecs::EntityId player) const {
    if (!m_world->HasComponent<PvPStateComponent>(player)) return 0;
    return m_world->GetComponent<PvPStateComponent>(player).faction_id;
}

// [SEQUENCE: MVP5-62] Checks if two factions are hostile to each other.
bool OpenWorldPvPSystem::AreFactionsHostile(uint32_t faction1, uint32_t faction2) const {
    if (faction1 == faction2) return false;
    if (faction1 == 0 || faction2 == 0) return false;
    
    auto it = hostile_factions_.find(faction1);
    if (it != hostile_factions_.end()) {
        return it->second.find(faction2) != it->second.end();
    }
    return false;
}

// [SEQUENCE: MVP5-63] Initiates a zone capture for a player.
bool OpenWorldPvPSystem::StartCapture(core::ecs::EntityId player, core::ecs::EntityId zone) {
    if (!m_world) return false;
    
    auto& zone_comp = m_world->GetComponent<PvPZoneComponent>(zone);
    
    if (!IsPlayerInZone(player, zone)) {
        return false;
    }
    
    zone_comp.capturing_players.push_back(player);
    
    spdlog::debug("Player {} started capturing zone {}", player, zone);
    return true;
}

// [SEQUENCE: MVP5-64] Updates which zone each player is currently in.
void OpenWorldPvPSystem::UpdatePlayerZones(float delta_time) {
    static float update_timer = 0.0f;
    update_timer += delta_time;
    
    if (update_timer < config_.zone_update_interval) {
        return;
    }
    update_timer = 0.0f;
    
    if (!m_world) return;
    
    for (auto player : m_world->GetEntitiesWith<TransformComponent>()) {
        auto& transform = m_world->GetComponent<TransformComponent>(player);
        
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
        
        if (!m_world->HasComponent<PvPStateComponent>(player)) m_world->AddComponent(player, PvPStateComponent{});
        auto& player_state = m_world->GetComponent<PvPStateComponent>(player);
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

// [SEQUENCE: MVP5-65] Handles the event of a player entering a PvP zone.
void OpenWorldPvPSystem::OnPlayerEnterZone(core::ecs::EntityId player, core::ecs::EntityId zone) {
    if (!m_world) return;
    
    auto& zone_comp = m_world->GetComponent<PvPZoneComponent>(zone);
    if (!zone_comp.pvp_enabled) return;
    
    if (!m_world->HasComponent<PvPStateComponent>(player)) m_world->AddComponent(player, PvPStateComponent{});
    auto& player_state = m_world->GetComponent<PvPStateComponent>(player);
    player_state.pvp_flagged = true;
    player_state.flag_time = std::chrono::steady_clock::now();
    
    if (zone_comp.controlling_faction == player_state.faction_id && 
        player_state.faction_id != 0) {
        
        auto& stats = m_world->GetComponent<CombatStatsComponent>(player);
        stats.damage_increase += config_.territory_buff_bonus;
        stats.damage_reduction += config_.territory_buff_bonus;
    }
    
    spdlog::info("Player {} entered PvP zone '{}'", player, zone_comp.zone_name);
}

// [SEQUENCE: MVP5-66] Handles the event of a player leaving a PvP zone.
void OpenWorldPvPSystem::OnPlayerLeaveZone(core::ecs::EntityId player, core::ecs::EntityId zone) {
    if (!m_world) return;
    
    auto& zone_comp = m_world->GetComponent<PvPZoneComponent>(zone);
    
    zone_comp.capturing_players.erase(
        std::remove(zone_comp.capturing_players.begin(),
                   zone_comp.capturing_players.end(), player),
        zone_comp.capturing_players.end()
    );
    
    if (!m_world->HasComponent<PvPStateComponent>(player)) return;
    auto& player_state = m_world->GetComponent<PvPStateComponent>(player);
    if (zone_comp.controlling_faction == player_state.faction_id) {
        auto& stats = m_world->GetComponent<CombatStatsComponent>(player);
        stats.damage_increase -= config_.territory_buff_bonus;
        stats.damage_reduction -= config_.territory_buff_bonus;
    }
    
    spdlog::info("Player {} left PvP zone '{}'", player, zone_comp.zone_name);
}

// [SEQUENCE: MVP5-67] Updates the capture progress for all zones.
void OpenWorldPvPSystem::UpdateZoneCaptures(float delta_time) {
    if (!m_world) return;
    
    for (auto zone_entity : pvp_zones_) {
        auto& zone = m_world->GetComponent<PvPZoneComponent>(zone_entity);
        if (zone.capturing_players.empty()) continue;
        
        std::unordered_map<uint32_t, uint32_t> faction_counts;
        
        for (auto player : zone.capturing_players) {
            uint32_t faction = GetPlayerFaction(player);
            if (faction != 0) {
                faction_counts[faction]++;
            }
        }
        
        uint32_t dominant_faction = 0;
        uint32_t max_count = 0;
        
        for (const auto& [faction, count] : faction_counts) {
            if (count > max_count) {
                max_count = count;
                dominant_faction = faction;
            }
        }
        
        if (dominant_faction != 0 && dominant_faction != zone.controlling_faction) {
            zone.capture_progress += config_.capture_tick_rate * delta_time * max_count;
            
            if (zone.capture_progress >= 100.0f) {
                OnZoneCaptured(zone_entity, dominant_faction);
            }
        } else if (dominant_faction == zone.controlling_faction) {
            zone.capture_progress = std::max(0.0f, zone.capture_progress - 
                                            config_.capture_tick_rate * delta_time);
        }
    }
}

// [SEQUENCE: MVP5-68] Handles the event of a zone being captured.
void OpenWorldPvPSystem::OnZoneCaptured(core::ecs::EntityId zone_entity, uint32_t faction_id) {
    if (!m_world) return;
    
    auto& zone = m_world->GetComponent<PvPZoneComponent>(zone_entity);
    
    uint32_t old_faction = zone.controlling_faction;
    zone.controlling_faction = faction_id;
    zone.capture_progress = 0.0f;
    
    zone.capturing_players.clear();
    
    stats_.zones_flipped++;
    stats_.faction_territories[faction_id]++;
    if (old_faction != 0) {
        stats_.faction_territories[old_faction]--;
    }
    
    for (auto player : m_world->GetEntitiesWith<PvPStateComponent>()) {
        auto& state = m_world->GetComponent<PvPStateComponent>(player);
        if (state.faction_id == faction_id && state.current_zone == zone_entity) {
            GrantObjectiveReward(player, 1);
        }
    }
    
    spdlog::info("Zone '{}' captured by faction {} (was {})",
                zone.zone_name, faction_id, old_faction);
}

// [SEQUENCE: MVP5-69] Handles a player killing another player in open-world PvP.
void OpenWorldPvPSystem::OnPlayerKilledPlayer(core::ecs::EntityId killer, core::ecs::EntityId victim) {
    if (!CanAttack(killer, victim)) {
        return;
    }
    
    if (m_world) {
        auto& killer_stats = m_world->GetComponent<PvPStatsComponent>(killer);
        auto& victim_stats = m_world->GetComponent<PvPStatsComponent>(victim);
        
        killer_stats.world_pvp_kills++;
        killer_stats.kills++;
        victim_stats.deaths++;
        
        UpdateKillStreak(killer);
        victim_stats.current_streak = 0;
        
        GrantHonorKill(killer, victim);
    }
    
    uint32_t killer_faction = GetPlayerFaction(killer);
    if (killer_faction != 0) {
        stats_.faction_kills[killer_faction]++;
    }
    
    stats_.total_kills++;
}

// [SEQUENCE: MVP5-70] Grants honor points for a kill, considering diminishing returns.
void OpenWorldPvPSystem::GrantHonorKill(core::ecs::EntityId killer, core::ecs::EntityId victim) {
    if (!m_world) return;
    
    auto kill_key = std::make_pair(killer, victim);
    auto& kill_record = kill_history_[kill_key];
    
    auto now = std::chrono::steady_clock::now();
    auto time_since_last = std::chrono::duration_cast<std::chrono::minutes>(
        now - kill_record.last_kill_time
    ).count();
    
    if (time_since_last > 60) {  // 1 hour
        kill_record.kill_count = 0;
    }
    
    kill_record.kill_count++;
    kill_record.last_kill_time = now;
    
    uint32_t honor = config_.honor_per_kill;
    if (kill_record.kill_count > config_.honor_diminishing_returns) {
        honor = honor / kill_record.kill_count;
    }
    
    auto killer_zone = GetPlayerZone(killer);
    if (killer_zone != 0) {
        auto& zone = m_world->GetComponent<PvPZoneComponent>(killer_zone);
        if (zone.controlling_faction != GetPlayerFaction(killer)) {
            honor = static_cast<uint32_t>(honor * 1.5f);  // 50% bonus
        }
    }
    
    auto& pvp_stats = m_world->GetComponent<PvPStatsComponent>(killer);
    pvp_stats.honor_points += honor;
    spdlog::debug("Player {} gained {} honor for killing {}", killer, honor, victim);
}

// [SEQUENCE: MVP5-71] Updates the PvP flags for all players.
void OpenWorldPvPSystem::UpdatePvPFlags([[maybe_unused]] float delta_time) {
    auto now = std::chrono::steady_clock::now();
    
    for (auto player : m_world->GetEntitiesWith<PvPStateComponent>()) {
        auto& state = m_world->GetComponent<PvPStateComponent>(player);
        if (!state.pvp_flagged) continue;
        
        if (state.current_zone == 0) {
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

// [SEQUENCE: MVP5-72] Handles a player capturing a specific objective within a zone.
bool OpenWorldPvPSystem::CaptureObjective(core::ecs::EntityId player, 
                                         core::ecs::EntityId zone_entity,
                                         uint32_t objective_id) {
    if (!m_world) return false;
    
    auto& zone = m_world->GetComponent<PvPZoneComponent>(zone_entity);
    
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

// [SEQUENCE: MVP5-73] Handles the event of an objective being captured.
void OpenWorldPvPSystem::OnObjectiveCaptured(core::ecs::EntityId zone_entity,
                                            uint32_t objective_id,
                                            uint32_t faction_id) {
    stats_.objectives_captured++;
    
    for (auto player : m_world->GetEntitiesWith<PvPStateComponent>()) {
        auto& state = m_world->GetComponent<PvPStateComponent>(player);
        if (state.faction_id == faction_id && state.current_zone == zone_entity) {
            GrantObjectiveReward(player, 2);
        }
    }
    
    spdlog::info("Objective {} in zone {} captured by faction {}",
                objective_id, zone_entity, faction_id);
}

// [SEQUENCE: MVP5-74] Grants rewards for capturing an objective.
void OpenWorldPvPSystem::GrantObjectiveReward(core::ecs::EntityId player, uint32_t) {
    if (!m_world) return;
    
    auto& pvp_stats = m_world->GetComponent<PvPStatsComponent>(player);
    pvp_stats.honor_points += config_.honor_per_objective;
    pvp_stats.objectives_completed++;
}

// [SEQUENCE: MVP5-75] Updates a player's kill streak.
void OpenWorldPvPSystem::UpdateKillStreak(core::ecs::EntityId player) {
    if (!m_world) return;
    
    auto& pvp_stats = m_world->GetComponent<PvPStatsComponent>(player);
    pvp_stats.current_streak++;
    pvp_stats.max_killing_spree = std::max(pvp_stats.max_killing_spree,
                                            pvp_stats.current_streak);
    
    if (pvp_stats.current_streak == 3) {
        spdlog::info("Player {} is on a killing spree!", player);
    } else if (pvp_stats.current_streak == 5) {
        spdlog::info("Player {} is dominating!", player);
    } else if (pvp_stats.current_streak == 10) {
        spdlog::info("Player {} is UNSTOPPABLE!", player);
    }
}

// [SEQUENCE: MVP5-76] Checks if a player is in a specific zone.
bool OpenWorldPvPSystem::IsPlayerInZone(core::ecs::EntityId player, core::ecs::EntityId zone) const {
    if (!m_world->HasComponent<PvPStateComponent>(player)) return false;
    return m_world->GetComponent<PvPStateComponent>(player).current_zone == zone;
}

// [SEQUENCE: MVP5-77] Gets the zone a player is currently in.
core::ecs::EntityId OpenWorldPvPSystem::GetPlayerZone(core::ecs::EntityId player) const {
    if (!m_world->HasComponent<PvPStateComponent>(player)) return 0;
    return m_world->GetComponent<PvPStateComponent>(player).current_zone;
}

// [SEQUENCE: MVP5-78] Adds a new objective to a PvP zone.
void OpenWorldPvPSystem::AddObjective(core::ecs::EntityId zone, uint32_t objective_id,
                                     const core::utils::Vector3& position) {
    if (!m_world) return;
    
    auto& zone_comp = m_world->GetComponent<PvPZoneComponent>(zone);
    PvPZoneComponent::Objective obj;
    obj.objective_id = objective_id;
    obj.position = position;
    obj.controlling_team = 0;
    obj.capture_radius = 10.0f;
    obj.point_value = 1;
    
    zone_comp.objectives.push_back(obj);
}

// [SEQUENCE: MVP5-79] Enables or disables PvP in a zone.
bool OpenWorldPvPSystem::SetZonePvPEnabled(core::ecs::EntityId zone, bool enabled) {
    if (!m_world) return false;
    
    auto& zone_comp = m_world->GetComponent<PvPZoneComponent>(zone);
    zone_comp.pvp_enabled = enabled;
    return true;
}

// [SEQUENCE: MVP5-80] Gets a list of all players currently flagged for PvP.
std::vector<core::ecs::EntityId> OpenWorldPvPSystem::GetPvPEnabledPlayers() const {
    std::vector<core::ecs::EntityId> result;
    for (auto player : m_world->GetEntitiesWith<PvPStateComponent>()) {
        if (m_world->GetComponent<PvPStateComponent>(player).pvp_flagged) {
            result.push_back(player);
        }
    }
    return result;
}

// [SEQUENCE: MVP5-81] Stops a player from capturing a zone.
bool OpenWorldPvPSystem::StopCapture(core::ecs::EntityId player, core::ecs::EntityId zone) {
    if (!m_world) return false;
    
    auto& zone_comp = m_world->GetComponent<PvPZoneComponent>(zone);
    zone_comp.capturing_players.erase(
        std::remove(zone_comp.capturing_players.begin(),
                   zone_comp.capturing_players.end(), player),
        zone_comp.capturing_players.end()
    );
    
    return true;
}

// [SEQUENCE: MVP5-82] Gets the current capture progress of a zone.
float OpenWorldPvPSystem::GetCaptureProgress(core::ecs::EntityId zone) const {
    if (!m_world) return 0.0f;
    
    auto& zone_comp = m_world->GetComponent<PvPZoneComponent>(zone);
    return zone_comp.capture_progress;
}

// [SEQUENCE: MVP5-83] Handles a player assisting in a kill.
void OpenWorldPvPSystem::OnPlayerAssist(core::ecs::EntityId assister, [[maybe_unused]] core::ecs::EntityId victim) {
    if (!m_world) return;
    
    auto& pvp_stats = m_world->GetComponent<PvPStatsComponent>(assister);
    pvp_stats.assists++;
    pvp_stats.honor_points += config_.honor_per_assist;
}

} // namespace mmorpg::game::systems::pvp