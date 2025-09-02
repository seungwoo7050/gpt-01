#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <map>
#include <memory>
#include <chrono>
#include <mutex>
#include <atomic>
#include <optional>
#include <algorithm>
#include <cmath>
#include <spdlog/spdlog.h>

namespace mmorpg::ranking {

// [SEQUENCE: MVP13-29] Comprehensive ranking system for competitive modes
// 종합 랭킹 시스템 - 경쟁 모드를 위한 순위 관리

// [SEQUENCE: MVP13-30] Ranking categories
enum class RankingCategory {
    ARENA_1V1,              // 1대1 아레나 랭킹
    ARENA_2V2,              // 2대2 아레나 랭킹
    ARENA_3V3,              // 3대3 아레나 랭킹
    ARENA_5V5,              // 5대5 아레나 랭킹
    BATTLEGROUND,           // 전장 랭킹
    GUILD_WARS,             // 길드전 랭킹
    OVERALL_PVP,            // 전체 PvP 랭킹
    PVE_RAID,               // 레이드 랭킹
    PVE_DUNGEON,            // 던전 랭킹
    ACHIEVEMENT_POINTS,     // 업적 포인트 랭킹
    LEVEL_RACE,             // 레벨 경쟁 랭킹
    WEALTH,                 // 재산 랭킹
    PLAYTIME,               // 플레이 시간 랭킹
    CUSTOM                  // 커스텀 랭킹
};

// [SEQUENCE: MVP13-31] Time periods for rankings
enum class RankingPeriod {
    DAILY,                  // 일간 랭킹
    WEEKLY,                 // 주간 랭킹
    MONTHLY,                // 월간 랭킹
    SEASONAL,               // 시즌 랭킹
    ALL_TIME                // 전체 기간 랭킹
};

// [SEQUENCE: MVP13-32] Player rank information
struct PlayerRankInfo {
    uint64_t player_id;
    std::string player_name;
    std::string guild_name;
    
    // [SEQUENCE: MVP13-33] Rank data
    struct RankData {
        uint32_t rank{0};                   // 현재 순위
        uint32_t previous_rank{0};          // 이전 순위
        int32_t rank_change{0};             // 순위 변동
        
        int32_t rating{1500};               // 현재 레이팅
        int32_t peak_rating{1500};          // 최고 레이팅
        
        uint32_t wins{0};                   // 승리 횟수
        uint32_t losses{0};                 // 패배 횟수
        uint32_t draws{0};                  // 무승부
        
        double win_rate{0.0};               // 승률
        uint32_t win_streak{0};             // 현재 연승
        uint32_t best_win_streak{0};        // 최고 연승
        
        std::chrono::system_clock::time_point last_update; // 마지막 업데이트
        
        // Calculate win rate
        void UpdateWinRate() {
            uint32_t total = wins + losses + draws;
            win_rate = total > 0 ? static_cast<double>(wins) / total : 0.0;
        }
    };
    
    // [SEQUENCE: MVP13-34] Additional statistics
    struct Statistics {
        uint64_t total_damage_dealt{0};     // 총 가한 피해
        uint64_t total_damage_taken{0};     // 총 받은 피해
        uint64_t total_healing_done{0};     // 총 치유량
        uint32_t killing_blows{0};          // 결정타
        uint32_t deaths{0};                 // 사망 횟수
        double kd_ratio{0.0};               // K/D 비율
        
        // Special achievements
        uint32_t mvp_count{0};              // MVP 획득 횟수
        uint32_t perfect_games{0};          // 완벽한 게임 (무사망 승리)
        uint32_t comeback_wins{0};          // 역전승
        
        void UpdateKDRatio() {
            kd_ratio = deaths > 0 ? 
                      static_cast<double>(killing_blows) / deaths : 
                      static_cast<double>(killing_blows);
        }
    };
    
    RankData rank_data;
    Statistics stats;
    
    // [SEQUENCE: MVP13-35] Display information
    std::string class_name;                 // 직업
    uint32_t level{1};                      // 레벨
    std::string title;                      // 칭호
    uint32_t achievement_points{0};         // 업적 포인트
    
