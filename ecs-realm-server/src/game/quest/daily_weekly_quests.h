#pragma once

#include "quest_system.h"
#include "quest_chains.h"
#include <chrono>
#include <unordered_set>
#include <random>

namespace mmorpg::game::quest {

// [SEQUENCE: 1429] Daily and weekly quest system
// 일일/주간 퀘스트 리셋, 로테이션, 보상 관리

// [SEQUENCE: 1430] Reset schedule
enum class ResetSchedule {
    DAILY_4AM,      // Daily at 4 AM server time
    DAILY_MIDNIGHT, // Daily at midnight
    WEEKLY_MONDAY,  // Weekly on Monday
    WEEKLY_TUESDAY, // Weekly on Tuesday
    WEEKLY_SUNDAY   // Weekly on Sunday
};

// [SEQUENCE: 1431] Daily quest configuration
struct DailyQuestConfig {
    uint32_t quest_pool_id;
    uint32_t max_daily_quests = 3;
    uint32_t max_completions_per_quest = 1;
    ResetSchedule reset_schedule = ResetSchedule::DAILY_4AM;
    
    // Reward multipliers for daily quests
    float experience_multiplier = 0.8f;
    float gold_multiplier = 1.0f;
    float reputation_multiplier = 1.2f;
    
    // Rotation settings
    bool use_rotation = true;
    uint32_t rotation_pool_size = 10;
    
    // Level-based filtering
    bool filter_by_level = true;
    int32_t level_range = 5;  // ±5 levels from player
};

// [SEQUENCE: 1432] Weekly quest configuration
struct WeeklyQuestConfig {
    uint32_t quest_pool_id;
    uint32_t max_weekly_quests = 3;
    uint32_t max_completions_per_quest = 1;
    ResetSchedule reset_schedule = ResetSchedule::WEEKLY_MONDAY;
    
    // Reward multipliers for weekly quests
    float experience_multiplier = 2.0f;
    float gold_multiplier = 2.5f;
    float reputation_multiplier = 3.0f;
    
    // Weekly quest specific
    bool require_all_dailies = false;  // Must complete all dailies first
    uint32_t min_daily_completions = 0;  // Min dailies needed
};

// [SEQUENCE: 1433] Quest pool for rotation
struct QuestPool {
    uint32_t pool_id;
    std::string pool_name;
    std::vector<uint32_t> quest_ids;
    
    // Weight-based selection
    std::unordered_map<uint32_t, float> quest_weights;
    
    // Conditions for quest availability
    std::function<bool(uint64_t)> availability_check;
    
    // Get random quests from pool
    std::vector<uint32_t> GetRandomQuests(uint32_t count, uint64_t entity_id) const;
};

// [SEQUENCE: 1434] Daily/Weekly quest progress
struct TimedQuestProgress {
    uint32_t quest_id;
    uint32_t completions = 0;
    std::chrono::system_clock::time_point first_completion;
    std::chrono::system_clock::time_point last_completion;
    
    bool CanCompleteAgain(uint32_t max_completions) const {
        return completions < max_completions;
    }
};

// [SEQUENCE: 1435] Player's daily/weekly data
struct PlayerTimedQuestData {
    uint64_t entity_id;
    
    // Daily quests
    std::unordered_map<uint32_t, TimedQuestProgress> daily_progress;
    std::chrono::system_clock::time_point last_daily_reset;
    
    // Weekly quests
    std::unordered_map<uint32_t, TimedQuestProgress> weekly_progress;
    std::chrono::system_clock::time_point last_weekly_reset;
    
    // Statistics
    uint32_t total_dailies_completed = 0;
    uint32_t total_weeklies_completed = 0;
    uint32_t consecutive_daily_days = 0;
    
    // Active quests for today/this week
    std::vector<uint32_t> available_daily_quests;
    std::vector<uint32_t> available_weekly_quests;
};

// [SEQUENCE: 1436] Daily quest manager
class DailyQuestManager {
public:
    DailyQuestManager(const DailyQuestConfig& config) : config_(config) {
        InitializeResetTime();
    }
    
    // [SEQUENCE: 1437] Check and perform daily reset
    bool CheckAndReset(PlayerTimedQuestData& player_data) {
        auto now = std::chrono::system_clock::now();
        auto next_reset = GetNextResetTime(player_data.last_daily_reset);
        
        if (now >= next_reset) {
            PerformDailyReset(player_data);
            return true;
        }
        
        return false;
    }
    
