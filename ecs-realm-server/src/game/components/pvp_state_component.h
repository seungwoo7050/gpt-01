// [SEQUENCE: MVP5-9] A new component, extracted from openworld_pvp_system.h, to track an individual player's PvP state.
#pragma once

#include "core/ecs/types.h"
#include <chrono>
#include <unordered_set>

namespace mmorpg::game::components {

struct PvPStateComponent {
    bool pvp_flagged = false;
    uint32_t faction_id = 0;
    std::chrono::steady_clock::time_point flag_time;
    std::chrono::steady_clock::time_point last_pvp_action;
    core::ecs::EntityId current_zone = 0;
    std::unordered_set<core::ecs::EntityId> recent_attackers;
};

} // namespace mmorpg::game::components
