#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <functional>
#include <mutex>
#include <atomic>
#include "../core/types.h"
#include "../matchmaking/matchmaking_service.h"
#include "../ranking/ranking_system.h"

namespace mmorpg::arena {

// [SEQUENCE: MVP13-110] Arena match types and configurations
enum class ArenaType {
    ARENA_1V1,      // Solo duel
    ARENA_2V2,      // Small team battle
    ARENA_3V3,      // Standard team arena
    ARENA_5V5,      // Large team arena
    DEATHMATCH,     // Free-for-all
    CUSTOM          // Custom rules
};

// [SEQUENCE: MVP13-111] Arena map types
enum class ArenaMap {
    COLOSSEUM,      // Classic circular arena
    RUINS,          // Ancient ruins with obstacles
    BRIDGE,         // Narrow bridge map
    PILLARS,        // Map with strategic pillars
    MAZE,           // Complex maze layout
    FLOATING,       // Floating platforms
    RANDOM          // Random selection
};

// [SEQUENCE: MVP13-112] Arena match states
enum class ArenaState {
    WAITING_FOR_PLAYERS,
    COUNTDOWN,
    IN_PROGRESS,
    SUDDEN_DEATH,
    FINISHED,
    ABANDONED
};

// [SEQUENCE: MVP13-113] Arena player data
struct ArenaPlayer {
    uint64_t player_id;
    std::string player_name;
    uint32_t team_id;
    
    // Combat stats
    struct CombatStats {
        uint32_t kills{0};
        uint32_t deaths{0};
        uint32_t assists{0};
        uint64_t damage_dealt{0};
        uint64_t damage_taken{0};
        uint64_t healing_done{0};
        uint32_t crowd_control_score{0};
    } stats;
    
    // Status
    bool is_alive{true};
    bool is_connected{true};
    std::chrono::steady_clock::time_point respawn_time;
    
    // Rating info
    int32_t current_rating{1500};
    int32_t rating_change{0};
};

// [SEQUENCE: MVP13-114] Arena match configuration
struct ArenaConfig {
    ArenaType type{ArenaType::ARENA_3V3};
    ArenaMap map{ArenaMap::RANDOM};
    
    // Match settings
    uint32_t time_limit_seconds{600};      // 10 minutes default
    uint32_t score_limit{0};               // 0 = no limit
    uint32_t respawn_time_seconds{5};
    bool allow_consumables{false};
    bool normalize_gear{true};              // Equalize gear stats
    
    // Special rules
    bool sudden_death_enabled{true};
    uint32_t sudden_death_after_seconds{480}; // 8 minutes
    bool healing_reduction_in_sudden_death{true};
    
    // Rewards
    uint32_t winner_honor_points{50};
    uint32_t loser_honor_points{15};
    double winner_xp_multiplier{2.0};
    double loser_xp_multiplier{1.0};
};

// [SEQUENCE: MVP13-115] Arena match instance
class ArenaMatch {
public:
    // [SEQUENCE: MVP13-116] Initialize arena match
    ArenaMatch(uint64_t match_id, const ArenaConfig& config)
        : match_id_(match_id)
        , config_(config)
        , state_(ArenaState::WAITING_FOR_PLAYERS)
        , start_time_(std::chrono::steady_clock::now()) {
        
        // Arena 맵별 특수 효과 초기화
        InitializeMapEffects();
    }
    
    // [SEQUENCE: MVP13-117] Add player to arena
    void AddPlayer(uint64_t player_id, const std::string& name, 
                  uint32_t team_id, int32_t rating) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        ArenaPlayer player;
        player.player_id = player_id;
        player.player_name = name;
        player.team_id = team_id;
        player.current_rating = rating;
        
        players_[player_id] = player;
        team_players_[team_id].push_back(player_id);
        
        spdlog::info("Player {} joined arena match {} on team {}", 
                    name, match_id_, team_id);
    }
    
    // [SEQUENCE: MVP13-118] Start match countdown
    void StartCountdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (state_ != ArenaState::WAITING_FOR_PLAYERS) {
            return;
        }
        
        state_ = ArenaState::COUNTDOWN;
        countdown_start_ = std::chrono::steady_clock::now();
        
