#pragma once

#include "core/ecs/optimized/system.h"

namespace mmorpg::game::systems::pvp {

// [SEQUENCE: MVP5-84] Placeholder for a future arena system.
class ArenaSystem : public core::ecs::optimized::System {
public:
    void Update(float delta_time) override;
};

}