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
#include <functional>
#include <deque>
#include <set>
#include <spdlog/spdlog.h>
#include "ranking_system.h"

namespace mmorpg::ranking {

// [SEQUENCE: 2734] Comprehensive leaderboard system for displaying rankings
// 종합 리더보드 시스템 - 순위 표시 및 관리

// [SEQUENCE: 2735] Leaderboard types
enum class LeaderboardType {
    GLOBAL,                 // 전체 서버 리더보드
    REGIONAL,               // 지역별 리더보드
    FRIENDS,                // 친구 리더보드
    GUILD,                  // 길드 리더보드
    CLASS_SPECIFIC,         // 직업별 리더보드
    LEVEL_BRACKET,          // 레벨 구간별 리더보드
    CUSTOM                  // 커스텀 리더보드
};

// [SEQUENCE: 2736] Leaderboard display modes
enum class DisplayMode {
    STANDARD,               // 표준 표시 (순위, 이름, 레이팅)
    DETAILED,               // 상세 표시 (통계 포함)
    COMPACT,                // 간략 표시 (모바일용)
    COMPARISON,             // 비교 모드
    HEATMAP                 // 히트맵 표시
};

// [SEQUENCE: 2737] Leaderboard entry with display data
struct LeaderboardEntry {
    // [SEQUENCE: 2738] Basic information
    uint32_t rank;
    uint32_t previous_rank;
    int32_t rank_change;            // 순위 변동
    
    uint64_t player_id;
    std::string player_name;
    std::string guild_name;
    std::string guild_tag;          // 길드 태그 (짧은 이름)
    
    // [SEQUENCE: 2739] Visual elements
    std::string class_icon_url;     // 직업 아이콘 URL
    std::string tier_icon_url;      // 티어 아이콘 URL
    std::string country_flag_url;   // 국가 깃발 URL
    std::string avatar_url;         // 플레이어 아바타 URL
    
    // [SEQUENCE: 2740] Rating and performance
    int32_t rating;
    int32_t peak_rating;
    std::string tier_name;
    RankingTier tier;
    
    uint32_t matches_played;
    uint32_t wins;
    uint32_t losses;
    double win_rate;
    
    // [SEQUENCE: 2741] Recent performance
    std::vector<bool> recent_matches;    // 최근 5경기 (true=승리)
    int32_t rating_trend;                // 최근 레이팅 변화 추세
    std::string trend_indicator;         // "↑", "↓", "→"
    
    // [SEQUENCE: 2742] Activity status
    bool is_online;
    bool is_in_match;
    std::chrono::system_clock::time_point last_seen;
    std::string activity_status;        // "온라인", "경기 중", "5분 전"
    
    // [SEQUENCE: 2743] Achievements and badges
    std::vector<std::string> badge_urls;    // 업적 배지 URL들
    std::string special_title;              // 특별 칭호
    bool is_season_champion;                // 지난 시즌 챔피언
    bool is_verified;                       // 인증된 플레이어
    
    // Comparison data (for comparison mode)
    std::optional<int32_t> rating_difference;     // 나와의 레이팅 차이
    std::optional<double> win_rate_difference;    // 나와의 승률 차이
};

// [SEQUENCE: 2744] Leaderboard configuration
struct LeaderboardConfig {
    LeaderboardType type;
    RankingCategory category;
    RankingPeriod period;
    DisplayMode display_mode;
    
    // [SEQUENCE: 2745] Filtering options
    struct Filters {
        std::optional<std::string> region;          // 지역 필터
        std::optional<std::string> class_name;      // 직업 필터
        std::optional<RankingTier> min_tier;        // 최소 티어
        std::optional<RankingTier> max_tier;        // 최대 티어
        std::optional<uint32_t> min_level;          // 최소 레벨
        std::optional<uint32_t> max_level;          // 최대 레벨
        bool online_only{false};                    // 온라인 플레이어만
        bool active_only{false};                    // 활성 플레이어만 (7일 이내)
    } filters;
    
    // [SEQUENCE: 2746] Display options
    struct DisplayOptions {
        uint32_t entries_per_page{20};              // 페이지당 항목 수
        bool show_offline_status{true};             // 오프라인 상태 표시
        bool show_rating_changes{true};             // 레이팅 변화 표시
        bool show_recent_matches{true};             // 최근 경기 결과 표시
        bool show_badges{true};                     // 배지 표시
        bool highlight_friends{true};               // 친구 강조
        bool highlight_guild_members{true};         // 길드원 강조
    } display_options;
    
    // [SEQUENCE: 2747] Sorting options
    enum class SortBy {
        RANK,               // 순위순 (기본)
        RATING,             // 레이팅순
        WIN_RATE,           // 승률순
        MATCHES_PLAYED,     // 경기수순
        RECENT_ACTIVITY,    // 최근 활동순
        ALPHABETICAL        // 알파벳순
    };
    
    SortBy sort_by{SortBy::RANK};
    bool sort_ascending{true};
};

// [SEQUENCE: 2748] Leaderboard service
class LeaderboardService {
public:
    LeaderboardService(std::shared_ptr<RankingService> ranking_service)
        : ranking_service_(ranking_service) {
        InitializeCache();
        StartCacheWorker();
    }
    
