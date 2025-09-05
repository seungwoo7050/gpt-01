#include "game/systems/guild/guild_war_seamless_system.h"
#include "game/components/transform_component.h"
#include "game/components/health_component.h"
#include "game/components/guild_component.h"
#include "core/ecs/optimized/optimized_world.h"
#include <cmath>
#include <spdlog/spdlog.h>
#include <algorithm>

namespace mmorpg::game::systems::guild {

using namespace mmorpg::game::components;

// [SEQUENCE: MVP5-33] Implements the main update loop for the seamless war system.
void GuildWarSeamlessSystem::Update(float delta_time) {
    UpdatePlayerTerritories();
    UpdateWarPhases();
    UpdateWars(delta_time);

    static float resource_timer = 0.0f;
    resource_timer += delta_time;
    if (resource_timer >= 3600.0f) {  // Every hour
        DistributeTerritoryResources();
        resource_timer = 0.0f;
    }
}

// [SEQUENCE: MVP5-34] Registers a new territory in the world.
void GuildWarSeamlessSystem::RegisterTerritory(uint32_t territory_id, const std::string& name,
                                              const core::utils::Vector3& center, float radius) {
    TerritoryInfo territory;
    territory.territory_id = territory_id;
    territory.name = name;
    territory.center = center;
    territory.radius = radius;
    territory.current_owner = 0;  // Unclaimed
    
    if (name.find("Mines") != std::string::npos) {
        territory.resources.materials_per_hour = 1000;
    } else if (name.find("Market") != std::string::npos) {
        territory.resources.gold_per_hour = 2000;
    } else if (name.find("Fortress") != std::string::npos) {
        territory.resources.honor_per_hour = 200;
    }
    
    territories_[territory_id] = territory;
    spdlog::info("Registered territory: {} at ({}, {}, {}) radius {}", 
                name, center.x, center.y, center.z, radius);
}

// [SEQUENCE: MVP5-35] Declares a seamless war between two guilds over specified territories.
bool GuildWarSeamlessSystem::DeclareSeamlessWar(uint32_t guild_a, uint32_t guild_b,
                                               const std::vector<uint32_t>& contested_territory_ids) {
    if (guild_wars_[guild_a].size() >= config_.max_concurrent_wars ||
        guild_wars_[guild_b].size() >= config_.max_concurrent_wars) {
        spdlog::warn("Guild {} or {} at war limit", guild_a, guild_b);
        return false;
    }
    
    if (contested_territory_ids.empty() || 
        contested_territory_ids.size() > config_.max_territories_per_war) {
        spdlog::warn("Invalid territory count for war: {}", contested_territory_ids.size());
        return false;
    }
    
    auto war = std::make_unique<SeamlessWar>();
    war->war_id = next_war_id_++;
    war->guild_a_id = guild_a;
    war->guild_b_id = guild_b;
    war->phase = SeamlessWar::WarPhase::DECLARATION;
    war->declaration_time = std::chrono::steady_clock::now();
    war->war_start_time = war->declaration_time + std::chrono::seconds(
        static_cast<int>(config_.declaration_duration));
    
    for (uint32_t territory_id : contested_territory_ids) {
        auto ter_it = territories_.find(territory_id);
        if (ter_it != territories_.end()) {
            SeamlessWar::Territory territory;
            territory.territory_id = territory_id;
            territory.name = ter_it->second.name;
            territory.center = ter_it->second.center;
            territory.radius = ter_it->second.radius;
            territory.controlling_guild = ter_it->second.current_owner;
            territory.resource_value = ter_it->second.resources.gold_per_hour / 10;
            
            war->contested_territories.push_back(territory);
        }
    }
    
    uint32_t war_id = war->war_id;
    guild_wars_[guild_a].push_back(war_id);
    guild_wars_[guild_b].push_back(war_id);
    active_wars_[war_id] = std::move(war);
    
    stats_.total_wars_declared++;
    
    NotifyGuildMembers(guild_a, "War declared against guild " + std::to_string(guild_b));
    NotifyGuildMembers(guild_b, "War declared by guild " + std::to_string(guild_a));
    
    spdlog::info("Seamless war {} declared between guilds {} and {} for {} territories",
                war_id, guild_a, guild_b, contested_territory_ids.size());
    
    return true;
}

// [SEQUENCE: MVP5-36] Updates which players are in which territories.
void GuildWarSeamlessSystem::UpdatePlayerTerritories() {
    if (!m_world) return;

    for (auto& [territory_id, players] : territory_players_) {
        players.clear();
    }
    
    for (auto player : m_world->GetEntitiesWith<TransformComponent>()) {
        auto& transform = m_world->GetComponent<TransformComponent>(player);
        
        for (const auto& [territory_id, territory] : territories_) {
            float dx = transform.position.x - territory.center.x;
            float dy = transform.position.y - territory.center.y;
            float dz = transform.position.z - territory.center.z;
            float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (distance <= territory.radius) {
                territory_players_[territory_id].insert(player);
                break;
            }
        }
    }
}

// [SEQUENCE: MVP5-37] Transitions wars between phases (e.g., declaration to active).
void GuildWarSeamlessSystem::UpdateWarPhases() {
    auto now = std::chrono::steady_clock::now();
    
    for (auto& [war_id, war] : active_wars_) {
        switch (war->phase) {
            case SeamlessWar::WarPhase::DECLARATION: {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    now - war->declaration_time).count();
                
                if (elapsed >= config_.declaration_duration - config_.preparation_duration) {
                    war->phase = SeamlessWar::WarPhase::PREPARATION;
                    NotifyGuildMembers(war->guild_a_id, "War begins in 1 hour!");
                    NotifyGuildMembers(war->guild_b_id, "War begins in 1 hour!");
                }
                break;
            }
            
            case SeamlessWar::WarPhase::PREPARATION: {
                if (now >= war->war_start_time) {
                    StartWar(*war);
                }
                break;
            }
            
            case SeamlessWar::WarPhase::ACTIVE: {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    now - war->war_start_time).count();
                
                if (elapsed >= config_.war_duration) {
                    EndWar(*war);
                }
                break;
            }
            
            case SeamlessWar::WarPhase::RESOLUTION: {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    now - war->war_end_time).count();
                
                if (elapsed >= config_.resolution_duration) {
                    // War fully resolved, can be cleaned up
                }
                break;
            }
        }
    }
}

