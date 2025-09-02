#include "game/systems/guild/guild_war_seamless_system.h"
#include "game/components/transform_component.h"
#include "game/components/health_component.h"
#include "core/ecs/world.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace mmorpg::game::systems::guild {

// [SEQUENCE: 1] System initialization
void GuildWarSeamlessSystem::OnSystemInit() {
    // Register main world territories
    RegisterTerritory(1, "Northern Fortress", {0, 1000, 0}, 200.0f);
    RegisterTerritory(2, "Eastern Mines", {1000, 0, 0}, 150.0f);
    RegisterTerritory(3, "Southern Port", {0, -1000, 0}, 180.0f);
    RegisterTerritory(4, "Western Plains", {-1000, 0, 0}, 250.0f);
    RegisterTerritory(5, "Central Market", {0, 0, 0}, 100.0f);
    
    spdlog::info("GuildWarSeamlessSystem initialized with {} territories", territories_.size());
}

// [SEQUENCE: 2] System cleanup
void GuildWarSeamlessSystem::OnSystemShutdown() {
    // End all active wars
    for (auto& [war_id, war] : active_wars_) {
        EndWar(*war);
    }
    active_wars_.clear();
    territories_.clear();
    spdlog::info("GuildWarSeamlessSystem shut down");
}

// [SEQUENCE: 3] Main update loop
void GuildWarSeamlessSystem::Update(float delta_time) {
    // Update player positions in territories
    UpdatePlayerTerritories();
    
    // Update war phases
    UpdateWarPhases();
    
    // Update ongoing wars
    UpdateWars(delta_time);
    
    // Distribute resources from controlled territories
    static float resource_timer = 0.0f;
    resource_timer += delta_time;
    if (resource_timer >= 3600.0f) {  // Every hour
        DistributeTerritoryResources();
        resource_timer = 0.0f;
    }
}

// [SEQUENCE: 4] Register territory
void GuildWarSeamlessSystem::RegisterTerritory(uint32_t territory_id, const std::string& name,
                                              const core::utils::Vector3& center, float radius) {
    TerritoryInfo territory;
    territory.territory_id = territory_id;
    territory.name = name;
    territory.center = center;
    territory.radius = radius;
    territory.current_owner = 0;  // Unclaimed
    
    // Set resource values based on territory
    if (name.find("Mines") != std::string::npos) {
        territory.resources.materials_per_hour = 1000;  // Double materials
    } else if (name.find("Market") != std::string::npos) {
        territory.resources.gold_per_hour = 2000;  // Double gold
    } else if (name.find("Fortress") != std::string::npos) {
        territory.resources.honor_per_hour = 200;  // Double honor
    }
    
    territories_[territory_id] = territory;
    spdlog::info("Registered territory: {} at ({}, {}, {}) radius {}", 
                name, center.x, center.y, center.z, radius);
}

// [SEQUENCE: 5] Declare seamless war
bool GuildWarSeamlessSystem::DeclareSeamlessWar(uint32_t guild_a, uint32_t guild_b,
                                               const std::vector<uint32_t>& contested_territory_ids) {
    // Check war limits
    if (guild_wars_[guild_a].size() >= config_.max_concurrent_wars ||
        guild_wars_[guild_b].size() >= config_.max_concurrent_wars) {
        spdlog::warn("Guild {} or {} at war limit", guild_a, guild_b);
        return false;
    }
    
    // Validate territories
    if (contested_territory_ids.empty() || 
        contested_territory_ids.size() > config_.max_territories_per_war) {
        spdlog::warn("Invalid territory count for war: {}", contested_territory_ids.size());
        return false;
    }
    
    // Create war
    auto war = std::make_unique<SeamlessWar>();
    war->war_id = next_war_id_++;
    war->guild_a_id = guild_a;
    war->guild_b_id = guild_b;
    war->phase = SeamlessWar::WarPhase::DECLARATION;
    war->declaration_time = std::chrono::steady_clock::now();
    war->war_start_time = war->declaration_time + std::chrono::seconds(
        static_cast<int>(config_.declaration_duration));
    
    // Add contested territories
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
    
    // Register war
    uint32_t war_id = war->war_id;
    guild_wars_[guild_a].push_back(war_id);
    guild_wars_[guild_b].push_back(war_id);
    active_wars_[war_id] = std::move(war);
    
    stats_.total_wars_declared++;
    
    // Notify guilds
    NotifyGuildMembers(guild_a, "War declared against guild " + std::to_string(guild_b));
    NotifyGuildMembers(guild_b, "War declared by guild " + std::to_string(guild_a));
    
    spdlog::info("Seamless war {} declared between guilds {} and {} for {} territories",
                war_id, guild_a, guild_b, contested_territory_ids.size());
    
    return true;
}

