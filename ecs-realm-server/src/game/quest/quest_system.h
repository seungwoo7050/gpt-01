#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <variant>
#include <optional>
#include <functional>

namespace mmorpg::game::quest {

// [SEQUENCE: 1309] Quest system foundation
// 퀘스트 정의, 진행 상태, 보상을 관리하는 시스템

// [SEQUENCE: 1310] Quest types
enum class QuestType {
    MAIN_STORY,         // 메인 스토리
    SIDE_QUEST,         // 사이드 퀘스트
    DAILY,              // 일일 퀘스트
    WEEKLY,             // 주간 퀘스트
    REPEATABLE,         // 반복 가능
    CHAIN,              // 연계 퀘스트
    HIDDEN,             // 숨겨진 퀘스트
    EVENT               // 이벤트 퀘스트
};

// [SEQUENCE: 1311] Quest states
enum class QuestState {
    NOT_AVAILABLE,      // 수락 불가
    AVAILABLE,          // 수락 가능
    ACTIVE,             // 진행 중
    COMPLETED,          // 완료 (보상 대기)
    REWARDED,           // 보상 수령 완료
    FAILED,             // 실패
    ABANDONED           // 포기
};

// [SEQUENCE: 1312] Quest objective types
enum class ObjectiveType {
    KILL,               // 몬스터 처치
    COLLECT,            // 아이템 수집
    DELIVER,            // 아이템 전달
    ESCORT,             // NPC 호위
    INTERACT,           // 오브젝트 상호작용
    REACH_LOCATION,     // 특정 위치 도달
    SURVIVE,            // 생존
    TIMER,              // 시간 제한
    CUSTOM              // 커스텀 조건
};

// [SEQUENCE: 1313] Quest requirement
struct QuestRequirement {
    uint32_t min_level = 1;
    uint32_t max_level = 0;         // 0 = no limit
    std::vector<uint32_t> required_quests;  // Prerequisites
    std::vector<uint32_t> required_items;
    std::vector<uint32_t> required_skills;
    uint32_t required_class = 0;    // 0 = any class
    uint32_t required_faction = 0;  // 0 = any faction
    
    // Custom requirements
    std::function<bool(uint64_t)> custom_check;
};

// [SEQUENCE: 1314] Quest objective
struct QuestObjective {
    uint32_t objective_id;
    ObjectiveType type;
    std::string description;
    
    // Target information
    uint32_t target_id = 0;         // Monster/Item/NPC ID
    uint32_t target_count = 1;      // Required count
    uint32_t current_count = 0;     // Progress
    
    // Location (for REACH_LOCATION)
    float target_x = 0.0f;
    float target_y = 0.0f;
    float target_z = 0.0f;
    float radius = 5.0f;
    
    // Timer (for TIMER objectives)
    uint32_t time_limit_seconds = 0;
    
    // Optional objectives
    bool is_optional = false;
    bool is_hidden = false;         // Hidden until discovered
    
    // Completion check
    bool IsComplete() const {
        return current_count >= target_count;
    }
    
    float GetProgress() const {
        if (target_count == 0) return 1.0f;
        return static_cast<float>(current_count) / target_count;
    }
};

// [SEQUENCE: 1315] Quest reward
struct QuestReward {
    // Experience and currency
    uint64_t experience = 0;
    uint64_t gold = 0;
    uint64_t reputation = 0;
    
    // Items
    struct ItemReward {
        uint32_t item_id;
        uint32_t count;
        float chance = 1.0f;        // For random rewards
    };
    std::vector<ItemReward> guaranteed_items;
    std::vector<ItemReward> choice_items;  // Player chooses one
    std::vector<ItemReward> random_items;  // Chance-based
    
    // Skills/Abilities
    std::vector<uint32_t> skill_ids;
    
    // Titles
    std::vector<uint32_t> title_ids;
    
    // Follow-up quests
    std::vector<uint32_t> unlock_quest_ids;
};

// [SEQUENCE: 1316] Quest definition
struct QuestDefinition {
    uint32_t quest_id;
    std::string name;
    std::string description;
    QuestType type;
    
    // Requirements
    QuestRequirement requirements;
    
    // Objectives
    std::vector<QuestObjective> objectives;
    bool all_objectives_required = true;  // false = any objective
    
    // Rewards
    QuestReward rewards;
    
    // Quest giver/completer NPCs
    uint32_t start_npc_id = 0;
    uint32_t end_npc_id = 0;
    
    // Dialogue (client hint)
    std::string start_dialogue;
    std::string progress_dialogue;
    std::string complete_dialogue;
    
    // Time limit
    uint32_t time_limit_seconds = 0;  // 0 = no limit
    
    // Repeatable settings
    bool is_repeatable = false;
    uint32_t cooldown_seconds = 0;
    uint32_t max_completions = 0;     // 0 = unlimited
    
    // Chain quest
    uint32_t next_quest_id = 0;       // Auto-start next quest
    
