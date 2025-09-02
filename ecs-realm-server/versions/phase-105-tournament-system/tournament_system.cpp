#include "tournament_system.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <random>

namespace mmorpg::tournament {

// [SEQUENCE: 2870] Tournament system integration
class TournamentIntegration {
public:
    // [SEQUENCE: 2871] Initialize tournament system with game server
    static void InitializeWithGameServer(GameServer* server,
                                       TournamentSystem* tournament_system,
                                       ArenaSystem* arena_system,
                                       RankingService* ranking_service) {
        
        // [SEQUENCE: 2872] Handle tournament registration command
        server->RegisterCommand("tournament", [tournament_system, server](
            uint64_t player_id, const std::vector<std::string>& args) {
            
            if (args.empty()) {
                ShowTournamentHelp(server, player_id);
                return;
            }
            
            const std::string& subcommand = args[0];
            
            if (subcommand == "list") {
                ListTournaments(server, player_id, tournament_system);
            }
            else if (subcommand == "info" && args.size() >= 2) {
                uint64_t tournament_id = std::stoull(args[1]);
                ShowTournamentInfo(server, player_id, tournament_system, tournament_id);
            }
            else if (subcommand == "register" && args.size() >= 2) {
                uint64_t tournament_id = std::stoull(args[1]);
                RegisterForTournament(server, player_id, tournament_system, 
                                    tournament_id, ranking_service);
            }
            else if (subcommand == "checkin" && args.size() >= 2) {
                uint64_t tournament_id = std::stoull(args[1]);
                CheckInForTournament(server, player_id, tournament_system, tournament_id);
            }
            else if (subcommand == "standings" && args.size() >= 2) {
                uint64_t tournament_id = std::stoull(args[1]);
                ShowTournamentStandings(server, player_id, tournament_system, tournament_id);
            }
            else {
                ShowTournamentHelp(server, player_id);
            }
        });
        
        // [SEQUENCE: 2873] Handle arena match completion for tournaments
        arena_system->OnMatchComplete = [tournament_system](
            uint64_t arena_match_id,
            uint64_t winner_id,
            const ArenaMatch::MatchStatistics& stats) {
            
            // Check if this is a tournament match
            tournament_system->ProcessArenaMatchCompletion(
                arena_match_id, winner_id, stats);
        };
        
        // [SEQUENCE: 2874] Create recurring tournaments
        CreateDailyTournaments(tournament_system);
        CreateWeeklyTournaments(tournament_system);
        CreateSpecialEventTournaments(tournament_system);
        
        // [SEQUENCE: 2875] Tournament notification system
        server->ScheduleRecurringTask("tournament_notifications",
            std::chrono::minutes(5), [tournament_system, server]() {
                
                auto schedule = tournament_system->GetTournamentSchedule();
                
                // Notify about tournaments starting soon
                for (const auto& tournament : schedule.upcoming) {
                    auto time_until_start = tournament.start_time - 
                        std::chrono::system_clock::now();
                    
                    auto minutes = std::chrono::duration_cast<std::chrono::minutes>(
                        time_until_start).count();
                    
                    if (minutes == 30) {
                        std::string message = "Tournament '" + tournament.name + 
                            "' starts in 30 minutes! Use /tournament register " + 
                            std::to_string(tournament.tournament_id) + " to join!";
                        server->BroadcastAnnouncement(message);
                    }
                    else if (minutes == 5) {
                        std::string message = "Tournament '" + tournament.name + 
                            "' starts in 5 minutes! Check-in now!";
                        server->BroadcastAnnouncement(message);
                    }
                }
            });
    }
    
private:
    // [SEQUENCE: 2876] Show tournament help
    static void ShowTournamentHelp(GameServer* server, uint64_t player_id) {
        std::stringstream help;
        help << "=== Tournament Commands ===\n"
             << "/tournament list - Show upcoming tournaments\n"
             << "/tournament info <id> - Show tournament details\n"
             << "/tournament register <id> - Register for tournament\n"
             << "/tournament checkin <id> - Check in for tournament\n"
             << "/tournament standings <id> - Show current standings\n";
        
        server->SendMessage(player_id, help.str());
    }
    
