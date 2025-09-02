#include "leaderboard_system.h"
#include <sstream>
#include <iomanip>
#include <fstream>
#include <algorithm>

namespace mmorpg::ranking {

// [SEQUENCE: MVP12-152] Leaderboard system integration examples
class LeaderboardIntegration {
public:
    // [SEQUENCE: MVP12-153] Initialize leaderboard with game server
    static void InitializeWithGameServer(GameServer* server,
                                       LeaderboardSystem* leaderboard_system,
                                       RankingService* ranking_service) {
        
        // 리더보드 뷰 요청 처리
        server->RegisterPacketHandler<LeaderboardRequestPacket>(
            [leaderboard_system, ranking_service](uint64_t player_id, 
                                                const LeaderboardRequestPacket& request) {
                
                LeaderboardResponsePacket response;
                
                // 요청된 리더보드 타입에 따라 처리
                switch (request.type) {
                    case LeaderboardType::GLOBAL_RANKING: {
                        auto entries = leaderboard_system->GetGlobalLeaderboard(
                            request.category, request.page, request.per_page);
                        response.entries = entries;
                        break;
                    }
                    
                    case LeaderboardType::REGIONAL_RANKING: {
                        std::string region = GetPlayerRegion(player_id);
                        auto entries = leaderboard_system->GetRegionalLeaderboard(
                            request.category, region, request.page, request.per_page);
                        response.entries = entries;
                        break;
                    }
                    
                    case LeaderboardType::FRIENDS_RANKING: {
                        auto entries = leaderboard_system->GetFriendsLeaderboard(
                            player_id, request.category);
                        response.entries = entries;
                        break;
                    }
                    
                    case LeaderboardType::GUILD_RANKING: {
                        uint64_t guild_id = GetPlayerGuildId(player_id);
                        auto entries = leaderboard_system->GetGuildLeaderboard(
                            guild_id, request.category);
                        response.entries = entries;
                        break;
                    }
                    
                    case LeaderboardType::CLASS_SPECIFIC: {
                        CharacterClass player_class = GetPlayerClass(player_id);
                        auto entries = leaderboard_system->GetClassLeaderboard(
                            player_class, request.category, request.page, request.per_page);
                        response.entries = entries;
                        break;
                    }
                }
                
                // 플레이어 자신의 순위 정보 포함
                response.my_rank = ranking_service->GetPlayerRank(player_id, request.category);
                
                return response;
            });
        
        // [SEQUENCE: MVP12-154] Subscribe to rank updates for live updates
        ranking_service->OnRankUpdate = [leaderboard_system](uint64_t player_id,
                                                           RankingCategory category,
                                                           uint32_t old_rank,
                                                           uint32_t new_rank) {
            // 캐시 무효화
            leaderboard_system->InvalidateCache(category);
            
            // 리더보드에 관심있는 플레이어들에게 실시간 업데이트 전송
            if (ShouldBroadcastUpdate(old_rank, new_rank)) {
                BroadcastLeaderboardUpdate(category, player_id, old_rank, new_rank);
            }
        };
        
        // [SEQUENCE: MVP12-155] Handle leaderboard refresh timer
        server->ScheduleRecurringTask("leaderboard_refresh", 
            std::chrono::minutes(5), [leaderboard_system]() {
                // 모든 캐시된 리더보드 새로고침
                leaderboard_system->RefreshAllCaches();
            });
    }
    
private:
    static bool ShouldBroadcastUpdate(uint32_t old_rank, uint32_t new_rank) {
        // Top 100 내의 순위 변경만 브로드캐스트
        return (old_rank <= 100 || new_rank <= 100);
    }
    
    static std::string GetPlayerRegion(uint64_t player_id) {
        // 실제 구현에서는 플레이어의 지역 정보 조회
        return "NA";
    }
    
    static uint64_t GetPlayerGuildId(uint64_t player_id) {
        // 실제 구현에서는 플레이어의 길드 정보 조회
        return 0;
    }
    
