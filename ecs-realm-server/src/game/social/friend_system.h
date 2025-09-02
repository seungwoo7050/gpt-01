#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <optional>
#include <functional>
#include <spdlog/spdlog.h>

namespace mmorpg::game::social {

// [SEQUENCE: MVP11-33] Friend system for social interactions
// 친구 시스템으로 플레이어 간 소셜 관계 관리

// [SEQUENCE: MVP11-34] Friend status
enum class FriendStatus {
    PENDING,        // Friend request sent, waiting for response
    ACCEPTED,       // Friends
    BLOCKED,        // Blocked user
    DECLINED        // Request declined
};

// [SEQUENCE: MVP11-35] Online status
enum class OnlineStatus {
    OFFLINE,
    ONLINE,
    AWAY,           // AFK
    BUSY,           // Do not disturb
    INVISIBLE       // Appear offline
};

// [SEQUENCE: MVP11-36] Friend info
struct FriendInfo {
    uint64_t friend_id;
    std::string character_name;
    std::string note;  // Personal note about friend
    
    FriendStatus status = FriendStatus::PENDING;
    OnlineStatus online_status = OnlineStatus::OFFLINE;
    
    // Timestamps
    std::chrono::system_clock::time_point added_time;
    std::chrono::system_clock::time_point last_seen;
    
    // Current location (if online and not private)
    std::string current_zone;
    uint32_t level = 1;
    uint32_t class_id = 0;
    
    // Interaction stats
    uint32_t messages_sent = 0;
    uint32_t messages_received = 0;
    uint32_t trades_completed = 0;
    uint32_t dungeons_together = 0;
};

// [SEQUENCE: MVP11-37] Friend request
struct FriendRequest {
    uint64_t requester_id;
    uint64_t target_id;
    std::string message;
    std::chrono::system_clock::time_point request_time;
    
    bool IsExpired() const {
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::hours>(
            now - request_time
        ).count();
        return elapsed > 72;  // 3 days expiry
    }
};

// [SEQUENCE: MVP11-38] Friend list configuration
struct FriendListConfig {
    uint32_t max_friends = 100;
    uint32_t max_blocked = 50;
    uint32_t max_pending_requests = 20;
    bool allow_offline_requests = true;
    bool show_location_to_friends = true;
    bool receive_online_notifications = true;
};

// [SEQUENCE: MVP11-39] Friend list for a player
class FriendList {
public:
    FriendList(uint64_t owner_id, const FriendListConfig& config = {})
        : owner_id_(owner_id), config_(config) {}
    
    // [SEQUENCE: MVP11-40] Send friend request
    bool SendFriendRequest(uint64_t target_id, const std::string& message = "") {
        // Check if already friends
        if (IsFriend(target_id)) {
            spdlog::warn("Player {} already friends with {}", owner_id_, target_id);
            return false;
        }
        
        // Check if blocked
        if (IsBlocked(target_id)) {
            spdlog::warn("Player {} has blocked {}", owner_id_, target_id);
            return false;
        }
        
        // Check pending limit
        if (GetPendingRequestCount() >= config_.max_pending_requests) {
            spdlog::warn("Player {} has too many pending requests", owner_id_);
            return false;
        }
        
        // Create request
        FriendRequest request;
        request.requester_id = owner_id_;
        request.target_id = target_id;
        request.message = message;
        request.request_time = std::chrono::system_clock::now();
        
        outgoing_requests_[target_id] = request;
        
        spdlog::info("Player {} sent friend request to {}", owner_id_, target_id);
        return true;
    }
    
    // [SEQUENCE: MVP11-41] Receive friend request
    void ReceiveFriendRequest(const FriendRequest& request) {
        incoming_requests_[request.requester_id] = request;
        
        spdlog::info("Player {} received friend request from {}", 
                    owner_id_, request.requester_id);
    }
    
    // [SEQUENCE: MVP11-42] Accept friend request
    bool AcceptFriendRequest(uint64_t requester_id) {
        auto it = incoming_requests_.find(requester_id);
        if (it == incoming_requests_.end()) {
            return false;
        }
        
        // Check friend limit
        if (GetFriendCount() >= config_.max_friends) {
            spdlog::warn("Player {} friend list is full", owner_id_);
            return false;
        }
        
        // Add as friend
        FriendInfo friend_info;
        friend_info.friend_id = requester_id;
        friend_info.status = FriendStatus::ACCEPTED;
        friend_info.added_time = std::chrono::system_clock::now();
        friend_info.last_seen = friend_info.added_time;
        
        friends_[requester_id] = friend_info;
        incoming_requests_.erase(it);
        
        spdlog::info("Player {} accepted friend request from {}", 
                    owner_id_, requester_id);
        return true;
    }
    