    // Comparison operator for sorting
    bool operator<(const PlayerRankInfo& other) const {
        return rank_data.rating > other.rank_data.rating; // 높은 레이팅이 앞
    }
};

// [SEQUENCE: MVP13-36] Ranking tier system
enum class RankingTier {
    UNRANKED,               // 언랭크 (배치 전)
    BRONZE,                 // 브론즈 (1000-1199)
    SILVER,                 // 실버 (1200-1399)
    GOLD,                   // 골드 (1400-1599)
    PLATINUM,               // 플래티넘 (1600-1799)
    DIAMOND,                // 다이아몬드 (1800-1999)
    MASTER,                 // 마스터 (2000-2199)
    GRANDMASTER,            // 그랜드마스터 (2200-2399)
    CHALLENGER              // 챌린저 (2400+)
};

// [SEQUENCE: MVP13-37] Tier requirements and rewards
struct TierInfo {
    RankingTier tier;
    int32_t min_rating;
    int32_t max_rating;
    std::string tier_name;
    std::string icon_path;
    
    // [SEQUENCE: MVP13-38] Tier rewards
    struct Rewards {
        uint32_t currency_bonus{0};         // 통화 보너스
        float experience_multiplier{1.0f};   // 경험치 배율
        std::vector<uint32_t> exclusive_items; // 전용 아이템
        std::string exclusive_title;        // 전용 칭호
        bool seasonal_mount{false};         // 시즌 탈것
    } rewards;
    
    // [SEQUENCE: MVP13-39] Tier decay settings
    struct Decay {
        bool enabled{false};                // 티어 하락 활성화
        uint32_t inactive_days{14};         // 비활성 기간
        int32_t daily_rating_loss{5};       // 일일 레이팅 감소
        int32_t min_rating_floor{0};        // 최소 레이팅 하한선
    } decay;
};

// [SEQUENCE: MVP13-40] Season information
struct SeasonInfo {
    uint32_t season_id;
    std::string season_name;
    std::chrono::system_clock::time_point start_date;
    std::chrono::system_clock::time_point end_date;
    bool is_active{true};
    
    // [SEQUENCE: MVP13-41] Season rewards
    struct SeasonRewards {
        std::unordered_map<RankingTier, std::vector<uint32_t>> tier_rewards;
        std::vector<uint32_t> participation_rewards;
        std::vector<uint32_t> top_100_rewards;
        std::vector<uint32_t> top_10_rewards;
        uint32_t rank_1_exclusive_mount{0};
    } rewards;
    
    bool IsInSeason() const {
        auto now = std::chrono::system_clock::now();
        return is_active && now >= start_date && now <= end_date;
    }
    
    std::chrono::seconds GetTimeRemaining() const {
        auto now = std::chrono::system_clock::now();
        if (now >= end_date) return std::chrono::seconds(0);
        return std::chrono::duration_cast<std::chrono::seconds>(end_date - now);
    }
};

// [SEQUENCE: MVP13-42] Ranking service
class RankingService {
public:
    RankingService() {
        InitializeTiers();
        StartDecayWorker();
    }
    
    ~RankingService() {
        StopDecayWorker();
    }
    
    // [SEQUENCE: MVP13-43] Update player ranking
    void UpdatePlayerRanking(uint64_t player_id, 
                           RankingCategory category,
                           int32_t rating_change,
                           bool is_win) {
        std::lock_guard<std::mutex> lock(rankings_mutex_);
        
        auto& rankings = category_rankings_[category];
        auto it = std::find_if(rankings.begin(), rankings.end(),
            [player_id](const PlayerRankInfo& info) {
                return info.player_id == player_id;
            });
        
        if (it == rankings.end()) {
            // 새 플레이어 추가
            PlayerRankInfo new_player;
            new_player.player_id = player_id;
            new_player.rank_data.rating = 1500 + rating_change;
            rankings.push_back(new_player);
            it = rankings.end() - 1;
        } else {
            // 기존 플레이어 업데이트
            it->rank_data.rating += rating_change;
        }
        
        // [SEQUENCE: MVP13-44] Update statistics
        auto& rank_data = it->rank_data;
        
        if (is_win) {
            rank_data.wins++;
            rank_data.win_streak++;
            rank_data.best_win_streak = std::max(rank_data.best_win_streak, 
                                                rank_data.win_streak);
        } else {
            rank_data.losses++;
            rank_data.win_streak = 0;
        }
        
        rank_data.UpdateWinRate();
        rank_data.peak_rating = std::max(rank_data.peak_rating, rank_data.rating);
        rank_data.last_update = std::chrono::system_clock::now();
        
        // 순위 재계산
        RecalculateRanks(category);
        
        // 티어 업데이트
        UpdatePlayerTier(player_id, category, rank_data.rating);
        
        spdlog::info("Updated ranking for player {} in {}: rating {} ({}{})", 
                    player_id, GetCategoryName(category), rank_data.rating,
                    rating_change > 0 ? "+" : "", rating_change);
    }
    