    // [SEQUENCE: 2877] List available tournaments
    static void ListTournaments(GameServer* server, uint64_t player_id,
                              TournamentSystem* tournament_system) {
        auto schedule = tournament_system->GetTournamentSchedule();
        
        std::stringstream msg;
        msg << "=== Tournament Schedule ===\n\n";
        
        // Upcoming tournaments
        if (!schedule.upcoming.empty()) {
            msg << "UPCOMING TOURNAMENTS:\n";
            for (const auto& t : schedule.upcoming) {
                msg << FormatTournamentListing(t) << "\n";
            }
            msg << "\n";
        }
        
        // In progress tournaments
        if (!schedule.in_progress.empty()) {
            msg << "IN PROGRESS:\n";
            for (const auto& t : schedule.in_progress) {
                msg << FormatTournamentListing(t) << "\n";
            }
            msg << "\n";
        }
        
        // Recently completed
        if (!schedule.recent_completed.empty()) {
            msg << "RECENTLY COMPLETED:\n";
            for (const auto& t : schedule.recent_completed) {
                msg << FormatTournamentListing(t) << "\n";
            }
        }
        
        server->SendMessage(player_id, msg.str());
    }
    
    // [SEQUENCE: 2878] Show tournament information
    static void ShowTournamentInfo(GameServer* server, uint64_t player_id,
                                 TournamentSystem* tournament_system,
                                 uint64_t tournament_id) {
        auto tournament = tournament_system->GetTournament(tournament_id);
        if (!tournament) {
            server->SendMessage(player_id, "Tournament not found.");
            return;
        }
        
        const auto& config = tournament->GetConfig();
        
        std::stringstream info;
        info << "=== " << config.tournament_name << " ===\n\n"
             << "Format: " << GetFormatName(config.format) << "\n"
             << "Type: " << GetArenaTypeName(config.arena_type) << "\n"
             << "Status: " << GetStateName(tournament->GetState()) << "\n"
             << "Participants: " << tournament->GetParticipantCount() 
             << "/" << config.max_participants << "\n\n";
        
        // Schedule
        info << "SCHEDULE:\n"
             << "Registration ends: " << FormatTime(config.registration_end) << "\n"
             << "Tournament starts: " << FormatTime(config.tournament_start) << "\n\n";
        
        // Requirements
        const auto& req = config.requirements;
        info << "REQUIREMENTS:\n";
        if (req.minimum_rating > 0) {
            info << "- Rating: " << req.minimum_rating << "-" << req.maximum_rating << "\n";
        }
        if (req.minimum_level > 1) {
            info << "- Level: " << req.minimum_level << "-" << req.maximum_level << "\n";
        }
        if (req.minimum_arena_matches > 0) {
            info << "- Min arena matches: " << req.minimum_arena_matches << "\n";
        }
        if (req.entry_fee_gold > 0 || req.entry_fee_tokens > 0) {
            info << "- Entry fee: ";
            if (req.entry_fee_gold > 0) info << req.entry_fee_gold << " gold ";
            if (req.entry_fee_tokens > 0) info << req.entry_fee_tokens << " tokens";
            info << "\n";
        }
        info << "\n";
        
        // Rewards
        info << "REWARDS:\n";
        for (const auto& [placement, reward] : config.rewards) {
            info << GetPlacementString(placement) << ": ";
            if (reward.gold > 0) info << reward.gold << " gold ";
            if (reward.honor_points > 0) info << reward.honor_points << " honor ";
            if (!reward.title.empty()) info << "[" << reward.title << "] ";
            info << "\n";
        }
        
        server->SendMessage(player_id, info.str());
    }
    