    ~LeaderboardService() {
        StopCacheWorker();
    }
    
    // [SEQUENCE: 2749] Get leaderboard page
    struct LeaderboardPage {
        std::vector<LeaderboardEntry> entries;
        uint32_t current_page;
        uint32_t total_pages;
        uint32_t total_entries;
        
        // [SEQUENCE: 2750] Page metadata
        struct Metadata {
            std::chrono::system_clock::time_point last_updated;
            std::chrono::seconds update_interval{60};
            
            // Statistics
            double average_rating;
            double average_win_rate;
            RankingTier most_common_tier;
            
            // Distribution
            std::unordered_map<RankingTier, uint32_t> tier_distribution;
            std::unordered_map<std::string, uint32_t> class_distribution;
        } metadata;
        
        // Navigation helpers
        bool HasPreviousPage() const { return current_page > 0; }
        bool HasNextPage() const { return current_page < total_pages - 1; }
    };
    
    LeaderboardPage GetLeaderboard(const LeaderboardConfig& config, 
                                  uint32_t page = 0) {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        // [SEQUENCE: 2751] Check cache first
        auto cache_key = GenerateCacheKey(config, page);
        auto cache_it = leaderboard_cache_.find(cache_key);
        
        if (cache_it != leaderboard_cache_.end()) {
            auto& cached = cache_it->second;
            auto age = std::chrono::steady_clock::now() - cached.timestamp;
            
            if (age < std::chrono::seconds(cache_ttl_seconds_)) {
                return cached.page;
            }
        }
        
        // [SEQUENCE: 2752] Build fresh leaderboard
        LeaderboardPage page_data = BuildLeaderboard(config, page);
        
        // Cache the result
        leaderboard_cache_[cache_key] = {
            .page = page_data,
            .timestamp = std::chrono::steady_clock::now()
        };
        
        return page_data;
    }
    
    // [SEQUENCE: 2753] Get player's position in leaderboard
    struct PlayerPosition {
        uint32_t rank;
        uint32_t page;
        std::vector<LeaderboardEntry> surrounding_entries; // ±5 entries
        
        // Percentile information
        double percentile;          // Top X%
        std::string percentile_text; // "Top 1%"
        
        // Distance to next tier
        RankingTier current_tier;
        RankingTier next_tier;
        int32_t points_to_next_tier;
        uint32_t players_to_surpass; // Players to beat for next tier
    };
    
    std::optional<PlayerPosition> GetPlayerPosition(
        uint64_t player_id,
        const LeaderboardConfig& config) {
        
        auto rank_info = ranking_service_->GetPlayerRank(
            player_id, config.category);
        
        if (!rank_info.has_value()) {
            return std::nullopt;
        }
        
        PlayerPosition position;
        position.rank = rank_info->rank_data.rank;
        position.page = (position.rank - 1) / config.display_options.entries_per_page;
        
        // [SEQUENCE: 2754] Get surrounding entries
        int32_t start_rank = std::max(1, static_cast<int32_t>(position.rank) - 5);
        int32_t end_rank = position.rank + 5;
        
        auto all_players = ranking_service_->GetTopPlayers(
            config.category, end_rank);
        
        for (size_t i = start_rank - 1; i < std::min(all_players.size(), 
                                                     static_cast<size_t>(end_rank)); ++i) {
            position.surrounding_entries.push_back(
                ConvertToLeaderboardEntry(all_players[i], config));
        }
        
        // [SEQUENCE: 2755] Calculate percentile
        auto total_players = GetTotalPlayers(config);
        if (total_players > 0) {
            position.percentile = (1.0 - static_cast<double>(position.rank) / 
                                 total_players) * 100.0;
            
            if (position.percentile >= 99.0) {
                position.percentile_text = "Top 1%";
            } else if (position.percentile >= 95.0) {
                position.percentile_text = "Top 5%";
            } else if (position.percentile >= 90.0) {
                position.percentile_text = "Top 10%";
            } else {
                position.percentile_text = "Top " + 
                    std::to_string(100 - static_cast<int>(position.percentile)) + "%";
            }
        }
        
        // Calculate tier progression
        position.current_tier = ranking_service_->GetPlayerTier(player_id, config.category);
        position.next_tier = static_cast<RankingTier>(
            static_cast<int>(position.current_tier) + 1);
        
        return position;
    }
    
    // [SEQUENCE: 2756] Search leaderboard
    std::vector<LeaderboardEntry> SearchLeaderboard(
        const std::string& query,
        const LeaderboardConfig& config,
        uint32_t max_results = 20) {
        
        std::vector<LeaderboardEntry> results;
        
        // Search by player name
        auto name_matches = ranking_service_->SearchRankings(
            config.category, query);
        
        for (const auto& player : name_matches) {
            if (MatchesFilters(player, config.filters)) {
                results.push_back(ConvertToLeaderboardEntry(player, config));
                
                if (results.size() >= max_results) {
                    break;
                }
            }
        }
        
        // Also search by guild name if no player matches
        if (results.empty()) {
            auto all_players = ranking_service_->GetTopPlayers(
                config.category, 1000);
            
            for (const auto& player : all_players) {
                if (player.guild_name.find(query) != std::string::npos &&
                    MatchesFilters(player, config.filters)) {
                    results.push_back(ConvertToLeaderboardEntry(player, config));
                    
                    if (results.size() >= max_results) {
                        break;
                    }
                }
            }
        }
        
        return results;
    }
    
