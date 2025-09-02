#pragma once

#include <string>
#include <vector>
#include <queue>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <mutex>
#include <atomic>
#include <functional>
#include <optional>
#include <deque>
#include <algorithm>
#include <spdlog/spdlog.h>

namespace mmorpg::matchmaking {

// [SEQUENCE: MVP13-1] Matchmaking service for competitive game modes
// 매치메이킹 서비스 - 경쟁 게임 모드를 위한 플레이어 매칭

// [SEQUENCE: MVP13-2] Match types and modes
enum class MatchType {
    ARENA_1V1,          // 1대1 아레나
    ARENA_2V2,          // 2대2 아레나
    ARENA_3V3,          // 3대3 아레나
    ARENA_5V5,          // 5대5 아레나
    BATTLEGROUND_10V10, // 10대10 전장
    BATTLEGROUND_20V20, // 20대20 전장
    RANKED_SOLO,        // 랭크 솔로
    RANKED_TEAM,        // 랭크 팀
    TOURNAMENT,         // 토너먼트
    CUSTOM              // 커스텀 매치
};

// [SEQUENCE: MVP13-3] Player matchmaking profile
struct MatchmakingProfile {
    uint64_t player_id;
    std::string player_name;
    
    // [SEQUENCE: MVP13-4] Rating information
    struct RatingInfo {
        int32_t current_rating{1500};      // 현재 레이팅 (ELO)
        int32_t peak_rating{1500};         // 최고 레이팅
        double rating_deviation{350.0};     // 레이팅 편차 (불확실성)
        uint32_t matches_played{0};        // 플레이한 매치 수
        uint32_t wins{0};                  // 승리 횟수
        uint32_t losses{0};                // 패배 횟수
        
        double GetWinRate() const {
            uint32_t total = wins + losses;
            return total > 0 ? static_cast<double>(wins) / total : 0.5;
        }
    };
    
    std::unordered_map<MatchType, RatingInfo> ratings; // 매치 타입별 레이팅
    
    // [SEQUENCE: MVP13-5] Player preferences
    struct Preferences {
        std::vector<MatchType> preferred_modes;     // 선호 게임 모드
        std::vector<uint32_t> blocked_players;      // 차단한 플레이어
        int32_t max_ping_ms{150};                   // 최대 허용 핑
        std::string preferred_region;               // 선호 지역
        bool allow_cross_region{false};             // 타 지역 매칭 허용
    } preferences;
    
    // [SEQUENCE: MVP13-6] Current status
    struct Status {
        bool in_queue{false};                       // 큐 대기 중
        bool in_match{false};                       // 매치 진행 중
        std::chrono::steady_clock::time_point queue_start_time;
        std::optional<uint64_t> current_match_id;  // 현재 매치 ID
        std::optional<uint64_t> team_id;           // 팀 ID (팀 매치용)
    } status;
    
    // Player metrics for matchmaking
    int32_t average_ping_ms{50};
    std::string region;
    std::vector<uint32_t> recent_opponents;         // 최근 상대 (반복 매칭 방지)
};

// [SEQUENCE: MVP13-7] Match requirements
struct MatchRequirements {
    MatchType match_type;
    int32_t min_players;                            // 최소 플레이어 수
    int32_t max_players;                            // 최대 플레이어 수
    int32_t players_per_team;                       // 팀당 플레이어 수
    
    // [SEQUENCE: MVP13-8] Rating constraints
    struct RatingConstraints {
        int32_t initial_rating_range{100};          // 초기 레이팅 범위
        int32_t max_rating_range{500};              // 최대 레이팅 범위
        double range_expansion_rate{50.0};          // 초당 범위 확장률
        bool strict_rating{false};                  // 엄격한 레이팅 매칭
    } rating_constraints;
    
    // [SEQUENCE: MVP13-9] Team balancing
    struct TeamBalance {
        bool balance_by_rating{true};               // 레이팅으로 팀 밸런싱
        bool balance_by_roles{false};               // 역할로 팀 밸런싱
        int32_t max_team_rating_diff{50};           // 팀간 최대 레이팅 차이
        bool allow_premade_teams{true};             // 미리 구성된 팀 허용
    } team_balance;
    
