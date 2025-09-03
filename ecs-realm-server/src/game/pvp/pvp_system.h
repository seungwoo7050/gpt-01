#pragma once

#include <atomic>
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <chrono>
#include <queue>
#include <optional>

namespace mmorpg::game::pvp {

// [SEQUENCE: 1241] PvP system
// 플레이어 간 전투, 결투, 아레나, 전장을 관리하는 시스템

// [SEQUENCE: 1242] PvP types
enum class PvPType {
    DUEL,               // 1v1 결투
    ARENA_2V2,          // 2v2 아레나
    ARENA_3V3,          // 3v3 아레나
    ARENA_5V5,          // 5v5 아레나
    BATTLEGROUND_10V10, // 10v10 전장
    BATTLEGROUND_20V20, // 20v20 전장
    WORLD_PVP,          // 오픈 월드 PvP
    GUILD_WAR           // 길드전
};

// [SEQUENCE: 1243] PvP states
enum class PvPState {
    NONE,               // PvP 비활성
    QUEUED,             // 매칭 대기 중
    PREPARATION,        // 준비 단계
    IN_PROGRESS,        // 진행 중
    ENDING,             // 종료 처리 중
    COMPLETED           // 완료
};

// [SEQUENCE: 1244] PvP zone types
enum class PvPZoneType {
    SAFE_ZONE,          // PvP 불가 지역
    CONTESTED,          // PvP 가능 지역
    HOSTILE,            // 적대적 지역 (상대 진영)
    ARENA,              // 아레나
    BATTLEGROUND,       // 전장
    DUEL_ZONE           // 결투 가능 지역
};

// [SEQUENCE: 1245] PvP match info
struct PvPMatchInfo {
    uint64_t match_id;
    PvPType type;
    PvPState state = PvPState::NONE;
    
    // Teams
    std::vector<uint64_t> team_a;
    std::vector<uint64_t> team_b;
    
    // Timing
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    uint32_t duration_seconds = 900;    // 15 minutes default
    
    // Score
    int32_t team_a_score = 0;
    int32_t team_b_score = 0;
    
    // Win conditions
    int32_t score_limit = 0;            // 0 = no limit
    int32_t kill_limit = 0;             // 0 = no limit
    
    // Map info
    uint32_t map_id = 0;
    std::string map_name;
};

// [SEQUENCE: 1246] Player PvP stats
struct PlayerPvPStats {
    // Overall stats
    uint32_t total_kills = 0;
    uint32_t total_deaths = 0;
    uint32_t total_assists = 0;
    uint32_t killing_blows = 0;
    
    // Match stats
    uint32_t matches_played = 0;
    uint32_t matches_won = 0;
    uint32_t matches_lost = 0;
    uint32_t matches_draw = 0;
    
    // Rating
    int32_t rating = 1500;              // ELO-style rating
    int32_t highest_rating = 1500;
    
    // Honor/Currency
    uint64_t honor_points = 0;
    uint64_t conquest_points = 0;
    
    // Streaks
    uint32_t current_win_streak = 0;
    uint32_t best_win_streak = 0;
    uint32_t current_kill_streak = 0;
    uint32_t best_kill_streak = 0;
    
    // Per match type stats
    std::unordered_map<PvPType, uint32_t> wins_by_type;
    std::unordered_map<PvPType, uint32_t> losses_by_type;
};

// [SEQUENCE: 1247] PvP reward
struct PvPReward {
    uint64_t experience = 0;
    uint64_t honor_points = 0;
    uint64_t conquest_points = 0;
    std::vector<uint32_t> item_ids;
    uint64_t currency = 0;
};

// [SEQUENCE: 1248] Duel request
struct DuelRequest {
    uint64_t challenger_id;
    uint64_t target_id;
    std::chrono::steady_clock::time_point request_time;
    uint32_t timeout_seconds = 30;
};

// [SEQUENCE: 1249] PvP controller (per player)
class PvPController {
public:
    PvPController(uint64_t entity_id) : entity_id_(entity_id) {}
    