    // [SEQUENCE: 2879] Register player for tournament
    static void RegisterForTournament(GameServer* server, uint64_t player_id,
                                    TournamentSystem* tournament_system,
                                    uint64_t tournament_id,
                                    RankingService* ranking_service) {
        auto tournament = tournament_system->GetTournament(tournament_id);
        if (!tournament) {
            server->SendMessage(player_id, "Tournament not found.");
            return;
        }
        
        // Check requirements
        const auto& req = tournament->GetConfig().requirements;
        
        // Check rating
        if (req.minimum_rating > 0) {
            int32_t player_rating = ranking_service->GetPlayerRating(
                player_id, GetRankingCategory(tournament->GetConfig().arena_type));
            
            if (player_rating < req.minimum_rating || 
                player_rating > req.maximum_rating) {
                server->SendMessage(player_id, 
                    "Your rating (" + std::to_string(player_rating) + 
                    ") does not meet the requirements.");
                return;
            }
        }
        
        // Check level
        uint32_t player_level = server->GetPlayerLevel(player_id);
        if (player_level < req.minimum_level || 
            player_level > req.maximum_level) {
            server->SendMessage(player_id, "Your level does not meet the requirements.");
            return;
        }
        
        // Check entry fee
        if (req.entry_fee_gold > 0) {
            if (!server->DeductGold(player_id, req.entry_fee_gold)) {
                server->SendMessage(player_id, "Insufficient gold for entry fee.");
                return;
            }
        }
        
        // Register
        std::string player_name = server->GetPlayerName(player_id);
        if (tournament_system->RegisterForTournament(tournament_id, player_id, player_name)) {
            server->SendMessage(player_id, 
                "Successfully registered for " + tournament->GetConfig().tournament_name + 
                "! Don't forget to check in before the tournament starts.");
        } else {
            server->SendMessage(player_id, "Failed to register. Tournament may be full.");
            // Refund entry fee if registration failed
            if (req.entry_fee_gold > 0) {
                server->GrantGold(player_id, req.entry_fee_gold);
            }
        }
    }
    
    // [SEQUENCE: 2880] Check in for tournament
    static void CheckInForTournament(GameServer* server, uint64_t player_id,
                                   TournamentSystem* tournament_system,
                                   uint64_t tournament_id) {
        auto tournament = tournament_system->GetTournament(tournament_id);
        if (!tournament) {
            server->SendMessage(player_id, "Tournament not found.");
            return;
        }
        
        if (tournament->GetState() != Tournament::TournamentState::CHECK_IN) {
            server->SendMessage(player_id, "Check-in is not available at this time.");
            return;
        }
        
        if (tournament->CheckInParticipant(player_id)) {
            server->SendMessage(player_id, "Successfully checked in! Good luck!");
        } else {
            server->SendMessage(player_id, "Check-in failed. Are you registered?");
        }
    }
    
    // [SEQUENCE: 2881] Show tournament standings
    static void ShowTournamentStandings(GameServer* server, uint64_t player_id,
                                      TournamentSystem* tournament_system,
                                      uint64_t tournament_id) {
        auto tournament = tournament_system->GetTournament(tournament_id);
        if (!tournament) {
            server->SendMessage(player_id, "Tournament not found.");
            return;
        }
        
        auto standings = tournament->GetStandings();
        
        std::stringstream msg;
        msg << "=== " << tournament->GetConfig().tournament_name << " Standings ===\n";
        msg << "Round: " << tournament->GetCurrentRound() << "\n\n";
        
        int rank = 1;
        for (const auto& [participant, placement] : standings) {
            msg << rank << ". " << participant.name 
                << " (" << participant.stats.matches_won << "-" 
                << participant.stats.matches_played - participant.stats.matches_won << ")\n";
            rank++;
            
            if (rank > 10) {
                msg << "... and " << (standings.size() - 10) << " more\n";
                break;
            }
        }
        
        server->SendMessage(player_id, msg.str());
    }
    
