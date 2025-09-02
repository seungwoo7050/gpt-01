#pragma once

#include <cstdint>

namespace mmorpg::game::components {

// [SEQUENCE: MVP2-18] Stores an entity's current and maximum health, shield, and death state.
struct HealthComponent {
    float current_hp = 100.0f;
    float max_hp = 100.0f;
    float hp_regen_rate = 1.0f;  // HP per second
    float shield = 0.0f;
    float max_shield = 0.0f;
    bool is_dead = false;
};

} // namespace mmorpg::game::components

