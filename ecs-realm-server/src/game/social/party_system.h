#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <optional>
#include <algorithm>
#include <spdlog/spdlog.h>

namespace mmorpg::game::social {

// [SEQUENCE: MVP11-208] Party system for group gameplay
// 파티 시스템으로 그룹 플레이 관리

// [SEQUENCE: MVP11-209] Party roles
enum class PartyRole {
    LEADER,         // Can invite, kick, change settings
    ASSISTANT,      // Can invite
    MEMBER          // Regular member
};

// [SEQUENCE: MVP11-210] Loot distribution method
enum class LootMethod {
    FREE_FOR_ALL,   // Anyone can loot
    ROUND_ROBIN,    // Take turns
    MASTER_LOOTER,  // Leader distributes
    GROUP_LOOT,     // Roll for items
    NEED_BEFORE_GREED  // Need/Greed system
};

// [SEQUENCE: MVP11-211] Party member info
struct PartyMember {
    uint64_t player_id;
    std::string character_name;
    PartyRole role = PartyRole::MEMBER;
    
    // Character info
    uint32_t level = 1;
    uint32_t class_id = 0;
    uint32_t current_hp = 100;
    uint32_t max_hp = 100;
    uint32_t current_mp = 100;
    uint32_t max_mp = 100;
    
    // Location
    uint32_t zone_id = 0;
    float x = 0, y = 0, z = 0;
    
    // Status
    bool is_online = true;
    bool is_ready = false;  // For ready checks
    bool is_in_combat = false;
    bool is_dead = false;
    
    // Join time
    std::chrono::system_clock::time_point join_time;
    
    // Loot tracking
    uint32_t items_looted = 0;
    uint64_t gold_looted = 0;
};

// [SEQUENCE: MVP11-212] Party settings
struct PartySettings {
    LootMethod loot_method = LootMethod::GROUP_LOOT;
    uint32_t loot_threshold = 2;  // Item quality threshold for group loot
    uint64_t master_looter_id = 0;
    
    bool experience_sharing = true;
    float experience_range = 100.0f;  // Range for XP sharing
    
    bool allow_invites = true;
    uint32_t min_level = 0;
    uint32_t max_level = 0;  // 0 = no limit
};

// [SEQUENCE: MVP11-213] Party class
class Party {
public:
    Party(uint32_t party_id, uint64_t leader_id, const std::string& leader_name)
        : party_id_(party_id) {
        
        // Add leader
        PartyMember leader;
        leader.player_id = leader_id;
        leader.character_name = leader_name;
        leader.role = PartyRole::LEADER;
        leader.join_time = std::chrono::system_clock::now();
        
        members_.push_back(leader);
        creation_time_ = std::chrono::system_clock::now();
    }
    
    // [SEQUENCE: MVP11-214] Add member
    bool AddMember(uint64_t player_id, const std::string& character_name) {
        if (IsFull()) {
            spdlog::warn("Party {} is full", party_id_);
            return false;
        }
        
        if (GetMember(player_id)) {
            spdlog::warn("Player {} already in party", player_id);
            return false;
        }
        
        PartyMember member;
        member.player_id = player_id;
        member.character_name = character_name;
        member.role = PartyRole::MEMBER;
        member.join_time = std::chrono::system_clock::now();
        
        members_.push_back(member);
        
        spdlog::info("Player {} joined party {}", player_id, party_id_);
        return true;
    }
    
    // [SEQUENCE: MVP11-215] Remove member
    bool RemoveMember(uint64_t player_id) {
        auto it = std::find_if(members_.begin(), members_.end(),
            [player_id](const PartyMember& m) { return m.player_id == player_id; });
        
        if (it == members_.end()) {
            return false;
        }
        
        bool was_leader = (it->role == PartyRole::LEADER);
        members_.erase(it);
        
        // Handle leader leaving
        if (was_leader && !members_.empty()) {
            PromoteNewLeader();
        }
        
        spdlog::info("Player {} left party {}", player_id, party_id_);
        return true;
    }
    
    // [SEQUENCE: MVP11-216] Change member role
    bool ChangeMemberRole(uint64_t player_id, PartyRole new_role) {
        auto* member = GetMember(player_id);
        if (!member) {
            return false;
        }
        
        // Can't have multiple leaders
        if (new_role == PartyRole::LEADER) {
            // Demote current leader
            for (auto& m : members_) {
                if (m.role == PartyRole::LEADER) {
                    m.role = PartyRole::MEMBER;
                    break;
                }
            }
        }
        
        member->role = new_role;
        return true;
    }
    
