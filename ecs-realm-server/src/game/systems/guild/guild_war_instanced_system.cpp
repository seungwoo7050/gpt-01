#include "game/systems/guild/guild_war_instanced_system.h"
#include "game/components/transform_component.h"
#include "game/components/health_component.h"
#include "game/components/guild_component.h"
#include "core/ecs/optimized/optimized_world.h"
#include <cmath>
#include <spdlog/spdlog.h>
#include <algorithm>

namespace mmorpg::game::systems::guild {

using namespace mmorpg::game::components;

// [SEQUENCE: 3] Main update loop
void GuildWarInstancedSystem::Update(float delta_time) {
    // Update active war instances
    UpdateWarInstances(delta_time);
    
    // Clean up expired declarations
    auto now = std::chrono::steady_clock::now();
    for (auto& [guild_id, declarations] : pending_declarations_) {
        declarations.erase(
            std::remove_if(declarations.begin(), declarations.end(),
                [&](const GuildWarDeclaration& decl) {
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                        now - decl.declaration_time).count();
                    return elapsed > config_.declaration_expire_time;
                }),
            declarations.end()
        );
    }
}

// [SEQUENCE: 4] Declare war between guilds
bool GuildWarInstancedSystem::DeclareWar(uint32_t attacker_guild_id, uint32_t defender_guild_id) {
    // Check if already at war
    if (IsGuildAtWar(attacker_guild_id) || IsGuildAtWar(defender_guild_id)) {
        spdlog::warn("Guild {} or {} already at war", attacker_guild_id, defender_guild_id);
        return false;
    }
    
    // Check for existing declaration
    auto& declarations = pending_declarations_[defender_guild_id];
    auto it = std::find_if(declarations.begin(), declarations.end(),
        [attacker_guild_id](const GuildWarDeclaration& decl) {
            return decl.attacker_guild_id == attacker_guild_id;
        });
    
    if (it != declarations.end()) {
        spdlog::warn("War already declared between {} and {}", attacker_guild_id, defender_guild_id);
        return false;
    }
    
    // Create declaration
    GuildWarDeclaration declaration;
    declaration.attacker_guild_id = attacker_guild_id;
    declaration.defender_guild_id = defender_guild_id;
    declaration.declaration_time = std::chrono::steady_clock::now();
    declaration.war_start_time = declaration.declaration_time + std::chrono::minutes(5);
    
    declarations.push_back(declaration);
    
    spdlog::info("Guild {} declared war on guild {}", attacker_guild_id, defender_guild_id);
    return true;
}

// [SEQUENCE: 5] Accept war declaration
bool GuildWarInstancedSystem::AcceptWarDeclaration(uint32_t guild_id) {
    auto it = pending_declarations_.find(guild_id);
    if (it == pending_declarations_.end() || it->second.empty()) {
        return false;
    }
    
    // Accept the first declaration (could add UI selection)
    auto& declaration = it->second.front();
    declaration.accepted = true;
    
    // Create war instance
    auto instance_id = CreateWarInstance(declaration.attacker_guild_id, declaration.defender_guild_id);
    
    // Remove declaration
    it->second.erase(it->second.begin());
    
    return instance_id != 0;
}

