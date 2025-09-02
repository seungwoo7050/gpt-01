#include "dynamic_quest.h"
#include "../core/logger.h"
#include "../world/world_manager.h"
#include "../npc/npc_manager.h"
#include "../item/item_manager.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

namespace mmorpg::quest {

// [SEQUENCE: 3673] Quest template validation
bool QuestTemplate::CanGenerate(const QuestGenerationParams& params) const {
    // Check level requirements
    if (params.player_level < min_level_ || params.player_level > max_level_) {
        return false;
    }
    
    // Check zone requirements
    if (!required_zones_.empty()) {
        bool zone_match = false;
        for (const auto& zone : required_zones_) {
            if (zone == params.current_zone) {
                zone_match = true;
                break;
            }
        }
        if (!zone_match) {
            return false;
        }
    }
    
    // Check if recently completed (cooldown)
    if (cooldown_hours_ > 0) {
        // TODO: Check player's quest history for cooldown
    }
    
    return true;
}

// [SEQUENCE: 3674] Quest generation engine constructor
QuestGenerationEngine::QuestGenerationEngine()
    : rng_(std::chrono::steady_clock::now().time_since_epoch().count()) {
}

// [SEQUENCE: 3675] Generate quest from parameters
GeneratedQuestPtr QuestGenerationEngine::GenerateQuest(
    const QuestGenerationParams& params) {
    
    // Select appropriate template
    auto& manager = DynamicQuestManager::Instance();
    auto template_quest = SelectTemplate(params);
    if (!template_quest) {
        spdlog::warn("[DynamicQuest] No valid template found for generation");
        return nullptr;
    }
    
    // Generate unique quest ID
    static std::atomic<uint32_t> quest_id_counter{100000};
    uint32_t quest_id = quest_id_counter.fetch_add(1);
    
    // Create quest instance
    auto quest = std::make_shared<GeneratedQuest>(quest_id, template_quest->GetId());
    quest->SetSeed(rng_());
    
    // Generate quest name
    std::string quest_name = GenerateQuestName(*template_quest, params);
    quest->SetName(quest_name);
    
    // Generate objectives
    auto objectives = GenerateObjectives(*template_quest, params);
    for (const auto& obj : objectives) {
        quest->AddObjective(obj);
    }
    
    // Generate description
    std::string description = GenerateQuestDescription(*template_quest, objectives);
    quest->SetDescription(description);
    
    // Calculate rewards
    float difficulty = CalculateDifficultyModifier(params);
    auto rewards = CalculateRewards(*template_quest, params, difficulty);
    quest->SetRewards(rewards);
    
    // Set quest properties
    quest->SetLevel(params.player_level);
    quest->SetType(QuestType::DYNAMIC);
    quest->SetTimeLimit(0);  // TODO: Add time limits based on template
    
    // Store generation context
    quest->SetDynamicData("generation_zone", params.current_zone);
    quest->SetDynamicData("difficulty_modifier", difficulty);
    quest->SetDynamicData("world_events", params.world_events);
    
    spdlog::info("[DynamicQuest] Generated quest: {} ({})", 
                quest_name, template_quest->GetId());
    
    return quest;
}

// [SEQUENCE: 3676] Generate multiple quests
std::vector<GeneratedQuestPtr> QuestGenerationEngine::GenerateMultipleQuests(
    const QuestGenerationParams& params, uint32_t count) {
    
    std::vector<GeneratedQuestPtr> quests;
    std::unordered_set<std::string> used_templates;
    
    for (uint32_t i = 0; i < count; ++i) {
        auto quest = GenerateQuest(params);
        if (quest) {
            // Avoid duplicate templates in same batch
            if (used_templates.find(quest->GetTemplateId()) == used_templates.end()) {
                quests.push_back(quest);
                used_templates.insert(quest->GetTemplateId());
            }
        }
    }
    
    return quests;
}

// [SEQUENCE: 3677] Template selection
QuestTemplatePtr QuestGenerationEngine::SelectTemplate(
    const QuestGenerationParams& params) {
    
    auto& manager = DynamicQuestManager::Instance();
    auto valid_templates = GetValidTemplates(params);
    
    if (valid_templates.empty()) {
        return nullptr;
    }
    
    // Weight-based selection
    std::vector<float> weights;
    float total_weight = 0.0f;
    
    for (const auto& tmpl : valid_templates) {
        float weight = 1.0f;
        
        // Adjust weight based on player preferences
        if (!params.preferred_type.empty()) {
            if (tmpl->GetName().find(params.preferred_type) != std::string::npos) {
                weight *= 2.0f;
            }
        }
        
        // Adjust weight based on world events
        for (const auto& [event, intensity] : params.world_events) {
            if (tmpl->GetName().find(event) != std::string::npos) {
                weight *= (1.0f + intensity);
            }
        }
        
        weights.push_back(weight);
        total_weight += weight;
    }
    
    // Select based on weighted random
    std::uniform_real_distribution<float> dist(0.0f, total_weight);
    float selection = dist(rng_);
    
    float cumulative = 0.0f;
    for (size_t i = 0; i < valid_templates.size(); ++i) {
        cumulative += weights[i];
        if (selection <= cumulative) {
            return valid_templates[i];
        }
    }
    
    return valid_templates.back();
}

// [SEQUENCE: 3678] Get valid templates
std::vector<QuestTemplatePtr> QuestGenerationEngine::GetValidTemplates(
    const QuestGenerationParams& params) {
    
    auto& manager = DynamicQuestManager::Instance();
    std::vector<QuestTemplatePtr> valid_templates;
    
    // Check all registered templates
    // Note: This would iterate through all templates in the manager
    // For now, returning empty vector as manager implementation is needed
    
    return valid_templates;
}

// [SEQUENCE: 3679] Generate objectives
std::vector<QuestObjective> QuestGenerationEngine::GenerateObjectives(
    const QuestTemplate& template_quest,
    const QuestGenerationParams& params) {
    
    std::vector<QuestObjective> objectives;
    
    for (const auto& obj_template : template_quest.GetObjectiveTemplates()) {
        QuestObjective objective;
        objective.type = obj_template.type;
        
        // Select target
        uint32_t target_id = SelectTarget(obj_template, params);
        objective.target_id = target_id;
        
        // Calculate count
        float difficulty = CalculateDifficultyModifier(params);
        uint32_t count = CalculateObjectiveCount(obj_template, difficulty);
        objective.required_count = count;
        
        // Generate description
        std::string desc = obj_template.description_template;
        // Replace placeholders
        size_t pos = desc.find("{target}");
        if (pos != std::string::npos) {
            desc.replace(pos, 8, std::to_string(target_id));
        }
        pos = desc.find("{count}");
        if (pos != std::string::npos) {
            desc.replace(pos, 7, std::to_string(count));
        }
        objective.description = desc;
        
        objectives.push_back(objective);
    }
    
    return objectives;
}

// [SEQUENCE: 3680] Calculate rewards
QuestRewards QuestGenerationEngine::CalculateRewards(
    const QuestTemplate& template_quest,
    const QuestGenerationParams& params,
    float difficulty_multiplier) {
    
    const auto& reward_template = template_quest.GetRewardTemplate();
    QuestRewards rewards;
    
    // Calculate experience
    float level_mult = std::pow(reward_template.level_scaling, params.player_level);
    float diff_mult = std::pow(reward_template.difficulty_scaling, difficulty_multiplier);
    rewards.experience = static_cast<uint32_t>(
        reward_template.base_experience * level_mult * diff_mult);
    
    // Calculate gold
    rewards.gold = static_cast<uint32_t>(
        reward_template.base_gold * level_mult * diff_mult);
    
    // Select item rewards
    for (const auto& item_reward : reward_template.possible_items) {
        if (params.player_level >= item_reward.min_level) {
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);
            if (dist(rng_) <= item_reward.drop_chance * difficulty_multiplier) {
                rewards.items.emplace_back(item_reward.item_id, item_reward.quantity);
            }
        }
    }
    
