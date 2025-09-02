#pragma once

#include "core/ecs/system.h"

namespace mmorpg::game::systems {

// [SEQUENCE: MVP9-62] PvP Manager for handling player versus player interactions
class PvpManager : public core::ecs::System {
public:
    PvpManager() : System("PvpManager") {}

    // [SEQUENCE: MVP9-63] Handle a duel request
    void HandleDuelRequest(core::ecs::EntityId requester, core::ecs::EntityId target);

    // [SEQUENCE: MVP9-64] Update PvP states
    void Update(float delta_time) override;
};

} // namespace mmorpg::game::systems