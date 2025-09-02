#pragma once

#include "core/utils/vector3.h"

namespace mmorpg::game::components {

// [SEQUENCE: MVP2-16] Stores an entity's position, rotation, and scale in 3D space.
struct TransformComponent {
    core::utils::Vector3 position{0.0f, 0.0f, 0.0f};
    core::utils::Vector3 rotation{0.0f, 0.0f, 0.0f};
    core::utils::Vector3 scale{1.0f, 1.0f, 1.0f};
};

} // namespace mmorpg::game::components