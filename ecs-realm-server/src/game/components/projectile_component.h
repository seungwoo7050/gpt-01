// [SEQUENCE: MVP4-8] A new component to represent a projectile in the world.
#pragma once

#include "core/ecs/types.h"
#include "core/utils/vector3.h"

namespace mmorpg::game::components {

struct ProjectileComponent {
    core::ecs::EntityId owner;
    core::utils::Vector3 velocity;
    float speed;
    float range;
    float traveled;
    float damage;
    float radius;  // Collision radius
    bool is_physical;
    bool piercing;  // Goes through enemies
    uint32_t skill_id;
};

} // namespace mmorpg::game::components
