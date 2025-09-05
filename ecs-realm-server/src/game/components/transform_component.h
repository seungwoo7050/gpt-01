#pragma once

#include "core/utils/vector3.h"

namespace mmorpg::game::components {

// [SEQUENCE: MVP2-4] Defines TransformComponent and VelocityComponent for spatial state.

struct TransformComponent {
    core::utils::Vector3 position{0.0f, 0.0f, 0.0f};
    core::utils::Vector3 rotation{0.0f, 0.0f, 0.0f}; // Euler angles in radians
    core::utils::Vector3 scale{1.0f, 1.0f, 1.0f};
    
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

struct VelocityComponent {
    core::utils::Vector3 linear{0.0f, 0.0f, 0.0f};
    core::utils::Vector3 angular{0.0f, 0.0f, 0.0f};
    float max_speed = 10.0f; // Units per second
    float acceleration = 20.0f; // Units per second squared
};

}
