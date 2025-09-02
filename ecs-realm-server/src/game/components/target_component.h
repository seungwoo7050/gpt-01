// [SEQUENCE: MVP4-7] Holds all data related to an entity's target.
#pragma once

#include "core/ecs/types.h"
#include <chrono>

namespace mmorpg::game::components {


enum class TargetType {
    NONE,
    ENEMY,
    ALLY,
    SELF,
    GROUND,    // Ground-targeted location
    OBJECT     // Interactable object
};


struct TargetComponent {
    
    core::ecs::EntityId current_target = 0;
    TargetType target_type = TargetType::NONE;
    
    
    float max_target_range = 50.0f;            // Maximum targeting range
    std::chrono::steady_clock::time_point last_validation_time;
    bool target_in_range = false;
    bool target_in_sight = true;               // Line of sight check
    
    
    bool auto_attacking = false;
    std::chrono::steady_clock::time_point next_auto_attack_time;
    float auto_attack_range = 5.0f;            // Melee range
    
    
    std::vector<core::ecs::EntityId> target_history;
    size_t target_history_index = 0;
    
    
    core::ecs::EntityId assist_target = 0;
    
    
    core::ecs::EntityId focus_target = 0;
};

} // namespace mmorpg::game::components