    // [SEQUENCE: MVP13-45] Get player rank
    std::optional<PlayerRankInfo> GetPlayerRank(uint64_t player_id, 
                                               RankingCategory category) const {
        std::lock_guard<std::mutex> lock(rankings_mutex_);
        
        auto it = category_rankings_.find(category);
        if (it == category_rankings_.end()) {
            return std::nullopt;
        }
        
        const auto& rankings = it->second;
        auto player_it = std::find_if(rankings.begin(), rankings.end(),
            [player_id](const PlayerRankInfo& info) {
                return info.player_id == player_id;
            });
        
        if (player_it != rankings.end()) {
            return *player_it;
        }
        
        return std::nullopt;
    }
    
    // [SEQUENCE: MVP13-46] Get top players
    std::vector<PlayerRankInfo> GetTopPlayers(RankingCategory category, 
                                             uint32_t count = 100) const {
        std::lock_guard<std::mutex> lock(rankings_mutex_);
        
        auto it = category_rankings_.find(category);
        if (it == category_rankings_.end()) {
            return {};
        }
        
        const auto& rankings = it->second;
        count = std::min(count, static_cast<uint32_t>(rankings.size()));
        
        return std::vector<PlayerRankInfo>(rankings.begin(), 
                                         rankings.begin() + count);
    }
    
    // [SEQUENCE: MVP13-47] Get rankings by period
    std::vector<PlayerRankInfo> GetPeriodRankings(RankingCategory category,
                                                 RankingPeriod period,
                                                 uint32_t count = 100) const {
        std::lock_guard<std::mutex> lock(rankings_mutex_);
        
        // 기간별 랭킹은 별도 저장소에서 가져옴
        auto key = std::make_pair(category, period);
        auto it = period_rankings_.find(key);
        
        if (it == period_rankings_.end()) {
            return {};
        }
        
        const auto& rankings = it->second;
        count = std::min(count, static_cast<uint32_t>(rankings.size()));
        
        return std::vector<PlayerRankInfo>(rankings.begin(), 
                                         rankings.begin() + count);
    }
    
    // [SEQUENCE: MVP13-48] Get player tier
    RankingTier GetPlayerTier(uint64_t player_id, 
                             RankingCategory category) const {
        auto rank_info = GetPlayerRank(player_id, category);
        if (!rank_info.has_value()) {
            return RankingTier::UNRANKED;
        }
        
        return GetTierByRating(rank_info->rank_data.rating);
    }
    
    // [SEQUENCE: MVP13-49] Search rankings
    std::vector<PlayerRankInfo> SearchRankings(RankingCategory category,
                                              const std::string& player_name) const {
        std::lock_guard<std::mutex> lock(rankings_mutex_);
        
        std::vector<PlayerRankInfo> results;
        auto it = category_rankings_.find(category);
        
        if (it != category_rankings_.end()) {
            for (const auto& player : it->second) {
                if (player.player_name.find(player_name) != std::string::npos) {
                    results.push_back(player);
                    if (results.size() >= 10) break; // 최대 10개
                }
            }
        }
        
        return results;
    }
    
    // [SEQUENCE: MVP13-50] Get tier distribution
    std::unordered_map<RankingTier, uint32_t> GetTierDistribution(
        RankingCategory category) const {
        std::lock_guard<std::mutex> lock(rankings_mutex_);
        
        std::unordered_map<RankingTier, uint32_t> distribution;
        auto it = category_rankings_.find(category);
        
        if (it != category_rankings_.end()) {
            for (const auto& player : it->second) {
                auto tier = GetTierByRating(player.rank_data.rating);
                distribution[tier]++;
            }
        }
        
        return distribution;
    }
    
