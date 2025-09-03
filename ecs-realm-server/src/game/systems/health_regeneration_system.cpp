#include "game/systems/health_regeneration_system.h"
#include "core/ecs/world.h"
#include "game/components/health_component.h"
#include "game/components/network_component.h"
#include <spdlog/spdlog.h>

namespace mmorpg::game::systems {

// [SEQUENCE: 1] System initialization
void HealthRegenerationSystem::OnSystemInit() {
    spdlog::info("HealthRegenerationSystem initialized");
}

// [SEQUENCE: 2] System shutdown
void HealthRegenerationSystem::OnSystemShutdown() {
    spdlog::info("HealthRegenerationSystem shutdown");
}

// [SEQUENCE: 3] Update health regeneration
void HealthRegenerationSystem::Update(float delta_time) {
    auto* storage = GetComponentStorage();
    if (!storage) return;
    
    auto* health_storage = storage->GetStorage<components::HealthComponent>();
    if (!health_storage) return;
    
    // Process all entities with health components
    for (auto& [entity, health] : health_storage->GetAllComponents()) {
        // Skip dead entities
        if (health.is_dead) continue;
        
        // Skip if at full health
        if (health.current_hp >= health.max_hp) continue;
        
        // Apply regeneration
        float old_hp = health.current_hp;
        health.Regenerate(delta_time);
        
        // Mark for network update if health changed
        if (health.current_hp != old_hp) {
            auto* network = world_->GetComponent<components::NetworkComponent>(entity);
            if (network) {
                network->MarkHealthDirty();
            }
        }
    }
}

} // namespace mmorpg::game::systems