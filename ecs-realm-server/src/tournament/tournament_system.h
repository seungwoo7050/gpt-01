#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <optional>
#include <queue>
#include <mutex>
#include <atomic>
#include "../core/types.h"
#include "../arena/arena_system.h"
#include "../ranking/ranking_system.h"

namespace mmorpg::tournament {

// [SEQUENCE: MVP13-137] Tournament types and formats
enum class TournamentFormat {
    SINGLE_ELIMINATION,     // 단판 토너먼트
    DOUBLE_ELIMINATION,     // 패자부활전 있음
    ROUND_ROBIN,           // 리그전 (모두와 경기)
    SWISS,                 // 스위스 방식
    LADDER,                // 래더 토너먼트
    CUSTOM                 // 커스텀 규칙
};

// [SEQUENCE: MVP13-138] Tournament entry requirements
struct TournamentRequirements {
    // Rating requirements
    int32_t minimum_rating{0};
    int32_t maximum_rating{9999};
    
    // Level requirements
    uint32_t minimum_level{1};
    uint32_t maximum_level{100};
    
    // Participation requirements
    uint32_t minimum_arena_matches{10};
    uint32_t minimum_win_rate{0};  // Percentage (0-100)
    
    // Entry fee
    uint32_t entry_fee_gold{0};
    uint32_t entry_fee_tokens{0};
    
    // Team requirements (for team tournaments)
    uint32_t team_size{1};
    bool require_guild_team{false};
    
    // Equipment restrictions
    bool allow_consumables{false};
    bool normalize_gear{true};
    std::vector<uint32_t> banned_items;
};

// [SEQUENCE: MVP13-139] Tournament configuration
struct TournamentConfig {
    std::string tournament_name;
    TournamentFormat format{TournamentFormat::SINGLE_ELIMINATION};
    ArenaType arena_type{ArenaType::ARENA_3V3};
    
    // Schedule
    std::chrono::system_clock::time_point registration_start;
    std::chrono::system_clock::time_point registration_end;
    std::chrono::system_clock::time_point tournament_start;
    
    // Capacity
    uint32_t min_participants{8};
    uint32_t max_participants{128};
    
    // Match settings
    ArenaConfig default_arena_config;
    uint32_t round_duration_minutes{15};
    uint32_t break_between_rounds_minutes{5};
    
    // Requirements
    TournamentRequirements requirements;
    
    // Rewards (by placement)
    std::unordered_map<uint32_t, TournamentReward> rewards;
};

// [SEQUENCE: MVP13-140] Tournament reward structure
struct TournamentReward {
    uint32_t placement;  // 1st, 2nd, 3rd, etc.
    
    // Currency rewards
    uint32_t gold{0};
    uint32_t honor_points{0};
    uint32_t tournament_tokens{0};
    
    // Item rewards
    std::vector<uint32_t> item_ids;
    
    // Special rewards
    std::string title;
    uint32_t achievement_id{0};
    uint32_t mount_id{0};
    
    // Rating bonus
    int32_t rating_bonus{0};
};

// [SEQUENCE: MVP13-141] Tournament participant
struct TournamentParticipant {
    uint64_t participant_id;  // Player or team ID
    std::string name;
    
    // Registration info
    std::chrono::system_clock::time_point registration_time;
    bool is_checked_in{false};
    
    // Tournament progress
    uint32_t current_round{0};
    uint32_t wins{0};
    uint32_t losses{0};
    uint32_t bracket_position{0};
    
    // Stats
    struct TournamentStats {
        uint32_t matches_played{0};
        uint32_t matches_won{0};
        uint32_t total_kills{0};
        uint32_t total_deaths{0};
        uint64_t total_damage{0};
        uint64_t total_healing{0};
    } stats;
    
    // For team tournaments
    std::vector<uint64_t> team_members;
    uint64_t team_captain{0};
};

// [SEQUENCE: MVP13-142] Tournament match
struct TournamentMatch {
    uint64_t match_id;
    uint32_t round_number;
    uint32_t bracket_position;
    
    // Participants
    uint64_t participant1_id;
    uint64_t participant2_id;
    
    // Match state
    enum class MatchState {
        SCHEDULED,
        READY,
        IN_PROGRESS,
        COMPLETED,
        NO_SHOW
    } state{MatchState::SCHEDULED};
    