    // [SEQUENCE: 2757] Get friends leaderboard
    std::vector<LeaderboardEntry> GetFriendsLeaderboard(
        uint64_t player_id,
        const std::vector<uint64_t>& friend_ids,
        const LeaderboardConfig& config) {
        
        std::vector<LeaderboardEntry> entries;
        
        // Add self
        auto self_rank = ranking_service_->GetPlayerRank(player_id, config.category);
        if (self_rank.has_value()) {
            auto entry = ConvertToLeaderboardEntry(self_rank.value(), config);
            entry.is_verified = true; // Mark self
            entries.push_back(entry);
        }
        
        // Add friends
        for (uint64_t friend_id : friend_ids) {
            auto friend_rank = ranking_service_->GetPlayerRank(friend_id, config.category);
            if (friend_rank.has_value()) {
                entries.push_back(ConvertToLeaderboardEntry(friend_rank.value(), config));
            }
        }
        
        // Sort by rank
        std::sort(entries.begin(), entries.end(),
            [](const LeaderboardEntry& a, const LeaderboardEntry& b) {
                return a.rank < b.rank;
            });
        
        return entries;
    }
    
    // [SEQUENCE: 2758] Get guild leaderboard
    std::vector<LeaderboardEntry> GetGuildLeaderboard(
        const std::string& guild_name,
        const LeaderboardConfig& config) {
        
        std::vector<LeaderboardEntry> entries;
        
        // Get all guild members from top rankings
        auto top_players = ranking_service_->GetTopPlayers(config.category, 10000);
        
        for (const auto& player : top_players) {
            if (player.guild_name == guild_name) {
                entries.push_back(ConvertToLeaderboardEntry(player, config));
            }
        }
        
        return entries;
    }
    
    // [SEQUENCE: 2759] Get leaderboard statistics
    struct LeaderboardStatistics {
        uint32_t total_ranked_players;
        uint32_t active_players_24h;
        uint32_t active_players_7d;
        
        // [SEQUENCE: 2760] Rating statistics
        struct RatingStats {
            int32_t highest_rating;
            int32_t lowest_rating;
            double average_rating;
            double median_rating;
            double standard_deviation;
        } rating_stats;
        
        // [SEQUENCE: 2761] Activity statistics
        struct ActivityStats {
            uint32_t total_matches_24h;
            uint32_t unique_players_24h;
            double average_matches_per_player;
            
            std::map<uint32_t, uint32_t> matches_by_hour; // Hour -> match count
            uint32_t peak_hour;
            uint32_t quiet_hour;
        } activity_stats;
        
        // [SEQUENCE: 2762] Class balance
        struct ClassBalance {
            std::unordered_map<std::string, uint32_t> class_counts;
            std::unordered_map<std::string, double> class_win_rates;
            std::unordered_map<std::string, double> class_avg_ratings;
            
            std::string most_played_class;
            std::string highest_win_rate_class;
            std::string highest_rated_class;
        } class_balance;
        
        // Tier progression
        std::unordered_map<RankingTier, uint32_t> tier_distribution;
        std::unordered_map<RankingTier, double> tier_promotion_rates;
    };
    
    LeaderboardStatistics GetStatistics(RankingCategory category) {
        LeaderboardStatistics stats;
        
        auto all_players = ranking_service_->GetTopPlayers(category, 100000);
        stats.total_ranked_players = all_players.size();
        
        // [SEQUENCE: 2763] Calculate various statistics
        CalculateRatingStats(all_players, stats.rating_stats);
        CalculateActivityStats(all_players, stats.activity_stats);
        CalculateClassBalance(all_players, stats.class_balance);
        
        // Tier distribution
        stats.tier_distribution = ranking_service_->GetTierDistribution(category);
        
        return stats;
    }
    
    // [SEQUENCE: 2764] Export leaderboard data
    void ExportLeaderboard(const LeaderboardConfig& config,
                          const std::string& format,
                          const std::string& filename) {
        if (format == "csv") {
            ExportToCSV(config, filename);
        } else if (format == "json") {
            ExportToJSON(config, filename);
        } else if (format == "html") {
            ExportToHTML(config, filename);
        } else {
            spdlog::error("Unsupported export format: {}", format);
        }
    }
    
private:
    std::shared_ptr<RankingService> ranking_service_;
    
    // [SEQUENCE: 2765] Cache system
    struct CachedLeaderboard {
        LeaderboardPage page;
        std::chrono::steady_clock::time_point timestamp;
    };
    
    mutable std::mutex cache_mutex_;
    std::unordered_map<std::string, CachedLeaderboard> leaderboard_cache_;
    uint32_t cache_ttl_seconds_{60}; // 1 minute TTL
    
    std::atomic<bool> cache_worker_running_{false};
    std::thread cache_worker_thread_;
    
