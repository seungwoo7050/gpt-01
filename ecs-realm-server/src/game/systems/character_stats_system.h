#pragma once

#include "core/ecs/system.h"

namespace mmorpg::game::systems {

// [SEQUENCE: MVP9-54] Character stats and leveling system
class CharacterStatsSystem : public core::ecs::System {
public:
    CharacterStatsSystem() : System("CharacterStatsSystem") {}

    // [SEQUENCE: MVP9-55] Add experience to an entity
    void AddExperience(core::ecs::EntityId entity_id, uint64_t amount);

    // [SEQUENCE: MVP9-56] Recalculate all derived stats for an entity
    void RecalculateStats(core::ecs::EntityId entity_id);
};

} // namespace mmorpg::game::systems