        // 플레이어들에게 카운트다운 시작 알림
        BroadcastCountdownStart();
    }
    
    // [SEQUENCE: MVP13-119] Start the match
    void StartMatch() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        state_ = ArenaState::IN_PROGRESS;
        match_start_time_ = std::chrono::steady_clock::now();
        
        // 초기 위치로 플레이어 배치
        TeleportPlayersToStartPositions();
        
        // 매치 시작 이벤트
        OnMatchStart();
        
        spdlog::info("Arena match {} started with {} players", 
                    match_id_, players_.size());
    }
    
    // [SEQUENCE: MVP13-120] Handle player kill
    void HandlePlayerKill(uint64_t killer_id, uint64_t victim_id, 
                         uint64_t assister_id = 0) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto killer_it = players_.find(killer_id);
        auto victim_it = players_.find(victim_id);
        
        if (killer_it == players_.end() || victim_it == players_.end()) {
            return;
        }
        
        // 통계 업데이트
        killer_it->second.stats.kills++;
        victim_it->second.stats.deaths++;
        victim_it->second.is_alive = false;
        
        if (assister_id != 0) {
            auto assister_it = players_.find(assister_id);
            if (assister_it != players_.end()) {
                assister_it->second.stats.assists++;
            }
        }
        
        // 리스폰 타이머 설정
        victim_it->second.respawn_time = std::chrono::steady_clock::now() + 
            std::chrono::seconds(config_.respawn_time_seconds);
        
        // 점수 업데이트
        team_scores_[killer_it->second.team_id]++;
        
        // 킬 알림
        BroadcastKillFeed(killer_id, victim_id, assister_id);
        
        // 승리 조건 체크
        CheckVictoryConditions();
    }
    
    // [SEQUENCE: MVP13-121] Update match state
    void Update() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::steady_clock::now();
        
        switch (state_) {
            case ArenaState::COUNTDOWN: {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    now - countdown_start_).count();
                
                if (elapsed >= COUNTDOWN_DURATION) {
                    StartMatch();
                }
                break;
            }
            
            case ArenaState::IN_PROGRESS: {
                // 리스폰 처리
                ProcessRespawns(now);
                
                // 시간 제한 체크
                auto match_duration = std::chrono::duration_cast<std::chrono::seconds>(
                    now - match_start_time_).count();
                
                if (match_duration >= config_.time_limit_seconds) {
                    EndMatchByTimeout();
                } else if (config_.sudden_death_enabled && 
                          match_duration >= config_.sudden_death_after_seconds &&
                          state_ != ArenaState::SUDDEN_DEATH) {
                    EnterSuddenDeath();
                }
                
                // 맵 특수 효과 업데이트
                UpdateMapEffects(now);
                break;
            }
            
            default:
                break;
        }
    }
    
    // [SEQUENCE: MVP13-122] Get match statistics
    struct MatchStatistics {
        uint32_t total_kills{0};
        uint32_t match_duration_seconds{0};
        uint64_t total_damage{0};
        uint64_t total_healing{0};
        
        struct TeamStats {
            uint32_t team_id;
            uint32_t score;
            std::vector<ArenaPlayer> players;
            bool is_winner;
        };
        std::vector<TeamStats> team_stats;
        
        // MVP calculation
        uint64_t mvp_player_id{0};
        std::string mvp_reason;
    };
    
    MatchStatistics GetMatchStatistics() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        MatchStatistics stats;
        
        // Calculate totals
        for (const auto& [id, player] : players_) {
            stats.total_kills += player.stats.kills;
            stats.total_damage += player.stats.damage_dealt;
            stats.total_healing += player.stats.healing_done;
        }
        
        // Match duration
        if (state_ == ArenaState::FINISHED) {
            stats.match_duration_seconds = 
                std::chrono::duration_cast<std::chrono::seconds>(
                    match_end_time_ - match_start_time_).count();
        }
        
        // Team statistics
        for (const auto& [team_id, score] : team_scores_) {
            MatchStatistics::TeamStats team;
            team.team_id = team_id;
            team.score = score;
            team.is_winner = (team_id == winning_team_id_);
            
            for (uint64_t player_id : team_players_[team_id]) {
                auto it = players_.find(player_id);
                if (it != players_.end()) {
                    team.players.push_back(it->second);
                }
            }
            
            stats.team_stats.push_back(team);
        }
        
        // Calculate MVP
        CalculateMVP(stats);
        
        return stats;
    }
    
    // Getters
    uint64_t GetMatchId() const { return match_id_; }
    ArenaState GetState() const { return state_; }
    const ArenaConfig& GetConfig() const { return config_; }
    
