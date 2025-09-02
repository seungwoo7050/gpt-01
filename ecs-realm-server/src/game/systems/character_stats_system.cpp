#include "character_stats_system.h"
#include "core/ecs/world.h"
#include "game/components/character_stats_component.h"
#include "game/components/experience_component.h"

namespace mmorpg::game::systems {

using namespace core::ecs;
using namespace game::components;

// [SEQUENCE: MVP9-57] Add experience to an entity
void CharacterStatsSystem::AddExperience(EntityId entity_id, uint64_t amount) {
    auto& world = World::Instance();
    if (!world.HasComponent<ExperienceComponent>(entity_id)) {
        return;
    }

    auto& exp_comp = world.GetComponent<ExperienceComponent>(entity_id);
    exp_comp.current_exp += amount;

    // Check for level up
    while (exp_comp.current_exp >= exp_comp.exp_to_next_level) {
        exp_comp.current_exp -= exp_comp.exp_to_next_level;
        exp_comp.level++;
        // In a real system, get next level's EXP requirement from a table
        exp_comp.exp_to_next_level *= 1.5; 
        
        // Recalculate stats on level up
        RecalculateStats(entity_id);
    }
}

// [SEQUENCE: MVP9-58] Recalculate all derived stats
void CharacterStatsSystem::RecalculateStats(EntityId entity_id) {
    auto& world = World::Instance();
    if (!world.HasComponent<CharacterStatsComponent>(entity_id)) {
        return;
    }

    auto& stats_comp = world.GetComponent<CharacterStatsComponent>(entity_id);
    
    // This is where you would apply formulas based on primary attributes,
    // gear, and status effects to calculate secondary stats like
    // attack_power, health, mana, crit_chance, etc.
    
    // Example:
    // stats_comp.attack_power = stats_comp.strength * 2.0f;
}

} // namespace mmorpg::game::systems