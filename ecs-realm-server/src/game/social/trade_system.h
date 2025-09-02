#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <optional>
#include <mutex>
#include <spdlog/spdlog.h>

namespace mmorpg::game::social {

// [SEQUENCE: MVP11-101] Trade system for secure item/gold exchange
// 거래 시스템으로 안전한 아이템/골드 교환 관리

// [SEQUENCE: MVP11-102] Trade state
enum class TradeState {
    NONE,           // No trade
    INITIATED,      // Trade request sent
    NEGOTIATING,    // Both parties accepted, can modify
    LOCKED,         // One party locked their offer
    BOTH_LOCKED,    // Both locked, waiting confirmation
    CONFIRMED,      // Ready to complete
    COMPLETED,      // Trade successful
    CANCELLED       // Trade cancelled
};

// [SEQUENCE: MVP11-103] Trade slot
struct TradeSlot {
    uint64_t item_instance_id = 0;  // Unique item instance
    uint32_t item_id = 0;           // Item template ID
    uint32_t quantity = 1;
    
    bool IsEmpty() const { return item_instance_id == 0; }
    
    void Clear() {
        item_instance_id = 0;
        item_id = 0;
        quantity = 1;
    }
};

// [SEQUENCE: MVP11-104] Trade offer
struct TradeOffer {
    uint64_t player_id;
    uint64_t gold_amount = 0;
    std::vector<TradeSlot> item_slots;
    bool is_locked = false;
    bool is_confirmed = false;
    std::chrono::system_clock::time_point last_modified;
    
    void Reset() {
        gold_amount = 0;
        for (auto& slot : item_slots) {
            slot.Clear();
        }
        is_locked = false;
        is_confirmed = false;
        last_modified = std::chrono::system_clock::now();
    }
};

// [SEQUENCE: MVP11-105] Trade session
class TradeSession {
public:
    TradeSession(uint64_t initiator_id, uint64_t target_id, uint32_t session_id)
        : session_id_(session_id), state_(TradeState::INITIATED) {
        
        participants_[0].player_id = initiator_id;
        participants_[1].player_id = target_id;
        
        // Initialize slots (6 slots per player)
        for (int i = 0; i < 2; ++i) {
            participants_[i].item_slots.resize(6);
        }
        
        creation_time_ = std::chrono::system_clock::now();
    }
    
    // [SEQUENCE: MVP11-106] Accept trade request
    bool AcceptTradeRequest() {
        if (state_ != TradeState::INITIATED) {
            return false;
        }
        
        state_ = TradeState::NEGOTIATING;
        spdlog::info("Trade {} accepted, now negotiating", session_id_);
        return true;
    }
    
    // [SEQUENCE: MVP11-107] Add item to trade
    bool AddItem(uint64_t player_id, uint32_t slot_index, 
                uint64_t item_instance_id, uint32_t item_id, uint32_t quantity) {
        if (state_ != TradeState::NEGOTIATING) {
            spdlog::warn("Cannot add items in state {}", static_cast<int>(state_));
            return false;
        }
        
        auto* offer = GetPlayerOffer(player_id);
        if (!offer || offer->is_locked) {
            return false;
        }
        
        if (slot_index >= offer->item_slots.size()) {
            return false;
        }
        
        // Add item to slot
        offer->item_slots[slot_index].item_instance_id = item_instance_id;
        offer->item_slots[slot_index].item_id = item_id;
        offer->item_slots[slot_index].quantity = quantity;
        offer->last_modified = std::chrono::system_clock::now();
        
        // Reset other player's lock if they had locked
        auto* other_offer = GetOtherPlayerOffer(player_id);
        if (other_offer && other_offer->is_locked) {
            other_offer->is_locked = false;
            state_ = TradeState::NEGOTIATING;
        }
        
        return true;
    }
    
    // [SEQUENCE: MVP11-108] Remove item from trade
    bool RemoveItem(uint64_t player_id, uint32_t slot_index) {
        if (state_ != TradeState::NEGOTIATING) {
            return false;
        }
        
        auto* offer = GetPlayerOffer(player_id);
        if (!offer || offer->is_locked) {
            return false;
        }
        
        if (slot_index >= offer->item_slots.size()) {
            return false;
        }
        
        offer->item_slots[slot_index].Clear();
        offer->last_modified = std::chrono::system_clock::now();
        
        // Reset other player's lock
        auto* other_offer = GetOtherPlayerOffer(player_id);
        if (other_offer && other_offer->is_locked) {
            other_offer->is_locked = false;
            state_ = TradeState::NEGOTIATING;
        }
        
        return true;
    }
    