    // [SEQUENCE: MVP11-43] Decline friend request
    bool DeclineFriendRequest(uint64_t requester_id) {
        auto it = incoming_requests_.find(requester_id);
        if (it == incoming_requests_.end()) {
            return false;
        }
        
        incoming_requests_.erase(it);
        declined_ids_.insert(requester_id);
        
        spdlog::info("Player {} declined friend request from {}", 
                    owner_id_, requester_id);
        return true;
    }
    
    // [SEQUENCE: MVP11-44] Remove friend
    bool RemoveFriend(uint64_t friend_id) {
        auto it = friends_.find(friend_id);
        if (it == friends_.end()) {
            return false;
        }
        
        friends_.erase(it);
        
        spdlog::info("Player {} removed friend {}", owner_id_, friend_id);
        return true;
    }
    
    // [SEQUENCE: MVP11-45] Block user
    bool BlockUser(uint64_t user_id) {
        // Check blocked limit
        if (blocked_users_.size() >= config_.max_blocked) {
            spdlog::warn("Player {} blocked list is full", owner_id_);
            return false;
        }
        
        // Remove from friends if exists
        RemoveFriend(user_id);
        
        // Add to blocked
        blocked_users_.insert(user_id);
        
        spdlog::info("Player {} blocked user {}", owner_id_, user_id);
        return true;
    }
    
    // [SEQUENCE: MVP11-46] Unblock user
    bool UnblockUser(uint64_t user_id) {
        return blocked_users_.erase(user_id) > 0;
    }
    
    // [SEQUENCE: MVP11-47] Update friend info
    void UpdateFriendInfo(uint64_t friend_id, 
                         const std::function<void(FriendInfo&)>& updater) {
        auto it = friends_.find(friend_id);
        if (it != friends_.end()) {
            updater(it->second);
        }
    }
    
    // [SEQUENCE: MVP11-48] Update friend online status
    void UpdateFriendOnlineStatus(uint64_t friend_id, OnlineStatus status) {
        UpdateFriendInfo(friend_id, [status](FriendInfo& info) {
            info.online_status = status;
            if (status != OnlineStatus::OFFLINE) {
                info.last_seen = std::chrono::system_clock::now();
            }
        });
    }
    
    // [SEQUENCE: MVP11-49] Set personal note
    bool SetFriendNote(uint64_t friend_id, const std::string& note) {
        auto it = friends_.find(friend_id);
        if (it == friends_.end()) {
            return false;
        }
        
        it->second.note = note;
        return true;
    }
    
    // Query methods
    bool IsFriend(uint64_t user_id) const {
        auto it = friends_.find(user_id);
        return it != friends_.end() && it->second.status == FriendStatus::ACCEPTED;
    }
    
    bool IsBlocked(uint64_t user_id) const {
        return blocked_users_.find(user_id) != blocked_users_.end();
    }
    
    bool HasIncomingRequest(uint64_t from_id) const {
        return incoming_requests_.find(from_id) != incoming_requests_.end();
    }
    
    bool HasOutgoingRequest(uint64_t to_id) const {
        return outgoing_requests_.find(to_id) != outgoing_requests_.end();
    }
    
    // [SEQUENCE: MVP11-50] Get friend info
    const FriendInfo* GetFriendInfo(uint64_t friend_id) const {
        auto it = friends_.find(friend_id);
        return it != friends_.end() ? &it->second : nullptr;
    }
    
    // [SEQUENCE: MVP11-51] Get all friends
    std::vector<FriendInfo> GetAllFriends() const {
        std::vector<FriendInfo> result;
        for (const auto& [id, info] : friends_) {
            if (info.status == FriendStatus::ACCEPTED) {
                result.push_back(info);
            }
        }
        return result;
    }
    
    // [SEQUENCE: MVP11-52] Get online friends
    std::vector<FriendInfo> GetOnlineFriends() const {
        std::vector<FriendInfo> result;
        for (const auto& [id, info] : friends_) {
            if (info.status == FriendStatus::ACCEPTED && 
                info.online_status != OnlineStatus::OFFLINE) {
                result.push_back(info);
            }
        }
        return result;
    }
    
