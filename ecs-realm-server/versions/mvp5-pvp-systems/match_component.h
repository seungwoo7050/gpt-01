#pragma once

#include "core/ecs/types.h"
#include <vector>
#include <chrono>
#include <unordered_map>

namespace mmorpg::game::components {

// [SEQUENCE: 1] Match types for different PvP modes
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

// [SEQUENCE: 2] Match states
enum class MatchState {
    WAITING_FOR_PLAYERS,
    STARTING,          // Countdown phase
    IN_PROGRESS,
    OVERTIME,
    ENDING,
    COMPLETED
};

// [SEQUENCE: 3] Team information
struct TeamInfo {
    uint32_t team_id;
    std::vector<core::ecs::EntityId> members;
    uint32_t score = 0;
    uint32_t kills = 0;
    uint32_t deaths = 0;
    bool ready = false;
};

// [SEQUENCE: 4] Individual player match data
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

// [SEQUENCE: 5] Match component for tracking PvP matches
struct MatchComponent {
    // [SEQUENCE: 6] Match identification
    uint64_t match_id = 0;
    MatchType match_type = MatchType::ARENA_1V1;
    MatchState state = MatchState::WAITING_FOR_PLAYERS;
    
    // [SEQUENCE: 7] Teams
    std::vector<TeamInfo> teams;
    std::unordered_map<core::ecs::EntityId, PlayerMatchData> player_data;
    
    // [SEQUENCE: 8] Match timing
    std::chrono::steady_clock::time_point match_start_time;
    std::chrono::steady_clock::time_point match_end_time;
    float match_duration = 300.0f;      // 5 minutes default
    float overtime_duration = 60.0f;    // 1 minute overtime
    float countdown_remaining = 10.0f;  // Pre-match countdown
    
    // [SEQUENCE: 9] Victory conditions
    uint32_t score_limit = 0;           // 0 = no limit
    uint32_t kill_limit = 0;            // 0 = no limit
    bool sudden_death = false;          // First kill wins
    
    // [SEQUENCE: 10] Arena-specific settings
    bool gear_normalized = true;        // Equalize gear
    bool consumables_allowed = false;   // Potions, etc.
    uint32_t arena_map_id = 1;         // Which arena
    
    // [SEQUENCE: 11] Match results
    uint32_t winning_team_id = 0;
    std::vector<core::ecs::EntityId> mvp_players;
    std::unordered_map<core::ecs::EntityId, int32_t> rating_changes;
    
    // [SEQUENCE: 12] Spectator support
    std::vector<core::ecs::EntityId> spectators;
    bool allow_spectators = true;
    float spectator_delay = 2.0f;       // Seconds
};

// [SEQUENCE: 13] PvP zone component for open world
struct PvPZoneComponent {
    // [SEQUENCE: 14] Zone identification
    uint32_t zone_id;
    std::string zone_name;
    
    // [SEQUENCE: 15] PvP rules
    bool pvp_enabled = true;
    bool faction_based = true;          // Faction vs faction
    bool free_for_all = false;          // Everyone vs everyone
    uint32_t min_level = 10;            // Level requirement
    
    // [SEQUENCE: 16] Territory control
    uint32_t controlling_faction = 0;   // 0 = neutral
    float capture_progress = 0.0f;      // 0-100%
    std::vector<core::ecs::EntityId> capturing_players;
    
    // [SEQUENCE: 17] Objectives
    struct Objective {
        uint32_t objective_id;
        core::utils::Vector3 position;
        uint32_t controlling_team = 0;
        float capture_radius = 10.0f;
        uint32_t point_value = 1;
    };
    std::vector<Objective> objectives;
    
    // [SEQUENCE: 18] Zone bonuses
    float experience_bonus = 1.0f;      // XP multiplier
    float honor_bonus = 1.0f;           // Honor multiplier
    std::vector<uint32_t> buff_ids;     // Active zone buffs
};

} // namespace mmorpg::game::components