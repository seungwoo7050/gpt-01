#include "ranking_system.h"
#include <fstream>
#include <iomanip>

namespace mmorpg::ranking {

// [SEQUENCE: MVP12-86] Ranking system integration with game server
class RankingIntegration {
public:
    // [SEQUENCE: MVP12-87] Initialize ranking system with game server
    static void InitializeWithGameServer(GameServer* server, 
                                       RankingService* ranking_service) {
        
        // 티어 변경 시 알림 및 보상
        ranking_service->OnTierChange = [server](uint64_t player_id, 
                                               RankingCategory category,
                                               RankingTier old_tier, 
                                               RankingTier new_tier) {
            // 플레이어에게 티어 변경 알림
            TierChangePacket packet;
            packet.category = category;
            packet.old_tier = old_tier;
            packet.new_tier = new_tier;
            packet.is_promotion = new_tier > old_tier;
            
            server->SendPacket(player_id, packet);
            
            // 승급 축하 메시지
            if (packet.is_promotion) {
                std::string message = "Congratulations! You've been promoted to " + 
                                    GetTierName(new_tier) + "!";
                server->SendSystemMessage(player_id, message);
                
                // 서버 공지 (다이아몬드 이상)
                if (new_tier >= RankingTier::DIAMOND) {
                    std::string announcement = server->GetPlayerName(player_id) + 
                        " has reached " + GetTierName(new_tier) + " tier!";
                    server->BroadcastAnnouncement(announcement);
                }
            }
        };
        
        // [SEQUENCE: MVP12-88] Grant tier rewards
        ranking_service->GrantTierRewards = [server, ranking_service](
            uint64_t player_id, RankingTier tier) {
            
            auto tier_info = GetTierInfo(tier);
            if (!tier_info) return;
            
            const auto& rewards = tier_info->rewards;
            
            // 통화 보너스
            if (rewards.currency_bonus > 0) {
                server->GrantCurrency(player_id, CurrencyType::HONOR, 
                                    rewards.currency_bonus);
            }
            
            // 전용 아이템
            for (uint32_t item_id : rewards.exclusive_items) {
                server->GrantItem(player_id, item_id, 1);
            }
            
            // 전용 칭호
            if (!rewards.exclusive_title.empty()) {
                server->GrantTitle(player_id, rewards.exclusive_title);
            }
            
            // 시즌 탈것
            if (rewards.seasonal_mount) {
                uint32_t mount_id = GetSeasonalMountId(tier);
                server->GrantMount(player_id, mount_id);
            }
            
            spdlog::info("Granted tier {} rewards to player {}", 
                        GetTierName(tier), player_id);
        };
        
        // [SEQUENCE: MVP12-89] Distribute season rewards
        ranking_service->DistributeSeasonRewards = [server, ranking_service]() {
            // 모든 카테고리에 대해 시즌 보상 지급
            std::vector<RankingCategory> pvp_categories = {
                RankingCategory::ARENA_1V1,
                RankingCategory::ARENA_2V2,
                RankingCategory::ARENA_3V3,
                RankingCategory::ARENA_5V5,
                RankingCategory::BATTLEGROUND
            };
            
            for (auto category : pvp_categories) {
                auto top_players = ranking_service->GetTopPlayers(category, 1000);
                
                for (size_t i = 0; i < top_players.size(); ++i) {
                    uint64_t player_id = top_players[i].player_id;
                    uint32_t rank = i + 1;
                    
                    // 순위별 보상
                    if (rank == 1) {
                        // 1위 전용 보상
                        server->GrantTitle(player_id, "Season Champion - " + 
                                         GetCategoryName(category));
                        server->GrantMount(player_id, GetRank1Mount(category));
                        server->GrantCurrency(player_id, CurrencyType::SEASON_TOKENS, 
                                            10000);
                    } else if (rank <= 10) {
                        // Top 10 보상
                        server->GrantTitle(player_id, "Top 10 - " + 
                                         GetCategoryName(category));
                        server->GrantCurrency(player_id, CurrencyType::SEASON_TOKENS, 
                                            5000);
                    } else if (rank <= 100) {
                        // Top 100 보상
                        server->GrantTitle(player_id, "Top 100 - " + 
                                         GetCategoryName(category));
                        server->GrantCurrency(player_id, CurrencyType::SEASON_TOKENS, 
                                            2000);
                    }
                    
                    // 티어별 보상
                    auto tier = ranking_service->GetPlayerTier(player_id, category);
                    GrantSeasonTierRewards(server, player_id, tier);
                }
            }
        };
        
        // [SEQUENCE: MVP12-90] Hook into match results
        server->RegisterMatchResultHandler([ranking_service](
            const MatchResult& result) {
            
            // 승자 처리
            for (const auto& winner : result.winners) {
                int32_t rating_gain = CalculateRatingChange(
                    winner.rating, result.GetAverageLoserRating(), true);
                
                ranking_service->UpdatePlayerRanking(
                    winner.player_id, result.category, rating_gain, true);
                
                // 통계 업데이트
                UpdatePlayerStatistics(ranking_service, winner, result, true);
            }
            
            // 패자 처리
            for (const auto& loser : result.losers) {
                int32_t rating_loss = CalculateRatingChange(
                    loser.rating, result.GetAverageWinnerRating(), false);
                
                ranking_service->UpdatePlayerRanking(
                    loser.player_id, result.category, rating_loss, false);
                
                // 통계 업데이트
                UpdatePlayerStatistics(ranking_service, loser, result, false);
            }
        });
    }
    
private:
    // [SEQUENCE: MVP12-91] Calculate rating change using ELO
    static int32_t CalculateRatingChange(int32_t player_rating, 
                                        int32_t opponent_rating, 
                                        bool won) {
        const double K = 32.0; // K-factor
        
        // Expected score
        double expected = 1.0 / (1.0 + std::pow(10.0, 
            (opponent_rating - player_rating) / 400.0));
        
        // Actual score
        double actual = won ? 1.0 : 0.0;
        
        // Rating change
        return static_cast<int32_t>(K * (actual - expected));
    }
    
