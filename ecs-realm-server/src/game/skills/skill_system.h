#pragma once

#include "core/ecs/system.h"
#include "game/components/skill_component.h"

namespace mmorpg::game::systems {

// [SEQUENCE: MVP9-19] Skill system for managing abilities
class SkillSystem : public core::ecs::System {
public:
    SkillSystem() : System("SkillSystem") {}

    // [SEQUENCE: MVP9-20] Update active skill casts and cooldowns
    void Update(float delta_time) override;

    // [SEQUENCE: MVP9-21] Use a skill
    bool UseSkill(core::ecs::EntityId caster_id, uint32_t skill_id, core::ecs::EntityId target_id);

private:
    // [SEQUENCE: MVP9-22] Process a single skill cast
    void ProcessCasting(core::ecs::EntityId caster_id, components::SkillComponent& skill_comp, float delta_time);
    
    // [SEQUENCE: MVP9-23] Apply the effects of a skill
    void ApplySkillEffect(core::ecs::EntityId caster_id, const components::Skill& skill, core::ecs::EntityId target_id);
};

} // namespace mmorpg::game::systems