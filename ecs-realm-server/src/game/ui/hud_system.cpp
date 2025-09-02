#include "game/ui/hud_system.h"
#include <iostream> // Placeholder for actual rendering/network logic

namespace mmorpg::game::ui {

// [SEQUENCE: MVP7-5] Update HUD elements for all players
void HudSystem::Update(float delta_time) {
    // This would iterate over players and update their HUDs
}

// [SEQUENCE: MVP7-6] Show a notification on a player's HUD
void HudSystem::ShowNotification(core::ecs::EntityId player, const std::string& message) {
    // In a real implementation, this would send a packet to the client
    std::cout << "Player " << player << " notification: " << message << std::endl;
}

// [SEQUENCE: MVP7-7] Update the player's health bar
void HudSystem::UpdateHealth(core::ecs::EntityId player, float current_health, float max_health) {
    // In a real implementation, this would send a packet to the client
    std::cout << "Player " << player << " health: " << current_health << "/" << max_health << std::endl;
}

}