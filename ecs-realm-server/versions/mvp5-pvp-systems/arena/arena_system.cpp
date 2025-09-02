#include "game/systems/pvp/arena_system.h"
#include "game/components/transform_component.h"
#include "game/components/health_component.h"
#include "game/components/combat_stats_component.h"
#include "core/ecs/world.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

namespace mmorpg::game::systems::pvp {

// [SEQUENCE: 1] Initialize arena system
void ArenaSystem::OnSystemInit() {
    // Initialize arena maps
    arena_maps_[1] = {
        1, "Ruins of Valor", MatchType::ARENA_1V1,
        core::utils::Vector3(-20, 0, 0),  // Team 1 spawn
        core::utils::Vector3(20, 0, 0),   // Team 2 spawn
        30.0f  // Map radius
    };
    
    arena_maps_[2] = {
        2, "Circle of Blood", MatchType::ARENA_2V2,
        core::utils::Vector3(-30, -30, 0),
        core::utils::Vector3(30, 30, 0),
        40.0f
    };
    
    arena_maps_[3] = {
        3, "Grand Colosseum", MatchType::ARENA_3V3,
        core::utils::Vector3(-40, -40, 0),
        core::utils::Vector3(40, 40, 0),
        50.0f
    };
    
    spdlog::info("ArenaSystem initialized with {} maps", arena_maps_.size());
}

// [SEQUENCE: 2] Cleanup
void ArenaSystem::OnSystemShutdown() {
    // End all active matches
    for (auto match : active_matches_) {
        EndMatch(match, 0);  // Draw
    }
    
    matchmaking_queues_.clear();
    player_queue_type_.clear();
    spdlog::info("ArenaSystem shut down");
}

// [SEQUENCE: 3] Main update loop
void ArenaSystem::Update(float delta_time) {
    // Update matchmaking
    UpdateMatchmaking(delta_time);
    
    // Update active matches
    UpdateMatches(delta_time);
}

// [SEQUENCE: 4] Queue for arena
bool ArenaSystem::QueueForArena(core::ecs::EntityId player, MatchType type) {
    // Check if already in queue
    if (player_queue_type_.find(player) != player_queue_type_.end()) {
        spdlog::warn("Player {} already in queue", player);
        return false;
    }
    
    // Check if in active match
    if (player_to_match_.find(player) != player_to_match_.end()) {
        spdlog::warn("Player {} already in match", player);
        return false;
    }
    
    auto* world = GetWorld();
    if (!world) return false;
    
    // Get player rating
    auto* pvp_stats = world->GetComponent<PvPStatsComponent>(player);
    if (!pvp_stats) return false;
    
    // Create queue entry
    QueueEntry entry;
    entry.player = player;
    entry.rating = pvp_stats->rating;
    entry.queue_time = std::chrono::steady_clock::now();
    entry.match_type = type;
    
    // Add to queue
    matchmaking_queues_[type].push_back(entry);
    player_queue_type_[player] = type;
    
    // Update stats
    pvp_stats->in_queue = true;
    pvp_stats->queue_type = static_cast<uint32_t>(type);
    pvp_stats->queue_start_time = entry.queue_time;
    
    stats_.players_in_queue++;
    
    spdlog::info("Player {} queued for {} (rating: {})", 
                player, static_cast<int>(type), entry.rating);
    
    return true;
}

// [SEQUENCE: 5] Leave queue
bool ArenaSystem::LeaveQueue(core::ecs::EntityId player) {
    auto it = player_queue_type_.find(player);
    if (it == player_queue_type_.end()) {
        return false;
    }
    
    MatchType type = it->second;
    player_queue_type_.erase(it);
    
    // Remove from queue
    auto& queue = matchmaking_queues_[type];
    queue.erase(
        std::remove_if(queue.begin(), queue.end(),
            [player](const QueueEntry& entry) {
                return entry.player == player;
            }),
        queue.end()
    );
    
    // Update player stats
    auto* world = GetWorld();
    if (world) {
        auto* pvp_stats = world->GetComponent<PvPStatsComponent>(player);
        if (pvp_stats) {
            pvp_stats->in_queue = false;
        }
    }
    
    stats_.players_in_queue--;
    spdlog::info("Player {} left queue", player);
    
    return true;
}

// [SEQUENCE: 6] Create match
core::ecs::EntityId ArenaSystem::CreateMatch(MatchType type,
                                            const std::vector<core::ecs::EntityId>& team1,
                                            const std::vector<core::ecs::EntityId>& team2) {
    auto* world = GetWorld();
    if (!world) return 0;
    
    // Create match entity
    auto match_entity = world->CreateEntity();
    
    // Add match component
    MatchComponent match;
    match.match_id = static_cast<uint64_t>(match_entity);
    match.match_type = type;
    match.state = MatchState::WAITING_FOR_PLAYERS;
    
    // Setup teams
    TeamInfo team1_info;
    team1_info.team_id = 1;
    team1_info.members = team1;
    match.teams.push_back(team1_info);
    
    TeamInfo team2_info;
    team2_info.team_id = 2;
    team2_info.members = team2;
    match.teams.push_back(team2_info);
    
    // Setup player data
    for (auto player : team1) {
        PlayerMatchData data;
        data.player_id = player;
        data.team_id = 1;
        data.join_time = std::chrono::steady_clock::now();
        match.player_data[player] = data;
        player_to_match_[player] = match_entity;
    }
    
    for (auto player : team2) {
        PlayerMatchData data;
        data.player_id = player;
        data.team_id = 2;
        data.join_time = std::chrono::steady_clock::now();
        match.player_data[player] = data;
        player_to_match_[player] = match_entity;
    }
    
    // Set victory conditions based on type
    switch (type) {
        case MatchType::ARENA_1V1:
            match.kill_limit = 1;  // First to die loses
            match.arena_map_id = 1;
            break;
        case MatchType::ARENA_2V2:
            match.kill_limit = 0;  // Eliminate all enemies
            match.arena_map_id = 2;
            break;
        case MatchType::ARENA_3V3:
            match.kill_limit = 0;
            match.arena_map_id = 3;
            break;
        default:
            break;
    }
    
    world->AddComponent(match_entity, match);
    
    // Add to active matches
    active_matches_.push_back(match_entity);
    stats_.active_matches++;
    stats_.total_matches++;
    
    spdlog::info("Created {} match {} with {} vs {} players",
                static_cast<int>(type), match_entity, team1.size(), team2.size());
    
    return match_entity;
}

// [SEQUENCE: 7] Start match
bool ArenaSystem::StartMatch(core::ecs::EntityId match_entity) {
    auto* world = GetWorld();
    if (!world) return false;
    
    auto* match = world->GetComponent<MatchComponent>(match_entity);
    if (!match || match->state != MatchState::WAITING_FOR_PLAYERS) {
        return false;
    }
    
    // Initialize arena
    InitializeArenaMap(match_entity, match->arena_map_id);
    
    // Teleport players
    TeleportPlayersToArena(match->teams[0].members, 1, match->arena_map_id);
    TeleportPlayersToArena(match->teams[1].members, 2, match->arena_map_id);
    
    // Start countdown
    match->state = MatchState::STARTING;
    match->countdown_remaining = 10.0f;
    
    spdlog::info("Starting match {}", match_entity);
    return true;
}

// [SEQUENCE: 8] Update matchmaking
void ArenaSystem::UpdateMatchmaking(float delta_time) {
    static float matchmaking_timer = 0.0f;
    matchmaking_timer += delta_time;
    
    if (matchmaking_timer < config_.matchmaking_interval) {
        return;
    }
    matchmaking_timer = 0.0f;
    
    // Process each queue type
    for (auto& [type, queue] : matchmaking_queues_) {
        ProcessMatchQueue(type);
    }
}

// [SEQUENCE: 9] Process match queue
void ArenaSystem::ProcessMatchQueue(MatchType type) {
    auto& queue = matchmaking_queues_[type];
    if (queue.empty()) return;
    
    uint32_t team_size = GetTeamSize(type);
    if (queue.size() < team_size * 2) {
        return;  // Not enough players
    }
    
    // Sort by rating
    std::sort(queue.begin(), queue.end(),
        [](const QueueEntry& a, const QueueEntry& b) {
            return a.rating < b.rating;
        });
    
    // Try to create matches
    auto now = std::chrono::steady_clock::now();
    std::vector<QueueEntry> matched_players;
    
    for (size_t i = 0; i + team_size * 2 <= queue.size(); i++) {
        // Calculate rating spread based on queue time
        auto queue_duration = std::chrono::duration_cast<std::chrono::seconds>(
            now - queue[i].queue_time
        ).count();
        
        float allowed_spread = config_.rating_spread + 
            (queue_duration / 30.0f) * config_.rating_spread_growth;
        
        // Check if players within rating range
        bool valid_match = true;
        int32_t min_rating = queue[i].rating;
        int32_t max_rating = queue[i].rating;
        
        for (size_t j = i + 1; j < i + team_size * 2; j++) {
            min_rating = std::min(min_rating, queue[j].rating);
            max_rating = std::max(max_rating, queue[j].rating);
        }
        
        if (max_rating - min_rating > allowed_spread) {
            continue;
        }
        
        // Create match with these players
        std::vector<QueueEntry> match_entries(
            queue.begin() + i, 
            queue.begin() + i + team_size * 2
        );
        
        if (TryCreateMatch(match_entries, type)) {
            // Add to matched list for removal
            matched_players.insert(matched_players.end(),
                                 match_entries.begin(),
                                 match_entries.end());
            i += team_size * 2 - 1;  // Skip matched players
        }
    }
    
    // Remove matched players from queue
    for (const auto& entry : matched_players) {
        player_queue_type_.erase(entry.player);
        stats_.players_in_queue--;
        
        // Update queue time stats
        auto queue_time = std::chrono::duration_cast<std::chrono::seconds>(
            now - entry.queue_time
        ).count();
        stats_.average_queue_time = (stats_.average_queue_time + queue_time) / 2.0f;
    }
    
    queue.erase(
        std::remove_if(queue.begin(), queue.end(),
            [&matched_players](const QueueEntry& entry) {
                return std::find_if(matched_players.begin(), matched_players.end(),
                    [&entry](const QueueEntry& matched) {
                        return matched.player == entry.player;
                    }) != matched_players.end();
            }),
        queue.end()
    );
}

// [SEQUENCE: 10] Try to create match
bool ArenaSystem::TryCreateMatch(const std::vector<QueueEntry>& entries, MatchType type) {
    uint32_t team_size = GetTeamSize(type);
    
    // Split into teams (try to balance by rating)
    std::vector<core::ecs::EntityId> team1, team2;
    
    // Simple alternating assignment for balance
    for (size_t i = 0; i < entries.size(); i++) {
        if (i % 2 == 0) {
            team1.push_back(entries[i].player);
        } else {
            team2.push_back(entries[i].player);
        }
    }
    
    // Create and start match
    auto match_entity = CreateMatch(type, team1, team2);
    if (match_entity != 0) {
        StartMatch(match_entity);
        return true;
    }
    
    return false;
}

// [SEQUENCE: 11] Update active matches
void ArenaSystem::UpdateMatches(float delta_time) {
    auto* world = GetWorld();
    if (!world) return;
    
    std::vector<core::ecs::EntityId> completed_matches;
    
    for (auto match_entity : active_matches_) {
        auto* match = world->GetComponent<MatchComponent>(match_entity);
        if (!match) continue;
        
        switch (match->state) {
            case MatchState::STARTING:
                UpdateMatchCountdown(*match, delta_time);
                break;
                
            case MatchState::IN_PROGRESS:
                UpdateMatchProgress(*match, delta_time);
                CheckVictoryConditions(match_entity, *match);
                break;
                
            case MatchState::ENDING:
                match->state = MatchState::COMPLETED;
                completed_matches.push_back(match_entity);
                break;
                
            default:
                break;
        }
    }
    
    // Clean up completed matches
    for (auto match : completed_matches) {
        active_matches_.erase(
            std::remove(active_matches_.begin(), active_matches_.end(), match),
            active_matches_.end()
        );
        stats_.active_matches--;
        
        // Clean up player mappings
        auto* match_comp = world->GetComponent<MatchComponent>(match);
        if (match_comp) {
            for (const auto& [player, data] : match_comp->player_data) {
                player_to_match_.erase(player);
                RestorePlayerState(player);
            }
        }
        
        world->DestroyEntity(match);
    }
}

// [SEQUENCE: 12] Update match countdown
void ArenaSystem::UpdateMatchCountdown(MatchComponent& match, float delta_time) {
    match.countdown_remaining -= delta_time;
    
    if (match.countdown_remaining <= 0) {
        match.state = MatchState::IN_PROGRESS;
        match.match_start_time = std::chrono::steady_clock::now();
        
        spdlog::info("Match {} started", match.match_id);
    }
}

// [SEQUENCE: 13] Update match progress
void ArenaSystem::UpdateMatchProgress(MatchComponent& match, float delta_time) {
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - match.match_start_time
    ).count();
    