    // [SEQUENCE: 2766] Initialize cache system
    void InitializeCache() {
        // Pre-populate cache for common leaderboards
        std::vector<LeaderboardConfig> common_configs = {
            {
                .type = LeaderboardType::GLOBAL,
                .category = RankingCategory::ARENA_3V3,
                .period = RankingPeriod::ALL_TIME,
                .display_mode = DisplayMode::STANDARD
            },
            {
                .type = LeaderboardType::GLOBAL,
                .category = RankingCategory::BATTLEGROUND,
                .period = RankingPeriod::WEEKLY,
                .display_mode = DisplayMode::STANDARD
            }
        };
        
        for (const auto& config : common_configs) {
            GetLeaderboard(config, 0); // Populate cache
        }
    }
    
    // [SEQUENCE: 2767] Cache maintenance worker
    void StartCacheWorker() {
        cache_worker_running_ = true;
        cache_worker_thread_ = std::thread([this]() {
            while (cache_worker_running_) {
                std::this_thread::sleep_for(std::chrono::minutes(5));
                
                if (!cache_worker_running_) break;
                
                CleanExpiredCache();
                RefreshPopularLeaderboards();
            }
        });
    }
    
    void StopCacheWorker() {
        cache_worker_running_ = false;
        if (cache_worker_thread_.joinable()) {
            cache_worker_thread_.join();
        }
    }
    
