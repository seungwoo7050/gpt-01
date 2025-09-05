#pragma once

#include "core/ecs/types.h"
#include <chrono>
#include <unordered_set>

namespace mmorpg::game::components {

// [SEQUENCE: MVP5-5] Tracks an individual player's PvP state, such as whether they are flagged for PvP.
struct PvPStateComponent {
    bool pvp_flagged = false;
    uint32_t faction_id = 0;
    std::chrono::steady_clock::time_point flag_time;
    std::chrono::steady_clock::time_point last_pvp_action;
    core::ecs::EntityId current_zone = 0;
    std::unordered_set<core::ecs::EntityId> recent_attackers;
};

} // namespace mmorpg::game::components