    // Check time limit
    if (elapsed >= match.match_duration) {
        // Determine winner by score/kills
        uint32_t winner = 0;
        if (match.teams[0].kills > match.teams[1].kills) {
            winner = 1;
        } else if (match.teams[1].kills > match.teams[0].kills) {
            winner = 2;
        }
        // If tied, go to overtime or draw
        
        EndMatch(match.match_id, winner);
    }
}

// [SEQUENCE: 14] Check victory conditions
void ArenaSystem::CheckVictoryConditions(core::ecs::EntityId match_entity, MatchComponent& match) {
    auto* world = GetWorld();
    if (!world) return;
    
    // Check if any team is eliminated
    for (size_t i = 0; i < match.teams.size(); i++) {
        bool team_alive = false;
        
        for (auto player : match.teams[i].members) {
            auto* health = world->GetComponent<HealthComponent>(player);
            if (health && !health->is_dead && !match.player_data[player].is_disconnected) {
                team_alive = true;
                break;
            }
        }
        
        if (!team_alive) {
            // Other team wins
            uint32_t winning_team = (i == 0) ? 2 : 1;
            EndMatch(match_entity, winning_team);
            return;
        }
    }
    
    // Check kill limit
    if (match.kill_limit > 0) {
        for (size_t i = 0; i < match.teams.size(); i++) {
            if (match.teams[i].kills >= match.kill_limit) {
                EndMatch(match_entity, match.teams[i].team_id);
                return;
            }
        }
    }
}

