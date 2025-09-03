#pragma once

#include <algorithm>

namespace mmorpg::game::components {

// [SEQUENCE: MVP2-17] Stores an entity's current and maximum health.
// [SEQUENCE: MVP2-17] Stores an entity's current and maximum health.
// [SEQUENCE: 1] Health component for damageable entities
struct HealthComponent {
    float current_hp = 100.0f;
    float max_hp = 100.0f;
    float hp_regen_rate = 1.0f;  // HP per second
    float shield = 0.0f;
    float max_shield = 0.0f;
    bool is_dead = false;
    
    // [SEQUENCE: 2] Apply damage
    float TakeDamage(float damage) {
        if (is_dead) return 0.0f;
        
        float actual_damage = damage;
        
        // Apply to shield first
        if (shield > 0) {
            float shield_damage = std::min(shield, damage);
            shield -= shield_damage;
            actual_damage -= shield_damage;
        }
        
        // Apply remaining to health
        if (actual_damage > 0) {
            current_hp = std::max(0.0f, current_hp - actual_damage);
            if (current_hp <= 0) {
                is_dead = true;
            }
        }
        
        return damage; // Return total damage dealt
    }
    
    // [SEQUENCE: 3] Heal entity
    float Heal(float amount) {
        if (is_dead) return 0.0f;
        
        float old_hp = current_hp;
        current_hp = std::min(max_hp, current_hp + amount);
        return current_hp - old_hp; // Return actual healing done
    }
    
    // [SEQUENCE: 4] Regenerate health over time
    void Regenerate(float delta_time) {
        if (!is_dead && current_hp < max_hp) {
            Heal(hp_regen_rate * delta_time);
        }
    }
    
    // [SEQUENCE: 5] Get health percentage
    float GetHealthPercent() const {
        return max_hp > 0 ? (current_hp / max_hp) : 0.0f;
    }
};

} // namespace mmorpg::game::components