    // Results
    uint64_t winner_id{0};
    uint64_t arena_match_id{0};  // Reference to arena match
    
    // Schedule
    std::chrono::system_clock::time_point scheduled_time;
    std::chrono::system_clock::time_point actual_start_time;
    std::chrono::system_clock::time_point end_time;
};

// [SEQUENCE: MVP13-143] Tournament bracket
class TournamentBracket {
public:
    // [SEQUENCE: MVP13-144] Initialize bracket
    TournamentBracket(TournamentFormat format, uint32_t participant_count)
        : format_(format)
        , participant_count_(participant_count) {
        
        GenerateBracket();
    }
    
    // [SEQUENCE: MVP13-145] Generate initial bracket
    void GenerateBracket() {
        switch (format_) {
            case TournamentFormat::SINGLE_ELIMINATION:
                GenerateSingleElimination();
                break;
                
            case TournamentFormat::DOUBLE_ELIMINATION:
                GenerateDoubleElimination();
                break;
                
            case TournamentFormat::ROUND_ROBIN:
                GenerateRoundRobin();
                break;
                
            case TournamentFormat::SWISS:
                GenerateSwiss();
                break;
                
            default:
                spdlog::error("Unsupported tournament format");
                break;
        }
    }
    
    // [SEQUENCE: MVP13-146] Get next matches for round
    std::vector<TournamentMatch> GetRoundMatches(uint32_t round) const {
        std::vector<TournamentMatch> round_matches;
        
        for (const auto& [match_id, match] : matches_) {
            if (match.round_number == round) {
                round_matches.push_back(match);
            }
        }
        
        return round_matches;
    }
    
    // [SEQUENCE: MVP13-147] Update match result
    void UpdateMatchResult(uint64_t match_id, uint64_t winner_id) {
        auto it = matches_.find(match_id);
        if (it == matches_.end()) {
            return;
        }
        
        it->second.winner_id = winner_id;
        it->second.state = TournamentMatch::MatchState::COMPLETED;
        
        // Progress winner to next round
        ProgressWinner(match_id, winner_id);
        
        // Handle loser in double elimination
        if (format_ == TournamentFormat::DOUBLE_ELIMINATION) {
            uint64_t loser_id = (winner_id == it->second.participant1_id) ?
                it->second.participant2_id : it->second.participant1_id;
            HandleLoserBracket(match_id, loser_id);
        }
    }
    
    // [SEQUENCE: MVP13-148] Check if tournament is complete
    bool IsComplete() const {
        // Check if final match is completed
        auto final_match = GetFinalMatch();
        if (final_match.has_value()) {
            return final_match->state == TournamentMatch::MatchState::COMPLETED;
        }
        return false;
    }
    
    // [SEQUENCE: MVP13-149] Get tournament standings
    std::vector<std::pair<uint64_t, uint32_t>> GetStandings() const {
        std::vector<std::pair<uint64_t, uint32_t>> standings;
        
        // Calculate standings based on format
        switch (format_) {
            case TournamentFormat::SINGLE_ELIMINATION:
            case TournamentFormat::DOUBLE_ELIMINATION:
                standings = GetEliminationStandings();
                break;
                
            case TournamentFormat::ROUND_ROBIN:
                standings = GetRoundRobinStandings();
                break;
                
            case TournamentFormat::SWISS:
                standings = GetSwissStandings();
                break;
        }
        
        return standings;
    }
    
private:
    TournamentFormat format_;
    uint32_t participant_count_;
    std::unordered_map<uint64_t, TournamentMatch> matches_;
    std::atomic<uint64_t> next_match_id_{1};
    
    // Bracket generation methods
    void GenerateSingleElimination() {
        uint32_t rounds = CalculateRounds(participant_count_);
        uint32_t current_round_matches = participant_count_ / 2;
        
        // Generate matches for each round
        for (uint32_t round = 1; round <= rounds; ++round) {
            for (uint32_t i = 0; i < current_round_matches; ++i) {
                TournamentMatch match;
                match.match_id = next_match_id_++;
                match.round_number = round;
                match.bracket_position = i;
                
                matches_[match.match_id] = match;
            }
            
            current_round_matches /= 2;
        }
    }
    
    void GenerateDoubleElimination() {
        // Winners bracket
        GenerateSingleElimination();
        
        // Losers bracket (더 복잡한 구조)
        // 실제 구현에서는 패자 브래킷 생성 로직 필요
    }
    
