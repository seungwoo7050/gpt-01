#pragma once

#include "core/ecs/types.h"
#include <chrono>
#include <vector>

namespace mmorpg::game::components {

enum class TargetType {
    NONE,
    ENEMY,
    ALLY,
    SELF,
    GROUND,    // Ground-targeted location
    OBJECT     // Interactable object
};

// [SEQUENCE: MVP4-3] Holds all data related to an entity's target.
struct TargetComponent {
    // Current target
    core::ecs::EntityId current_target = 0;
    TargetType target_type = TargetType::NONE;
    
    // Target validation
    float max_target_range = 50.0f;            // Maximum targeting range
    std::chrono::steady_clock::time_point last_validation_time;
    bool target_in_range = false;
    bool target_in_sight = true;               // Line of sight check
    
    // Auto-attack state
    bool auto_attacking = false;
    float next_auto_attack_time = 0.0f;
    float auto_attack_range = 5.0f;            // Melee range
    
    // Target history (for tab-targeting)
    std::vector<core::ecs::EntityId> target_history;
    size_t target_history_index = 0;
    
    // Assist target (target of target)
    core::ecs::EntityId assist_target = 0;
    
    // Focus target (secondary target)
    core::ecs::EntityId focus_target = 0;
};

} // namespace mmorpg::game::components