    // [SEQUENCE: MVP11-217] Update member info
    void UpdateMemberInfo(uint64_t player_id, 
                         const std::function<void(PartyMember&)>& updater) {
        auto* member = GetMember(player_id);
        if (member) {
            updater(*member);
            last_update_time_ = std::chrono::system_clock::now();
        }
    }
    
    // [SEQUENCE: MVP11-218] Update member stats
    void UpdateMemberStats(uint64_t player_id, uint32_t hp, uint32_t max_hp,
                          uint32_t mp, uint32_t max_mp) {
        UpdateMemberInfo(player_id, [=](PartyMember& m) {
            m.current_hp = hp;
            m.max_hp = max_hp;
            m.current_mp = mp;
            m.max_mp = max_mp;
        });
    }
    
    // [SEQUENCE: MVP11-219] Update member location
    void UpdateMemberLocation(uint64_t player_id, uint32_t zone_id,
                             float x, float y, float z) {
        UpdateMemberInfo(player_id, [=](PartyMember& m) {
            m.zone_id = zone_id;
            m.x = x;
            m.y = y;
            m.z = z;
        });
    }
    
    // [SEQUENCE: MVP11-220] Set ready check
    void StartReadyCheck(uint64_t initiator_id) {
        if (!IsLeaderOrAssistant(initiator_id)) {
            return;
        }
        
        ready_check_active_ = true;
        ready_check_time_ = std::chrono::system_clock::now();
        
        // Reset all ready states
        for (auto& member : members_) {
            member.is_ready = false;
        }
        
        // Initiator is automatically ready
        auto* initiator = GetMember(initiator_id);
        if (initiator) {
            initiator->is_ready = true;
        }
    }
    
    // [SEQUENCE: MVP11-221] Set member ready
    void SetMemberReady(uint64_t player_id, bool ready) {
        if (!ready_check_active_) {
            return;
        }
        
        auto* member = GetMember(player_id);
        if (member) {
            member->is_ready = ready;
        }
        
        // Check if all ready
        bool all_ready = true;
        for (const auto& m : members_) {
            if (m.is_online && !m.is_ready) {
                all_ready = false;
                break;
            }
        }
        
        if (all_ready) {
            ready_check_active_ = false;
            spdlog::info("Party {} ready check passed", party_id_);
        }
    }
    
    // [SEQUENCE: MVP11-222] Calculate experience share
    std::unordered_map<uint64_t, uint64_t> CalculateExperienceShare(
        uint64_t base_experience, uint64_t killer_id) {
        
        std::unordered_map<uint64_t, uint64_t> shares;
        
        if (!settings_.experience_sharing) {
            shares[killer_id] = base_experience;
            return shares;
        }
        
        // Get eligible members (alive, in range, online)
        std::vector<const PartyMember*> eligible;
        const PartyMember* killer = GetMember(killer_id);
        
        if (!killer) {
            return shares;
        }
        
        for (const auto& member : members_) {
            if (!member.is_online || member.is_dead) {
                continue;
            }
            
            // Check range
            if (member.player_id != killer_id) {
                float dx = member.x - killer->x;
                float dy = member.y - killer->y;
                float dz = member.z - killer->z;
                float distance = std::sqrt(dx*dx + dy*dy + dz*dz);
                
                if (distance > settings_.experience_range) {
                    continue;
                }
            }
            
            eligible.push_back(&member);
        }
        
        if (eligible.empty()) {
            return shares;
        }
        
        // Calculate shares with level-based weighting
        uint32_t total_level = 0;
        for (const auto* member : eligible) {
            total_level += member->level;
        }
        
        for (const auto* member : eligible) {
            uint64_t share = (base_experience * member->level) / total_level;
            
            // Bonus for being in a party
            share = static_cast<uint64_t>(share * 1.1f);
            
            shares[member->player_id] = share;
        }
        
        return shares;
    }
    
    // [SEQUENCE: MVP11-223] Distribute loot
    uint64_t DetermineLooter(uint32_t item_quality) {
        switch (settings_.loot_method) {
            case LootMethod::FREE_FOR_ALL:
                return 0;  // Anyone can loot
                
            case LootMethod::MASTER_LOOTER:
                return settings_.master_looter_id;
                
            case LootMethod::ROUND_ROBIN:
                // Cycle through members
                if (!members_.empty()) {
                    last_looter_index_ = (last_looter_index_ + 1) % members_.size();
                    return members_[last_looter_index_].player_id;
                }
                break;
                
            case LootMethod::GROUP_LOOT:
            case LootMethod::NEED_BEFORE_GREED:
                if (item_quality >= settings_.loot_threshold) {
                    return 0;  // Trigger roll system
                }
                break;
        }
        
        return 0;
    }
    