    // Match settings
    std::chrono::seconds max_queue_time{300};       // 최대 대기 시간
    bool allow_rejoin{true};                        // 재접속 허용
    bool ranked_match{false};                       // 랭크 매치 여부
};

// [SEQUENCE: MVP13-10] Matchmaking queue entry
struct QueueEntry {
    std::shared_ptr<MatchmakingProfile> player;
    MatchType match_type;
    std::chrono::steady_clock::time_point queue_time;
    int32_t expanded_rating_range{0};               // 확장된 레이팅 범위
    
    // [SEQUENCE: MVP13-11] Calculate current acceptable rating range
    int32_t GetAcceptableRatingRange(const MatchRequirements& requirements) const {
        auto elapsed = std::chrono::steady_clock::now() - queue_time;
        auto seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
        
        int32_t base_range = requirements.rating_constraints.initial_rating_range;
        int32_t expansion = static_cast<int32_t>(
            seconds * requirements.rating_constraints.range_expansion_rate
        );
        
        return std::min(base_range + expansion, 
                       requirements.rating_constraints.max_rating_range);
    }
    
    // Check if can match with another player
    bool CanMatchWith(const QueueEntry& other, const MatchRequirements& requirements) const {
        // 같은 플레이어 체크
        if (player->player_id == other.player->player_id) return false;
        
        // 차단 목록 체크
        auto& blocked = player->preferences.blocked_players;
        if (std::find(blocked.begin(), blocked.end(), other.player->player_id) != blocked.end()) {
            return false;
        }
        
        // 레이팅 범위 체크
        auto& my_rating = player->ratings[match_type];
        auto& other_rating = other.player->ratings[match_type];
        
        int32_t rating_diff = std::abs(my_rating.current_rating - other_rating.current_rating);
        int32_t acceptable_range = GetAcceptableRatingRange(requirements);
        
        return rating_diff <= acceptable_range;
    }
};

// [SEQUENCE: MVP13-12] Match group (potential match)
struct MatchGroup {
    std::string match_id;
    MatchType match_type;
    std::vector<std::shared_ptr<QueueEntry>> players;
    
    // [SEQUENCE: MVP13-13] Team assignments
    struct Team {
        std::string team_id;
        std::vector<uint64_t> player_ids;
        int32_t total_rating{0};
        
        void AddPlayer(const MatchmakingProfile& player, MatchType type) {
            player_ids.push_back(player.player_id);
            total_rating += player.ratings.at(type).current_rating;
        }
        
        double GetAverageRating() const {
            return player_ids.empty() ? 0 : 
                   static_cast<double>(total_rating) / player_ids.size();
        }
    };
    
    std::vector<Team> teams;
    
    // [SEQUENCE: MVP13-14] Match quality metrics
    struct QualityMetrics {
        double rating_balance{0.0};     // 팀 밸런스 점수 (0-1)
        double wait_time_score{0.0};    // 대기 시간 점수 (0-1)
        double ping_score{0.0};         // 핑 점수 (0-1)
        double overall_quality{0.0};    // 전체 품질 점수
        
        void Calculate(const std::vector<Team>& teams, 
                      const std::vector<std::shared_ptr<QueueEntry>>& players) {
            // 팀 레이팅 밸런스 계산
            if (teams.size() >= 2) {
                double min_rating = teams[0].GetAverageRating();
                double max_rating = teams[0].GetAverageRating();
                
                for (const auto& team : teams) {
                    double avg = team.GetAverageRating();
                    min_rating = std::min(min_rating, avg);
                    max_rating = std::max(max_rating, avg);
                }
                
                double diff = max_rating - min_rating;
                rating_balance = 1.0 - (diff / 500.0); // 500점 차이를 최대로 봄
                rating_balance = std::max(0.0, rating_balance);
            }
            
            // 대기 시간 점수 계산
            auto now = std::chrono::steady_clock::now();
            double total_wait = 0;
            for (const auto& entry : players) {
                auto wait = std::chrono::duration_cast<std::chrono::seconds>(
                    now - entry->queue_time
                ).count();
                total_wait += wait;
            }
            double avg_wait = total_wait / players.size();
            wait_time_score = std::min(1.0, avg_wait / 60.0); // 60초를 기준으로
            
            // 핑 점수 계산
            double total_ping = 0;
            for (const auto& entry : players) {
                total_ping += entry->player->average_ping_ms;
            }
            double avg_ping = total_ping / players.size();
            ping_score = 1.0 - (avg_ping / 200.0); // 200ms를 최대로 봄
            ping_score = std::max(0.0, ping_score);
            
            // 전체 품질 점수
            overall_quality = (rating_balance * 0.5 + 
                             wait_time_score * 0.3 + 
                             ping_score * 0.2);
        }
    } quality_metrics;
    