    // [SEQUENCE: 1250] PvP state management
    PvPState GetState() const { return current_state_; }
    void SetState(PvPState state) { current_state_ = state; }
    
    // [SEQUENCE: 1251] Match management
    void SetCurrentMatch(uint64_t match_id) { current_match_id_ = match_id; }
    uint64_t GetCurrentMatch() const { return current_match_id_; }
    
    // [SEQUENCE: 1252] Stats tracking
    PlayerPvPStats& GetStats() { return stats_; }
    const PlayerPvPStats& GetStats() const { return stats_; }
    
    // [SEQUENCE: 1253] Kill tracking
    void RecordKill(uint64_t victim_id);
    void RecordDeath(uint64_t killer_id);
    void RecordAssist(uint64_t victim_id);
    
    // [SEQUENCE: 1254] PvP flags
    bool IsPvPEnabled() const { return pvp_enabled_; }
    void SetPvPEnabled(bool enabled) { pvp_enabled_ = enabled; }
    bool IsInCombat() const { return in_pvp_combat_; }
    void SetInCombat(bool in_combat) { in_pvp_combat_ = in_combat; }
    
    // [SEQUENCE: 1255] Zone tracking
    void SetCurrentZone(PvPZoneType zone) { current_zone_ = zone; }
    PvPZoneType GetCurrentZone() const { return current_zone_; }
    
private:
    uint64_t entity_id_;
    PvPState current_state_ = PvPState::NONE;
    uint64_t current_match_id_ = 0;
    
    // Stats
    PlayerPvPStats stats_;
    
    // Flags
    bool pvp_enabled_ = false;
    bool in_pvp_combat_ = false;
    
    // Zone
    PvPZoneType current_zone_ = PvPZoneType::SAFE_ZONE;
    
    // Combat log
    std::vector<uint64_t> recent_kills_;
    std::vector<uint64_t> recent_deaths_;
    std::chrono::steady_clock::time_point last_pvp_action_;
};

// [SEQUENCE: 1256] Matchmaking queue
class MatchmakingQueue {
public:
    MatchmakingQueue(PvPType type) : pvp_type_(type) {}
    
    // [SEQUENCE: 1257] Queue management
    void AddPlayer(uint64_t player_id, int32_t rating);
    void RemovePlayer(uint64_t player_id);
    bool IsPlayerQueued(uint64_t player_id) const;
    
    // [SEQUENCE: 1258] Match creation
    std::optional<PvPMatchInfo> TryCreateMatch();
    
    // [SEQUENCE: 1259] Queue info
    size_t GetQueueSize() const { return queued_players_.size(); }
    float GetAverageWaitTime() const;
    
private:
    PvPType pvp_type_;
    
    struct QueuedPlayer {
        uint64_t player_id;
        int32_t rating;
        std::chrono::steady_clock::time_point queue_time;
    };
    
    std::vector<QueuedPlayer> queued_players_;
    
    // [SEQUENCE: 1260] Rating-based matching
    bool ArePlayersCompatible(const QueuedPlayer& p1, const QueuedPlayer& p2) const;
    int32_t GetRatingDifference(const QueuedPlayer& p1, const QueuedPlayer& p2) const;
};

// [SEQUENCE: 1261] PvP manager (global)
class PvPManager {
public:
    static PvPManager& Instance() {
        static PvPManager instance;
        return instance;
    }
    
    // [SEQUENCE: 1262] Controller management
    std::shared_ptr<PvPController> CreateController(uint64_t entity_id);
    std::shared_ptr<PvPController> GetController(uint64_t entity_id) const;
    void RemoveController(uint64_t entity_id);
    
    // [SEQUENCE: 1263] Duel system
    bool SendDuelRequest(uint64_t challenger_id, uint64_t target_id);
    bool AcceptDuel(uint64_t target_id, uint64_t challenger_id);
    bool DeclineDuel(uint64_t target_id, uint64_t challenger_id);
    void StartDuel(uint64_t player1_id, uint64_t player2_id);
    void EndDuel(uint64_t winner_id, uint64_t loser_id);
    
