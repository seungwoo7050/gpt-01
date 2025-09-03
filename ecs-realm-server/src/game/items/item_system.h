#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <optional>
#include <variant>

namespace mmorpg::game::items {

// [SEQUENCE: 1077] Item system
// 아이템 정의, 인벤토리 관리, 장비 시스템을 포함하는 종합 시스템

// [SEQUENCE: 1078] Item types
enum class ItemType {
    EQUIPMENT,      // 장비 아이템
    CONSUMABLE,     // 소비 아이템
    MATERIAL,       // 재료 아이템
    QUEST,          // 퀘스트 아이템
    CURRENCY,       // 화폐
    MISC            // 기타
};

// [SEQUENCE: 1079] Equipment types
enum class EquipmentType {
    NONE,
    // Weapons
    SWORD_1H,       // 한손검
    SWORD_2H,       // 양손검
    DAGGER,         // 단검
    BOW,            // 활
    STAFF,          // 지팡이
    WAND,           // 마법봉
    SHIELD,         // 방패
    
    // Armor
    HELMET,         // 투구
    CHEST,          // 갑옷
    LEGS,           // 다리
    BOOTS,          // 신발
    GLOVES,         // 장갑
    
    // Accessories
    NECKLACE,       // 목걸이
    RING,           // 반지
    TRINKET         // 장신구
};

// [SEQUENCE: 1080] Item rarity
enum class ItemRarity {
    COMMON,         // 일반 (회색)
    UNCOMMON,       // 고급 (녹색)
    RARE,           // 희귀 (파란색)
    EPIC,           // 영웅 (보라색)
    LEGENDARY,      // 전설 (주황색)
    MYTHIC          // 신화 (빨간색)
};

// [SEQUENCE: 1081] Item binding
enum class ItemBinding {
    NONE,           // 거래 가능
    BIND_ON_PICKUP, // 획득 시 귀속
    BIND_ON_EQUIP,  // 장착 시 귀속
    BIND_ON_USE     // 사용 시 귀속
};

// [SEQUENCE: 1082] Item stats
struct ItemStats {
    // Primary stats
    int32_t strength = 0;
    int32_t agility = 0;
    int32_t intelligence = 0;
    int32_t stamina = 0;
    
    // Secondary stats
    int32_t attack_power = 0;
    int32_t spell_power = 0;
    int32_t armor = 0;
    int32_t magic_resist = 0;
    
    // Additional stats
    float critical_chance = 0.0f;
    float critical_damage = 0.0f;
    float attack_speed = 0.0f;
    float movement_speed = 0.0f;
    
    // Resistances
    std::unordered_map<std::string, int32_t> resistances;
};

// [SEQUENCE: 1083] Item requirements
struct ItemRequirements {
    uint32_t level = 1;
    uint32_t strength = 0;
    uint32_t agility = 0;
    uint32_t intelligence = 0;
    
    std::vector<uint32_t> required_classes;    // Class IDs
    std::vector<uint32_t> required_skills;     // Skill IDs
    std::vector<uint32_t> required_quests;     // Quest IDs
};

// [SEQUENCE: 1084] Item data definition
struct ItemData {
    uint32_t item_id = 0;
    std::string name;
    std::string description;
    
    // Basic properties
    ItemType type = ItemType::MISC;
    EquipmentType equipment_type = EquipmentType::NONE;
    ItemRarity rarity = ItemRarity::COMMON;
    ItemBinding binding = ItemBinding::NONE;
    
    // Stack and durability
    uint32_t max_stack = 1;
    uint32_t max_durability = 0;    // 0 = indestructible
    
    // Value
    uint64_t buy_price = 0;
    uint64_t sell_price = 0;
    
    // Requirements
    ItemRequirements requirements;
    
    // Stats (for equipment)
    ItemStats stats;
    
    // Use effect (for consumables)
    uint32_t use_effect_id = 0;     // Status effect ID
    float use_cooldown = 0.0f;
    
    // Visual (client hints)
    std::string icon_name;
    std::string model_name;
    uint32_t display_id = 0;
};

// [SEQUENCE: 1085] Item instance
struct ItemInstance {
    uint64_t instance_id = 0;       // Unique instance ID
    uint32_t item_id = 0;           // Item template ID
    uint32_t stack_count = 1;
    uint32_t current_durability = 0;
    
    // Binding
    bool is_bound = false;
    uint64_t bound_to = 0;          // Player ID
    
    // Enhancements
    uint32_t enchantment_id = 0;
    std::vector<uint32_t> gem_ids;
    
    // Random stats (for random-rolled items)
    std::optional<ItemStats> random_stats;
    
    // Metadata
    std::chrono::steady_clock::time_point created_time;
    std::unordered_map<std::string, std::string> custom_data;
};

// [SEQUENCE: 1086] Inventory slot
struct InventorySlot {
    std::optional<ItemInstance> item;
    bool is_locked = false;         // Cannot be modified
};

// [SEQUENCE: 1087] Equipment slots
enum class EquipmentSlot {
    HEAD,
    NECK,
    SHOULDERS,
    CHEST,
    WAIST,
    LEGS,
    FEET,
    WRISTS,
    HANDS,
    FINGER_1,
    FINGER_2,
    TRINKET_1,
    TRINKET_2,
    MAIN_HAND,
    OFF_HAND,
    RANGED,
    TABARD,
    SHIRT,
    MAX_SLOTS
};

// [SEQUENCE: 1088] Inventory manager
class InventoryManager {
public:
    InventoryManager(uint64_t owner_id, size_t bag_slots = 20)
        : owner_id_(owner_id), bag_slots_(bag_slots) {
        inventory_.resize(bag_slots_);
        equipment_.resize(static_cast<size_t>(EquipmentSlot::MAX_SLOTS));
    }
    
