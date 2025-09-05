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

// [SEQUENCE: MVP5-11] Implements the main update loop for the system.
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

// [SEQUENCE: MVP5-12] Implements the logic for one guild to declare war on another.
bool GuildWarInstancedSystem::DeclareWar(uint32_t attacker_guild_id, uint32_t defender_guild_id) {
    if (IsGuildAtWar(attacker_guild_id) || IsGuildAtWar(defender_guild_id)) {
        spdlog::warn("Guild {} or {} already at war", attacker_guild_id, defender_guild_id);
        return false;
    }
    
    auto& declarations = pending_declarations_[defender_guild_id];
    auto it = std::find_if(declarations.begin(), declarations.end(),
        [attacker_guild_id](const GuildWarDeclaration& decl) {
            return decl.attacker_guild_id == attacker_guild_id;
        });
    
    if (it != declarations.end()) {
        spdlog::warn("War already declared between {} and {}", attacker_guild_id, defender_guild_id);
        return false;
    }
    
    GuildWarDeclaration declaration;
    declaration.attacker_guild_id = attacker_guild_id;
    declaration.defender_guild_id = defender_guild_id;
    declaration.declaration_time = std::chrono::steady_clock::now();
    declaration.war_start_time = declaration.declaration_time + std::chrono::minutes(5);
    
    declarations.push_back(declaration);
    
    spdlog::info("Guild {} declared war on guild {}", attacker_guild_id, defender_guild_id);
    return true;
}

// [SEQUENCE: MVP5-13] Implements the logic for a guild to accept a war declaration.
bool GuildWarInstancedSystem::AcceptWarDeclaration(uint32_t guild_id) {
    auto it = pending_declarations_.find(guild_id);
    if (it == pending_declarations_.end() || it->second.empty()) {
        return false;
    }
    
    auto& declaration = it->second.front();
    declaration.accepted = true;
    
    auto instance_id = CreateWarInstance(declaration.attacker_guild_id, declaration.defender_guild_id);
    
    it->second.erase(it->second.begin());
    
    return instance_id != 0;
}

// [SEQUENCE: MVP5-14] Creates a new instanced version of a guild war.
uint64_t GuildWarInstancedSystem::CreateWarInstance(uint32_t attacker_guild_id, uint32_t defender_guild_id) {
    auto instance = std::make_unique<GuildWarInstance>();
    instance->instance_id = next_instance_id_++;
    instance->attacker_guild_id = attacker_guild_id;
    instance->defender_guild_id = defender_guild_id;
    instance->start_time = std::chrono::steady_clock::now();
    instance->state = GuildWarInstance::InstanceState::PREPARING;
    
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
    
    guild_to_instance_[attacker_guild_id] = instance->instance_id;
    guild_to_instance_[defender_guild_id] = instance->instance_id;
    
    uint64_t instance_id = instance->instance_id;
    active_instances_[instance_id] = std::move(instance);
    
    stats_.total_wars++;
    stats_.active_wars++;
    
    spdlog::info("Created guild war instance {} between guilds {} and {}", 
                instance_id, attacker_guild_id, defender_guild_id);
    
    return instance_id;
}

// [SEQUENCE: MVP5-15] Implements the logic for a player to join a war instance.
bool GuildWarInstancedSystem::JoinWarInstance(core::ecs::EntityId player, uint64_t instance_id) {
    auto it = active_instances_.find(instance_id);
    if (it == active_instances_.end()) {
        return false;
    }
    
    auto& instance = *it->second;
    
    if (instance.state != GuildWarInstance::InstanceState::PREPARING &&
        instance.state != GuildWarInstance::InstanceState::ACTIVE) {
        return false;
    }
    
    if (!m_world) return false;
    
    auto& guild_comp = m_world->GetComponent<GuildComponent>(player);
    
    bool is_attacker = (guild_comp.guild_id == instance.attacker_guild_id);
    bool is_defender = (guild_comp.guild_id == instance.defender_guild_id);
    
    if (!is_attacker && !is_defender) {
        return false;
    }
    
    if (is_attacker && instance.attacker_participants.size() >= config_.max_participants) {
        return false;
    }
    if (is_defender && instance.defender_participants.size() >= config_.max_participants) {
        return false;
    }
    
    if (is_attacker) {
        instance.attacker_participants.push_back(player);
    } else {
        instance.defender_participants.push_back(player);
    }
    
    player_to_instance_[player] = instance_id;
    
    TeleportPlayerToInstance(player, instance);
    
    guild_comp.in_guild_war = true;
    guild_comp.war_contribution = 0;
    
    spdlog::debug("Player {} joined guild war instance {}", player, instance_id);
    
    if (instance.state == GuildWarInstance::InstanceState::PREPARING) {
        if (instance.attacker_participants.size() >= config_.min_participants &&
            instance.defender_participants.size() >= config_.min_participants) {
            instance.state = GuildWarInstance::InstanceState::ACTIVE;
            spdlog::info("Guild war instance {} is now active", instance_id);
        }
    }
    
    return true;
}