    // Set reputation
    if (reward_template.reputation_gain > 0) {
        rewards.reputation = static_cast<uint32_t>(
            reward_template.reputation_gain * difficulty_multiplier);
    }
    
    return rewards;
}

// [SEQUENCE: 3681] Generate quest name
std::string QuestGenerationEngine::GenerateQuestName(
    const QuestTemplate& template_quest,
    const QuestGenerationParams& params) {
    
    std::string base_name = template_quest.GetName();
    
    // Add context-specific modifiers
    if (!params.current_zone.empty()) {
        base_name = params.current_zone + " " + base_name;
    }
    
    // Add urgency for world events
    for (const auto& [event, intensity] : params.world_events) {
        if (intensity > 0.8f) {
            base_name = "Urgent: " + base_name;
            break;
        }
    }
    
    return base_name;
}

// [SEQUENCE: 3682] Generate quest description
std::string QuestGenerationEngine::GenerateQuestDescription(
    const QuestTemplate& template_quest,
    const std::vector<QuestObjective>& objectives) {
    
    std::string desc = template_quest.GetDescriptionTemplate();
    
    // Build objective list
    std::string obj_list;
    for (const auto& obj : objectives) {
        obj_list += "\n- " + obj.description;
    }
    
    // Replace placeholder
    size_t pos = desc.find("{objectives}");
    if (pos != std::string::npos) {
        desc.replace(pos, 12, obj_list);
    }
    
    return desc;
}