    // [SEQUENCE: 2882] Create daily tournaments
    static void CreateDailyTournaments(TournamentSystem* tournament_system) {
        // Daily 1v1 tournament
        TournamentConfig daily_1v1;
        daily_1v1.tournament_name = "Daily 1v1 Championship";
        daily_1v1.format = TournamentFormat::SINGLE_ELIMINATION;
        daily_1v1.arena_type = ArenaType::ARENA_1V1;
        
        // Schedule for 8 PM server time
        auto now = std::chrono::system_clock::now();
        auto today_8pm = GetTodayTime(20, 0); // 8 PM
        if (today_8pm <= now) {
            today_8pm += std::chrono::hours(24); // Tomorrow
        }
        
        daily_1v1.registration_start = now;
        daily_1v1.registration_end = today_8pm - std::chrono::minutes(30);
        daily_1v1.tournament_start = today_8pm;
        
        daily_1v1.min_participants = 8;
        daily_1v1.max_participants = 64;
        
        // Requirements
        daily_1v1.requirements.minimum_rating = 1200;
        daily_1v1.requirements.minimum_arena_matches = 10;
        daily_1v1.requirements.entry_fee_gold = 100;
        
        // Rewards
        daily_1v1.rewards[1] = {1, 1000, 100, 50, {40001}, "Daily Champion", 0, 0, 50};
        daily_1v1.rewards[2] = {2, 500, 50, 25, {40002}, "", 0, 0, 25};
        daily_1v1.rewards[3] = {3, 250, 25, 10, {40003}, "", 0, 0, 10};
        
        tournament_system->CreateTournament(daily_1v1);
        
        // Daily 3v3 tournament
        TournamentConfig daily_3v3;
        daily_3v3.tournament_name = "Daily 3v3 Arena Cup";
        daily_3v3.format = TournamentFormat::DOUBLE_ELIMINATION;
        daily_3v3.arena_type = ArenaType::ARENA_3V3;
        
        daily_3v3.registration_start = now;
        daily_3v3.registration_end = today_8pm - std::chrono::minutes(30);
        daily_3v3.tournament_start = today_8pm + std::chrono::minutes(30);
        
        daily_3v3.min_participants = 8;
        daily_3v3.max_participants = 32;
        daily_3v3.requirements.team_size = 3;
        
        tournament_system->CreateTournament(daily_3v3);
    }
    
    // [SEQUENCE: 2883] Create weekly tournaments
    static void CreateWeeklyTournaments(TournamentSystem* tournament_system) {
        // Weekly championship - Saturday 6 PM
        TournamentConfig weekly_championship;
        weekly_championship.tournament_name = "Weekly Arena Championship";
        weekly_championship.format = TournamentFormat::SWISS;
        weekly_championship.arena_type = ArenaType::ARENA_3V3;
        
        auto saturday_6pm = GetNextWeekdayTime(6, 18, 0); // Saturday 6 PM
        
        weekly_championship.registration_start = saturday_6pm - std::chrono::hours(48);
        weekly_championship.registration_end = saturday_6pm - std::chrono::hours(1);
        weekly_championship.tournament_start = saturday_6pm;
        
        weekly_championship.min_participants = 16;
        weekly_championship.max_participants = 128;
        
        // Higher requirements for weekly
        weekly_championship.requirements.minimum_rating = 1500;
        weekly_championship.requirements.minimum_arena_matches = 50;
        weekly_championship.requirements.minimum_win_rate = 45;
        weekly_championship.requirements.entry_fee_tokens = 10;
        
        // Better rewards
        weekly_championship.rewards[1] = {1, 5000, 500, 200, {40101, 40102}, 
                                        "Weekly Champion", 50001, 0, 100};
        weekly_championship.rewards[2] = {2, 2500, 250, 100, {40103}, "", 0, 0, 50};
        weekly_championship.rewards[3] = {3, 1000, 100, 50, {40104}, "", 0, 0, 25};
        
        // Top 8 get rewards
        for (uint32_t i = 4; i <= 8; ++i) {
            weekly_championship.rewards[i] = {i, 500, 50, 25, {}, "", 0, 0, 10};
        }
        
        tournament_system->CreateTournament(weekly_championship);
    }
    