// [SEQUENCE: 6] Create war instance
uint64_t GuildWarInstancedSystem::CreateWarInstance(uint32_t attacker_guild_id, uint32_t defender_guild_id) {
    auto instance = std::make_unique<GuildWarInstance>();
    instance->instance_id = next_instance_id_++;
    instance->attacker_guild_id = attacker_guild_id;
    instance->defender_guild_id = defender_guild_id;
    instance->start_time = std::chrono::steady_clock::now();
    instance->state = GuildWarInstance::InstanceState::PREPARING;
    
    // [SEQUENCE: 7] Create objectives
    instance->objectives.push_back({
        1, "Central Keep", {0, 0, 0}, 0, 0.0f, 200
    });
    instance->objectives.push_back({
        2, "North Tower", {0, 300, 0}, 0, 0.0f, 100
    });
    instance->objectives.push_back({
        3, "South Tower", {0, -300, 0}, 0, 0.0f, 100
    });
    instance->objectives.push_back({
        4, "Supply Depot", {-200, 0, 0}, 0, 0.0f, 50
    });
    instance->objectives.push_back({
        5, "Armory", {200, 0, 0}, 0, 0.0f, 50
    });
    
    // Map guilds to instance
    guild_to_instance_[attacker_guild_id] = instance->instance_id;
    guild_to_instance_[defender_guild_id] = instance->instance_id;
    
    // Store instance
    uint64_t instance_id = instance->instance_id;
    active_instances_[instance_id] = std::move(instance);
    
    stats_.total_wars++;
    stats_.active_wars++;
    
    spdlog::info("Created guild war instance {} between guilds {} and {}", 
                instance_id, attacker_guild_id, defender_guild_id);
    
    return instance_id;
}

// [SEQUENCE: 8] Join war instance
bool GuildWarInstancedSystem::JoinWarInstance(core::ecs::EntityId player, uint64_t instance_id) {
    auto it = active_instances_.find(instance_id);
    if (it == active_instances_.end()) {
        return false;
    }
    
    auto& instance = *it->second;
    
    // Check instance state
    if (instance.state != GuildWarInstance::InstanceState::PREPARING &&
        instance.state != GuildWarInstance::InstanceState::ACTIVE) {
        return false;
    }
    
    // Get player's guild
    if (!world_) return false;
    
    auto& guild_comp = world_->GetComponent<GuildComponent>(player);
    
    // Check if player's guild is in this war
    bool is_attacker = (guild_comp.guild_id == instance.attacker_guild_id);
    bool is_defender = (guild_comp.guild_id == instance.defender_guild_id);
    
    if (!is_attacker && !is_defender) {
        return false;
    }
    
    // Check participant limits
    if (is_attacker && instance.attacker_participants.size() >= config_.max_participants) {
        return false;
    }
    if (is_defender && instance.defender_participants.size() >= config_.max_participants) {
        return false;
    }
    
    // Add to participants
    if (is_attacker) {
        instance.attacker_participants.push_back(player);
    } else {
        instance.defender_participants.push_back(player);
    }
    
    // Map player to instance
    player_to_instance_[player] = instance_id;
    
    // Teleport player
    TeleportPlayerToInstance(player, instance);
    
    // Update guild component
    guild_comp.in_guild_war = true;
    guild_comp.war_contribution = 0;
    
    spdlog::debug("Player {} joined guild war instance {}", player, instance_id);
    
    // Check if ready to start
    if (instance.state == GuildWarInstance::InstanceState::PREPARING) {
        if (instance.attacker_participants.size() >= config_.min_participants &&
            instance.defender_participants.size() >= config_.min_participants) {
            instance.state = GuildWarInstance::InstanceState::ACTIVE;
            spdlog::info("Guild war instance {} is now active", instance_id);
        }
    }
    
    return true;
}

// [SEQUENCE: 9] Teleport player to instance
void GuildWarInstancedSystem::TeleportPlayerToInstance(core::ecs::EntityId player, GuildWarInstance& instance) {
    if (!world_) return;
    
    auto& transform = world_->GetComponent<TransformComponent>(player);
    
    // Save original position
    instance.original_positions[player] = transform.position;
    
    // Teleport to spawn point
    auto& guild_comp = world_->GetComponent<GuildComponent>(player);
    if (guild_comp.guild_id == instance.attacker_guild_id) {
        transform.position = instance.attacker_spawn;
    } else {
        transform.position = instance.defender_spawn;
    }
    
    // Add random spawn offset
    transform.position.x += (rand() % 20 - 10);
    transform.position.y += (rand() % 20 - 10);
}

