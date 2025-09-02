#pragma once

#include "core/ecs/types.h"

namespace mmorpg::game::components {

// [SEQUENCE: 1] Combat statistics component for damage calculations
struct CombatStatsComponent {
    // [SEQUENCE: 2] Offensive stats
    float attack_power = 10.0f;      // Physical damage multiplier
    float spell_power = 10.0f;       // Magical damage multiplier
    float critical_chance = 0.05f;   // 5% base crit chance
    float critical_damage = 1.5f;    // 150% damage on crit
    float attack_speed = 1.0f;       // Attacks per second
    float cast_speed = 1.0f;         // Cast time multiplier
    
    // [SEQUENCE: 3] Defensive stats
    float armor = 0.0f;              // Physical damage reduction
    float magic_resist = 0.0f;       // Magical damage reduction
    float dodge_chance = 0.05f;      // 5% base dodge
    float block_chance = 0.0f;       // Shield block chance
    float block_value = 0.0f;        // Damage blocked
    
    // [SEQUENCE: 4] Resource stats
    float health_regen = 1.0f;       // HP per second
    float mana_regen = 1.0f;         // MP per second
    float stamina_regen = 5.0f;      // Stamina per second
    
    // [SEQUENCE: 5] Combat modifiers
    float damage_reduction = 0.0f;   // Flat damage reduction %
    float damage_increase = 0.0f;    // Flat damage increase %
    float healing_power = 1.0f;      // Healing multiplier
    float movement_speed = 1.0f;     // Movement speed multiplier
    
    // [SEQUENCE: 6] Level-based stats
    int level = 1;
    int experience = 0;
    int skill_points = 0;
};

} // namespace mmorpg::game::components