    // [SEQUENCE: 1438] Get available daily quests
    std::vector<uint32_t> GetAvailableQuests(uint64_t entity_id,
                                            const PlayerTimedQuestData& player_data) {
        // Check if reset needed
        auto now = std::chrono::system_clock::now();
        auto next_reset = GetNextResetTime(player_data.last_daily_reset);
        
        if (now >= next_reset || player_data.available_daily_quests.empty()) {
            // Need to generate new daily quests
            return GenerateDailyQuests(entity_id);
        }
        
        return player_data.available_daily_quests;
    }
    
    // [SEQUENCE: 1439] Can accept daily quest
    bool CanAcceptDailyQuest(uint32_t quest_id, 
                            const PlayerTimedQuestData& player_data) const {
        // Check if quest is in today's rotation
        auto it = std::find(player_data.available_daily_quests.begin(),
                           player_data.available_daily_quests.end(),
                           quest_id);
        
        if (it == player_data.available_daily_quests.end()) {
            return false;  // Not available today
        }
        
        // Check completion count
        auto progress_it = player_data.daily_progress.find(quest_id);
        if (progress_it != player_data.daily_progress.end()) {
            return progress_it->second.CanCompleteAgain(config_.max_completions_per_quest);
        }
        
        return true;  // Haven't done it yet
    }
    
    // [SEQUENCE: 1440] Complete daily quest
    void CompleteDailyQuest(uint32_t quest_id, PlayerTimedQuestData& player_data) {
        auto& progress = player_data.daily_progress[quest_id];
        progress.quest_id = quest_id;
        progress.completions++;
        
        auto now = std::chrono::system_clock::now();
        if (progress.completions == 1) {
            progress.first_completion = now;
        }
        progress.last_completion = now;
        
        player_data.total_dailies_completed++;
        
        spdlog::info("Entity {} completed daily quest {} ({} times today)",
                    player_data.entity_id, quest_id, progress.completions);
    }
    
    // [SEQUENCE: 1441] Get time until next reset
    std::chrono::seconds GetTimeUntilReset(const PlayerTimedQuestData& player_data) const {
        auto now = std::chrono::system_clock::now();
        auto next_reset = GetNextResetTime(player_data.last_daily_reset);
        
        return std::chrono::duration_cast<std::chrono::seconds>(next_reset - now);
    }
    
    // [SEQUENCE: 1442] Register quest pool
    void RegisterQuestPool(const QuestPool& pool) {
        quest_pools_[pool.pool_id] = pool;
    }
    
private:
    DailyQuestConfig config_;
    std::unordered_map<uint32_t, QuestPool> quest_pools_;
    std::mt19937 rng_{std::chrono::steady_clock::now().time_since_epoch().count()};
    
    // [SEQUENCE: 1443] Initialize reset time based on schedule
    void InitializeResetTime() {
        // Set up reset time calculation based on config
    }
    
    // [SEQUENCE: 1444] Calculate next reset time
    std::chrono::system_clock::time_point GetNextResetTime(
        std::chrono::system_clock::time_point last_reset) const {
        
        using namespace std::chrono;
        auto now = system_clock::now();
        auto now_time_t = system_clock::to_time_t(now);
        auto now_tm = *std::localtime(&now_time_t);
        
        // Calculate next reset based on schedule
        switch (config_.reset_schedule) {
            case ResetSchedule::DAILY_4AM: {
                // Set to 4 AM
                now_tm.tm_hour = 4;
                now_tm.tm_min = 0;
                now_tm.tm_sec = 0;
                
                auto reset_time = system_clock::from_time_t(std::mktime(&now_tm));
                if (reset_time <= now) {
                    // Already past 4 AM today, set to tomorrow
                    reset_time += hours(24);
                }
                return reset_time;
            }
            
            case ResetSchedule::DAILY_MIDNIGHT: {
                // Set to midnight
                now_tm.tm_hour = 0;
                now_tm.tm_min = 0;
                now_tm.tm_sec = 0;
                
                auto reset_time = system_clock::from_time_t(std::mktime(&now_tm));
                if (reset_time <= now) {
                    reset_time += hours(24);
                }
                return reset_time;
            }
            
            default:
                // Daily midnight as fallback
                return GetNextResetTime(last_reset);
        }
    }
    
    // [SEQUENCE: 1445] Perform daily reset
    void PerformDailyReset(PlayerTimedQuestData& player_data) {
        spdlog::info("Performing daily reset for entity {}", player_data.entity_id);
        
        // Clear daily progress
        player_data.daily_progress.clear();
        
        // Update reset time
        player_data.last_daily_reset = std::chrono::system_clock::now();
        
        // Update consecutive days
        // TODO: Check if player completed at least one daily yesterday
        
        // Generate new daily quests
        player_data.available_daily_quests = GenerateDailyQuests(player_data.entity_id);
    }
    