    void CleanExpiredCache() {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        auto now = std::chrono::steady_clock::now();
        auto it = leaderboard_cache_.begin();
        
        while (it != leaderboard_cache_.end()) {
            auto age = now - it->second.timestamp;
            if (age > std::chrono::seconds(cache_ttl_seconds_ * 10)) {
                it = leaderboard_cache_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    void RefreshPopularLeaderboards() {
        // Refresh frequently accessed leaderboards
        // This would track access patterns and refresh popular ones
    }
    
    // [SEQUENCE: 2768] Build leaderboard from ranking data
    LeaderboardPage BuildLeaderboard(const LeaderboardConfig& config, 
                                    uint32_t page) {
        LeaderboardPage page_data;
        page_data.current_page = page;
        
        // Get all players for the category
        auto all_players = GetFilteredPlayers(config);
        
        // Sort according to config
        SortPlayers(all_players, config);
        
        // Calculate pagination
        page_data.total_entries = all_players.size();
        page_data.total_pages = (page_data.total_entries + 
                               config.display_options.entries_per_page - 1) / 
                               config.display_options.entries_per_page;
        
        // Extract page entries
        uint32_t start_idx = page * config.display_options.entries_per_page;
        uint32_t end_idx = std::min(start_idx + config.display_options.entries_per_page,
                                   static_cast<uint32_t>(all_players.size()));
        
        for (uint32_t i = start_idx; i < end_idx; ++i) {
            page_data.entries.push_back(
                ConvertToLeaderboardEntry(all_players[i], config));
        }
        
        // Calculate metadata
        CalculatePageMetadata(page_data, all_players);
        
        return page_data;
    }
    
    // [SEQUENCE: 2769] Get filtered players
    std::vector<PlayerRankInfo> GetFilteredPlayers(const LeaderboardConfig& config) {
        auto all_players = ranking_service_->GetTopPlayers(config.category, 100000);
        std::vector<PlayerRankInfo> filtered;
        
        for (const auto& player : all_players) {
            if (MatchesFilters(player, config.filters)) {
                filtered.push_back(player);
            }
        }
        
        return filtered;
    }
    
    // [SEQUENCE: 2770] Check if player matches filters
    bool MatchesFilters(const PlayerRankInfo& player,
                       const LeaderboardConfig::Filters& filters) {
        // Class filter
        if (filters.class_name.has_value() && 
            player.class_name != filters.class_name.value()) {
            return false;
        }
        
        // Level filter
        if (filters.min_level.has_value() && 
            player.level < filters.min_level.value()) {
            return false;
        }
        
        if (filters.max_level.has_value() && 
            player.level > filters.max_level.value()) {
            return false;
        }
        
        // Tier filter
        auto tier = ranking_service_->GetPlayerTier(player.player_id, 
                                                   RankingCategory::ARENA_3V3);
        
        if (filters.min_tier.has_value() && tier < filters.min_tier.value()) {
            return false;
        }
        
        if (filters.max_tier.has_value() && tier > filters.max_tier.value()) {
            return false;
        }
        
        // Activity filter
        if (filters.active_only) {
            auto days_since = std::chrono::duration_cast<std::chrono::days>(
                std::chrono::system_clock::now() - player.rank_data.last_update
            ).count();
            
            if (days_since > 7) {
                return false;
            }
        }
        
        return true;
    }
    
    // [SEQUENCE: 2771] Sort players according to config
    void SortPlayers(std::vector<PlayerRankInfo>& players,
                    const LeaderboardConfig& config) {
        switch (config.sort_by) {
            case LeaderboardConfig::SortBy::RATING:
                std::sort(players.begin(), players.end(),
                    [&](const PlayerRankInfo& a, const PlayerRankInfo& b) {
                        return config.sort_ascending ? 
                            a.rank_data.rating < b.rank_data.rating :
                            a.rank_data.rating > b.rank_data.rating;
                    });
                break;
                
            case LeaderboardConfig::SortBy::WIN_RATE:
                std::sort(players.begin(), players.end(),
                    [&](const PlayerRankInfo& a, const PlayerRankInfo& b) {
                        return config.sort_ascending ?
                            a.rank_data.win_rate < b.rank_data.win_rate :
                            a.rank_data.win_rate > b.rank_data.win_rate;
                    });
                break;
                
            case LeaderboardConfig::SortBy::MATCHES_PLAYED:
                std::sort(players.begin(), players.end(),
                    [&](const PlayerRankInfo& a, const PlayerRankInfo& b) {
                        uint32_t a_matches = a.rank_data.wins + a.rank_data.losses;
                        uint32_t b_matches = b.rank_data.wins + b.rank_data.losses;
                        return config.sort_ascending ?
                            a_matches < b_matches : a_matches > b_matches;
                    });
                break;
                
            case LeaderboardConfig::SortBy::RECENT_ACTIVITY:
                std::sort(players.begin(), players.end(),
                    [&](const PlayerRankInfo& a, const PlayerRankInfo& b) {
                        return config.sort_ascending ?
                            a.rank_data.last_update < b.rank_data.last_update :
                            a.rank_data.last_update > b.rank_data.last_update;
                    });
                break;
                
            case LeaderboardConfig::SortBy::ALPHABETICAL:
                std::sort(players.begin(), players.end(),
                    [&](const PlayerRankInfo& a, const PlayerRankInfo& b) {
                        return config.sort_ascending ?
                            a.player_name < b.player_name :
                            a.player_name > b.player_name;
                    });
                break;
                
            case LeaderboardConfig::SortBy::RANK:
            default:
                // Already sorted by rank from ranking service
                if (!config.sort_ascending) {
                    std::reverse(players.begin(), players.end());
                }
                break;
        }
    }
    
    // [SEQUENCE: 2772] Convert rank info to leaderboard entry
    LeaderboardEntry ConvertToLeaderboardEntry(const PlayerRankInfo& rank_info,
                                              const LeaderboardConfig& config) {
        LeaderboardEntry entry;
        
        // Basic information
        entry.rank = rank_info.rank_data.rank;
        entry.previous_rank = rank_info.rank_data.previous_rank;
        entry.rank_change = rank_info.rank_data.rank_change;
        
        entry.player_id = rank_info.player_id;
        entry.player_name = rank_info.player_name;
        entry.guild_name = rank_info.guild_name;
        entry.guild_tag = GetGuildTag(rank_info.guild_name);
        
        // Visual elements
        entry.class_icon_url = "assets/icons/classes/" + rank_info.class_name + ".png";
        entry.tier = ranking_service_->GetPlayerTier(rank_info.player_id, config.category);
        entry.tier_name = GetTierName(entry.tier);
        entry.tier_icon_url = "assets/icons/tiers/" + entry.tier_name + ".png";
        entry.avatar_url = "api/avatar/" + std::to_string(rank_info.player_id);
        
        // Rating and performance
        entry.rating = rank_info.rank_data.rating;
        entry.peak_rating = rank_info.rank_data.peak_rating;
        entry.matches_played = rank_info.rank_data.wins + rank_info.rank_data.losses;
        entry.wins = rank_info.rank_data.wins;
        entry.losses = rank_info.rank_data.losses;
        entry.win_rate = rank_info.rank_data.win_rate;
        
        // Recent performance (would come from match history)
        // For now, generate sample data
        GenerateRecentMatches(entry);
        
        // Rating trend
        entry.rating_trend = CalculateRatingTrend(rank_info);
        if (entry.rating_trend > 0) {
            entry.trend_indicator = "↑";
        } else if (entry.rating_trend < 0) {
            entry.trend_indicator = "↓";
        } else {
            entry.trend_indicator = "→";
        }
        
        // Activity status
        UpdateActivityStatus(entry, rank_info);
        
        // Achievements and badges
        PopulateAchievements(entry, rank_info);
        
        return entry;
    }
    
    // [SEQUENCE: 2773] Calculate page metadata
    void CalculatePageMetadata(LeaderboardPage& page,
                              const std::vector<PlayerRankInfo>& all_players) {
        page.metadata.last_updated = std::chrono::system_clock::now();
        
        if (all_players.empty()) return;
        
        // Calculate statistics
        double total_rating = 0;
        double total_win_rate = 0;
        std::unordered_map<RankingTier, uint32_t> tier_counts;
        std::unordered_map<std::string, uint32_t> class_counts;
        
        for (const auto& player : all_players) {
            total_rating += player.rank_data.rating;
            total_win_rate += player.rank_data.win_rate;
            
            auto tier = ranking_service_->GetPlayerTier(player.player_id, 
                                                       RankingCategory::ARENA_3V3);
            tier_counts[tier]++;
            class_counts[player.class_name]++;
        }
        
        page.metadata.average_rating = total_rating / all_players.size();
        page.metadata.average_win_rate = total_win_rate / all_players.size();
        
        // Find most common tier
        auto max_tier_it = std::max_element(tier_counts.begin(), tier_counts.end(),
            [](const auto& a, const auto& b) {
                return a.second < b.second;
            });
        
        if (max_tier_it != tier_counts.end()) {
            page.metadata.most_common_tier = max_tier_it->first;
        }
        
        page.metadata.tier_distribution = tier_counts;
        page.metadata.class_distribution = class_counts;
    }
    
    // [SEQUENCE: 2774] Helper functions
    std::string GenerateCacheKey(const LeaderboardConfig& config, uint32_t page) {
        return std::to_string(static_cast<int>(config.type)) + "_" +
               std::to_string(static_cast<int>(config.category)) + "_" +
               std::to_string(static_cast<int>(config.period)) + "_" +
               std::to_string(page);
    }
    
    std::string GetGuildTag(const std::string& guild_name) {
        // Extract guild tag from name or generate one
        if (guild_name.empty()) return "";
        
        // Simple approach: first 3-4 characters
        return guild_name.substr(0, std::min(static_cast<size_t>(4), guild_name.length()));
    }
    
    std::string GetTierName(RankingTier tier) {
        static const std::unordered_map<RankingTier, std::string> tier_names = {
            {RankingTier::BRONZE, "bronze"},
            {RankingTier::SILVER, "silver"},
            {RankingTier::GOLD, "gold"},
            {RankingTier::PLATINUM, "platinum"},
            {RankingTier::DIAMOND, "diamond"},
            {RankingTier::MASTER, "master"},
            {RankingTier::GRANDMASTER, "grandmaster"},
            {RankingTier::CHALLENGER, "challenger"}
        };
        
        auto it = tier_names.find(tier);
        return it != tier_names.end() ? it->second : "unranked";
    }
    
    void GenerateRecentMatches(LeaderboardEntry& entry) {
        // In real implementation, this would fetch from match history
        // For now, generate based on win rate
        std::random_device rd;
        std::mt19937 gen(rd());
        std::bernoulli_distribution dist(entry.win_rate);
        
        for (int i = 0; i < 5; ++i) {
            entry.recent_matches.push_back(dist(gen));
        }
    }
    
    int32_t CalculateRatingTrend(const PlayerRankInfo& rank_info) {
        // In real implementation, would analyze rating history
        // For now, use rank change as indicator
        return -rank_info.rank_data.rank_change * 10; // Negative because lower rank is better
    }
    
    void UpdateActivityStatus(LeaderboardEntry& entry,
                             const PlayerRankInfo& rank_info) {
        // Check if online (would query game server)
        entry.is_online = false; // Placeholder
        entry.is_in_match = false; // Placeholder
        entry.last_seen = rank_info.rank_data.last_update;
        
        auto now = std::chrono::system_clock::now();
        auto time_diff = now - entry.last_seen;
        
        if (entry.is_online) {
            entry.activity_status = "온라인";
            if (entry.is_in_match) {
                entry.activity_status = "경기 중";
            }
        } else {
            auto minutes = std::chrono::duration_cast<std::chrono::minutes>(time_diff).count();
            auto hours = std::chrono::duration_cast<std::chrono::hours>(time_diff).count();
            auto days = std::chrono::duration_cast<std::chrono::days>(time_diff).count();
            
            if (minutes < 60) {
                entry.activity_status = std::to_string(minutes) + "분 전";
            } else if (hours < 24) {
                entry.activity_status = std::to_string(hours) + "시간 전";
            } else {
                entry.activity_status = std::to_string(days) + "일 전";
            }
        }
    }
    
    void PopulateAchievements(LeaderboardEntry& entry,
                             const PlayerRankInfo& rank_info) {
        // Season champion badge
        if (rank_info.rank_data.previous_rank == 1) {
            entry.badge_urls.push_back("assets/badges/season_champion.png");
            entry.is_season_champion = true;
        }
        
        // Tier badges
        if (entry.tier >= RankingTier::MASTER) {
            entry.badge_urls.push_back("assets/badges/tier_" + GetTierName(entry.tier) + ".png");
        }
        
        // Win streak badges
        if (rank_info.rank_data.best_win_streak >= 20) {
            entry.badge_urls.push_back("assets/badges/win_streak_20.png");
        } else if (rank_info.rank_data.best_win_streak >= 10) {
            entry.badge_urls.push_back("assets/badges/win_streak_10.png");
        }
        
        // Perfect game badges
        if (rank_info.stats.perfect_games >= 100) {
            entry.badge_urls.push_back("assets/badges/perfect_100.png");
        }
        
        // Special title
        if (entry.tier == RankingTier::CHALLENGER && entry.rank <= 10) {
            entry.special_title = "Elite Challenger";
        }
    }
    
    uint32_t GetTotalPlayers(const LeaderboardConfig& config) {
        auto all_players = ranking_service_->GetTopPlayers(config.category, 100000);
        return all_players.size();
    }
    
    // [SEQUENCE: 2775] Statistics calculation
    void CalculateRatingStats(const std::vector<PlayerRankInfo>& players,
                            LeaderboardStatistics::RatingStats& stats) {
        if (players.empty()) return;
        
        stats.highest_rating = players[0].rank_data.rating; // Assuming sorted
        stats.lowest_rating = players.back().rank_data.rating;
        
        double total = 0;
        for (const auto& player : players) {
            total += player.rank_data.rating;
        }
        stats.average_rating = total / players.size();
        
        // Calculate standard deviation
        double variance = 0;
        for (const auto& player : players) {
            double diff = player.rank_data.rating - stats.average_rating;
            variance += diff * diff;
        }
        stats.standard_deviation = std::sqrt(variance / players.size());
        
        // Median
        if (players.size() % 2 == 0) {
            stats.median_rating = (players[players.size()/2 - 1].rank_data.rating + 
                                 players[players.size()/2].rank_data.rating) / 2.0;
        } else {
            stats.median_rating = players[players.size()/2].rank_data.rating;
        }
    }
    
    void CalculateActivityStats(const std::vector<PlayerRankInfo>& players,
                              LeaderboardStatistics::ActivityStats& stats) {
        auto now = std::chrono::system_clock::now();
        auto day_ago = now - std::chrono::hours(24);
        auto week_ago = now - std::chrono::days(7);
        
        for (const auto& player : players) {
            if (player.rank_data.last_update >= day_ago) {
                stats.unique_players_24h++;
            }
        }
        
        // Match distribution by hour would require match history data
        // This is placeholder logic
        for (int hour = 0; hour < 24; ++hour) {
            stats.matches_by_hour[hour] = 100 + (hour % 12) * 50; // Sample data
        }
        
        // Find peak and quiet hours
        auto peak = std::max_element(stats.matches_by_hour.begin(), 
                                   stats.matches_by_hour.end(),
            [](const auto& a, const auto& b) {
                return a.second < b.second;
            });
        
        if (peak != stats.matches_by_hour.end()) {
            stats.peak_hour = peak->first;
        }
    }
    
    void CalculateClassBalance(const std::vector<PlayerRankInfo>& players,
                             LeaderboardStatistics::ClassBalance& balance) {
        std::unordered_map<std::string, double> total_win_rates;
        std::unordered_map<std::string, double> total_ratings;
        
        for (const auto& player : players) {
            balance.class_counts[player.class_name]++;
            total_win_rates[player.class_name] += player.rank_data.win_rate;
            total_ratings[player.class_name] += player.rank_data.rating;
        }
        
        // Calculate averages
        for (const auto& [class_name, count] : balance.class_counts) {
            if (count > 0) {
                balance.class_win_rates[class_name] = total_win_rates[class_name] / count;
                balance.class_avg_ratings[class_name] = total_ratings[class_name] / count;
            }
        }
        
        // Find best performing classes
        auto most_played = std::max_element(balance.class_counts.begin(),
                                          balance.class_counts.end(),
            [](const auto& a, const auto& b) {
                return a.second < b.second;
            });
        
        if (most_played != balance.class_counts.end()) {
            balance.most_played_class = most_played->first;
        }
        
        auto highest_wr = std::max_element(balance.class_win_rates.begin(),
                                         balance.class_win_rates.end(),
            [](const auto& a, const auto& b) {
                return a.second < b.second;
            });
        
        if (highest_wr != balance.class_win_rates.end()) {
            balance.highest_win_rate_class = highest_wr->first;
        }
    }
    
    // [SEQUENCE: 2776] Export functions
    void ExportToCSV(const LeaderboardConfig& config, const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            spdlog::error("Failed to open file for CSV export: {}", filename);
            return;
        }
        
        // Header
        file << "Rank,Player Name,Guild,Rating,Wins,Losses,Win Rate,Tier\n";
        
        // Get all pages
        uint32_t page = 0;
        while (true) {
            auto page_data = GetLeaderboard(config, page);
            
            for (const auto& entry : page_data.entries) {
                file << entry.rank << ","
                     << "\"" << entry.player_name << "\","
                     << "\"" << entry.guild_name << "\","
                     << entry.rating << ","
                     << entry.wins << ","
                     << entry.losses << ","
                     << std::fixed << std::setprecision(2) << entry.win_rate * 100 << "%,"
                     << entry.tier_name << "\n";
            }
            
            if (!page_data.HasNextPage()) break;
            page++;
        }
        
        file.close();
        spdlog::info("Exported leaderboard to CSV: {}", filename);
    }
    
    void ExportToJSON(const LeaderboardConfig& config, const std::string& filename) {
        // JSON export implementation
    }
    
    void ExportToHTML(const LeaderboardConfig& config, const std::string& filename) {
        // HTML export implementation
    }
};

// [SEQUENCE: 2777] Leaderboard UI components
class LeaderboardUIComponents {
public:
    // [SEQUENCE: 2778] Generate HTML for leaderboard entry
    static std::string GenerateEntryHTML(const LeaderboardEntry& entry) {
        std::stringstream html;
        
        html << "<div class='leaderboard-entry";
        if (entry.is_online) html << " online";
        if (entry.is_in_match) html << " in-match";
        if (entry.is_verified) html << " verified";
        html << "'>\n";
        
        // Rank section
        html << "  <div class='rank-section'>\n";
        html << "    <span class='rank'>" << entry.rank << "</span>\n";
        if (entry.rank_change != 0) {
            html << "    <span class='rank-change " 
                 << (entry.rank_change > 0 ? "up" : "down") << "'>"
                 << entry.trend_indicator << std::abs(entry.rank_change) << "</span>\n";
        }
        html << "  </div>\n";
        
        // Player info section
        html << "  <div class='player-info'>\n";
        html << "    <img src='" << entry.avatar_url << "' class='avatar'>\n";
        html << "    <img src='" << entry.class_icon_url << "' class='class-icon'>\n";
        html << "    <div class='names'>\n";
        html << "      <span class='player-name'>" << entry.player_name << "</span>\n";
        if (!entry.guild_name.empty()) {
            html << "      <span class='guild-tag'>[" << entry.guild_tag << "]</span>\n";
        }
        if (!entry.special_title.empty()) {
            html << "      <span class='title'>" << entry.special_title << "</span>\n";
        }
        html << "    </div>\n";
        html << "  </div>\n";
        
        // Rating section
        html << "  <div class='rating-section'>\n";
        html << "    <img src='" << entry.tier_icon_url << "' class='tier-icon'>\n";
        html << "    <span class='rating'>" << entry.rating << "</span>\n";
        html << "    <span class='tier-name'>" << entry.tier_name << "</span>\n";
        html << "  </div>\n";
        
        // Stats section
        html << "  <div class='stats-section'>\n";
        html << "    <span class='wins'>" << entry.wins << "W</span>\n";
        html << "    <span class='losses'>" << entry.losses << "L</span>\n";
        html << "    <span class='winrate'>" << std::fixed << std::setprecision(1) 
             << entry.win_rate * 100 << "%</span>\n";
        html << "  </div>\n";
        
        // Recent matches
        if (!entry.recent_matches.empty()) {
            html << "  <div class='recent-matches'>\n";
            for (bool won : entry.recent_matches) {
                html << "    <span class='match-result " 
                     << (won ? "win" : "loss") << "'></span>\n";
            }
            html << "  </div>\n";
        }
        
        // Badges
        if (!entry.badge_urls.empty()) {
            html << "  <div class='badges'>\n";
            for (const auto& badge : entry.badge_urls) {
                html << "    <img src='" << badge << "' class='badge'>\n";
            }
            html << "  </div>\n";
        }
        
        // Activity status
        html << "  <div class='activity'>\n";
        html << "    <span class='status'>" << entry.activity_status << "</span>\n";
        html << "  </div>\n";
        
        html << "</div>\n";
        
        return html.str();
    }
    
    // [SEQUENCE: 2779] Generate tier distribution chart
    static std::string GenerateTierDistributionChart(
        const std::unordered_map<RankingTier, uint32_t>& distribution) {
        
        std::stringstream html;
        
        html << "<div class='tier-distribution-chart'>\n";
        
        // Calculate total for percentages
        uint32_t total = 0;
        for (const auto& [tier, count] : distribution) {
            total += count;
        }
        
        // Generate bars
        for (int i = static_cast<int>(RankingTier::CHALLENGER); 
             i >= static_cast<int>(RankingTier::BRONZE); --i) {
            
            auto tier = static_cast<RankingTier>(i);
            auto it = distribution.find(tier);
            uint32_t count = it != distribution.end() ? it->second : 0;
            double percentage = total > 0 ? (static_cast<double>(count) / total * 100) : 0;
            
            html << "  <div class='tier-bar'>\n";
            html << "    <span class='tier-label'>" << GetTierDisplayName(tier) << "</span>\n";
            html << "    <div class='bar-container'>\n";
            html << "      <div class='bar " << GetTierClassName(tier) 
                 << "' style='width: " << percentage << "%'></div>\n";
            html << "    </div>\n";
            html << "    <span class='percentage'>" << std::fixed << std::setprecision(1) 
                 << percentage << "%</span>\n";
            html << "    <span class='count'>(" << count << ")</span>\n";
            html << "  </div>\n";
        }
        
        html << "</div>\n";
        
        return html.str();
    }
    
private:
    static std::string GetTierDisplayName(RankingTier tier) {
        static const std::unordered_map<RankingTier, std::string> names = {
            {RankingTier::BRONZE, "Bronze"},
            {RankingTier::SILVER, "Silver"},
            {RankingTier::GOLD, "Gold"},
            {RankingTier::PLATINUM, "Platinum"},
            {RankingTier::DIAMOND, "Diamond"},
            {RankingTier::MASTER, "Master"},
            {RankingTier::GRANDMASTER, "Grandmaster"},
            {RankingTier::CHALLENGER, "Challenger"}
        };
        
        auto it = names.find(tier);
        return it != names.end() ? it->second : "Unranked";
    }
    
    static std::string GetTierClassName(RankingTier tier) {
        static const std::unordered_map<RankingTier, std::string> classes = {
            {RankingTier::BRONZE, "tier-bronze"},
            {RankingTier::SILVER, "tier-silver"},
            {RankingTier::GOLD, "tier-gold"},
            {RankingTier::PLATINUM, "tier-platinum"},
            {RankingTier::DIAMOND, "tier-diamond"},
            {RankingTier::MASTER, "tier-master"},
            {RankingTier::GRANDMASTER, "tier-grandmaster"},
            {RankingTier::CHALLENGER, "tier-challenger"}
        };
        
        auto it = classes.find(tier);
        return it != classes.end() ? it->second : "tier-unranked";
    }
};

} // namespace mmorpg::ranking