    void GenerateRoundRobin() {
        // 모든 참가자가 서로 한 번씩 경기
        for (uint32_t i = 0; i < participant_count_; ++i) {
            for (uint32_t j = i + 1; j < participant_count_; ++j) {
                TournamentMatch match;
                match.match_id = next_match_id_++;
                match.round_number = CalculateRoundRobinRound(i, j);
                
                matches_[match.match_id] = match;
            }
        }
    }
    
    void GenerateSwiss() {
        // 첫 라운드만 생성, 이후는 동적으로
        uint32_t first_round_matches = participant_count_ / 2;
        
        for (uint32_t i = 0; i < first_round_matches; ++i) {
            TournamentMatch match;
            match.match_id = next_match_id_++;
            match.round_number = 1;
            match.bracket_position = i;
            
            matches_[match.match_id] = match;
        }
    }
    
    // Helper methods
    uint32_t CalculateRounds(uint32_t participants) const {
        return static_cast<uint32_t>(std::ceil(std::log2(participants)));
    }
    
    void ProgressWinner(uint64_t match_id, uint64_t winner_id) {
        // Find next match in bracket
        // Implementation depends on bracket structure
    }
    
    void HandleLoserBracket(uint64_t match_id, uint64_t loser_id) {
        // Move loser to losers bracket
        // Implementation for double elimination
    }
    
    std::optional<TournamentMatch> GetFinalMatch() const {
        // Find the final match
        uint32_t max_round = 0;
        for (const auto& [id, match] : matches_) {
            max_round = std::max(max_round, match.round_number);
        }
        
        for (const auto& [id, match] : matches_) {
            if (match.round_number == max_round) {
                return match;
            }
        }
        
        return std::nullopt;
    }
    
    std::vector<std::pair<uint64_t, uint32_t>> GetEliminationStandings() const {
        // Calculate standings based on elimination progress
        std::vector<std::pair<uint64_t, uint32_t>> standings;
        // Implementation...
        return standings;
    }
    
    std::vector<std::pair<uint64_t, uint32_t>> GetRoundRobinStandings() const {
        // Calculate standings based on wins/losses
        std::vector<std::pair<uint64_t, uint32_t>> standings;
        // Implementation...
        return standings;
    }
    
    std::vector<std::pair<uint64_t, uint32_t>> GetSwissStandings() const {
        // Calculate standings based on Swiss system
        std::vector<std::pair<uint64_t, uint32_t>> standings;
        // Implementation...
        return standings;
    }
    
    uint32_t CalculateRoundRobinRound(uint32_t i, uint32_t j) const {
        // Calculate which round this match belongs to
        return 1; // Simplified
    }
};

// [SEQUENCE: MVP13-150] Tournament instance
class Tournament {
public:
    // [SEQUENCE: MVP13-151] Create tournament
    Tournament(uint64_t tournament_id, const TournamentConfig& config)
        : tournament_id_(tournament_id)
        , config_(config)
        , state_(TournamentState::REGISTRATION) {
        
        spdlog::info("Tournament {} created: {}", 
                    tournament_id_, config_.tournament_name);
    }
    
    // [SEQUENCE: MVP13-152] Tournament states
    enum class TournamentState {
        REGISTRATION,      // 참가 신청 중
        CHECK_IN,         // 체크인 기간
        BRACKET_GENERATION, // 대진표 생성 중
        IN_PROGRESS,      // 진행 중
        COMPLETED,        // 완료
        CANCELLED         // 취소됨
    };
    
    // [SEQUENCE: MVP13-153] Register participant
    bool RegisterParticipant(uint64_t participant_id, const std::string& name,
                           const std::vector<uint64_t>& team_members = {}) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (state_ != TournamentState::REGISTRATION) {
            return false;
        }
        
        if (participants_.size() >= config_.max_participants) {
            return false;
        }
        
        // Check requirements
        if (!CheckRequirements(participant_id)) {
            return false;
        }
        
        TournamentParticipant participant;
        participant.participant_id = participant_id;
        participant.name = name;
        participant.registration_time = std::chrono::system_clock::now();
        participant.team_members = team_members;
        
        if (!team_members.empty()) {
            participant.team_captain = participant_id;
        }
        
        participants_[participant_id] = participant;
        
        spdlog::info("Participant {} registered for tournament {}", 
                    name, tournament_id_);
        
