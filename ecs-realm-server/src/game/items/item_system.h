#pragma once

#include "core/ecs/system.h"

namespace mmorpg::game::systems {

// [SEQUENCE: MVP9-42] Item system for managing item lifecycle and effects
class ItemSystem : public core::ecs::System {
public:
    ItemSystem() : System("ItemSystem") {}

    // [SEQUENCE: MVP9-43] Update items, e.g., for cooldowns on consumables
    void Update(float delta_time) override;

    // [SEQUENCE: MVP9-44] Use an item from an inventory
    bool UseItem(core::ecs::EntityId user_id, uint64_t item_instance_id);
};

} // namespace mmorpg::game::systems