#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <optional>
#include <algorithm>
#include <spdlog/spdlog.h>

namespace mmorpg::game::social {

// [SEQUENCE: MVP11-65] Guild system for player organizations
// 길드 시스템으로 플레이어 조직 관리

// [SEQUENCE: MVP11-66] Guild rank permissions
enum class GuildPermission {
    INVITE_MEMBER      = 1 << 0,
    KICK_MEMBER        = 1 << 1,
    PROMOTE_MEMBER     = 1 << 2,
    DEMOTE_MEMBER      = 1 << 3,
    EDIT_MOTD          = 1 << 4,  // Message of the Day
    EDIT_RANKS         = 1 << 5,
    USE_GUILD_BANK     = 1 << 6,
    WITHDRAW_GOLD      = 1 << 7,
    DEPOSIT_ITEMS      = 1 << 8,
    WITHDRAW_ITEMS     = 1 << 9,
    EDIT_GUILD_INFO    = 1 << 10,
    DISBAND_GUILD      = 1 << 11,
    ALL_PERMISSIONS    = 0xFFFF
};

// [SEQUENCE: MVP11-67] Guild rank
struct GuildRank {
    uint32_t rank_id;
    std::string rank_name;
    uint32_t permissions = 0;
    uint32_t daily_gold_withdrawal_limit = 0;
    uint32_t daily_item_withdrawal_limit = 0;
    
    bool HasPermission(GuildPermission perm) const {
        return (permissions & static_cast<uint32_t>(perm)) != 0;
    }
    
    void GrantPermission(GuildPermission perm) {
        permissions |= static_cast<uint32_t>(perm);
    }
    
    void RevokePermission(GuildPermission perm) {
        permissions &= ~static_cast<uint32_t>(perm);
    }
};

// [SEQUENCE: MVP11-68] Guild member
struct GuildMember {
    uint64_t player_id;
    std::string character_name;
    uint32_t rank_id = 0;
    std::chrono::system_clock::time_point join_date;
    std::chrono::system_clock::time_point last_online;
    
    // Contribution stats
    uint64_t contribution_points = 0;
    uint64_t gold_deposited = 0;
    uint32_t items_deposited = 0;
    uint32_t quests_completed = 0;
    
    // Daily limits tracking
    uint64_t gold_withdrawn_today = 0;
    uint32_t items_withdrawn_today = 0;
    std::chrono::system_clock::time_point last_withdrawal_reset;
    
    // Member note
    std::string public_note;
    std::string officer_note;
};

// [SEQUENCE: MVP11-69] Guild bank tab
struct GuildBankTab {
    uint32_t tab_id;
    std::string tab_name;
    std::string tab_icon;
    uint32_t required_rank = 0;  // Minimum rank to access
    
    // Items stored (item_id -> count)
    std::unordered_map<uint32_t, uint32_t> items;
    
    // Access log
    struct AccessLog {
        uint64_t player_id;
        std::string action;  // "deposit", "withdraw"
        uint32_t item_id;
        uint32_t count;
        std::chrono::system_clock::time_point timestamp;
    };
    std::vector<AccessLog> access_logs;
};

// [SEQUENCE: MVP11-70] Guild configuration
struct GuildConfig {
    uint32_t max_members = 100;
    uint32_t max_ranks = 10;
    uint32_t max_bank_tabs = 6;
    uint32_t min_members_to_create = 5;
    uint64_t creation_cost = 10000;  // Gold
    uint32_t inactive_kick_days = 30;
    bool allow_multiple_guilds = false;
};

// [SEQUENCE: MVP11-71] Guild data
class Guild {
public:
    Guild(uint32_t guild_id, const std::string& name, uint64_t founder_id)
        : guild_id_(guild_id), guild_name_(name), founder_id_(founder_id) {
        creation_date_ = std::chrono::system_clock::now();
        InitializeDefaultRanks();
    }
    
    // [SEQUENCE: MVP11-72] Add member
    bool AddMember(uint64_t player_id, const std::string& character_name) {
        if (members_.size() >= config_.max_members) {
            spdlog::warn("Guild {} is full", guild_id_);
            return false;
        }
        
        if (members_.find(player_id) != members_.end()) {
            spdlog::warn("Player {} already in guild {}", player_id, guild_id_);
            return false;
        }
        
        GuildMember member;
        member.player_id = player_id;
        member.character_name = character_name;
        member.rank_id = GetLowestRankId();
        member.join_date = std::chrono::system_clock::now();
        member.last_online = member.join_date;
        
        members_[player_id] = member;
        
        spdlog::info("Player {} joined guild {}", player_id, guild_id_);
        return true;
    }
    