    // Flags
    bool auto_complete = false;        // Complete without NPC
    bool share_progress = false;       // Party members share progress
};

// [SEQUENCE: 1317] Quest progress
struct QuestProgress {
    uint32_t quest_id;
    QuestState state = QuestState::NOT_AVAILABLE;
    std::vector<QuestObjective> objectives;  // Copy with progress
    
    // Timing
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point complete_time;
    std::chrono::steady_clock::time_point last_update;
    
    // Completion tracking
    uint32_t completion_count = 0;
    std::chrono::steady_clock::time_point last_completion;
    
    // Check if quest is complete
    bool IsComplete() const {
        if (state != QuestState::ACTIVE) return false;
        
        for (const auto& obj : objectives) {
            if (!obj.is_optional && !obj.IsComplete()) {
                return false;
            }
        }
        return true;
    }
    
    // Get overall progress
    float GetProgress() const {
        if (objectives.empty()) return 0.0f;
        
        float total_progress = 0.0f;
        int required_objectives = 0;
        
        for (const auto& obj : objectives) {
            if (!obj.is_optional) {
                total_progress += obj.GetProgress();
                required_objectives++;
            }
        }
        
        return required_objectives > 0 ? total_progress / required_objectives : 1.0f;
    }
};

// [SEQUENCE: 1318] Quest log (per player)
class QuestLog {
public:
    QuestLog(uint64_t entity_id) : entity_id_(entity_id) {}
    
    // [SEQUENCE: 1319] Quest management
    bool CanAcceptQuest(uint32_t quest_id) const;
    bool AcceptQuest(uint32_t quest_id);
    bool AbandonQuest(uint32_t quest_id);
    bool CompleteQuest(uint32_t quest_id);
    
    // [SEQUENCE: 1320] Progress tracking
    void UpdateObjectiveProgress(uint32_t quest_id, uint32_t objective_id, 
                                int32_t delta = 1);
    void UpdateProgress(ObjectiveType type, uint32_t target_id, 
                       int32_t count = 1);
    
    // [SEQUENCE: 1321] Quest queries
    QuestProgress* GetQuestProgress(uint32_t quest_id);
    const QuestProgress* GetQuestProgress(uint32_t quest_id) const;
    std::vector<uint32_t> GetActiveQuests() const;
    std::vector<uint32_t> GetCompletedQuests() const;
    bool HasCompletedQuest(uint32_t quest_id) const;
    
    // [SEQUENCE: 1322] Quest limits
    size_t GetActiveQuestCount() const { return active_quests_.size(); }
    static constexpr size_t MAX_ACTIVE_QUESTS = 25;
    
private:
    uint64_t entity_id_;
    
    // Active quests
    std::unordered_map<uint32_t, QuestProgress> active_quests_;
    
    // Completed quest history
    std::unordered_set<uint32_t> completed_quests_;
    std::unordered_map<uint32_t, uint32_t> quest_completion_counts_;
    
    // [SEQUENCE: 1323] Internal helpers
    bool MeetsRequirements(const QuestDefinition& quest) const;
    void InitializeObjectives(QuestProgress& progress, const QuestDefinition& quest);
};

// [SEQUENCE: 1324] Quest manager (global)
class QuestManager {
public:
    static QuestManager& Instance() {
        static QuestManager instance;
        return instance;
    }
    
    // [SEQUENCE: 1325] Quest registration
    void RegisterQuest(const QuestDefinition& quest);
    const QuestDefinition* GetQuest(uint32_t quest_id) const;
    
    // [SEQUENCE: 1326] Quest log management
    std::shared_ptr<QuestLog> CreateQuestLog(uint64_t entity_id);
    std::shared_ptr<QuestLog> GetQuestLog(uint64_t entity_id) const;
    void RemoveQuestLog(uint64_t entity_id);
    
    // [SEQUENCE: 1327] Quest availability
    std::vector<uint32_t> GetAvailableQuests(uint64_t entity_id) const;
    std::vector<uint32_t> GetQuestsByType(QuestType type) const;
    std::vector<uint32_t> GetQuestsByNPC(uint32_t npc_id) const;
    
    // [SEQUENCE: 1328] Global progress updates
    void OnMonsterKilled(uint64_t killer_id, uint32_t monster_id);
    void OnItemCollected(uint64_t collector_id, uint32_t item_id, uint32_t count);
    void OnLocationReached(uint64_t entity_id, float x, float y, float z);
    void OnNPCInteraction(uint64_t entity_id, uint32_t npc_id);
    
    // [SEQUENCE: 1329] Quest chains
    void ProcessQuestChain(uint64_t entity_id, uint32_t completed_quest_id);
    
    // [SEQUENCE: 1330] Daily/Weekly reset
    void ResetDailyQuests();
    void ResetWeeklyQuests();
    
private:
    QuestManager() = default;
    
    // Quest database
    std::unordered_map<uint32_t, QuestDefinition> quest_definitions_;
    