// [SEQUENCE: 3683] Helper - Select target
uint32_t QuestGenerationEngine::SelectTarget(
    const ObjectiveTemplate& objective,
    const QuestGenerationParams& params) {
    
    if (objective.possible_targets.empty()) {
        return 0;
    }
    
    // Filter targets by level and zone
    std::vector<uint32_t> valid_targets;
    for (uint32_t target : objective.possible_targets) {
        // TODO: Check if target is valid for player level and zone
        valid_targets.push_back(target);
    }
    
    if (valid_targets.empty()) {
        return objective.possible_targets[0];
    }
    
    // Random selection
    std::uniform_int_distribution<size_t> dist(0, valid_targets.size() - 1);
    return valid_targets[dist(rng_)];
}

// [SEQUENCE: 3684] Helper - Calculate objective count
uint32_t QuestGenerationEngine::CalculateObjectiveCount(
    const ObjectiveTemplate& objective,
    float difficulty_modifier) {
    
    uint32_t base_count = (objective.min_count + objective.max_count) / 2;
    uint32_t modified_count = static_cast<uint32_t>(
        base_count * difficulty_modifier * objective.difficulty_modifier);
    
    // Clamp to range
    return std::clamp(modified_count, objective.min_count, objective.max_count);
}

// [SEQUENCE: 3685] Helper - Calculate difficulty modifier
float QuestGenerationEngine::CalculateDifficultyModifier(
    const QuestGenerationParams& params) {
    
    float modifier = 1.0f;
    
    // Player progression factor
    modifier *= (1.0f + params.player_level * 0.01f);
    
    // World event intensity
    float max_intensity = 0.0f;
    for (const auto& [event, intensity] : params.world_events) {
        max_intensity = std::max(max_intensity, intensity);
    }
    modifier *= (1.0f + max_intensity * 0.5f);
    
    // Reputation bonus
    modifier *= (1.0f + params.reputation_level * 0.001f);
    
    return std::clamp(modifier, 0.5f, 3.0f);
}

// [SEQUENCE: 3686] Dynamic quest manager constructor
DynamicQuestManager::DynamicQuestManager()
    : generation_engine_(std::make_unique<QuestGenerationEngine>()) {
    
    spdlog::info("[DynamicQuest] Manager initialized");
}

// [SEQUENCE: 3687] Register quest template
void DynamicQuestManager::RegisterTemplate(QuestTemplatePtr template_quest) {
    if (!template_quest) {
        return;
    }
    
    std::unique_lock lock(mutex_);
    
    const std::string& id = template_quest->GetId();
    templates_[id] = template_quest;
    
    // Index by type
    QuestTemplateType type = template_quest->GetType();
    templates_by_type_[type].push_back(id);
    
    spdlog::debug("[DynamicQuest] Registered template: {} (type: {})",
                 id, static_cast<int>(type));
}