    // [SEQUENCE: MVP11-109] Set gold amount
    bool SetGoldAmount(uint64_t player_id, uint64_t amount) {
        if (state_ != TradeState::NEGOTIATING) {
            return false;
        }
        
        auto* offer = GetPlayerOffer(player_id);
        if (!offer || offer->is_locked) {
            return false;
        }
        
        offer->gold_amount = amount;
        offer->last_modified = std::chrono::system_clock::now();
        
        // Reset other player's lock
        auto* other_offer = GetOtherPlayerOffer(player_id);
        if (other_offer && other_offer->is_locked) {
            other_offer->is_locked = false;
            state_ = TradeState::NEGOTIATING;
        }
        
        return true;
    }
    
    // [SEQUENCE: MVP11-110] Lock trade offer
    bool LockOffer(uint64_t player_id) {
        if (state_ != TradeState::NEGOTIATING && state_ != TradeState::LOCKED) {
            return false;
        }
        
        auto* offer = GetPlayerOffer(player_id);
        if (!offer) {
            return false;
        }
        
        offer->is_locked = true;
        
        // Check if both locked
        if (participants_[0].is_locked && participants_[1].is_locked) {
            state_ = TradeState::BOTH_LOCKED;
        } else {
            state_ = TradeState::LOCKED;
        }
        
        return true;
    }
    
    // [SEQUENCE: MVP11-111] Unlock trade offer
    bool UnlockOffer(uint64_t player_id) {
        auto* offer = GetPlayerOffer(player_id);
        if (!offer) {
            return false;
        }
        
        offer->is_locked = false;
        offer->is_confirmed = false;
        state_ = TradeState::NEGOTIATING;
        
        return true;
    }
    
    // [SEQUENCE: MVP11-112] Confirm trade
    bool ConfirmTrade(uint64_t player_id) {
        if (state_ != TradeState::BOTH_LOCKED) {
            return false;
        }
        
        auto* offer = GetPlayerOffer(player_id);
        if (!offer || !offer->is_locked) {
            return false;
        }
        
        offer->is_confirmed = true;
        
        // Check if both confirmed
        if (participants_[0].is_confirmed && participants_[1].is_confirmed) {
            state_ = TradeState::CONFIRMED;
            return true;
        }
        
        return true;
    }
    
    // [SEQUENCE: MVP11-113] Cancel trade
    void Cancel() {
        state_ = TradeState::CANCELLED;
        cancellation_time_ = std::chrono::system_clock::now();
    }
    
    // [SEQUENCE: MVP11-114] Validate trade
    bool ValidateTrade() const {
        if (state_ != TradeState::CONFIRMED) {
            return false;
        }
        
        // Both must be locked and confirmed
        if (!participants_[0].is_locked || !participants_[0].is_confirmed ||
            !participants_[1].is_locked || !participants_[1].is_confirmed) {
            return false;
        }
        
        // TODO: Validate inventory space
        // TODO: Validate gold amounts
        // TODO: Validate item ownership
        
        return true;
    }
    
    // [SEQUENCE: MVP11-115] Complete trade
    bool CompleteTrade() {
        if (!ValidateTrade()) {
            return false;
        }
        
        state_ = TradeState::COMPLETED;
        completion_time_ = std::chrono::system_clock::now();
        
        return true;
    }
    
    // Getters
    uint32_t GetSessionId() const { return session_id_; }
    TradeState GetState() const { return state_; }
    const TradeOffer& GetParticipant(int index) const { return participants_[index]; }
    
    uint64_t GetInitiatorId() const { return participants_[0].player_id; }
    uint64_t GetTargetId() const { return participants_[1].player_id; }
    
    bool IsParticipant(uint64_t player_id) const {
        return participants_[0].player_id == player_id || 
               participants_[1].player_id == player_id;
    }
    
    // [SEQUENCE: MVP11-116] Check if trade expired
    bool IsExpired() const {
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(
            now - creation_time_
        ).count();
        
        return elapsed > 5;  // 5 minute timeout
    }
    
private:
    uint32_t session_id_;
    TradeState state_;
    TradeOffer participants_[2];
    
    std::chrono::system_clock::time_point creation_time_;
    std::chrono::system_clock::time_point completion_time_;
    std::chrono::system_clock::time_point cancellation_time_;
    
