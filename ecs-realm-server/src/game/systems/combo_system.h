#pragma once

#include "core/ecs/system.h"

namespace mmorpg::game::systems {

// [SEQUENCE: MVP9-59] Combo system for handling input sequences
class ComboSystem : public core::ecs::System {
public:
    ComboSystem() : System("ComboSystem") {}

    // [SEQUENCE: MVP9-60] Process player input for combos
    void ProcessInput(core::ecs::EntityId entity_id, int input_id);

    // [SEQUENCE: MVP9-61] Update combo states, check for timeouts
    void Update(float delta_time) override;
};

} // namespace mmorpg::game::systems