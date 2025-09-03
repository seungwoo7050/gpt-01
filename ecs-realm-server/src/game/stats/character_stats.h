#pragma once

#include <cmath>
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <functional>

namespace mmorpg::game::stats {

// [SEQUENCE: 1153] Character stats and leveling system
// 캐릭터의 능력치, 레벨업, 스탯 포인트 분배를 관리하는 시스템

// [SEQUENCE: 1154] Primary attributes
enum class PrimaryAttribute {
    STRENGTH,       // 물리 공격력, 방어력 증가
    AGILITY,        // 공격 속도, 회피율 증가
    INTELLIGENCE,   // 마법 공격력, 마나 증가
    VITALITY,       // 체력, 체력 재생 증가
    DEXTERITY,      // 명중률, 치명타율 증가
    WISDOM          // 마나 재생, 마법 저항 증가
};

// [SEQUENCE: 1155] Secondary stats (derived from primary)
struct SecondaryStats {
    // Health & Resources
    int32_t max_health = 100;
    int32_t max_mana = 50;
    int32_t max_stamina = 100;
    
    // Combat - Physical
    int32_t attack_power = 0;
    int32_t armor = 0;
    float attack_speed = 1.0f;      // Attacks per second
    float hit_chance = 0.95f;       // 0.0 - 1.0
    float dodge_chance = 0.05f;     // 0.0 - 1.0
    float block_chance = 0.0f;      // 0.0 - 1.0
    float parry_chance = 0.0f;      // 0.0 - 1.0
    
    // Combat - Magical
    int32_t spell_power = 0;
    int32_t magic_resist = 0;
    float cast_speed = 1.0f;        // Multiplier
    float spell_crit_chance = 0.05f;
    
    // Combat - Critical
    float critical_chance = 0.05f;   // 0.0 - 1.0
    float critical_damage = 1.5f;    // Multiplier
    
    // Regeneration
    float health_regen = 1.0f;       // Per second
    float mana_regen = 1.0f;         // Per second
    float stamina_regen = 5.0f;      // Per second
    
    // Movement
    float movement_speed = 5.0f;     // Units per second
    float jump_height = 2.0f;        // Units
    
    // Resistances
    std::unordered_map<std::string, float> resistances;  // Damage type -> reduction %
};

// [SEQUENCE: 1156] Character level data
struct LevelData {
    uint32_t level = 1;
    uint64_t current_experience = 0;
    uint64_t experience_to_next = 100;
    uint32_t available_stat_points = 0;
    uint32_t available_skill_points = 1;
};

// [SEQUENCE: 1157] Base stats configuration
struct BaseStatsConfig {
    // Starting values at level 1
    int32_t base_strength = 10;
    int32_t base_agility = 10;
    int32_t base_intelligence = 10;
    int32_t base_vitality = 10;
    int32_t base_dexterity = 10;
    int32_t base_wisdom = 10;
    
    // Per level bonuses
    int32_t strength_per_level = 1;
    int32_t agility_per_level = 1;
    int32_t intelligence_per_level = 1;
    int32_t vitality_per_level = 1;
    int32_t dexterity_per_level = 1;
    int32_t wisdom_per_level = 1;
    
    // Stat points per level
    uint32_t stat_points_per_level = 5;
    uint32_t skill_points_per_level = 1;
};

// [SEQUENCE: 1158] Experience table
class ExperienceTable {
public:
    // [SEQUENCE: 1159] Calculate experience required for level
    static uint64_t GetExperienceForLevel(uint32_t level) {
        // Standard exponential curve
        // Level 1: 100 XP, Level 50: ~1M XP, Level 100: ~10M XP
        return static_cast<uint64_t>(100 * std::pow(1.1f, level - 1));
    }
    
    // [SEQUENCE: 1160] Calculate total experience for level
    static uint64_t GetTotalExperienceForLevel(uint32_t level) {
        uint64_t total = 0;
        for (uint32_t i = 1; i < level; ++i) {
            total += GetExperienceForLevel(i);
        }
        return total;
    }
    
    // [SEQUENCE: 1161] Calculate level from total experience
    static uint32_t GetLevelFromExperience(uint64_t total_exp) {
        uint32_t level = 1;
        uint64_t required = 0;
        
        while (required <= total_exp && level < MAX_LEVEL) {
            required += GetExperienceForLevel(level);
            if (required <= total_exp) {
                level++;
            }
        }
        
        return level;
    }
    