    // [SEQUENCE: 1446] Generate daily quests for player
    std::vector<uint32_t> GenerateDailyQuests(uint64_t entity_id) {
        auto pool_it = quest_pools_.find(config_.quest_pool_id);
        if (pool_it == quest_pools_.end()) {
            return {};
        }
        
        return pool_it->second.GetRandomQuests(config_.max_daily_quests, entity_id);
    }
};

// [SEQUENCE: 1447] Weekly quest manager
class WeeklyQuestManager {
public:
    WeeklyQuestManager(const WeeklyQuestConfig& config) : config_(config) {}
    
    // [SEQUENCE: 1448] Check and perform weekly reset
    bool CheckAndReset(PlayerTimedQuestData& player_data) {
        auto now = std::chrono::system_clock::now();
        auto next_reset = GetNextWeeklyReset(player_data.last_weekly_reset);
        
        if (now >= next_reset) {
            PerformWeeklyReset(player_data);
            return true;
        }
        
        return false;
    }
    
    // [SEQUENCE: 1449] Can accept weekly quest
    bool CanAcceptWeeklyQuest(uint32_t quest_id,
                             const PlayerTimedQuestData& player_data) const {
        // Check daily requirement
        if (config_.require_all_dailies) {
            // TODO: Check if all dailies completed this week
        }
        
        if (config_.min_daily_completions > 0) {
            // TODO: Check daily completion count
        }
        
        // Check if quest is available
        auto it = std::find(player_data.available_weekly_quests.begin(),
                           player_data.available_weekly_quests.end(),
                           quest_id);
        
        if (it == player_data.available_weekly_quests.end()) {
            return false;
        }
        
        // Check completion count
        auto progress_it = player_data.weekly_progress.find(quest_id);
        if (progress_it != player_data.weekly_progress.end()) {
            return progress_it->second.CanCompleteAgain(config_.max_completions_per_quest);
        }
        
        return true;
    }
    
    // [SEQUENCE: 1450] Complete weekly quest
    void CompleteWeeklyQuest(uint32_t quest_id, PlayerTimedQuestData& player_data) {
        auto& progress = player_data.weekly_progress[quest_id];
        progress.quest_id = quest_id;
        progress.completions++;
        
        auto now = std::chrono::system_clock::now();
        if (progress.completions == 1) {
            progress.first_completion = now;
        }
        progress.last_completion = now;
        
        player_data.total_weeklies_completed++;
        
        spdlog::info("Entity {} completed weekly quest {} ({} times this week)",
                    player_data.entity_id, quest_id, progress.completions);
    }
    
private:
    WeeklyQuestConfig config_;
    
    // [SEQUENCE: 1451] Get next weekly reset time
    std::chrono::system_clock::time_point GetNextWeeklyReset(
        std::chrono::system_clock::time_point last_reset) const {
        
        using namespace std::chrono;
        auto now = system_clock::now();
        auto now_time_t = system_clock::to_time_t(now);
        auto now_tm = *std::localtime(&now_time_t);
        
        // Calculate days until target weekday
        int target_weekday = 1;  // Monday by default
        switch (config_.reset_schedule) {
            case ResetSchedule::WEEKLY_MONDAY: target_weekday = 1; break;
            case ResetSchedule::WEEKLY_TUESDAY: target_weekday = 2; break;
            case ResetSchedule::WEEKLY_SUNDAY: target_weekday = 0; break;
            default: break;
        }
        
        int days_until_reset = (target_weekday - now_tm.tm_wday + 7) % 7;
        if (days_until_reset == 0 && now_tm.tm_hour >= 4) {
            days_until_reset = 7;  // Next week
        }
        
        // Set to 4 AM on target day
        now_tm.tm_hour = 4;
        now_tm.tm_min = 0;
        now_tm.tm_sec = 0;
        
        auto reset_time = system_clock::from_time_t(std::mktime(&now_tm));
        reset_time += hours(24 * days_until_reset);
        
        return reset_time;
    }
    
    // [SEQUENCE: 1452] Perform weekly reset
    void PerformWeeklyReset(PlayerTimedQuestData& player_data) {
        spdlog::info("Performing weekly reset for entity {}", player_data.entity_id);
        
        // Clear weekly progress
        player_data.weekly_progress.clear();
        
        // Update reset time
        player_data.last_weekly_reset = std::chrono::system_clock::now();
        
        // Generate new weekly quests
        // TODO: Generate based on player level and progression
    }
};

// [SEQUENCE: 1453] Timed quest system manager
class TimedQuestSystem {
public:
    static TimedQuestSystem& Instance() {
        static TimedQuestSystem instance;
        return instance;
    }
    