// [SEQUENCE: 3688] Get template by ID
QuestTemplatePtr DynamicQuestManager::GetTemplate(const std::string& template_id) {
    std::shared_lock lock(mutex_);
    auto it = templates_.find(template_id);
    return (it != templates_.end()) ? it->second : nullptr;
}

// [SEQUENCE: 3689] Generate quest for player
GeneratedQuestPtr DynamicQuestManager::GenerateQuestForPlayer(Player& player) {
    // Build generation parameters
    auto params = BuildGenerationParams(player);
    
    // Generate quest
    auto quest = generation_engine_->GenerateQuest(params);
    if (!quest) {
        return nullptr;
    }
    
    // Register quest
    {
        std::unique_lock lock(mutex_);
        active_quests_[quest->GetId()] = quest;
        player_generated_quests_[player.GetId()].push_back(quest->GetId());
    }
    
    // Record statistics
    RecordQuestGeneration(*quest);
    
    return quest;
}

// [SEQUENCE: 3690] Generate daily quests
std::vector<GeneratedQuestPtr> DynamicQuestManager::GenerateDailyQuests(
    Player& player, uint32_t count) {
    
    auto params = BuildGenerationParams(player);
    auto quests = generation_engine_->GenerateMultipleQuests(params, count);
    
    // Register all quests
    {
        std::unique_lock lock(mutex_);
        for (const auto& quest : quests) {
            active_quests_[quest->GetId()] = quest;
            player_generated_quests_[player.GetId()].push_back(quest->GetId());
            RecordQuestGeneration(*quest);
        }
    }
    
    spdlog::info("[DynamicQuest] Generated {} daily quests for player {}",
                quests.size(), player.GetId());
    
    return quests;
}

// [SEQUENCE: 3691] World event handlers
void DynamicQuestManager::OnWorldEvent(const std::string& event_type, float intensity) {
    std::unique_lock lock(mutex_);
    current_world_events_[event_type] = intensity;
    
    spdlog::info("[DynamicQuest] World event updated: {} (intensity: {})",
                event_type, intensity);
}

void DynamicQuestManager::OnMonsterKilled(uint32_t monster_id, const Vector3& position) {
    // Potentially trigger revenge or investigation quests
    // TODO: Implement monster-triggered quest generation
}

void DynamicQuestManager::OnItemDiscovered(uint32_t item_id, uint64_t player_id) {
    // Potentially trigger collection or delivery quests
    // TODO: Implement item-triggered quest generation
}

void DynamicQuestManager::OnZoneExplored(const std::string& zone_name, uint64_t player_id) {
    // Potentially trigger exploration quests
    // TODO: Implement exploration-triggered quest generation
}

// [SEQUENCE: 3692] Build generation parameters
QuestGenerationParams DynamicQuestManager::BuildGenerationParams(Player& player) {
    QuestGenerationParams params;
    
    params.player_level = player.GetLevel();
    params.player_position = player.GetPosition();
    params.completed_quests = player.GetCompletedQuests();
    params.active_quests = player.GetActiveQuests();
    params.reputation_level = player.GetReputation();
    
    // Get world context
    auto& world = WorldManager::Instance();
    params.time_of_day = world.GetTimeOfDay();
    params.current_zone = world.GetZoneName(params.player_position);
    
    // Copy world events
    {
        std::shared_lock lock(mutex_);
        params.world_events = current_world_events_;
    }
    
    // Get nearby entities
    params.nearby_npcs = world.GetNearbyNPCs(params.player_position, 100.0f);
    params.nearby_monsters = world.GetNearbyMonsters(params.player_position, 100.0f);
    
    return params;
}

// [SEQUENCE: 3693] Quest template builder implementation
QuestTemplateBuilder& QuestTemplateBuilder::Name(const std::string& name) {
    template_->SetName(name);
    return *this;
}

QuestTemplateBuilder& QuestTemplateBuilder::Description(const std::string& desc_template) {
    template_->SetDescriptionTemplate(desc_template);
    return *this;
}

