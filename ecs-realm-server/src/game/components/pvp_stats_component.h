#pragma once

#include "core/ecs/types.h"
#include <chrono>

namespace mmorpg::game::components {

// [SEQUENCE: 1] PvP-specific statistics and rankings
struct PvPStatsComponent {
    // [SEQUENCE: 2] Match statistics
    uint32_t matches_played = 0;
    uint32_t matches_won = 0;
    uint32_t matches_lost = 0;
    uint32_t matches_drawn = 0;
    
    // [SEQUENCE: 3] Kill/Death statistics
    uint32_t kills = 0;
    uint32_t deaths = 0;
    uint32_t assists = 0;
    uint32_t killing_sprees = 0;
    uint32_t max_killing_spree = 0;
    
    // [SEQUENCE: 4] Ranking data
    int32_t rating = 1500;              // ELO/MMR rating
    int32_t peak_rating = 1500;         // Highest achieved
    uint32_t rank_points = 0;           // Points in current tier
    uint32_t season_number = 1;         // Current season
    
    // [SEQUENCE: 5] Arena-specific stats
    uint32_t arena_1v1_wins = 0;
    uint32_t arena_2v2_wins = 0;
    uint32_t arena_3v3_wins = 0;
    float average_damage_dealt = 0.0f;
    float average_damage_taken = 0.0f;
    float average_healing_done = 0.0f;
    
    // [SEQUENCE: 6] Open world PvP stats
    uint32_t world_pvp_kills = 0;
    uint32_t honor_points = 0;
    uint32_t territory_captures = 0;
    uint32_t objectives_completed = 0;
    
    // [SEQUENCE: 7] Matchmaking data
    std::chrono::steady_clock::time_point last_match_time;
    std::chrono::steady_clock::time_point queue_start_time;
    bool in_queue = false;
    uint32_t queue_type = 0;  // 1v1, 2v2, etc.
    
    // [SEQUENCE: 8] Behavioral tracking
    uint32_t reports_received = 0;
    uint32_t commendations = 0;
    uint32_t disconnects = 0;
    float reputation_score = 100.0f;    // 0-100
    
    // [SEQUENCE: 9] Rewards tracking
    uint32_t current_streak = 0;        // Win streak
    uint32_t best_streak = 0;
    std::chrono::steady_clock::time_point last_reward_time;
};





} // namespace mmorpg::game::components