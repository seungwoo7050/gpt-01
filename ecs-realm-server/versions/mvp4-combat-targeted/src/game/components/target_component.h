#pragma once

#include "core/ecs/types.h"
#include <chrono>

namespace mmorpg::game::components {

// [SEQUENCE: 1] Target types for different targeting modes
enum class TargetType {
    NONE,
    ENEMY,
    ALLY,
    SELF,
    GROUND,    // Ground-targeted location
    OBJECT     // Interactable object
};

// [SEQUENCE: 2] Component for target-based combat
struct TargetComponent {
    // [SEQUENCE: 3] Current target
    core::ecs::EntityId current_target = 0;
    TargetType target_type = TargetType::NONE;
    
    // [SEQUENCE: 4] Target validation
    float max_target_range = 50.0f;            // Maximum targeting range
    std::chrono::steady_clock::time_point last_validation_time;
    bool target_in_range = false;
    bool target_in_sight = true;               // Line of sight check
    
    // [SEQUENCE: 5] Auto-attack state
    bool auto_attacking = false;
    std::chrono::steady_clock::time_point next_auto_attack_time;
    float auto_attack_range = 5.0f;            // Melee range
    
    // [SEQUENCE: 6] Target history (for tab-targeting)
    std::vector<core::ecs::EntityId> target_history;
    size_t target_history_index = 0;
    
    // [SEQUENCE: 7] Assist target (target of target)
    core::ecs::EntityId assist_target = 0;
    
    // [SEQUENCE: 8] Focus target (secondary target)
    core::ecs::EntityId focus_target = 0;
};

} // namespace mmorpg::game::components