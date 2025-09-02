// [SEQUENCE: MVP5-8] A new component, extracted from match_component.h, to define a PvP-enabled zone in the world.
#pragma once

#include "core/ecs/types.h"
#include "core/utils/vector3.h"
#include <string>
#include <vector>

namespace mmorpg::game::components {

struct PvPZoneComponent {
    uint32_t zone_id;
    std::string zone_name;
    
    bool pvp_enabled = true;
    bool faction_based = true;
    bool free_for_all = false;
    uint32_t min_level = 10;
    
    uint32_t controlling_faction = 0;
    float capture_progress = 0.0f;
    std::vector<core::ecs::EntityId> capturing_players;
    
    struct Objective {
        uint32_t objective_id;
        core::utils::Vector3 position;
        uint32_t controlling_team = 0;
        float capture_radius = 10.0f;
        uint32_t point_value = 1;
    };
    std::vector<Objective> objectives;
    
    float experience_bonus = 1.0f;
    float honor_bonus = 1.0f;
    std::vector<uint32_t> buff_ids;
};

} // namespace mmorpg::game::components