// [SEQUENCE: 6] Update player territories
void GuildWarSeamlessSystem::UpdatePlayerTerritories() {
    auto* world = GetWorld();
    if (!world) return;
    
    // Clear previous mappings
    player_in_territory_.clear();
    for (auto& [territory_id, players] : territory_players_) {
        players.clear();
    }
    
    // Check all players with transforms
    auto players = world->GetEntitiesWithComponent<TransformComponent>();
    
    for (auto player : players) {
        auto* transform = world->GetComponent<TransformComponent>(player);
        if (!transform) continue;
        
        // Check which territory player is in
        for (const auto& [territory_id, territory] : territories_) {
            float distance = (transform->position - territory.center).Length();
            if (distance <= territory.radius) {
                player_in_territory_[player] = territory_id;
                territory_players_[territory_id].insert(player);
                break;  // Player can only be in one territory
            }
        }
    }
}

// [SEQUENCE: 7] Update war phases
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
                    // This would be handled in a cleanup phase
                }
                break;
            }
        }
    }
}

// [SEQUENCE: 8] Start war
void GuildWarSeamlessSystem::StartWar(SeamlessWar& war) {
    war.phase = SeamlessWar::WarPhase::ACTIVE;
    war.war_start_time = std::chrono::steady_clock::now();
    
    // Mark territories as contested
    for (auto& territory : war.contested_territories) {
        territories_[territory.territory_id].claimed_by_guilds.insert(war.guild_a_id);
        territories_[territory.territory_id].claimed_by_guilds.insert(war.guild_b_id);
    }
    
    NotifyGuildMembers(war.guild_a_id, "War has begun! Capture the territories!");
    NotifyGuildMembers(war.guild_b_id, "War has begun! Defend your lands!");
    
    spdlog::info("Seamless war {} is now active", war.war_id);
}

// [SEQUENCE: 9] Update wars
void GuildWarSeamlessSystem::UpdateWars(float delta_time) {
    for (auto& [war_id, war] : active_wars_) {
        if (war->phase != SeamlessWar::WarPhase::ACTIVE) {
            continue;
        }
        
        // Update territory battles
        UpdateTerritoryBattles(delta_time);
        
        // Track territory control time
        for (const auto& territory : war->contested_territories) {
            if (territory.controlling_guild != 0) {
                war->territory_control_time[territory.controlling_guild] += delta_time;
            }
        }
    }
}

// [SEQUENCE: 10] Update territory battles
void GuildWarSeamlessSystem::UpdateTerritoryBattles(float delta_time) {
    auto* world = GetWorld();
    if (!world) return;
    
    for (auto& [war_id, war] : active_wars_) {
        if (war->phase != SeamlessWar::WarPhase::ACTIVE) continue;
        
        for (auto& territory : war->contested_territories) {
            // Count guild members in territory
            uint32_t guild_a_count = 0;
            uint32_t guild_b_count = 0;
            
            auto ter_players_it = territory_players_.find(territory.territory_id);
            if (ter_players_it != territory_players_.end()) {
                for (auto player : ter_players_it->second) {
                    auto* guild_comp = world->GetComponent<GuildComponent>(player);
                    if (guild_comp) {
                        if (guild_comp->guild_id == war->guild_a_id) {
                            guild_a_count++;
                            war->guild_a_participants.insert(player);
                        } else if (guild_comp->guild_id == war->guild_b_id) {
                            guild_b_count++;
                            war->guild_b_participants.insert(player);
                        }
                    }
                }
            }
            
            // Update control percentage
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
                // Decay towards neutral if no clear controller
                if (territory.control_percentage > 0) {
                    territory.control_percentage -= config_.capture_rate * 0.5f * delta_time;
                } else if (territory.control_percentage < 0) {
                    territory.control_percentage += config_.capture_rate * 0.5f * delta_time;
                }
            }
            
            // Clamp control percentage
            territory.control_percentage = std::clamp(territory.control_percentage, -100.0f, 100.0f);
        }
    }
}