    static CharacterClass GetPlayerClass(uint64_t player_id) {
        // 실제 구현에서는 플레이어의 클래스 정보 조회
        return CharacterClass::WARRIOR;
    }
    
    static void BroadcastLeaderboardUpdate(RankingCategory category,
                                         uint64_t player_id,
                                         uint32_t old_rank,
                                         uint32_t new_rank) {
        // 리더보드 업데이트를 관심있는 플레이어들에게 전송
    }
};

// [SEQUENCE: MVP12-156] Leaderboard data persistence
class LeaderboardPersistence {
public:
    // [SEQUENCE: MVP12-157] Save leaderboard snapshot
    static void SaveLeaderboardSnapshot(const LeaderboardSystem& system,
                                      RankingCategory category,
                                      const std::string& filename) {
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            spdlog::error("Failed to save leaderboard snapshot: {}", filename);
            return;
        }
        
        // 헤더 정보
        LeaderboardSnapshotHeader header;
        header.version = 1;
        header.category = category;
        header.timestamp = std::chrono::system_clock::now();
        header.entry_count = 0;
        
        // 헤더 위치 기억 (나중에 entry_count 업데이트)
        auto header_pos = file.tellp();
        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        
        // 리더보드 엔트리들 저장
        auto entries = system.GetGlobalLeaderboard(category, 0, 10000);
        
        for (const auto& entry : entries) {
            SaveEntry(file, entry);
            header.entry_count++;
        }
        
        // 헤더의 entry_count 업데이트
        file.seekp(header_pos);
        file.write(reinterpret_cast<const char*>(&header), sizeof(header));
        
        file.close();
        spdlog::info("Saved leaderboard snapshot: {} entries to {}", 
                    header.entry_count, filename);
    }
    
    // [SEQUENCE: MVP12-158] Export leaderboard to various formats
    static void ExportLeaderboard(const LeaderboardSystem& system,
                                RankingCategory category,
                                ExportFormat format,
                                const std::string& filename) {
        switch (format) {
            case ExportFormat::CSV:
                ExportToCSV(system, category, filename);
                break;
            case ExportFormat::JSON:
                ExportToJSON(system, category, filename);
                break;
            case ExportFormat::HTML:
                ExportToHTML(system, category, filename);
                break;
        }
    }
    
private:
    struct LeaderboardSnapshotHeader {
        uint32_t version;
        RankingCategory category;
        std::chrono::system_clock::time_point timestamp;
        uint32_t entry_count;
    };
    
    static void SaveEntry(std::ofstream& file, const LeaderboardEntry& entry) {
        // 엔트리 데이터 직렬화
        file.write(reinterpret_cast<const char*>(&entry.rank), sizeof(entry.rank));
        
        // 문자열 저장
        uint32_t name_len = entry.player_name.length();
        file.write(reinterpret_cast<const char*>(&name_len), sizeof(name_len));
        file.write(entry.player_name.c_str(), name_len);
        
        file.write(reinterpret_cast<const char*>(&entry.rating), sizeof(entry.rating));
        file.write(reinterpret_cast<const char*>(&entry.tier), sizeof(entry.tier));
        
        // Recent matches
        uint32_t matches_count = entry.recent_matches.size();
        file.write(reinterpret_cast<const char*>(&matches_count), sizeof(matches_count));
        for (bool won : entry.recent_matches) {
            file.write(reinterpret_cast<const char*>(&won), sizeof(won));
        }
    }
    
    // [SEQUENCE: MVP12-159] Export to CSV format
    static void ExportToCSV(const LeaderboardSystem& system,
                          RankingCategory category,
                          const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            spdlog::error("Failed to export to CSV: {}", filename);
            return;
        }
        
        // CSV 헤더
        file << "Rank,Player Name,Rating,Tier,Wins,Losses,Win Rate,Streak,Recent Form\n";
        
        auto entries = system.GetGlobalLeaderboard(category, 0, 1000);
        
