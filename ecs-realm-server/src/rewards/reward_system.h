#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <functional>
#include <optional>
#include <mutex>
#include "../core/types.h"
#include "../inventory/inventory_system.h"

namespace mmorpg::rewards {

// [SEQUENCE: MVP13-180] Reward types
enum class RewardType {
    CURRENCY,          // 골드, 토큰 등
    ITEM,             // 아이템
    EXPERIENCE,       // 경험치
    TITLE,            // 칭호
    ACHIEVEMENT,      // 업적
    MOUNT,            // 탈것
    PET,              // 펫
    COSMETIC,         // 외형 아이템
    SKILL_POINT,      // 스킬 포인트
    REPUTATION,       // 평판
    BUFF,             // 임시 버프
    UNLOCK            // 콘텐츠 잠금 해제
};

// [SEQUENCE: MVP13-181] Currency types
enum class CurrencyType {
    GOLD,                    // 기본 화폐
    HONOR_POINTS,           // PvP 명예 점수
    ARENA_TOKENS,           // 아레나 토큰
    TOURNAMENT_TOKENS,      // 토너먼트 토큰
    DUNGEON_TOKENS,         // 던전 토큰
    RAID_TOKENS,            // 레이드 토큰
    GUILD_POINTS,           // 길드 포인트
    ACHIEVEMENT_POINTS,     // 업적 점수
    SEASONAL_TOKENS,        // 시즌 토큰
    PREMIUM_CURRENCY        // 프리미엄 화폐
};

// [SEQUENCE: MVP13-182] Reward definition
struct Reward {
    RewardType type;
    
    // Currency rewards
    CurrencyType currency_type{CurrencyType::GOLD};
    uint32_t currency_amount{0};
    
    // Item rewards
    uint32_t item_id{0};
    uint32_t item_count{1};
    ItemQuality item_quality{ItemQuality::COMMON};
    
    // Experience rewards
    uint64_t experience_amount{0};
    double experience_multiplier{1.0};
    
    // Special rewards
    std::string title_id;
    uint32_t achievement_id{0};
    uint32_t mount_id{0};
    uint32_t pet_id{0};
    uint32_t cosmetic_id{0};
    
    // Skill/reputation
    uint32_t skill_points{0};
    std::string reputation_faction;
    int32_t reputation_amount{0};
    
    // Buff rewards
    uint32_t buff_id{0};
    uint32_t buff_duration_minutes{60};
    
    // Unlock rewards
    std::string unlock_content_id;
    
    // Display info
    std::string display_name;
    std::string description;
    std::string icon_path;
};

// [SEQUENCE: MVP13-183] Reward package (multiple rewards)
struct RewardPackage {
    std::string package_id;
    std::string package_name;
    std::string description;
    
    std::vector<Reward> rewards;
    
    // Package conditions
    uint32_t required_level{1};
    std::vector<std::string> required_achievements;
    
    // Display
    std::string icon_path;
    bool show_all_rewards{true};  // Show all or just highlights
};

// [SEQUENCE: MVP13-184] Reward source tracking
struct RewardSource {
    enum class SourceType {
        QUEST,
        ACHIEVEMENT,
        ARENA_MATCH,
        TOURNAMENT,
        DUNGEON,
        RAID,
        WORLD_BOSS,
        DAILY_LOGIN,
        MILESTONE,
        EVENT,
        MAIL,
        GM_GRANT,
        SHOP_PURCHASE,
        LEVEL_UP
    } type;
    
    std::string source_id;      // Quest ID, achievement ID, etc.
    std::string source_name;    // Human-readable name
    
    std::chrono::system_clock::time_point timestamp;
};

// [SEQUENCE: MVP13-185] Reward history entry
struct RewardHistoryEntry {
    uint64_t entry_id;
    uint64_t player_id;
    
    Reward reward;
    RewardSource source;
    