// [SEQUENCE: 15] End match
bool ArenaSystem::EndMatch(core::ecs::EntityId match_entity, uint32_t winning_team) {
    auto* world = GetWorld();
    if (!world) return false;
    
    auto* match = world->GetComponent<MatchComponent>(match_entity);
    if (!match || match->state == MatchState::COMPLETED) {
        return false;
    }
    
    match->state = MatchState::ENDING;
    match->match_end_time = std::chrono::steady_clock::now();
    match->winning_team_id = winning_team;
    
    // Calculate match duration
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
        match->match_end_time - match->match_start_time
    ).count();
    stats_.average_match_duration = (stats_.average_match_duration + duration) / 2.0f;
    
    // Apply rating changes
    ApplyRatingChanges(*match);
    
    // Distribute rewards
    DistributeRewards(*match);
    
    spdlog::info("Match {} ended. Winner: Team {}", match_entity, winning_team);
    
    return true;
}

// [SEQUENCE: 16] Calculate rating change (ELO)
int32_t ArenaSystem::CalculateRatingChange(int32_t winner_rating, int32_t loser_rating) {
    // Standard ELO formula
    float expected_score = 1.0f / (1.0f + std::pow(10.0f, (loser_rating - winner_rating) / 400.0f));
    int32_t change = static_cast<int32_t>(config_.k_factor * (1.0f - expected_score));
    
    return std::max(1, change);  // Minimum 1 point change
}

