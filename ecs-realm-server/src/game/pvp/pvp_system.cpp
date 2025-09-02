#include "pvp_system.h"
#include "../combat/combat_system.h"
#include "../stats/character_stats.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <random>
#include <cmath>

namespace mmorpg::game::pvp {

// [SEQUENCE: MVP14-723] Random generator for matchmaking
static thread_local std::mt19937 pvp_rng(std::chrono::steady_clock::now().time_since_epoch().count());

// [SEQUENCE: MVP14-724] PvPController - Record kill
void PvPController::RecordKill(uint64_t victim_id) {
    stats_.total_kills++;
    stats_.current_kill_streak++;
    
    if (stats_.current_kill_streak > stats_.best_kill_streak) {
        stats_.best_kill_streak = stats_.current_kill_streak;
    }
    
    recent_kills_.push_back(victim_id);
    if (recent_kills_.size() > 10) {
        recent_kills_.erase(recent_kills_.begin());
    }
    
    last_pvp_action_ = std::chrono::steady_clock::now();
    
    spdlog::info("Player {} killed player {} (streak: {})", 
                 entity_id_, victim_id, stats_.current_kill_streak);
}

// [SEQUENCE: MVP14-725] PvPController - Record death
void PvPController::RecordDeath(uint64_t killer_id) {
    stats_.total_deaths++;
    stats_.current_kill_streak = 0;  // Reset kill streak
    
    recent_deaths_.push_back(killer_id);
    if (recent_deaths_.size() > 10) {
        recent_deaths_.erase(recent_deaths_.begin());
    }
    
    last_pvp_action_ = std::chrono::steady_clock::now();
    
    spdlog::info("Player {} was killed by player {}", entity_id_, killer_id);
}

// [SEQUENCE: MVP14-726] PvPController - Record assist
void PvPController::RecordAssist(uint64_t victim_id) {
    stats_.total_assists++;
    last_pvp_action_ = std::chrono::steady_clock::now();
    
    spdlog::debug("Player {} assisted in killing player {}", entity_id_, victim_id);
}

// [SEQUENCE: MVP14-727] MatchmakingQueue - Add player
void MatchmakingQueue::AddPlayer(uint64_t player_id, int32_t rating) {
    // Check if already queued
    auto it = std::find_if(queued_players_.begin(), queued_players_.end(),
        [player_id](const QueuedPlayer& p) { return p.player_id == player_id; });
    
    if (it != queued_players_.end()) {
        return;  // Already queued
    }
    
    QueuedPlayer player{player_id, rating, std::chrono::steady_clock::now()};
    queued_players_.push_back(player);
    
    // Sort by rating for better matching
    std::sort(queued_players_.begin(), queued_players_.end(),
        [](const QueuedPlayer& a, const QueuedPlayer& b) {
            return a.rating > b.rating;
        });
    
    spdlog::debug("Player {} (rating: {}) joined {} queue", 
                  player_id, rating, static_cast<int>(pvp_type_));
}

// [SEQUENCE: MVP14-728] MatchmakingQueue - Remove player
void MatchmakingQueue::RemovePlayer(uint64_t player_id) {
    queued_players_.erase(
        std::remove_if(queued_players_.begin(), queued_players_.end(),
            [player_id](const QueuedPlayer& p) { return p.player_id == player_id; }),
        queued_players_.end()
    );
}

// [SEQUENCE: MVP14-729] MatchmakingQueue - Is player queued
bool MatchmakingQueue::IsPlayerQueued(uint64_t player_id) const {
    return std::any_of(queued_players_.begin(), queued_players_.end(),
        [player_id](const QueuedPlayer& p) { return p.player_id == player_id; });
}

// [SEQUENCE: MVP14-730] MatchmakingQueue - Try create match
std::optional<PvPMatchInfo> MatchmakingQueue::TryCreateMatch() {
    // Determine team size based on PvP type
    size_t team_size = 1;
    switch (pvp_type_) {
        case PvPType::DUEL: team_size = 1; break;
        case PvPType::ARENA_2V2: team_size = 2; break;
        case PvPType::ARENA_3V3: team_size = 3; break;
        case PvPType::ARENA_5V5: team_size = 5; break;
        case PvPType::BATTLEGROUND_10V10: team_size = 10; break;
        case PvPType::BATTLEGROUND_20V20: team_size = 20; break;
        default: return std::nullopt;
    }
    
    // Need at least 2 teams worth of players
    if (queued_players_.size() < team_size * 2) {
        return std::nullopt;
    }
    
    PvPMatchInfo match;
    match.type = pvp_type_;
    match.state = PvPState::PREPARATION;
    
    // Simple matching: take players with similar ratings
    for (size_t i = 0; i < team_size * 2 && i < queued_players_.size(); ++i) {
        if (i < team_size) {
            match.team_a.push_back(queued_players_[i].player_id);
        } else {
            match.team_b.push_back(queued_players_[i].player_id);
        }
    }
    
    // Remove matched players from queue
    queued_players_.erase(queued_players_.begin(), 
                         queued_players_.begin() + team_size * 2);
    
    // Set match parameters
    match.start_time = std::chrono::steady_clock::now();
    
    switch (pvp_type_) {
        case PvPType::DUEL:
            match.duration_seconds = 300;  // 5 minutes
            match.kill_limit = 1;
            break;
        case PvPType::ARENA_2V2:
        case PvPType::ARENA_3V3:
        case PvPType::ARENA_5V5:
            match.duration_seconds = 600;  // 10 minutes
            break;
        case PvPType::BATTLEGROUND_10V10:
        case PvPType::BATTLEGROUND_20V20:
            match.duration_seconds = 1200; // 20 minutes
            match.score_limit = 1000;
            break;
    }
    
    spdlog::info("Created {} match with {} players per team", 
                 static_cast<int>(pvp_type_), team_size);
    
    return match;
}

// [SEQUENCE: MVP14-731] MatchmakingQueue - Get average wait time
float MatchmakingQueue::GetAverageWaitTime() const {
    if (queued_players_.empty()) {
        return 0.0f;
    }
    
    auto now = std::chrono::steady_clock::now();
    float total_wait = 0.0f;
    
    for (const auto& player : queued_players_) {
        auto wait_time = std::chrono::duration_cast<std::chrono::seconds>(
            now - player.queue_time).count();
        total_wait += wait_time;
    }
    
    return total_wait / queued_players_.size();
}

// [SEQUENCE: MVP14-732] MatchmakingQueue - Are players compatible
bool MatchmakingQueue::ArePlayersCompatible(const QueuedPlayer& p1, const QueuedPlayer& p2) const {
    int32_t rating_diff = std::abs(p1.rating - p2.rating);
    
    // Allow wider rating range based on queue time
    auto now = std::chrono::steady_clock::now();
    auto p1_wait = std::chrono::duration_cast<std::chrono::seconds>(now - p1.queue_time).count();
    auto p2_wait = std::chrono::duration_cast<std::chrono::seconds>(now - p2.queue_time).count();
    auto max_wait = std::max(p1_wait, p2_wait);
    
    // Base rating difference: 100, +10 per 30 seconds of wait
    int32_t allowed_diff = 100 + (max_wait / 30) * 10;
    
    return rating_diff <= allowed_diff;
}

// [SEQUENCE: MVP14-733] PvPManager - Create controller
std::shared_ptr<PvPController> PvPManager::CreateController(uint64_t entity_id) {
    auto controller = std::make_shared<PvPController>(entity_id);
    controllers_[entity_id] = controller;
    
    spdlog::debug("Created PvP controller for entity {}", entity_id);
    return controller;
}

// [SEQUENCE: MVP14-734] PvPManager - Get controller
std::shared_ptr<PvPController> PvPManager::GetController(uint64_t entity_id) const {
    auto it = controllers_.find(entity_id);
    return (it != controllers_.end()) ? it->second : nullptr;
}

// [SEQUENCE: MVP14-735] PvPManager - Remove controller
void PvPManager::RemoveController(uint64_t entity_id) {
    // Remove from any queues
    LeaveQueue(entity_id);
    
    controllers_.erase(entity_id);
    spdlog::debug("Removed PvP controller for entity {}", entity_id);
}

// [SEQUENCE: MVP14-736] PvPManager - Send duel request
bool PvPManager::SendDuelRequest(uint64_t challenger_id, uint64_t target_id) {
    // Validate both players exist
    auto challenger = GetController(challenger_id);
    auto target = GetController(target_id);
    
    if (!challenger || !target) {
        return false;
    }
    
    // Check if already in PvP
    if (challenger->GetState() != PvPState::NONE || 
        target->GetState() != PvPState::NONE) {
        return false;
    }
    
    // Check zone
    if (challenger->GetCurrentZone() == PvPZoneType::SAFE_ZONE ||
        target->GetCurrentZone() == PvPZoneType::SAFE_ZONE) {
        return false;
    }
    
    // Check for existing request
    auto existing = std::find_if(pending_duels_.begin(), pending_duels_.end(),
        [challenger_id, target_id](const DuelRequest& req) {
            return (req.challenger_id == challenger_id && req.target_id == target_id) ||
                   (req.challenger_id == target_id && req.target_id == challenger_id);
        });
    
    if (existing != pending_duels_.end()) {
        return false;
    }
    
    // Create request
    DuelRequest request{challenger_id, target_id, std::chrono::steady_clock::now()};
    pending_duels_.push_back(request);
    
    spdlog::info("Player {} challenged player {} to a duel", challenger_id, target_id);
    return true;
}

// [SEQUENCE: MVP14-737] PvPManager - Accept duel
bool PvPManager::AcceptDuel(uint64_t target_id, uint64_t challenger_id) {
    auto it = std::find_if(pending_duels_.begin(), pending_duels_.end(),
        [challenger_id, target_id](const DuelRequest& req) {
            return req.challenger_id == challenger_id && req.target_id == target_id;
        });
    
    if (it == pending_duels_.end()) {
        return false;
    }
    
    // Remove request
    pending_duels_.erase(it);
    
    // Start duel
    StartDuel(challenger_id, target_id);
    return true;
}

// [SEQUENCE: MVP14-738] PvPManager - Decline duel
bool PvPManager::DeclineDuel(uint64_t target_id, uint64_t challenger_id) {
    auto it = std::find_if(pending_duels_.begin(), pending_duels_.end(),
        [challenger_id, target_id](const DuelRequest& req) {
            return req.challenger_id == challenger_id && req.target_id == target_id;
        });
    
    if (it == pending_duels_.end()) {
        return false;
    }
    
    pending_duels_.erase(it);
    spdlog::info("Player {} declined duel from player {}", target_id, challenger_id);
    return true;
}

// [SEQUENCE: MVP14-739] PvPManager - Start duel
void PvPManager::StartDuel(uint64_t player1_id, uint64_t player2_id) {
    PvPMatchInfo match;
    match.match_id = CreateMatch(match);
    match.type = PvPType::DUEL;
    match.state = PvPState::IN_PROGRESS;
    match.team_a.push_back(player1_id);
    match.team_b.push_back(player2_id);
    match.start_time = std::chrono::steady_clock::now();
    match.duration_seconds = 300;  // 5 minutes
    match.kill_limit = 1;
    
    // Update controllers
    auto p1 = GetController(player1_id);
    auto p2 = GetController(player2_id);
    
    if (p1 && p2) {
        p1->SetState(PvPState::IN_PROGRESS);
        p1->SetCurrentMatch(match.match_id);
        
        p2->SetState(PvPState::IN_PROGRESS);
        p2->SetCurrentMatch(match.match_id);
        
        spdlog::info("Duel started between {} and {}", player1_id, player2_id);
    }
}

// [SEQUENCE: MVP14-740] PvPManager - End duel
void PvPManager::EndDuel(uint64_t winner_id, uint64_t loser_id) {
    // Find match
    uint64_t match_id = 0;
    auto winner_controller = GetController(winner_id);
    if (winner_controller) {
        match_id = winner_controller->GetCurrentMatch();
    }
    
    auto match = GetMatch(match_id);
    if (!match) {
        return;
    }
    
    // Update stats
    auto loser_controller = GetController(loser_id);
    if (winner_controller && loser_controller) {
        winner_controller->RecordKill(loser_id);
        winner_controller->GetStats().matches_won++;
        winner_controller->GetStats().current_win_streak++;
        
        loser_controller->RecordDeath(winner_id);
        loser_controller->GetStats().matches_lost++;
        loser_controller->GetStats().current_win_streak = 0;
        
        // Update ratings
        UpdateRatings(winner_id, loser_id);
        
        // Reset states
        winner_controller->SetState(PvPState::NONE);
        winner_controller->SetCurrentMatch(0);
        
        loser_controller->SetState(PvPState::NONE);
        loser_controller->SetCurrentMatch(0);
    }
    
    // End match
    EndMatch(match_id);
    
    spdlog::info("Duel ended: {} defeated {}", winner_id, loser_id);
}

// [SEQUENCE: MVP14-741] PvPManager - Queue for PvP
bool PvPManager::QueueForPvP(uint64_t player_id, PvPType type) {
    auto controller = GetController(player_id);
    if (!controller || controller->GetState() != PvPState::NONE) {
        return false;
    }
    
    // Get player rating
    int32_t rating = controller->GetStats().rating;
    
    // Add to appropriate queue
    auto queue_it = queues_.find(type);
    if (queue_it != queues_.end()) {
        queue_it->second->AddPlayer(player_id, rating);
        controller->SetState(PvPState::QUEUED);
        
        spdlog::info("Player {} queued for {} (rating: {})", 
                     player_id, static_cast<int>(type), rating);
        return true;
    }
    
    return false;
}

// [SEQUENCE: MVP14-742] PvPManager - Leave queue
bool PvPManager::LeaveQueue(uint64_t player_id) {
    auto controller = GetController(player_id);
    if (!controller || controller->GetState() != PvPState::QUEUED) {
        return false;
    }
    
    // Remove from all queues
    for (auto& [type, queue] : queues_) {
        queue->RemovePlayer(player_id);
    }
    
    controller->SetState(PvPState::NONE);
    spdlog::info("Player {} left PvP queue", player_id);
    return true;
}

// [SEQUENCE: MVP14-743] PvPManager - Update matchmaking
void PvPManager::UpdateMatchmaking() {
    for (auto& [type, queue] : queues_) {
        auto match_opt = queue->TryCreateMatch();
        if (match_opt.has_value()) {
            auto match = match_opt.value();
            match.match_id = CreateMatch(match);
            
            // Notify players
            for (uint64_t player_id : match.team_a) {
                auto controller = GetController(player_id);
                if (controller) {
                    controller->SetState(PvPState::PREPARATION);
                    controller->SetCurrentMatch(match.match_id);
                }
            }
            
            for (uint64_t player_id : match.team_b) {
                auto controller = GetController(player_id);
                if (controller) {
                    controller->SetState(PvPState::PREPARATION);
                    controller->SetCurrentMatch(match.match_id);
                }
            }
            
            spdlog::info("Created {} match {} with {} vs {} players", 
                        static_cast<int>(type), match.match_id, 
                        match.team_a.size(), match.team_b.size());
        }
    }
}

// [SEQUENCE: MVP14-744] PvPManager - Create match
uint64_t PvPManager::CreateMatch(const PvPMatchInfo& info) {
    uint64_t match_id = next_match_id_++;
    PvPMatchInfo match = info;
    match.match_id = match_id;
    active_matches_[match_id] = match;
    return match_id;
}

// [SEQUENCE: MVP14-745] PvPManager - Get match
PvPMatchInfo* PvPManager::GetMatch(uint64_t match_id) {
    auto it = active_matches_.find(match_id);
    return (it != active_matches_.end()) ? &it->second : nullptr;
}

// [SEQUENCE: MVP14-746] PvPManager - Can attack
bool PvPManager::CanAttack(uint64_t attacker_id, uint64_t target_id) const {
    auto attacker = GetController(attacker_id);
    auto target = GetController(target_id);
    
    if (!attacker || !target) {
        return false;
    }
    
    // Check PvP enabled
    if (!attacker->IsPvPEnabled() || !target->IsPvPEnabled()) {
        return false;
    }
    
    // Check zones
    PvPZoneType attacker_zone = attacker->GetCurrentZone();
    if (attacker_zone == PvPZoneType::SAFE_ZONE) {
        return false;
    }
    
    // Check if in same match
    if (attacker->GetCurrentMatch() != 0 && 
        attacker->GetCurrentMatch() == target->GetCurrentMatch()) {
        return !IsAlly(attacker_id, target_id);
    }
    
    // World PvP rules
    if (attacker_zone == PvPZoneType::CONTESTED || 
        attacker_zone == PvPZoneType::HOSTILE) {
        return true;  // TODO: Check factions
    }
    
    return false;
}

// [SEQUENCE: MVP14-747] PvPManager - Update ratings
void PvPManager::UpdateRatings(uint64_t winner_id, uint64_t loser_id) {
    auto winner = GetController(winner_id);
    auto loser = GetController(loser_id);
    
    if (!winner || !loser) {
        return;
    }
    
    int32_t winner_rating = winner->GetStats().rating;
    int32_t loser_rating = loser->GetStats().rating;
    
    int32_t rating_change = CalculateRatingChange(winner_rating, loser_rating);
    
    // Update ratings
    winner->GetStats().rating += rating_change;
    loser->GetStats().rating -= rating_change;
    
    // Update highest rating
    if (winner->GetStats().rating > winner->GetStats().highest_rating) {
        winner->GetStats().highest_rating = winner->GetStats().rating;
    }
    
    // Minimum rating
    if (loser->GetStats().rating < 0) {
        loser->GetStats().rating = 0;
    }
    
    spdlog::info("Rating update: {} (+{}) defeated {} (-{})", 
                 winner_id, rating_change, loser_id, rating_change);
}

// [SEQUENCE: MVP14-748] PvPManager - Calculate rating change
int32_t PvPManager::CalculateRatingChange(int32_t winner_rating, int32_t loser_rating) {
    // Simple ELO calculation
    const float K = 32.0f;  // K-factor
    
    float expected_winner = 1.0f / (1.0f + std::pow(10.0f, (loser_rating - winner_rating) / 400.0f));
    int32_t change = static_cast<int32_t>(K * (1.0f - expected_winner));
    
    // Minimum change of 1
    return std::max(1, change);
}

// [SEQUENCE: MVP14-749] PvPManager - Initialize queues
void PvPManager::InitializeQueues() {
    queues_[PvPType::ARENA_2V2] = std::make_unique<MatchmakingQueue>(PvPType::ARENA_2V2);
    queues_[PvPType::ARENA_3V3] = std::make_unique<MatchmakingQueue>(PvPType::ARENA_3V3);
    queues_[PvPType::ARENA_5V5] = std::make_unique<MatchmakingQueue>(PvPType::ARENA_5V5);
    queues_[PvPType::BATTLEGROUND_10V10] = std::make_unique<MatchmakingQueue>(PvPType::BATTLEGROUND_10V10);
    queues_[PvPType::BATTLEGROUND_20V20] = std::make_unique<MatchmakingQueue>(PvPType::BATTLEGROUND_20V20);
}

// [SEQUENCE: MVP14-750] PvPManager - Update
void PvPManager::Update(float delta_time) {
    ProcessExpiredDuels();
    UpdateMatchmaking();
    UpdateMatches(delta_time);
}

// [SEQUENCE: MVP14-751] PvPManager - Process expired duels
void PvPManager::ProcessExpiredDuels() {
    auto now = std::chrono::steady_clock::now();
    
    pending_duels_.erase(
        std::remove_if(pending_duels_.begin(), pending_duels_.end(),
            [now](const DuelRequest& req) {
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    now - req.request_time).count();
                return elapsed >= req.timeout_seconds;
            }),
        pending_duels_.end()
    );
}