private:
    uint64_t match_id_;
    ArenaConfig config_;
    ArenaState state_;
    
    // Players
    std::unordered_map<uint64_t, ArenaPlayer> players_;
    std::unordered_map<uint32_t, std::vector<uint64_t>> team_players_;
    std::unordered_map<uint32_t, uint32_t> team_scores_;
    
    // Timing
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point countdown_start_;
    std::chrono::steady_clock::time_point match_start_time_;
    std::chrono::steady_clock::time_point match_end_time_;
    
    // Match result
    uint32_t winning_team_id_{0};
    
    mutable std::mutex mutex_;
    
    static constexpr int COUNTDOWN_DURATION = 10; // seconds
    
    // [SEQUENCE: MVP13-123] Initialize map-specific effects
    void InitializeMapEffects() {
        switch (config_.map) {
            case ArenaMap::PILLARS:
                // 기둥이 주기적으로 올라가고 내려감
                map_effects_.push_back(std::make_unique<PillarEffect>());
                break;
                
            case ArenaMap::FLOATING:
                // 플랫폼이 움직이고 사라짐
                map_effects_.push_back(std::make_unique<FloatingPlatformEffect>());
                break;
                
            case ArenaMap::MAZE:
                // 미로 벽이 변경됨
                map_effects_.push_back(std::make_unique<MazeWallEffect>());
                break;
        }
    }
    
    // [SEQUENCE: MVP13-124] Process player respawns
    void ProcessRespawns(std::chrono::steady_clock::time_point now) {
        for (auto& [id, player] : players_) {
            if (!player.is_alive && now >= player.respawn_time) {
                RespawnPlayer(id);
            }
        }
    }
    
    // [SEQUENCE: MVP13-125] Check victory conditions
    void CheckVictoryConditions() {
        // Score limit check
        if (config_.score_limit > 0) {
            for (const auto& [team_id, score] : team_scores_) {
                if (score >= config_.score_limit) {
                    EndMatch(team_id);
                    return;
                }
            }
        }
        
        // Elimination check (모든 적 팀 제거)
        std::unordered_map<uint32_t, bool> team_has_alive;
        for (const auto& [id, player] : players_) {
            if (player.is_alive && player.is_connected) {
                team_has_alive[player.team_id] = true;
            }
        }
        
        if (team_has_alive.size() == 1) {
            EndMatch(team_has_alive.begin()->first);
        }
    }
    
    // [SEQUENCE: MVP13-126] Enter sudden death mode
    void EnterSuddenDeath() {
        state_ = ArenaState::SUDDEN_DEATH;
        
        // Sudden death 효과 적용
        if (config_.healing_reduction_in_sudden_death) {
            // 힐링 50% 감소 디버프 적용
            ApplySuddenDeathDebuffs();
        }
        
        // 맵 축소 또는 위험 지역 생성
        ActivateSuddenDeathMapEffects();
        
        BroadcastSuddenDeathStart();
    }
    
    // [SEQUENCE: MVP13-127] End match
    void EndMatch(uint32_t winning_team) {
        state_ = ArenaState::FINISHED;
        match_end_time_ = std::chrono::steady_clock::now();
        winning_team_id_ = winning_team;
        
        // Calculate rating changes
        CalculateRatingChanges();
        
        // Grant rewards
        DistributeRewards();
        
        // Broadcast results
        BroadcastMatchEnd();
        
        spdlog::info("Arena match {} ended. Winner: Team {}", 
                    match_id_, winning_team);
    }
    
    // [SEQUENCE: MVP13-128] Calculate rating changes
    void CalculateRatingChanges() {
        // 팀별 평균 레이팅 계산
        std::unordered_map<uint32_t, int32_t> team_avg_ratings;
        std::unordered_map<uint32_t, uint32_t> team_player_counts;
        
        for (const auto& [id, player] : players_) {
            team_avg_ratings[player.team_id] += player.current_rating;
            team_player_counts[player.team_id]++;
        }
        
        for (auto& [team_id, total_rating] : team_avg_ratings) {
            team_avg_ratings[team_id] = total_rating / team_player_counts[team_id];
        }
        
        // ELO 기반 레이팅 변경 계산
        for (auto& [id, player] : players_) {
            bool won = (player.team_id == winning_team_id_);
            int32_t opponent_avg = 0;
            
            // 상대팀 평균 레이팅 찾기
            for (const auto& [team_id, avg_rating] : team_avg_ratings) {
                if (team_id != player.team_id) {
                    opponent_avg = avg_rating;
                    break;
                }
            }
            
            // ELO calculation
            double expected = 1.0 / (1.0 + std::pow(10.0, 
                (opponent_avg - player.current_rating) / 400.0));
            double actual = won ? 1.0 : 0.0;
            int32_t k_factor = 32;
            
            player.rating_change = static_cast<int32_t>(
                k_factor * (actual - expected));
        }
    }
    
    // [SEQUENCE: MVP13-129] Calculate MVP
    void CalculateMVP(MatchStatistics& stats) const {
        uint64_t mvp_id = 0;
        double best_score = 0;
        
        for (const auto& [id, player] : players_) {
            // MVP 점수 계산 (kills + assists*0.5 - deaths + damage/1000 + healing/2000)
            double score = player.stats.kills + 
                          player.stats.assists * 0.5 - 
                          player.stats.deaths +
                          player.stats.damage_dealt / 1000.0 +
                          player.stats.healing_done / 2000.0 +
                          player.stats.crowd_control_score / 100.0;
            
            if (score > best_score) {
                best_score = score;
                mvp_id = id;
            }
        }
        
        stats.mvp_player_id = mvp_id;
        
        // MVP 이유 결정
        if (mvp_id != 0) {
            const auto& mvp = players_.at(mvp_id);
            if (mvp.stats.kills >= mvp.stats.assists && 
                mvp.stats.kills >= mvp.stats.healing_done / 1000) {
                stats.mvp_reason = "Most Kills";
            } else if (mvp.stats.healing_done > mvp.stats.damage_dealt) {
                stats.mvp_reason = "Top Healer";
            } else {
                stats.mvp_reason = "Best Overall Performance";
            }
        }
    }
    
    // Helper functions
    void BroadcastCountdownStart() {}
    void TeleportPlayersToStartPositions() {}
    void OnMatchStart() {}
    void BroadcastKillFeed(uint64_t killer, uint64_t victim, uint64_t assist) {}
    void EndMatchByTimeout() {}
    void UpdateMapEffects(std::chrono::steady_clock::time_point now) {}
    void RespawnPlayer(uint64_t player_id) {}
    void ApplySuddenDeathDebuffs() {}
    void ActivateSuddenDeathMapEffects() {}
    void BroadcastSuddenDeathStart() {}
    void DistributeRewards() {}
    void BroadcastMatchEnd() {}
    
    // Map effects
    struct MapEffect {
        virtual ~MapEffect() = default;
        virtual void Update(std::chrono::steady_clock::time_point now) = 0;
    };
    
    struct PillarEffect : MapEffect {
        void Update(std::chrono::steady_clock::time_point now) override {}
    };
    
    struct FloatingPlatformEffect : MapEffect {
        void Update(std::chrono::steady_clock::time_point now) override {}
    };
    
    struct MazeWallEffect : MapEffect {
        void Update(std::chrono::steady_clock::time_point now) override {}
    };
    
    std::vector<std::unique_ptr<MapEffect>> map_effects_;
};