    // [SEQUENCE: 1089] Add items
    std::vector<ItemInstance> AddItems(const std::vector<ItemInstance>& items);
    bool AddItem(ItemInstance& item);
    
    // [SEQUENCE: 1090] Remove items
    bool RemoveItem(size_t slot_index, uint32_t count = 1);
    bool RemoveItemById(uint32_t item_id, uint32_t count = 1);
    
    // [SEQUENCE: 1091] Move items
    bool MoveItem(size_t from_slot, size_t to_slot);
    bool SplitStack(size_t slot_index, uint32_t split_count);
    bool MergeStacks(size_t from_slot, size_t to_slot);
    
    // [SEQUENCE: 1092] Equipment management
    bool EquipItem(size_t bag_slot);
    bool UnequipItem(EquipmentSlot equipment_slot);
    bool SwapEquipment(size_t bag_slot, EquipmentSlot equipment_slot);
    
    // [SEQUENCE: 1093] Query inventory
    std::optional<ItemInstance> GetItem(size_t slot_index) const;
    std::optional<ItemInstance> GetEquippedItem(EquipmentSlot slot) const;
    uint32_t GetItemCount(uint32_t item_id) const;
    bool HasItem(uint32_t item_id, uint32_t count = 1) const;
    
    // [SEQUENCE: 1094] Find items
    std::optional<size_t> FindFirstEmptySlot() const;
    std::vector<size_t> FindItemSlots(uint32_t item_id) const;
    
    // [SEQUENCE: 1095] Inventory info
    size_t GetUsedSlots() const;
    size_t GetFreeSlots() const;
    bool IsFull() const { return GetFreeSlots() == 0; }
    
    // [SEQUENCE: 1096] Calculate total stats from equipment
    ItemStats CalculateEquipmentStats() const;
    
private:
    uint64_t owner_id_;
    size_t bag_slots_;
    
    // Storage
    std::vector<InventorySlot> inventory_;
    std::vector<InventorySlot> equipment_;
    
    // [SEQUENCE: 1097] Helper functions
    bool CanEquipItem(const ItemData& item_data, EquipmentSlot slot) const;
    EquipmentSlot GetEquipmentSlotForItem(const ItemData& item_data) const;
    bool CheckRequirements(const ItemData& item_data) const;
};

// [SEQUENCE: 1098] Item manager (global)
class ItemManager {
public:
    static ItemManager& Instance() {
        static ItemManager instance;
        return instance;
    }
    
    // [SEQUENCE: 1099] Item data management
    void RegisterItem(const ItemData& item_data);
    const ItemData* GetItemData(uint32_t item_id) const;
    
    // [SEQUENCE: 1100] Create item instances
    ItemInstance CreateItemInstance(uint32_t item_id, uint32_t count = 1);
    std::vector<ItemInstance> CreateLoot(uint32_t loot_table_id);
    
    // [SEQUENCE: 1101] Item operations
    bool UseItem(uint64_t player_id, const ItemInstance& item);
    bool RepairItem(ItemInstance& item, float repair_amount = 1.0f);
    bool EnchantItem(ItemInstance& item, uint32_t enchantment_id);
    
    // [SEQUENCE: 1102] Trading and binding
    bool CanTrade(const ItemInstance& item) const;
    void BindItem(ItemInstance& item, uint64_t player_id);
    
    // [SEQUENCE: 1103] Generate random stats
    ItemStats GenerateRandomStats(ItemRarity rarity, uint32_t item_level);
    
private:
    ItemManager() = default;
    
    // Item database
    std::unordered_map<uint32_t, ItemData> item_database_;
    
    // Instance tracking
    std::atomic<uint64_t> next_instance_id_{1};
    
    // [SEQUENCE: 1104] Loot tables
    struct LootEntry {
        uint32_t item_id;
        float chance;           // 0.0 - 1.0
        uint32_t min_count;
        uint32_t max_count;
    };
    std::unordered_map<uint32_t, std::vector<LootEntry>> loot_tables_;
};

// [SEQUENCE: 1105] Item factory
class ItemFactory {
public:
    // [SEQUENCE: 1106] Create weapon
    static ItemData CreateWeapon(
        uint32_t item_id,
        const std::string& name,
        EquipmentType type,
        ItemRarity rarity,
        int32_t damage,
        float attack_speed
    );
    
    // [SEQUENCE: 1107] Create armor
    static ItemData CreateArmor(
        uint32_t item_id,
        const std::string& name,
        EquipmentType type,
        ItemRarity rarity,
        int32_t armor,
        int32_t stamina
    );
    
    // [SEQUENCE: 1108] Create consumable
    static ItemData CreateConsumable(
        uint32_t item_id,
        const std::string& name,
        uint32_t effect_id,
        uint32_t max_stack,
        float cooldown
    );
    
    // [SEQUENCE: 1109] Create quest item
    static ItemData CreateQuestItem(
        uint32_t item_id,
        const std::string& name,
        const std::string& description,
        uint32_t quest_id
    );
};

// [SEQUENCE: 1110] Common item IDs
namespace CommonItems {
    // Currencies
    constexpr uint32_t GOLD = 1;
    constexpr uint32_t SILVER = 2;
    constexpr uint32_t COPPER = 3;
    
    // Consumables
    constexpr uint32_t HEALTH_POTION_SMALL = 100;
    constexpr uint32_t HEALTH_POTION_LARGE = 101;
    constexpr uint32_t MANA_POTION_SMALL = 102;
    constexpr uint32_t MANA_POTION_LARGE = 103;
    
    // Materials
    constexpr uint32_t IRON_ORE = 200;
    constexpr uint32_t LEATHER = 201;
    constexpr uint32_t CLOTH = 202;
}

} // namespace mmorpg::game::items