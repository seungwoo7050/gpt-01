#pragma once

#include "quest_system.h"
#include "../items/item_system.h"
#include "../stats/character_stats.h"
#include <memory>
#include <vector>
#include <random>

namespace mmorpg::game::quest {

// [SEQUENCE: 1383] Quest reward system
// 퀘스트 보상 계산, 검증, 지급을 관리하는 시스템

// [SEQUENCE: 1384] Reward calculation modifiers
struct RewardModifiers {
    float experience_multiplier = 1.0f;
    float gold_multiplier = 1.0f;
    float reputation_multiplier = 1.0f;
    float item_drop_bonus = 0.0f;      // 0.0 - 1.0
    
    // Special modifiers
    bool double_rewards = false;        // Event bonus
    bool guaranteed_rare_item = false;  // Special promotion
    
    // Level-based scaling
    int32_t level_difference = 0;      // Player level - Quest level
    
    float GetExperienceModifier() const {
        float modifier = experience_multiplier;
        
        // Reduce XP for overleveled players
        if (level_difference > 5) {
            modifier *= std::max(0.1f, 1.0f - (level_difference - 5) * 0.1f);
        }
        
        if (double_rewards) {
            modifier *= 2.0f;
        }
        
        return modifier;
    }
    
    float GetGoldModifier() const {
        float modifier = gold_multiplier;
        
        if (double_rewards) {
            modifier *= 2.0f;
        }
        
        return modifier;
    }
};

// [SEQUENCE: 1385] Reward calculator
class RewardCalculator {
public:
    // [SEQUENCE: 1386] Calculate base rewards
    static QuestReward CalculateBaseRewards(const QuestDefinition& quest, 
                                           uint32_t player_level) {
        QuestReward rewards = quest.rewards;  // Copy base rewards
        
        // Scale experience based on quest type
        float xp_scale = 1.0f;
        switch (quest.type) {
            case QuestType::MAIN_STORY:
                xp_scale = 1.5f;
                break;
            case QuestType::SIDE_QUEST:
                xp_scale = 1.0f;
                break;
            case QuestType::DAILY:
                xp_scale = 0.8f;
                break;
            case QuestType::WEEKLY:
                xp_scale = 2.0f;
                break;
            case QuestType::HIDDEN:
                xp_scale = 1.2f;
                break;
        }
        
        rewards.experience = static_cast<uint64_t>(rewards.experience * xp_scale);
        
        return rewards;
    }
    
    // [SEQUENCE: 1387] Apply modifiers
    static QuestReward ApplyModifiers(const QuestReward& base_rewards,
                                     const RewardModifiers& modifiers) {
        QuestReward modified = base_rewards;
        
        // Apply experience modifier
        modified.experience = static_cast<uint64_t>(
            base_rewards.experience * modifiers.GetExperienceModifier()
        );
        
        // Apply gold modifier
        modified.gold = static_cast<uint64_t>(
            base_rewards.gold * modifiers.GetGoldModifier()
        );
        
        // Apply reputation modifier
        modified.reputation = static_cast<uint64_t>(
            base_rewards.reputation * modifiers.reputation_multiplier
        );
        
        // Apply item drop bonus
        if (modifiers.item_drop_bonus > 0.0f) {
            // Increase chance for random items
            for (auto& item : modified.random_items) {
                item.chance = std::min(1.0f, item.chance + modifiers.item_drop_bonus);
            }
        }
        
        // Guaranteed rare item modifier
        if (modifiers.guaranteed_rare_item && !modified.random_items.empty()) {
            // Find highest value item and make it guaranteed
            auto best_item = std::max_element(
                modified.random_items.begin(),
                modified.random_items.end(),
                [](const auto& a, const auto& b) {
                    return a.item_id < b.item_id;  // TODO: Compare by rarity
                }
            );
            
            if (best_item != modified.random_items.end()) {
                best_item->chance = 1.0f;
            }
        }
        
        return modified;
    }
    
    // [SEQUENCE: 1388] Calculate dynamic rewards
    static QuestReward CalculateDynamicRewards(const QuestDefinition& quest,
                                              const QuestProgress& progress,
                                              uint32_t player_level) {
        QuestReward rewards = CalculateBaseRewards(quest, player_level);
        
        // Bonus for optional objectives
        int optional_completed = 0;
        int total_optional = 0;
        
        for (const auto& obj : progress.objectives) {
            if (obj.is_optional) {
                total_optional++;
                if (obj.IsComplete()) {
                    optional_completed++;
                }
            }
        }
        
        if (total_optional > 0 && optional_completed > 0) {
            float bonus_multiplier = 1.0f + (0.1f * optional_completed);
            rewards.experience = static_cast<uint64_t>(rewards.experience * bonus_multiplier);
            rewards.gold = static_cast<uint64_t>(rewards.gold * bonus_multiplier);
        }
        
        // Time-based bonus (for timed quests)
        if (quest.time_limit_seconds > 0) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                progress.complete_time - progress.start_time
            ).count();
            
