#pragma once

#include "quest_system.h"
#include "quest_chains.h"
#include <chrono>
#include <unordered_set>
#include <random>

namespace mmorpg::game::quest {

// [SEQUENCE: MVP7-364] Daily and weekly quest system

// [SEQUENCE: MVP7-365] Reset schedule
enum class ResetSchedule {
    DAILY_4AM,
    DAILY_MIDNIGHT,
    WEEKLY_MONDAY,
    WEEKLY_TUESDAY,
    WEEKLY_SUNDAY
};

// [SEQUENCE: MVP7-366] Daily quest configuration
struct DailyQuestConfig {
    uint32_t quest_pool_id;
    uint32_t max_daily_quests = 3;
    uint32_t max_completions_per_quest = 1;
    ResetSchedule reset_schedule = ResetSchedule::DAILY_4AM;
    float experience_multiplier = 0.8f;
    float gold_multiplier = 1.0f;
    float reputation_multiplier = 1.2f;
    bool use_rotation = true;
    uint32_t rotation_pool_size = 10;
    bool filter_by_level = true;
    int32_t level_range = 5;
};

// [SEQUENCE: MVP7-367] Weekly quest configuration
struct WeeklyQuestConfig {
    uint32_t quest_pool_id;
    uint32_t max_weekly_quests = 3;
    uint32_t max_completions_per_quest = 1;
    ResetSchedule reset_schedule = ResetSchedule::WEEKLY_MONDAY;
    float experience_multiplier = 2.0f;
    float gold_multiplier = 2.5f;
    float reputation_multiplier = 3.0f;
    bool require_all_dailies = false;
    uint32_t min_daily_completions = 0;
};

// [SEQUENCE: MVP7-368] Quest pool for rotation
struct QuestPool {
    uint32_t pool_id;
    std::string pool_name;
    std::vector<uint32_t> quest_ids;
    std::unordered_map<uint32_t, float> quest_weights;
    std::function<bool(uint64_t)> availability_check;
    std::vector<uint32_t> GetRandomQuests(uint32_t count, uint64_t entity_id) const;
};

// [SEQUENCE: MVP7-369] Daily/Weekly quest progress
struct TimedQuestProgress {
    uint32_t quest_id;
    uint32_t completions = 0;
    std::chrono::system_clock::time_point first_completion;
    std::chrono::system_clock::time_point last_completion;
    bool CanCompleteAgain(uint32_t max_completions) const;
};

// [SEQUENCE: MVP7-370] Player's daily/weekly data
struct PlayerTimedQuestData {
    uint64_t entity_id;
    std::unordered_map<uint32_t, TimedQuestProgress> daily_progress;
    std::chrono::system_clock::time_point last_daily_reset;
    std::unordered_map<uint32_t, TimedQuestProgress> weekly_progress;
    std::chrono::system_clock::time_point last_weekly_reset;
    uint32_t total_dailies_completed = 0;
    uint32_t total_weeklies_completed = 0;
    uint32_t consecutive_daily_days = 0;
    std::vector<uint32_t> available_daily_quests;
    std::vector<uint32_t> available_weekly_quests;
};

// [SEQUENCE: MVP7-371] Daily quest manager
class DailyQuestManager {
public:
    DailyQuestManager(const DailyQuestConfig& config);
    // [SEQUENCE: MVP7-372] Check and perform daily reset
    bool CheckAndReset(PlayerTimedQuestData& player_data);
    // [SEQUENCE: MVP7-373] Get available daily quests
    std::vector<uint32_t> GetAvailableQuests(uint64_t entity_id, const PlayerTimedQuestData& player_data);
    // [SEQUENCE: MVP7-374] Can accept daily quest
    bool CanAcceptDailyQuest(uint32_t quest_id, const PlayerTimedQuestData& player_data) const;
    // [SEQUENCE: MVP7-375] Complete daily quest
    void CompleteDailyQuest(uint32_t quest_id, PlayerTimedQuestData& player_data);
    // [SEQUENCE: MVP7-376] Get time until next reset
    std::chrono::seconds GetTimeUntilReset(const PlayerTimedQuestData& player_data) const;
    // [SEQUENCE: MVP7-377] Register quest pool
    void RegisterQuestPool(const QuestPool& pool);
private:
    DailyQuestConfig config_;
    std::unordered_map<uint32_t, QuestPool> quest_pools_;
    std::mt19937 rng_;
    // [SEQUENCE: MVP7-378] Initialize reset time based on schedule
    void InitializeResetTime();
    // [SEQUENCE: MVP7-379] Calculate next reset time
    std::chrono::system_clock::time_point GetNextResetTime(std::chrono::system_clock::time_point last_reset) const;
    // [SEQUENCE: MVP7-380] Perform daily reset
    void PerformDailyReset(PlayerTimedQuestData& player_data);
    // [SEQUENCE: MVP7-381] Generate daily quests for player
    std::vector<uint32_t> GenerateDailyQuests(uint64_t entity_id);
};

// [SEQUENCE: MVP7-382] Weekly quest manager
class WeeklyQuestManager {
public:
    WeeklyQuestManager(const WeeklyQuestConfig& config);
    // [SEQUENCE: MVP7-383] Check and perform weekly reset
    bool CheckAndReset(PlayerTimedQuestData& player_data);
    // [SEQUENCE: MVP7-384] Can accept weekly quest
    bool CanAcceptWeeklyQuest(uint32_t quest_id, const PlayerTimedQuestData& player_data) const;
    // [SEQUENCE: MVP7-385] Complete weekly quest
    void CompleteWeeklyQuest(uint32_t quest_id, PlayerTimedQuestData& player_data);
private:
    WeeklyQuestConfig config_;
    // [SEQUENCE: MVP7-386] Get next weekly reset time
    std::chrono::system_clock::time_point GetNextWeeklyReset(std::chrono::system_clock::time_point last_reset) const;
    // [SEQUENCE: MVP7-387] Perform weekly reset
    void PerformWeeklyReset(PlayerTimedQuestData& player_data);
};

// [SEQUENCE: MVP7-388] Timed quest system manager
class TimedQuestSystem {
public:
    static TimedQuestSystem& Instance();
    // [SEQUENCE: MVP7-389] Initialize system
    void Initialize(const DailyQuestConfig& daily_config, const WeeklyQuestConfig& weekly_config);
    // [SEQUENCE: MVP7-390] Get or create player data
    PlayerTimedQuestData& GetPlayerData(uint64_t entity_id);
    // [SEQUENCE: MVP7-391] Check for resets
    void CheckResets(uint64_t entity_id);
    // [SEQUENCE: MVP7-392] Get available quests
    std::vector<uint32_t> GetAvailableDailyQuests(uint64_t entity_id);
    // [SEQUENCE: MVP7-393] Process quest completion
    void OnQuestCompleted(uint64_t entity_id, uint32_t quest_id);
    DailyQuestManager* GetDailyManager();
    WeeklyQuestManager* GetWeeklyManager();
private:
    TimedQuestSystem() = default;
    std::unique_ptr<DailyQuestManager> daily_manager_;
    std::unique_ptr<WeeklyQuestManager> weekly_manager_;
    std::unordered_map<uint64_t, PlayerTimedQuestData> player_data_;
};

// [SEQUENCE: MVP7-394] Quest pool implementation

} // namespace mmorpg::game::quest