// [SEQUENCE: MVP5-38] Starts a war, transitioning it to the ACTIVE phase.
void GuildWarSeamlessSystem::StartWar(SeamlessWar& war) {
    war.phase = SeamlessWar::WarPhase::ACTIVE;
    war.war_start_time = std::chrono::steady_clock::now();
    
    for (auto& territory : war.contested_territories) {
        territories_[territory.territory_id].claimed_by_guilds.insert(war.guild_a_id);
        territories_[territory.territory_id].claimed_by_guilds.insert(war.guild_b_id);
    }
    
    NotifyGuildMembers(war.guild_a_id, "War has begun! Capture the territories!");
    NotifyGuildMembers(war.guild_b_id, "War has begun! Defend your lands!");
    
    spdlog::info("Seamless war {} is now active", war.war_id);
}

// [SEQUENCE: MVP5-39] Updates ongoing wars.
void GuildWarSeamlessSystem::UpdateWars(float delta_time) {
    for (auto& [war_id, war] : active_wars_) {
        if (war->phase != SeamlessWar::WarPhase::ACTIVE) {
            continue;
        }
        
        UpdateTerritoryBattles(delta_time);
        
        for (const auto& territory : war->contested_territories) {
            if (territory.controlling_guild != 0) {
                war->territory_control_time[territory.controlling_guild] += delta_time;
            }
        }
    }
}