    // [SEQUENCE: 1454] Initialize system
    void Initialize(const DailyQuestConfig& daily_config,
                   const WeeklyQuestConfig& weekly_config) {
        daily_manager_ = std::make_unique<DailyQuestManager>(daily_config);
        weekly_manager_ = std::make_unique<WeeklyQuestManager>(weekly_config);
        
        spdlog::info("Timed quest system initialized");
    }
    
    // [SEQUENCE: 1455] Get or create player data
    PlayerTimedQuestData& GetPlayerData(uint64_t entity_id) {
        auto it = player_data_.find(entity_id);
        if (it == player_data_.end()) {
            PlayerTimedQuestData data;
            data.entity_id = entity_id;
            data.last_daily_reset = std::chrono::system_clock::now();
            data.last_weekly_reset = std::chrono::system_clock::now();
            
            player_data_[entity_id] = data;
            return player_data_[entity_id];
        }
        
        return it->second;
    }
    
    // [SEQUENCE: 1456] Check for resets
    void CheckResets(uint64_t entity_id) {
        auto& data = GetPlayerData(entity_id);
        
        if (daily_manager_) {
            daily_manager_->CheckAndReset(data);
        }
        
        if (weekly_manager_) {
            weekly_manager_->CheckAndReset(data);
        }
    }
    
    // [SEQUENCE: 1457] Get available quests
    std::vector<uint32_t> GetAvailableDailyQuests(uint64_t entity_id) {
        CheckResets(entity_id);
        auto& data = GetPlayerData(entity_id);
        
        if (daily_manager_) {
            return daily_manager_->GetAvailableQuests(entity_id, data);
        }
        
        return {};
    }
    
    // [SEQUENCE: 1458] Process quest completion
    void OnQuestCompleted(uint64_t entity_id, uint32_t quest_id) {
        auto& data = GetPlayerData(entity_id);
        
        // Check if it's a daily quest
        if (daily_manager_ && daily_manager_->CanAcceptDailyQuest(quest_id, data)) {
            daily_manager_->CompleteDailyQuest(quest_id, data);
            
            // Apply daily quest modifiers to rewards
            RewardModifiers modifiers;
            modifiers.experience_multiplier = 0.8f;
            modifiers.gold_multiplier = 1.0f;
            
            // TODO: Apply modifiers to quest rewards
        }
        
        // Check if it's a weekly quest
        if (weekly_manager_ && weekly_manager_->CanAcceptWeeklyQuest(quest_id, data)) {
            weekly_manager_->CompleteWeeklyQuest(quest_id, data);
            
            // Apply weekly quest modifiers
            RewardModifiers modifiers;
            modifiers.experience_multiplier = 2.0f;
            modifiers.gold_multiplier = 2.5f;
            
            // TODO: Apply modifiers to quest rewards
        }
    }
    
    // Getters
    DailyQuestManager* GetDailyManager() { return daily_manager_.get(); }
    WeeklyQuestManager* GetWeeklyManager() { return weekly_manager_.get(); }
    
private:
    TimedQuestSystem() = default;
    
    std::unique_ptr<DailyQuestManager> daily_manager_;
    std::unique_ptr<WeeklyQuestManager> weekly_manager_;
    std::unordered_map<uint64_t, PlayerTimedQuestData> player_data_;
};

// [SEQUENCE: 1459] Quest pool implementation
std::vector<uint32_t> QuestPool::GetRandomQuests(uint32_t count, uint64_t entity_id) const {
    if (quest_ids.empty() || count == 0) {
        return {};
    }
    
    // Check availability
    if (availability_check && !availability_check(entity_id)) {
        return {};
    }
    
    std::vector<uint32_t> available = quest_ids;
    std::vector<uint32_t> selected;
    
    // Use weights if available
    if (!quest_weights.empty()) {
        // Weighted random selection
        std::vector<float> weights;
        for (const auto& quest_id : available) {
            auto it = quest_weights.find(quest_id);
            weights.push_back(it != quest_weights.end() ? it->second : 1.0f);
        }
        
        std::discrete_distribution<> dist(weights.begin(), weights.end());
        std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
        
        for (uint32_t i = 0; i < count && !available.empty(); ++i) {
            int index = dist(rng);
            selected.push_back(available[index]);
            
            // Remove selected quest to avoid duplicates
            available.erase(available.begin() + index);
            weights.erase(weights.begin() + index);
            
            if (!weights.empty()) {
                dist = std::discrete_distribution<>(weights.begin(), weights.end());
            }
        }
    } else {
        // Simple random selection
        std::shuffle(available.begin(), available.end(), 
                    std::mt19937(std::chrono::steady_clock::now().time_since_epoch().count()));
        
        count = std::min(count, static_cast<uint32_t>(available.size()));
        selected.assign(available.begin(), available.begin() + count);
    }
    
    return selected;
}

} // namespace mmorpg::game::quest