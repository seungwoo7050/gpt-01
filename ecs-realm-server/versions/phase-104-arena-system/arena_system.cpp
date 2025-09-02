#include "arena_system.h"
#include <algorithm>
#include <numeric>
#include <iomanip>

namespace mmorpg::arena {

// [SEQUENCE: 2820] Arena system integration
class ArenaIntegration {
public:
    // [SEQUENCE: 2821] Initialize arena system with game server
    static void InitializeWithGameServer(GameServer* server,
                                       ArenaSystem* arena_system,
                                       MatchmakingService* matchmaking_service,
                                       RankingService* ranking_service) {
        
        arena_system->SetMatchmakingService(matchmaking_service);
        arena_system->SetRankingService(ranking_service);
        
        // [SEQUENCE: 2822] Handle matchmaking completion
        matchmaking_service->OnMatchFound = [arena_system, server](
            const MatchFoundEvent& event) {
            
            // Arena 타입인 경우에만 처리
            if (!IsArenaCategory(event.category)) {
                return;
            }
            
            // Arena 설정 생성
            ArenaConfig config;
            config.type = GetArenaType(event.category);
            config.map = SelectRandomMap();
            
            // Arena 매치 생성
            uint64_t match_id = arena_system->CreateArenaMatch(config);
            auto match = arena_system->GetMatch(match_id);
            
            if (!match) {
                spdlog::error("Failed to create arena match");
                return;
            }
            
            // 플레이어들을 매치에 추가
            uint32_t team_id = 1;
            for (const auto& team : event.teams) {
                for (const auto& player : team.players) {
                    match->AddPlayer(player.player_id, 
                                   server->GetPlayerName(player.player_id),
                                   team_id,
                                   player.rating);
                    
                    // 플레이어를 arena 인스턴스로 이동
                    server->TeleportPlayer(player.player_id, 
                                         GetArenaInstanceId(match_id));
                }
                team_id++;
            }
            
            // 카운트다운 시작
            match->StartCountdown();
            
            spdlog::info("Arena match {} created for {} players", 
                        match_id, event.teams.size() * event.teams[0].players.size());
        };
        
        // [SEQUENCE: 2823] Handle player actions in arena
        server->RegisterPacketHandler<ArenaActionPacket>(
            [arena_system](uint64_t player_id, const ArenaActionPacket& packet) {
                
                auto match = arena_system->GetMatch(packet.match_id);
                if (!match) {
                    return;
                }
                
                switch (packet.action_type) {
                    case ArenaActionType::PLAYER_KILL:
                        match->HandlePlayerKill(packet.killer_id, 
                                              packet.victim_id,
                                              packet.assister_id);
                        break;
                        
                    case ArenaActionType::DAMAGE_DEALT:
                        // Update damage statistics
                        break;
                        
                    case ArenaActionType::HEALING_DONE:
                        // Update healing statistics
                        break;
                }
            });
        
        // [SEQUENCE: 2824] Handle match completion
        server->ScheduleRecurringTask("arena_update", 
            std::chrono::milliseconds(100), [arena_system, ranking_service]() {
                
                arena_system->Update();
                
                // Check for completed matches
                auto matches = arena_system->GetActiveMatches();
                for (const auto& match : matches) {
                    if (match->GetState() == ArenaState::FINISHED) {
                        ProcessMatchCompletion(match, ranking_service);
                    }
                }
            });
        
        // [SEQUENCE: 2825] Arena queue command
        server->RegisterCommand("queue", [arena_system, server](
            uint64_t player_id, const std::vector<std::string>& args) {
            
            if (args.empty()) {
                server->SendMessage(player_id, "Usage: /queue <1v1|2v2|3v3|5v5>");
                return;
            }
            
            ArenaType type = ParseArenaType(args[0]);
            if (type == ArenaType::CUSTOM) {
                server->SendMessage(player_id, "Invalid arena type");
                return;
            }
            
            // Get player's current rating
            int32_t rating = ranking_service->GetPlayerRating(
                player_id, GetRankingCategory(type));
            
            arena_system->QueueForArena(player_id, type, rating);
            server->SendMessage(player_id, "You have joined the " + args[0] + " queue");
        });
    }
    
private:
    // [SEQUENCE: 2826] Process match completion
    static void ProcessMatchCompletion(std::shared_ptr<ArenaMatch> match,
                                     RankingService* ranking_service) {
        auto stats = match->GetMatchStatistics();
        
        // Update rankings for all players
        for (const auto& team : stats.team_stats) {
            for (const auto& player : team.players) {
                // Apply rating change
                ranking_service->UpdatePlayerRanking(
                    player.player_id,
                    GetRankingCategory(match->GetConfig().type),
                    player.rating_change,
                    team.is_winner);
                
                // Send match summary to player
                SendMatchSummary(player.player_id, stats, player);
            }
        }
        
        // Log match results
        LogMatchResults(match->GetMatchId(), stats);
        
        // Achievement checks
        CheckArenaAchievements(stats);
    }
    