QuestTemplateBuilder& QuestTemplateBuilder::LevelRange(uint32_t min_level, uint32_t max_level) {
    template_->SetMinLevel(min_level);
    template_->SetMaxLevel(max_level);
    return *this;
}

QuestTemplateBuilder& QuestTemplateBuilder::AddKillObjective(
    const std::vector<uint32_t>& monster_ids,
    uint32_t min_count, uint32_t max_count) {
    
    ObjectiveTemplate obj;
    obj.type = "kill";
    obj.description_template = "Defeat {count} {target}";
    obj.possible_targets = monster_ids;
    obj.min_count = min_count;
    obj.max_count = max_count;
    
    template_->AddObjectiveTemplate(obj);
    return *this;
}

QuestTemplateBuilder& QuestTemplateBuilder::BaseRewards(uint32_t exp, uint32_t gold) {
    RewardTemplate rewards = template_->GetRewardTemplate();
    rewards.base_experience = exp;
    rewards.base_gold = gold;
    template_->SetRewardTemplate(rewards);
    return *this;
}

QuestTemplatePtr QuestTemplateBuilder::Build() {
    return template_;
}

// [SEQUENCE: 3694] Predefined templates
namespace PredefinedTemplates {

QuestTemplatePtr CreateBountyHunterTemplate() {
    QuestTemplateBuilder builder("bounty_hunter", QuestTemplateType::KILL);
    
    return builder
        .Name("Bounty Hunter")
        .Description("A local authority has posted a bounty. {objectives}")
        .LevelRange(10, 50)
        .AddKillObjective({1001, 1002, 1003}, 5, 10)  // Bandit IDs
        .BaseRewards(500, 100)
        .Build();
}

QuestTemplatePtr CreateGatheringTemplate() {
    QuestTemplateBuilder builder("gathering", QuestTemplateType::COLLECT);
    
    return builder
        .Name("Resource Gathering")
        .Description("Help gather resources for the local craftsmen. {objectives}")
        .LevelRange(1, 30)
        .AddCollectObjective({2001, 2002, 2003}, 10, 20)  // Resource IDs
        .BaseRewards(200, 50)
        .Build();
}

QuestTemplatePtr CreateCourierTemplate() {
    QuestTemplateBuilder builder("courier", QuestTemplateType::DELIVERY);
    
    return builder
        .Name("Express Delivery")
        .Description("Deliver an important package. {objectives}")
        .LevelRange(5, 40)
        .AddDeliveryObjective(3001, 4001)  // Package ID, NPC ID
        .BaseRewards(300, 75)
        .Build();
}

} // namespace PredefinedTemplates

// [SEQUENCE: 3695] Quest generation utilities
namespace QuestGenerationUtils {

std::string GenerateQuestTitle(QuestTemplateType type,
                              const std::string& target_name,
                              const std::string& location_name) {
    switch (type) {
        case QuestTemplateType::KILL:
            return "Eliminate " + target_name;
        case QuestTemplateType::COLLECT:
            return "Gather " + target_name;
        case QuestTemplateType::DELIVERY:
            return "Deliver to " + location_name;
        case QuestTemplateType::ESCORT:
            return "Escort to " + location_name;
        case QuestTemplateType::EXPLORATION:
            return "Explore " + location_name;
        default:
            return "Quest in " + location_name;
    }
}

float CalculateQuestDifficulty(const GeneratedQuest& quest,
                              const Player& player) {
    float difficulty = 1.0f;
    
    // Level difference
    int level_diff = quest.GetLevel() - player.GetLevel();
    difficulty *= (1.0f + level_diff * 0.1f);
    
    // Objective count
    size_t obj_count = quest.GetObjectives().size();
    difficulty *= (1.0f + obj_count * 0.2f);
    
    // Time limit
    if (quest.GetTimeLimit() > 0) {
        difficulty *= 1.5f;
    }
    
    return std::clamp(difficulty, 0.1f, 10.0f);
}

} // namespace QuestGenerationUtils

} // namespace mmorpg::quest