// [SEQUENCE: 10] Update war instances
void GuildWarInstancedSystem::UpdateWarInstances(float delta_time) {
    std::vector<uint64_t> instances_to_remove;
    
    for (auto& [instance_id, instance] : active_instances_) {
        UpdateInstanceState(*instance, delta_time);
        
        if (instance->state == GuildWarInstance::InstanceState::CLEANUP) {
            instances_to_remove.push_back(instance_id);
        }
    }
    
    // Remove completed instances
    for (auto instance_id : instances_to_remove) {
        auto it = active_instances_.find(instance_id);
        if (it != active_instances_.end()) {
            auto& instance = *it->second;
            
            // Clear mappings
            guild_to_instance_.erase(instance.attacker_guild_id);
            guild_to_instance_.erase(instance.defender_guild_id);
            
            for (auto player : instance.attacker_participants) {
                player_to_instance_.erase(player);
            }
            for (auto player : instance.defender_participants) {
                player_to_instance_.erase(player);
            }
            
            active_instances_.erase(it);
            stats_.active_wars--;
        }
    }
}

// [SEQUENCE: 11] Update instance state
void GuildWarInstancedSystem::UpdateInstanceState(GuildWarInstance& instance, float delta_time) {
    switch (instance.state) {
        case GuildWarInstance::InstanceState::PREPARING: {
            // Waiting for minimum participants
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - instance.start_time).count();
            
            if (elapsed > config_.preparation_time) {
                // Force start or cancel
                if (instance.attacker_participants.size() >= config_.min_participants &&
                    instance.defender_participants.size() >= config_.min_participants) {
                    instance.state = GuildWarInstance::InstanceState::ACTIVE;
                } else {
                    // Not enough participants, cancel war
                    EndWarInstance(instance, 0);  // Draw
                }
            }
            break;
        }
        
        case GuildWarInstance::InstanceState::ACTIVE: {
            // Update war timer
            instance.remaining_time -= delta_time;
            
            // Update objective capture
            UpdateObjectiveCapture(instance, delta_time);
            
            // Check victory conditions
            if (CheckVictoryConditions(instance) || instance.remaining_time <= 0) {
                // Determine winner
                uint32_t winner = 0;
                if (instance.attacker_score > instance.defender_score) {
                    winner = instance.attacker_guild_id;
                } else if (instance.defender_score > instance.attacker_score) {
                    winner = instance.defender_guild_id;
                }
                EndWarInstance(instance, winner);
            }
            break;
        }
        
        case GuildWarInstance::InstanceState::ENDING: {
            // Give players time to see results
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - instance.end_time).count();
            
            if (elapsed > 30) {  // 30 seconds to view results
                // Return all players
                for (auto player : instance.attacker_participants) {
                    ReturnPlayerFromInstance(player, instance);
                }
                for (auto player : instance.defender_participants) {
                    ReturnPlayerFromInstance(player, instance);
                }
                
                instance.state = GuildWarInstance::InstanceState::CLEANUP;
            }
            break;
        }
        
        default:
            break;
    }
}

