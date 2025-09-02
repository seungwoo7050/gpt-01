#include "game/systems/health_regeneration_system.h"
#include "core/ecs/world.h"
#include "game/components/health_component.h"
#include "game/components/network_component.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace mmorpg::game::systems {

// [SEQUENCE: MVP2-23] Implements passive health regeneration for entities with a HealthComponent.
void HealthRegenerationSystem::OnSystemInit() {
    spdlog::info("HealthRegenerationSystem initialized");
}

void HealthRegenerationSystem::OnSystemShutdown() {
    spdlog::info("HealthRegenerationSystem shutdown");
}

void HealthRegenerationSystem::Update(float delta_time) {
    for (const auto& entity : entities_) {
        auto& health = world_->GetComponent<components::HealthComponent>(entity);

        // Skip dead entities
        if (health.is_dead) continue;
        
        // Skip if at full health
        if (health.current_hp >= health.max_hp) continue;
        
        // Apply regeneration
        float old_hp = health.current_hp;
        health.current_hp = std::min(health.max_hp, health.current_hp + (health.hp_regen_rate * delta_time));
        
        // Mark for network update if health changed
        if (health.current_hp != old_hp) {
            if (world_->HasComponent<components::NetworkComponent>(entity)) {
                auto& network = world_->GetComponent<components::NetworkComponent>(entity);
                network.MarkHealthDirty();
            }
        }
    }
}

} // namespace mmorpg::game::systems