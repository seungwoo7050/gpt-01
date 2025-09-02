#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <variant>
#include <optional>
#include <functional>

namespace mmorpg::game::quest {

// [SEQUENCE: MVP7-227] Quest system foundation

// [SEQUENCE: MVP7-228] Quest types
enum class QuestType {
    MAIN_STORY,
    SIDE_QUEST,
    DAILY,
    WEEKLY,
    REPEATABLE,
    CHAIN,
    HIDDEN,
    EVENT
};

// [SEQUENCE: MVP7-229] Quest states
enum class QuestState {
    NOT_AVAILABLE,
    AVAILABLE,
    ACTIVE,
    COMPLETED,
    REWARDED,
    FAILED,
    ABANDONED
};

// [SEQUENCE: MVP7-230] Quest objective types
enum class ObjectiveType {
    KILL,
    COLLECT,
    DELIVER,
    ESCORT,
    INTERACT,
    REACH_LOCATION,
    SURVIVE,
    TIMER,
    CUSTOM
};

// [SEQUENCE: MVP7-231] Quest requirement
struct QuestRequirement {
    uint32_t min_level = 1;
    uint32_t max_level = 0;
    std::vector<uint32_t> required_quests;
    std::vector<uint32_t> required_items;
    std::vector<uint32_t> required_skills;
    uint32_t required_class = 0;
    uint32_t required_faction = 0;
    std::function<bool(uint64_t)> custom_check;
};

// [SEQUENCE: MVP7-232] Quest objective
struct QuestObjective {
    uint32_t objective_id;
    ObjectiveType type;
    std::string description;
    uint32_t target_id = 0;
    uint32_t target_count = 1;
    uint32_t current_count = 0;
    float target_x = 0.0f;
    float target_y = 0.0f;
    float target_z = 0.0f;
    float radius = 5.0f;
    uint32_t time_limit_seconds = 0;
    bool is_optional = false;
    bool is_hidden = false;
    bool IsComplete() const;
    float GetProgress() const;
};

// [SEQUENCE: MVP7-233] Quest reward
struct QuestReward {
    uint64_t experience = 0;
    uint64_t gold = 0;
    uint64_t reputation = 0;
    struct ItemReward {
        uint32_t item_id;
        uint32_t count;
        float chance = 1.0f;
    };
    std::vector<ItemReward> guaranteed_items;
    std::vector<ItemReward> choice_items;
    std::vector<ItemReward> random_items;
    std::vector<uint32_t> skill_ids;
    std::vector<uint32_t> title_ids;
    std::vector<uint32_t> unlock_quest_ids;
};

// [SEQUENCE: MVP7-234] Quest definition
struct QuestDefinition {
    uint32_t quest_id;
    std::string name;
    std::string description;
    QuestType type;
    QuestRequirement requirements;
    std::vector<QuestObjective> objectives;
    bool all_objectives_required = true;
    QuestReward rewards;
    uint32_t start_npc_id = 0;
    uint32_t end_npc_id = 0;
    std::string start_dialogue;
    std::string progress_dialogue;
    std::string complete_dialogue;
    uint32_t time_limit_seconds = 0;
    bool is_repeatable = false;
    uint32_t cooldown_seconds = 0;
    uint32_t max_completions = 0;
    uint32_t next_quest_id = 0;
    bool auto_complete = false;
    bool share_progress = false;
};

// [SEQUENCE: MVP7-235] Quest progress
struct QuestProgress {
    uint32_t quest_id;
    QuestState state = QuestState::NOT_AVAILABLE;
    std::vector<QuestObjective> objectives;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point complete_time;
    std::chrono::steady_clock::time_point last_update;
    uint32_t completion_count = 0;
    std::chrono::steady_clock::time_point last_completion;
    bool IsComplete() const;
    float GetProgress() const;
};

// [SEQUENCE: MVP7-236] Quest log (per player)
class QuestLog {
public:
    QuestLog(uint64_t entity_id) : entity_id_(entity_id) {}
    
    // [SEQUENCE: MVP7-237] Quest management
    bool CanAcceptQuest(uint32_t quest_id) const;
    bool AcceptQuest(uint32_t quest_id);
    bool AbandonQuest(uint32_t quest_id);
    bool CompleteQuest(uint32_t quest_id);
    
    // [SEQUENCE: MVP7-238] Progress tracking
    void UpdateObjectiveProgress(uint32_t quest_id, uint32_t objective_id, int32_t delta = 1);
    void UpdateProgress(ObjectiveType type, uint32_t target_id, int32_t count = 1);
    