// [SEQUENCE: MVP5-40] Updates territory control based on player presence.
void GuildWarSeamlessSystem::UpdateTerritoryBattles(float delta_time) {
    if (!m_world) return;
    
    for (auto& [war_id, war] : active_wars_) {
        if (war->phase != SeamlessWar::WarPhase::ACTIVE) continue;
        
        for (auto& territory : war->contested_territories) {
            uint32_t guild_a_count = 0;
            uint32_t guild_b_count = 0;
            
            auto ter_players_it = territory_players_.find(territory.territory_id);
            if (ter_players_it != territory_players_.end()) {
                for (auto player : ter_players_it->second) {
                    auto& guild_comp = m_world->GetComponent<GuildComponent>(player);
                    if (guild_comp.guild_id == war->guild_a_id) {
                        guild_a_count++;
                        war->guild_a_participants.insert(player);
                    } else if (guild_comp.guild_id == war->guild_b_id) {
                        guild_b_count++;
                        war->guild_b_participants.insert(player);
                    }
                }
            }
            
            if (guild_a_count >= config_.min_players_to_capture && guild_a_count > guild_b_count) {
                float capture_rate = std::min(
                    (guild_a_count - guild_b_count) * config_.capture_rate,
                    config_.max_capture_rate
                );
                territory.control_percentage += capture_rate * delta_time;
                
                if (territory.control_percentage >= config_.territory_control_threshold) {
                    if (territory.controlling_guild != war->guild_a_id) {
                        territory.controlling_guild = war->guild_a_id;
                        territories_[territory.territory_id].current_owner = war->guild_a_id;
                        stats_.territories_changed_hands++;
                        
                        NotifyGuildMembers(war->guild_a_id, 
                            "Captured " + territory.name + "!");
                        NotifyGuildMembers(war->guild_b_id, 
                            "Lost " + territory.name + "!");
                    }
                }
            } else if (guild_b_count >= config_.min_players_to_capture && guild_b_count > guild_a_count) {
                float capture_rate = std::min(
                    (guild_b_count - guild_a_count) * config_.capture_rate,
                    config_.max_capture_rate
                );
                territory.control_percentage -= capture_rate * delta_time;
                
                if (territory.control_percentage <= -config_.territory_control_threshold) {
                    if (territory.controlling_guild != war->guild_b_id) {
                        territory.controlling_guild = war->guild_b_id;
                        territories_[territory.territory_id].current_owner = war->guild_b_id;
                        stats_.territories_changed_hands++;
                        
                        NotifyGuildMembers(war->guild_b_id, 
                            "Captured " + territory.name + "!");
                        NotifyGuildMembers(war->guild_a_id, 
                            "Lost " + territory.name + "!");
                    }
                }
            } else {
                if (territory.control_percentage > 0) {
                    territory.control_percentage -= config_.capture_rate * 0.5f * delta_time;
                } else if (territory.control_percentage < 0) {
                    territory.control_percentage += config_.capture_rate * 0.5f * delta_time;
                }
            }
            
            territory.control_percentage = std::clamp(territory.control_percentage, -100.0f, 100.0f);
        }
    }
}

// [SEQUENCE: MVP5-41] Handles a kill that occurs during a seamless war.
void GuildWarSeamlessSystem::OnWarKill(core::ecs::EntityId killer, core::ecs::EntityId victim) {
    if (!m_world) return;
    
    auto& killer_guild = m_world->GetComponent<GuildComponent>(killer);
    auto& victim_guild = m_world->GetComponent<GuildComponent>(victim);
    
    for (auto& [war_id, war] : active_wars_) {
        if (war->phase != SeamlessWar::WarPhase::ACTIVE) continue;
        
        bool killer_in_war = (killer_guild.guild_id == war->guild_a_id || 
                             killer_guild.guild_id == war->guild_b_id);
        bool victim_in_war = (victim_guild.guild_id == war->guild_a_id || 
                             victim_guild.guild_id == war->guild_b_id);
        
        if (killer_in_war && victim_in_war && 
            killer_guild.guild_id != victim_guild.guild_id) {
            
            if (killer_guild.guild_id == war->guild_a_id) {
                war->guild_a_kills++;
                war->guild_b_deaths++;
            } else {
                war->guild_b_kills++;
                war->guild_a_deaths++;
            }
            
            war->player_war_score[killer] += config_.points_per_kill;
            
            killer_guild.war_contribution += config_.points_per_kill;
            
            spdlog::debug("War kill in war {}: {} killed {}", 
                         war_id, killer, victim);
        }
    }
}