    // [SEQUENCE: MVP12-92] Update player statistics
    static void UpdatePlayerStatistics(RankingService* ranking_service,
                                     const PlayerMatchData& player_data,
                                     const MatchResult& match_result,
                                     bool won) {
        // 이 부분은 실제로는 더 복잡한 통계 업데이트가 필요
        // 여기서는 간단한 예시만 제공
        
        // K/D 업데이트
        // 피해량 업데이트
        // MVP 카운트 업데이트 등
    }
    
    // [SEQUENCE: MVP12-93] Grant season tier rewards
    static void GrantSeasonTierRewards(GameServer* server, 
                                     uint64_t player_id,
                                     RankingTier tier) {
        // 티어별 시즌 종료 보상
        std::unordered_map<RankingTier, uint32_t> tier_tokens = {
            {RankingTier::BRONZE, 100},
            {RankingTier::SILVER, 200},
            {RankingTier::GOLD, 500},
            {RankingTier::PLATINUM, 1000},
            {RankingTier::DIAMOND, 2000},
            {RankingTier::MASTER, 3000},
            {RankingTier::GRANDMASTER, 5000},
            {RankingTier::CHALLENGER, 10000}
        };
        
        auto it = tier_tokens.find(tier);
        if (it != tier_tokens.end()) {
            server->GrantCurrency(player_id, CurrencyType::SEASON_TOKENS, 
                                it->second);
        }
    }
    
    // Utility functions
    static std::string GetTierName(RankingTier tier) {
        static const std::unordered_map<RankingTier, std::string> names = {
            {RankingTier::UNRANKED, "Unranked"},
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
        return it != names.end() ? it->second : "Unknown";
    }
    
    static std::string GetCategoryName(RankingCategory category) {
        // Implementation...
        return "Category";
    }
    
    static uint32_t GetSeasonalMountId(RankingTier tier) {
        // 티어별 시즌 탈것 ID
        static const std::unordered_map<RankingTier, uint32_t> mounts = {
            {RankingTier::MASTER, 50001},
            {RankingTier::GRANDMASTER, 50002},
            {RankingTier::CHALLENGER, 50003}
        };
        
        auto it = mounts.find(tier);
        return it != mounts.end() ? it->second : 0;
    }
    
    static uint32_t GetRank1Mount(RankingCategory category) {
        // 카테고리별 1위 전용 탈것
        return 50100 + static_cast<uint32_t>(category);
    }
};

// [SEQUENCE: MVP12-94] Ranking data persistence
class RankingPersistence {
public:
    // [SEQUENCE: MVP12-95] Save rankings to database
    static void SaveRankings(const RankingService& service, Database* db) {
        // 트랜잭션 시작
        db->BeginTransaction();
        
        try {
            // 각 카테고리별로 저장
            for (const auto& category : GetAllCategories()) {
                auto rankings = service.GetTopPlayers(category, 10000);
                
                for (const auto& player : rankings) {
                    SavePlayerRank(db, category, player);
                }
            }
            
            db->CommitTransaction();
            spdlog::info("Saved rankings to database");
            
        } catch (const std::exception& e) {
            db->RollbackTransaction();
            spdlog::error("Failed to save rankings: {}", e.what());
        }
    }
    