            if (elapsed < quest.time_limit_seconds / 2) {
                // Completed in half the time - 20% bonus
                rewards.experience = static_cast<uint64_t>(rewards.experience * 1.2f);
            }
        }
        
        return rewards;
    }
};

// [SEQUENCE: 1389] Reward validator
class RewardValidator {
public:
    // [SEQUENCE: 1390] Validate reward eligibility
    static bool CanReceiveRewards(uint64_t entity_id, const QuestProgress& progress) {
        // Must be completed
        if (progress.state != QuestState::COMPLETED) {
            return false;
        }
        
        // Check if already rewarded
        if (progress.state == QuestState::REWARDED) {
            return false;
        }
        
        // TODO: Check inventory space
        // TODO: Check currency caps
        
        return true;
    }
    
    // [SEQUENCE: 1391] Validate item rewards
    static bool ValidateItemRewards(uint64_t entity_id, const QuestReward& rewards) {
        // TODO: Get player inventory
        // items::InventoryManager* inventory = GetPlayerInventory(entity_id);
        
        // Count total items
        size_t total_items = rewards.guaranteed_items.size();
        if (!rewards.choice_items.empty()) {
            total_items++;  // Player chooses one
        }
        
        // Estimate random items (worst case)
        total_items += rewards.random_items.size();
        
        // TODO: Check inventory space
        // return inventory->GetFreeSlots() >= total_items;
        
        return true;  // Placeholder
    }
};

// [SEQUENCE: 1392] Reward distributor
class RewardDistributor {
private:
    static thread_local std::mt19937 rng_;
    
public:
    // [SEQUENCE: 1393] Grant rewards to player
    static bool GrantRewards(uint64_t entity_id, const QuestReward& rewards,
                           uint32_t chosen_item_index = 0) {
        spdlog::info("Granting quest rewards to entity {}", entity_id);
        
        // Grant experience
        if (rewards.experience > 0) {
            GrantExperience(entity_id, rewards.experience);
        }
        
        // Grant gold
        if (rewards.gold > 0) {
            GrantCurrency(entity_id, "gold", rewards.gold);
        }
        
        // Grant reputation
        if (rewards.reputation > 0) {
            GrantReputation(entity_id, rewards.reputation);
        }
        
        // Grant guaranteed items
        for (const auto& item : rewards.guaranteed_items) {
            GrantItem(entity_id, item.item_id, item.count);
        }
        
        // Grant chosen item
        if (chosen_item_index < rewards.choice_items.size()) {
            const auto& chosen = rewards.choice_items[chosen_item_index];
            GrantItem(entity_id, chosen.item_id, chosen.count);
        }
        
        // Roll for random items
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        for (const auto& item : rewards.random_items) {
            if (dist(rng_) <= item.chance) {
                GrantItem(entity_id, item.item_id, item.count);
                spdlog::debug("Entity {} received random item {} ({}% chance)", 
                            entity_id, item.item_id, item.chance * 100);
            }
        }
        
        // Grant skills
        for (uint32_t skill_id : rewards.skill_ids) {
            GrantSkill(entity_id, skill_id);
        }
        
        // Grant titles
        for (uint32_t title_id : rewards.title_ids) {
            GrantTitle(entity_id, title_id);
        }
        
        // Unlock follow-up quests
        for (uint32_t quest_id : rewards.unlock_quest_ids) {
            // This is handled by quest chain system
            spdlog::debug("Unlocked quest {} for entity {}", quest_id, entity_id);
        }
        
        return true;
    }
    
private:
    // [SEQUENCE: 1394] Grant experience
    static void GrantExperience(uint64_t entity_id, uint64_t amount) {
        // TODO: Get character stats component
        // auto stats = GetCharacterStats(entity_id);
        // stats->AddExperience(amount);
        spdlog::debug("Granted {} XP to entity {}", amount, entity_id);
    }
    
