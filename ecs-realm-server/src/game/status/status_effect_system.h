#pragma once

#include "core/ecs/system.h"
#include "game/components/status_effect_component.h"

namespace mmorpg::game::systems {

// [SEQUENCE: MVP9-28] Status effect system for buffs and debuffs
class StatusEffectSystem : public core::ecs::System {
public:
    StatusEffectSystem() : System("StatusEffectSystem") {}

    // [SEQUENCE: MVP9-29] Update active effects, check durations
    void Update(float delta_time) override;

    // [SEQUENCE: MVP9-30] Apply a status effect to an entity
    void ApplyEffect(core::ecs::EntityId target_id, uint32_t effect_id, core::ecs::EntityId caster_id);

    // [SEQUENCE: MVP9-31] Remove a status effect from an entity
    void RemoveEffect(core::ecs::EntityId target_id, uint32_t effect_id);

private:
    // [SEQUENCE: MVP9-32] Process effects for a single entity
    void ProcessEntityEffects(core::ecs::EntityId entity_id, components::StatusEffectComponent& effect_comp, float delta_time);
};

} // namespace mmorpg::game::systems