// [SEQUENCE: 17] Apply rating changes
void ArenaSystem::ApplyRatingChanges(const MatchComponent& match) {
    auto* world = GetWorld();
    if (!world) return;
    
    if (match.winning_team_id == 0) {
        return;  // Draw, no rating change
    }
    
    // Calculate average ratings
    int32_t team1_avg_rating = 0;
    int32_t team2_avg_rating = 0;
    
    for (auto player : match.teams[0].members) {
        auto* pvp_stats = world->GetComponent<PvPStatsComponent>(player);
        if (pvp_stats) {
            team1_avg_rating += pvp_stats->rating;
        }
    }
    team1_avg_rating /= match.teams[0].members.size();
    
    for (auto player : match.teams[1].members) {
        auto* pvp_stats = world->GetComponent<PvPStatsComponent>(player);
        if (pvp_stats) {
            team2_avg_rating += pvp_stats->rating;
        }
    }
    team2_avg_rating /= match.teams[1].members.size();
    
    // Calculate rating change
    int32_t rating_change;
    if (match.winning_team_id == 1) {
        rating_change = CalculateRatingChange(team1_avg_rating, team2_avg_rating);
    } else {
        rating_change = CalculateRatingChange(team2_avg_rating, team1_avg_rating);
    }
    
    // Apply changes
    for (size_t i = 0; i < match.teams.size(); i++) {
        bool is_winner = (match.teams[i].team_id == match.winning_team_id);
        
        for (auto player : match.teams[i].members) {
            auto* pvp_stats = world->GetComponent<PvPStatsComponent>(player);
            if (pvp_stats) {
                // Update rating
                if (is_winner) {
                    pvp_stats->rating += rating_change;
                    pvp_stats->matches_won++;
                    pvp_stats->current_streak++;
                    pvp_stats->best_streak = std::max(pvp_stats->best_streak, 
                                                     pvp_stats->current_streak);
                } else {
                    pvp_stats->rating = std::max(1000, pvp_stats->rating - rating_change);
                    pvp_stats->matches_lost++;
                    pvp_stats->current_streak = 0;
                }
                
                pvp_stats->matches_played++;
                pvp_stats->peak_rating = std::max(pvp_stats->peak_rating, pvp_stats->rating);
                
                // Update specific arena stats
                if (is_winner) {
                    switch (match.match_type) {
                        case MatchType::ARENA_1V1:
                            pvp_stats->arena_1v1_wins++;
                            break;
                        case MatchType::ARENA_2V2:
                            pvp_stats->arena_2v2_wins++;
                            break;
                        case MatchType::ARENA_3V3:
                            pvp_stats->arena_3v3_wins++;
                            break;
                        default:
                            break;
                    }
                }
                
                spdlog::debug("Player {} rating: {} -> {} ({}{})",
                            player, pvp_stats->rating - (is_winner ? rating_change : -rating_change),
                            pvp_stats->rating, is_winner ? "+" : "-", rating_change);
            }
        }
    }
}

