#pragma once

#include "core/ecs/types.h"
#include <vector>
#include <chrono>
#include <unordered_map>

namespace mmorpg::game::components {

// [SEQUENCE: MVP5-18] Match types for different PvP modes
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

// [SEQUENCE: MVP5-20] Team information
struct TeamInfo {
    uint32_t team_id;
    std::vector<core::ecs::EntityId> members;
    uint32_t score = 0;
    uint32_t kills = 0;
    uint32_t deaths = 0;
    bool ready = false;
};

// [SEQUENCE: MVP5-21] Individual player match data
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

// [SEQUENCE: MVP5-22] Component to manage match state and participants
struct MatchComponent {
    // [SEQUENCE: MVP5-23] Match types for different PvP modes
    enum class MatchType {
        NONE,
        DUEL,
        ARENA_1V1,
        ARENA_2V2,
        ARENA_3V3,
        GUILD_WAR
    };

    // [SEQUENCE: MVP5-24] Match state
    enum class State {
        PREPARING,
        IN_PROGRESS,
        FINISHED
    };

    // [SEQUENCE: MVP5-25] Match data
    MatchType type = MatchType::NONE;
    State state = State::PREPARING;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    uint32_t winning_team = 0;

    // [SEQUENCE: MVP5-26] Teams and players
    std::vector<core::ecs::EntityId> team1;
    std::vector<core::ecs::EntityId> team2;

    // [SEQUENCE: MVP5-27] Match objectives
    uint32_t team1_score = 0;
    uint32_t team2_score = 0;

    // [SEQUENCE: MVP5-28] Match configuration
    uint32_t time_limit_seconds = 600; // 10 minutes
    uint32_t score_limit = 5;

    // [SEQUENCE: MVP5-29] Spectator support
    std::vector<core::ecs::EntityId> spectators;
};



} // namespace mmorpg::game::components
   // 0-100%
    std::vector<core::ecs::EntityId> capturing_players;
    
    // [SEQUENCE: MVP5-34] Objectives
    struct Objective {
        uint32_t objective_id;
        core::utils::Vector3 position;
        uint32_t controlling_team = 0;
        float capture_radius = 10.0f;
        uint32_t point_value = 1;
    };
    std::vector<Objective> objectives;
    
    // [SEQUENCE: MVP5-35] Zone bonuses
    float experience_bonus = 1.0f;      // XP multiplier
    float honor_bonus = 1.0f;           // Honor multiplier
    std::vector<uint32_t> buff_ids;     // Active zone buffs
};

} // namespace mmorpg::game::components

} // namespace mmorpg::game::components