    // [SEQUENCE: 2884] Create special event tournaments
    static void CreateSpecialEventTournaments(TournamentSystem* tournament_system) {
        // Monthly championship - First Saturday of month
        if (IsFirstSaturdayOfMonth()) {
            TournamentConfig monthly_championship;
            monthly_championship.tournament_name = "Monthly Grand Championship";
            monthly_championship.format = TournamentFormat::DOUBLE_ELIMINATION;
            monthly_championship.arena_type = ArenaType::ARENA_3V3;
            
            auto saturday_8pm = GetNextWeekdayTime(6, 20, 0); // Saturday 8 PM
            
            monthly_championship.registration_start = saturday_8pm - std::chrono::hours(168); // 1 week
            monthly_championship.registration_end = saturday_8pm - std::chrono::hours(2);
            monthly_championship.tournament_start = saturday_8pm;
            
            monthly_championship.min_participants = 32;
            monthly_championship.max_participants = 256;
            
            // Elite requirements
            monthly_championship.requirements.minimum_rating = 1800;
            monthly_championship.requirements.minimum_arena_matches = 100;
            monthly_championship.requirements.minimum_win_rate = 50;
            monthly_championship.requirements.entry_fee_tokens = 50;
            
            // Exclusive rewards
            monthly_championship.rewards[1] = {1, 20000, 2000, 1000, 
                {40201, 40202, 40203}, "Grand Champion", 50002, 50101, 250};
            
            tournament_system->CreateTournament(monthly_championship);
        }
        
        // Holiday tournaments
        if (IsHolidayPeriod()) {
            CreateHolidayTournament(tournament_system);
        }
    }
    
    // [SEQUENCE: 2885] Format tournament listing
    static std::string FormatTournamentListing(
        const TournamentSystem::TournamentSchedule::ScheduledTournament& t) {
        std::stringstream ss;
        ss << "[" << t.tournament_id << "] " << t.name 
           << " (" << GetFormatName(t.format) << " " 
           << GetArenaTypeName(t.arena_type) << ") - "
           << t.registered_count << "/" << t.max_participants
           << " - Starts: " << FormatTimeRelative(t.start_time);
        return ss.str();
    }
    
    // Helper functions
    static std::string GetFormatName(TournamentFormat format) {
        switch (format) {
            case TournamentFormat::SINGLE_ELIMINATION: return "Single Elim";
            case TournamentFormat::DOUBLE_ELIMINATION: return "Double Elim";
            case TournamentFormat::ROUND_ROBIN: return "Round Robin";
            case TournamentFormat::SWISS: return "Swiss";
            case TournamentFormat::LADDER: return "Ladder";
            default: return "Custom";
        }
    }
    
    static std::string GetArenaTypeName(ArenaType type) {
        switch (type) {
            case ArenaType::ARENA_1V1: return "1v1";
            case ArenaType::ARENA_2V2: return "2v2";
            case ArenaType::ARENA_3V3: return "3v3";
            case ArenaType::ARENA_5V5: return "5v5";
            case ArenaType::DEATHMATCH: return "FFA";
            default: return "Unknown";
        }
    }
    
    static std::string GetStateName(Tournament::TournamentState state) {
        switch (state) {
            case Tournament::TournamentState::REGISTRATION: return "Registration Open";
            case Tournament::TournamentState::CHECK_IN: return "Check-In Period";
            case Tournament::TournamentState::BRACKET_GENERATION: return "Generating Bracket";
            case Tournament::TournamentState::IN_PROGRESS: return "In Progress";
            case Tournament::TournamentState::COMPLETED: return "Completed";
            case Tournament::TournamentState::CANCELLED: return "Cancelled";
            default: return "Unknown";
        }
    }
    
