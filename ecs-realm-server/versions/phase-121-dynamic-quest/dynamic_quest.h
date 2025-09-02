#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <random>
#include <chrono>
#include <optional>
#include "../core/types.h"
#include "../core/singleton.h"
#include "quest.h"
#include "../player/player.h"
#include "../world/world_state.h"

namespace mmorpg::quest {

// [SEQUENCE: 3656] Quest template types
enum class QuestTemplateType {
    KILL,           // 몬스터 처치
    COLLECT,        // 아이템 수집
    DELIVERY,       // 배달 임무
    ESCORT,         // 호위 임무
    EXPLORATION,    // 탐험 임무
    CRAFT,          // 제작 임무
    INTERACTION,    // NPC 상호작용
    SURVIVAL,       // 생존 임무
    PUZZLE,         // 퍼즐 해결
    COMPETITION     // 경쟁 임무
};

// [SEQUENCE: 3657] Quest generation parameters
struct QuestGenerationParams {
    uint32_t player_level;
    Vector3 player_position;
    std::vector<uint32_t> completed_quests;
    std::vector<uint32_t> active_quests;
    uint32_t reputation_level;
    std::string preferred_type;
    
    // World context
    float time_of_day;
    std::string current_zone;
    std::vector<uint32_t> nearby_npcs;
    std::vector<uint32_t> nearby_monsters;
    std::unordered_map<std::string, float> world_events;  // event_name -> intensity
};

// [SEQUENCE: 3658] Quest objective template
struct ObjectiveTemplate {
    std::string type;
    std::string description_template;
    
    // Target selection
    std::vector<uint32_t> possible_targets;
    uint32_t min_count{1};
    uint32_t max_count{10};
    
    // Requirements
    uint32_t min_level{1};
    uint32_t max_level{100};
    std::vector<std::string> required_zones;
    
    // Modifiers
    float difficulty_modifier{1.0f};
    float reward_modifier{1.0f};
    uint32_t time_limit{0};  // 0 = no limit
};

// [SEQUENCE: 3659] Quest reward template
struct RewardTemplate {
    // Base rewards
    uint32_t base_experience{100};
    uint32_t base_gold{10};
    
    // Scaling factors
    float level_scaling{1.1f};
    float difficulty_scaling{1.2f};
    float time_bonus_scaling{1.5f};
    
    // Item rewards
    struct ItemReward {
        uint32_t item_id;
        uint32_t quantity;
        float drop_chance;
        uint32_t min_level;
    };
    std::vector<ItemReward> possible_items;
    
    // Special rewards
    std::vector<uint32_t> skill_unlocks;
    std::vector<std::string> title_unlocks;
    uint32_t reputation_gain{0};
};

// [SEQUENCE: 3660] Quest template
class QuestTemplate {
public:
    QuestTemplate(const std::string& id, QuestTemplateType type)
        : id_(id), type_(type) {}
    
    // Template properties
    const std::string& GetId() const { return id_; }
    QuestTemplateType GetType() const { return type_; }
    
    void SetName(const std::string& name) { name_ = name; }
    const std::string& GetName() const { return name_; }
    
    void SetDescriptionTemplate(const std::string& desc) { description_template_ = desc; }
    const std::string& GetDescriptionTemplate() const { return description_template_; }
    
    // Objectives
    void AddObjectiveTemplate(const ObjectiveTemplate& objective) {
        objective_templates_.push_back(objective);
    }
    const std::vector<ObjectiveTemplate>& GetObjectiveTemplates() const {
        return objective_templates_;
    }
    
    // Rewards
    void SetRewardTemplate(const RewardTemplate& reward) { reward_template_ = reward; }
    const RewardTemplate& GetRewardTemplate() const { return reward_template_; }
    
    // Generation constraints
    void SetMinLevel(uint32_t level) { min_level_ = level; }
    void SetMaxLevel(uint32_t level) { max_level_ = level; }
    void SetRequiredZones(const std::vector<std::string>& zones) { required_zones_ = zones; }
    void SetCooldownHours(uint32_t hours) { cooldown_hours_ = hours; }
    
    // Validation
    bool CanGenerate(const QuestGenerationParams& params) const;
    
private:
    std::string id_;
    QuestTemplateType type_;
    std::string name_;
    std::string description_template_;
    
    std::vector<ObjectiveTemplate> objective_templates_;
    RewardTemplate reward_template_;
    
    uint32_t min_level_{1};
    uint32_t max_level_{100};
    std::vector<std::string> required_zones_;
    uint32_t cooldown_hours_{0};
    float generation_weight_{1.0f};
};

using QuestTemplatePtr = std::shared_ptr<QuestTemplate>;

// [SEQUENCE: 3661] Generated quest instance
class GeneratedQuest : public Quest {
public:
    GeneratedQuest(uint32_t id, const std::string& template_id)
        : Quest(id), template_id_(template_id) {
        generation_time_ = std::chrono::steady_clock::now();
    }
    
