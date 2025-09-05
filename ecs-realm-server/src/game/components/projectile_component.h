#pragma once

#include "core/ecs/types.h"
#include "core/utils/vector3.h"

namespace mmorpg::game::components {

// [SEQUENCE: MVP4-4] Represents a projectile in the world, storing its owner, trajectory, and combat properties.
struct ProjectileComponent {
    core::ecs::EntityId owner = 0;
    core::utils::Vector3 velocity;
    float speed = 10.0f;
    float range = 50.0f;
    float traveled = 0.0f;
    float damage = 10.0f;
    float radius = 0.5f;
    bool is_physical = true;
    bool piercing = false;
    uint32_t skill_id = 0;
};

} // namespace mmorpg::game::components
