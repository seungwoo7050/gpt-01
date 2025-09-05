#pragma once

#include "core/ecs/system.h"

namespace mmorpg::game::systems {

// [SEQUENCE: MVP5-86] Manager for handling player-versus-player interactions like duels.
class PvpManager : public core::ecs::System {
public:
    PvpManager() = default;

    // [SEQUENCE: MVP5-87] Public API for the PvP Manager.
    void HandleDuelRequest(core::ecs::EntityId requester, core::ecs::EntityId target);
    void Update(float delta_time);
};

} // namespace mmorpg::game::systems