    // [SEQUENCE: MVP13-51] Start new season
    void StartNewSeason(const SeasonInfo& season) {
        std::lock_guard<std::mutex> lock(rankings_mutex_);
        
        // 현재 시즌 종료
        if (current_season_.has_value()) {
            EndCurrentSeason();
        }
        
        // 새 시즌 시작
        current_season_ = season;
        
        // 모든 플레이어 소프트 리셋
        for (auto& [category, rankings] : category_rankings_) {
            for (auto& player : rankings) {
                // 소프트 리셋: (현재 레이팅 + 1500) / 2
                player.rank_data.rating = (player.rank_data.rating + 1500) / 2;
                player.rank_data.wins = 0;
                player.rank_data.losses = 0;
                player.rank_data.draws = 0;
                player.rank_data.win_streak = 0;
                player.rank_data.UpdateWinRate();
            }
            
            // 순위 재계산
            RecalculateRanks(category);
        }
        
        spdlog::info("Started new season: {} (ID: {})", 
                    season.season_name, season.season_id);
    }
    
private:
    mutable std::mutex rankings_mutex_;
    std::unordered_map<RankingCategory, std::vector<PlayerRankInfo>> category_rankings_;
    std::map<std::pair<RankingCategory, RankingPeriod>, 
             std::vector<PlayerRankInfo>> period_rankings_;
    
    std::unordered_map<RankingTier, TierInfo> tier_info_;
    std::optional<SeasonInfo> current_season_;
    
    std::atomic<bool> decay_worker_running_{false};
    std::thread decay_worker_thread_;
    
    // [SEQUENCE: MVP13-52] Initialize tier information
    void InitializeTiers() {
        tier_info_[RankingTier::BRONZE] = {
            .tier = RankingTier::BRONZE,
            .min_rating = 1000,
            .max_rating = 1199,
            .tier_name = "Bronze",
            .icon_path = "icons/tiers/bronze.png",
            .rewards = {
                .currency_bonus = 100,
                .experience_multiplier = 1.0f
            },
            .decay = {
                .enabled = false
            }
        };
        
        tier_info_[RankingTier::SILVER] = {
            .tier = RankingTier::SILVER,
            .min_rating = 1200,
            .max_rating = 1399,
            .tier_name = "Silver",
            .icon_path = "icons/tiers/silver.png",
            .rewards = {
                .currency_bonus = 200,
                .experience_multiplier = 1.1f
            },
            .decay = {
                .enabled = false
            }
        };
        
        tier_info_[RankingTier::GOLD] = {
            .tier = RankingTier::GOLD,
            .min_rating = 1400,
            .max_rating = 1599,
            .tier_name = "Gold",
            .icon_path = "icons/tiers/gold.png",
            .rewards = {
                .currency_bonus = 300,
                .experience_multiplier = 1.2f,
                .exclusive_items = {10001, 10002} // Gold tier items
            },
            .decay = {
                .enabled = true,
                .inactive_days = 14,
                .daily_rating_loss = 5,
                .min_rating_floor = 1400
            }
        };
        
        tier_info_[RankingTier::PLATINUM] = {
            .tier = RankingTier::PLATINUM,
            .min_rating = 1600,
            .max_rating = 1799,
            .tier_name = "Platinum",
            .icon_path = "icons/tiers/platinum.png",
            .rewards = {
                .currency_bonus = 500,
                .experience_multiplier = 1.3f,
                .exclusive_items = {10003, 10004},
                .exclusive_title = "Platinum Warrior"
            },
            .decay = {
                .enabled = true,
                .inactive_days = 7,
                .daily_rating_loss = 10,
                .min_rating_floor = 1600
            }
        };
        
        tier_info_[RankingTier::DIAMOND] = {
            .tier = RankingTier::DIAMOND,
            .min_rating = 1800,
            .max_rating = 1999,
            .tier_name = "Diamond",
            .icon_path = "icons/tiers/diamond.png",
            .rewards = {
                .currency_bonus = 750,
                .experience_multiplier = 1.4f,
                .exclusive_items = {10005, 10006},
                .exclusive_title = "Diamond Champion"
            },
            .decay = {
                .enabled = true,
                .inactive_days = 7,
                .daily_rating_loss = 15,
                .min_rating_floor = 1800
            }
        };
        
        tier_info_[RankingTier::MASTER] = {
            .tier = RankingTier::MASTER,
            .min_rating = 2000,
            .max_rating = 2199,
            .tier_name = "Master",
            .icon_path = "icons/tiers/master.png",
            .rewards = {
                .currency_bonus = 1000,
                .experience_multiplier = 1.5f,
                .exclusive_items = {10007, 10008},
                .exclusive_title = "Master Gladiator",
                .seasonal_mount = true
            },
            .decay = {
                .enabled = true,
                .inactive_days = 3,
                .daily_rating_loss = 20,
                .min_rating_floor = 2000
            }
        };
        
        tier_info_[RankingTier::GRANDMASTER] = {
            .tier = RankingTier::GRANDMASTER,
            .min_rating = 2200,
            .max_rating = 2399,
            .tier_name = "Grandmaster",
            .icon_path = "icons/tiers/grandmaster.png",
            .rewards = {
                .currency_bonus = 1500,
                .experience_multiplier = 1.75f,
                .exclusive_items = {10009, 10010, 10011},
                .exclusive_title = "Grandmaster Elite",
                .seasonal_mount = true
            },
            .decay = {
                .enabled = true,
                .inactive_days = 2,
                .daily_rating_loss = 25,
                .min_rating_floor = 2200
            }
        };
        
        tier_info_[RankingTier::CHALLENGER] = {
            .tier = RankingTier::CHALLENGER,
            .min_rating = 2400,
            .max_rating = 9999,
            .tier_name = "Challenger",
            .icon_path = "icons/tiers/challenger.png",
            .rewards = {
                .currency_bonus = 2000,
                .experience_multiplier = 2.0f,
                .exclusive_items = {10012, 10013, 10014, 10015},
                .exclusive_title = "Challenger Legend",
                .seasonal_mount = true
            },
            .decay = {
                .enabled = true,
                .inactive_days = 1,
                .daily_rating_loss = 30,
                .min_rating_floor = 2400
            }
        };
    }
    
