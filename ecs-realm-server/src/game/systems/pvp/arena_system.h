// [SEQUENCE: MVP5-15] Placeholder for a future arena system.
#pragma once

#include "core/ecs/system.h"

namespace mmorpg::game::systems::pvp {

class ArenaSystem : public core::ecs::System {
public:
    ArenaSystem() = default;
    
    void OnSystemInit() override;
    void OnSystemShutdown() override;
    void Update(float delta_time) override;
};

} // namespace mmorpg::game::systems::pvp