    // Query methods
    PartyMember* GetMember(uint64_t player_id) {
        auto it = std::find_if(members_.begin(), members_.end(),
            [player_id](const PartyMember& m) { return m.player_id == player_id; });
        
        return (it != members_.end()) ? &(*it) : nullptr;
    }
    
    const PartyMember* GetLeader() const {
        auto it = std::find_if(members_.begin(), members_.end(),
            [](const PartyMember& m) { return m.role == PartyRole::LEADER; });
        
        return (it != members_.end()) ? &(*it) : nullptr;
    }
    
    bool IsLeader(uint64_t player_id) const {
        const auto* member = const_cast<Party*>(this)->GetMember(player_id);
        return member && member->role == PartyRole::LEADER;
    }
    
    bool IsLeaderOrAssistant(uint64_t player_id) const {
        const auto* member = const_cast<Party*>(this)->GetMember(player_id);
        return member && (member->role == PartyRole::LEADER || 
                         member->role == PartyRole::ASSISTANT);
    }
    
    bool IsFull() const { return members_.size() >= max_size_; }
    bool IsEmpty() const { return members_.empty(); }
    size_t GetSize() const { return members_.size(); }
    
    // Getters
    uint32_t GetId() const { return party_id_; }
    const std::vector<PartyMember>& GetMembers() const { return members_; }
    PartySettings& GetSettings() { return settings_; }
    const PartySettings& GetSettings() const { return settings_; }
    
private:
    uint32_t party_id_;
    std::vector<PartyMember> members_;
    PartySettings settings_;
    
    static constexpr size_t max_size_ = 5;  // Standard party size
    
    // Ready check
    bool ready_check_active_ = false;
    std::chrono::system_clock::time_point ready_check_time_;
    
    // Loot tracking
    size_t last_looter_index_ = 0;
    
    // Timestamps
    std::chrono::system_clock::time_point creation_time_;
    std::chrono::system_clock::time_point last_update_time_;
    
    // [SEQUENCE: MVP11-224] Promote new leader
    void PromoteNewLeader() {
        // Prefer assistants
        auto it = std::find_if(members_.begin(), members_.end(),
            [](const PartyMember& m) { return m.role == PartyRole::ASSISTANT; });
        
        if (it == members_.end()) {
            // No assistant, pick first member
            it = members_.begin();
        }
        
        if (it != members_.end()) {
            it->role = PartyRole::LEADER;
            spdlog::info("Player {} promoted to party leader", it->player_id);
        }
    }
};

// [SEQUENCE: MVP11-225] Party invite
struct PartyInvite {
    uint64_t inviter_id;
    uint64_t target_id;
    uint32_t party_id;
    std::chrono::system_clock::time_point invite_time;
    
    bool IsExpired() const {
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - invite_time
        ).count();
        return elapsed > 60;  // 1 minute timeout
    }
};

// [SEQUENCE: MVP11-226] Raid group (multiple parties)
class RaidGroup {
public:
    RaidGroup(uint32_t raid_id) : raid_id_(raid_id) {}
    
    // [SEQUENCE: MVP11-227] Add party to raid
    bool AddParty(std::shared_ptr<Party> party) {
        if (parties_.size() >= max_parties_) {
            return false;
        }
        
        parties_.push_back(party);
        return true;
    }
    
    // [SEQUENCE: MVP11-228] Remove party from raid
    bool RemoveParty(uint32_t party_id) {
        auto it = std::find_if(parties_.begin(), parties_.end(),
            [party_id](const auto& p) { return p->GetId() == party_id; });
        
        if (it != parties_.end()) {
            parties_.erase(it);
            return true;
        }
        
        return false;
    }
    
    // [SEQUENCE: MVP11-229] Get all members
    std::vector<PartyMember> GetAllMembers() const {
        std::vector<PartyMember> all_members;
        
        for (const auto& party : parties_) {
            const auto& members = party->GetMembers();
            all_members.insert(all_members.end(), members.begin(), members.end());
        }
        
        return all_members;
    }
    