    // [SEQUENCE: MVP11-73] Remove member
    bool RemoveMember(uint64_t player_id) {
        auto it = members_.find(player_id);
        if (it == members_.end()) {
            return false;
        }
        
        members_.erase(it);
        
        // Check if guild should disband
        if (members_.empty()) {
            spdlog::info("Guild {} has no members, marking for disbanding", guild_id_);
            is_disbanded_ = true;
        }
        
        return true;
    }
    
    // [SEQUENCE: MVP11-74] Promote/Demote member
    bool ChangeMemberRank(uint64_t player_id, uint32_t new_rank_id) {
        auto member_it = members_.find(player_id);
        if (member_it == members_.end()) {
            return false;
        }
        
        auto rank_it = ranks_.find(new_rank_id);
        if (rank_it == ranks_.end()) {
            return false;
        }
        
        member_it->second.rank_id = new_rank_id;
        
        spdlog::info("Player {} rank changed to {} in guild {}", 
                    player_id, new_rank_id, guild_id_);
        return true;
    }
    
    // [SEQUENCE: MVP11-75] Check member permission
    bool HasPermission(uint64_t player_id, GuildPermission perm) const {
        auto member_it = members_.find(player_id);
        if (member_it == members_.end()) {
            return false;
        }
        
        auto rank_it = ranks_.find(member_it->second.rank_id);
        if (rank_it == ranks_.end()) {
            return false;
        }
        
        return rank_it->second.HasPermission(perm);
    }
    
    // [SEQUENCE: MVP11-76] Update MOTD
    bool SetMotd(const std::string& motd, uint64_t setter_id) {
        if (!HasPermission(setter_id, GuildPermission::EDIT_MOTD)) {
            return false;
        }
        
        motd_ = motd;
        motd_setter_id_ = setter_id;
        motd_timestamp_ = std::chrono::system_clock::now();
        
        return true;
    }
    
    // [SEQUENCE: MVP11-77] Deposit gold
    bool DepositGold(uint64_t player_id, uint64_t amount) {
        auto member_it = members_.find(player_id);
        if (member_it == members_.end()) {
            return false;
        }
        
        guild_bank_gold_ += amount;
        member_it->second.gold_deposited += amount;
        member_it->second.contribution_points += amount / 100;  // 1 point per 100 gold
        
        // Log transaction
        BankTransaction transaction;
        transaction.player_id = player_id;
        transaction.type = TransactionType::GOLD_DEPOSIT;
        transaction.amount = amount;
        transaction.timestamp = std::chrono::system_clock::now();
        
        bank_transactions_.push_back(transaction);
        
        return true;
    }
    
    // [SEQUENCE: MVP11-78] Withdraw gold
    bool WithdrawGold(uint64_t player_id, uint64_t amount) {
        if (!HasPermission(player_id, GuildPermission::WITHDRAW_GOLD)) {
            return false;
        }
        
        if (guild_bank_gold_ < amount) {
            return false;
        }
        
        auto member_it = members_.find(player_id);
        if (member_it == members_.end()) {
            return false;
        }
        
        // Check daily limit
        auto& member = member_it->second;
        ResetDailyLimits(member);
        
        auto rank_it = ranks_.find(member.rank_id);
        if (rank_it != ranks_.end()) {
            if (member.gold_withdrawn_today + amount > 
                rank_it->second.daily_gold_withdrawal_limit) {
                spdlog::warn("Player {} exceeded daily gold withdrawal limit", player_id);
                return false;
            }
        }
        
        guild_bank_gold_ -= amount;
        member.gold_withdrawn_today += amount;
        
        // Log transaction
        BankTransaction transaction;
        transaction.player_id = player_id;
        transaction.type = TransactionType::GOLD_WITHDRAWAL;
        transaction.amount = amount;
        transaction.timestamp = std::chrono::system_clock::now();
        
        bank_transactions_.push_back(transaction);
        
        return true;
    }
    
    // [SEQUENCE: MVP11-79] Add guild rank
    bool AddRank(const std::string& rank_name, uint32_t permissions) {
        if (ranks_.size() >= config_.max_ranks) {
            return false;
        }
        
        uint32_t new_rank_id = next_rank_id_++;
        
        GuildRank rank;
        rank.rank_id = new_rank_id;
        rank.rank_name = rank_name;
        rank.permissions = permissions;
        
        ranks_[new_rank_id] = rank;
        
        return true;
    }
    