        return true;
    }
    
    // [SEQUENCE: MVP13-154] Check in participant
    bool CheckInParticipant(uint64_t participant_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (state_ != TournamentState::CHECK_IN) {
            return false;
        }
        
        auto it = participants_.find(participant_id);
        if (it == participants_.end()) {
            return false;
        }
        
        it->second.is_checked_in = true;
        checked_in_count_++;
        
        return true;
    }
    
    // [SEQUENCE: MVP13-155] Start tournament
    void StartTournament() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (state_ != TournamentState::CHECK_IN) {
            return;
        }
        
        // Remove participants who didn't check in
        RemoveNoShows();
        
        // Check minimum participants
        if (participants_.size() < config_.min_participants) {
            state_ = TournamentState::CANCELLED;
            spdlog::warn("Tournament {} cancelled: insufficient participants", 
                        tournament_id_);
            return;
        }
        
        // Generate bracket
        state_ = TournamentState::BRACKET_GENERATION;
        bracket_ = std::make_unique<TournamentBracket>(
            config_.format, participants_.size());
        
        // Assign participants to bracket
        AssignBracketPositions();
        
        // Start first round
        state_ = TournamentState::IN_PROGRESS;
        current_round_ = 1;
        StartRound(current_round_);
        
        spdlog::info("Tournament {} started with {} participants", 
                    tournament_id_, participants_.size());
    }
    
    // [SEQUENCE: MVP13-156] Process match completion
    void ProcessMatchCompletion(uint64_t arena_match_id, 
                              uint64_t winner_id,
                              const ArenaMatch::MatchStatistics& stats) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Find tournament match
        uint64_t tournament_match_id = 0;
        for (const auto& [id, match] : current_round_matches_) {
            if (match.arena_match_id == arena_match_id) {
                tournament_match_id = id;
                break;
            }
        }
        
        if (tournament_match_id == 0) {
            return;
        }
        
        // Update bracket
        bracket_->UpdateMatchResult(tournament_match_id, winner_id);
        
        // Update participant stats
        UpdateParticipantStats(stats);
        
        // Check if round is complete
        if (IsRoundComplete()) {
            CompleteRound();
        }
    }
    
    // [SEQUENCE: MVP13-157] Get current standings
    std::vector<std::pair<TournamentParticipant, uint32_t>> GetStandings() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto bracket_standings = bracket_->GetStandings();
        std::vector<std::pair<TournamentParticipant, uint32_t>> standings;
        
        for (const auto& [participant_id, placement] : bracket_standings) {
            auto it = participants_.find(participant_id);
            if (it != participants_.end()) {
                standings.push_back({it->second, placement});
            }
        }
        
        return standings;
    }
    
    // Getters
    uint64_t GetId() const { return tournament_id_; }
    TournamentState GetState() const { return state_; }
    const TournamentConfig& GetConfig() const { return config_; }
    uint32_t GetParticipantCount() const { return participants_.size(); }
    uint32_t GetCurrentRound() const { return current_round_; }
    
