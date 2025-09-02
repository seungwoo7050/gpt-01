#pragma once

#include "core/ecs/system.h"
#include "game/components/pvp_stats_component.h"
#include "game/components/match_component.h"
#include <queue>
#include <memory>

namespace mmorpg::game::systems::pvp {

// [SEQUENCE: 1] Arena-based instanced PvP system
class ArenaSystem : public core::ecs::System {
public:
    ArenaSystem() : System("ArenaSystem") {}
    
    // [SEQUENCE: 2] System lifecycle
    void OnSystemInit() override;
    void OnSystemShutdown() override;
    
    // [SEQUENCE: 3] System update
    void Update(float delta_time) override;
    
    // [SEQUENCE: 4] System metadata
    core::ecs::SystemStage GetStage() const override {
        return core::ecs::SystemStage::UPDATE;
    }
    int GetPriority() const override { return 200; }
    
    // [SEQUENCE: 5] Queue management
    bool QueueForArena(core::ecs::EntityId player, MatchType type);
    bool LeaveQueue(core::ecs::EntityId player);
    bool IsInQueue(core::ecs::EntityId player) const;
    
    // [SEQUENCE: 6] Match management
    core::ecs::EntityId CreateMatch(MatchType type, 
                                   const std::vector<core::ecs::EntityId>& team1,
                                   const std::vector<core::ecs::EntityId>& team2);
    
    bool StartMatch(core::ecs::EntityId match_entity);
    bool EndMatch(core::ecs::EntityId match_entity, uint32_t winning_team);
    
    // [SEQUENCE: 7] Match queries
    core::ecs::EntityId GetPlayerMatch(core::ecs::EntityId player) const;
    std::vector<core::ecs::EntityId> GetActiveMatches() const;
    
    // [SEQUENCE: 8] Spectator management
    bool AddSpectator(core::ecs::EntityId match, core::ecs::EntityId spectator);
    bool RemoveSpectator(core::ecs::EntityId match, core::ecs::EntityId spectator);
    
private:
    // [SEQUENCE: 9] Queue entry for matchmaking
    struct QueueEntry {
        core::ecs::EntityId player;
        int32_t rating;
        std::chrono::steady_clock::time_point queue_time;
        MatchType match_type;
        bool in_group = false;
        std::vector<core::ecs::EntityId> group_members;
    };
    
    // [SEQUENCE: 10] Matchmaking queues
    std::unordered_map<MatchType, std::vector<QueueEntry>> matchmaking_queues_;
    std::unordered_map<core::ecs::EntityId, MatchType> player_queue_type_;
    
    // [SEQUENCE: 11] Active matches
    std::vector<core::ecs::EntityId> active_matches_;
    std::unordered_map<core::ecs::EntityId, core::ecs::EntityId> player_to_match_;
    
    // [SEQUENCE: 12] Match lifecycle
    void UpdateMatchmaking(float delta_time);
    void UpdateMatches(float delta_time);
    void ProcessMatchQueue(MatchType type);
    
    // [SEQUENCE: 13] Match creation
    bool TryCreateMatch(const std::vector<QueueEntry>& entries, MatchType type);
    void InitializeArenaMap(core::ecs::EntityId match_entity, uint32_t map_id);
    void TeleportPlayersToArena(const std::vector<core::ecs::EntityId>& players,
                               uint32_t team_id, uint32_t map_id);
    
    // [SEQUENCE: 14] Match updates
    void UpdateMatchCountdown(MatchComponent& match, float delta_time);
    void UpdateMatchProgress(MatchComponent& match, float delta_time);
    void CheckVictoryConditions(core::ecs::EntityId match_entity, MatchComponent& match);
    
    // [SEQUENCE: 15] Rating calculations
    int32_t CalculateRatingChange(int32_t winner_rating, int32_t loser_rating);
    void ApplyRatingChanges(const MatchComponent& match);
    void DistributeRewards(const MatchComponent& match);
    
    // [SEQUENCE: 16] Player state management
    void OnPlayerDeath(core::ecs::EntityId player, core::ecs::EntityId killer);
    void OnPlayerDisconnect(core::ecs::EntityId player);
    void RestorePlayerState(core::ecs::EntityId player);
    
    // [SEQUENCE: 17] Configuration
    struct ArenaConfig {
        float matchmaking_interval = 5.0f;      // Check queues every 5s
        int32_t rating_spread = 200;            // Initial MMR range
        float max_queue_time = 300.0f;          // 5 minute max wait
        float rating_spread_growth = 50.0f;     // Per 30s in queue
        
        // ELO constants
        float k_factor = 32.0f;                 // Rating volatility
        float placement_k_factor = 64.0f;       // First 10 matches
        
        // Rewards
        uint32_t win_honor = 100;
        uint32_t loss_honor = 25;
        uint32_t win_currency = 50;
    } config_;
    
    // [SEQUENCE: 18] Statistics
    struct ArenaStats {
        uint32_t total_matches = 0;
        uint32_t active_matches = 0;
        uint32_t players_in_queue = 0;
        float average_queue_time = 0.0f;
        float average_match_duration = 0.0f;
    } stats_;
    
    // [SEQUENCE: 19] Arena maps
    struct ArenaMap {
        uint32_t map_id;
        std::string name;
        MatchType supported_types;
        core::utils::Vector3 team1_spawn;
        core::utils::Vector3 team2_spawn;
        float map_radius = 50.0f;
    };
    std::unordered_map<uint32_t, ArenaMap> arena_maps_;
    
    // [SEQUENCE: 20] Helper methods
    bool IsMatchValid(MatchType type, size_t team_size) const;
    uint32_t GetTeamSize(MatchType type) const;
    void BroadcastMatchUpdate(core::ecs::EntityId match_entity);
};

} // namespace mmorpg::game::systems::pvp