    // Statistics
    size_t GetFriendCount() const {
        return std::count_if(friends_.begin(), friends_.end(),
            [](const auto& pair) { 
                return pair.second.status == FriendStatus::ACCEPTED; 
            });
    }
    
    size_t GetPendingRequestCount() const {
        return incoming_requests_.size();
    }
    
    size_t GetBlockedCount() const {
        return blocked_users_.size();
    }
    
    // [SEQUENCE: MVP11-53] Clean expired requests
    void CleanExpiredRequests() {
        // Clean incoming
        std::erase_if(incoming_requests_, [](const auto& pair) {
            return pair.second.IsExpired();
        });
        
        // Clean outgoing
        std::erase_if(outgoing_requests_, [](const auto& pair) {
            return pair.second.IsExpired();
        });
    }
    
private:
    uint64_t owner_id_;
    FriendListConfig config_;
    
    // Friends map
    std::unordered_map<uint64_t, FriendInfo> friends_;
    
    // Friend requests
    std::unordered_map<uint64_t, FriendRequest> incoming_requests_;
    std::unordered_map<uint64_t, FriendRequest> outgoing_requests_;
    
    // Blocked users
    std::unordered_set<uint64_t> blocked_users_;
    
    // Declined requests (to prevent spam)
    std::unordered_set<uint64_t> declined_ids_;
};

// [SEQUENCE: MVP11-54] Friend system manager
class FriendSystemManager {
public:
    static FriendSystemManager& Instance() {
        static FriendSystemManager instance;
        return instance;
    }
    
    // [SEQUENCE: MVP11-55] Initialize system
    void Initialize(const FriendListConfig& default_config = {}) {
        default_config_ = default_config;
        spdlog::info("Friend system initialized");
    }
    
    // [SEQUENCE: MVP11-56] Get or create friend list
    std::shared_ptr<FriendList> GetFriendList(uint64_t player_id) {
        auto it = friend_lists_.find(player_id);
        if (it == friend_lists_.end()) {
            auto list = std::make_shared<FriendList>(player_id, default_config_);
            friend_lists_[player_id] = list;
            return list;
        }
        return it->second;
    }
    
    // [SEQUENCE: MVP11-57] Process friend request
    bool ProcessFriendRequest(uint64_t from_id, uint64_t to_id, 
                            const std::string& message = "") {
        auto sender_list = GetFriendList(from_id);
        auto receiver_list = GetFriendList(to_id);
        
        // Check if receiver blocked sender
        if (receiver_list->IsBlocked(from_id)) {
            return false;
        }
        
        // Send request
        if (!sender_list->SendFriendRequest(to_id, message)) {
            return false;
        }
        
        // Create request object
        FriendRequest request;
        request.requester_id = from_id;
        request.target_id = to_id;
        request.message = message;
        request.request_time = std::chrono::system_clock::now();
        
        // Receive request
        receiver_list->ReceiveFriendRequest(request);
        
        // TODO: Send notification to receiver if online
        
        return true;
    }
    
    // [SEQUENCE: MVP11-58] Process friend acceptance
    bool ProcessFriendAcceptance(uint64_t accepter_id, uint64_t requester_id) {
        auto accepter_list = GetFriendList(accepter_id);
        auto requester_list = GetFriendList(requester_id);
        
        // Accept on accepter side
        if (!accepter_list->AcceptFriendRequest(requester_id)) {
            return false;
        }
        
        // Add on requester side
        FriendInfo friend_info;
        friend_info.friend_id = accepter_id;
        friend_info.status = FriendStatus::ACCEPTED;
        friend_info.added_time = std::chrono::system_clock::now();
        
        requester_list->UpdateFriendInfo(accepter_id, 
            [&friend_info](FriendInfo& info) {
                info = friend_info;
            });
        
        // Clear outgoing request
        requester_list->outgoing_requests_.erase(accepter_id);
        
        spdlog::info("Friend relationship established: {} <-> {}", 
                    requester_id, accepter_id);
        
        // TODO: Send notifications to both players
        
        return true;
    }
    
    // [SEQUENCE: MVP11-59] Update player online status
    void UpdatePlayerOnlineStatus(uint64_t player_id, OnlineStatus status) {
        // Update status for all friends
        for (auto& [friend_id, friend_list] : friend_lists_) {
            if (friend_list->IsFriend(player_id)) {
                friend_list->UpdateFriendOnlineStatus(player_id, status);
                
                // TODO: Send notification if configured
            }
        }
        
        // Store player's current status
        player_online_status_[player_id] = status;
    }
    