private:
    uint64_t tournament_id_;
    TournamentConfig config_;
    TournamentState state_;
    
    // Participants
    std::unordered_map<uint64_t, TournamentParticipant> participants_;
    uint32_t checked_in_count_{0};
    
    // Bracket
    std::unique_ptr<TournamentBracket> bracket_;
    uint32_t current_round_{0};
    std::unordered_map<uint64_t, TournamentMatch> current_round_matches_;
    
    mutable std::mutex mutex_;
    
    // [SEQUENCE: MVP13-158] Check participant requirements
    bool CheckRequirements(uint64_t participant_id) {
        // In real implementation, check:
        // - Rating requirements
        // - Level requirements
        // - Match history
        // - Entry fee payment
        return true;
    }
    
    // [SEQUENCE: MVP13-159] Remove no-show participants
    void RemoveNoShows() {
        auto it = participants_.begin();
        while (it != participants_.end()) {
            if (!it->second.is_checked_in) {
                spdlog::info("Removing no-show participant: {}", it->second.name);
                it = participants_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    // [SEQUENCE: MVP13-160] Assign bracket positions
    void AssignBracketPositions() {
        // Seeding based on rating
        std::vector<std::pair<uint64_t, int32_t>> seeded_participants;
        
        for (const auto& [id, participant] : participants_) {
            // Get participant rating
            int32_t rating = GetParticipantRating(id);
            seeded_participants.push_back({id, rating});
        }
        
        // Sort by rating (highest first)
        std::sort(seeded_participants.begin(), seeded_participants.end(),
            [](const auto& a, const auto& b) {
                return a.second > b.second;
            });
        
        // Assign positions to avoid early matchups between top seeds
        AssignSeedingPositions(seeded_participants);
    }
    
    // [SEQUENCE: MVP13-161] Start tournament round
    void StartRound(uint32_t round) {
        auto round_matches = bracket_->GetRoundMatches(round);
        current_round_matches_.clear();
        
        for (auto& match : round_matches) {
            // Schedule match
            match.scheduled_time = std::chrono::system_clock::now() + 
                std::chrono::minutes(config_.round_duration_minutes * (round - 1));
            
            current_round_matches_[match.match_id] = match;
            
            // Create arena match when ready
            ScheduleArenaMatch(match);
        }
        
        spdlog::info("Tournament {} round {} started with {} matches", 
                    tournament_id_, round, round_matches.size());
    }
    
    // [SEQUENCE: MVP13-162] Complete tournament round
    void CompleteRound() {
        current_round_++;
        
        // Check if tournament is complete
        if (bracket_->IsComplete()) {
            CompleteTournament();
            return;
        }
        
        // Start next round after break
        std::this_thread::sleep_for(
            std::chrono::minutes(config_.break_between_rounds_minutes));
        
        StartRound(current_round_);
    }
    
    // [SEQUENCE: MVP13-163] Complete tournament
    void CompleteTournament() {
        state_ = TournamentState::COMPLETED;
        
        // Get final standings
        auto standings = GetStandings();
        
        // Distribute rewards
        DistributeRewards(standings);
        
        // Log results
        LogTournamentResults(standings);
        
        spdlog::info("Tournament {} completed. Winner: {}", 
                    tournament_id_, 
                    standings.empty() ? "Unknown" : standings[0].first.name);
    }
    
    // Helper methods
    void UpdateParticipantStats(const ArenaMatch::MatchStatistics& stats) {
        // Update tournament stats for all participants in the match
    }
    
    bool IsRoundComplete() const {
        for (const auto& [id, match] : current_round_matches_) {
            if (match.state != TournamentMatch::MatchState::COMPLETED &&
                match.state != TournamentMatch::MatchState::NO_SHOW) {
                return false;
            }
        }
        return true;
    }
    
    int32_t GetParticipantRating(uint64_t participant_id) const {
        // Get rating from ranking service
        return 1500; // Default
    }
    
    void AssignSeedingPositions(const std::vector<std::pair<uint64_t, int32_t>>& seeds) {
        // Standard tournament seeding algorithm
    }
    
    void ScheduleArenaMatch(const TournamentMatch& match) {
        // Create arena match through arena system
    }
    
    void DistributeRewards(const std::vector<std::pair<TournamentParticipant, uint32_t>>& standings) {
        // Distribute rewards based on placement
    }
    
    void LogTournamentResults(const std::vector<std::pair<TournamentParticipant, uint32_t>>& standings) {
        // Log detailed results for analytics
    }
};

// [SEQUENCE: MVP13-164] Tournament system manager
class TournamentSystem {
public:
    // [SEQUENCE: MVP13-165] Create tournament
    uint64_t CreateTournament(const TournamentConfig& config) {
        uint64_t tournament_id = next_tournament_id_++;
        
        auto tournament = std::make_shared<Tournament>(tournament_id, config);
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            tournaments_[tournament_id] = tournament;
            
            // Schedule state transitions
            ScheduleTournamentTransitions(tournament);
        }
        
        spdlog::info("Created tournament {}: {}", 
                    tournament_id, config.tournament_name);
        
        return tournament_id;
    }
    
    // [SEQUENCE: MVP13-166] Get active tournaments
    std::vector<std::shared_ptr<Tournament>> GetActiveTournaments() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<std::shared_ptr<Tournament>> active;
        
        for (const auto& [id, tournament] : tournaments_) {
            auto state = tournament->GetState();
            if (state != Tournament::TournamentState::COMPLETED &&
                state != Tournament::TournamentState::CANCELLED) {
                active.push_back(tournament);
            }
        }
        
        return active;
    }
    
    // [SEQUENCE: MVP13-167] Get tournament by ID
    std::shared_ptr<Tournament> GetTournament(uint64_t tournament_id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = tournaments_.find(tournament_id);
        if (it != tournaments_.end()) {
            return it->second;
        }
        
        return nullptr;
    }
    
    // [SEQUENCE: MVP13-168] Register for tournament
    bool RegisterForTournament(uint64_t tournament_id, 
                             uint64_t participant_id,
                             const std::string& name,
                             const std::vector<uint64_t>& team_members = {}) {
        auto tournament = GetTournament(tournament_id);
        if (!tournament) {
            return false;
        }
        
        return tournament->RegisterParticipant(participant_id, name, team_members);
    }
    
    // [SEQUENCE: MVP13-169] Get tournament schedule
    struct TournamentSchedule {
        struct ScheduledTournament {
            uint64_t tournament_id;
            std::string name;
            TournamentFormat format;
            ArenaType arena_type;
            std::chrono::system_clock::time_point start_time;
            uint32_t registered_count;
            uint32_t max_participants;
            TournamentRequirements requirements;
        };
        
        std::vector<ScheduledTournament> upcoming;
        std::vector<ScheduledTournament> in_progress;
        std::vector<ScheduledTournament> recent_completed;
    };
    
    TournamentSchedule GetTournamentSchedule() const {
        TournamentSchedule schedule;
        
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::system_clock::now();
        
        for (const auto& [id, tournament] : tournaments_) {
            ScheduledTournament sched_tournament;
            sched_tournament.tournament_id = id;
            sched_tournament.name = tournament->GetConfig().tournament_name;
            sched_tournament.format = tournament->GetConfig().format;
            sched_tournament.arena_type = tournament->GetConfig().arena_type;
            sched_tournament.start_time = tournament->GetConfig().tournament_start;
            sched_tournament.registered_count = tournament->GetParticipantCount();
            sched_tournament.max_participants = tournament->GetConfig().max_participants;
            sched_tournament.requirements = tournament->GetConfig().requirements;
            
            switch (tournament->GetState()) {
                case Tournament::TournamentState::REGISTRATION:
                case Tournament::TournamentState::CHECK_IN:
                    schedule.upcoming.push_back(sched_tournament);
                    break;
                    
                case Tournament::TournamentState::IN_PROGRESS:
                    schedule.in_progress.push_back(sched_tournament);
                    break;
                    
                case Tournament::TournamentState::COMPLETED:
                    if (now - sched_tournament.start_time < std::chrono::hours(24)) {
                        schedule.recent_completed.push_back(sched_tournament);
                    }
                    break;
            }
        }
        
        return schedule;
    }
    
    // [SEQUENCE: MVP13-170] Process arena match completion
    void ProcessArenaMatchCompletion(uint64_t arena_match_id,
                                   uint64_t winner_id,
                                   const ArenaMatch::MatchStatistics& stats) {
        // Find which tournament this match belongs to
        std::lock_guard<std::mutex> lock(mutex_);
        
        for (auto& [id, tournament] : tournaments_) {
            if (tournament->GetState() == Tournament::TournamentState::IN_PROGRESS) {
                tournament->ProcessMatchCompletion(arena_match_id, winner_id, stats);
            }
        }
    }
    
private:
    std::unordered_map<uint64_t, std::shared_ptr<Tournament>> tournaments_;
    std::atomic<uint64_t> next_tournament_id_{1};
    
    mutable std::mutex mutex_;
    
    // [SEQUENCE: MVP13-171] Schedule tournament state transitions
    void ScheduleTournamentTransitions(std::shared_ptr<Tournament> tournament) {
        const auto& config = tournament->GetConfig();
        
        // Schedule check-in period start
        auto check_in_time = config.registration_end;
        ScheduleTask(check_in_time, [tournament]() {
            if (tournament->GetState() == Tournament::TournamentState::REGISTRATION) {
                // Transition to check-in
                // tournament->StartCheckIn();
            }
        });
        
        // Schedule tournament start
        ScheduleTask(config.tournament_start, [tournament]() {
            tournament->StartTournament();
        });
    }
    
    void ScheduleTask(std::chrono::system_clock::time_point when,
                     std::function<void()> task) {
        // In real implementation, use a proper scheduler
        std::thread([when, task]() {
            std::this_thread::sleep_until(when);
            task();
        }).detach();
    }
};

} // namespace mmorpg::tournament