// [SEQUENCE: 18] Initialize arena map
void ArenaSystem::InitializeArenaMap(core::ecs::EntityId match_entity, uint32_t map_id) {
    // In a real implementation, this would:
    // 1. Create arena instance
    // 2. Load map geometry
    // 3. Set boundaries
    // 4. Disable outside interference
    
    spdlog::debug("Initialized arena map {} for match {}", map_id, match_entity);
}

// [SEQUENCE: 19] Teleport players to arena
void ArenaSystem::TeleportPlayersToArena(const std::vector<core::ecs::EntityId>& players,
                                        uint32_t team_id, uint32_t map_id) {
    auto* world = GetWorld();
    if (!world) return;
    
    auto map_it = arena_maps_.find(map_id);
    if (map_it == arena_maps_.end()) return;
    
    const ArenaMap& map = map_it->second;
    core::utils::Vector3 spawn_pos = (team_id == 1) ? map.team1_spawn : map.team2_spawn;
    
    // Spread players around spawn point
    float angle_step = 2.0f * M_PI / players.size();
    float radius = 5.0f;
    
    for (size_t i = 0; i < players.size(); i++) {
        auto* transform = world->GetComponent<TransformComponent>(players[i]);
        if (transform) {
            // Save original position
            // TODO: Store for restoration after match
            
            // Teleport to arena
            transform->position.x = spawn_pos.x + radius * std::cos(i * angle_step);
            transform->position.y = spawn_pos.y + radius * std::sin(i * angle_step);
            transform->position.z = spawn_pos.z;
        }
        
        // Normalize gear if enabled
        auto* match = world->GetComponent<MatchComponent>(map_id);
        if (match && match->gear_normalized) {
            // TODO: Apply stat templates
        }
        
        // Full heal/resource
        auto* health = world->GetComponent<HealthComponent>(players[i]);
        if (health) {
            health->current_health = health->max_health;
            health->current_mana = health->max_mana;
            health->is_dead = false;
        }
    }
}

