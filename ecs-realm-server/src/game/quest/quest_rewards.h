#pragma once

#include "quest_system.h"
#include "../items/item_system.h"
#include "../stats/character_stats.h"
#include <memory>
#include <vector>
#include <random>

namespace mmorpg::game::quest {

// [SEQUENCE: MVP7-338] Quest reward system

// [SEQUENCE: MVP7-339] Reward calculation modifiers
struct RewardModifiers {
    float experience_multiplier = 1.0f;
    float gold_multiplier = 1.0f;
    float reputation_multiplier = 1.0f;
    float item_drop_bonus = 0.0f;
    bool double_rewards = false;
    bool guaranteed_rare_item = false;
    int32_t level_difference = 0;
    float GetExperienceModifier() const;
    float GetGoldModifier() const;
};

// [SEQUENCE: MVP7-340] Reward calculator
class RewardCalculator {
public:
    // [SEQUENCE: MVP7-341] Calculate base rewards
    static QuestReward CalculateBaseRewards(const QuestDefinition& quest, uint32_t player_level);
    
    // [SEQUENCE: MVP7-342] Apply modifiers
    static QuestReward ApplyModifiers(const QuestReward& base_rewards, const RewardModifiers& modifiers);
    
    // [SEQUENCE: MVP7-343] Calculate dynamic rewards
    static QuestReward CalculateDynamicRewards(const QuestDefinition& quest, const QuestProgress& progress, uint32_t player_level);
};

// [SEQUENCE: MVP7-344] Reward validator
class RewardValidator {
public:
    // [SEQUENCE: MVP7-345] Validate reward eligibility
    static bool CanReceiveRewards(uint64_t entity_id, const QuestProgress& progress);
    
    // [SEQUENCE: MVP7-346] Validate item rewards
    static bool ValidateItemRewards(uint64_t entity_id, const QuestReward& rewards);
};

// [SEQUENCE: MVP7-347] Reward distributor
class RewardDistributor {
private:
    static thread_local std::mt19937 rng_;
public:
    // [SEQUENCE: MVP7-348] Grant rewards to player
    static bool GrantRewards(uint64_t entity_id, const QuestReward& rewards, uint32_t chosen_item_index = 0);
private:
    // [SEQUENCE: MVP7-349] Grant experience
    static void GrantExperience(uint64_t entity_id, uint64_t amount);
    // [SEQUENCE: MVP7-350] Grant currency
    static void GrantCurrency(uint64_t entity_id, const std::string& currency, uint64_t amount);
    // [SEQUENCE: MVP7-351] Grant item
    static void GrantItem(uint64_t entity_id, uint32_t item_id, uint32_t count);
    // [SEQUENCE: MVP7-352] Grant reputation
    static void GrantReputation(uint64_t entity_id, uint64_t amount);
    // [SEQUENCE: MVP7-353] Grant skill
    static void GrantSkill(uint64_t entity_id, uint32_t skill_id);
    // [SEQUENCE: MVP7-354] Grant title
    static void GrantTitle(uint64_t entity_id, uint32_t title_id);
};

// [SEQUENCE: MVP7-355] Reward manager
class RewardManager {
public:
    // [SEQUENCE: MVP7-356] Process quest completion
    bool ProcessQuestCompletion(uint64_t entity_id, uint32_t quest_id, const RewardModifiers& modifiers = {}, uint32_t chosen_item_index = 0);
    
    // [SEQUENCE: MVP7-357] Preview rewards
    QuestReward PreviewRewards(uint64_t entity_id, uint32_t quest_id, const RewardModifiers& modifiers = {}) const;
    
    // [SEQUENCE: MVP7-358] Set reward modifiers (e.g., for events)
    void SetGlobalModifiers(const RewardModifiers& modifiers);
    const RewardModifiers& GetGlobalModifiers() const;
private:
    RewardModifiers global_modifiers_;
};

} // namespace mmorpg::game::quest