    // [SEQUENCE: MVP13-53] Recalculate ranks
    void RecalculateRanks(RankingCategory category) {
        auto& rankings = category_rankings_[category];
        
        // 레이팅 기준 정렬
        std::sort(rankings.begin(), rankings.end());
        
        // 순위 업데이트
        for (size_t i = 0; i < rankings.size(); ++i) {
            rankings[i].rank_data.previous_rank = rankings[i].rank_data.rank;
            rankings[i].rank_data.rank = i + 1;
            rankings[i].rank_data.rank_change = 
                rankings[i].rank_data.previous_rank - rankings[i].rank_data.rank;
        }
    }
    
    // [SEQUENCE: MVP13-54] Get tier by rating
    RankingTier GetTierByRating(int32_t rating) const {
        if (rating < 1000) return RankingTier::UNRANKED;
        if (rating < 1200) return RankingTier::BRONZE;
        if (rating < 1400) return RankingTier::SILVER;
        if (rating < 1600) return RankingTier::GOLD;
        if (rating < 1800) return RankingTier::PLATINUM;
        if (rating < 2000) return RankingTier::DIAMOND;
        if (rating < 2200) return RankingTier::MASTER;
        if (rating < 2400) return RankingTier::GRANDMASTER;
        return RankingTier::CHALLENGER;
    }
    
    // [SEQUENCE: MVP13-55] Update player tier
    void UpdatePlayerTier(uint64_t player_id, RankingCategory category, 
                         int32_t new_rating) {
        auto old_tier = GetPlayerTier(player_id, category);
        auto new_tier = GetTierByRating(new_rating);
        
        if (old_tier != new_tier) {
            // 티어 변경 이벤트
            OnTierChange(player_id, category, old_tier, new_tier);
            
            // 승급 보상
            if (new_tier > old_tier) {
                GrantTierRewards(player_id, new_tier);
            }
        }
    }
    
    // [SEQUENCE: MVP13-56] Decay worker thread
    void StartDecayWorker() {
        decay_worker_running_ = true;
        decay_worker_thread_ = std::thread([this]() {
            while (decay_worker_running_) {
                // 매일 자정에 실행
                auto now = std::chrono::system_clock::now();
                auto midnight = GetNextMidnight();
                
                std::this_thread::sleep_until(midnight);
                
                if (!decay_worker_running_) break;
                
                ProcessRatingDecay();
            }
        });
    }
    
    void StopDecayWorker() {
        decay_worker_running_ = false;
        if (decay_worker_thread_.joinable()) {
            decay_worker_thread_.join();
        }
    }
    