    // [SEQUENCE: 2827] Send match summary to player
    static void SendMatchSummary(uint64_t player_id,
                                const ArenaMatch::MatchStatistics& stats,
                                const ArenaPlayer& player) {
        MatchSummaryPacket packet;
        
        packet.match_duration = stats.match_duration_seconds;
        packet.is_winner = false;
        
        // Find if player was on winning team
        for (const auto& team : stats.team_stats) {
            if (team.is_winner) {
                for (const auto& p : team.players) {
                    if (p.player_id == player_id) {
                        packet.is_winner = true;
                        break;
                    }
                }
            }
        }
        
        // Personal stats
        packet.kills = player.stats.kills;
        packet.deaths = player.stats.deaths;
        packet.assists = player.stats.assists;
        packet.damage_dealt = player.stats.damage_dealt;
        packet.damage_taken = player.stats.damage_taken;
        packet.healing_done = player.stats.healing_done;
        
        // Rating change
        packet.rating_before = player.current_rating - player.rating_change;
        packet.rating_after = player.current_rating;
        packet.rating_change = player.rating_change;
        
        // MVP info
        packet.mvp_player_id = stats.mvp_player_id;
        packet.mvp_reason = stats.mvp_reason;
        packet.is_mvp = (player_id == stats.mvp_player_id);
        
        // Rewards
        auto config = GetArenaConfig();
        if (packet.is_winner) {
            packet.honor_gained = config.winner_honor_points;
            packet.xp_multiplier = config.winner_xp_multiplier;
        } else {
            packet.honor_gained = config.loser_honor_points;
            packet.xp_multiplier = config.loser_xp_multiplier;
        }
        
        // Send packet
        SendPacket(player_id, packet);
    }
    
    // [SEQUENCE: 2828] Log match results for analytics
    static void LogMatchResults(uint64_t match_id,
                               const ArenaMatch::MatchStatistics& stats) {
        Json::Value log_entry;
        
        log_entry["match_id"] = match_id;
        log_entry["duration_seconds"] = stats.match_duration_seconds;
        log_entry["total_kills"] = stats.total_kills;
        log_entry["total_damage"] = stats.total_damage;
        log_entry["total_healing"] = stats.total_healing;
        
        Json::Value teams(Json::arrayValue);
        for (const auto& team : stats.team_stats) {
            Json::Value team_data;
            team_data["team_id"] = team.team_id;
            team_data["score"] = team.score;
            team_data["is_winner"] = team.is_winner;
            
            Json::Value players(Json::arrayValue);
            for (const auto& player : team.players) {
                Json::Value player_data;
                player_data["player_id"] = player.player_id;
                player_data["name"] = player.player_name;
                player_data["kills"] = player.stats.kills;
                player_data["deaths"] = player.stats.deaths;
                player_data["assists"] = player.stats.assists;
                player_data["kda"] = CalculateKDA(player.stats);
                player_data["damage_dealt"] = player.stats.damage_dealt;
                player_data["healing_done"] = player.stats.healing_done;
                player_data["rating_change"] = player.rating_change;
                
                players.append(player_data);
            }
            team_data["players"] = players;
            
            teams.append(team_data);
        }
        log_entry["teams"] = teams;
        
        // MVP
        log_entry["mvp"]["player_id"] = stats.mvp_player_id;
        log_entry["mvp"]["reason"] = stats.mvp_reason;
        
        // Write to analytics log
        WriteAnalyticsLog("arena_matches", log_entry);
    }
    