    // [SEQUENCE: 1395] Grant currency
    static void GrantCurrency(uint64_t entity_id, const std::string& currency, uint64_t amount) {
        // TODO: Get currency component
        // auto wallet = GetWallet(entity_id);
        // wallet->AddCurrency(currency, amount);
        spdlog::debug("Granted {} {} to entity {}", amount, currency, entity_id);
    }
    
    // [SEQUENCE: 1396] Grant item
    static void GrantItem(uint64_t entity_id, uint32_t item_id, uint32_t count) {
        // TODO: Get inventory
        // auto inventory = GetInventory(entity_id);
        // auto item = ItemManager::Instance().CreateItemInstance(item_id, count);
        // inventory->AddItem(item);
        spdlog::debug("Granted {}x item {} to entity {}", count, item_id, entity_id);
    }
    
    // [SEQUENCE: 1397] Grant reputation
    static void GrantReputation(uint64_t entity_id, uint64_t amount) {
        // TODO: Get reputation component
        spdlog::debug("Granted {} reputation to entity {}", amount, entity_id);
    }
    
    // [SEQUENCE: 1398] Grant skill
    static void GrantSkill(uint64_t entity_id, uint32_t skill_id) {
        // TODO: Get skill component
        spdlog::debug("Granted skill {} to entity {}", skill_id, entity_id);
    }
    
    // [SEQUENCE: 1399] Grant title
    static void GrantTitle(uint64_t entity_id, uint32_t title_id) {
        // TODO: Get title component
        spdlog::debug("Granted title {} to entity {}", title_id, entity_id);
    }
};

thread_local std::mt19937 RewardDistributor::rng_(
    std::chrono::steady_clock::now().time_since_epoch().count()
);

// [SEQUENCE: 1400] Reward manager
class RewardManager {
public:
    // [SEQUENCE: 1401] Process quest completion
    bool ProcessQuestCompletion(uint64_t entity_id, uint32_t quest_id,
                              const RewardModifiers& modifiers = {},
                              uint32_t chosen_item_index = 0) {
        // Get quest definition
        const auto* quest = QuestManager::Instance().GetQuest(quest_id);
        if (!quest) {
            return false;
        }
        
        // Get quest progress
        auto quest_log = QuestManager::Instance().GetQuestLog(entity_id);
        if (!quest_log) {
            return false;
        }
        
        const auto* progress = quest_log->GetQuestProgress(quest_id);
        if (!progress) {
            return false;
        }
        
        // Validate eligibility
        if (!RewardValidator::CanReceiveRewards(entity_id, *progress)) {
            spdlog::warn("Entity {} not eligible for quest {} rewards", 
                        entity_id, quest_id);
            return false;
        }
        
        // TODO: Get player level
        uint32_t player_level = 1;  // Placeholder
        
        // Calculate rewards
        QuestReward final_rewards = RewardCalculator::CalculateDynamicRewards(
            *quest, *progress, player_level
        );
        
        // Apply modifiers
        final_rewards = RewardCalculator::ApplyModifiers(final_rewards, modifiers);
        
        // Validate item rewards
        if (!RewardValidator::ValidateItemRewards(entity_id, final_rewards)) {
            spdlog::warn("Entity {} insufficient inventory space for quest {} rewards", 
                        entity_id, quest_id);
            return false;
        }
        
        // Grant rewards
        if (RewardDistributor::GrantRewards(entity_id, final_rewards, chosen_item_index)) {
            // Mark quest as rewarded
            // TODO: Update quest state to REWARDED
            
            spdlog::info("Entity {} received rewards for quest {}: {}", 
                        entity_id, quest_id, quest->name);
            
            return true;
        }
        
        return false;
    }
    
    // [SEQUENCE: 1402] Preview rewards
    QuestReward PreviewRewards(uint64_t entity_id, uint32_t quest_id,
                              const RewardModifiers& modifiers = {}) const {
        const auto* quest = QuestManager::Instance().GetQuest(quest_id);
        if (!quest) {
            return {};
        }
        
        // TODO: Get player level
        uint32_t player_level = 1;
        
        QuestReward preview = RewardCalculator::CalculateBaseRewards(*quest, player_level);
        preview = RewardCalculator::ApplyModifiers(preview, modifiers);
        
        return preview;
    }
    
    // [SEQUENCE: 1403] Set reward modifiers (e.g., for events)
    void SetGlobalModifiers(const RewardModifiers& modifiers) {
        global_modifiers_ = modifiers;
    }
    
    const RewardModifiers& GetGlobalModifiers() const {
        return global_modifiers_;
    }
    
private:
    RewardModifiers global_modifiers_;
};

} // namespace mmorpg::game::quest