    // [SEQUENCE: MVP11-80] Update member contribution
    void UpdateMemberContribution(uint64_t player_id, uint64_t points) {
        auto it = members_.find(player_id);
        if (it != members_.end()) {
            it->second.contribution_points += points;
        }
    }
    
    // [SEQUENCE: MVP11-81] Get member info
    const GuildMember* GetMember(uint64_t player_id) const {
        auto it = members_.find(player_id);
        return it != members_.end() ? &it->second : nullptr;
    }
    
    // [SEQUENCE: MVP11-82] Get all members
    std::vector<GuildMember> GetAllMembers() const {
        std::vector<GuildMember> result;
        for (const auto& [id, member] : members_) {
            result.push_back(member);
        }
        return result;
    }
    
    // [SEQUENCE: MVP11-83] Get online members
    std::vector<GuildMember> GetOnlineMembers() const {
        std::vector<GuildMember> result;
        auto now = std::chrono::system_clock::now();
        
        for (const auto& [id, member] : members_) {
            auto elapsed = std::chrono::duration_cast<std::chrono::minutes>(
                now - member.last_online
            ).count();
            
            if (elapsed < 5) {  // Consider online if seen in last 5 minutes
                result.push_back(member);
            }
        }
        
        return result;
    }
    
    // Guild properties
    uint32_t GetId() const { return guild_id_; }
    const std::string& GetName() const { return guild_name_; }
    uint32_t GetLevel() const { return guild_level_; }
    uint64_t GetExperience() const { return guild_experience_; }
    size_t GetMemberCount() const { return members_.size(); }
    bool IsDisbanded() const { return is_disbanded_; }
    
    // [SEQUENCE: MVP11-84] Level up guild
    void AddExperience(uint64_t exp) {
        guild_experience_ += exp;
        
        // Simple level calculation
        uint32_t new_level = 1;
        uint64_t required_exp = 1000;
        uint64_t total_exp = 0;
        
        while (total_exp + required_exp <= guild_experience_) {
            total_exp += required_exp;
            new_level++;
            required_exp = new_level * 1000;  // Linear progression
        }
        
        if (new_level > guild_level_) {
            guild_level_ = new_level;
            OnLevelUp();
        }
    }
    
private:
    uint32_t guild_id_;
    std::string guild_name_;
    std::string guild_tag_;  // Short tag like [ABC]
    uint64_t founder_id_;
    
    // Guild status
    uint32_t guild_level_ = 1;
    uint64_t guild_experience_ = 0;
    bool is_disbanded_ = false;
    
    // Members
    std::unordered_map<uint64_t, GuildMember> members_;
    std::unordered_map<uint32_t, GuildRank> ranks_;
    uint32_t next_rank_id_ = 0;
    
    // Guild bank
    uint64_t guild_bank_gold_ = 0;
    std::vector<GuildBankTab> bank_tabs_;
    
    // Guild info
    std::string motd_;
    uint64_t motd_setter_id_ = 0;
    std::chrono::system_clock::time_point motd_timestamp_;
    std::string guild_description_;
    
    // Timestamps
    std::chrono::system_clock::time_point creation_date_;
    
    // Configuration
    GuildConfig config_;
    
    // [SEQUENCE: MVP11-85] Transaction log
    enum class TransactionType {
        GOLD_DEPOSIT,
        GOLD_WITHDRAWAL,
        ITEM_DEPOSIT,
        ITEM_WITHDRAWAL
    };
    
    struct BankTransaction {
        uint64_t player_id;
        TransactionType type;
        uint64_t amount;  // Gold amount or item count
        uint32_t item_id = 0;  // For item transactions
        std::chrono::system_clock::time_point timestamp;
    };
    
    std::vector<BankTransaction> bank_transactions_;
    
