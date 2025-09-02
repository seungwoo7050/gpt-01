#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <unordered_map>
#include <set>
#include "player_housing.h"
#include "../inventory/inventory_system.h"
#include "../core/types.h"

namespace mmorpg::housing {

// [SEQUENCE: 3456] Housing storage types
enum class HousingStorageType {
    PERSONAL_CHEST,     // 개인 상자
    SHARED_STORAGE,     // 공유 저장소
    WARDROBE,          // 옷장
    DISPLAY_CASE,      // 전시 케이스
    BANK_VAULT,        // 은행 금고
    CRAFTING_STORAGE,  // 제작 재료 보관함
    GARDEN_SHED,       // 정원 창고
    MAGICAL_CHEST      // 마법 상자
};

// [SEQUENCE: 3457] Storage container properties
struct StorageContainerProperties {
    HousingStorageType type;
    std::string name;
    uint32_t base_capacity{20};
    
    // Storage restrictions
    enum class RestrictionType {
        NONE,           // 제한 없음
        EQUIPMENT_ONLY, // 장비만
        MATERIALS_ONLY, // 재료만
        CONSUMABLES_ONLY, // 소모품만
        VALUABLES_ONLY, // 귀중품만
        SPECIFIC_TYPES  // 특정 유형만
    } restriction{RestrictionType::NONE};
    
    std::vector<uint32_t> allowed_item_types;  // For SPECIFIC_TYPES
    
    // Features
    bool auto_sort{false};
    bool preserve_quality{false};  // Items don't decay
    bool shared_access{false};
    bool linked_storage{false};    // Access from other houses
    
    // Security
    bool requires_key{false};
    uint32_t lock_difficulty{0};
    bool trap_enabled{false};
};

// [SEQUENCE: 3458] Storage container instance
class HousingStorageContainer {
public:
    // [SEQUENCE: 3459] Container management
    HousingStorageContainer(uint64_t container_id,
                           const StorageContainerProperties& props);
    
    // Item operations
    bool CanStoreItem(uint32_t item_id, uint32_t quantity) const;
    bool AddItem(uint32_t item_id, uint32_t quantity, 
                 const ItemProperties& properties);
    bool RemoveItem(uint32_t item_id, uint32_t quantity);
    bool TransferItem(uint32_t item_id, uint32_t quantity,
                     HousingStorageContainer& target);
    
    // Container queries
    uint32_t GetUsedSlots() const;
    uint32_t GetTotalCapacity() const { return current_capacity_; }
    float GetWeightUsage() const;
    
    // [SEQUENCE: 3460] Item search and filtering
    struct StoredItem {
        uint32_t item_id;
        uint32_t quantity;
        ItemProperties properties;
        std::chrono::system_clock::time_point stored_date;
        uint64_t stored_by_player_id;
    };
    
    std::vector<StoredItem> GetAllItems() const;
    std::vector<StoredItem> SearchItems(const std::string& name_filter) const;
    std::vector<StoredItem> GetItemsByType(uint32_t item_type) const;
    StoredItem* FindItem(uint32_t item_id);
    
    // Sorting and organization
    void AutoSort();
    void CompactStorage();
    void MergeStacks();
    
    // Security
    bool IsLocked() const { return is_locked_; }
    bool Unlock(uint32_t key_item_id);
    bool CheckTrap(uint64_t player_id);
    
    // Capacity upgrades
    void UpgradeCapacity(uint32_t additional_slots);
    
private:
    uint64_t container_id_;
    StorageContainerProperties properties_;
    
    std::unordered_map<uint32_t, StoredItem> stored_items_;
    uint32_t current_capacity_;
    float current_weight_{0.0f};
    float max_weight_{1000.0f};
    
    bool is_locked_{false};
    bool trap_triggered_{false};
    
    // Access tracking
    std::chrono::system_clock::time_point last_accessed_;
    uint64_t last_accessed_by_;
    
    // Helper methods
    bool ValidateItemRestrictions(uint32_t item_id) const;
    void UpdateWeight();
};

// [SEQUENCE: 3461] Storage room management
class HousingStorageRoom {
public:
    struct StorageRoomConfig {
        uint32_t max_containers{10};
        float temperature_control{20.0f};  // For perishables
        float humidity_control{50.0f};
        bool climate_controlled{false};
        bool security_enhanced{false};
        