    // Generation data
    const std::string& GetTemplateId() const { return template_id_; }
    std::chrono::steady_clock::time_point GetGenerationTime() const { return generation_time_; }
    
    void SetSeed(uint64_t seed) { generation_seed_ = seed; }
    uint64_t GetSeed() const { return generation_seed_; }
    
    // Dynamic properties
    void SetDynamicData(const std::string& key, const std::any& value) {
        dynamic_data_[key] = value;
    }
    
    template<typename T>
    std::optional<T> GetDynamicData(const std::string& key) const {
        auto it = dynamic_data_.find(key);
        if (it != dynamic_data_.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (const std::bad_any_cast&) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }
    
private:
    std::string template_id_;
    uint64_t generation_seed_;
    std::chrono::steady_clock::time_point generation_time_;
    std::unordered_map<std::string, std::any> dynamic_data_;
};

using GeneratedQuestPtr = std::shared_ptr<GeneratedQuest>;

// [SEQUENCE: 3662] Quest generation engine
class QuestGenerationEngine {
public:
    QuestGenerationEngine();
    
    // [SEQUENCE: 3663] Quest generation
    GeneratedQuestPtr GenerateQuest(const QuestGenerationParams& params);
    std::vector<GeneratedQuestPtr> GenerateMultipleQuests(
        const QuestGenerationParams& params, uint32_t count);
    
    // Template selection
    QuestTemplatePtr SelectTemplate(const QuestGenerationParams& params);
    std::vector<QuestTemplatePtr> GetValidTemplates(const QuestGenerationParams& params);
    
    // Objective generation
    std::vector<QuestObjective> GenerateObjectives(
        const QuestTemplate& template_quest,
        const QuestGenerationParams& params);
    
    // Reward calculation
    QuestRewards CalculateRewards(
        const QuestTemplate& template_quest,
        const QuestGenerationParams& params,
        float difficulty_multiplier);
    
    // Content generation
    std::string GenerateQuestName(const QuestTemplate& template_quest,
                                 const QuestGenerationParams& params);
    std::string GenerateQuestDescription(const QuestTemplate& template_quest,
                                       const std::vector<QuestObjective>& objectives);
    
private:
    std::mt19937 rng_;
    
    // Helper methods
    uint32_t SelectTarget(const ObjectiveTemplate& objective,
                         const QuestGenerationParams& params);
    uint32_t CalculateObjectiveCount(const ObjectiveTemplate& objective,
                                   float difficulty_modifier);
    float CalculateDifficultyModifier(const QuestGenerationParams& params);
};

// [SEQUENCE: 3664] Dynamic quest manager
class DynamicQuestManager : public Singleton<DynamicQuestManager> {
public:
    // [SEQUENCE: 3665] Template management
    void RegisterTemplate(QuestTemplatePtr template_quest);
    QuestTemplatePtr GetTemplate(const std::string& template_id);
    void LoadTemplatesFromFile(const std::string& filename);
    
    // [SEQUENCE: 3666] Quest generation
    GeneratedQuestPtr GenerateQuestForPlayer(Player& player);
    std::vector<GeneratedQuestPtr> GenerateDailyQuests(Player& player, uint32_t count);
    GeneratedQuestPtr GenerateEventQuest(const std::string& event_type,
                                       const QuestGenerationParams& params);
    
    // [SEQUENCE: 3667] Quest management
    void OfferGeneratedQuest(Player& player, GeneratedQuestPtr quest);
    void RefreshPlayerQuests(Player& player);
    void CleanupExpiredQuests();
    
    // [SEQUENCE: 3668] World event integration
    void OnWorldEvent(const std::string& event_type, float intensity);
    void OnMonsterKilled(uint32_t monster_id, const Vector3& position);
    void OnItemDiscovered(uint32_t item_id, uint64_t player_id);
    void OnZoneExplored(const std::string& zone_name, uint64_t player_id);
    
    // [SEQUENCE: 3669] Quest chains
    struct QuestChain {
        std::string chain_id;
        std::vector<std::string> template_ids;
        uint32_t current_index{0};
        std::unordered_map<std::string, std::any> chain_data;
    };
    
    void StartQuestChain(Player& player, const std::string& chain_id);
    void ProgressQuestChain(Player& player, uint32_t completed_quest_id);
    
    // Statistics
    struct GenerationStats {
        uint64_t total_generated{0};
        uint64_t total_completed{0};
        std::unordered_map<QuestTemplateType, uint64_t> type_distribution;
        std::unordered_map<std::string, uint64_t> template_usage;
        float average_completion_time{0.0f};
        float average_difficulty{0.0f};
    };
    
    GenerationStats GetStats() const { return stats_; }
    
private:
    friend class Singleton<DynamicQuestManager>;
    DynamicQuestManager();
    
    // Templates
    std::unordered_map<std::string, QuestTemplatePtr> templates_;
    std::unordered_map<QuestTemplateType, std::vector<std::string>> templates_by_type_;
    
