#pragma once

#include "core/singleton.h"
#include "game/components/inventory_component.h"

namespace mmorpg::game::systems {

// [SEQUENCE: MVP9-47] Inventory Manager to handle inventory operations
class InventoryManager : public Singleton<InventoryManager> {
public:
    // [SEQUENCE: MVP9-48] Add an item to an entity's inventory
    bool AddItem(core::ecs::EntityId entity_id, uint32_t item_id, uint32_t quantity = 1);

    // [SEQUENCE: MVP9-49] Remove an item from an entity's inventory
    bool RemoveItem(core::ecs::EntityId entity_id, uint32_t item_id, uint32_t quantity = 1);

    // [SEQUENCE: MVP9-50] Check if an entity has a specific item
    bool HasItem(core::ecs::EntityId entity_id, uint32_t item_id, uint32_t quantity = 1) const;
};

} // namespace mmorpg::game::systems