        // Special features
        bool time_frozen{false};      // Items don't decay
        bool dimension_expanded{false}; // Extra space
        bool auto_organize{false};
    };
    
    // [SEQUENCE: 3462] Room management
    HousingStorageRoom(uint64_t room_id, const StorageRoomConfig& config);
    
    // Container management
    bool AddContainer(std::shared_ptr<HousingStorageContainer> container,
                     const Vector3& position);
    bool RemoveContainer(uint64_t container_id);
    bool MoveContainer(uint64_t container_id, const Vector3& new_position);
    
    std::shared_ptr<HousingStorageContainer> GetContainer(uint64_t container_id);
    std::vector<std::shared_ptr<HousingStorageContainer>> GetAllContainers();
    
    // Room features
    void EnableClimateControl(float temperature, float humidity);
    void ActivateTimeFreezing();
    void ExpandDimensions(uint32_t extra_container_slots);
    
    // Organization
    void AutoOrganizeContainers();
    void ConsolidateItems();  // Merge items across containers
    
    // Access control
    bool GrantAccess(uint64_t player_id, uint32_t permission_level);
    bool RevokeAccess(uint64_t player_id);
    bool HasAccess(uint64_t player_id) const;
    
private:
    uint64_t room_id_;
    StorageRoomConfig config_;
    
    struct ContainerPlacement {
        std::shared_ptr<HousingStorageContainer> container;
        Vector3 position;
        float rotation;
    };
    
    std::unordered_map<uint64_t, ContainerPlacement> containers_;
    std::unordered_map<uint64_t, uint32_t> access_permissions_;
    
    // Environmental effects
    std::chrono::steady_clock::time_point last_climate_update_;
    
    void UpdateClimateEffects();
    void ApplyTimeFreezingEffect();
};

// [SEQUENCE: 3463] Linked storage network
class LinkedStorageNetwork {
public:
    struct NetworkNode {
        uint64_t house_id;
        uint64_t container_id;
        std::string node_name;
        bool is_active{true};
        uint32_t access_tier{1};  // Higher tiers can access lower
    };
    
    // [SEQUENCE: 3464] Network management
    void AddNode(const NetworkNode& node);
    void RemoveNode(uint64_t container_id);
    void LinkNodes(uint64_t container_id_1, uint64_t container_id_2);
    
    // Cross-container operations
    bool TransferItemAcrossNetwork(uint64_t source_container,
                                  uint64_t target_container,
                                  uint32_t item_id,
                                  uint32_t quantity);
    
    std::vector<StoredItem> SearchNetworkItems(uint64_t player_id,
                                              const std::string& search_term);
    
    uint32_t GetTotalNetworkCapacity(uint64_t player_id) const;
    uint32_t GetTotalNetworkUsage(uint64_t player_id) const;
    
    // Network features
    void EnableAutoBalancing();  // Distribute items evenly
    void SetupCraftingIntegration();  // Auto-pull materials
    
private:
    std::unordered_map<uint64_t, NetworkNode> nodes_;
    std::unordered_map<uint64_t, std::set<uint64_t>> connections_;  // Adjacency list
    
    bool auto_balancing_{false};
    bool crafting_integration_{false};
    
    // Pathfinding for transfers
    std::vector<uint64_t> FindPath(uint64_t source, uint64_t target);
    bool ValidateAccess(uint64_t player_id, uint64_t container_id);
};

// [SEQUENCE: 3465] Storage manager
class HousingStorageManager {
public:
    static HousingStorageManager& Instance() {
        static HousingStorageManager instance;
        return instance;
    }
    
    // [SEQUENCE: 3466] Storage creation
    std::shared_ptr<HousingStorageContainer> CreateContainer(
        HousingStorageType type,
        const std::string& custom_name = "");
    
    std::shared_ptr<HousingStorageRoom> CreateStorageRoom(
        uint64_t house_id,
        const HousingStorageRoom::StorageRoomConfig& config);
    
    // Container templates
    void RegisterContainerTemplate(HousingStorageType type,
                                 const StorageContainerProperties& props);
    
    // Network operations
    void CreateStorageNetwork(uint64_t player_id);
    LinkedStorageNetwork* GetPlayerNetwork(uint64_t player_id);
    
