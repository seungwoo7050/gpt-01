#pragma once

#include "core/ecs/types.h"
#include <vector>
#include <chrono>
#include <unordered_map>

namespace mmorpg::game::components {

enum class MatchType {
    ARENA_1V1,
    ARENA_2V2,
    ARENA_3V3,
    ARENA_5V5,
    BATTLEGROUND_10V10,
    BATTLEGROUND_20V20,
    WORLD_PVP_SKIRMISH,
    DUEL,
    TOURNAMENT
};

enum class MatchState {
    WAITING_FOR_PLAYERS,
    STARTING,          // Countdown phase
    IN_PROGRESS,
    OVERTIME,
    ENDING,
    COMPLETED
};

struct TeamInfo {
    uint32_t team_id;
    std::vector<core::ecs::EntityId> members;
    uint32_t score = 0;
    uint32_t kills = 0;
    uint32_t deaths = 0;
    bool ready = false;
};

struct PlayerMatchData {
    core::ecs::EntityId player_id;
    uint32_t team_id;
    uint32_t kills = 0;
    uint32_t deaths = 0;
    uint32_t assists = 0;
    float damage_dealt = 0.0f;
    float damage_taken = 0.0f;
    float healing_done = 0.0f;
    std::chrono::steady_clock::time_point join_time;
    bool is_disconnected = false;
};

// [SEQUENCE: MVP5-3] Defines the data for a PvP match instance.
struct MatchComponent {
    // Match identification
    uint64_t match_id = 0;
    MatchType match_type = MatchType::ARENA_1V1;
    MatchState state = MatchState::WAITING_FOR_PLAYERS;
    
    // Teams
    std::vector<TeamInfo> teams;
    std::unordered_map<core::ecs::EntityId, PlayerMatchData> player_data;
    
    // Match timing
    std::chrono::steady_clock::time_point match_start_time;
    std::chrono::steady_clock::time_point match_end_time;
    float match_duration = 300.0f;      // 5 minutes default
    float overtime_duration = 60.0f;    // 1 minute overtime
    float countdown_remaining = 10.0f;  // Pre-match countdown
    
    // Victory conditions
    uint32_t score_limit = 0;           // 0 = no limit
    uint32_t kill_limit = 0;            // 0 = no limit
    bool sudden_death = false;          // First kill wins
    
    // Arena-specific settings
    bool gear_normalized = true;        // Equalize gear
    bool consumables_allowed = false;   // Potions, etc.
    uint32_t arena_map_id = 1;         // Which arena
    
    // Match results
    uint32_t winning_team_id = 0;
    std::vector<core::ecs::EntityId> mvp_players;
    std::unordered_map<core::ecs::EntityId, int32_t> rating_changes;
    
    // Spectator support
    std::vector<core::ecs::EntityId> spectators;
    bool allow_spectators = true;
    float spectator_delay = 2.0f;       // Seconds
};

} // namespace mmorpg::game::components
