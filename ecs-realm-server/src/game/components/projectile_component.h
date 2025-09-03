#pragma once

#include "core/ecs/component.h"
#include "core/utils/vector3.h"
#include "core/ecs/types.h"

namespace mmorpg::game::components {

// [SEQUENCE: MVP4-8]
struct ProjectileComponent {
    core::ecs::EntityId owner;
    core::utils::Vector3 velocity;
    float speed;
    float range;
    float traveled;
    float damage;
    float radius;
    bool is_physical;
    bool piercing;
    uint32_t skill_id;
};

}