    std::chrono::system_clock::time_point granted_time;
    bool claimed{false};
    std::chrono::system_clock::time_point claimed_time;
};

// [SEQUENCE: MVP13-186] Daily/weekly reward tracker
struct TimedRewardTracker {
    // Daily rewards
    struct DailyReward {
        uint32_t consecutive_days{0};
        std::chrono::system_clock::time_point last_claim_date;
        uint32_t current_streak{0};
        uint32_t best_streak{0};
    } daily;
    
    // Weekly rewards
    struct WeeklyReward {
        uint32_t activities_completed{0};
        std::vector<std::string> completed_activities;
        std::chrono::system_clock::time_point week_start;
        bool claimed{false};
    } weekly;
    
    // Monthly rewards
    struct MonthlyReward {
        uint32_t login_days{0};
        std::vector<uint32_t> claimed_days;
        uint32_t current_month;
        uint32_t current_year;
    } monthly;
};

// [SEQUENCE: MVP13-187] Reward claim conditions
struct ClaimCondition {
    enum class ConditionType {
        LEVEL_REQUIREMENT,
        ITEM_REQUIREMENT,
        CURRENCY_REQUIREMENT,
        ACHIEVEMENT_REQUIREMENT,
        QUEST_COMPLETION,
        TIME_WINDOW,
        INVENTORY_SPACE,
        FACTION_STANDING,
        GUILD_MEMBERSHIP
    } type;
    
    // Condition parameters
    uint32_t required_level{0};
    uint32_t required_item_id{0};
    uint32_t required_item_count{0};
    CurrencyType required_currency{CurrencyType::GOLD};
    uint32_t required_currency_amount{0};
    std::string required_achievement;
    std::string required_quest;
    
    // Time window
    std::chrono::system_clock::time_point window_start;
    std::chrono::system_clock::time_point window_end;
    
    // Space requirement
    uint32_t required_inventory_slots{1};
    
    // Faction/guild
    std::string required_faction;
    int32_t required_reputation{0};
    uint64_t required_guild_id{0};
    
    // Error message if condition not met
    std::string failure_message;
};

// [SEQUENCE: MVP13-188] Reward service
class RewardService {
public:
    // [SEQUENCE: MVP13-189] Grant reward to player
    bool GrantReward(uint64_t player_id, const Reward& reward, 
                    const RewardSource& source) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Log reward grant
        LogRewardGrant(player_id, reward, source);
        
        // Process based on reward type
        bool success = false;
        
        switch (reward.type) {
            case RewardType::CURRENCY:
                success = GrantCurrency(player_id, reward);
                break;
                
            case RewardType::ITEM:
                success = GrantItem(player_id, reward);
                break;
                
            case RewardType::EXPERIENCE:
                success = GrantExperience(player_id, reward);
                break;
                
            case RewardType::TITLE:
                success = GrantTitle(player_id, reward);
                break;
                
            case RewardType::ACHIEVEMENT:
                success = GrantAchievement(player_id, reward);
                break;
                
            case RewardType::MOUNT:
                success = GrantMount(player_id, reward);
                break;
                
            case RewardType::PET:
                success = GrantPet(player_id, reward);
                break;
                
            case RewardType::COSMETIC:
                success = GrantCosmetic(player_id, reward);
                break;
                
            case RewardType::SKILL_POINT:
                success = GrantSkillPoints(player_id, reward);
                break;
                
            case RewardType::REPUTATION:
                success = GrantReputation(player_id, reward);
                break;
                
            case RewardType::BUFF:
                success = GrantBuff(player_id, reward);
                break;
                
            case RewardType::UNLOCK:
                success = GrantUnlock(player_id, reward);
                break;
        }
        
        if (success) {
            // Add to history
            AddToHistory(player_id, reward, source);
            
            // Notify player
            NotifyRewardGranted(player_id, reward);
            
            // Fire reward event
            OnRewardGranted(player_id, reward, source);
        }
        
        return success;
    }
    