    // [SEQUENCE: MVP11-86] Initialize default ranks
    void InitializeDefaultRanks() {
        // Guild Master
        GuildRank master_rank;
        master_rank.rank_id = 0;
        master_rank.rank_name = "Guild Master";
        master_rank.permissions = static_cast<uint32_t>(GuildPermission::ALL_PERMISSIONS);
        master_rank.daily_gold_withdrawal_limit = UINT32_MAX;
        ranks_[0] = master_rank;
        
        // Officer
        GuildRank officer_rank;
        officer_rank.rank_id = 1;
        officer_rank.rank_name = "Officer";
        officer_rank.GrantPermission(GuildPermission::INVITE_MEMBER);
        officer_rank.GrantPermission(GuildPermission::KICK_MEMBER);
        officer_rank.GrantPermission(GuildPermission::EDIT_MOTD);
        officer_rank.GrantPermission(GuildPermission::USE_GUILD_BANK);
        officer_rank.daily_gold_withdrawal_limit = 5000;
        ranks_[1] = officer_rank;
        
        // Member
        GuildRank member_rank;
        member_rank.rank_id = 2;
        member_rank.rank_name = "Member";
        member_rank.GrantPermission(GuildPermission::USE_GUILD_BANK);
        member_rank.daily_gold_withdrawal_limit = 1000;
        ranks_[2] = member_rank;
        
        // Initiate
        GuildRank initiate_rank;
        initiate_rank.rank_id = 3;
        initiate_rank.rank_name = "Initiate";
        initiate_rank.daily_gold_withdrawal_limit = 100;
        ranks_[3] = initiate_rank;
        
        next_rank_id_ = 4;
    }
    
    // [SEQUENCE: MVP11-87] Get lowest rank ID
    uint32_t GetLowestRankId() const {
        uint32_t lowest_id = 0;
        for (const auto& [id, rank] : ranks_) {
            if (id > lowest_id) {
                lowest_id = id;
            }
        }
        return lowest_id;
    }
    
    // [SEQUENCE: MVP11-88] Reset daily limits
    void ResetDailyLimits(GuildMember& member) {
        auto now = std::chrono::system_clock::now();
        auto last_reset = member.last_withdrawal_reset;
        
        auto hours_since_reset = std::chrono::duration_cast<std::chrono::hours>(
            now - last_reset
        ).count();
        
        if (hours_since_reset >= 24) {
            member.gold_withdrawn_today = 0;
            member.items_withdrawn_today = 0;
            member.last_withdrawal_reset = now;
        }
    }
    
    // [SEQUENCE: MVP11-89] On level up
    void OnLevelUp() {
        // Increase limits
        config_.max_members = 100 + (guild_level_ - 1) * 10;
        config_.max_bank_tabs = std::min(6u, 2u + guild_level_ / 5);
        
        spdlog::info("Guild {} leveled up to {}", guild_id_, guild_level_);
    }
};

// [SEQUENCE: MVP11-90] Guild manager
class GuildManager {
public:
    static GuildManager& Instance() {
        static GuildManager instance;
        return instance;
    }
    
    // [SEQUENCE: MVP11-91] Create guild
    std::shared_ptr<Guild> CreateGuild(const std::string& name, 
                                      uint64_t founder_id,
                                      const std::vector<uint64_t>& charter_signers) {
        // Validate name
        if (name.length() < 3 || name.length() > 24) {
            spdlog::warn("Invalid guild name length: {}", name);
            return nullptr;
        }
        
        // Check if name exists
        if (GuildNameExists(name)) {
            spdlog::warn("Guild name already exists: {}", name);
            return nullptr;
        }
        
        // Check charter signers
        if (charter_signers.size() < config_.min_members_to_create - 1) {
            spdlog::warn("Not enough charter signers for guild creation");
            return nullptr;
        }
        
        // Create guild
        uint32_t guild_id = next_guild_id_++;
        auto guild = std::make_shared<Guild>(guild_id, name, founder_id);
        
        // Add founder
        guild->AddMember(founder_id, "Founder");  // TODO: Get actual name
        guild->ChangeMemberRank(founder_id, 0);  // Guild Master rank
        
        // Add charter signers
        for (uint64_t signer_id : charter_signers) {
            guild->AddMember(signer_id, "Member");  // TODO: Get actual names
        }
        
        guilds_[guild_id] = guild;
        guild_name_index_[name] = guild_id;
        
        // Update player guild mapping
        player_guilds_[founder_id] = guild_id;
        for (uint64_t signer_id : charter_signers) {
            player_guilds_[signer_id] = guild_id;
        }
        
        spdlog::info("Guild {} created with ID {}", name, guild_id);
        return guild;
    }
    
    // [SEQUENCE: MVP11-92] Get guild
    std::shared_ptr<Guild> GetGuild(uint32_t guild_id) {
        auto it = guilds_.find(guild_id);
        return it != guilds_.end() ? it->second : nullptr;
    }
    