    // [SEQUENCE: MVP7-239] Quest queries
    QuestProgress* GetQuestProgress(uint32_t quest_id);
    const QuestProgress* GetQuestProgress(uint32_t quest_id) const;
    std::vector<uint32_t> GetActiveQuests() const;
    std::vector<uint32_t> GetCompletedQuests() const;
    bool HasCompletedQuest(uint32_t quest_id) const;
    
    // [SEQUENCE: MVP7-240] Quest limits
    size_t GetActiveQuestCount() const { return active_quests_.size(); }
    static constexpr size_t MAX_ACTIVE_QUESTS = 25;
    
private:
    uint64_t entity_id_;
    std::unordered_map<uint32_t, QuestProgress> active_quests_;
    std::unordered_set<uint32_t> completed_quests_;
    std::unordered_map<uint32_t, uint32_t> quest_completion_counts_;
    
    // [SEQUENCE: MVP7-241] Internal helpers
    bool MeetsRequirements(const QuestDefinition& quest) const;
    void InitializeObjectives(QuestProgress& progress, const QuestDefinition& quest);
};

// [SEQUENCE: MVP7-242] Quest manager (global)
class QuestManager {
public:
    static QuestManager& Instance();
    
    // [SEQUENCE: MVP7-243] Quest registration
    void RegisterQuest(const QuestDefinition& quest);
    const QuestDefinition* GetQuest(uint32_t quest_id) const;
    
    // [SEQUENCE: MVP7-244] Quest log management
    std::shared_ptr<QuestLog> CreateQuestLog(uint64_t entity_id);
    std::shared_ptr<QuestLog> GetQuestLog(uint64_t entity_id) const;
    void RemoveQuestLog(uint64_t entity_id);
    
    // [SEQUENCE: MVP7-245] Quest availability
    std::vector<uint32_t> GetAvailableQuests(uint64_t entity_id) const;
    std::vector<uint32_t> GetQuestsByType(QuestType type) const;
    std::vector<uint32_t> GetQuestsByNPC(uint32_t npc_id) const;
    
    // [SEQUENCE: MVP7-246] Global progress updates
    void OnMonsterKilled(uint64_t killer_id, uint32_t monster_id);
    void OnItemCollected(uint64_t collector_id, uint32_t item_id, uint32_t count);
    void OnLocationReached(uint64_t entity_id, float x, float y, float z);
    void OnNPCInteraction(uint64_t entity_id, uint32_t npc_id);
    
    // [SEQUENCE: MVP7-247] Quest chains
    void ProcessQuestChain(uint64_t entity_id, uint32_t completed_quest_id);
    
    // [SEQUENCE: MVP7-248] Daily/Weekly reset
    void ResetDailyQuests();
    void ResetWeeklyQuests();
    
private:
    QuestManager() = default;
    std::unordered_map<uint32_t, QuestDefinition> quest_definitions_;
    std::unordered_map<uint64_t, std::shared_ptr<QuestLog>> quest_logs_;
    std::unordered_map<QuestType, std::vector<uint32_t>> quests_by_type_;
    std::unordered_map<uint32_t, std::vector<uint32_t>> quests_by_npc_;
};

// [SEQUENCE: MVP7-249] Quest factory
class QuestFactory {
public:
    // [SEQUENCE: MVP7-250] Main story quest
    static QuestDefinition CreateMainStoryQuest();
    
    // [SEQUENCE: MVP7-251] Kill quest
    static QuestDefinition CreateKillQuest();
    
    // [SEQUENCE: MVP7-252] Collection quest
    static QuestDefinition CreateCollectionQuest();
    
    // [SEQUENCE: MVP7-253] Chain quest
    static QuestDefinition CreateChainQuest(uint32_t quest_id, uint32_t chain_index);
};

// [SEQUENCE: MVP7-254] Quest events
enum class QuestEventType {
    QUEST_ACCEPTED,
    QUEST_COMPLETED,
    QUEST_FAILED,
    QUEST_ABANDONED,
    OBJECTIVE_PROGRESS,
    OBJECTIVE_COMPLETED
};

struct QuestEvent {
    QuestEventType type;
    uint64_t entity_id;
    uint32_t quest_id;
    uint32_t objective_id = 0;
    std::chrono::steady_clock::time_point timestamp;
};

} // namespace mmorpg::game::quest