    // [SEQUENCE: 1264] Matchmaking
    bool QueueForPvP(uint64_t player_id, PvPType type);
    bool LeaveQueue(uint64_t player_id);
    void UpdateMatchmaking();
    
    // [SEQUENCE: 1265] Match management
    uint64_t CreateMatch(const PvPMatchInfo& info);
    PvPMatchInfo* GetMatch(uint64_t match_id);
    void StartMatch(uint64_t match_id);
    void EndMatch(uint64_t match_id);
    
    // [SEQUENCE: 1266] PvP validation
    bool CanAttack(uint64_t attacker_id, uint64_t target_id) const;
    bool IsAlly(uint64_t player1_id, uint64_t player2_id) const;
    bool IsEnemy(uint64_t player1_id, uint64_t player2_id) const;
    
    // [SEQUENCE: 1267] Zone management
    void SetZonePvPType(uint32_t zone_id, PvPZoneType type);
    PvPZoneType GetZonePvPType(uint32_t zone_id) const;
    
    // [SEQUENCE: 1268] Rewards
    PvPReward CalculateRewards(uint64_t player_id, uint64_t match_id, bool won);
    void GrantRewards(uint64_t player_id, const PvPReward& rewards);
    
    // [SEQUENCE: 1269] Rating calculation
    int32_t CalculateRatingChange(int32_t winner_rating, int32_t loser_rating);
    void UpdateRatings(uint64_t winner_id, uint64_t loser_id);
    
    // [SEQUENCE: 1270] Update
    void Update(float delta_time);
    
private:
    PvPManager() {
        InitializeQueues();
    }
    
    // Controllers
    std::unordered_map<uint64_t, std::shared_ptr<PvPController>> controllers_;
    
    // Active matches
    std::unordered_map<uint64_t, PvPMatchInfo> active_matches_;
    std::atomic<uint64_t> next_match_id_{1};
    
    // Duel requests
    std::vector<DuelRequest> pending_duels_;
    
    // Matchmaking queues
    std::unordered_map<PvPType, std::unique_ptr<MatchmakingQueue>> queues_;
    
    // Zone PvP types
    std::unordered_map<uint32_t, PvPZoneType> zone_pvp_types_;
    
    // [SEQUENCE: 1271] Initialize queues
    void InitializeQueues();
    
    // [SEQUENCE: 1272] Process expired duels
    void ProcessExpiredDuels();
    
    // [SEQUENCE: 1273] Update match states
    void UpdateMatches(float delta_time);
};

// [SEQUENCE: 1274] PvP utilities
class PvPUtilities {
public:
    // Rating tiers
    static std::string GetRatingTier(int32_t rating) {
        if (rating < 1000) return "Bronze";
        if (rating < 1250) return "Silver";
        if (rating < 1500) return "Gold";
        if (rating < 1750) return "Platinum";
        if (rating < 2000) return "Diamond";
        if (rating < 2250) return "Master";
        if (rating < 2500) return "Grandmaster";
        return "Challenger";
    }
    
    // Kill/Death ratio
    static float GetKDRatio(const PlayerPvPStats& stats) {
        if (stats.total_deaths == 0) {
            return static_cast<float>(stats.total_kills);
        }
        return static_cast<float>(stats.total_kills) / stats.total_deaths;
    }
    
    // Win rate
    static float GetWinRate(const PlayerPvPStats& stats) {
        if (stats.matches_played == 0) {
            return 0.0f;
        }
        return static_cast<float>(stats.matches_won) / stats.matches_played;
    }
};

// [SEQUENCE: 1275] PvP event types
enum class PvPEventType {
    KILL,
    DEATH,
    ASSIST,
    MATCH_START,
    MATCH_END,
    OBJECTIVE_CAPTURED,
    FLAG_CAPTURED,
    FLAG_RETURNED
};

// [SEQUENCE: 1276] PvP event
struct PvPEvent {
    PvPEventType type;
    uint64_t source_player_id;
    uint64_t target_player_id = 0;
    uint64_t match_id;
    std::chrono::steady_clock::time_point timestamp;
    std::unordered_map<std::string, std::string> data;
};

} // namespace mmorpg::game::pvp