// [SEQUENCE: 12] Update objective capture
void GuildWarInstancedSystem::UpdateObjectiveCapture(GuildWarInstance& instance, float delta_time) {
    static float tick_timer = 0.0f;
    tick_timer += delta_time;
    
    if (tick_timer < config_.objective_tick_rate) {
        return;
    }
    tick_timer = 0.0f;
    
    if (!world_) return;
    
    // Update each objective
    for (auto& objective : instance.objectives) {
        // Count players near objective
        uint32_t attacker_count = 0;
        uint32_t defender_count = 0;
        
        for (auto player : instance.attacker_participants) {
            auto& transform = world_->GetComponent<TransformComponent>(player);
            float dx = transform.position.x - objective.position.x;
            float dy = transform.position.y - objective.position.y;
            float dz = transform.position.z - objective.position.z;
            float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (distance < 20.0f) {  // Capture radius
                attacker_count++;
            }
        }
        
        for (auto player : instance.defender_participants) {
            auto& transform = world_->GetComponent<TransformComponent>(player);
            float dx = transform.position.x - objective.position.x;
            float dy = transform.position.y - objective.position.y;
            float dz = transform.position.z - objective.position.z;
            float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (distance < 20.0f) {
                defender_count++;
            }
        }
        
        // Update capture progress
        if (attacker_count > defender_count) {
            objective.capture_progress += (attacker_count - defender_count) * delta_time * 10.0f;
            if (objective.capture_progress >= 100.0f) {
                objective.capture_progress = 100.0f;
                if (objective.controlling_guild != instance.attacker_guild_id) {
                    OnObjectiveCaptured(instance.instance_id, objective.objective_id, instance.attacker_guild_id);
                }
            }
        } else if (defender_count > attacker_count) {
            objective.capture_progress -= (defender_count - attacker_count) * delta_time * 10.0f;
            if (objective.capture_progress <= -100.0f) {
                objective.capture_progress = -100.0f;
                if (objective.controlling_guild != instance.defender_guild_id) {
                    OnObjectiveCaptured(instance.instance_id, objective.objective_id, instance.defender_guild_id);
                }
            }
        }
        
        // Grant points for controlled objectives
        if (objective.controlling_guild == instance.attacker_guild_id) {
            instance.attacker_score += config_.points_per_objective_tick;
        } else if (objective.controlling_guild == instance.defender_guild_id) {
            instance.defender_score += config_.points_per_objective_tick;
        }
    }
}

// [SEQUENCE: 13] Handle objective capture
void GuildWarInstancedSystem::OnObjectiveCaptured(uint64_t instance_id, uint32_t objective_id, uint32_t guild_id) {
    auto it = active_instances_.find(instance_id);
    if (it == active_instances_.end()) return;
    
    auto& instance = *it->second;
    
    for (auto& objective : instance.objectives) {
        if (objective.objective_id == objective_id) {
            objective.controlling_guild = guild_id;
            
            // Grant bonus points for capture
            if (guild_id == instance.attacker_guild_id) {
                instance.attacker_score += objective.point_value;
            } else {
                instance.defender_score += objective.point_value;
            }
            
            spdlog::info("Objective {} captured by guild {} in war {}", 
                        objective.name, guild_id, instance_id);
            break;
        }
    }
}

// [SEQUENCE: 14] Handle player kill in war
void GuildWarInstancedSystem::OnPlayerKilledInWar(core::ecs::EntityId killer, core::ecs::EntityId victim) {
    // Find which instance they're in
    auto killer_it = player_to_instance_.find(killer);
    auto victim_it = player_to_instance_.find(victim);
    
    if (killer_it == player_to_instance_.end() || victim_it == player_to_instance_.end()) {
        return;
    }
    
    if (killer_it->second != victim_it->second) {
        return;  // Not in same instance
    }
    
    auto instance_it = active_instances_.find(killer_it->second);
    if (instance_it == active_instances_.end()) return;
    
    auto& instance = *instance_it->second;
    if (!world_) return;
    
    // Update kill counts and scores
    auto& killer_guild = world_->GetComponent<GuildComponent>(killer);
    
    if (killer_guild.guild_id == instance.attacker_guild_id) {
        instance.attacker_kills++;
        instance.attacker_score += config_.points_per_kill;
    } else {
        instance.defender_kills++;
        instance.defender_score += config_.points_per_kill;
    }
    
    // Update player contribution
    killer_guild.war_contribution += config_.points_per_kill;
}

// [SEQUENCE: 15] Check victory conditions
bool GuildWarInstancedSystem::CheckVictoryConditions(GuildWarInstance& instance) {
    // Score limit victory
    if (instance.attacker_score >= config_.score_limit ||
        instance.defender_score >= config_.score_limit) {
        return true;
    }
    
    // Total domination (all objectives controlled)
    bool all_attacker = true;
    bool all_defender = true;
    
    for (const auto& objective : instance.objectives) {
        if (objective.controlling_guild != instance.attacker_guild_id) {
            all_attacker = false;
        }
        if (objective.controlling_guild != instance.defender_guild_id) {
            all_defender = false;
        }
    }
    
    return all_attacker || all_defender;
}

