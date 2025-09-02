#pragma once

#include "core/utils/vector3.h"

namespace mmorpg::game::components {

// [SEQUENCE: MVP2-17] Stores an entity's linear and angular velocity for movement calculations.
struct VelocityComponent {
    core::utils::Vector3 linear{0.0f, 0.0f, 0.0f};
    core::utils::Vector3 angular{0.0f, 0.0f, 0.0f};
    float max_speed = 10.0f; // Units per second
    float acceleration = 20.0f; // Units per second squared
};

}