    // [SEQUENCE: MVP13-190] Grant reward package
    bool GrantRewardPackage(uint64_t player_id, const RewardPackage& package,
                          const RewardSource& source) {
        // Check package conditions
        if (!CheckPackageConditions(player_id, package)) {
            return false;
        }
        
        // Grant all rewards in package
        bool all_success = true;
        std::vector<Reward> granted_rewards;
        
        for (const auto& reward : package.rewards) {
            if (GrantReward(player_id, reward, source)) {
                granted_rewards.push_back(reward);
            } else {
                all_success = false;
                // Rollback granted rewards on failure
                RollbackRewards(player_id, granted_rewards);
                break;
            }
        }
        
        if (all_success) {
            NotifyPackageGranted(player_id, package);
        }
        
        return all_success;
    }
    
    // [SEQUENCE: MVP13-191] Check and grant daily rewards
    void ProcessDailyRewards(uint64_t player_id) {
        auto& tracker = GetTimedRewardTracker(player_id);
        
        auto now = std::chrono::system_clock::now();
        auto today = GetDateOnly(now);
        auto last_claim = GetDateOnly(tracker.daily.last_claim_date);
        
        // Check if already claimed today
        if (today == last_claim) {
            return;
        }
        
        // Check consecutive days
        auto days_diff = GetDaysDifference(last_claim, today);
        
        if (days_diff == 1) {
            // Consecutive day
            tracker.daily.consecutive_days++;
            tracker.daily.current_streak++;
        } else {
            // Streak broken
            tracker.daily.consecutive_days = 1;
            tracker.daily.current_streak = 1;
        }
        
        // Update best streak
        tracker.daily.best_streak = std::max(tracker.daily.best_streak,
                                            tracker.daily.current_streak);
        
        // Get appropriate daily reward
        auto daily_reward = GetDailyReward(tracker.daily.consecutive_days);
        
        // Grant reward
        RewardSource source;
        source.type = RewardSource::SourceType::DAILY_LOGIN;
        source.source_name = "Daily Login Bonus Day " + 
                           std::to_string(tracker.daily.consecutive_days);
        source.timestamp = now;
        
        GrantReward(player_id, daily_reward, source);
        
        // Update tracker
        tracker.daily.last_claim_date = now;
        SaveTimedRewardTracker(player_id, tracker);
    }
    
    // [SEQUENCE: MVP13-192] Process milestone rewards
    void CheckMilestoneRewards(uint64_t player_id, const std::string& milestone_type,
                             uint64_t progress_value) {
        auto milestones = GetMilestonesForType(milestone_type);
        
        for (const auto& milestone : milestones) {
            if (progress_value >= milestone.required_value &&
                !HasClaimedMilestone(player_id, milestone.milestone_id)) {
                
                // Grant milestone reward
                RewardSource source;
                source.type = RewardSource::SourceType::MILESTONE;
                source.source_id = milestone.milestone_id;
                source.source_name = milestone.name;
                source.timestamp = std::chrono::system_clock::now();
                
                GrantRewardPackage(player_id, milestone.reward_package, source);
                
                // Mark as claimed
                MarkMilestoneClaimed(player_id, milestone.milestone_id);
            }
        }
    }
    
    // [SEQUENCE: MVP13-193] Get reward history
    std::vector<RewardHistoryEntry> GetRewardHistory(uint64_t player_id,
                                                    uint32_t limit = 100) const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = player_reward_history_.find(player_id);
        if (it == player_reward_history_.end()) {
            return {};
        }
        
        const auto& history = it->second;
        
        // Return most recent entries
        std::vector<RewardHistoryEntry> recent_history;
        
        size_t start = history.size() > limit ? history.size() - limit : 0;
        for (size_t i = start; i < history.size(); ++i) {
            recent_history.push_back(history[i]);
        }
        