// [SEQUENCE: 11] Handle war kill
void GuildWarSeamlessSystem::OnWarKill(core::ecs::EntityId killer, core::ecs::EntityId victim) {
    auto* world = GetWorld();
    if (!world) return;
    
    auto* killer_guild = world->GetComponent<GuildComponent>(killer);
    auto* victim_guild = world->GetComponent<GuildComponent>(victim);
    
    if (!killer_guild || !victim_guild) return;
    
    // Find relevant wars
    for (auto& [war_id, war] : active_wars_) {
        if (war->phase != SeamlessWar::WarPhase::ACTIVE) continue;
        
        bool killer_in_war = (killer_guild->guild_id == war->guild_a_id || 
                             killer_guild->guild_id == war->guild_b_id);
        bool victim_in_war = (victim_guild->guild_id == war->guild_a_id || 
                             victim_guild->guild_id == war->guild_b_id);
        
        if (killer_in_war && victim_in_war && 
            killer_guild->guild_id != victim_guild->guild_id) {
            
            // Update kill counts
            if (killer_guild->guild_id == war->guild_a_id) {
                war->guild_a_kills++;
                war->guild_b_deaths++;
            } else {
                war->guild_b_kills++;
                war->guild_a_deaths++;
            }
            
            // Update player score
            war->player_war_score[killer] += config_.points_per_kill;
            
            // Update guild component
            killer_guild->war_contribution += config_.points_per_kill;
            
            spdlog::debug("War kill in war {}: {} killed {}", 
                         war_id, killer, victim);
        }
    }
}

// [SEQUENCE: 12] End war
void GuildWarSeamlessSystem::EndWar(SeamlessWar& war) {
    war.phase = SeamlessWar::WarPhase::RESOLUTION;
    war.war_end_time = std::chrono::steady_clock::now();
    
    // Determine victor
    DetermineWarVictor(war);
    
    // Distribute rewards
    DistributeWarRewards(war);
    
    // Clear territory claims
    for (const auto& territory : war.contested_territories) {
        territories_[territory.territory_id].claimed_by_guilds.clear();
    }
    
    stats_.wars_completed++;
    
    spdlog::info("Seamless war {} ended. Guild A: {} kills, Guild B: {} kills",
                war.war_id, war.guild_a_kills, war.guild_b_kills);
}