    // [SEQUENCE: MVP11-60] Update player location
    void UpdatePlayerLocation(uint64_t player_id, const std::string& zone,
                            uint32_t level, uint32_t class_id) {
        // Update for all friends
        for (auto& [friend_id, friend_list] : friend_lists_) {
            if (friend_list->IsFriend(player_id)) {
                friend_list->UpdateFriendInfo(player_id, 
                    [&](FriendInfo& info) {
                        info.current_zone = zone;
                        info.level = level;
                        info.class_id = class_id;
                    });
            }
        }
    }
    
    // [SEQUENCE: MVP11-61] Remove friendship (both sides)
    void RemoveFriendship(uint64_t player1_id, uint64_t player2_id) {
        auto list1 = GetFriendList(player1_id);
        auto list2 = GetFriendList(player2_id);
        
        list1->RemoveFriend(player2_id);
        list2->RemoveFriend(player1_id);
        
        spdlog::info("Friendship removed: {} <-> {}", player1_id, player2_id);
    }
    
    // [SEQUENCE: MVP11-62] Clean all expired requests
    void CleanupExpiredRequests() {
        for (auto& [player_id, friend_list] : friend_lists_) {
            friend_list->CleanExpiredRequests();
        }
    }
    
    // [SEQUENCE: MVP11-63] Get mutual friends
    std::vector<uint64_t> GetMutualFriends(uint64_t player1_id, uint64_t player2_id) {
        auto list1 = GetFriendList(player1_id);
        auto list2 = GetFriendList(player2_id);
        
        auto friends1 = list1->GetAllFriends();
        auto friends2 = list2->GetAllFriends();
        
        std::unordered_set<uint64_t> set1;
        for (const auto& f : friends1) {
            set1.insert(f.friend_id);
        }
        
        std::vector<uint64_t> mutual;
        for (const auto& f : friends2) {
            if (set1.find(f.friend_id) != set1.end()) {
                mutual.push_back(f.friend_id);
            }
        }
        
        return mutual;
    }
    
private:
    FriendSystemManager() = default;
    
    FriendListConfig default_config_;
    std::unordered_map<uint64_t, std::shared_ptr<FriendList>> friend_lists_;
    std::unordered_map<uint64_t, OnlineStatus> player_online_status_;
};

// [SEQUENCE: MVP11-64] Friend activity tracker
class FriendActivityTracker {
public:
    // Track message between friends
    static void TrackMessage(uint64_t from_id, uint64_t to_id) {
        auto from_list = FriendSystemManager::Instance().GetFriendList(from_id);
        from_list->UpdateFriendInfo(to_id, [](FriendInfo& info) {
            info.messages_sent++;
        });
        
        auto to_list = FriendSystemManager::Instance().GetFriendList(to_id);
        to_list->UpdateFriendInfo(from_id, [](FriendInfo& info) {
            info.messages_received++;
        });
    }
    
    // Track trade between friends
    static void TrackTrade(uint64_t player1_id, uint64_t player2_id) {
        auto list1 = FriendSystemManager::Instance().GetFriendList(player1_id);
        list1->UpdateFriendInfo(player2_id, [](FriendInfo& info) {
            info.trades_completed++;
        });
        
        auto list2 = FriendSystemManager::Instance().GetFriendList(player2_id);
        list2->UpdateFriendInfo(player1_id, [](FriendInfo& info) {
            info.trades_completed++;
        });
    }
    
    // Track dungeon completion
    static void TrackDungeonTogether(const std::vector<uint64_t>& party_members) {
        for (size_t i = 0; i < party_members.size(); ++i) {
            for (size_t j = i + 1; j < party_members.size(); ++j) {
                auto list_i = FriendSystemManager::Instance().GetFriendList(party_members[i]);
                auto list_j = FriendSystemManager::Instance().GetFriendList(party_members[j]);
                
                if (list_i->IsFriend(party_members[j])) {
                    list_i->UpdateFriendInfo(party_members[j], [](FriendInfo& info) {
                        info.dungeons_together++;
                    });
                    
                    list_j->UpdateFriendInfo(party_members[i], [](FriendInfo& info) {
                        info.dungeons_together++;
                    });
                }
            }
        }
    }
};