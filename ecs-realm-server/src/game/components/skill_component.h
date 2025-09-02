// [SEQUENCE: MVP4-6] Manages an entity's skills, cooldowns, and casting state.
#pragma once

#include "core/ecs/types.h"
#include <unordered_map>
#include <chrono>
#include <vector>

namespace mmorpg::game::components {


enum class SkillType {
    INSTANT,        // Instant cast, no target required
    TARGETED,       // Requires target selection
    SKILLSHOT,      // Directional skill, no target lock
    AREA_OF_EFFECT, // Ground-targeted AoE
    CHANNELED,      // Continuous cast
    TOGGLE,         // On/off ability
    PASSIVE         // Always active
};


enum class ResourceType {
    MANA,
    STAMINA,
    ENERGY,
    RAGE,
    COMBO_POINTS
};


struct Skill {
    uint32_t skill_id = 0;
    std::string name;
    SkillType type = SkillType::INSTANT;
    
    
    ResourceType resource_type = ResourceType::MANA;
    float resource_cost = 0.0f;
    float cooldown_duration = 0.0f;    // Seconds
    float cast_time = 0.0f;            // Seconds
    float channel_duration = 0.0f;     // For channeled skills
    
    
    float range = 5.0f;                // Max cast range
    float radius = 0.0f;               // AoE radius (0 for single target)
    float base_damage = 0.0f;          // Base damage amount
    float damage_coefficient = 1.0f;   // Scaling with stats
    bool is_physical = true;           // Physical or magical damage
    
    
    std::vector<uint32_t> buff_ids;    // Buffs applied on cast
    std::vector<uint32_t> debuff_ids;  // Debuffs applied to target
    uint32_t combo_starter = 0;        // Skill ID this combos from
    uint32_t combo_ender = 0;          // Skill ID this combos into
};


struct SkillCooldown {
    std::chrono::steady_clock::time_point ready_time;
    bool is_on_cooldown = false;
};


struct SkillComponent {
    
    std::unordered_map<uint32_t, Skill> skills;
    
    
    std::unordered_map<uint32_t, SkillCooldown> cooldowns;
    
    
    std::vector<uint32_t> skill_bar;  // Skill IDs in slots 1-10
    
    
    uint32_t casting_skill_id = 0;
    std::chrono::steady_clock::time_point cast_end_time;
    core::ecs::EntityId cast_target = 0;
    
    
    std::chrono::steady_clock::time_point global_cooldown_end;
    float global_cooldown_duration = 1.0f;  // 1 second GCD
    
    
    uint32_t last_skill_used = 0;
    std::chrono::steady_clock::time_point combo_window_end;

    bool is_attacking = false;
    std::chrono::steady_clock::time_point last_attack_time;
};

} // namespace mmorpg::game::components