#include "character_stats.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

namespace mmorpg::game::stats {

// [SEQUENCE: 1175] CharacterStats - Initialize base stats
void CharacterStats::InitializeBaseStats() {
    // Set base stats from class configuration
    base_strength_ = class_config_.base_strength;
    base_agility_ = class_config_.base_agility;
    base_intelligence_ = class_config_.base_intelligence;
    base_vitality_ = class_config_.base_vitality;
    base_dexterity_ = class_config_.base_dexterity;
    base_wisdom_ = class_config_.base_wisdom;
    
    // Initialize level data
    level_data_.level = 1;
    level_data_.current_experience = 0;
    level_data_.experience_to_next = ExperienceTable::GetExperienceForLevel(1);
    level_data_.available_stat_points = 0;
    level_data_.available_skill_points = 1;
    
    spdlog::debug("Initialized character {} stats with class {}", entity_id_, class_id_);
}

// [SEQUENCE: 1176] CharacterStats - Allocate stat point
bool CharacterStats::AllocateStatPoint(PrimaryAttribute attribute) {
    if (level_data_.available_stat_points <= 0) {
        spdlog::warn("Character {} has no available stat points", entity_id_);
        return false;
    }
    
    // Allocate point based on attribute
    switch (attribute) {
        case PrimaryAttribute::STRENGTH:
            allocated_strength_++;
            break;
        case PrimaryAttribute::AGILITY:
            allocated_agility_++;
            break;
        case PrimaryAttribute::INTELLIGENCE:
            allocated_intelligence_++;
            break;
        case PrimaryAttribute::VITALITY:
            allocated_vitality_++;
            break;
        case PrimaryAttribute::DEXTERITY:
            allocated_dexterity_++;
            break;
        case PrimaryAttribute::WISDOM:
            allocated_wisdom_++;
            break;
    }
    
    level_data_.available_stat_points--;
    
    spdlog::debug("Character {} allocated stat point to attribute {}", 
                  entity_id_, static_cast<int>(attribute));
    
    return true;
}

// [SEQUENCE: 1177] CharacterStats - Allocate multiple stat points
bool CharacterStats::AllocateStatPoints(const std::unordered_map<PrimaryAttribute, uint32_t>& allocation) {
    // Calculate total points needed
    uint32_t total_points = 0;
    for (const auto& [attr, points] : allocation) {
        total_points += points;
    }
    
    if (total_points > level_data_.available_stat_points) {
        spdlog::warn("Character {} trying to allocate {} points but only has {}", 
                     entity_id_, total_points, level_data_.available_stat_points);
        return false;
    }
    
    // Allocate all points
    for (const auto& [attr, points] : allocation) {
        for (uint32_t i = 0; i < points; ++i) {
            AllocateStatPoint(attr);
        }
    }
    
    return true;
}

// [SEQUENCE: 1178] CharacterStats - Reset stat points
void CharacterStats::ResetStatPoints() {
    // Return all allocated points
    level_data_.available_stat_points += allocated_strength_;
    level_data_.available_stat_points += allocated_agility_;
    level_data_.available_stat_points += allocated_intelligence_;
    level_data_.available_stat_points += allocated_vitality_;
    level_data_.available_stat_points += allocated_dexterity_;
    level_data_.available_stat_points += allocated_wisdom_;
    
    // Reset allocations
    allocated_strength_ = 0;
    allocated_agility_ = 0;
    allocated_intelligence_ = 0;
    allocated_vitality_ = 0;
    allocated_dexterity_ = 0;
    allocated_wisdom_ = 0;
    
    spdlog::info("Character {} reset all stat points", entity_id_);
}

// [SEQUENCE: 1179] CharacterStats - Add experience
void CharacterStats::AddExperience(uint64_t amount) {
    uint32_t old_level = level_data_.level;
    level_data_.current_experience += amount;
    
    // Check for level up
    while (level_data_.current_experience >= level_data_.experience_to_next && 
           level_data_.level < ExperienceTable::MAX_LEVEL) {
        
        level_data_.current_experience -= level_data_.experience_to_next;
        level_data_.level++;
        level_data_.experience_to_next = ExperienceTable::GetExperienceForLevel(level_data_.level);
    }
    
    // Process level ups
    if (level_data_.level > old_level) {
        ProcessLevelUp(old_level, level_data_.level);
    }
    
    spdlog::debug("Character {} gained {} experience (level {} -> {})", 
                  entity_id_, amount, old_level, level_data_.level);
}

// [SEQUENCE: 1180] CharacterStats - Set level
bool CharacterStats::SetLevel(uint32_t new_level) {
    if (new_level < 1 || new_level > ExperienceTable::MAX_LEVEL) {
        return false;
    }
    
    uint32_t old_level = level_data_.level;
    level_data_.level = new_level;
    
    // Calculate experience for new level
    level_data_.current_experience = 0;
    level_data_.experience_to_next = ExperienceTable::GetExperienceForLevel(new_level);
    
    // Process level change
    if (new_level != old_level) {
        ProcessLevelUp(old_level, new_level);
    }
    
    return true;
}

// [SEQUENCE: 1181] CharacterStats - Process level up
void CharacterStats::ProcessLevelUp(uint32_t old_level, uint32_t new_level) {
    int32_t level_diff = new_level - old_level;
    
    if (level_diff > 0) {
        // Level up - gain stat and skill points
        level_data_.available_stat_points += class_config_.stat_points_per_level * level_diff;
        level_data_.available_skill_points += class_config_.skill_points_per_level * level_diff;
        
        // Apply per-level stat growth
        base_strength_ += class_config_.strength_per_level * level_diff;
        base_agility_ += class_config_.agility_per_level * level_diff;
        base_intelligence_ += class_config_.intelligence_per_level * level_diff;
        base_vitality_ += class_config_.vitality_per_level * level_diff;
        base_dexterity_ += class_config_.dexterity_per_level * level_diff;
        base_wisdom_ += class_config_.wisdom_per_level * level_diff;
        
        spdlog::info("Character {} leveled up from {} to {}! Gained {} stat points and {} skill points",
                     entity_id_, old_level, new_level, 
                     class_config_.stat_points_per_level * level_diff,
                     class_config_.skill_points_per_level * level_diff);
    } else {
        // Level down - remove stat growth (but keep allocated points)
        base_strength_ += class_config_.strength_per_level * level_diff;  // level_diff is negative
        base_agility_ += class_config_.agility_per_level * level_diff;
        base_intelligence_ += class_config_.intelligence_per_level * level_diff;
        base_vitality_ += class_config_.vitality_per_level * level_diff;
        base_dexterity_ += class_config_.dexterity_per_level * level_diff;
        base_wisdom_ += class_config_.wisdom_per_level * level_diff;
        
        spdlog::info("Character {} level decreased from {} to {}", 
                     entity_id_, old_level, new_level);
    }
}

// [SEQUENCE: 1182] CharacterStats - Calculate secondary stats
SecondaryStats CharacterStats::CalculateSecondaryStats() const {
    SecondaryStats stats;
    
    // Get total primary attributes
    int32_t total_str = GetStrength();
    int32_t total_agi = GetAgility();
    int32_t total_int = GetIntelligence();
    int32_t total_vit = GetVitality();
    int32_t total_dex = GetDexterity();
    int32_t total_wis = GetWisdom();
    
    // Health and resources
    stats.max_health = StatsFormulas::CalculateMaxHealth(total_vit, level_data_.level);
    stats.max_mana = StatsFormulas::CalculateMaxMana(total_int, total_wis, level_data_.level);
    stats.max_stamina = 100 + (total_vit * 5);
    
    // Combat stats
    stats.attack_power = StatsFormulas::CalculateAttackPower(total_str, total_dex, level_data_.level);
    stats.spell_power = StatsFormulas::CalculateSpellPower(total_int, level_data_.level);
    stats.armor = StatsFormulas::CalculateArmor(total_agi, total_vit);
    stats.magic_resist = total_wis * 2;
    
    // Speed and rates
    stats.attack_speed = StatsFormulas::CalculateAttackSpeed(total_agi, total_dex);
    stats.cast_speed = 1.0f + (total_int * 0.002f);
    
    // Chances
    stats.critical_chance = StatsFormulas::CalculateCriticalChance(total_dex, total_agi);
    stats.dodge_chance = StatsFormulas::CalculateDodgeChance(total_agi);
    stats.hit_chance = 0.95f + (total_dex * 0.001f);  // Cap at 99%
    stats.hit_chance = std::min(stats.hit_chance, 0.99f);
    
    // Regeneration
    stats.health_regen = StatsFormulas::CalculateHealthRegen(total_vit, level_data_.level);
    stats.mana_regen = StatsFormulas::CalculateManaRegen(total_wis, total_int, level_data_.level);
    stats.stamina_regen = 5.0f + (total_vit * 0.2f);
    
    // Movement
    stats.movement_speed = 5.0f + (total_agi * 0.02f);
    stats.jump_height = 2.0f + (total_agi * 0.01f);
    
    // Class-specific bonuses
    if (class_id_ == 1) {  // Warrior
        stats.block_chance = 0.1f + (total_str * 0.001f);
        stats.parry_chance = 0.05f + (total_dex * 0.0005f);
    } else if (class_id_ == 2) {  // Mage
        stats.spell_crit_chance = 0.05f + (total_int * 0.002f);
    } else if (class_id_ == 3) {  // Rogue
        stats.critical_damage = 1.5f + (total_dex * 0.01f);
    }
    
    return stats;
}

// [SEQUENCE: 1183] CharacterStats - Add stat modifier
void CharacterStats::AddStatModifier(const std::string& source, PrimaryAttribute attr, int32_t value) {
    StatModifier modifier{attr, value};
    stat_modifiers_[source].push_back(modifier);
    
    RecalculateBonuses();
    
    spdlog::debug("Added stat modifier from {} to character {}: {} +{}", 
                  source, entity_id_, static_cast<int>(attr), value);
}

// [SEQUENCE: 1184] CharacterStats - Remove stat modifier
void CharacterStats::RemoveStatModifier(const std::string& source) {
    stat_modifiers_.erase(source);
    RecalculateBonuses();
    
    spdlog::debug("Removed stat modifiers from {} for character {}", source, entity_id_);
}

// [SEQUENCE: 1185] CharacterStats - Clear all modifiers
void CharacterStats::ClearAllModifiers() {
    stat_modifiers_.clear();
    RecalculateBonuses();
    
    spdlog::debug("Cleared all stat modifiers for character {}", entity_id_);
}

// [SEQUENCE: 1186] CharacterStats - Recalculate bonuses
void CharacterStats::RecalculateBonuses() {
    // Reset all bonuses
    bonus_strength_ = 0;
    bonus_agility_ = 0;
    bonus_intelligence_ = 0;
    bonus_vitality_ = 0;
    bonus_dexterity_ = 0;
    bonus_wisdom_ = 0;
    
    // Apply all modifiers
    for (const auto& [source, modifiers] : stat_modifiers_) {
        for (const auto& modifier : modifiers) {
            switch (modifier.attribute) {
                case PrimaryAttribute::STRENGTH:
                    bonus_strength_ += modifier.value;
                    break;
                case PrimaryAttribute::AGILITY:
                    bonus_agility_ += modifier.value;
                    break;
                case PrimaryAttribute::INTELLIGENCE:
                    bonus_intelligence_ += modifier.value;
                    break;
                case PrimaryAttribute::VITALITY:
                    bonus_vitality_ += modifier.value;
                    break;
                case PrimaryAttribute::DEXTERITY:
                    bonus_dexterity_ += modifier.value;
                    break;
                case PrimaryAttribute::WISDOM:
                    bonus_wisdom_ += modifier.value;
                    break;
            }
        }
    }
}

} // namespace mmorpg::game::stats