    // [SEQUENCE: MVP12-96] Load rankings from database
    static void LoadRankings(RankingService& service, Database* db) {
        try {
            for (const auto& category : GetAllCategories()) {
                auto query = "SELECT * FROM rankings WHERE category = ? "
                           "ORDER BY rating DESC";
                
                auto results = db->Execute(query, static_cast<int>(category));
                
                for (const auto& row : results) {
                    PlayerRankInfo info = ParsePlayerRankInfo(row);
                    // Service에 로드하는 메서드 필요
                }
            }
            
            spdlog::info("Loaded rankings from database");
            
        } catch (const std::exception& e) {
            spdlog::error("Failed to load rankings: {}", e.what());
        }
    }
    
    // [SEQUENCE: MVP12-97] Export rankings to file
    static void ExportRankingsToFile(const RankingService& service,
                                    RankingCategory category,
                                    const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            spdlog::error("Failed to open file: {}", filename);
            return;
        }
        
        // CSV 헤더
        file << "Rank,Player Name,Guild,Rating,Wins,Losses,Win Rate,Tier\n";
        
        auto rankings = service.GetTopPlayers(category, 1000);
        
        for (const auto& player : rankings) {
            file << player.rank_data.rank << ","
                 << player.player_name << ","
                 << player.guild_name << ","
                 << player.rank_data.rating << ","
                 << player.rank_data.wins << ","
                 << player.rank_data.losses << ","
                 << std::fixed << std::setprecision(2) 
                 << player.rank_data.win_rate * 100 << "%,"
                 << GetTierName(service.GetPlayerTier(player.player_id, category))
                 << "\n";
        }
        
        file.close();
        spdlog::info("Exported rankings to {}", filename);
    }
    
private:
    static std::vector<RankingCategory> GetAllCategories() {
        return {
            RankingCategory::ARENA_1V1,
            RankingCategory::ARENA_2V2,
            RankingCategory::ARENA_3V3,
            RankingCategory::ARENA_5V5,
            RankingCategory::BATTLEGROUND,
            RankingCategory::GUILD_WARS,
            RankingCategory::OVERALL_PVP
        };
    }
    
    static void SavePlayerRank(Database* db, RankingCategory category,
                              const PlayerRankInfo& player) {
        std::string query = R"(
            INSERT OR REPLACE INTO rankings 
            (player_id, category, rank, rating, wins, losses, draws, 
             win_rate, peak_rating, last_update)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )";
        
        db->Execute(query,
            player.player_id,
            static_cast<int>(category),
            player.rank_data.rank,
            player.rank_data.rating,
            player.rank_data.wins,
            player.rank_data.losses,
            player.rank_data.draws,
            player.rank_data.win_rate,
            player.rank_data.peak_rating,
            std::chrono::system_clock::to_time_t(player.rank_data.last_update)
        );
    }
    
    static PlayerRankInfo ParsePlayerRankInfo(const DatabaseRow& row) {
        PlayerRankInfo info;
        // Parse row data into PlayerRankInfo
        return info;
    }
};

// [SEQUENCE: MVP12-98] Ranking UI data provider
class RankingUIProvider {
public:
    // [SEQUENCE: MVP12-99] Get formatted leaderboard data
    struct LeaderboardEntry {
        uint32_t rank;
        uint32_t previous_rank;
        std::string rank_change_indicator; // "↑", "↓", "="
        
        std::string player_name;
        std::string guild_name;
        std::string class_icon;
        
        int32_t rating;
        std::string tier_name;
        std::string tier_icon;
        
        uint32_t wins;
        uint32_t losses;
        std::string win_rate_display;
        
        bool is_online;
        bool is_in_match;
    };
    
