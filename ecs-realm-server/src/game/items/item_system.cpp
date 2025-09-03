#include "item_system.h"
#include "../status/status_effect_system.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <random>

namespace mmorpg::game::items {

// [SEQUENCE: 1111] Random generator for item stats
static thread_local std::mt19937 item_rng(std::chrono::steady_clock::now().time_since_epoch().count());

// [SEQUENCE: 1112] InventoryManager - Add items
std::vector<ItemInstance> InventoryManager::AddItems(const std::vector<ItemInstance>& items) {
    std::vector<ItemInstance> remaining_items;
    
    for (auto item : items) {
        if (!AddItem(item)) {
            remaining_items.push_back(item);
        }
    }
    
    return remaining_items;
}

// [SEQUENCE: 1113] InventoryManager - Add single item
bool InventoryManager::AddItem(ItemInstance& item) {
    const auto* item_data = ItemManager::Instance().GetItemData(item.item_id);
    if (!item_data) {
        spdlog::warn("Attempt to add unknown item: {}", item.item_id);
        return false;
    }
    
    // [SEQUENCE: 1114] Try to stack with existing items
    if (item_data->max_stack > 1) {
        auto slots = FindItemSlots(item.item_id);
        for (size_t slot : slots) {
            if (inventory_[slot].item && 
                inventory_[slot].item->stack_count < item_data->max_stack) {
                
                uint32_t can_add = item_data->max_stack - inventory_[slot].item->stack_count;
                uint32_t to_add = std::min(can_add, item.stack_count);
                
                inventory_[slot].item->stack_count += to_add;
                item.stack_count -= to_add;
                
                if (item.stack_count == 0) {
                    return true;
                }
            }
        }
    }
    
    // [SEQUENCE: 1115] Add to empty slot
    while (item.stack_count > 0) {
        auto empty_slot = FindFirstEmptySlot();
        if (!empty_slot) {
            return false; // Inventory full
        }
        
        uint32_t stack_size = std::min(item.stack_count, item_data->max_stack);
        
        ItemInstance new_stack = item;
        new_stack.stack_count = stack_size;
        inventory_[*empty_slot].item = new_stack;
        
        item.stack_count -= stack_size;
    }
    
    return true;
}

// [SEQUENCE: 1116] InventoryManager - Remove item
bool InventoryManager::RemoveItem(size_t slot_index, uint32_t count) {
    if (slot_index >= inventory_.size() || !inventory_[slot_index].item) {
        return false;
    }
    
    auto& slot = inventory_[slot_index];
    if (slot.item->stack_count < count) {
        return false;
    }
    
    slot.item->stack_count -= count;
    if (slot.item->stack_count == 0) {
        slot.item = std::nullopt;
    }
    
    return true;
}

// [SEQUENCE: 1117] InventoryManager - Remove item by ID
bool InventoryManager::RemoveItemById(uint32_t item_id, uint32_t count) {
    uint32_t removed = 0;
    auto slots = FindItemSlots(item_id);
    
    for (size_t slot : slots) {
        if (removed >= count) break;
        
        uint32_t to_remove = std::min(count - removed, inventory_[slot].item->stack_count);
        if (RemoveItem(slot, to_remove)) {
            removed += to_remove;
        }
    }
    
    return removed == count;
}

// [SEQUENCE: 1118] InventoryManager - Move item
bool InventoryManager::MoveItem(size_t from_slot, size_t to_slot) {
    if (from_slot >= inventory_.size() || to_slot >= inventory_.size()) {
        return false;
    }
    
    if (!inventory_[from_slot].item) {
        return false;
    }
    
    // Simple swap
    std::swap(inventory_[from_slot].item, inventory_[to_slot].item);
    return true;
}

// [SEQUENCE: 1119] InventoryManager - Split stack
bool InventoryManager::SplitStack(size_t slot_index, uint32_t split_count) {
    if (slot_index >= inventory_.size() || !inventory_[slot_index].item) {
        return false;
    }
    
    auto& source_item = inventory_[slot_index].item;
    if (source_item->stack_count <= split_count) {
        return false;
    }
    
    auto empty_slot = FindFirstEmptySlot();
    if (!empty_slot) {
        return false;
    }
    
    // Create new stack
    ItemInstance new_stack = *source_item;
    new_stack.stack_count = split_count;
    inventory_[*empty_slot].item = new_stack;
    
    // Reduce source stack
    source_item->stack_count -= split_count;
    
    return true;
}

// [SEQUENCE: 1120] InventoryManager - Equip item
bool InventoryManager::EquipItem(size_t bag_slot) {
    if (bag_slot >= inventory_.size() || !inventory_[bag_slot].item) {
        return false;
    }
    
    const auto& item = inventory_[bag_slot].item;
    const auto* item_data = ItemManager::Instance().GetItemData(item->item_id);
    if (!item_data || item_data->type != ItemType::EQUIPMENT) {
        return false;
    }
    
    // [SEQUENCE: 1121] Check requirements
    if (!CheckRequirements(*item_data)) {
        spdlog::debug("Player {} does not meet requirements for item {}", 
                     owner_id_, item_data->name);
        return false;
    }
    
    // [SEQUENCE: 1122] Find equipment slot
    EquipmentSlot equip_slot = GetEquipmentSlotForItem(*item_data);
    if (equip_slot == EquipmentSlot::MAX_SLOTS) {
        return false;
    }
    
    // [SEQUENCE: 1123] Handle existing equipment
    if (equipment_[static_cast<size_t>(equip_slot)].item) {
        // Swap items
        std::swap(inventory_[bag_slot].item, 
                 equipment_[static_cast<size_t>(equip_slot)].item);
    } else {
        // Move to equipment slot
        equipment_[static_cast<size_t>(equip_slot)].item = inventory_[bag_slot].item;
        inventory_[bag_slot].item = std::nullopt;
    }
    
    // Bind on equip
    if (item_data->binding == ItemBinding::BIND_ON_EQUIP) {
        ItemManager::Instance().BindItem(*equipment_[static_cast<size_t>(equip_slot)].item, owner_id_);
    }
    
    spdlog::debug("Player {} equipped {}", owner_id_, item_data->name);
    return true;
}

// [SEQUENCE: 1124] InventoryManager - Unequip item
bool InventoryManager::UnequipItem(EquipmentSlot equipment_slot) {
    size_t slot_index = static_cast<size_t>(equipment_slot);
    if (slot_index >= equipment_.size() || !equipment_[slot_index].item) {
        return false;
    }
    
    auto empty_slot = FindFirstEmptySlot();
    if (!empty_slot) {
        return false; // Inventory full
    }
    
    inventory_[*empty_slot].item = equipment_[slot_index].item;
    equipment_[slot_index].item = std::nullopt;
    
    return true;
}

// [SEQUENCE: 1125] InventoryManager - Get item count
uint32_t InventoryManager::GetItemCount(uint32_t item_id) const {
    uint32_t total_count = 0;
    
    for (const auto& slot : inventory_) {
        if (slot.item && slot.item->item_id == item_id) {
            total_count += slot.item->stack_count;
        }
    }
    
    return total_count;
}

// [SEQUENCE: 1126] InventoryManager - Find empty slot
std::optional<size_t> InventoryManager::FindFirstEmptySlot() const {
    for (size_t i = 0; i < inventory_.size(); ++i) {
        if (!inventory_[i].item && !inventory_[i].is_locked) {
            return i;
        }
    }
    return std::nullopt;
}

// [SEQUENCE: 1127] InventoryManager - Find item slots
std::vector<size_t> InventoryManager::FindItemSlots(uint32_t item_id) const {
    std::vector<size_t> slots;
    
    for (size_t i = 0; i < inventory_.size(); ++i) {
        if (inventory_[i].item && inventory_[i].item->item_id == item_id) {
            slots.push_back(i);
        }
    }
    
    return slots;
}

// [SEQUENCE: 1128] InventoryManager - Calculate equipment stats
ItemStats InventoryManager::CalculateEquipmentStats() const {
    ItemStats total_stats;
    
    for (const auto& slot : equipment_) {
        if (!slot.item) continue;
        
        const auto* item_data = ItemManager::Instance().GetItemData(slot.item->item_id);
        if (!item_data) continue;
        
        // Add base stats
        total_stats.strength += item_data->stats.strength;
        total_stats.agility += item_data->stats.agility;
        total_stats.intelligence += item_data->stats.intelligence;
        total_stats.stamina += item_data->stats.stamina;
        
        total_stats.attack_power += item_data->stats.attack_power;
        total_stats.spell_power += item_data->stats.spell_power;
        total_stats.armor += item_data->stats.armor;
        total_stats.magic_resist += item_data->stats.magic_resist;
        
        total_stats.critical_chance += item_data->stats.critical_chance;
        total_stats.critical_damage += item_data->stats.critical_damage;
        total_stats.attack_speed += item_data->stats.attack_speed;
        total_stats.movement_speed += item_data->stats.movement_speed;
        
        // Add random stats if present
        if (slot.item->random_stats) {
            const auto& random = *slot.item->random_stats;
            total_stats.strength += random.strength;
            total_stats.agility += random.agility;
            total_stats.intelligence += random.intelligence;
            total_stats.stamina += random.stamina;
            // ... etc
        }
    }
    
    return total_stats;
}

// [SEQUENCE: 1129] InventoryManager - Get equipment slot for item
EquipmentSlot InventoryManager::GetEquipmentSlotForItem(const ItemData& item_data) const {
    switch (item_data.equipment_type) {
        case EquipmentType::HELMET: return EquipmentSlot::HEAD;
        case EquipmentType::CHEST: return EquipmentSlot::CHEST;
        case EquipmentType::LEGS: return EquipmentSlot::LEGS;
        case EquipmentType::BOOTS: return EquipmentSlot::FEET;
        case EquipmentType::GLOVES: return EquipmentSlot::HANDS;
        case EquipmentType::NECKLACE: return EquipmentSlot::NECK;
        case EquipmentType::RING: return EquipmentSlot::FINGER_1; // TODO: Handle second ring
        case EquipmentType::SWORD_1H:
        case EquipmentType::DAGGER:
        case EquipmentType::WAND: return EquipmentSlot::MAIN_HAND;
        case EquipmentType::SHIELD: return EquipmentSlot::OFF_HAND;
        case EquipmentType::SWORD_2H:
        case EquipmentType::STAFF:
        case EquipmentType::BOW: return EquipmentSlot::MAIN_HAND; // Two-handed
        default: return EquipmentSlot::MAX_SLOTS;
    }
}

// [SEQUENCE: 1130] InventoryManager - Check requirements
bool InventoryManager::CheckRequirements(const ItemData& item_data) const {
    // TODO: Get player stats and check against requirements
    // For now, always return true
    return true;
}

// [SEQUENCE: 1131] ItemManager - Register item
void ItemManager::RegisterItem(const ItemData& item_data) {
    item_database_[item_data.item_id] = item_data;
    spdlog::info("Registered item: {} (ID: {})", item_data.name, item_data.item_id);
}

// [SEQUENCE: 1132] ItemManager - Get item data
const ItemData* ItemManager::GetItemData(uint32_t item_id) const {
    auto it = item_database_.find(item_id);
    return (it != item_database_.end()) ? &it->second : nullptr;
}

// [SEQUENCE: 1133] ItemManager - Create item instance
ItemInstance ItemManager::CreateItemInstance(uint32_t item_id, uint32_t count) {
    ItemInstance instance;
    instance.instance_id = next_instance_id_++;
    instance.item_id = item_id;
    instance.stack_count = count;
    instance.created_time = std::chrono::steady_clock::now();
    
    const auto* item_data = GetItemData(item_id);
    if (item_data) {
        instance.current_durability = item_data->max_durability;
        
        // Bind on pickup
        if (item_data->binding == ItemBinding::BIND_ON_PICKUP) {
            instance.is_bound = true;
            // bound_to will be set when picked up
        }
    }
    
    return instance;
}

// [SEQUENCE: 1134] ItemManager - Create loot
std::vector<ItemInstance> ItemManager::CreateLoot(uint32_t loot_table_id) {
    std::vector<ItemInstance> loot;
    
    auto table_it = loot_tables_.find(loot_table_id);
    if (table_it == loot_tables_.end()) {
        return loot;
    }
    
    std::uniform_real_distribution<float> roll_dist(0.0f, 1.0f);
    std::uniform_int_distribution<uint32_t> count_dist;
    
    for (const auto& entry : table_it->second) {
        if (roll_dist(item_rng) <= entry.chance) {
            count_dist = std::uniform_int_distribution<uint32_t>(entry.min_count, entry.max_count);
            uint32_t count = count_dist(item_rng);
            
            loot.push_back(CreateItemInstance(entry.item_id, count));
        }
    }
    
    return loot;
}

// [SEQUENCE: 1135] ItemManager - Use item
bool ItemManager::UseItem(uint64_t player_id, const ItemInstance& item) {
    const auto* item_data = GetItemData(item.item_id);
    if (!item_data || item_data->type != ItemType::CONSUMABLE) {
        return false;
    }
    
    if (item_data->use_effect_id > 0) {
        // Apply status effect
        status::StatusEffectManager::Instance().ApplyEffect(
            player_id, item_data->use_effect_id, 0
        );
    }
    
    // Bind on use
    if (item_data->binding == ItemBinding::BIND_ON_USE) {
        // Note: This would need to update the actual instance
    }
    
    spdlog::debug("Player {} used item {}", player_id, item_data->name);
    return true;
}

// [SEQUENCE: 1136] ItemManager - Generate random stats
ItemStats ItemManager::GenerateRandomStats(ItemRarity rarity, uint32_t item_level) {
    ItemStats stats;
    
    // Number of random stats based on rarity
    int stat_count = 0;
    switch (rarity) {
        case ItemRarity::UNCOMMON: stat_count = 1; break;
        case ItemRarity::RARE: stat_count = 2; break;
        case ItemRarity::EPIC: stat_count = 3; break;
        case ItemRarity::LEGENDARY: stat_count = 4; break;
        case ItemRarity::MYTHIC: stat_count = 5; break;
        default: return stats;
    }
    
    // Random stat distribution
    std::uniform_int_distribution<int> stat_type_dist(0, 3);
    std::uniform_int_distribution<int> stat_value_dist(item_level / 2, item_level * 2);
    
    for (int i = 0; i < stat_count; ++i) {
        int stat_type = stat_type_dist(item_rng);
        int value = stat_value_dist(item_rng);
        
        switch (stat_type) {
            case 0: stats.strength += value; break;
            case 1: stats.agility += value; break;
            case 2: stats.intelligence += value; break;
            case 3: stats.stamina += value; break;
        }
    }
    
    return stats;
}

// [SEQUENCE: 1137] ItemFactory - Create weapon
ItemData ItemFactory::CreateWeapon(
    uint32_t item_id,
    const std::string& name,
    EquipmentType type,
    ItemRarity rarity,
    int32_t damage,
    float attack_speed) {
    
    ItemData item;
    item.item_id = item_id;
    item.name = name;
    item.type = ItemType::EQUIPMENT;
    item.equipment_type = type;
    item.rarity = rarity;
    item.max_durability = 100;
    
    // Base damage as attack power
    item.stats.attack_power = damage;
    item.stats.attack_speed = attack_speed;
    
    // Rarity bonuses
    switch (rarity) {
        case ItemRarity::UNCOMMON:
            item.stats.strength = 5;
            break;
        case ItemRarity::RARE:
            item.stats.strength = 10;
            item.stats.critical_chance = 0.02f;
            break;
        case ItemRarity::EPIC:
            item.stats.strength = 15;
            item.stats.critical_chance = 0.03f;
            item.stats.critical_damage = 0.1f;
            break;
        case ItemRarity::LEGENDARY:
            item.stats.strength = 20;
            item.stats.critical_chance = 0.05f;
            item.stats.critical_damage = 0.2f;
            item.stats.attack_speed = 0.1f;
            break;
        case ItemRarity::MYTHIC:
            item.stats.strength = 30;
            item.stats.critical_chance = 0.08f;
            item.stats.critical_damage = 0.3f;
            item.stats.attack_speed = 0.15f;
            break;
        default:
            break;
    }
    
    return item;
}

// [SEQUENCE: 1138] ItemFactory - Create armor
ItemData ItemFactory::CreateArmor(
    uint32_t item_id,
    const std::string& name,
    EquipmentType type,
    ItemRarity rarity,
    int32_t armor,
    int32_t stamina) {
    
    ItemData item;
    item.item_id = item_id;
    item.name = name;
    item.type = ItemType::EQUIPMENT;
    item.equipment_type = type;
    item.rarity = rarity;
    item.max_durability = 100;
    
    // Base defense stats
    item.stats.armor = armor;
    item.stats.stamina = stamina;
    
    // Rarity bonuses
    switch (rarity) {
        case ItemRarity::UNCOMMON:
            item.stats.stamina += 5;
            break;
        case ItemRarity::RARE:
            item.stats.stamina += 10;
            item.stats.magic_resist = 10;
            break;
        case ItemRarity::EPIC:
            item.stats.stamina += 15;
            item.stats.magic_resist = 20;
            item.stats.armor += armor * 0.1f;
            break;
        case ItemRarity::LEGENDARY:
            item.stats.stamina += 25;
            item.stats.magic_resist = 30;
            item.stats.armor += armor * 0.2f;
            item.stats.movement_speed = 0.05f;
            break;
        case ItemRarity::MYTHIC:
            item.stats.stamina += 40;
            item.stats.magic_resist = 50;
            item.stats.armor += armor * 0.3f;
            item.stats.movement_speed = 0.1f;
            break;
        default:
            break;
    }
    
    return item;
}

// [SEQUENCE: 1139] ItemFactory - Create consumable
ItemData ItemFactory::CreateConsumable(
    uint32_t item_id,
    const std::string& name,
    uint32_t effect_id,
    uint32_t max_stack,
    float cooldown) {
    
    ItemData item;
    item.item_id = item_id;
    item.name = name;
    item.type = ItemType::CONSUMABLE;
    item.max_stack = max_stack;
    item.use_effect_id = effect_id;
    item.use_cooldown = cooldown;
    
    // Set default prices
    item.buy_price = 50;
    item.sell_price = 10;
    
    return item;
}

// [SEQUENCE: 1140] ItemFactory - Create quest item
ItemData ItemFactory::CreateQuestItem(
    uint32_t item_id,
    const std::string& name,
    const std::string& description,
    uint32_t quest_id) {
    
    ItemData item;
    item.item_id = item_id;
    item.name = name;
    item.description = description;
    item.type = ItemType::QUEST;
    item.binding = ItemBinding::BIND_ON_PICKUP;
    item.max_stack = 1;
    
    // Quest items cannot be sold
    item.buy_price = 0;
    item.sell_price = 0;
    
    // Store quest ID in requirements (abuse of field)
    item.requirements.required_quests.push_back(quest_id);
    
    return item;
}

// [SEQUENCE: 1141] ItemManager - Bind item
void ItemManager::BindItem(ItemInstance& item, uint64_t player_id) {
    item.is_bound = true;
    item.bound_to = player_id;
    
    spdlog::debug("Item {} bound to player {}", item.instance_id, player_id);
}

// [SEQUENCE: 1142] ItemManager - Can trade
bool ItemManager::CanTrade(const ItemInstance& item) const {
    if (item.is_bound) {
        return false;
    }
    
    const auto* item_data = GetItemData(item.item_id);
    if (!item_data) {
        return false;
    }
    
    // Quest items cannot be traded
    if (item_data->type == ItemType::QUEST) {
        return false;
    }
    
    // Check binding type
    if (item_data->binding != ItemBinding::NONE) {
        return false;
    }
    
    return true;
}

// [SEQUENCE: 1143] ItemManager - Repair item
bool ItemManager::RepairItem(ItemInstance& item, float repair_amount) {
    const auto* item_data = GetItemData(item.item_id);
    if (!item_data || item_data->max_durability == 0) {
        return false;
    }
    
    item.current_durability = std::min(
        item.current_durability + static_cast<uint32_t>(item_data->max_durability * repair_amount),
        item_data->max_durability
    );
    
    return true;
}

// [SEQUENCE: 1144] ItemManager - Enchant item
bool ItemManager::EnchantItem(ItemInstance& item, uint32_t enchantment_id) {
    const auto* item_data = GetItemData(item.item_id);
    if (!item_data || item_data->type != ItemType::EQUIPMENT) {
        return false;
    }
    
    // TODO: Validate enchantment is appropriate for item type
    item.enchantment_id = enchantment_id;
    
    spdlog::info("Item {} enchanted with enchantment {}", item.instance_id, enchantment_id);
    return true;
}

// [SEQUENCE: 1145] InventoryManager - Merge stacks
bool InventoryManager::MergeStacks(size_t from_slot, size_t to_slot) {
    if (from_slot >= inventory_.size() || to_slot >= inventory_.size()) {
        return false;
    }
    
    auto& from_item = inventory_[from_slot].item;
    auto& to_item = inventory_[to_slot].item;
    
    if (!from_item || !to_item) {
        return false;
    }
    
    // Must be same item
    if (from_item->item_id != to_item->item_id) {
        return false;
    }
    
    const auto* item_data = ItemManager::Instance().GetItemData(from_item->item_id);
    if (!item_data || item_data->max_stack <= 1) {
        return false;
    }
    
    // Calculate how many can be moved
    uint32_t space_available = item_data->max_stack - to_item->stack_count;
    uint32_t to_move = std::min(space_available, from_item->stack_count);
    
    if (to_move == 0) {
        return false;
    }
    
    // Move items
    to_item->stack_count += to_move;
    from_item->stack_count -= to_move;
    
    // Remove empty slot
    if (from_item->stack_count == 0) {
        from_item = std::nullopt;
    }
    
    return true;
}

// [SEQUENCE: 1146] InventoryManager - Get item
std::optional<ItemInstance> InventoryManager::GetItem(size_t slot_index) const {
    if (slot_index >= inventory_.size()) {
        return std::nullopt;
    }
    return inventory_[slot_index].item;
}

// [SEQUENCE: 1147] InventoryManager - Get equipped item
std::optional<ItemInstance> InventoryManager::GetEquippedItem(EquipmentSlot slot) const {
    size_t index = static_cast<size_t>(slot);
    if (index >= equipment_.size()) {
        return std::nullopt;
    }
    return equipment_[index].item;
}

// [SEQUENCE: 1148] InventoryManager - Has item
bool InventoryManager::HasItem(uint32_t item_id, uint32_t count) const {
    return GetItemCount(item_id) >= count;
}

// [SEQUENCE: 1149] InventoryManager - Get used slots
size_t InventoryManager::GetUsedSlots() const {
    size_t used = 0;
    for (const auto& slot : inventory_) {
        if (slot.item) {
            used++;
        }
    }
    return used;
}

// [SEQUENCE: 1150] InventoryManager - Get free slots
size_t InventoryManager::GetFreeSlots() const {
    return bag_slots_ - GetUsedSlots();
}

// [SEQUENCE: 1151] InventoryManager - Swap equipment
bool InventoryManager::SwapEquipment(size_t bag_slot, EquipmentSlot equipment_slot) {
    if (bag_slot >= inventory_.size()) {
        return false;
    }
    
    size_t equip_index = static_cast<size_t>(equipment_slot);
    if (equip_index >= equipment_.size()) {
        return false;
    }
    
    // Simple swap
    std::swap(inventory_[bag_slot].item, equipment_[equip_index].item);
    return true;
}

// [SEQUENCE: 1152] InventoryManager - Can equip item
bool InventoryManager::CanEquipItem(const ItemData& item_data, EquipmentSlot slot) const {
    // Check if item type matches slot
    EquipmentSlot expected_slot = GetEquipmentSlotForItem(item_data);
    if (expected_slot != slot) {
        return false;
    }
    
    // Check requirements
    return CheckRequirements(item_data);
}

} // namespace mmorpg::game::items