    static constexpr uint32_t MAX_LEVEL = 100;
};

// [SEQUENCE: 1162] Character stats component
class CharacterStats {
public:
    CharacterStats(uint64_t entity_id, uint32_t class_id)
        : entity_id_(entity_id), class_id_(class_id) {
        InitializeBaseStats();
    }
    
    // [SEQUENCE: 1163] Primary attributes
    int32_t GetStrength() const { return base_strength_ + bonus_strength_; }
    int32_t GetAgility() const { return base_agility_ + bonus_agility_; }
    int32_t GetIntelligence() const { return base_intelligence_ + bonus_intelligence_; }
    int32_t GetVitality() const { return base_vitality_ + bonus_vitality_; }
    int32_t GetDexterity() const { return base_dexterity_ + bonus_dexterity_; }
    int32_t GetWisdom() const { return base_wisdom_ + bonus_wisdom_; }
    
    // [SEQUENCE: 1164] Allocate stat points
    bool AllocateStatPoint(PrimaryAttribute attribute);
    bool AllocateStatPoints(const std::unordered_map<PrimaryAttribute, uint32_t>& allocation);
    void ResetStatPoints();  // Return all allocated points
    
    // [SEQUENCE: 1165] Level management
    void AddExperience(uint64_t amount);
    bool SetLevel(uint32_t new_level);
    const LevelData& GetLevelData() const { return level_data_; }
    
    // [SEQUENCE: 1166] Calculate secondary stats
    SecondaryStats CalculateSecondaryStats() const;
    
    // [SEQUENCE: 1167] Stat modifiers (from buffs, equipment, etc.)
    void AddStatModifier(const std::string& source, PrimaryAttribute attr, int32_t value);
    void RemoveStatModifier(const std::string& source);
    void ClearAllModifiers();
    
    // [SEQUENCE: 1168] Class-specific stat growth
    void SetClassConfig(const BaseStatsConfig& config) { class_config_ = config; }
    const BaseStatsConfig& GetClassConfig() const { return class_config_; }
    
private:
    uint64_t entity_id_;
    uint32_t class_id_;
    
    // Level and experience
    LevelData level_data_;
    
    // Base stats (from level and allocation)
    int32_t base_strength_ = 10;
    int32_t base_agility_ = 10;
    int32_t base_intelligence_ = 10;
    int32_t base_vitality_ = 10;
    int32_t base_dexterity_ = 10;
    int32_t base_wisdom_ = 10;
    
    // Allocated stats (from stat points)
    int32_t allocated_strength_ = 0;
    int32_t allocated_agility_ = 0;
    int32_t allocated_intelligence_ = 0;
    int32_t allocated_vitality_ = 0;
    int32_t allocated_dexterity_ = 0;
    int32_t allocated_wisdom_ = 0;
    
    // Bonus stats (from equipment, buffs, etc.)
    int32_t bonus_strength_ = 0;
    int32_t bonus_agility_ = 0;
    int32_t bonus_intelligence_ = 0;
    int32_t bonus_vitality_ = 0;
    int32_t bonus_dexterity_ = 0;
    int32_t bonus_wisdom_ = 0;
    
    // Class configuration
    BaseStatsConfig class_config_;
    
    // Stat modifiers by source
    struct StatModifier {
        PrimaryAttribute attribute;
        int32_t value;
    };
    std::unordered_map<std::string, std::vector<StatModifier>> stat_modifiers_;
    
    // [SEQUENCE: 1169] Initialize base stats
    void InitializeBaseStats();
    
    // [SEQUENCE: 1170] Level up processing
    void ProcessLevelUp(uint32_t old_level, uint32_t new_level);
    
    // [SEQUENCE: 1171] Recalculate bonuses
    void RecalculateBonuses();
};

// [SEQUENCE: 1172] Stats formulas
class StatsFormulas {
public:
    // Health calculation
    static int32_t CalculateMaxHealth(int32_t vitality, int32_t level) {
        return 100 + (vitality * 10) + (level * 20);
    }
    
    // Mana calculation
    static int32_t CalculateMaxMana(int32_t intelligence, int32_t wisdom, int32_t level) {
        return 50 + (intelligence * 8) + (wisdom * 5) + (level * 10);
    }
    