    // [SEQUENCE: 2829] Check for arena achievements
    static void CheckArenaAchievements(const ArenaMatch::MatchStatistics& stats) {
        for (const auto& team : stats.team_stats) {
            for (const auto& player : team.players) {
                // Flawless Victory (no deaths)
                if (team.is_winner && player.stats.deaths == 0) {
                    GrantAchievement(player.player_id, "FLAWLESS_VICTORY");
                }
                
                // Killing Spree (10+ kills)
                if (player.stats.kills >= 10) {
                    GrantAchievement(player.player_id, "KILLING_SPREE");
                }
                
                // Healer achievement (50k+ healing)
                if (player.stats.healing_done >= 50000) {
                    GrantAchievement(player.player_id, "ARENA_HEALER");
                }
                
                // Tank achievement (100k+ damage taken)
                if (player.stats.damage_taken >= 100000) {
                    GrantAchievement(player.player_id, "ARENA_TANK");
                }
                
                // Perfect KDA (5+ KDA ratio)
                double kda = CalculateKDA(player.stats);
                if (kda >= 5.0) {
                    GrantAchievement(player.player_id, "PERFECT_KDA");
                }
            }
        }
        
        // Team achievements
        for (const auto& team : stats.team_stats) {
            if (team.is_winner && team.score >= 15 && 
                GetOpponentScore(stats, team.team_id) == 0) {
                // Shutout victory
                for (const auto& player : team.players) {
                    GrantAchievement(player.player_id, "SHUTOUT_VICTORY");
                }
            }
        }
    }
    
    // Helper functions
    static bool IsArenaCategory(MatchmakingCategory category) {
        return category == MatchmakingCategory::ARENA_1V1 ||
               category == MatchmakingCategory::ARENA_2V2 ||
               category == MatchmakingCategory::ARENA_3V3 ||
               category == MatchmakingCategory::ARENA_5V5;
    }
    
    static ArenaType GetArenaType(MatchmakingCategory category) {
        switch (category) {
            case MatchmakingCategory::ARENA_1V1: return ArenaType::ARENA_1V1;
            case MatchmakingCategory::ARENA_2V2: return ArenaType::ARENA_2V2;
            case MatchmakingCategory::ARENA_3V3: return ArenaType::ARENA_3V3;
            case MatchmakingCategory::ARENA_5V5: return ArenaType::ARENA_5V5;
            default: return ArenaType::ARENA_3V3;
        }
    }
    
    static RankingCategory GetRankingCategory(ArenaType type) {
        switch (type) {
            case ArenaType::ARENA_1V1: return RankingCategory::ARENA_1V1;
            case ArenaType::ARENA_2V2: return RankingCategory::ARENA_2V2;
            case ArenaType::ARENA_3V3: return RankingCategory::ARENA_3V3;
            case ArenaType::ARENA_5V5: return RankingCategory::ARENA_5V5;
            default: return RankingCategory::ARENA_3V3;
        }
    }
    
    static ArenaMap SelectRandomMap() {
        static std::vector<ArenaMap> maps = {
            ArenaMap::COLOSSEUM,
            ArenaMap::RUINS,
            ArenaMap::BRIDGE,
            ArenaMap::PILLARS,
            ArenaMap::MAZE,
            ArenaMap::FLOATING
        };
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, maps.size() - 1);
        
        return maps[dis(gen)];
    }
    
    static uint32_t GetArenaInstanceId(uint64_t match_id) {
        return 10000 + static_cast<uint32_t>(match_id);
    }
    
    static ArenaType ParseArenaType(const std::string& type_str) {
        if (type_str == "1v1") return ArenaType::ARENA_1V1;
        if (type_str == "2v2") return ArenaType::ARENA_2V2;
        if (type_str == "3v3") return ArenaType::ARENA_3V3;
        if (type_str == "5v5") return ArenaType::ARENA_5V5;
        return ArenaType::CUSTOM;
    }
    
    static double CalculateKDA(const ArenaPlayer::CombatStats& stats) {
        if (stats.deaths == 0) {
            return static_cast<double>(stats.kills + stats.assists);
        }
        return static_cast<double>(stats.kills + stats.assists) / stats.deaths;
    }
    
    static uint32_t GetOpponentScore(const ArenaMatch::MatchStatistics& stats,
                                    uint32_t team_id) {
        for (const auto& team : stats.team_stats) {
            if (team.team_id != team_id) {
                return team.score;
            }
        }
        return 0;
    }
    
    static ArenaConfig GetArenaConfig() {
        return ArenaConfig();
    }
    