    // Create balanced teams
    void CreateBalancedTeams(const MatchRequirements& requirements) {
        if (!requirements.team_balance.balance_by_rating) {
            // 단순 분할
            CreateSimpleTeams(requirements);
            return;
        }
        
        // [SEQUENCE: MVP13-15] Sort by rating for balanced distribution
        std::vector<std::shared_ptr<QueueEntry>> sorted_players = players;
        std::sort(sorted_players.begin(), sorted_players.end(),
            [&](const auto& a, const auto& b) {
                return a->player->ratings[match_type].current_rating >
                       b->player->ratings[match_type].current_rating;
            });
        
        // Snake draft for balance (1-2-2-1 pattern)
        int num_teams = sorted_players.size() / requirements.players_per_team;
        teams.resize(num_teams);
        
        for (int i = 0; i < num_teams; ++i) {
            teams[i].team_id = "team_" + std::to_string(i);
        }
        
        int team_index = 0;
        bool reverse = false;
        
        for (const auto& entry : sorted_players) {
            teams[team_index].AddPlayer(*entry->player, match_type);
            
            if (!reverse) {
                team_index++;
                if (team_index >= num_teams) {
                    team_index = num_teams - 1;
                    reverse = true;
                }
            } else {
                team_index--;
                if (team_index < 0) {
                    team_index = 0;
                    reverse = false;
                }
            }
        }
        
        // Calculate match quality
        quality_metrics.Calculate(teams, players);
    }
    
private:
    void CreateSimpleTeams(const MatchRequirements& requirements) {
        int num_teams = players.size() / requirements.players_per_team;
        teams.resize(num_teams);
        
        for (int i = 0; i < num_teams; ++i) {
            teams[i].team_id = "team_" + std::to_string(i);
        }
        
        // 순차적으로 팀 할당
        for (size_t i = 0; i < players.size(); ++i) {
            int team_index = i % num_teams;
            teams[team_index].AddPlayer(*players[i]->player, match_type);
        }
    }
};

// [SEQUENCE: MVP13-16] Matchmaking service
class MatchmakingService {
public:
    MatchmakingService() {
        // Initialize match requirements for each type
        InitializeMatchRequirements();
        
        // Start matchmaking worker thread
        StartMatchmakingWorker();
    }
    
    ~MatchmakingService() {
        StopMatchmakingWorker();
    }
    
    // [SEQUENCE: MVP13-17] Add player to matchmaking queue
    bool AddToQueue(std::shared_ptr<MatchmakingProfile> player, MatchType match_type) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        
        // 이미 큐에 있는지 확인
        if (player->status.in_queue) {
            spdlog::warn("Player {} already in queue", player->player_id);
            return false;
        }
        
        // 매치 중인지 확인
        if (player->status.in_match) {
            spdlog::warn("Player {} already in match", player->player_id);
            return false;
        }
        
        // 큐 엔트리 생성
        auto entry = std::make_shared<QueueEntry>();
        entry->player = player;
        entry->match_type = match_type;
        entry->queue_time = std::chrono::steady_clock::now();
        
        // 큐에 추가
        match_queues_[match_type].push_back(entry);
        player_queue_map_[player->player_id] = entry;
        
        // 플레이어 상태 업데이트
        player->status.in_queue = true;
        player->status.queue_start_time = entry->queue_time;
        
        spdlog::info("Player {} added to {} queue. Queue size: {}", 
                    player->player_id, 
                    GetMatchTypeName(match_type),
                    match_queues_[match_type].size());
        
        // 즉시 매치 시도
        TryCreateMatches(match_type);
        
        return true;
    }
    
    // [SEQUENCE: MVP13-18] Remove player from queue
    bool RemoveFromQueue(uint64_t player_id) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        
        auto it = player_queue_map_.find(player_id);
        if (it == player_queue_map_.end()) {
            return false;
        }
        