    // [SEQUENCE: MVP13-57] Process rating decay
    void ProcessRatingDecay() {
        std::lock_guard<std::mutex> lock(rankings_mutex_);
        
        auto now = std::chrono::system_clock::now();
        
        for (auto& [category, rankings] : category_rankings_) {
            for (auto& player : rankings) {
                auto tier = GetTierByRating(player.rank_data.rating);
                auto tier_it = tier_info_.find(tier);
                
                if (tier_it == tier_info_.end() || !tier_it->second.decay.enabled) {
                    continue;
                }
                
                const auto& decay = tier_it->second.decay;
                
                // 비활성 기간 확인
                auto days_inactive = std::chrono::duration_cast<std::chrono::days>(
                    now - player.rank_data.last_update
                ).count();
                
                if (days_inactive >= decay.inactive_days) {
                    // 레이팅 감소
                    int32_t old_rating = player.rank_data.rating;
                    player.rank_data.rating -= decay.daily_rating_loss;
                    
                    // 최소값 제한
                    player.rank_data.rating = std::max(player.rank_data.rating, 
                                                     decay.min_rating_floor);
                    
                    if (old_rating != player.rank_data.rating) {
                        spdlog::info("Rating decay for player {}: {} -> {} (inactive {} days)",
                                    player.player_id, old_rating, player.rank_data.rating,
                                    days_inactive);
                    }
                }
            }
            
            // 순위 재계산
            RecalculateRanks(category);
        }
    }
    
    // [SEQUENCE: MVP13-58] End current season
    void EndCurrentSeason() {
        if (!current_season_.has_value()) return;
        
        // 시즌 종료 보상 지급
        DistributeSeasonRewards();
        
        // 시즌 통계 저장
        SaveSeasonStatistics();
        
        // 시즌 랭킹 아카이브
        ArchiveSeasonRankings();
        
        spdlog::info("Ended season: {} (ID: {})", 
                    current_season_->season_name, 
                    current_season_->season_id);
    }
    
    // Event callbacks
    std::function<void(uint64_t, RankingCategory, RankingTier, RankingTier)> OnTierChange;
    std::function<void(uint64_t, RankingTier)> GrantTierRewards;
    std::function<void()> DistributeSeasonRewards;
    std::function<void()> SaveSeasonStatistics;
    std::function<void()> ArchiveSeasonRankings;
    
    // Utility functions
    std::string GetCategoryName(RankingCategory category) const {
        static const std::unordered_map<RankingCategory, std::string> names = {
            {RankingCategory::ARENA_1V1, "1v1 Arena"},
            {RankingCategory::ARENA_2V2, "2v2 Arena"},
            {RankingCategory::ARENA_3V3, "3v3 Arena"},
            {RankingCategory::BATTLEGROUND, "Battleground"},
            {RankingCategory::GUILD_WARS, "Guild Wars"},
            {RankingCategory::OVERALL_PVP, "Overall PvP"}
        };
        
        auto it = names.find(category);
        return it != names.end() ? it->second : "Unknown";
    }
    
    std::chrono::system_clock::time_point GetNextMidnight() const {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm* tm = std::localtime(&time_t);
        
        tm->tm_hour = 0;
        tm->tm_min = 0;
        tm->tm_sec = 0;
        tm->tm_mday += 1; // 다음날
        
        return std::chrono::system_clock::from_time_t(std::mktime(tm));
    }
};

// [SEQUENCE: MVP13-59] Ranking statistics and analytics
class RankingAnalytics {
public:
    // [SEQUENCE: MVP13-60] Rating progression tracking
    struct RatingProgression {
        uint64_t player_id;
        RankingCategory category;
        std::vector<std::pair<std::chrono::system_clock::time_point, int32_t>> history;
        
        void AddDataPoint(int32_t rating) {
            history.emplace_back(std::chrono::system_clock::now(), rating);
            
            // 최대 30일치 데이터만 유지
            if (history.size() > 720) { // 30일 * 24시간
                history.erase(history.begin());
            }
        }
        
        // Calculate trend (positive = improving, negative = declining)
        double CalculateTrend() const {
            if (history.size() < 2) return 0.0;
            
            // 선형 회귀를 사용한 트렌드 계산
            double sum_x = 0, sum_y = 0, sum_xy = 0, sum_xx = 0;
            size_t n = history.size();
            
            for (size_t i = 0; i < n; ++i) {
                double x = static_cast<double>(i);
                double y = history[i].second;
                
                sum_x += x;
                sum_y += y;
                sum_xy += x * y;
                sum_xx += x * x;
            }
            
            double slope = (n * sum_xy - sum_x * sum_y) / (n * sum_xx - sum_x * sum_x);
            return slope;
        }
    };
    