// [SEQUENCE: MVP14-752] PvPManager - Update matches
void PvPManager::UpdateMatches(float delta_time) {
    auto now = std::chrono::steady_clock::now();
    std::vector<uint64_t> matches_to_end;
    
    for (auto& [match_id, match] : active_matches_) {
        if (match.state == PvPState::IN_PROGRESS) {
            // Check time limit
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - match.start_time).count();
            
            if (elapsed >= match.duration_seconds) {
                matches_to_end.push_back(match_id);
            }
        }
    }
    
    // End timed out matches
    for (uint64_t match_id : matches_to_end) {
        EndMatch(match_id);
    }
}

// [SEQUENCE: MVP14-753] PvPManager - Is ally
bool PvPManager::IsAlly(uint64_t player1_id, uint64_t player2_id) const {
    // Check if in same match
    auto p1 = GetController(player1_id);
    auto p2 = GetController(player2_id);
    
    if (!p1 || !p2) {
        return false;
    }
    
    uint64_t match_id = p1->GetCurrentMatch();
    if (match_id == 0 || match_id != p2->GetCurrentMatch()) {
        return false;
    }
    
    auto match = const_cast<PvPManager*>(this)->GetMatch(match_id);
    if (!match) {
        return false;
    }
    
    // Check if on same team
    bool p1_team_a = std::find(match->team_a.begin(), match->team_a.end(), player1_id) != match->team_a.end();
    bool p2_team_a = std::find(match->team_a.begin(), match->team_a.end(), player2_id) != match->team_a.end();
    
    return p1_team_a == p2_team_a;
}