    size_t GetTotalMembers() const {
        size_t total = 0;
        for (const auto& party : parties_) {
            total += party->GetSize();
        }
        return total;
    }
    
private:
    uint32_t raid_id_;
    std::vector<std::shared_ptr<Party>> parties_;
    static constexpr size_t max_parties_ = 8;  // 40-man raid max
};

// [SEQUENCE: MVP11-230] Party manager
class PartyManager {
public:
    static PartyManager& Instance() {
        static PartyManager instance;
        return instance;
    }
    
    // [SEQUENCE: MVP11-231] Create party
    std::shared_ptr<Party> CreateParty(uint64_t leader_id, 
                                      const std::string& leader_name) {
        // Check if player already in party
        if (GetPlayerParty(leader_id)) {
            spdlog::warn("Player {} already in a party", leader_id);
            return nullptr;
        }
        
        uint32_t party_id = next_party_id_++;
        auto party = std::make_shared<Party>(party_id, leader_id, leader_name);
        
        parties_[party_id] = party;
        player_parties_[leader_id] = party_id;
        
        spdlog::info("Party {} created with leader {}", party_id, leader_id);
        return party;
    }
    
    // [SEQUENCE: MVP11-232] Invite to party
    bool InviteToParty(uint64_t inviter_id, uint64_t target_id) {
        auto party = GetPlayerParty(inviter_id);
        if (!party) {
            return false;
        }
        
        // Check permissions
        if (!party->IsLeaderOrAssistant(inviter_id)) {
            return false;
        }
        
        // Check if target already in party
        if (GetPlayerParty(target_id)) {
            return false;
        }
        
        // Create invite
        PartyInvite invite;
        invite.inviter_id = inviter_id;
        invite.target_id = target_id;
        invite.party_id = party->GetId();
        invite.invite_time = std::chrono::system_clock::now();
        
        pending_invites_[target_id] = invite;
        
        // TODO: Notify target player
        
        return true;
    }
    
    // [SEQUENCE: MVP11-233] Accept party invite
    bool AcceptPartyInvite(uint64_t player_id, const std::string& player_name) {
        auto invite_it = pending_invites_.find(player_id);
        if (invite_it == pending_invites_.end()) {
            return false;
        }
        
        if (invite_it->second.IsExpired()) {
            pending_invites_.erase(invite_it);
            return false;
        }
        
        auto party = GetParty(invite_it->second.party_id);
        if (!party || party->IsFull()) {
            return false;
        }
        
        if (party->AddMember(player_id, player_name)) {
            player_parties_[player_id] = party->GetId();
            pending_invites_.erase(invite_it);
            return true;
        }
        
        return false;
    }
    
    // [SEQUENCE: MVP11-234] Leave party
    bool LeaveParty(uint64_t player_id) {
        auto party = GetPlayerParty(player_id);
        if (!party) {
            return false;
        }
        
        party->RemoveMember(player_id);
        player_parties_.erase(player_id);
        
        // Disband if empty
        if (party->IsEmpty()) {
            parties_.erase(party->GetId());
            spdlog::info("Party {} disbanded (empty)", party->GetId());
        }
        
        return true;
    }
    
    // [SEQUENCE: MVP11-235] Get player's party
    std::shared_ptr<Party> GetPlayerParty(uint64_t player_id) {
        auto it = player_parties_.find(player_id);
        if (it == player_parties_.end()) {
            return nullptr;
        }
        
        return GetParty(it->second);
    }
    
    // [SEQUENCE: MVP11-236] Get party by ID
    std::shared_ptr<Party> GetParty(uint32_t party_id) {
        auto it = parties_.find(party_id);
        return (it != parties_.end()) ? it->second : nullptr;
    }
    
    // [SEQUENCE: MVP11-237] Update all parties
    void UpdateParties() {
        // Clean expired invites
        std::erase_if(pending_invites_, [](const auto& pair) {
            return pair.second.IsExpired();
        });
        
        // Update party states
        for (auto& [id, party] : parties_) {
            // TODO: Update member online status
            // TODO: Check for disconnected members
        }
    }
    
private:
    PartyManager() = default;
    
    uint32_t next_party_id_ = 1;
    uint32_t next_raid_id_ = 1;
    
    std::unordered_map<uint32_t, std::shared_ptr<Party>> parties_;
    std::unordered_map<uint64_t, uint32_t> player_parties_;  // player_id -> party_id
    std::unordered_map<uint64_t, PartyInvite> pending_invites_;  // target_id -> invite
    
    std::unordered_map<uint32_t, std::shared_ptr<RaidGroup>> raid_groups_;
};

} // namespace mmorpg::game::social