    // [SEQUENCE: MVP11-93] Get guild by name
    std::shared_ptr<Guild> GetGuildByName(const std::string& name) {
        auto it = guild_name_index_.find(name);
        if (it != guild_name_index_.end()) {
            return GetGuild(it->second);
        }
        return nullptr;
    }
    
    // [SEQUENCE: MVP11-94] Get player's guild
    std::shared_ptr<Guild> GetPlayerGuild(uint64_t player_id) {
        auto it = player_guilds_.find(player_id);
        if (it != player_guilds_.end()) {
            return GetGuild(it->second);
        }
        return nullptr;
    }
    
    // [SEQUENCE: MVP11-95] Process guild invite
    bool InviteToGuild(uint32_t guild_id, uint64_t inviter_id, 
                      uint64_t target_id, const std::string& target_name) {
        auto guild = GetGuild(guild_id);
        if (!guild) {
            return false;
        }
        
        // Check inviter permission
        if (!guild->HasPermission(inviter_id, GuildPermission::INVITE_MEMBER)) {
            return false;
        }
        
        // Check if target already in a guild
        if (player_guilds_.find(target_id) != player_guilds_.end()) {
            return false;
        }
        
        // Create invite
        GuildInvite invite;
        invite.guild_id = guild_id;
        invite.inviter_id = inviter_id;
        invite.target_id = target_id;
        invite.invite_time = std::chrono::system_clock::now();
        
        guild_invites_[target_id] = invite;
        
        return true;
    }
    
    // [SEQUENCE: MVP11-96] Accept guild invite
    bool AcceptGuildInvite(uint64_t player_id, const std::string& player_name) {
        auto invite_it = guild_invites_.find(player_id);
        if (invite_it == guild_invites_.end()) {
            return false;
        }
        
        auto guild = GetGuild(invite_it->second.guild_id);
        if (!guild) {
            return false;
        }
        
        if (guild->AddMember(player_id, player_name)) {
            player_guilds_[player_id] = guild->GetId();
            guild_invites_.erase(invite_it);
            return true;
        }
        
        return false;
    }
    
    // [SEQUENCE: MVP11-97] Leave guild
    bool LeaveGuild(uint64_t player_id) {
        auto guild = GetPlayerGuild(player_id);
        if (!guild) {
            return false;
        }
        
        guild->RemoveMember(player_id);
        player_guilds_.erase(player_id);
        
        // Check if guild should be disbanded
        if (guild->IsDisbanded()) {
            DisbandGuild(guild->GetId());
        }
        
        return true;
    }
    
    // [SEQUENCE: MVP11-98] Update player online status
    void UpdatePlayerOnlineStatus(uint64_t player_id) {
        auto guild = GetPlayerGuild(player_id);
        if (guild) {
            auto member = guild->GetMember(player_id);
            if (member) {
                guild->UpdateMemberContribution(player_id, 0);  // Just update last_online
            }
        }
    }
    
private:
    GuildManager() = default;
    
    std::unordered_map<uint32_t, std::shared_ptr<Guild>> guilds_;
    std::unordered_map<std::string, uint32_t> guild_name_index_;
    std::unordered_map<uint64_t, uint32_t> player_guilds_;
    
    struct GuildInvite {
        uint32_t guild_id;
        uint64_t inviter_id;
        uint64_t target_id;
        std::chrono::system_clock::time_point invite_time;
    };
    
    std::unordered_map<uint64_t, GuildInvite> guild_invites_;
    
    uint32_t next_guild_id_ = 1;
    GuildConfig config_;
    
    // [SEQUENCE: MVP11-99] Check if guild name exists
    bool GuildNameExists(const std::string& name) {
        return guild_name_index_.find(name) != guild_name_index_.end();
    }
    
    // [SEQUENCE: MVP11-100] Disband guild
    void DisbandGuild(uint32_t guild_id) {
        auto guild_it = guilds_.find(guild_id);
        if (guild_it == guilds_.end()) {
            return;
        }
        
        auto guild = guild_it->second;
        
        // Remove all player mappings
        auto members = guild->GetAllMembers();
        for (const auto& member : members) {
            player_guilds_.erase(member.player_id);
        }
        
        // Remove from name index
        guild_name_index_.erase(guild->GetName());
        
        // Remove guild
        guilds_.erase(guild_it);
        
        spdlog::info("Guild {} disbanded", guild_id);
    }
};