// [SEQUENCE: MVP14-754] PvPManager - End match
void PvPManager::EndMatch(uint64_t match_id) {
    auto match = GetMatch(match_id);
    if (!match) {
        return;
    }
    
    match->state = PvPState::COMPLETED;
    match->end_time = std::chrono::steady_clock::now();
    
    // Determine winner
    bool team_a_won = match->team_a_score > match->team_b_score;
    
    // Update all players
    for (uint64_t player_id : match->team_a) {
        auto controller = GetController(player_id);
        if (controller) {
            controller->GetStats().matches_played++;
            if (team_a_won) {
                controller->GetStats().matches_won++;
            } else {
                controller->GetStats().matches_lost++;
            }
            controller->SetState(PvPState::NONE);
            controller->SetCurrentMatch(0);
        }
    }
    
    for (uint64_t player_id : match->team_b) {
        auto controller = GetController(player_id);
        if (controller) {
            controller->GetStats().matches_played++;
            if (!team_a_won) {
                controller->GetStats().matches_won++;
            } else {
                controller->GetStats().matches_lost++;
            }
            controller->SetState(PvPState::NONE);
            controller->SetCurrentMatch(0);
        }
    }
    
    // TODO: Grant rewards
    
    spdlog::info("Match {} ended. Team {} won ({} vs {})", 
                 match_id, team_a_won ? "A" : "B", 
                 match->team_a_score, match->team_b_score);
    
    // Remove match after a delay
    // In production, you'd archive it instead
}

} // namespace mmorpg::game::pvp