        auto entry = it->second;
        auto& queue = match_queues_[entry->match_type];
        
        // 큐에서 제거
        queue.erase(
            std::remove_if(queue.begin(), queue.end(),
                [player_id](const auto& e) {
                    return e->player->player_id == player_id;
                }),
            queue.end()
        );
        
        // 상태 업데이트
        entry->player->status.in_queue = false;
        player_queue_map_.erase(player_id);
        
        spdlog::info("Player {} removed from queue", player_id);
        return true;
    }
    
    // [SEQUENCE: MVP13-19] Get queue status
    struct QueueStatus {
        MatchType match_type;
        uint32_t players_in_queue;
        uint32_t average_wait_time_seconds;
        uint32_t estimated_wait_time_seconds;
    };
    
    QueueStatus GetQueueStatus(MatchType match_type) const {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        
        QueueStatus status;
        status.match_type = match_type;
        
        const auto& queue = match_queues_.at(match_type);
        status.players_in_queue = queue.size();
        
        // 평균 대기 시간 계산
        if (!queue.empty()) {
            auto now = std::chrono::steady_clock::now();
            uint64_t total_wait = 0;
            
            for (const auto& entry : queue) {
                auto wait = std::chrono::duration_cast<std::chrono::seconds>(
                    now - entry->queue_time
                ).count();
                total_wait += wait;
            }
            
            status.average_wait_time_seconds = total_wait / queue.size();
        }
        
        // 예상 대기 시간 (간단한 추정)
        auto& requirements = match_requirements_.at(match_type);
        int players_needed = requirements.min_players - status.players_in_queue;
        if (players_needed > 0) {
            // 분당 평균 10명이 큐에 참가한다고 가정
            status.estimated_wait_time_seconds = (players_needed * 6);
        }
        
        return status;
    }
    
    // [SEQUENCE: MVP13-20] Force match creation (for testing)
    std::optional<MatchGroup> ForceCreateMatch(MatchType match_type) {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return TryCreateMatches(match_type);
    }
    
    // [SEQUENCE: MVP13-21] Get player's current match
    std::optional<uint64_t> GetPlayerMatch(uint64_t player_id) const {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        
        auto it = player_queue_map_.find(player_id);
        if (it != player_queue_map_.end()) {
            return it->second->player->status.current_match_id;
        }
        
        return std::nullopt;
    }
    