    // [SEQUENCE: MVP11-117] Get player's offer
    TradeOffer* GetPlayerOffer(uint64_t player_id) {
        if (participants_[0].player_id == player_id) {
            return &participants_[0];
        } else if (participants_[1].player_id == player_id) {
            return &participants_[1];
        }
        return nullptr;
    }
    
    // [SEQUENCE: MVP11-118] Get other player's offer
    TradeOffer* GetOtherPlayerOffer(uint64_t player_id) {
        if (participants_[0].player_id == player_id) {
            return &participants_[1];
        } else if (participants_[1].player_id == player_id) {
            return &participants_[0];
        }
        return nullptr;
    }
};

// [SEQUENCE: MVP11-119] Trade request
struct TradeRequest {
    uint64_t initiator_id;
    uint64_t target_id;
    std::chrono::system_clock::time_point request_time;
    
    bool IsExpired() const {
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - request_time
        ).count();
        return elapsed > 30;  // 30 second timeout
    }
};

// [SEQUENCE: MVP11-120] Trade manager
class TradeManager {
public:
    static TradeManager& Instance() {
        static TradeManager instance;
        return instance;
    }
    
    // [SEQUENCE: MVP11-121] Request trade
    bool RequestTrade(uint64_t initiator_id, uint64_t target_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Check if players are already trading
        if (GetActiveTradeSession(initiator_id) || GetActiveTradeSession(target_id)) {
            spdlog::warn("Player already in trade");
            return false;
        }
        
        // Check for existing request
        auto key = MakeRequestKey(initiator_id, target_id);
        if (pending_requests_.find(key) != pending_requests_.end()) {
            spdlog::warn("Trade request already pending");
            return false;
        }
        
        // Create request
        TradeRequest request;
        request.initiator_id = initiator_id;
        request.target_id = target_id;
        request.request_time = std::chrono::system_clock::now();
        
        pending_requests_[key] = request;
        
        spdlog::info("Trade requested: {} -> {}", initiator_id, target_id);
        
        // TODO: Notify target player
        
        return true;
    }
    
    // [SEQUENCE: MVP11-122] Accept trade request
    std::shared_ptr<TradeSession> AcceptTradeRequest(uint64_t target_id, 
                                                     uint64_t initiator_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto key = MakeRequestKey(initiator_id, target_id);
        auto request_it = pending_requests_.find(key);
        
        if (request_it == pending_requests_.end()) {
            spdlog::warn("No pending trade request found");
            return nullptr;
        }
        
        if (request_it->second.IsExpired()) {
            pending_requests_.erase(request_it);
            spdlog::warn("Trade request expired");
            return nullptr;
        }
        
        // Create trade session
        uint32_t session_id = next_session_id_++;
        auto session = std::make_shared<TradeSession>(initiator_id, target_id, session_id);
        session->AcceptTradeRequest();
        
        active_sessions_[session_id] = session;
        player_sessions_[initiator_id] = session_id;
        player_sessions_[target_id] = session_id;
        
        // Remove request
        pending_requests_.erase(request_it);
        
        spdlog::info("Trade session {} created between {} and {}", 
                    session_id, initiator_id, target_id);
        
        return session;
    }
    
    // [SEQUENCE: MVP11-123] Decline trade request
    bool DeclineTradeRequest(uint64_t target_id, uint64_t initiator_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto key = MakeRequestKey(initiator_id, target_id);
        return pending_requests_.erase(key) > 0;
    }
    
    // [SEQUENCE: MVP11-124] Get active trade session
    std::shared_ptr<TradeSession> GetActiveTradeSession(uint64_t player_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = player_sessions_.find(player_id);
        if (it == player_sessions_.end()) {
            return nullptr;
        }
        
        auto session_it = active_sessions_.find(it->second);
        if (session_it == active_sessions_.end()) {
            return nullptr;
        }
        
        return session_it->second;
    }
    
    // [SEQUENCE: MVP11-125] Cancel trade
    bool CancelTrade(uint64_t player_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto session = GetActiveTradeSession(player_id);
        if (!session) {
            return false;
        }
        
        session->Cancel();
        
        // Remove from active sessions
        RemoveSession(session->GetSessionId());
        
        spdlog::info("Trade cancelled by player {}", player_id);
        
        // TODO: Notify both players
        
        return true;
    }
    