// [SEQUENCE: 13] Determine war victor
void GuildWarSeamlessSystem::DetermineWarVictor(const SeamlessWar& war) {
    uint32_t guild_a_score = 0;
    uint32_t guild_b_score = 0;
    
    // Score from kills
    guild_a_score += war.guild_a_kills * config_.points_per_kill;
    guild_b_score += war.guild_b_kills * config_.points_per_kill;
    
    // Score from territory control time
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
    
    // Determine winner
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

// [SEQUENCE: 14] Distribute war rewards
void GuildWarSeamlessSystem::DistributeWarRewards(const SeamlessWar& war) {
    auto* world = GetWorld();
    if (!world) return;
    
    // Reward participants based on contribution
    for (const auto& [player, score] : war.player_war_score) {
        auto* guild_comp = world->GetComponent<GuildComponent>(player);
        if (guild_comp) {
            // Base participation reward
            uint32_t reward = 500;
            
            // Contribution bonus
            reward += score * 10;
            
            // Victory bonus
            bool in_winning_guild = false;
            if (war.guild_a_kills > war.guild_b_kills && 
                guild_comp->guild_id == war.guild_a_id) {
                in_winning_guild = true;
            } else if (war.guild_b_kills > war.guild_a_kills && 
                      guild_comp->guild_id == war.guild_b_id) {
                in_winning_guild = true;
            }
            
            if (in_winning_guild) {
                reward *= 2;
            }
            
            // TODO: Actually grant rewards
            spdlog::debug("Player {} earned {} war rewards", player, reward);
        }
    }
}

// [SEQUENCE: 15] Distribute territory resources
void GuildWarSeamlessSystem::DistributeTerritoryResources() {
    for (const auto& [territory_id, territory] : territories_) {
        if (territory.current_owner != 0) {
            // TODO: Actually grant resources to guild
            spdlog::debug("Guild {} earned resources from {}: {} gold, {} materials, {} honor",
                         territory.current_owner, territory.name,
                         territory.resources.gold_per_hour,
                         territory.resources.materials_per_hour,
                         territory.resources.honor_per_hour);
            
            stats_.guild_territories_owned[territory.current_owner]++;
        }
    }
}

// [SEQUENCE: 16] Query functions
bool GuildWarSeamlessSystem::IsGuildInWar(uint32_t guild_id) const {
    auto it = guild_wars_.find(guild_id);
    if (it == guild_wars_.end()) return false;
    
    // Check if any wars are active
    for (uint32_t war_id : it->second) {
        auto war_it = active_wars_.find(war_id);
        if (war_it != active_wars_.end() && 
            war_it->second->phase == SeamlessWar::WarPhase::ACTIVE) {
            return true;
        }
    }
    
    return false;
}

bool GuildWarSeamlessSystem::IsInWarZone(core::ecs::EntityId player) const {
    auto it = player_in_territory_.find(player);
    if (it == player_in_territory_.end()) return false;
    
    uint32_t territory_id = it->second;
    
    // Check if this territory is contested in any active war
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

// [SEQUENCE: 17] Can attack check
bool GuildWarSeamlessSystem::CanAttackInWar(core::ecs::EntityId attacker, core::ecs::EntityId target) const {
    if (!IsInWarZone(attacker) || !IsInWarZone(target)) {
        return false;
    }
    
    auto* world = GetWorld();
    if (!world) return false;
    
    auto* attacker_guild = world->GetComponent<GuildComponent>(attacker);
    auto* target_guild = world->GetComponent<GuildComponent>(target);
    
    if (!attacker_guild || !target_guild) return false;
    
    // Can't attack same guild
    if (attacker_guild->guild_id == target_guild->guild_id) return false;
    
    // Check if guilds are at war
    for (const auto& [war_id, war] : active_wars_) {
        if (war->phase != SeamlessWar::WarPhase::ACTIVE) continue;
        
        bool attacker_in_war = (attacker_guild->guild_id == war->guild_a_id || 
                               attacker_guild->guild_id == war->guild_b_id);
        bool target_in_war = (target_guild->guild_id == war->guild_a_id || 
                             target_guild->guild_id == war->guild_b_id);
        
        if (attacker_in_war && target_in_war) {
            return true;
        }
    }
    
    return false;
}

// [SEQUENCE: 18] Claim territory
bool GuildWarSeamlessSystem::ClaimTerritory(uint32_t guild_id, uint32_t territory_id) {
    auto it = territories_.find(territory_id);
    if (it == territories_.end()) return false;
    
    // Can only claim unowned territories outside of war
    if (it->second.current_owner == 0 && it->second.claimed_by_guilds.empty()) {
        it->second.current_owner = guild_id;
        stats_.guild_territories_owned[guild_id]++;
        
        spdlog::info("Guild {} claimed territory {}", guild_id, it->second.name);
        return true;
    }
    
    return false;
}

// [SEQUENCE: 19] Get territory controller
uint32_t GuildWarSeamlessSystem::GetTerritoryController(uint32_t territory_id) const {
    auto it = territories_.find(territory_id);
    return (it != territories_.end()) ? it->second.current_owner : 0;
}

// [SEQUENCE: 20] Notify guild members
void GuildWarSeamlessSystem::NotifyGuildMembers(uint32_t guild_id, const std::string& message) {
    // TODO: Implement actual notification system
    spdlog::info("[Guild {}] {}", guild_id, message);
}

} // namespace mmorpg::game::systems::guild