private:
    mutable std::mutex queue_mutex_;
    std::unordered_map<MatchType, std::deque<std::shared_ptr<QueueEntry>>> match_queues_;
    std::unordered_map<uint64_t, std::shared_ptr<QueueEntry>> player_queue_map_;
    std::unordered_map<MatchType, MatchRequirements> match_requirements_;
    
    std::atomic<bool> running_{false};
    std::thread worker_thread_;
    
    // [SEQUENCE: MVP13-22] Initialize match requirements
    void InitializeMatchRequirements() {
        // 1v1 Arena
        match_requirements_[MatchType::ARENA_1V1] = {
            .match_type = MatchType::ARENA_1V1,
            .min_players = 2,
            .max_players = 2,
            .players_per_team = 1,
            .rating_constraints = {
                .initial_rating_range = 100,
                .max_rating_range = 400,
                .range_expansion_rate = 50.0,
                .strict_rating = true
            },
            .team_balance = {
                .balance_by_rating = false,  // 1v1은 팀 밸런싱 불필요
                .balance_by_roles = false,
                .max_team_rating_diff = 0,
                .allow_premade_teams = false
            },
            .max_queue_time = std::chrono::seconds(300),
            .allow_rejoin = true,
            .ranked_match = true
        };
        
        // 3v3 Arena
        match_requirements_[MatchType::ARENA_3V3] = {
            .match_type = MatchType::ARENA_3V3,
            .min_players = 6,
            .max_players = 6,
            .players_per_team = 3,
            .rating_constraints = {
                .initial_rating_range = 150,
                .max_rating_range = 500,
                .range_expansion_rate = 40.0,
                .strict_rating = false
            },
            .team_balance = {
                .balance_by_rating = true,
                .balance_by_roles = true,
                .max_team_rating_diff = 100,
                .allow_premade_teams = true
            },
            .max_queue_time = std::chrono::seconds(600),
            .allow_rejoin = true,
            .ranked_match = true
        };
        
        // 10v10 Battleground
        match_requirements_[MatchType::BATTLEGROUND_10V10] = {
            .match_type = MatchType::BATTLEGROUND_10V10,
            .min_players = 20,
            .max_players = 20,
            .players_per_team = 10,
            .rating_constraints = {
                .initial_rating_range = 200,
                .max_rating_range = 800,
                .range_expansion_rate = 60.0,
                .strict_rating = false
            },
            .team_balance = {
                .balance_by_rating = true,
                .balance_by_roles = false,
                .max_team_rating_diff = 200,
                .allow_premade_teams = true
            },
            .max_queue_time = std::chrono::seconds(900),
            .allow_rejoin = true,
            .ranked_match = false
        };
        
        // 추가 매치 타입들도 비슷하게 설정...
    }
    
    // [SEQUENCE: MVP13-23] Try to create matches from queue
    std::optional<MatchGroup> TryCreateMatches(MatchType match_type) {
        auto& queue = match_queues_[match_type];
        auto& requirements = match_requirements_[match_type];
        
        // 최소 인원 체크
        if (queue.size() < static_cast<size_t>(requirements.min_players)) {
            return std::nullopt;
        }
        
        // [SEQUENCE: MVP13-24] Find compatible players
        std::vector<std::shared_ptr<QueueEntry>> candidates;
        
        // 가장 오래 기다린 플레이어부터 시작
        for (auto& entry : queue) {
            if (candidates.empty()) {
                candidates.push_back(entry);
                continue;
            }
            
            // 모든 후보와 매칭 가능한지 확인
            bool compatible = true;
            for (const auto& candidate : candidates) {
                if (!entry->CanMatchWith(*candidate, requirements)) {
                    compatible = false;
                    break;
                }
            }
            
            if (compatible) {
                candidates.push_back(entry);
                
                // 필요한 인원을 모았으면 매치 생성
                if (candidates.size() >= static_cast<size_t>(requirements.min_players)) {
                    return CreateMatch(candidates, requirements);
                }
            }
        }
        
        return std::nullopt;
    }
    
    // [SEQUENCE: MVP13-25] Create match from candidates
    std::optional<MatchGroup> CreateMatch(
        const std::vector<std::shared_ptr<QueueEntry>>& candidates,
        const MatchRequirements& requirements) {
        
        MatchGroup match;
        match.match_id = GenerateMatchId();
        match.match_type = requirements.match_type;
        match.players = candidates;
        
        // 팀 구성
        match.CreateBalancedTeams(requirements);
        
        // 매치 품질이 너무 낮으면 취소
        if (match.quality_metrics.overall_quality < 0.3) {
            spdlog::warn("Match quality too low: {}", 
                        match.quality_metrics.overall_quality);
            return std::nullopt;
        }
        
        // 큐에서 플레이어 제거
        for (const auto& entry : candidates) {
            RemoveFromQueue(entry->player->player_id);
            
            // 플레이어 상태 업데이트
            entry->player->status.in_match = true;
            entry->player->status.current_match_id = std::stoull(match.match_id);
        }
        
        spdlog::info("Match created: {} with {} players, quality: {:.2f}",
                    match.match_id, candidates.size(), 
                    match.quality_metrics.overall_quality);
        
        // 매치 생성 이벤트 발생
        OnMatchCreated(match);
        
        return match;
    }
    
    // [SEQUENCE: MVP13-26] Matchmaking worker thread
    void StartMatchmakingWorker() {
        running_ = true;
        worker_thread_ = std::thread([this]() {
            while (running_) {
                // 모든 큐 타입에 대해 매칭 시도
                {
                    std::lock_guard<std::mutex> lock(queue_mutex_);
                    
                    for (auto& [match_type, queue] : match_queues_) {
                        if (!queue.empty()) {
                            TryCreateMatches(match_type);
                        }
                        
                        // 오래 기다린 플레이어 처리
                        HandleLongWaitPlayers(match_type);
                    }
                }
                
                // 1초마다 실행
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
        });
    }
    
    void StopMatchmakingWorker() {
        running_ = false;
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }
    }
    
    // [SEQUENCE: MVP13-27] Handle players waiting too long
    void HandleLongWaitPlayers(MatchType match_type) {
        auto& queue = match_queues_[match_type];
        auto& requirements = match_requirements_[match_type];
        auto now = std::chrono::steady_clock::now();
        
        for (auto it = queue.begin(); it != queue.end(); ) {
            auto wait_time = now - (*it)->queue_time;
            
            if (wait_time > requirements.max_queue_time) {
                spdlog::warn("Player {} exceeded max queue time for {}",
                           (*it)->player->player_id, GetMatchTypeName(match_type));
                
                // 타임아웃 이벤트 발생
                OnQueueTimeout(**it);
                
                // 큐에서 제거
                (*it)->player->status.in_queue = false;
                player_queue_map_.erase((*it)->player->player_id);
                it = queue.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    // Event callbacks
    std::function<void(const MatchGroup&)> OnMatchCreated;
    std::function<void(const QueueEntry&)> OnQueueTimeout;
    
    // Utility functions
    std::string GenerateMatchId() {
        static std::atomic<uint64_t> counter{0};
        return "match_" + std::to_string(++counter);
    }
    
    std::string GetMatchTypeName(MatchType type) {
        static const std::unordered_map<MatchType, std::string> names = {
            {MatchType::ARENA_1V1, "1v1 Arena"},
            {MatchType::ARENA_2V2, "2v2 Arena"},
            {MatchType::ARENA_3V3, "3v3 Arena"},
            {MatchType::ARENA_5V5, "5v5 Arena"},
            {MatchType::BATTLEGROUND_10V10, "10v10 Battleground"},
            {MatchType::BATTLEGROUND_20V20, "20v20 Battleground"},
            {MatchType::RANKED_SOLO, "Ranked Solo"},
            {MatchType::RANKED_TEAM, "Ranked Team"},
            {MatchType::TOURNAMENT, "Tournament"},
            {MatchType::CUSTOM, "Custom"}
        };
        
        auto it = names.find(type);
        return it != names.end() ? it->second : "Unknown";
    }
};

// [SEQUENCE: MVP13-28] Matchmaking statistics
class MatchmakingStatistics {
public:
    struct Stats {
        uint64_t total_matches_created{0};
        uint64_t total_players_matched{0};
        std::unordered_map<MatchType, uint64_t> matches_by_type;
        double average_wait_time_seconds{0.0};
        double average_match_quality{0.0};
        uint64_t queue_timeouts{0};
        
        // Time-based stats
        struct HourlyStats {
            uint32_t matches_created{0};
            uint32_t players_matched{0};
            double avg_wait_time{0.0};
        };
        std::array<HourlyStats, 24> hourly_stats;
    };
    
    void RecordMatch(const MatchGroup& match) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        stats_.total_matches_created++;
        stats_.total_players_matched += match.players.size();
        stats_.matches_by_type[match.match_type]++;
        
        // 대기 시간 계산
        auto now = std::chrono::steady_clock::now();
        double total_wait = 0;
        for (const auto& entry : match.players) {
            auto wait = std::chrono::duration_cast<std::chrono::seconds>(
                now - entry->queue_time
            ).count();
            total_wait += wait;
        }
        
        // 이동 평균으로 평균 대기 시간 업데이트
        double avg_wait = total_wait / match.players.size();
        stats_.average_wait_time_seconds = 
            (stats_.average_wait_time_seconds * 0.9) + (avg_wait * 0.1);
        
        // 매치 품질 업데이트
        stats_.average_match_quality = 
            (stats_.average_match_quality * 0.9) + 
            (match.quality_metrics.overall_quality * 0.1);
        
        // 시간별 통계
        auto hour = GetCurrentHour();
        stats_.hourly_stats[hour].matches_created++;
        stats_.hourly_stats[hour].players_matched += match.players.size();
    }
    
    void RecordTimeout() {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.queue_timeouts++;
    }
    
    Stats GetStats() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }
    
private:
    mutable std::mutex mutex_;
    Stats stats_;
    
    int GetCurrentHour() const {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm* tm = std::localtime(&time_t);
        return tm->tm_hour;
    }
};

} // namespace mmorpg::matchmaking