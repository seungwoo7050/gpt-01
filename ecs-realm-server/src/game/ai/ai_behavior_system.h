#pragma once

#include "core/ecs/system.h"
#include "game/ai/behavior_tree.h"

namespace mmorpg::game::systems {

// [SEQUENCE: MVP9-37] AI Behavior System to update all AI entities
class AiBehaviorSystem : public core::ecs::System {
public:
    AiBehaviorSystem() : System("AiBehaviorSystem") {}

    // [SEQUENCE: MVP9-38] Update loop for AI logic
    void Update(float delta_time) override;

private:
    // [SEQUENCE: MVP9-39] Process the behavior tree for a single AI entity
    void ProcessAi(core::ecs::EntityId entity_id, float delta_time);
};

} // namespace mmorpg::game::systems