    // Player quest logs
    std::unordered_map<uint64_t, std::shared_ptr<QuestLog>> quest_logs_;
    
    // Quest indices
    std::unordered_map<QuestType, std::vector<uint32_t>> quests_by_type_;
    std::unordered_map<uint32_t, std::vector<uint32_t>> quests_by_npc_;
};

// [SEQUENCE: 1331] Quest factory
class QuestFactory {
public:
    // [SEQUENCE: 1332] Main story quest
    static QuestDefinition CreateMainStoryQuest() {
        QuestDefinition quest;
        quest.quest_id = 1001;
        quest.name = "The Hero's Journey";
        quest.description = "Begin your epic adventure";
        quest.type = QuestType::MAIN_STORY;
        
        // Requirements
        quest.requirements.min_level = 1;
        
        // Objectives
        QuestObjective obj1;
        obj1.objective_id = 1;
        obj1.type = ObjectiveType::INTERACT;
        obj1.description = "Speak with the Village Elder";
        obj1.target_id = 1001;  // NPC ID
        obj1.target_count = 1;
        quest.objectives.push_back(obj1);
        
        // Rewards
        quest.rewards.experience = 100;
        quest.rewards.gold = 50;
        quest.rewards.guaranteed_items.push_back({2001, 1});  // Starter weapon
        
        return quest;
    }
    
    // [SEQUENCE: 1333] Kill quest
    static QuestDefinition CreateKillQuest() {
        QuestDefinition quest;
        quest.quest_id = 2001;
        quest.name = "Wolf Menace";
        quest.description = "Eliminate the wolves threatening the village";
        quest.type = QuestType::SIDE_QUEST;
        
        quest.requirements.min_level = 5;
        
        QuestObjective obj;
        obj.objective_id = 1;
        obj.type = ObjectiveType::KILL;
        obj.description = "Kill Gray Wolves";
        obj.target_id = 3001;  // Monster ID
        obj.target_count = 10;
        quest.objectives.push_back(obj);
        
        quest.rewards.experience = 500;
        quest.rewards.gold = 100;
        quest.rewards.reputation = 50;
        
        return quest;
    }
    
    // [SEQUENCE: 1334] Collection quest
    static QuestDefinition CreateCollectionQuest() {
        QuestDefinition quest;
        quest.quest_id = 2002;
        quest.name = "Herb Gathering";
        quest.description = "Collect medicinal herbs for the healer";
        quest.type = QuestType::REPEATABLE;
        
        quest.is_repeatable = true;
        quest.cooldown_seconds = 3600;  // 1 hour
        
        QuestObjective obj;
        obj.objective_id = 1;
        obj.type = ObjectiveType::COLLECT;
        obj.description = "Collect Healing Herbs";
        obj.target_id = 4001;  // Item ID
        obj.target_count = 5;
        quest.objectives.push_back(obj);
        
        quest.rewards.experience = 200;
        quest.rewards.gold = 75;
        quest.rewards.choice_items.push_back({5001, 1});  // Health potion
        quest.rewards.choice_items.push_back({5002, 1});  // Mana potion
        
        return quest;
    }
    
    // [SEQUENCE: 1335] Chain quest
    static QuestDefinition CreateChainQuest(uint32_t quest_id, uint32_t chain_index) {
        QuestDefinition quest;
        quest.quest_id = quest_id;
        quest.name = "Investigation Part " + std::to_string(chain_index);
        quest.type = QuestType::CHAIN;
        
        if (chain_index > 1) {
            quest.requirements.required_quests.push_back(quest_id - 1);
        }
        
        if (chain_index < 5) {
            quest.next_quest_id = quest_id + 1;
        }
        
        // Different objectives for each part
        QuestObjective obj;
        obj.objective_id = 1;
        
        switch (chain_index) {
            case 1:
                obj.type = ObjectiveType::REACH_LOCATION;
                obj.description = "Investigate the mysterious cave";
                obj.target_x = 100.0f;
                obj.target_y = 200.0f;
                break;
            case 2:
                obj.type = ObjectiveType::COLLECT;
                obj.description = "Find evidence";
                obj.target_id = 6001;
                obj.target_count = 3;
                break;
            case 3:
                obj.type = ObjectiveType::KILL;
                obj.description = "Defeat the cultists";
                obj.target_id = 3005;
                obj.target_count = 5;
                break;
            case 4:
                obj.type = ObjectiveType::DELIVER;
                obj.description = "Report findings to the captain";
                obj.target_id = 1005;  // NPC ID
                break;
            case 5:
                obj.type = ObjectiveType::KILL;
                obj.description = "Defeat the cult leader";
                obj.target_id = 3010;  // Boss ID
                obj.target_count = 1;
                break;
        }
        
        quest.objectives.push_back(obj);
        
        // Increasing rewards
        quest.rewards.experience = 1000 * chain_index;
        quest.rewards.gold = 200 * chain_index;
        
        return quest;
    }
};

// [SEQUENCE: 1336] Quest events
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