#pragma once

#include "core/ecs/system.h"

namespace mmorpg::game::systems {

// [SEQUENCE: 1] Health regeneration system for passive healing
class HealthRegenerationSystem : public core::ecs::System {
public:
    HealthRegenerationSystem() : System("HealthRegenerationSystem") {}
    
    // [SEQUENCE: 2] System lifecycle
    void OnSystemInit() override;
    void OnSystemShutdown() override;
    
    // [SEQUENCE: 3] Update health regeneration
    void Update(float delta_time) override;
    
    // [SEQUENCE: 4] System metadata
    core::ecs::SystemStage GetStage() const override { 
        return core::ecs::SystemStage::UPDATE; 
    }
    int GetPriority() const override { return 300; } // After combat
    
private:
    // [SEQUENCE: 5] Configuration
    float regen_delay_after_damage_ = 5.0f; // Seconds before regen starts
};

} // namespace mmorpg::game::systems