// [SEQUENCE: MVP13-130] Arena system service
class ArenaSystem {
public:
    // [SEQUENCE: MVP13-131] Create arena match
    uint64_t CreateArenaMatch(const ArenaConfig& config) {
        uint64_t match_id = next_match_id_++;
        
        auto match = std::make_shared<ArenaMatch>(match_id, config);
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            active_matches_[match_id] = match;
        }
        
        spdlog::info("Created arena match {} with type {}", 
                    match_id, static_cast<int>(config.type));
        
        return match_id;
    }
    
    // [SEQUENCE: MVP13-132] Queue for arena
    void QueueForArena(uint64_t player_id, ArenaType type, int32_t rating) {
        MatchmakingProfile profile;
        profile.player_id = player_id;
        profile.rating_info.current_rating = rating;
        profile.preferences.preferred_mode = GetMatchmakingCategory(type);
        
        // 매치메이킹 서비스에 등록
        matchmaking_service_->QueuePlayer(profile);
        
        spdlog::info("Player {} queued for {} arena with rating {}", 
                    player_id, GetArenaTypeName(type), rating);
    }
    
    // [SEQUENCE: MVP13-133] Get active matches
    std::vector<std::shared_ptr<ArenaMatch>> GetActiveMatches() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<std::shared_ptr<ArenaMatch>> matches;
        for (const auto& [id, match] : active_matches_) {
            if (match->GetState() == ArenaState::IN_PROGRESS) {
                matches.push_back(match);
            }
        }
        
        return matches;
    }
    
    // [SEQUENCE: MVP13-134] Get match by ID
    std::shared_ptr<ArenaMatch> GetMatch(uint64_t match_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = active_matches_.find(match_id);
        if (it != active_matches_.end()) {
            return it->second;
        }
        
        return nullptr;
    }
    
    // [SEQUENCE: MVP13-135] Update all active matches
    void Update() {
        std::vector<std::shared_ptr<ArenaMatch>> matches_to_update;
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& [id, match] : active_matches_) {
                matches_to_update.push_back(match);
            }
        }
        
        // Update outside of lock
        for (auto& match : matches_to_update) {
            match->Update();
            
            // Clean up finished matches
            if (match->GetState() == ArenaState::FINISHED) {
                auto now = std::chrono::steady_clock::now();
                if (std::chrono::duration_cast<std::chrono::minutes>(
                    now - match->GetMatchStatistics().match_duration_seconds).count() > 5) {
                    
                    std::lock_guard<std::mutex> lock(mutex_);
                    active_matches_.erase(match->GetMatchId());
                }
            }
        }
    }
    
    // [SEQUENCE: MVP13-136] Get arena statistics
    struct ArenaStatistics {
        uint32_t total_matches_today{0};
        uint32_t active_matches{0};
        uint32_t players_in_queue{0};
        
        std::unordered_map<ArenaType, uint32_t> matches_by_type;
        std::unordered_map<ArenaMap, uint32_t> popular_maps;
        
        double average_match_duration_seconds{0};
        double average_queue_time_seconds{0};
    };
    
    ArenaStatistics GetStatistics() const {
        ArenaStatistics stats;
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            stats.active_matches = active_matches_.size();
            
            for (const auto& [id, match] : active_matches_) {
                stats.matches_by_type[match->GetConfig().type]++;
                
                if (match->GetState() == ArenaState::IN_PROGRESS) {
                    // Additional statistics gathering
                }
            }
        }
        
        // Queue statistics from matchmaking service
        stats.players_in_queue = matchmaking_service_->GetQueueSize(
            MatchmakingCategory::ARENA_3V3);
        
        return stats;
    }
    
    // Set services
    void SetMatchmakingService(MatchmakingService* service) {
        matchmaking_service_ = service;
    }
    
    void SetRankingService(RankingService* service) {
        ranking_service_ = service;
    }
    