    // [SEQUENCE: MVP11-126] Complete trade
    bool CompleteTrade(std::shared_ptr<TradeSession> session) {
        if (!session->CompleteTrade()) {
            return false;
        }
        
        // Execute trade
        bool success = ExecuteTrade(session);
        
        if (success) {
            // Log trade
            LogTrade(session);
            
            // Remove session
            RemoveSession(session->GetSessionId());
        }
        
        return success;
    }
    
    // [SEQUENCE: MVP11-127] Clean expired
    void CleanupExpired() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Clean expired requests
        std::erase_if(pending_requests_, [](const auto& pair) {
            return pair.second.IsExpired();
        });
        
        // Clean expired sessions
        std::vector<uint32_t> expired_sessions;
        for (const auto& [id, session] : active_sessions_) {
            if (session->IsExpired()) {
                expired_sessions.push_back(id);
            }
        }
        
        for (uint32_t id : expired_sessions) {
            RemoveSession(id);
        }
    }
    
private:
    TradeManager() = default;
    
    std::mutex mutex_;
    uint32_t next_session_id_ = 1;
    
    // Active trade sessions
    std::unordered_map<uint32_t, std::shared_ptr<TradeSession>> active_sessions_;
    std::unordered_map<uint64_t, uint32_t> player_sessions_;  // player_id -> session_id
    
    // Pending trade requests
    std::unordered_map<std::string, TradeRequest> pending_requests_;
    
    // Trade history (for logging)
    struct TradeLog {
        uint32_t session_id;
        uint64_t player1_id;
        uint64_t player2_id;
        TradeOffer offer1;
        TradeOffer offer2;
        std::chrono::system_clock::time_point timestamp;
    };
    std::vector<TradeLog> trade_history_;
    
    // [SEQUENCE: MVP11-128] Make request key
    std::string MakeRequestKey(uint64_t initiator, uint64_t target) {
        return std::to_string(initiator) + "_" + std::to_string(target);
    }
    
    // [SEQUENCE: MVP11-129] Remove session
    void RemoveSession(uint32_t session_id) {
        auto session_it = active_sessions_.find(session_id);
        if (session_it == active_sessions_.end()) {
            return;
        }
        
        auto session = session_it->second;
        
        // Remove player mappings
        player_sessions_.erase(session->GetInitiatorId());
        player_sessions_.erase(session->GetTargetId());
        
        // Remove session
        active_sessions_.erase(session_it);
    }
    
    // [SEQUENCE: MVP11-130] Execute trade
    bool ExecuteTrade(std::shared_ptr<TradeSession> session) {
        // TODO: Begin transaction
        
        const auto& offer1 = session->GetParticipant(0);
        const auto& offer2 = session->GetParticipant(1);
        
        // TODO: Transfer items from player1 to player2
        // TODO: Transfer items from player2 to player1
        // TODO: Transfer gold
        
        // TODO: Commit transaction
        
        spdlog::info("Trade {} completed successfully", session->GetSessionId());
        
        return true;
    }
    
    // [SEQUENCE: MVP11-131] Log trade
    void LogTrade(std::shared_ptr<TradeSession> session) {
        TradeLog log;
        log.session_id = session->GetSessionId();
        log.player1_id = session->GetParticipant(0).player_id;
        log.player2_id = session->GetParticipant(1).player_id;
        log.offer1 = session->GetParticipant(0);
        log.offer2 = session->GetParticipant(1);
        log.timestamp = std::chrono::system_clock::now();
        
        trade_history_.push_back(log);
        
        // Keep only recent history
        if (trade_history_.size() > 10000) {
            trade_history_.erase(trade_history_.begin());
        }
    }
};

// [SEQUENCE: MVP11-132] Trade validator
class TradeValidator {
public:
    // Validate player can trade
    static bool CanPlayerTrade(uint64_t player_id) {
        // TODO: Check player level
        // TODO: Check if player is in combat
        // TODO: Check if player is alive
        // TODO: Check trade restrictions
        return true;
    }
    
    // Validate item can be traded
    static bool CanItemBeTraded(uint64_t item_instance_id) {
        // TODO: Check if item is bound
        // TODO: Check if item is quest item
        // TODO: Check if item is temporary
        return true;
    }
    
    // Validate gold amount
    static bool ValidateGoldAmount(uint64_t player_id, uint64_t amount) {
        // TODO: Check player has enough gold
        // TODO: Check for overflow
        return true;
    }
    
    // Validate inventory space
    static bool ValidateInventorySpace(uint64_t player_id, 
                                     const std::vector<TradeSlot>& incoming_items) {
        // TODO: Check player has enough inventory space
        return true;
    }
};