    // Physical attack power
    static int32_t CalculateAttackPower(int32_t strength, int32_t dexterity, int32_t level) {
        return strength * 2 + dexterity / 2 + level * 3;
    }
    
    // Spell power
    static int32_t CalculateSpellPower(int32_t intelligence, int32_t level) {
        return intelligence * 3 + level * 2;
    }
    
    // Armor
    static int32_t CalculateArmor(int32_t agility, int32_t vitality) {
        return agility / 2 + vitality;
    }
    
    // Critical chance (0.0 - 1.0)
    static float CalculateCriticalChance(int32_t dexterity, int32_t agility) {
        float base_crit = 0.05f;  // 5% base
        float bonus_crit = (dexterity * 0.002f) + (agility * 0.001f);  // 0.2% per DEX, 0.1% per AGI
        return std::min(base_crit + bonus_crit, 0.5f);  // Cap at 50%
    }
    
    // Dodge chance (0.0 - 1.0)
    static float CalculateDodgeChance(int32_t agility) {
        float base_dodge = 0.05f;  // 5% base
        float bonus_dodge = agility * 0.002f;  // 0.2% per AGI
        return std::min(base_dodge + bonus_dodge, 0.3f);  // Cap at 30%
    }
    
    // Attack speed
    static float CalculateAttackSpeed(int32_t agility, int32_t dexterity) {
        float base_speed = 1.0f;
        float bonus_speed = (agility * 0.005f) + (dexterity * 0.003f);
        return base_speed + bonus_speed;  // No hard cap
    }
    
    // Health regeneration per second
    static float CalculateHealthRegen(int32_t vitality, int32_t level) {
        return 1.0f + (vitality * 0.1f) + (level * 0.05f);
    }
    
    // Mana regeneration per second
    static float CalculateManaRegen(int32_t wisdom, int32_t intelligence, int32_t level) {
        return 1.0f + (wisdom * 0.2f) + (intelligence * 0.05f) + (level * 0.03f);
    }
};

// [SEQUENCE: 1173] Character class definitions
struct CharacterClassData {
    uint32_t class_id;
    std::string class_name;
    std::string description;
    
    // Stat configuration
    BaseStatsConfig stats_config;
    
    // Primary attribute focus (affects stat growth)
    PrimaryAttribute primary_attribute;
    PrimaryAttribute secondary_attribute;
    
    // Starting skills
    std::vector<uint32_t> starting_skills;
    
    // Class-specific modifiers
    std::unordered_map<std::string, float> class_modifiers;
};

// [SEQUENCE: 1174] Common character classes
namespace CharacterClasses {
    // Warrior - STR/VIT focus
    inline CharacterClassData CreateWarrior() {
        CharacterClassData warrior;
        warrior.class_id = 1;
        warrior.class_name = "Warrior";
        warrior.description = "Master of melee combat";
        warrior.primary_attribute = PrimaryAttribute::STRENGTH;
        warrior.secondary_attribute = PrimaryAttribute::VITALITY;
        
        // Higher STR/VIT growth
        warrior.stats_config.strength_per_level = 3;
        warrior.stats_config.vitality_per_level = 2;
        
        return warrior;
    }
    
    // Mage - INT/WIS focus
    inline CharacterClassData CreateMage() {
        CharacterClassData mage;
        mage.class_id = 2;
        mage.class_name = "Mage";
        mage.description = "Master of arcane magic";
        mage.primary_attribute = PrimaryAttribute::INTELLIGENCE;
        mage.secondary_attribute = PrimaryAttribute::WISDOM;
        
        // Higher INT/WIS growth
        mage.stats_config.intelligence_per_level = 3;
        mage.stats_config.wisdom_per_level = 2;
        
        return mage;
    }
    
    // Rogue - AGI/DEX focus
    inline CharacterClassData CreateRogue() {
        CharacterClassData rogue;
        rogue.class_id = 3;
        rogue.class_name = "Rogue";
        rogue.description = "Master of stealth and precision";
        rogue.primary_attribute = PrimaryAttribute::AGILITY;
        rogue.secondary_attribute = PrimaryAttribute::DEXTERITY;
        
        // Higher AGI/DEX growth
        rogue.stats_config.agility_per_level = 3;
        rogue.stats_config.dexterity_per_level = 2;
        
        return rogue;
    }
}

} // namespace mmorpg::game::stats