    static std::vector<LeaderboardEntry> GetLeaderboardData(
        const RankingService& service,
        RankingCategory category,
        uint32_t page,
        uint32_t per_page = 20) {
        
        uint32_t offset = page * per_page;
        auto rankings = service.GetTopPlayers(category, offset + per_page);
        
        std::vector<LeaderboardEntry> entries;
        
        for (size_t i = offset; i < std::min(rankings.size(), 
                                            static_cast<size_t>(offset + per_page)); ++i) {
            entries.push_back(FormatEntry(rankings[i], service, category));
        }
        
        return entries;
    }
    
    // [SEQUENCE: MVP12-100] Get player's rank card
    struct RankCard {
        PlayerRankInfo rank_info;
        RankingTier tier;
        std::string tier_progress; // "1850/2000 to Diamond"
        
        // Recent performance
        std::vector<bool> recent_matches; // true = win, false = loss
        std::string performance_trend; // "Improving", "Stable", "Declining"
        
        // Comparison with friends
        struct FriendComparison {
            std::string friend_name;
            int32_t friend_rating;
            int32_t rating_difference;
        };
        std::vector<FriendComparison> friend_comparisons;
        
        // Next rewards
        std::string next_tier_name;
        int32_t points_to_next_tier;
        std::vector<std::string> next_tier_rewards;
    };
    
    static RankCard GetPlayerRankCard(const RankingService& service,
                                     uint64_t player_id,
                                     RankingCategory category) {
        RankCard card;
        
        auto rank_opt = service.GetPlayerRank(player_id, category);
        if (!rank_opt.has_value()) {
            return card;
        }
        
        card.rank_info = rank_opt.value();
        card.tier = service.GetPlayerTier(player_id, category);
        
        // Calculate tier progress
        auto tier_info = GetTierInfo(card.tier);
        auto next_tier_info = GetTierInfo(static_cast<RankingTier>(
            static_cast<int>(card.tier) + 1));
        
        if (tier_info && next_tier_info) {
            card.tier_progress = std::to_string(card.rank_info.rank_data.rating) + 
                "/" + std::to_string(next_tier_info->min_rating) + 
                " to " + next_tier_info->tier_name;
            
            card.next_tier_name = next_tier_info->tier_name;
            card.points_to_next_tier = next_tier_info->min_rating - 
                                      card.rank_info.rank_data.rating;
        }
        
        // Recent matches would be populated from match history
        
        return card;
    }
    
    // [SEQUENCE: MVP12-101] Get tier distribution chart data
    struct TierDistribution {
        struct TierData {
            RankingTier tier;
            std::string tier_name;
            uint32_t player_count;
            double percentage;
            std::string color_hex; // For UI display
        };
        
        std::vector<TierData> distribution;
        uint32_t total_players;
        
        // Statistics
        double average_rating;
        int32_t median_rating;
        RankingTier most_populated_tier;
    };
    
    static TierDistribution GetTierDistribution(const RankingService& service,
                                               RankingCategory category) {
        TierDistribution dist;
        
        auto tier_counts = service.GetTierDistribution(category);
        dist.total_players = 0;
        
        // Tier colors for UI
        std::unordered_map<RankingTier, std::string> tier_colors = {
            {RankingTier::BRONZE, "#CD7F32"},
            {RankingTier::SILVER, "#C0C0C0"},
            {RankingTier::GOLD, "#FFD700"},
            {RankingTier::PLATINUM, "#E5E4E2"},
            {RankingTier::DIAMOND, "#B9F2FF"},
            {RankingTier::MASTER, "#FF4500"},
            {RankingTier::GRANDMASTER, "#DC143C"},
            {RankingTier::CHALLENGER, "#4B0082"}
        };
        
        for (const auto& [tier, count] : tier_counts) {
            TierData data;
            data.tier = tier;
            data.tier_name = GetTierName(tier);
            data.player_count = count;
            data.color_hex = tier_colors[tier];
            
            dist.distribution.push_back(data);
            dist.total_players += count;
        }
        
        // Calculate percentages
        for (auto& data : dist.distribution) {
            data.percentage = dist.total_players > 0 ? 
                (static_cast<double>(data.player_count) / dist.total_players * 100) : 0;
        }
        
        // Find most populated tier
        auto max_it = std::max_element(dist.distribution.begin(), 
                                      dist.distribution.end(),
            [](const TierData& a, const TierData& b) {
                return a.player_count < b.player_count;
            });
        
        if (max_it != dist.distribution.end()) {
            dist.most_populated_tier = max_it->tier;
        }
        
        return dist;
    }
    
private:
    static LeaderboardEntry FormatEntry(const PlayerRankInfo& player,
                                      const RankingService& service,
                                      RankingCategory category) {
        LeaderboardEntry entry;
        
        entry.rank = player.rank_data.rank;
        entry.previous_rank = player.rank_data.previous_rank;
        
        // Rank change indicator
        if (player.rank_data.rank_change > 0) {
            entry.rank_change_indicator = "↑";
        } else if (player.rank_data.rank_change < 0) {
            entry.rank_change_indicator = "↓";
        } else {
            entry.rank_change_indicator = "=";
        }
        
        entry.player_name = player.player_name;
        entry.guild_name = player.guild_name;
        entry.class_icon = "icons/classes/" + player.class_name + ".png";
        
        entry.rating = player.rank_data.rating;
        auto tier = service.GetPlayerTier(player.player_id, category);
        entry.tier_name = GetTierName(tier);
        entry.tier_icon = "icons/tiers/" + std::to_string(static_cast<int>(tier)) + ".png";
        
        entry.wins = player.rank_data.wins;
        entry.losses = player.rank_data.losses;
        
        // Format win rate
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) 
           << (player.rank_data.win_rate * 100) << "%";
        entry.win_rate_display = ss.str();
        