    static std::string GetPlacementString(uint32_t placement) {
        switch (placement) {
            case 1: return "1st Place";
            case 2: return "2nd Place";
            case 3: return "3rd Place";
            default: return std::to_string(placement) + "th Place";
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
    
    static std::string FormatTime(std::chrono::system_clock::time_point time) {
        auto time_t = std::chrono::system_clock::to_time_t(time);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M");
        return ss.str();
    }
    
    static std::string FormatTimeRelative(std::chrono::system_clock::time_point time) {
        auto now = std::chrono::system_clock::now();
        auto diff = time - now;
        
        auto hours = std::chrono::duration_cast<std::chrono::hours>(diff).count();
        auto minutes = std::chrono::duration_cast<std::chrono::minutes>(diff).count() % 60;
        
        if (hours > 24) {
            return "in " + std::to_string(hours / 24) + " days";
        } else if (hours > 0) {
            return "in " + std::to_string(hours) + "h " + std::to_string(minutes) + "m";
        } else if (minutes > 0) {
            return "in " + std::to_string(minutes) + " minutes";
        } else {
            return "now";
        }
    }
    
    static std::chrono::system_clock::time_point GetTodayTime(int hour, int minute) {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        
        tm.tm_hour = hour;
        tm.tm_min = minute;
        tm.tm_sec = 0;
        
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }
    
    static std::chrono::system_clock::time_point GetNextWeekdayTime(
        int weekday, int hour, int minute) {
        // Implementation to get next occurrence of weekday at specified time
        return std::chrono::system_clock::now() + std::chrono::hours(24);
    }
    
    static bool IsFirstSaturdayOfMonth() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto tm = *std::localtime(&time_t);
        
        return tm.tm_wday == 6 && tm.tm_mday <= 7;
    }
    
    static bool IsHolidayPeriod() {
        // Check if current date is during a holiday period
        return false;
    }
    
    static void CreateHolidayTournament(TournamentSystem* tournament_system) {
        // Create special holiday-themed tournament
    }
};

// [SEQUENCE: 2886] Tournament bracket algorithms
class BracketAlgorithms {
public:
    // [SEQUENCE: 2887] Standard tournament seeding
    static std::vector<std::pair<uint64_t, uint64_t>> CreateStandardSeeding(
        const std::vector<uint64_t>& participants) {
        
        std::vector<std::pair<uint64_t, uint64_t>> matches;
        
        if (participants.size() < 2) {
            return matches;
        }
        
        // Standard seeding: 1 vs last, 2 vs second-last, etc.
        size_t half = participants.size() / 2;
        for (size_t i = 0; i < half; ++i) {
            matches.push_back({
                participants[i],
                participants[participants.size() - 1 - i]
            });
        }
        
        return matches;
    }
    
    // [SEQUENCE: 2888] Swiss pairing algorithm
    static std::vector<std::pair<uint64_t, uint64_t>> CreateSwissPairing(
        const std::unordered_map<uint64_t, TournamentParticipant>& participants,
        uint32_t round) {
        
        std::vector<std::pair<uint64_t, uint64_t>> matches;
        
        // Group players by score
        std::unordered_map<uint32_t, std::vector<uint64_t>> score_groups;
        for (const auto& [id, participant] : participants) {
            score_groups[participant.wins].push_back(id);
        }
        
        // Pair within score groups
        for (auto& [score, group] : score_groups) {
            // Shuffle to add randomness
            std::random_device rd;
            std::mt19937 gen(rd());
            std::shuffle(group.begin(), group.end(), gen);
            
            // Pair adjacent players
            for (size_t i = 0; i + 1 < group.size(); i += 2) {
                matches.push_back({group[i], group[i + 1]});
            }
            
            // Handle odd player (pair with someone from adjacent score group)
            if (group.size() % 2 == 1) {
                // Implementation for odd player
            }
        }
        
        return matches;
    }
    