// [SEQUENCE: MVP5-16] Teleports a player to their spawn point within the war instance.
void GuildWarInstancedSystem::TeleportPlayerToInstance(core::ecs::EntityId player, GuildWarInstance& instance) {
    if (!m_world) return;
    
    auto& transform = m_world->GetComponent<TransformComponent>(player);
    
    instance.original_positions[player] = transform.position;
    
    auto& guild_comp = m_world->GetComponent<GuildComponent>(player);
    if (guild_comp.guild_id == instance.attacker_guild_id) {
        transform.position = instance.attacker_spawn;
    } else {
        transform.position = instance.defender_spawn;
    }
    
    transform.position.x += (rand() % 20 - 10);
    transform.position.y += (rand() % 20 - 10);
}

// [SEQUENCE: MVP5-17] Updates all active war instances.
void GuildWarInstancedSystem::UpdateWarInstances(float delta_time) {
    std::vector<uint64_t> instances_to_remove;
    
    for (auto& [instance_id, instance] : active_instances_) {
        UpdateInstanceState(*instance, delta_time);
        
        if (instance->state == GuildWarInstance::InstanceState::CLEANUP) {
            instances_to_remove.push_back(instance_id);
        }
    }
    
    for (auto instance_id : instances_to_remove) {
        auto it = active_instances_.find(instance_id);
        if (it != active_instances_.end()) {
            auto& instance = *it->second;
            
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

// [SEQUENCE: MVP5-18] Updates the state of a single war instance (e.g., timers, victory checks).
void GuildWarInstancedSystem::UpdateInstanceState(GuildWarInstance& instance, float delta_time) {
    switch (instance.state) {
        case GuildWarInstance::InstanceState::PREPARING: {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - instance.start_time).count();
            
            if (elapsed > config_.preparation_time) {
                if (instance.attacker_participants.size() >= config_.min_participants &&
                    instance.defender_participants.size() >= config_.min_participants) {
                    instance.state = GuildWarInstance::InstanceState::ACTIVE;
                } else {
                    EndWarInstance(instance, 0);  // Draw
                }
            }
            break;
        }
        
        case GuildWarInstance::InstanceState::ACTIVE: {
            instance.remaining_time -= delta_time;
            
            UpdateObjectiveCapture(instance, delta_time);
            
            if (CheckVictoryConditions(instance) || instance.remaining_time <= 0) {
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
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - instance.end_time).count();
            
            if (elapsed > 30) {  // 30 seconds to view results
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

// [SEQUENCE: MVP5-19] Updates the capture progress of all objectives in a war.
void GuildWarInstancedSystem::UpdateObjectiveCapture(GuildWarInstance& instance, float delta_time) {
    static float tick_timer = 0.0f;
    tick_timer += delta_time;
    
    if (tick_timer < config_.objective_tick_rate) {
        return;
    }
    tick_timer = 0.0f;
    
    if (!m_world) return;
    
    for (auto& objective : instance.objectives) {
        uint32_t attacker_count = 0;
        uint32_t defender_count = 0;
        
        for (auto player : instance.attacker_participants) {
            auto& transform = m_world->GetComponent<TransformComponent>(player);
            float dx = transform.position.x - objective.position.x;
            float dy = transform.position.y - objective.position.y;
            float dz = transform.position.z - objective.position.z;
            float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (distance < 20.0f) {  // Capture radius
                attacker_count++;
            }
        }
        
        for (auto player : instance.defender_participants) {
            auto& transform = m_world->GetComponent<TransformComponent>(player);
            float dx = transform.position.x - objective.position.x;
            float dy = transform.position.y - objective.position.y;
            float dz = transform.position.z - objective.position.z;
            float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (distance < 20.0f) {
                defender_count++;
            }
        }
        
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
        
        if (objective.controlling_guild == instance.attacker_guild_id) {
            instance.attacker_score += config_.points_per_objective_tick;
        } else if (objective.controlling_guild == instance.defender_guild_id) {
            instance.defender_score += config_.points_per_objective_tick;
        }
    }
}

// [SEQUENCE: MVP5-20] Handles the event of an objective being captured.
void GuildWarInstancedSystem::OnObjectiveCaptured(uint64_t instance_id, uint32_t objective_id, uint32_t guild_id) {
    auto it = active_instances_.find(instance_id);
    if (it == active_instances_.end()) return;
    
    auto& instance = *it->second;
    
    for (auto& objective : instance.objectives) {
        if (objective.objective_id == objective_id) {
            objective.controlling_guild = guild_id;
            
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

// [SEQUENCE: MVP5-21] Handles a player being killed during a war.
void GuildWarInstancedSystem::OnPlayerKilledInWar(core::ecs::EntityId killer, core::ecs::EntityId victim) {
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
    if (!m_world) return;
    
    auto& killer_guild = m_world->GetComponent<GuildComponent>(killer);
    
    if (killer_guild.guild_id == instance.attacker_guild_id) {
        instance.attacker_kills++;
        instance.attacker_score += config_.points_per_kill;
    } else {
        instance.defender_kills++;
        instance.defender_score += config_.points_per_kill;
    }
    
    killer_guild.war_contribution += config_.points_per_kill;
}

// [SEQUENCE: MVP5-22] Checks if the victory conditions for a war have been met.
bool GuildWarInstancedSystem::CheckVictoryConditions(GuildWarInstance& instance) {
    if (instance.attacker_score >= config_.score_limit ||
        instance.defender_score >= config_.score_limit) {
        return true;
    }
    
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

// [SEQUENCE: MVP5-23] Ends a war instance and determines the winner.
void GuildWarInstancedSystem::EndWarInstance(GuildWarInstance& instance, uint32_t winner_guild_id) {
    instance.state = GuildWarInstance::InstanceState::ENDING;
    instance.end_time = std::chrono::steady_clock::now();
    
    GrantWarRewards(instance);
    
    if (winner_guild_id == instance.attacker_guild_id) {
        stats_.guild_victories[instance.attacker_guild_id]++;
    } else if (winner_guild_id == instance.defender_guild_id) {
        stats_.guild_victories[instance.defender_guild_id]++;
    }
    
    spdlog::info("Guild war {} ended. Winner: Guild {}, Score: {} vs {}", 
                instance.instance_id, winner_guild_id,
                instance.attacker_score, instance.defender_score);
}

// [SEQUENCE: MVP5-24] Grants rewards to all participants of a war.
void GuildWarInstancedSystem::GrantWarRewards(const GuildWarInstance& instance) {
    if (!m_world) return;
    
    bool attacker_won = instance.attacker_score > instance.defender_score;
    
    for (auto player : instance.attacker_participants) {
        auto& guild_comp = m_world->GetComponent<GuildComponent>(player);
        guild_comp.total_war_participation++;
        
        uint32_t reward = attacker_won ? 1000 : 500;
        reward += guild_comp.war_contribution * 2;
        
        spdlog::debug("Player {} earned {} war points", player, reward);
    }
    
    for (auto player : instance.defender_participants) {
        auto& guild_comp = m_world->GetComponent<GuildComponent>(player);
        guild_comp.total_war_participation++;
        
        uint32_t reward = !attacker_won ? 1000 : 500;
        reward += guild_comp.war_contribution * 2;
        
        spdlog::debug("Player {} earned {} war points", player, reward);
    }
}

// [SEQUENCE: MVP5-25] Returns a player from a war instance to their original position.
void GuildWarInstancedSystem::ReturnPlayerFromInstance(core::ecs::EntityId player, GuildWarInstance& instance) {
    if (!m_world) return;
    
    auto pos_it = instance.original_positions.find(player);
    if (pos_it != instance.original_positions.end()) {
        auto& transform = m_world->GetComponent<TransformComponent>(player);
        transform.position = pos_it->second;
    }
    
    auto& guild_comp = m_world->GetComponent<GuildComponent>(player);
    guild_comp.in_guild_war = false;
}

// [SEQUENCE: MVP5-26] Checks if a guild is currently at war.
bool GuildWarInstancedSystem::IsGuildAtWar(uint32_t guild_id) const {
    return guild_to_instance_.find(guild_id) != guild_to_instance_.end();
}

// [SEQUENCE: MVP5-27] Gets the active war instance for a guild.
uint64_t GuildWarInstancedSystem::GetActiveWarInstance(uint32_t guild_id) const {
    auto it = guild_to_instance_.find(guild_id);
    return (it != guild_to_instance_.end()) ? it->second : 0;
}

// [SEQUENCE: MVP5-28] Implements the logic for a player to leave a war instance.
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
    
    ReturnPlayerFromInstance(player, instance);
    
    player_to_instance_.erase(player);
    
    return true;
}

} // namespace mmorpg::game::systems::guild
