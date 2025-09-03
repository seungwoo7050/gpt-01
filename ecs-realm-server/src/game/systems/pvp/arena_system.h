// [SEQUENCE: MVP5-15] Placeholder files were created for a future arena system.
#pragma once

#include "core/ecs/optimized/system.h"

namespace mmorpg::game::systems::pvp {

class ArenaSystem : public core::ecs::optimized::System {
public:
    void Update(float delta_time) override;
};

}
