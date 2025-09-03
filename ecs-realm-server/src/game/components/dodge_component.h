#pragma once

#include "core/ecs/component.h"
#include "core/utils/vector3.h"
#include <chrono>

namespace mmorpg::game::components {

// [SEQUENCE: MVP4-9]
struct DodgeComponent {
    std::chrono::steady_clock::time_point dodge_end_time;
    std::chrono::steady_clock::time_point next_dodge_time;
    core::utils::Vector3 dodge_direction;
    bool is_dodging = false;
    int dodge_charges = 2;
    float dodge_recharge_time = 5.0f;
};

}