    // Active quests
    std::unordered_map<uint32_t, GeneratedQuestPtr> active_quests_;
    std::unordered_map<uint64_t, std::vector<uint32_t>> player_generated_quests_;
    
    // Quest chains
    std::unordered_map<uint64_t, QuestChain> active_chains_;
    
    // Generation engine
    std::unique_ptr<QuestGenerationEngine> generation_engine_;
    
    // World state
    std::unordered_map<std::string, float> current_world_events_;
    
    // Statistics
    GenerationStats stats_;
    mutable std::shared_mutex mutex_;
    
    // Helper methods
    QuestGenerationParams BuildGenerationParams(Player& player);
    void RecordQuestGeneration(const GeneratedQuest& quest);
    void RecordQuestCompletion(const GeneratedQuest& quest, float completion_time);
};

// [SEQUENCE: 3670] Quest template builder
class QuestTemplateBuilder {
public:
    QuestTemplateBuilder(const std::string& id, QuestTemplateType type) {
        template_ = std::make_shared<QuestTemplate>(id, type);
    }
    
    // Basic properties
    QuestTemplateBuilder& Name(const std::string& name);
    QuestTemplateBuilder& Description(const std::string& desc_template);
    QuestTemplateBuilder& LevelRange(uint32_t min_level, uint32_t max_level);
    QuestTemplateBuilder& RequiredZones(const std::vector<std::string>& zones);
    QuestTemplateBuilder& Cooldown(uint32_t hours);
    
    // Objectives
    QuestTemplateBuilder& AddKillObjective(const std::vector<uint32_t>& monster_ids,
                                         uint32_t min_count, uint32_t max_count);
    QuestTemplateBuilder& AddCollectObjective(const std::vector<uint32_t>& item_ids,
                                            uint32_t min_count, uint32_t max_count);
    QuestTemplateBuilder& AddDeliveryObjective(uint32_t item_id, uint32_t npc_id);
    QuestTemplateBuilder& AddInteractionObjective(const std::vector<uint32_t>& npc_ids);
    QuestTemplateBuilder& AddExplorationObjective(const std::vector<std::string>& locations);
    
    // Rewards
    QuestTemplateBuilder& BaseRewards(uint32_t exp, uint32_t gold);
    QuestTemplateBuilder& ScalingFactors(float level_scale, float difficulty_scale);
    QuestTemplateBuilder& AddItemReward(uint32_t item_id, uint32_t quantity,
                                      float drop_chance, uint32_t min_level = 1);
    QuestTemplateBuilder& AddReputationReward(uint32_t faction_id, uint32_t amount);
    
    // Build
    QuestTemplatePtr Build();
    
private:
    QuestTemplatePtr template_;
};

// [SEQUENCE: 3671] Quest generation utilities
namespace QuestGenerationUtils {
    // Name generation
    std::string GenerateQuestTitle(QuestTemplateType type,
                                 const std::string& target_name,
                                 const std::string& location_name);
    
    // Description generation
    std::string GenerateFlavorText(QuestTemplateType type,
                                 const std::string& npc_name,
                                 const std::string& reason);
    
    // Target selection
    std::vector<uint32_t> SelectMonsterTargets(uint32_t player_level,
                                              const std::string& zone,
                                              uint32_t count);
    std::vector<uint32_t> SelectItemTargets(uint32_t player_level,
                                          const std::string& item_type,
                                          uint32_t count);
    
    // Difficulty calculation
    float CalculateQuestDifficulty(const GeneratedQuest& quest,
                                 const Player& player);
    
    // Validation
    bool ValidateGeneratedQuest(const GeneratedQuest& quest,
                               const Player& player);
}

// [SEQUENCE: 3672] Predefined quest templates
namespace PredefinedTemplates {
    // Kill quests
    QuestTemplatePtr CreateBountyHunterTemplate();
    QuestTemplatePtr CreatePestControlTemplate();
    QuestTemplatePtr CreateMonsterSlayerTemplate();
    
    // Collection quests
    QuestTemplatePtr CreateGatheringTemplate();
    QuestTemplatePtr CreateScavengerTemplate();
    QuestTemplatePtr CreateRareCollectorTemplate();
    
    // Delivery quests
    QuestTemplatePtr CreateCourierTemplate();
    QuestTemplatePtr CreateMerchantDeliveryTemplate();
    QuestTemplatePtr CreateUrgentDeliveryTemplate();
    
    // Exploration quests
    QuestTemplatePtr CreateScoutingTemplate();
    QuestTemplatePtr CreateCartographerTemplate();
    QuestTemplatePtr CreateTreasureHuntTemplate();
    
    // Event-based templates
    QuestTemplatePtr CreateInvasionDefenseTemplate();
    QuestTemplatePtr CreateSeasonalEventTemplate(const std::string& season);
    QuestTemplatePtr CreateWorldBossTemplate();
    
    // Chain quest templates
    std::vector<QuestTemplatePtr> CreateStoryChainTemplates(
        const std::string& story_arc);
}

} // namespace mmorpg::quest