    static void SendPacket(uint64_t player_id, const MatchSummaryPacket& packet) {
        // Implementation
    }
    
    static void WriteAnalyticsLog(const std::string& category, const Json::Value& data) {
        // Implementation
    }
    
    static void GrantAchievement(uint64_t player_id, const std::string& achievement) {
        // Implementation
    }
};

// [SEQUENCE: 2830] Arena map configurations
class ArenaMapConfig {
public:
    // [SEQUENCE: 2831] Get map-specific spawn points
    static std::vector<Vector3> GetSpawnPoints(ArenaMap map, uint32_t team_id) {
        switch (map) {
            case ArenaMap::COLOSSEUM:
                return GetColosseumSpawnPoints(team_id);
                
            case ArenaMap::RUINS:
                return GetRuinsSpawnPoints(team_id);
                
            case ArenaMap::BRIDGE:
                return GetBridgeSpawnPoints(team_id);
                
            case ArenaMap::PILLARS:
                return GetPillarsSpawnPoints(team_id);
                
            case ArenaMap::MAZE:
                return GetMazeSpawnPoints(team_id);
                
            case ArenaMap::FLOATING:
                return GetFloatingSpawnPoints(team_id);
                
            default:
                return GetDefaultSpawnPoints(team_id);
        }
    }
    
    // [SEQUENCE: 2832] Get map boundaries
    struct MapBounds {
        Vector3 min;
        Vector3 max;
        std::vector<Zone> danger_zones;  // 위험 지역 (낙사, 데미지 등)
    };
    
    static MapBounds GetMapBounds(ArenaMap map) {
        MapBounds bounds;
        
        switch (map) {
            case ArenaMap::COLOSSEUM:
                bounds.min = Vector3(-50, 0, -50);
                bounds.max = Vector3(50, 20, 50);
                break;
                
            case ArenaMap::BRIDGE:
                bounds.min = Vector3(-10, 0, -100);
                bounds.max = Vector3(10, 30, 100);
                // 다리 밖은 낙사 위험
                bounds.danger_zones.push_back({
                    Vector3(-50, -10, -100), 
                    Vector3(-10, 0, 100),
                    ZoneType::INSTANT_DEATH
                });
                bounds.danger_zones.push_back({
                    Vector3(10, -10, -100), 
                    Vector3(50, 0, 100),
                    ZoneType::INSTANT_DEATH
                });
                break;
                
            case ArenaMap::FLOATING:
                bounds.min = Vector3(-100, 0, -100);
                bounds.max = Vector3(100, 50, 100);
                // 플랫폼 사이 공간은 낙사
                GenerateFloatingPlatformDangerZones(bounds.danger_zones);
                break;
                
            default:
                bounds.min = Vector3(-75, 0, -75);
                bounds.max = Vector3(75, 30, 75);
                break;
        }
        
        return bounds;
    }
    
private:
    static std::vector<Vector3> GetColosseumSpawnPoints(uint32_t team_id) {
        if (team_id == 1) {
            return {
                Vector3(-30, 0, 0),
                Vector3(-35, 0, -5),
                Vector3(-35, 0, 5),
                Vector3(-40, 0, 0),
                Vector3(-40, 0, -10)
            };
        } else {
            return {
                Vector3(30, 0, 0),
                Vector3(35, 0, -5),
                Vector3(35, 0, 5),
                Vector3(40, 0, 0),
                Vector3(40, 0, -10)
            };
        }
    }
    
    static std::vector<Vector3> GetBridgeSpawnPoints(uint32_t team_id) {
        if (team_id == 1) {
            return {
                Vector3(0, 5, -80),
                Vector3(-5, 5, -80),
                Vector3(5, 5, -80),
                Vector3(0, 5, -85),
                Vector3(0, 5, -75)
            };
        } else {
            return {
                Vector3(0, 5, 80),
                Vector3(-5, 5, 80),
                Vector3(5, 5, 80),
                Vector3(0, 5, 85),
                Vector3(0, 5, 75)
            };
        }
    }
    
    static std::vector<Vector3> GetRuinsSpawnPoints(uint32_t team_id) {
        // Implementation...
        return GetDefaultSpawnPoints(team_id);
    }
    
    static std::vector<Vector3> GetPillarsSpawnPoints(uint32_t team_id) {
        // Implementation...
        return GetDefaultSpawnPoints(team_id);
    }
    