        return recent_history;
    }
    
    // [SEQUENCE: MVP13-194] Process weekly reset
    void ProcessWeeklyReset() {
        auto now = std::chrono::system_clock::now();
        
        // Reset weekly trackers for all players
        for (auto& [player_id, tracker] : timed_reward_trackers_) {
            tracker.weekly.activities_completed = 0;
            tracker.weekly.completed_activities.clear();
            tracker.weekly.week_start = now;
            tracker.weekly.claimed = false;
            
            SaveTimedRewardTracker(player_id, tracker);
        }
        
        spdlog::info("Weekly reward reset completed");
    }
    
    // [SEQUENCE: MVP13-195] Set up reward callbacks
    std::function<void(uint64_t, const Reward&, const RewardSource&)> OnRewardGranted;
    
    // Reward generation
    static Reward CreateCurrencyReward(CurrencyType type, uint32_t amount);
    static Reward CreateItemReward(uint32_t item_id, uint32_t count = 1);
    static Reward CreateExperienceReward(uint64_t amount);
    static Reward CreateTitleReward(const std::string& title_id);
    
private:
    // Player reward histories
    std::unordered_map<uint64_t, std::vector<RewardHistoryEntry>> player_reward_history_;
    
    // Timed reward tracking
    std::unordered_map<uint64_t, TimedRewardTracker> timed_reward_trackers_;
    
    // Milestone tracking
    std::unordered_map<uint64_t, std::unordered_set<std::string>> claimed_milestones_;
    
    mutable std::mutex mutex_;
    std::atomic<uint64_t> next_history_id_{1};
    
    // [SEQUENCE: MVP13-196] Grant specific reward types
    bool GrantCurrency(uint64_t player_id, const Reward& reward) {
        // Implementation would call currency service
        spdlog::info("Granting {} {} to player {}", 
                    reward.currency_amount, 
                    GetCurrencyName(reward.currency_type),
                    player_id);
        return true;
    }
    
    bool GrantItem(uint64_t player_id, const Reward& reward) {
        // Check inventory space
        if (!HasInventorySpace(player_id, reward.item_count)) {
            return false;
        }
        
        // Implementation would call inventory service
        spdlog::info("Granting item {} x{} to player {}", 
                    reward.item_id, reward.item_count, player_id);
        return true;
    }
    
    bool GrantExperience(uint64_t player_id, const Reward& reward) {
        uint64_t final_amount = reward.experience_amount * reward.experience_multiplier;
        // Implementation would call character service
        spdlog::info("Granting {} XP to player {}", final_amount, player_id);
        return true;
    }
    
    bool GrantTitle(uint64_t player_id, const Reward& reward) {
        // Implementation would call title service
        spdlog::info("Granting title '{}' to player {}", reward.title_id, player_id);
        return true;
    }
    
    bool GrantAchievement(uint64_t player_id, const Reward& reward) {
        // Implementation would call achievement service
        return true;
    }
    
    bool GrantMount(uint64_t player_id, const Reward& reward) {
        // Implementation would call mount service
        return true;
    }
    
    bool GrantPet(uint64_t player_id, const Reward& reward) {
        // Implementation would call pet service
        return true;
    }
    
    bool GrantCosmetic(uint64_t player_id, const Reward& reward) {
        // Implementation would call cosmetic service
        return true;
    }
    
    bool GrantSkillPoints(uint64_t player_id, const Reward& reward) {
        // Implementation would call skill service
        return true;
    }
    
    bool GrantReputation(uint64_t player_id, const Reward& reward) {
        // Implementation would call reputation service
        return true;
    }
    
    bool GrantBuff(uint64_t player_id, const Reward& reward) {
        // Implementation would call buff service
        return true;
    }
    
    bool GrantUnlock(uint64_t player_id, const Reward& reward) {
        // Implementation would call unlock service
        return true;
    }
    
    // [SEQUENCE: MVP13-197] Helper methods
    void LogRewardGrant(uint64_t player_id, const Reward& reward, 
                       const RewardSource& source) {
        Json::Value log_entry;
        log_entry["player_id"] = player_id;
        log_entry["reward_type"] = static_cast<int>(reward.type);
        log_entry["source_type"] = static_cast<int>(source.type);
        log_entry["source_id"] = source.source_id;
        log_entry["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
        
        // Log specific reward details
        switch (reward.type) {
            case RewardType::CURRENCY:
                log_entry["currency_type"] = static_cast<int>(reward.currency_type);
                log_entry["amount"] = reward.currency_amount;
                break;
            case RewardType::ITEM:
                log_entry["item_id"] = reward.item_id;
                log_entry["count"] = reward.item_count;
                break;
            // ... other types
        }
        
        // Write to reward log
        WriteRewardLog(log_entry);
    }
    
    void AddToHistory(uint64_t player_id, const Reward& reward,
                     const RewardSource& source) {
        RewardHistoryEntry entry;
        entry.entry_id = next_history_id_++;
        entry.player_id = player_id;
        entry.reward = reward;
        entry.source = source;
        entry.granted_time = std::chrono::system_clock::now();
        
        player_reward_history_[player_id].push_back(entry);
        
        // Limit history size
        auto& history = player_reward_history_[player_id];
        if (history.size() > MAX_HISTORY_SIZE) {
            history.erase(history.begin());
        }
    }
    
    void NotifyRewardGranted(uint64_t player_id, const Reward& reward) {
        // Send notification to player
        RewardNotification notification;
        notification.reward_type = reward.type;
        notification.display_name = reward.display_name;
        notification.icon_path = reward.icon_path;
        
        // Format message based on type
        notification.message = FormatRewardMessage(reward);
        
        SendNotification(player_id, notification);
    }
    
    void NotifyPackageGranted(uint64_t player_id, const RewardPackage& package) {
        // Send package notification
    }
    
    bool CheckPackageConditions(uint64_t player_id, const RewardPackage& package) {
        // Check level requirement
        if (GetPlayerLevel(player_id) < package.required_level) {
            return false;
        }
        
        // Check achievement requirements
        for (const auto& achievement : package.required_achievements) {
            if (!HasAchievement(player_id, achievement)) {
                return false;
            }
        }
        
        return true;
    }
    
    void RollbackRewards(uint64_t player_id, const std::vector<Reward>& rewards) {
        // Rollback granted rewards in case of failure
        for (const auto& reward : rewards) {
            RollbackReward(player_id, reward);
        }
    }
    
    void RollbackReward(uint64_t player_id, const Reward& reward) {
        // Implementation for each reward type
    }
    
    TimedRewardTracker& GetTimedRewardTracker(uint64_t player_id) {
        return timed_reward_trackers_[player_id];
    }
    
    void SaveTimedRewardTracker(uint64_t player_id, const TimedRewardTracker& tracker) {
        // Save to database
    }
    
    Reward GetDailyReward(uint32_t day_number) {
        // Progressive daily rewards
        // Days 1-6: Currency
        // Day 7: Special item
        // Days 8-13: Higher currency
        // Day 14: Mount/pet
        // etc.
        
        Reward reward;
        
        if (day_number % 7 == 0) {
            // Weekly milestone
            reward = CreateItemReward(GetWeeklyMilestoneItem(day_number / 7));
        } else if (day_number % 30 == 0) {
            // Monthly milestone
            reward.type = RewardType::MOUNT;
            reward.mount_id = GetMonthlyMount();
        } else {
            // Regular daily
            uint32_t base_amount = 100;
            uint32_t multiplier = (day_number - 1) / 7 + 1;
            reward = CreateCurrencyReward(CurrencyType::GOLD, 
                                        base_amount * multiplier);
        }
        
        return reward;
    }
    
    // Helper functions
    std::string GetCurrencyName(CurrencyType type) const {
        static const std::unordered_map<CurrencyType, std::string> names = {
            {CurrencyType::GOLD, "Gold"},
            {CurrencyType::HONOR_POINTS, "Honor Points"},
            {CurrencyType::ARENA_TOKENS, "Arena Tokens"},
            {CurrencyType::TOURNAMENT_TOKENS, "Tournament Tokens"},
            // ... etc
        };
        
        auto it = names.find(type);
        return it != names.end() ? it->second : "Unknown Currency";
    }
    
    std::string FormatRewardMessage(const Reward& reward) const {
        // Format user-friendly message
        return "You received " + reward.display_name + "!";
    }
    
    bool HasInventorySpace(uint64_t player_id, uint32_t slots_needed) const {
        // Check inventory space
        return true;
    }
    
    uint32_t GetPlayerLevel(uint64_t player_id) const {
        // Get player level
        return 1;
    }
    
    bool HasAchievement(uint64_t player_id, const std::string& achievement) const {
        // Check achievement
        return true;
    }
    
    std::chrono::system_clock::time_point GetDateOnly(
        std::chrono::system_clock::time_point time) const {
        // Strip time portion, keep date only
        return time;
    }
    
    int GetDaysDifference(std::chrono::system_clock::time_point t1,
                         std::chrono::system_clock::time_point t2) const {
        auto diff = t2 - t1;
        return std::chrono::duration_cast<std::chrono::hours>(diff).count() / 24;
    }
    
    void WriteRewardLog(const Json::Value& log_entry) {
        // Write to log file/database
    }
    
    void SendNotification(uint64_t player_id, const RewardNotification& notification) {
        // Send to player
    }
    
    struct RewardNotification {
        RewardType reward_type;
        std::string display_name;
        std::string message;
        std::string icon_path;
    };
    
    struct Milestone {
        std::string milestone_id;
        std::string name;
        uint64_t required_value;
        RewardPackage reward_package;
    };
    
    std::vector<Milestone> GetMilestonesForType(const std::string& type) const {
        // Get milestones
        return {};
    }
    
    bool HasClaimedMilestone(uint64_t player_id, const std::string& milestone_id) const {
        auto it = claimed_milestones_.find(player_id);
        if (it != claimed_milestones_.end()) {
            return it->second.count(milestone_id) > 0;
        }
        return false;
    }
    
    void MarkMilestoneClaimed(uint64_t player_id, const std::string& milestone_id) {
        claimed_milestones_[player_id].insert(milestone_id);
    }
    
    uint32_t GetWeeklyMilestoneItem(uint32_t week_number) const {
        // Return appropriate item ID
        return 10000 + week_number;
    }
    
    uint32_t GetMonthlyMount() const {
        // Return monthly mount ID
        return 50000;
    }
    
    static constexpr size_t MAX_HISTORY_SIZE = 1000;
};

