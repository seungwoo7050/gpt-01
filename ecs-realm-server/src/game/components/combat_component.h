#pragma once

#include <vector>
#include <chrono>

namespace mmorpg::game::components {

// [SEQUENCE: MVP4-10] This component was deprecated. Its logic was removed, and its state variables were moved to more appropriate components (`SkillComponent`, `TargetComponent`).
struct CombatComponent {
    // Basic stats
    float attack_power = 10.0f;
    float defense = 5.0f;
    float attack_speed = 1.0f; // Attacks per second
    float attack_range = 2.0f; // Units
    float critical_chance = 0.1f; // 10%
    float critical_multiplier = 2.0f;
    
    // Combat state
    uint64_t current_target = 0; // Entity ID of target
    std::chrono::steady_clock::time_point last_attack_time;
    bool is_attacking = false;
    
    // Skills (simplified for MVP)
    std::vector<uint32_t> available_skills;
    std::vector<uint32_t> skills_on_cooldown;
    
    // [SEQUENCE: 2] Calculate damage output
    float CalculateDamage() const {
        float base_damage = attack_power;
        
        // Check for critical hit (simplified random)
        static uint32_t crit_counter = 0;
        if ((++crit_counter % 10) < (critical_chance * 10)) {
            base_damage *= critical_multiplier;
        }
        
        return base_damage;
    }
    
    // [SEQUENCE: 3] Calculate damage reduction
    float CalculateDamageReduction(float incoming_damage) const {
        // Simple flat reduction for MVP
        return std::max(1.0f, incoming_damage - defense);
    }
    
    // [SEQUENCE: 4] Check if can attack
    bool CanAttack() const {
        if (!is_attacking || current_target == 0) {
            return false;
        }
        
        auto now = std::chrono::steady_clock::now();
        auto time_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_attack_time
        ).count();
        
        float attack_interval_ms = 1000.0f / attack_speed;
        return time_since_last >= attack_interval_ms;
    }
};

} // namespace mmorpg::game::components