    // [SEQUENCE: MVP13-61] Match history analysis
    struct MatchHistory {
        struct Match {
            std::chrono::system_clock::time_point timestamp;
            bool won;
            int32_t rating_change;
            uint64_t opponent_id;
            int32_t opponent_rating;
            std::chrono::seconds match_duration;
        };
        
        std::deque<Match> recent_matches;
        
        void AddMatch(const Match& match) {
            recent_matches.push_back(match);
            
            // 최근 100경기만 유지
            if (recent_matches.size() > 100) {
                recent_matches.pop_front();
            }
        }
        
        // Get performance against different rating ranges
        std::map<std::string, double> GetPerformanceByRatingRange() const {
            std::map<std::string, std::pair<int, int>> ranges; // wins, total
            
            for (const auto& match : recent_matches) {
                std::string range;
                int32_t diff = match.opponent_rating - (match.rating_change > 0 ? 
                                match.opponent_rating - match.rating_change : 
                                match.opponent_rating + match.rating_change);
                
                if (diff < -200) range = "Much Lower";
                else if (diff < -50) range = "Lower";
                else if (diff < 50) range = "Similar";
                else if (diff < 200) range = "Higher";
                else range = "Much Higher";
                
                ranges[range].second++;
                if (match.won) ranges[range].first++;
            }
            
            std::map<std::string, double> performance;
            for (const auto& [range, stats] : ranges) {
                performance[range] = stats.second > 0 ? 
                    static_cast<double>(stats.first) / stats.second : 0.0;
            }
            
            return performance;
        }
    };
    
    // [SEQUENCE: MVP13-62] Generate player report
    struct PlayerReport {
        PlayerRankInfo rank_info;
        RatingProgression progression;
        MatchHistory match_history;
        
        std::string strengths;
        std::string weaknesses;
        std::string recommendations;
        
        void GenerateAnalysis() {
            // 강점 분석
            if (rank_info.stats.kd_ratio > 2.0) {
                strengths += "Excellent K/D ratio. ";
            }
            if (rank_info.rank_data.win_rate > 0.6) {
                strengths += "High win rate. ";
            }
            if (rank_info.rank_data.best_win_streak > 10) {
                strengths += "Strong consistency with long win streaks. ";
            }
            
            // 약점 분석
            auto trend = progression.CalculateTrend();
            if (trend < -5.0) {
                weaknesses += "Rating declining - consider reviewing recent gameplay. ";
            }
            
            auto perf_by_rating = match_history.GetPerformanceByRatingRange();
            if (perf_by_rating["Higher"] < 0.3) {
                weaknesses += "Struggles against higher-rated opponents. ";
            }
            
            // 추천사항
            if (rank_info.rank_data.losses > rank_info.rank_data.wins * 1.5) {
                recommendations += "Focus on fundamentals and consider practice matches. ";
            }
            if (match_history.recent_matches.size() < 10) {
                recommendations += "Play more matches to stabilize rating. ";
            }
        }
    };
    
    // [SEQUENCE: MVP13-63] Season statistics
    struct SeasonStatistics {
        uint32_t season_id;
        std::unordered_map<RankingTier, uint32_t> final_distribution;
        
        struct TopPlayer {
            uint64_t player_id;
            std::string player_name;
            int32_t final_rating;
            uint32_t total_matches;
            double win_rate;
        };
        
        std::vector<TopPlayer> top_100_players;
        
        // Interesting statistics
        uint64_t most_improved_player_id;
        int32_t largest_rating_gain;
        
        uint64_t most_active_player_id;
        uint32_t most_matches_played;
        
        uint64_t highest_win_streak_player_id;
        uint32_t longest_win_streak;
        
        // Meta statistics
        std::unordered_map<std::string, uint32_t> class_distribution;
        std::unordered_map<std::string, double> class_win_rates;
        
        void CalculateMetaStatistics(const std::vector<PlayerRankInfo>& all_players) {
            for (const auto& player : all_players) {
                class_distribution[player.class_name]++;
                
                if (player.rank_data.wins + player.rank_data.losses > 0) {
                    class_win_rates[player.class_name] += player.rank_data.win_rate;
                }
            }
            
            // 평균 승률 계산
            for (auto& [class_name, total_wr] : class_win_rates) {
                if (class_distribution[class_name] > 0) {
                    class_win_rates[class_name] = total_wr / class_distribution[class_name];
                }
            }
        }
    };
};