#include "status_effect_system.h"
#include "core/ecs/world.h"

namespace mmorpg::game::systems {

using namespace core::ecs;
using namespace game::components;

// [SEQUENCE: MVP9-33] Update loop for status effect system
void StatusEffectSystem::Update(float delta_time) {
    auto& world = World::Instance();
    for (EntityId entity : world.GetEntitiesWith<StatusEffectComponent>()) {
        auto& effect_comp = world.GetComponent<StatusEffectComponent>(entity);
        ProcessEntityEffects(entity, effect_comp, delta_time);
    }
}

// [SEQUENCE: MVP9-34] Process effects for a single entity
void StatusEffectSystem::ProcessEntityEffects(EntityId entity_id, StatusEffectComponent& effect_comp, float delta_time) {
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = effect_comp.active_effects.begin(); it != effect_comp.active_effects.end(); ) {
        // Check duration
        if (now >= it->second.end_time) {
            // Effect expired
            it = effect_comp.active_effects.erase(it);
        } else {
            // Process periodic effects (DoTs, HoTs)
            // ...
            ++it;
        }
    }
}

// [SEQUENCE: MVP9-35] Apply a new status effect
void StatusEffectSystem::ApplyEffect(EntityId target_id, uint32_t effect_id, EntityId caster_id) {
    auto& world = World::Instance();
    if (!world.HasComponent<StatusEffectComponent>(target_id)) {
        world.AddComponent<StatusEffectComponent>(target_id);
    }
    
    auto& effect_comp = world.GetComponent<StatusEffectComponent>(target_id);
    
    // This is a simplified application. A real system would:
    // 1. Fetch StatusEffectData from a manager
    // 2. Handle stacking rules
    // 3. Calculate duration based on stats
    
    ActiveStatusEffect effect;
    effect.effect_id = effect_id;
    effect.caster_id = caster_id;
    effect.start_time = std::chrono::steady_clock::now();
    effect.end_time = effect.start_time + std::chrono::seconds(30); // Placeholder duration
    
    effect_comp.active_effects[effect_id] = effect;
}

// [SEQUENCE: MVP9-36] Remove a status effect
void StatusEffectSystem::RemoveEffect(EntityId target_id, uint32_t effect_id) {
    auto& world = World::Instance();
    if (world.HasComponent<StatusEffectComponent>(target_id)) {
        auto& effect_comp = world.GetComponent<StatusEffectComponent>(target_id);
        effect_comp.active_effects.erase(effect_id);
    }
}

} // namespace mmorpg::game::systems