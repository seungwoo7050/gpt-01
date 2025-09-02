#include "skill_system.h"
#include "core/ecs/world.h"
#include "game/systems/combat_system.h" // For applying damage

namespace mmorpg::game::systems {

using namespace core::ecs;
using namespace game::components;

// [SEQUENCE: MVP9-24] Update loop for the skill system
void SkillSystem::Update(float delta_time) {
    auto& world = World::Instance();
    for (EntityId entity : world.GetEntitiesWith<SkillComponent>()) {
        auto& skill_comp = world.GetComponent<SkillComponent>(entity);
        if (skill_comp.casting_skill_id != 0) {
            ProcessCasting(entity, skill_comp, delta_time);
        }
    }
}

// [SEQUENCE: MVP9-25] Attempt to use a skill
bool SkillSystem::UseSkill(EntityId caster_id, uint32_t skill_id, EntityId target_id) {
    auto& world = World::Instance();
    if (!world.HasComponent<SkillComponent>(caster_id)) {
        return false;
    }

    auto& skill_comp = world.GetComponent<SkillComponent>(caster_id);
    auto it = skill_comp.skills.find(skill_id);
    if (it == skill_comp.skills.end()) {
        return false; // Skill not learned
    }

    const auto& skill = it->second;

    // Check cooldown
    auto cd_it = skill_comp.cooldowns.find(skill_id);
    if (cd_it != skill_comp.cooldowns.end() && cd_it->second.is_on_cooldown) {
        if (std::chrono::steady_clock::now() < cd_it->second.ready_time) {
            return false; // On cooldown
        }
    }

    // Start casting
    skill_comp.casting_skill_id = skill_id;
    skill_comp.cast_target = target_id;
    skill_comp.cast_end_time = std::chrono::steady_clock::now() + std::chrono::duration<float>(skill.cast_time);

    return true;
}

// [SEQUENCE: MVP9-26] Process an ongoing cast
void SkillSystem::ProcessCasting(EntityId caster_id, SkillComponent& skill_comp, float delta_time) {
    if (std::chrono::steady_clock::now() >= skill_comp.cast_end_time) {
        // Casting finished, apply effect
        const auto& skill = skill_comp.skills.at(skill_comp.casting_skill_id);
        ApplySkillEffect(caster_id, skill, skill_comp.cast_target);

        // Set cooldown
        skill_comp.cooldowns[skill.skill_id] = {
            std::chrono::steady_clock::now() + std::chrono::duration<float>(skill.cooldown_duration),
            true
        };

        // Reset casting state
        skill_comp.casting_skill_id = 0;
    }
}

// [SEQUENCE: MVP9-27] Apply the skill's effect
void SkillSystem::ApplySkillEffect(EntityId caster_id, const Skill& skill, EntityId target_id) {
    // This is a simplified effect application.
    // A more advanced system would use an event bus or a component-based effect system.
    auto& world = World::Instance();
    auto* combat_system = world.GetSystem<CombatSystem>();
    if (combat_system) {
        // Assuming a simple damage effect for now
        float total_damage = skill.base_damage;
        
        // Add scaling from stats if applicable
        if (world.HasComponent<CombatStatsComponent>(caster_id)) {
            auto& stats = world.GetComponent<CombatStatsComponent>(caster_id);
            total_damage += skill.damage_coefficient * (skill.is_physical ? stats.attack_power : stats.spell_power);
        }
        
        // This is a conceptual call. The CombatSystem would need a method to apply direct damage.
        // combat_system->ApplyDirectDamage(target_id, total_damage);
    }
}

} // namespace mmorpg::game::systems