private:
    std::unordered_map<uint64_t, std::shared_ptr<ArenaMatch>> active_matches_;
    std::atomic<uint64_t> next_match_id_{1};
    
    MatchmakingService* matchmaking_service_{nullptr};
    RankingService* ranking_service_{nullptr};
    
    mutable std::mutex mutex_;
    
    // Helper functions
    MatchmakingCategory GetMatchmakingCategory(ArenaType type) {
        switch (type) {
            case ArenaType::ARENA_1V1: return MatchmakingCategory::ARENA_1V1;
            case ArenaType::ARENA_2V2: return MatchmakingCategory::ARENA_2V2;
            case ArenaType::ARENA_3V3: return MatchmakingCategory::ARENA_3V3;
            case ArenaType::ARENA_5V5: return MatchmakingCategory::ARENA_5V5;
            default: return MatchmakingCategory::ARENA_3V3;
        }
    }
    
    std::string GetArenaTypeName(ArenaType type) {
        switch (type) {
            case ArenaType::ARENA_1V1: return "1v1";
            case ArenaType::ARENA_2V2: return "2v2";
            case ArenaType::ARENA_3V3: return "3v3";
            case ArenaType::ARENA_5V5: return "5v5";
            case ArenaType::DEATHMATCH: return "Deathmatch";
            case ArenaType::CUSTOM: return "Custom";
            default: return "Unknown";
        }
    }
};

} // namespace mmorpg::arena