// [SEQUENCE: MVP5-42] Ends a war and transitions it to the RESOLUTION phase.
void GuildWarSeamlessSystem::EndWar(SeamlessWar& war) {
    war.phase = SeamlessWar::WarPhase::RESOLUTION;
    war.war_end_time = std::chrono::steady_clock::now();
    
    DetermineWarVictor(war);
    
    DistributeWarRewards(war);
    
    for (const auto& territory : war.contested_territories) {
        territories_[territory.territory_id].claimed_by_guilds.clear();
    }
    
    stats_.wars_completed++;
    
    spdlog::info("Seamless war {} ended. Guild A: {} kills, Guild B: {} kills",
                war.war_id, war.guild_a_kills, war.guild_b_kills);
}

// [SEQUENCE: MVP5-43] Determines the victor of a war based on score.
void GuildWarSeamlessSystem::DetermineWarVictor(const SeamlessWar& war) {
    uint32_t guild_a_score = 0;
    uint32_t guild_b_score = 0;
    
    guild_a_score += war.guild_a_kills * config_.points_per_kill;
    guild_b_score += war.guild_b_kills * config_.points_per_kill;
    
    for (const auto& [guild_id, control_time] : war.territory_control_time) {
        uint32_t time_score = static_cast<uint32_t>(
            control_time / 60.0f * config_.points_per_territory_minute
        );
        
        if (guild_id == war.guild_a_id) {
            guild_a_score += time_score;
        } else if (guild_id == war.guild_b_id) {
            guild_b_score += time_score;
        }
    }
    
    if (guild_a_score > guild_b_score) {
        stats_.guild_war_victories[war.guild_a_id]++;
        NotifyGuildMembers(war.guild_a_id, "Victory! War score: " + 
                          std::to_string(guild_a_score) + " vs " + 
                          std::to_string(guild_b_score));
        NotifyGuildMembers(war.guild_b_id, "Defeat. War score: " + 
                          std::to_string(guild_b_score) + " vs " + 
                          std::to_string(guild_a_score));
    } else if (guild_b_score > guild_a_score) {
        stats_.guild_war_victories[war.guild_b_id]++;
        NotifyGuildMembers(war.guild_b_id, "Victory! War score: " + 
                          std::to_string(guild_b_score) + " vs " + 
                          std::to_string(guild_a_score));
        NotifyGuildMembers(war.guild_a_id, "Defeat. War score: " + 
                          std::to_string(guild_a_score) + " vs " + 
                          std::to_string(guild_b_score));
    } else {
        NotifyGuildMembers(war.guild_a_id, "Draw. War score: " + 
                          std::to_string(guild_a_score));
        NotifyGuildMembers(war.guild_b_id, "Draw. War score: " + 
                          std::to_string(guild_b_score));
    }
}

// [SEQUENCE: MVP5-44] Distributes rewards to war participants.
void GuildWarSeamlessSystem::DistributeWarRewards(const SeamlessWar& war) {
    if (!m_world) return;
    
    for (const auto& [player, score] : war.player_war_score) {
        auto& guild_comp = m_world->GetComponent<GuildComponent>(player);
        uint32_t reward = 500;
        
        reward += score * 10;
        
        bool in_winning_guild = false;
        if (war.guild_a_kills > war.guild_b_kills && 
            guild_comp.guild_id == war.guild_a_id) {
            in_winning_guild = true;
        } else if (war.guild_b_kills > war.guild_a_kills && 
                    guild_comp.guild_id == war.guild_b_id) {
            in_winning_guild = true;
        }
        
        if (in_winning_guild) {
            reward *= 2;
        }
        
        spdlog::debug("Player {} earned {} war rewards", player, reward);
    }
}