        // Online status would come from server
        entry.is_online = false;
        entry.is_in_match = false;
        
        return entry;
    }
    
    static const TierInfo* GetTierInfo(RankingTier tier) {
        // This would access the tier info from RankingService
        return nullptr;
    }
    
    static std::string GetTierName(RankingTier tier) {
        // Implementation...
        return "Tier";
    }
};

// [SEQUENCE: MVP12-102] Season management
class SeasonManager {
public:
    // [SEQUENCE: MVP12-103] Create new season
    static SeasonInfo CreateSeason(uint32_t season_number,
                                  std::chrono::system_clock::time_point start_date,
                                  uint32_t duration_days) {
        SeasonInfo season;
        
        season.season_id = season_number;
        season.season_name = "Season " + std::to_string(season_number);
        season.start_date = start_date;
        season.end_date = start_date + std::chrono::hours(24 * duration_days);
        season.is_active = true;
        
        // Define season rewards
        DefineSeasonRewards(season);
        
        return season;
    }
    
    // [SEQUENCE: MVP12-104] Schedule season transition
    static void ScheduleSeasonTransition(RankingService* service,
                                       const SeasonInfo& next_season,
                                       std::chrono::system_clock::time_point transition_time) {
        // 스케줄러에 등록
        std::thread([service, next_season, transition_time]() {
            std::this_thread::sleep_until(transition_time);
            
            // 시즌 전환 실행
            service->StartNewSeason(next_season);
            
            // 시즌 시작 이벤트
            OnSeasonStart(next_season);
        }).detach();
    }
    
private:
    // [SEQUENCE: MVP12-105] Define rewards for season
    static void DefineSeasonRewards(SeasonInfo& season) {
        // 티어별 보상
        season.rewards.tier_rewards[RankingTier::BRONZE] = {20001, 20002};
        season.rewards.tier_rewards[RankingTier::SILVER] = {20003, 20004};
        season.rewards.tier_rewards[RankingTier::GOLD] = {20005, 20006, 20007};
        season.rewards.tier_rewards[RankingTier::PLATINUM] = {20008, 20009, 20010};
        season.rewards.tier_rewards[RankingTier::DIAMOND] = {20011, 20012, 20013};
        season.rewards.tier_rewards[RankingTier::MASTER] = {20014, 20015, 20016, 20017};
        season.rewards.tier_rewards[RankingTier::GRANDMASTER] = {20018, 20019, 20020, 20021};
        season.rewards.tier_rewards[RankingTier::CHALLENGER] = {20022, 20023, 20024, 20025};
        
        // 참가 보상
        season.rewards.participation_rewards = {20000};
        
        // Top 100 보상
        season.rewards.top_100_rewards = {20100, 20101};
        
        // Top 10 보상
        season.rewards.top_10_rewards = {20200, 20201, 20202};
        
        // 1위 전용 탈것
        season.rewards.rank_1_exclusive_mount = 50000 + season.season_id;
    }
    
    static std::function<void(const SeasonInfo&)> OnSeasonStart;
};

std::function<void(const SeasonInfo&)> SeasonManager::OnSeasonStart;

} // namespace mmorpg::ranking