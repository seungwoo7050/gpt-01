#include "inventory_manager.h"
#include "core/ecs/world.h"
#include "game/items/item_manager.h"

namespace mmorpg::game::systems {

using namespace core::ecs;
using namespace game::components;
using namespace game::items;

// [SEQUENCE: MVP9-51] Add an item to an inventory
bool InventoryManager::AddItem(EntityId entity_id, uint32_t item_id, uint32_t quantity) {
    auto& world = World::Instance();
    if (!world.HasComponent<InventoryComponent>(entity_id)) {
        world.AddComponent<InventoryComponent>(entity_id);
    }
    
    auto& inventory = world.GetComponent<InventoryComponent>(entity_id);
    
    // Simplified logic: find first empty slot or stack.
    // A real implementation would be much more complex.
    
    // Check for stacking
    const auto& item_data = ItemManager::Instance().GetItemData(item_id);
    if (item_data.max_stack > 1) {
        for (auto& item : inventory.items) {
            if (item.item_id == item_id && item.quantity < item_data.max_stack) {
                item.quantity += quantity;
                return true;
            }
        }
    }
    
    // Add to new slot
    if (inventory.items.size() < inventory.capacity) {
        inventory.items.push_back({ItemManager::Instance().CreateItemInstance(item_id), quantity});
        return true;
    }

    return false; // Inventory full
}

// [SEQUENCE: MVP9-52] Remove an item from an inventory
bool InventoryManager::RemoveItem(EntityId entity_id, uint32_t item_id, uint32_t quantity) {
    auto& world = World::Instance();
    if (!world.HasComponent<InventoryComponent>(entity_id)) {
        return false;
    }
    
    auto& inventory = world.GetComponent<InventoryComponent>(entity_id);
    
    for (auto it = inventory.items.begin(); it != inventory.items.end(); ++it) {
        if (it->instance.item_id == item_id) {
            if (it->quantity >= quantity) {
                it->quantity -= quantity;
                if (it->quantity == 0) {
                    inventory.items.erase(it);
                }
                return true;
            }
        }
    }
    
    return false; // Item not found or not enough quantity
}

// [SEQUENCE: MVP9-53] Check for an item in an inventory
bool InventoryManager::HasItem(EntityId entity_id, uint32_t item_id, uint32_t quantity) const {
    auto& world = World::Instance();
    if (!world.HasComponent<InventoryComponent>(entity_id)) {
        return false;
    }
    
    const auto& inventory = world.GetComponent<InventoryComponent>(entity_id);
    uint32_t total_quantity = 0;
    
    for (const auto& item : inventory.items) {
        if (item.instance.item_id == item_id) {
            total_quantity += item.quantity;
        }
    }
    
    return total_quantity >= quantity;
}

} // namespace mmorpg::game::systems