// [SEQUENCE: 20] Helper methods
uint32_t ArenaSystem::GetTeamSize(MatchType type) const {
    switch (type) {
        case MatchType::ARENA_1V1: return 1;
        case MatchType::ARENA_2V2: return 2;
        case MatchType::ARENA_3V3: return 3;
        case MatchType::ARENA_5V5: return 5;
        default: return 0;
    }
}

void ArenaSystem::DistributeRewards(const MatchComponent& match) {
    auto* world = GetWorld();
    if (!world) return;
    
    for (size_t i = 0; i < match.teams.size(); i++) {
        bool is_winner = (match.teams[i].team_id == match.winning_team_id);
        uint32_t honor = is_winner ? config_.win_honor : config_.loss_honor;
        uint32_t currency = is_winner ? config_.win_currency : 0;
        
        for (auto player : match.teams[i].members) {
            auto* pvp_stats = world->GetComponent<PvPStatsComponent>(player);
            if (pvp_stats) {
                pvp_stats->honor_points += honor;
                // TODO: Add currency
            }
        }
    }
}

void ArenaSystem::RestorePlayerState(core::ecs::EntityId player) {
    // TODO: Restore original position, remove arena buffs/debuffs
    spdlog::debug("Restored player {} state after arena", player);
}

void ArenaSystem::OnPlayerDeath(core::ecs::EntityId player, core::ecs::EntityId killer) {
    auto match_it = player_to_match_.find(player);
    if (match_it == player_to_match_.end()) return;
    
    auto* world = GetWorld();
    if (!world) return;
    
    auto* match = world->GetComponent<MatchComponent>(match_it->second);
    if (!match || match->state != MatchState::IN_PROGRESS) return;
    
    // Update match stats
    auto& player_data = match->player_data[player];
    player_data.deaths++;
    
    // Update team deaths
    for (auto& team : match->teams) {
        if (std::find(team.members.begin(), team.members.end(), player) != team.members.end()) {
            team.deaths++;
            break;
        }
    }
    
    // Update killer stats
    if (killer != 0) {
        auto killer_it = match->player_data.find(killer);
        if (killer_it != match->player_data.end()) {
            killer_it->second.kills++;
            
            // Update team kills
            for (auto& team : match->teams) {
                if (std::find(team.members.begin(), team.members.end(), killer) != team.members.end()) {
                    team.kills++;
                    break;
                }
            }
        }
    }
}

} // namespace mmorpg::game::systems::pvp