    // [SEQUENCE: 2889] Round robin scheduling
    static std::vector<std::vector<std::pair<uint64_t, uint64_t>>> CreateRoundRobinSchedule(
        const std::vector<uint64_t>& participants) {
        
        std::vector<std::vector<std::pair<uint64_t, uint64_t>>> rounds;
        size_t n = participants.size();
        
        if (n < 2) {
            return rounds;
        }
        
        // Handle odd number of participants
        std::vector<uint64_t> players = participants;
        bool has_bye = (n % 2 == 1);
        if (has_bye) {
            players.push_back(0); // 0 represents bye
            n++;
        }
        
        // Generate round-robin schedule
        for (size_t round = 0; round < n - 1; ++round) {
            std::vector<std::pair<uint64_t, uint64_t>> round_matches;
            
            for (size_t i = 0; i < n / 2; ++i) {
                uint64_t player1 = players[i];
                uint64_t player2 = players[n - 1 - i];
                
                if (player1 != 0 && player2 != 0) {
                    round_matches.push_back({player1, player2});
                }
            }
            
            rounds.push_back(round_matches);
            
            // Rotate players (keep first player fixed)
            uint64_t last = players[n - 1];
            for (size_t i = n - 1; i > 1; --i) {
                players[i] = players[i - 1];
            }
            players[1] = last;
        }
        
        return rounds;
    }
};

// [SEQUENCE: 2890] Tournament analytics
class TournamentAnalytics {
public:
    // [SEQUENCE: 2891] Log tournament completion
    static void LogTournamentCompletion(uint64_t tournament_id,
                                      const Tournament& tournament,
                                      const std::vector<std::pair<TournamentParticipant, uint32_t>>& final_standings) {
        Json::Value log_entry;
        
        const auto& config = tournament.GetConfig();
        
        // Tournament info
        log_entry["tournament_id"] = tournament_id;
        log_entry["name"] = config.tournament_name;
        log_entry["format"] = static_cast<int>(config.format);
        log_entry["arena_type"] = static_cast<int>(config.arena_type);
        log_entry["total_participants"] = static_cast<uint32_t>(final_standings.size());
        
        // Duration
        auto duration = config.tournament_start - 
            std::chrono::system_clock::now();
        log_entry["duration_minutes"] = 
            std::chrono::duration_cast<std::chrono::minutes>(duration).count();
        
        // Top finishers
        Json::Value top_finishers(Json::arrayValue);
        for (size_t i = 0; i < std::min(size_t(10), final_standings.size()); ++i) {
            const auto& [participant, placement] = final_standings[i];
            
            Json::Value finisher;
            finisher["placement"] = placement;
            finisher["participant_id"] = participant.participant_id;
            finisher["name"] = participant.name;
            finisher["matches_played"] = participant.stats.matches_played;
            finisher["matches_won"] = participant.stats.matches_won;
            finisher["total_kills"] = participant.stats.total_kills;
            finisher["total_deaths"] = participant.stats.total_deaths;
            
            top_finishers.append(finisher);
        }
        log_entry["top_finishers"] = top_finishers;
        
        // Write to analytics
        WriteAnalyticsLog("tournament_completions", log_entry);
    }
    
    // [SEQUENCE: 2892] Generate tournament report
    static TournamentReport GenerateTournamentReport(
        uint64_t tournament_id,
        const Tournament& tournament,
        const std::vector<std::pair<TournamentParticipant, uint32_t>>& final_standings) {
        
        TournamentReport report;
        report.tournament_id = tournament_id;
        report.tournament_name = tournament.GetConfig().tournament_name;
        
        // Calculate statistics
        uint32_t total_matches = 0;
        uint64_t total_kills = 0;
        uint64_t total_damage = 0;
        
        for (const auto& [participant, placement] : final_standings) {
            total_matches += participant.stats.matches_played;
            total_kills += participant.stats.total_kills;
            total_damage += participant.stats.total_damage;
        }
        
        report.total_matches = total_matches / 2; // Each match counted twice
        report.average_match_duration = 8.5; // minutes (example)
        report.total_kills = total_kills;
        report.total_damage = total_damage;
        
        // Most valuable player
        if (!final_standings.empty()) {
            report.champion_id = final_standings[0].first.participant_id;
            report.champion_name = final_standings[0].first.name;
        }
        
        // Popular strategies (example)
        report.most_picked_map = "Colosseum";
        report.average_match_score = "15-12";
        
        return report;
    }
    
    struct TournamentReport {
        uint64_t tournament_id;
        std::string tournament_name;
        uint32_t total_matches;
        double average_match_duration;
        uint64_t total_kills;
        uint64_t total_damage;
        uint64_t champion_id;
        std::string champion_name;
        std::string most_picked_map;
        std::string average_match_score;
    };
    
private:
    static void WriteAnalyticsLog(const std::string& category, const Json::Value& data) {
        // Implementation
    }
};

} // namespace mmorpg::tournament