        for (const auto& entry : entries) {
            file << entry.rank << ","
                 << "\"" << entry.player_name << "\","
                 << entry.rating << ","
                 << GetTierName(entry.tier) << ","
                 << entry.stats.total_wins << ","
                 << entry.stats.total_losses << ","
                 << std::fixed << std::setprecision(2) 
                 << entry.stats.win_rate * 100 << "%,"
                 << entry.stats.current_streak << ",";
            
            // Recent form (W/L 최근 5경기)
            file << "\"";
            int count = 0;
            for (auto it = entry.recent_matches.rbegin(); 
                 it != entry.recent_matches.rend() && count < 5; ++it, ++count) {
                file << (*it ? "W" : "L");
            }
            file << "\"\n";
        }
        
        file.close();
        spdlog::info("Exported leaderboard to CSV: {}", filename);
    }
    
    // [SEQUENCE: MVP12-160] Export to JSON format
    static void ExportToJSON(const LeaderboardSystem& system,
                           RankingCategory category,
                           const std::string& filename) {
        Json::Value root;
        Json::Value metadata;
        
        metadata["category"] = GetCategoryName(category);
        metadata["export_time"] = GetCurrentTimeString();
        metadata["total_players"] = system.GetTotalPlayers(category);
        
        root["metadata"] = metadata;
        
        Json::Value leaderboard(Json::arrayValue);
        auto entries = system.GetGlobalLeaderboard(category, 0, 1000);
        
        for (const auto& entry : entries) {
            Json::Value player;
            player["rank"] = entry.rank;
            player["player_name"] = entry.player_name;
            player["rating"] = entry.rating;
            player["tier"] = GetTierName(entry.tier);
            
            Json::Value stats;
            stats["total_matches"] = entry.stats.total_matches;
            stats["wins"] = entry.stats.total_wins;
            stats["losses"] = entry.stats.total_losses;
            stats["win_rate"] = entry.stats.win_rate;
            stats["kd_ratio"] = entry.stats.kd_ratio;
            stats["current_streak"] = entry.stats.current_streak;
            stats["best_streak"] = entry.stats.best_streak;
            
            player["stats"] = stats;
            
            // Recent matches
            Json::Value recent_matches(Json::arrayValue);
            for (bool won : entry.recent_matches) {
                recent_matches.append(won ? "win" : "loss");
            }
            player["recent_matches"] = recent_matches;
            
            // Badges
            Json::Value badges(Json::arrayValue);
            for (const auto& badge_url : entry.badge_urls) {
                badges.append(badge_url);
            }
            player["badges"] = badges;
            
            leaderboard.append(player);
        }
        
        root["leaderboard"] = leaderboard;
        
        // Write to file
        std::ofstream file(filename);
        if (file.is_open()) {
            Json::StreamWriterBuilder builder;
            builder["indentation"] = "  ";
            std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
            writer->write(root, &file);
            file.close();
            spdlog::info("Exported leaderboard to JSON: {}", filename);
        }
    }
    
    // [SEQUENCE: MVP12-161] Export to HTML format
    static void ExportToHTML(const LeaderboardSystem& system,
                           RankingCategory category,
                           const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            spdlog::error("Failed to export to HTML: {}", filename);
            return;
        }
        
        // HTML header
        file << R"(<!DOCTYPE html>
