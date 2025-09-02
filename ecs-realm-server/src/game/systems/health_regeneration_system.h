#pragma once

#include "core/ecs/system.h"

namespace mmorpg::game::systems {

// [SEQUENCE: MVP2-23] Implements passive health regeneration for entities with a HealthComponent.
class HealthRegenerationSystem : public core::ecs::System {
public:
    HealthRegenerationSystem() = default;
    
    void OnSystemInit() override;
    void OnSystemShutdown() override;
    void Update(float delta_time) override;
    
private:
    float regen_delay_after_damage_ = 5.0f; // Seconds before regen starts
};

} // namespace mmorpg::game::systems