// [SEQUENCE: 16] End war instance
void GuildWarInstancedSystem::EndWarInstance(GuildWarInstance& instance, uint32_t winner_guild_id) {
    instance.state = GuildWarInstance::InstanceState::ENDING;
    instance.end_time = std::chrono::steady_clock::now();
    
    // Grant rewards
    GrantWarRewards(instance);
    
    // Update statistics
    if (winner_guild_id == instance.attacker_guild_id) {
        stats_.guild_victories[instance.attacker_guild_id]++;
    } else if (winner_guild_id == instance.defender_guild_id) {
        stats_.guild_victories[instance.defender_guild_id]++;
    }
    
    spdlog::info("Guild war {} ended. Winner: Guild {}, Score: {} vs {}", 
                instance.instance_id, winner_guild_id,
                instance.attacker_score, instance.defender_score);
}

// [SEQUENCE: 17] Grant war rewards
void GuildWarInstancedSystem::GrantWarRewards(const GuildWarInstance& instance) {
    if (!world_) return;
    
    // Determine winner
    bool attacker_won = instance.attacker_score > instance.defender_score;
    
    // Reward all participants based on contribution
    for (auto player : instance.attacker_participants) {
        auto& guild_comp = world_->GetComponent<GuildComponent>(player);
        guild_comp.total_war_participation++;
        
        // Base reward + contribution bonus
        uint32_t reward = attacker_won ? 1000 : 500;
        reward += guild_comp.war_contribution * 2;
        
        // TODO: Grant actual rewards (gold, honor, items)
        spdlog::debug("Player {} earned {} war points", player, reward);
    }
    
    for (auto player : instance.defender_participants) {
        auto& guild_comp = world_->GetComponent<GuildComponent>(player);
        guild_comp.total_war_participation++;
        
        uint32_t reward = !attacker_won ? 1000 : 500;
        reward += guild_comp.war_contribution * 2;
        
        spdlog::debug("Player {} earned {} war points", player, reward);
    }
}

// [SEQUENCE: 18] Return player from instance
void GuildWarInstancedSystem::ReturnPlayerFromInstance(core::ecs::EntityId player, GuildWarInstance& instance) {
    if (!world_) return;
    
    // Restore original position
    auto pos_it = instance.original_positions.find(player);
    if (pos_it != instance.original_positions.end()) {
        auto& transform = world_->GetComponent<TransformComponent>(player);
        transform.position = pos_it->second;
    }
    
    // Update guild component
    auto& guild_comp = world_->GetComponent<GuildComponent>(player);
    guild_comp.in_guild_war = false;
}

// [SEQUENCE: 19] Query functions
bool GuildWarInstancedSystem::IsGuildAtWar(uint32_t guild_id) const {
    return guild_to_instance_.find(guild_id) != guild_to_instance_.end();
}

uint64_t GuildWarInstancedSystem::GetActiveWarInstance(uint32_t guild_id) const {
    auto it = guild_to_instance_.find(guild_id);
    return (it != guild_to_instance_.end()) ? it->second : 0;
}

// [SEQUENCE: 20] Leave war instance
bool GuildWarInstancedSystem::LeaveWarInstance(core::ecs::EntityId player) {
    auto it = player_to_instance_.find(player);
    if (it == player_to_instance_.end()) {
        return false;
    }
    
    auto instance_it = active_instances_.find(it->second);
    if (instance_it == active_instances_.end()) {
        return false;
    }
    
    auto& instance = *instance_it->second;
    
    // Remove from participants
    instance.attacker_participants.erase(
        std::remove(instance.attacker_participants.begin(),
                   instance.attacker_participants.end(), player),
        instance.attacker_participants.end()
    );
    
    instance.defender_participants.erase(
        std::remove(instance.defender_participants.begin(),
                   instance.defender_participants.end(), player),
        instance.defender_participants.end()
    );
    
    // Return player
    ReturnPlayerFromInstance(player, instance);
    
    // Remove mapping
    player_to_instance_.erase(player);
    
    return true;
}

} // namespace mmorpg::game::systems::guild
