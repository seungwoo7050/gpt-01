#pragma once

#include "core/ecs/system.h"

namespace mmorpg::game::ui {

// [SEQUENCE: MVP7-1] Manages the player's Head-Up Display (HUD)
class HudSystem : public core::ecs::System {
public:
    HudSystem() : System("HudSystem") {}

    // [SEQUENCE: MVP7-2] Update HUD elements for all players
    void Update(float delta_time) override;

    // [SEQUENCE: MVP7-3] Show a notification on a player's HUD
    void ShowNotification(core::ecs::EntityId player, const std::string& message);

    // [SEQUENCE: MVP7-4] Update the player's health bar
    void UpdateHealth(core::ecs::EntityId player, float current_health, float max_health);
};

}