// [SEQUENCE: MVP5-45] Distributes resources from controlled territories.
void GuildWarSeamlessSystem::DistributeTerritoryResources() {
    for (const auto& [territory_id, territory] : territories_) {
        if (territory.current_owner != 0) {
            spdlog::debug("Guild {} earned resources from {}: {} gold, {} materials, {} honor",
                         territory.current_owner, territory.name,
                         territory.resources.gold_per_hour,
                         territory.resources.materials_per_hour,
                         territory.resources.honor_per_hour);
            
            stats_.guild_territories_owned[territory.current_owner]++;
        }
    }
}

// [SEQUENCE: MVP5-46] Checks if a guild is in an active war.
bool GuildWarSeamlessSystem::IsGuildInWar(uint32_t guild_id) const {
    auto it = guild_wars_.find(guild_id);
    if (it == guild_wars_.end()) return false;
    
    for (uint32_t war_id : it->second) {
        auto war_it = active_wars_.find(war_id);
        if (war_it != active_wars_.end() && 
            war_it->second->phase == SeamlessWar::WarPhase::ACTIVE) {
            return true;
        }
    }
    
    return false;
}

// [SEQUENCE: MVP5-47] Checks if a player is in a contested war zone.
bool GuildWarSeamlessSystem::IsInWarZone(core::ecs::EntityId player) const {
    auto it = player_in_territory_.find(player);
    if (it == player_in_territory_.end()) return false;
    
    uint32_t territory_id = it->second;
    
    for (const auto& [war_id, war] : active_wars_) {
        if (war->phase != SeamlessWar::WarPhase::ACTIVE) continue;
        
        for (const auto& territory : war->contested_territories) {
            if (territory.territory_id == territory_id) {
                return true;
            }
        }
    }
    
    return false;
}

// [SEQUENCE: MVP5-48] Checks if two players can attack each other under war rules.
bool GuildWarSeamlessSystem::CanAttackInWar(core::ecs::EntityId attacker, core::ecs::EntityId target) const {
    if (!IsInWarZone(attacker) || !IsInWarZone(target)) {
        return false;
    }
    
    if (!m_world) return false;
    
    auto& attacker_guild = m_world->GetComponent<GuildComponent>(attacker);
    auto& target_guild = m_world->GetComponent<GuildComponent>(target);
    
    if (attacker_guild.guild_id == target_guild.guild_id) return false;
    
    for (const auto& [war_id, war] : active_wars_) {
        if (war->phase != SeamlessWar::WarPhase::ACTIVE) continue;
        
        bool attacker_in_war = (attacker_guild.guild_id == war->guild_a_id || 
                               attacker_guild.guild_id == war->guild_b_id);
        bool target_in_war = (target_guild.guild_id == war->guild_a_id || 
                             target_guild.guild_id == war->guild_b_id);
        
        if (attacker_in_war && target_in_war) {
            return true;
        }
    }
    
    return false;
}

// [SEQUENCE: MVP5-49] Allows a guild to claim an unowned territory.
bool GuildWarSeamlessSystem::ClaimTerritory(uint32_t guild_id, uint32_t territory_id) {
    auto it = territories_.find(territory_id);
    if (it == territories_.end()) return false;
    
    if (it->second.current_owner == 0 && it->second.claimed_by_guilds.empty()) {
        it->second.current_owner = guild_id;
        stats_.guild_territories_owned[guild_id]++;
        
        spdlog::info("Guild {} claimed territory {}", guild_id, it->second.name);
        return true;
    }
    
    return false;
}

// [SEQUENCE: MVP5-50] Gets the ID of the guild that controls a territory.
uint32_t GuildWarSeamlessSystem::GetTerritoryController(uint32_t territory_id) const {
    auto it = territories_.find(territory_id);
    return (it != territories_.end()) ? it->second.current_owner : 0;
}

// [SEQUENCE: MVP5-51] Notifies all members of a guild with a message.
void GuildWarSeamlessSystem::NotifyGuildMembers(uint32_t guild_id, const std::string& message) {
    // TODO: Implement actual notification system
    spdlog::info("[Guild {}] {}", guild_id, message);
}

} // namespace mmorpg::game::systems::guild