    // [SEQUENCE: 3467] Bulk operations
    struct BulkTransferRequest {
        uint64_t source_container;
        uint64_t target_container;
        std::vector<std::pair<uint32_t, uint32_t>> items;  // item_id, quantity
        bool move_all{false};
    };
    
    bool ExecuteBulkTransfer(const BulkTransferRequest& request);
    
    // Storage statistics
    struct StorageStats {
        uint32_t total_containers;
        uint32_t total_items_stored;
        uint64_t total_value_stored;
        std::unordered_map<HousingStorageType, uint32_t> containers_by_type;
    };
    
    StorageStats GetGlobalStats() const;
    StorageStats GetPlayerStats(uint64_t player_id) const;
    
private:
    HousingStorageManager() = default;
    
    std::unordered_map<HousingStorageType, StorageContainerProperties> container_templates_;
    std::unordered_map<uint64_t, std::shared_ptr<HousingStorageContainer>> all_containers_;
    std::unordered_map<uint64_t, std::shared_ptr<HousingStorageRoom>> storage_rooms_;
    std::unordered_map<uint64_t, std::unique_ptr<LinkedStorageNetwork>> player_networks_;
    
    std::atomic<uint64_t> next_container_id_{1};
    std::atomic<uint64_t> next_room_id_{1};
    
    void InitializeDefaultTemplates();
};

// [SEQUENCE: 3468] Special storage types
class SpecialStorageContainers {
public:
    // Display case for showing items
    class DisplayCase : public HousingStorageContainer {
    public:
        struct DisplaySlot {
            uint32_t item_id;
            Vector3 position;
            float rotation;
            float scale{1.0f};
            bool spotlight_enabled{false};
            std::string description_plaque;
        };
        
        // [SEQUENCE: 3469] Display management
        bool AddDisplayItem(uint32_t item_id, const DisplaySlot& slot);
        void RemoveDisplayItem(uint32_t slot_index);
        void UpdateDisplayDescription(uint32_t slot_index, 
                                    const std::string& description);
        
        std::vector<DisplaySlot> GetDisplayedItems() const;
        
    private:
        std::vector<DisplaySlot> display_slots_;
        uint32_t max_display_slots_{6};
    };
    
    // Wardrobe for outfit management
    class Wardrobe : public HousingStorageContainer {
    public:
        struct Outfit {
            std::string name;
            std::unordered_map<uint32_t, uint32_t> equipment_pieces;  // slot_id -> item_id
            uint32_t dye_preset_id;
            bool is_favorite{false};
        };
        
        // [SEQUENCE: 3470] Outfit management
        void SaveOutfit(const std::string& name, const Outfit& outfit);
        bool LoadOutfit(const std::string& name, Player& player);
        void DeleteOutfit(const std::string& name);
        
        std::vector<std::string> GetSavedOutfits() const;
        
    private:
        std::unordered_map<std::string, Outfit> saved_outfits_;
        uint32_t max_outfits_{20};
    };
    
    // Crafting material storage with auto-sorting
    class CraftingStorage : public HousingStorageContainer {
    public:
        // [SEQUENCE: 3471] Material organization
        void OrganizeByType();
        void OrganizeByQuality();
        void OrganizeByProfession(uint32_t profession_id);
        
        std::vector<StoredItem> GetMaterialsForRecipe(uint32_t recipe_id);
        bool HasMaterialsForRecipe(uint32_t recipe_id) const;
        
    private:
        void CategorizeItems();
    };
};

// [SEQUENCE: 3472] Storage utilities
namespace StorageUtils {
    // Capacity calculations
    uint32_t CalculateUpgradeCost(HousingStorageType type,
                                 uint32_t current_capacity,
                                 uint32_t desired_capacity);
    
    // Security
    uint32_t GenerateLockDifficulty(uint32_t container_value);
    bool AttemptLockpicking(uint32_t lock_difficulty,
                          uint32_t player_skill,
                          uint32_t lockpick_quality);
    
    // Organization algorithms
    void OptimizeStorageLayout(std::vector<HousingStorageContainer*> containers);
    void DistributeItemsEvenly(std::vector<HousingStorageContainer*> containers);
    
    // Value calculations
    uint64_t CalculateContainerValue(const HousingStorageContainer& container);
    uint64_t CalculateStorageRoomValue(const HousingStorageRoom& room);
}

} // namespace mmorpg::housing