    static std::vector<Vector3> GetMazeSpawnPoints(uint32_t team_id) {
        // Implementation...
        return GetDefaultSpawnPoints(team_id);
    }
    
    static std::vector<Vector3> GetFloatingSpawnPoints(uint32_t team_id) {
        // Implementation...
        return GetDefaultSpawnPoints(team_id);
    }
    
    static std::vector<Vector3> GetDefaultSpawnPoints(uint32_t team_id) {
        float x_offset = (team_id == 1) ? -40.0f : 40.0f;
        return {
            Vector3(x_offset, 0, 0),
            Vector3(x_offset, 0, -10),
            Vector3(x_offset, 0, 10),
            Vector3(x_offset - 5, 0, -5),
            Vector3(x_offset - 5, 0, 5)
        };
    }
    
    static void GenerateFloatingPlatformDangerZones(std::vector<Zone>& zones) {
        // 플로팅 맵의 플랫폼 사이 공간을 위험 지역으로 설정
        // 실제 구현에서는 플랫폼 위치에 따라 동적으로 생성
    }
    
    struct Zone {
        Vector3 min;
        Vector3 max;
        ZoneType type;
    };
    
    enum class ZoneType {
        INSTANT_DEATH,
        DAMAGE_OVER_TIME,
        SLOW,
        SILENCE
    };
};

// [SEQUENCE: 2833] Arena season management
class ArenaSeasonManager {
public:
    struct ArenaSeason {
        uint32_t season_id;
        std::string season_name;
        std::chrono::system_clock::time_point start_date;
        std::chrono::system_clock::time_point end_date;
        
        // Season-specific rules
        bool special_rules_enabled{false};
        std::string special_rule_description;
        
        // Season rewards
        struct SeasonRewards {
            std::unordered_map<RankingTier, std::vector<uint32_t>> tier_rewards;
            std::unordered_map<uint32_t, uint32_t> rating_milestone_rewards;
            uint32_t participation_reward{0};
            uint32_t season_exclusive_mount{0};
        } rewards;
    };
    
    // [SEQUENCE: 2834] Create new arena season
    static ArenaSeason CreateArenaSeason(uint32_t season_id,
                                        const std::string& name,
                                        uint32_t duration_days) {
        ArenaSeason season;
        
        season.season_id = season_id;
        season.season_name = name;
        season.start_date = std::chrono::system_clock::now();
        season.end_date = season.start_date + std::chrono::hours(24 * duration_days);
        
        // Define season rewards
        DefineSeasonRewards(season);
        
        // Special rules for this season
        if (season_id % 4 == 0) {
            season.special_rules_enabled = true;
            season.special_rule_description = "Double damage season - All damage increased by 100%";
        }
        
        return season;
    }
    
private:
    static void DefineSeasonRewards(ArenaSeason& season) {
        // Tier-based rewards
        season.rewards.tier_rewards[RankingTier::BRONZE] = {30001, 30002};
        season.rewards.tier_rewards[RankingTier::SILVER] = {30003, 30004};
        season.rewards.tier_rewards[RankingTier::GOLD] = {30005, 30006, 30007};
        season.rewards.tier_rewards[RankingTier::PLATINUM] = {30008, 30009, 30010};
        season.rewards.tier_rewards[RankingTier::DIAMOND] = {30011, 30012, 30013};
        season.rewards.tier_rewards[RankingTier::MASTER] = {30014, 30015, 30016};
        season.rewards.tier_rewards[RankingTier::GRANDMASTER] = {30017, 30018, 30019};
        season.rewards.tier_rewards[RankingTier::CHALLENGER] = {30020, 30021, 30022};
        
        // Rating milestones
        season.rewards.rating_milestone_rewards[1600] = 30100; // Weapon skin
        season.rewards.rating_milestone_rewards[1800] = 30101; // Armor set
        season.rewards.rating_milestone_rewards[2000] = 30102; // Title
        season.rewards.rating_milestone_rewards[2200] = 30103; // Mount
        season.rewards.rating_milestone_rewards[2400] = 30104; // Exclusive mount
        
        // Participation reward
        season.rewards.participation_reward = 30000;
        
        // Season exclusive mount for top players
        season.rewards.season_exclusive_mount = 50200 + season.season_id;
    }
};

} // namespace mmorpg::arena