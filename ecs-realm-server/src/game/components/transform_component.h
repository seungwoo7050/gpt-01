#pragma once

#include "core/utils/vector3.h"

namespace mmorpg::game::components {

// [SEQUENCE: MVP2-15] Game Components (`src/game/components/`)
// [SEQUENCE: MVP2-16] Stores an entity's position, rotation, and scale.
// [SEQUENCE: 1] Transform component for entity position and orientation
struct TransformComponent {
    core::utils::Vector3 position{0.0f, 0.0f, 0.0f};
    core::utils::Vector3 rotation{0.0f, 0.0f, 0.0f}; // Euler angles in radians
    core::utils::Vector3 scale{1.0f, 1.0f, 1.0f};
    
    // [SEQUENCE: 2] Helper methods
    void SetPosition(float x, float y, float z) {
        position.x = x;
        position.y = y;
        position.z = z;
    }
    
    void Translate(const core::utils::Vector3& delta) {
        position = position + delta;
    }
    
    void SetRotation(float pitch, float yaw, float roll) {
        rotation.x = pitch;
        rotation.y = yaw;
        rotation.z = roll;
    }
};

// [SEQUENCE: MVP2-17] Stores an entity's linear and angular velocity for movement calculations.
// [SEQUENCE: 3] Velocity component for movement
struct VelocityComponent {
    core::utils::Vector3 linear{0.0f, 0.0f, 0.0f};
    core::utils::Vector3 angular{0.0f, 0.0f, 0.0f};
    float max_speed = 10.0f; // Units per second
    float acceleration = 20.0f; // Units per second squared
};

} // namespace mmorpg::game::components