<html>
<head>
    <title>Leaderboard - )" << GetCategoryName(category) << R"(</title>
    <style>
        body { font-family: Arial, sans-serif; background-color: #f0f0f0; }
        .container { max-width: 1200px; margin: 0 auto; padding: 20px; }
        h1 { text-align: center; color: #333; }
        table { width: 100%; border-collapse: collapse; background-color: white; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        th, td { padding: 12px; text-align: left; border-bottom: 1px solid #ddd; }
        th { background-color: #4CAF50; color: white; }
        tr:hover { background-color: #f5f5f5; }
        .rank-1 { background-color: #FFD700; font-weight: bold; }
        .rank-2 { background-color: #C0C0C0; }
        .rank-3 { background-color: #CD7F32; }
        .tier { padding: 2px 8px; border-radius: 4px; color: white; font-size: 12px; }
        .tier-bronze { background-color: #CD7F32; }
        .tier-silver { background-color: #C0C0C0; }
        .tier-gold { background-color: #FFD700; }
        .tier-platinum { background-color: #E5E4E2; color: #333; }
        .tier-diamond { background-color: #B9F2FF; color: #333; }
        .tier-master { background-color: #FF4500; }
        .tier-grandmaster { background-color: #DC143C; }
        .tier-challenger { background-color: #4B0082; }
        .recent-form { font-family: monospace; }
        .win { color: green; font-weight: bold; }
        .loss { color: red; }
        .online { color: green; }
        .offline { color: gray; }
    </style>
</head>
<body>
    <div class="container">
        <h1>)" << GetCategoryName(category) << R"( Leaderboard</h1>
        <p style="text-align: center;">Last updated: )" << GetCurrentTimeString() << R"(</p>
        <table>
            <tr>
                <th>Rank</th>
                <th>Player</th>
                <th>Rating</th>
                <th>Tier</th>
                <th>Matches</th>
                <th>Win Rate</th>
                <th>K/D</th>
                <th>Streak</th>
                <th>Recent Form</th>
                <th>Status</th>
            </tr>
)";
        
        auto entries = system.GetGlobalLeaderboard(category, 0, 100);
        
        for (const auto& entry : entries) {
            std::string row_class = "";
            if (entry.rank == 1) row_class = "rank-1";
            else if (entry.rank == 2) row_class = "rank-2";
            else if (entry.rank == 3) row_class = "rank-3";
            
            file << "            <tr" << (row_class.empty() ? "" : " class=\"" + row_class + "\"") << ">\n";
            file << "                <td>" << entry.rank << "</td>\n";
            file << "                <td>" << entry.player_name << "</td>\n";
            file << "                <td>" << entry.rating << "</td>\n";
            file << "                <td><span class=\"tier tier-" << GetTierClass(entry.tier) << "\">" 
                 << GetTierName(entry.tier) << "</span></td>\n";
            file << "                <td>" << entry.stats.total_matches << "</td>\n";
            file << "                <td>" << std::fixed << std::setprecision(1) 
                 << entry.stats.win_rate * 100 << "%</td>\n";
            file << "                <td>" << std::fixed << std::setprecision(2) 
                 << entry.stats.kd_ratio << "</td>\n";
            file << "                <td>" << entry.stats.current_streak << "</td>\n";
            
            // Recent form
            file << "                <td class=\"recent-form\">";
            int count = 0;
            for (auto it = entry.recent_matches.rbegin(); 
                 it != entry.recent_matches.rend() && count < 5; ++it, ++count) {
                file << "<span class=\"" << (*it ? "win\">W" : "loss\">L") << "</span>";
            }
            file << "</td>\n";
            
            file << "                <td class=\"" << (entry.is_online ? "online\">Online" : "offline\">Offline") 
                 << "</td>\n";
            file << "            </tr>\n";
        }
        
        file << R"(        </table>
    </div>
</body>
</html>
)";
        
        file.close();
        spdlog::info("Exported leaderboard to HTML: {}", filename);
    }
    
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
    
    static std::string GetTierClass(RankingTier tier) {
        static const std::unordered_map<RankingTier, std::string> classes = {
            {RankingTier::BRONZE, "bronze"},
            {RankingTier::SILVER, "silver"},
            {RankingTier::GOLD, "gold"},
            {RankingTier::PLATINUM, "platinum"},
            {RankingTier::DIAMOND, "diamond"},
            {RankingTier::MASTER, "master"},
            {RankingTier::GRANDMASTER, "grandmaster"},
            {RankingTier::CHALLENGER, "challenger"}
        };
        
        auto it = classes.find(tier);
        return it != classes.end() ? it->second : "unranked";
    }
    
    static std::string GetCategoryName(RankingCategory category) {
        // Implementation...
        return "Arena";
    }
    
    static std::string GetCurrentTimeString() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
};

// [SEQUENCE: MVP12-162] Leaderboard UI generation
class LeaderboardUIGenerator {
public:
    // [SEQUENCE: MVP12-163] Generate leaderboard UI data
    static Json::Value GenerateLeaderboardUI(const LeaderboardSystem& system,
                                           RankingCategory category,
                                           LeaderboardType type,
                                           const LeaderboardViewOptions& options) {
        Json::Value ui_data;
        
        // Header information
        ui_data["header"] = GenerateHeader(category, type);
        
        // Leaderboard entries
        ui_data["entries"] = GenerateEntries(system, category, type, options);
        
        // Statistics panel
        ui_data["statistics"] = GenerateStatistics(system, category);
        
        // Filter options
        ui_data["filters"] = GenerateFilterOptions();
        
        // Pagination
        ui_data["pagination"] = GeneratePagination(system, category, options);
        
        return ui_data;
    }
    
    // [SEQUENCE: MVP12-164] Generate player comparison view
    static Json::Value GenerateComparisonView(const LeaderboardSystem& system,
                                            uint64_t player1_id,
                                            uint64_t player2_id,
                                            RankingCategory category) {
        Json::Value comparison;
        
        // Get player data
        auto player1_data = GetPlayerLeaderboardData(system, player1_id, category);
        auto player2_data = GetPlayerLeaderboardData(system, player2_id, category);
        
        // Basic comparison
        comparison["player1"] = FormatPlayerData(player1_data);
        comparison["player2"] = FormatPlayerData(player2_data);
        
        // Head to head stats
        comparison["head_to_head"] = GetHeadToHeadStats(player1_id, player2_id);
        
        // Performance comparison
        comparison["performance"] = ComparePerformance(player1_data, player2_data);
        
        // Recent form comparison
        comparison["recent_form"] = CompareRecentForm(player1_data, player2_data);
        
        return comparison;
    }
    
private:
    static Json::Value GenerateHeader(RankingCategory category, LeaderboardType type) {
        Json::Value header;
        
        header["title"] = GetLeaderboardTitle(category, type);
        header["subtitle"] = GetLeaderboardSubtitle(category, type);
        header["icon"] = GetCategoryIcon(category);
        header["last_update"] = GetLastUpdateTime();
        
        return header;
    }
    
    static Json::Value GenerateEntries(const LeaderboardSystem& system,
                                     RankingCategory category,
                                     LeaderboardType type,
                                     const LeaderboardViewOptions& options) {
        Json::Value entries(Json::arrayValue);
        
        std::vector<LeaderboardEntry> leaderboard_entries;
        
        // Get entries based on type
        switch (type) {
            case LeaderboardType::GLOBAL_RANKING:
                leaderboard_entries = system.GetGlobalLeaderboard(
                    category, options.page * options.per_page, options.per_page);
                break;
                
            // Other types...
        }
        
        // Format entries for UI
        for (const auto& entry : leaderboard_entries) {
            entries.append(FormatEntryForUI(entry));
        }
        
        return entries;
    }
    
    static Json::Value FormatEntryForUI(const LeaderboardEntry& entry) {
        Json::Value ui_entry;
        
        // Basic info
        ui_entry["rank"] = entry.rank;
        ui_entry["rank_display"] = FormatRank(entry.rank);
        ui_entry["player_name"] = entry.player_name;
        ui_entry["rating"] = entry.rating;
        ui_entry["rating_display"] = FormatRating(entry.rating);
        
        // Tier info
        ui_entry["tier"]["name"] = GetTierName(entry.tier);
        ui_entry["tier"]["icon"] = GetTierIcon(entry.tier);
        ui_entry["tier"]["color"] = GetTierColor(entry.tier);
        
        // Stats
        ui_entry["stats"]["matches"] = entry.stats.total_matches;
        ui_entry["stats"]["win_rate"] = FormatPercentage(entry.stats.win_rate);
        ui_entry["stats"]["kd_ratio"] = FormatKDRatio(entry.stats.kd_ratio);
        ui_entry["stats"]["streak"] = FormatStreak(entry.stats.current_streak);
        
        // Recent form visualization
        ui_entry["recent_form"] = FormatRecentMatches(entry.recent_matches);
        
        // Visual elements
        ui_entry["badges"] = entry.badge_urls;
        ui_entry["status_indicator"] = entry.is_online ? "online" : "offline";
        
        // Special indicators
        if (entry.rank <= 3) {
            ui_entry["special_frame"] = GetSpecialFrame(entry.rank);
        }
        
        return ui_entry;
    }
    
    static Json::Value GenerateStatistics(const LeaderboardSystem& system,
                                        RankingCategory category) {
        Json::Value stats;
        
        auto analysis = system.GetStatisticalAnalysis(category);
        
        // Tier distribution chart data
        Json::Value tier_chart;
        tier_chart["type"] = "pie";
        tier_chart["data"] = FormatTierDistribution(analysis.tier_distribution);
        stats["tier_distribution"] = tier_chart;
        
        // Average stats
        stats["averages"]["rating"] = analysis.average_rating;
        stats["averages"]["matches_played"] = analysis.average_matches;
        stats["averages"]["win_rate"] = FormatPercentage(analysis.average_win_rate);
        
        // Trends
        stats["trends"]["active_players"] = analysis.active_player_count;
        stats["trends"]["matches_today"] = analysis.matches_today;
        stats["trends"]["new_players_this_week"] = analysis.new_players_this_week;
        
        return stats;
    }
    
    static Json::Value GeneratePagination(const LeaderboardSystem& system,
                                        RankingCategory category,
                                        const LeaderboardViewOptions& options) {
        Json::Value pagination;
        
        uint32_t total_players = system.GetTotalPlayers(category);
        uint32_t total_pages = (total_players + options.per_page - 1) / options.per_page;
        
        pagination["current_page"] = options.page;
        pagination["total_pages"] = total_pages;
        pagination["per_page"] = options.per_page;
        pagination["total_entries"] = total_players;
        
        // Navigation links
        pagination["has_previous"] = options.page > 0;
        pagination["has_next"] = options.page < total_pages - 1;
        
        // Page numbers for UI
        Json::Value page_numbers(Json::arrayValue);
        int start = std::max(0, static_cast<int>(options.page) - 2);
        int end = std::min(static_cast<int>(total_pages), static_cast<int>(options.page) + 3);
        
        for (int i = start; i < end; ++i) {
            page_numbers.append(i);
        }
        pagination["page_numbers"] = page_numbers;
        
        return pagination;
    }
    
    // Helper functions
    static std::string FormatRank(uint32_t rank) {
        if (rank == 1) return "🥇 1st";
        if (rank == 2) return "🥈 2nd";
        if (rank == 3) return "🥉 3rd";
        return "#" + std::to_string(rank);
    }
    
    static std::string FormatRating(int32_t rating) {
        return std::to_string(rating) + " SR";
    }
    
    static std::string FormatPercentage(double value) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << (value * 100) << "%";
        return ss.str();
    }
    
    static std::string FormatKDRatio(double kd) {
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << kd;
        return ss.str();
    }
    
    static Json::Value FormatStreak(int32_t streak) {
        Json::Value streak_data;
        
        if (streak > 0) {
            streak_data["type"] = "win";
            streak_data["count"] = streak;
            streak_data["display"] = "🔥 " + std::to_string(streak) + "W";
            streak_data["color"] = "#4CAF50";
        } else if (streak < 0) {
            streak_data["type"] = "loss";
            streak_data["count"] = -streak;
            streak_data["display"] = std::to_string(-streak) + "L";
            streak_data["color"] = "#F44336";
        } else {
            streak_data["type"] = "neutral";
            streak_data["count"] = 0;
            streak_data["display"] = "-";
            streak_data["color"] = "#9E9E9E";
        }
        
        return streak_data;
    }
    
    static Json::Value FormatRecentMatches(const std::vector<bool>& matches) {
        Json::Value recent_form;
        
        recent_form["visual"] = "";
        recent_form["details"] = Json::arrayValue;
        
        int count = 0;
        for (auto it = matches.rbegin(); it != matches.rend() && count < 10; ++it, ++count) {
            Json::Value match;
            match["result"] = *it ? "win" : "loss";
            match["icon"] = *it ? "✓" : "✗";
            match["color"] = *it ? "#4CAF50" : "#F44336";
            
            recent_form["visual"] += match["icon"].asString();
            recent_form["details"].append(match);
        }
        
        return recent_form;
    }
    
    static std::string GetTierIcon(RankingTier tier) {
        static const std::unordered_map<RankingTier, std::string> icons = {
            {RankingTier::BRONZE, "🥉"},
            {RankingTier::SILVER, "🥈"},
            {RankingTier::GOLD, "🥇"},
            {RankingTier::PLATINUM, "💎"},
            {RankingTier::DIAMOND, "💠"},
            {RankingTier::MASTER, "⚔️"},
            {RankingTier::GRANDMASTER, "👑"},
            {RankingTier::CHALLENGER, "🏆"}
        };
        
        auto it = icons.find(tier);
        return it != icons.end() ? it->second : "❓";
    }
    
    static std::string GetTierColor(RankingTier tier) {
        static const std::unordered_map<RankingTier, std::string> colors = {
            {RankingTier::BRONZE, "#CD7F32"},
            {RankingTier::SILVER, "#C0C0C0"},
            {RankingTier::GOLD, "#FFD700"},
            {RankingTier::PLATINUM, "#E5E4E2"},
            {RankingTier::DIAMOND, "#B9F2FF"},
            {RankingTier::MASTER, "#FF4500"},
            {RankingTier::GRANDMASTER, "#DC143C"},
            {RankingTier::CHALLENGER, "#4B0082"}
        };
        
        auto it = colors.find(tier);
        return it != colors.end() ? it->second : "#808080";
    }
    
    static Json::Value GetSpecialFrame(uint32_t rank) {
        Json::Value frame;
        
        if (rank == 1) {
            frame["type"] = "legendary";
            frame["animation"] = "glow_gold";
            frame["particles"] = "sparkles";
        } else if (rank == 2) {
            frame["type"] = "epic";
            frame["animation"] = "shimmer_silver";
        } else if (rank == 3) {
            frame["type"] = "rare";
            frame["animation"] = "pulse_bronze";
        }
        
        return frame;
    }
    
    // Stub implementations
    static std::string GetLeaderboardTitle(RankingCategory category, LeaderboardType type) {
        return "Leaderboard";
    }
    
    static std::string GetLeaderboardSubtitle(RankingCategory category, LeaderboardType type) {
        return "Top Players";
    }
    
    static std::string GetCategoryIcon(RankingCategory category) {
        return "⚔️";
    }
    
    static std::string GetLastUpdateTime() {
        return "Just now";
    }
    
    static Json::Value GenerateFilterOptions() {
        return Json::Value();
    }
    
    static Json::Value FormatTierDistribution(const std::unordered_map<RankingTier, uint32_t>& dist) {
        return Json::Value();
    }
    
    static LeaderboardEntry GetPlayerLeaderboardData(const LeaderboardSystem& system,
                                                   uint64_t player_id,
                                                   RankingCategory category) {
        return LeaderboardEntry();
    }
    
    static Json::Value FormatPlayerData(const LeaderboardEntry& data) {
        return Json::Value();
    }
    
    static Json::Value GetHeadToHeadStats(uint64_t player1_id, uint64_t player2_id) {
        return Json::Value();
    }
    
    static Json::Value ComparePerformance(const LeaderboardEntry& p1, const LeaderboardEntry& p2) {
        return Json::Value();
    }
    
    static Json::Value CompareRecentForm(const LeaderboardEntry& p1, const LeaderboardEntry& p2) {
        return Json::Value();
    }
    
    static std::string GetTierName(RankingTier tier) {
        return "Tier";
    }
};

} // namespace mmorpg::ranking