// [SEQUENCE: MVP13-198] Reward factory methods
Reward RewardService::CreateCurrencyReward(CurrencyType type, uint32_t amount) {
    Reward reward;
    reward.type = RewardType::CURRENCY;
    reward.currency_type = type;
    reward.currency_amount = amount;
    reward.display_name = std::to_string(amount) + " " + GetCurrencyName(type);
    reward.icon_path = "icons/currency/" + std::to_string(static_cast<int>(type)) + ".png";
    return reward;
}

Reward RewardService::CreateItemReward(uint32_t item_id, uint32_t count) {
    Reward reward;
    reward.type = RewardType::ITEM;
    reward.item_id = item_id;
    reward.item_count = count;
    reward.display_name = GetItemName(item_id) + (count > 1 ? " x" + std::to_string(count) : "");
    reward.icon_path = "icons/items/" + std::to_string(item_id) + ".png";
    return reward;
}

Reward RewardService::CreateExperienceReward(uint64_t amount) {
    Reward reward;
    reward.type = RewardType::EXPERIENCE;
    reward.experience_amount = amount;
    reward.display_name = std::to_string(amount) + " Experience";
    reward.icon_path = "icons/misc/experience.png";
    return reward;
}

Reward RewardService::CreateTitleReward(const std::string& title_id) {
    Reward reward;
    reward.type = RewardType::TITLE;
    reward.title_id = title_id;
    reward.display_name = "Title: " + GetTitleDisplayName(title_id);
    reward.icon_path = "icons/titles/" + title_id + ".png";
    return reward;
}

// Static helper methods
static std::string GetCurrencyName(CurrencyType type) {
    // Implementation
    return "Currency";
}

static std::string GetItemName(uint32_t item_id) {
    // Implementation
    return "Item";
}

static std::string GetTitleDisplayName(const std::string& title_id) {
    // Implementation
    return title_id;
}

} // namespace mmorpg::rewards