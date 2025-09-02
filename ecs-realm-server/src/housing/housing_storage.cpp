#include "housing_storage.h"
#include "../core/logger.h"
#include "../player/player.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <queue>

namespace mmorpg::housing {

// [SEQUENCE: MVP14-43] Storage container constructor
HousingStorageContainer::HousingStorageContainer(uint64_t container_id,
                                               const StorageContainerProperties& props)
    : container_id_(container_id)
    , properties_(props)
    , current_capacity_(props.base_capacity) {
    
    if (properties_.requires_key) {
        is_locked_ = true;
    }
}

// [SEQUENCE: MVP14-44] Check if item can be stored
bool HousingStorageContainer::CanStoreItem(uint32_t item_id, uint32_t quantity) const {
    // Check capacity
    if (GetUsedSlots() >= current_capacity_) {
        return false;
    }
    
    // Check restrictions
    if (!ValidateItemRestrictions(item_id)) {
        return false;
    }
    
    // Check weight limit
    // In real implementation, would calculate item weight
    float item_weight = quantity * 1.0f;  // Placeholder
    if (current_weight_ + item_weight > max_weight_) {
        return false;
    }
    
    return true;
}

// [SEQUENCE: MVP14-45] Add item to container
bool HousingStorageContainer::AddItem(uint32_t item_id, uint32_t quantity,
                                    const ItemProperties& properties) {
    if (!CanStoreItem(item_id, quantity)) {
        return false;
    }
    
    // Check if item already exists (merge stacks)
    auto it = stored_items_.find(item_id);
    if (it != stored_items_.end()) {
        it->second.quantity += quantity;
    } else {
        StoredItem new_item{
            .item_id = item_id,
            .quantity = quantity,
            .properties = properties,
            .stored_date = std::chrono::system_clock::now(),
            .stored_by_player_id = 0  // Would get from context
        };
        stored_items_[item_id] = new_item;
    }
    
    UpdateWeight();
    last_accessed_ = std::chrono::system_clock::now();
    
    spdlog::debug("[HOUSING_STORAGE] Added {} x{} to container {}",
                 item_id, quantity, container_id_);
    
    return true;
}

// [SEQUENCE: MVP14-46] Remove item from container
bool HousingStorageContainer::RemoveItem(uint32_t item_id, uint32_t quantity) {
    auto it = stored_items_.find(item_id);
    if (it == stored_items_.end()) {
        return false;
    }
    
    if (it->second.quantity < quantity) {
        return false;
    }
    
    it->second.quantity -= quantity;
    if (it->second.quantity == 0) {
        stored_items_.erase(it);
    }
    
    UpdateWeight();
    last_accessed_ = std::chrono::system_clock::now();
    
    return true;
}

// [SEQUENCE: MVP14-47] Transfer item between containers
bool HousingStorageContainer::TransferItem(uint32_t item_id, uint32_t quantity,
                                         HousingStorageContainer& target) {
    auto it = stored_items_.find(item_id);
    if (it == stored_items_.end() || it->second.quantity < quantity) {
        return false;
    }
    
    if (!target.CanStoreItem(item_id, quantity)) {
        return false;
    }
    
    // Remove from source
    ItemProperties props = it->second.properties;
    if (!RemoveItem(item_id, quantity)) {
        return false;
    }
    
    // Add to target
    if (!target.AddItem(item_id, quantity, props)) {
        // Rollback
        AddItem(item_id, quantity, props);
        return false;
    }
    
    return true;
}

// [SEQUENCE: MVP14-48] Get used storage slots
uint32_t HousingStorageContainer::GetUsedSlots() const {
    return stored_items_.size();
}

// [SEQUENCE: MVP14-49] Get weight usage percentage
float HousingStorageContainer::GetWeightUsage() const {
    return (current_weight_ / max_weight_) * 100.0f;
}

// [SEQUENCE: MVP14-50] Get all stored items
std::vector<HousingStorageContainer::StoredItem> 
HousingStorageContainer::GetAllItems() const {
    std::vector<StoredItem> items;
    items.reserve(stored_items_.size());
    
    for (const auto& [item_id, item] : stored_items_) {
        items.push_back(item);
    }
    
    return items;
}

// [SEQUENCE: MVP14-51] Search items by name
std::vector<HousingStorageContainer::StoredItem> 
HousingStorageContainer::SearchItems(const std::string& name_filter) const {
    std::vector<StoredItem> results;
    
    // In real implementation, would search by item name
    for (const auto& [item_id, item] : stored_items_) {
        // Placeholder: check if item name contains filter
        results.push_back(item);
    }
    
    return results;
}

// [SEQUENCE: MVP14-52] Auto-sort container items
void HousingStorageContainer::AutoSort() {
    if (!properties_.auto_sort) {
        return;
    }
    
    // In real implementation, would reorganize items by type/quality/name
    spdlog::debug("[HOUSING_STORAGE] Auto-sorted container {}", container_id_);
}

// [SEQUENCE: MVP14-53] Compact storage space
void HousingStorageContainer::CompactStorage() {
    MergeStacks();
    // Additional compaction logic
}

// [SEQUENCE: MVP14-54] Merge similar item stacks
void HousingStorageContainer::MergeStacks() {
    // In real implementation, would merge partial stacks of same items
    spdlog::debug("[HOUSING_STORAGE] Merged stacks in container {}", container_id_);
}

// [SEQUENCE: MVP14-55] Unlock container
bool HousingStorageContainer::Unlock(uint32_t key_item_id) {
    if (!is_locked_) {
        return true;
    }
    
    // Verify key
    // In real implementation, would check if key matches
    is_locked_ = false;
    
    spdlog::info("[HOUSING_STORAGE] Container {} unlocked", container_id_);
    return true;
}

// [SEQUENCE: MVP14-56] Check container trap
bool HousingStorageContainer::CheckTrap(uint64_t player_id) {
    if (!properties_.trap_enabled || trap_triggered_) {
        return true;
    }
    
    // In real implementation, would roll against player's trap detection
    // and potentially trigger trap effects
    
    trap_triggered_ = true;
    return false;  // Trap activated
}

// [SEQUENCE: MVP14-57] Upgrade container capacity
void HousingStorageContainer::UpgradeCapacity(uint32_t additional_slots) {
    current_capacity_ += additional_slots;
    
    spdlog::info("[HOUSING_STORAGE] Container {} capacity upgraded to {}",
                container_id_, current_capacity_);
}

// [SEQUENCE: MVP14-58] Validate item restrictions
bool HousingStorageContainer::ValidateItemRestrictions(uint32_t item_id) const {
    switch (properties_.restriction) {
        case StorageContainerProperties::RestrictionType::NONE:
            return true;
            
        case StorageContainerProperties::RestrictionType::SPECIFIC_TYPES:
            return std::find(properties_.allowed_item_types.begin(),
                           properties_.allowed_item_types.end(),
                           item_id) != properties_.allowed_item_types.end();
            
        // Other restriction types would check item categories
        default:
            return true;
    }
}

// [SEQUENCE: MVP14-59] Update container weight
void HousingStorageContainer::UpdateWeight() {
    current_weight_ = 0.0f;
    
    for (const auto& [item_id, item] : stored_items_) {
        // In real implementation, would get actual item weight
        current_weight_ += item.quantity * 1.0f;  // Placeholder
    }
}

// [SEQUENCE: MVP14-60] Storage room constructor
HousingStorageRoom::HousingStorageRoom(uint64_t room_id,
                                     const StorageRoomConfig& config)
    : room_id_(room_id)
    , config_(config) {
    
    if (config_.climate_controlled) {
        last_climate_update_ = std::chrono::steady_clock::now();
    }
}

// [SEQUENCE: MVP14-61] Add container to room
bool HousingStorageRoom::AddContainer(std::shared_ptr<HousingStorageContainer> container,
                                    const Vector3& position) {
    if (containers_.size() >= config_.max_containers) {
        return false;
    }
    
    ContainerPlacement placement{
        .container = container,
        .position = position,
        .rotation = 0.0f
    };
    
    containers_[container->container_id_] = placement;
    
    spdlog::debug("[HOUSING_STORAGE] Added container to room {} at ({}, {}, {})",
                 room_id_, position.x, position.y, position.z);
    
    return true;
}

// [SEQUENCE: MVP14-62] Remove container from room
bool HousingStorageRoom::RemoveContainer(uint64_t container_id) {
    auto it = containers_.find(container_id);
    if (it == containers_.end()) {
        return false;
    }
    
    containers_.erase(it);
    return true;
}

// [SEQUENCE: MVP14-63] Get all containers in room
std::vector<std::shared_ptr<HousingStorageContainer>> 
HousingStorageRoom::GetAllContainers() {
    std::vector<std::shared_ptr<HousingStorageContainer>> result;
    result.reserve(containers_.size());
    
    for (const auto& [id, placement] : containers_) {
        result.push_back(placement.container);
    }
    
    return result;
}

// [SEQUENCE: MVP14-64] Enable climate control
void HousingStorageRoom::EnableClimateControl(float temperature, float humidity) {
    config_.climate_controlled = true;
    config_.temperature_control = temperature;
    config_.humidity_control = humidity;
    
    spdlog::info("[HOUSING_STORAGE] Room {} climate control enabled: {}°C, {}% humidity",
                room_id_, temperature, humidity);
}

// [SEQUENCE: MVP14-65] Auto-organize containers
void HousingStorageRoom::AutoOrganizeContainers() {
    if (!config_.auto_organize) {
        return;
    }
    
    // Sort containers by type and reorganize positions
    std::vector<ContainerPlacement*> sorted_containers;
    for (auto& [id, placement] : containers_) {
        sorted_containers.push_back(&placement);
    }
    
    // Grid layout
    float spacing = 2.0f;
    int grid_width = static_cast<int>(std::sqrt(sorted_containers.size()));
    
    for (size_t i = 0; i < sorted_containers.size(); ++i) {
        int x = i % grid_width;
        int z = i / grid_width;
        
        sorted_containers[i]->position = Vector3{
            static_cast<float>(x) * spacing,
            0.0f,
            static_cast<float>(z) * spacing
        };
    }
    
    spdlog::debug("[HOUSING_STORAGE] Auto-organized {} containers in room {}",
                 containers_.size(), room_id_);
}

// [SEQUENCE: MVP14-66] Consolidate items across containers
void HousingStorageRoom::ConsolidateItems() {
    // Collect all items
    std::unordered_map<uint32_t, uint32_t> all_items;
    std::vector<std::shared_ptr<HousingStorageContainer>> containers;
    
    for (auto& [id, placement] : containers_) {
        containers.push_back(placement.container);
        
        for (const auto& item : placement.container->GetAllItems()) {
            all_items[item.item_id] += item.quantity;
        }
    }
    
    // Clear all containers
    for (auto& container : containers) {
        auto items = container->GetAllItems();
        for (const auto& item : items) {
            container->RemoveItem(item.item_id, item.quantity);
        }
    }
    
    // Redistribute items efficiently
    size_t container_idx = 0;
    for (const auto& [item_id, quantity] : all_items) {
        uint32_t remaining = quantity;
        
        while (remaining > 0 && container_idx < containers.size()) {
            uint32_t to_add = std::min(remaining, 999u);  // Max stack size
            
            if (containers[container_idx]->AddItem(item_id, to_add, {})) {
                remaining -= to_add;
            } else {
                container_idx++;
            }
        }
    }
    
    spdlog::info("[HOUSING_STORAGE] Consolidated items in room {}", room_id_);
}

// [SEQUENCE: MVP14-67] Grant room access
bool HousingStorageRoom::GrantAccess(uint64_t player_id, uint32_t permission_level) {
    access_permissions_[player_id] = permission_level;
    return true;
}

// [SEQUENCE: MVP14-68] Check room access
bool HousingStorageRoom::HasAccess(uint64_t player_id) const {
    auto it = access_permissions_.find(player_id);
    return it != access_permissions_.end() && it->second > 0;
}

// [SEQUENCE: MVP14-69] Add node to storage network
void LinkedStorageNetwork::AddNode(const NetworkNode& node) {
    nodes_[node.container_id] = node;
    connections_[node.container_id] = std::set<uint64_t>();
    
    spdlog::debug("[HOUSING_STORAGE] Added node {} to storage network",
                 node.container_id);
}

// [SEQUENCE: MVP14-70] Link storage nodes
void LinkedStorageNetwork::LinkNodes(uint64_t container_id_1, uint64_t container_id_2) {
    if (nodes_.find(container_id_1) == nodes_.end() ||
        nodes_.find(container_id_2) == nodes_.end()) {
        return;
    }
    
    connections_[container_id_1].insert(container_id_2);
    connections_[container_id_2].insert(container_id_1);
    
    spdlog::debug("[HOUSING_STORAGE] Linked containers {} and {}",
                 container_id_1, container_id_2);
}

// [SEQUENCE: MVP14-71] Transfer item across network
bool LinkedStorageNetwork::TransferItemAcrossNetwork(uint64_t source_container,
                                                   uint64_t target_container,
                                                   uint32_t item_id,
                                                   uint32_t quantity) {
    // Find path between containers
    auto path = FindPath(source_container, target_container);
    if (path.empty()) {
        return false;
    }
    
    // In real implementation, would transfer through network
    spdlog::info("[HOUSING_STORAGE] Transferred item {} across {} nodes",
                item_id, path.size());
    
    return true;
}

// [SEQUENCE: MVP14-72] Search items across network
std::vector<HousingStorageContainer::StoredItem> 
LinkedStorageNetwork::SearchNetworkItems(uint64_t player_id,
                                        const std::string& search_term) {
    std::vector<HousingStorageContainer::StoredItem> results;
    
    for (const auto& [container_id, node] : nodes_) {
        if (!ValidateAccess(player_id, container_id)) {
            continue;
        }
        
        // In real implementation, would search container
        // and add matching items to results
    }
    
    return results;
}

// [SEQUENCE: MVP14-73] Find path between network nodes
std::vector<uint64_t> LinkedStorageNetwork::FindPath(uint64_t source, uint64_t target) {
    if (source == target) {
        return {source};
    }
    
    // BFS pathfinding
    std::queue<uint64_t> queue;
    std::unordered_map<uint64_t, uint64_t> parent;
    std::set<uint64_t> visited;
    
    queue.push(source);
    visited.insert(source);
    
    while (!queue.empty()) {
        uint64_t current = queue.front();
        queue.pop();
        
        if (current == target) {
            // Reconstruct path
            std::vector<uint64_t> path;
            uint64_t node = target;
            
            while (node != source) {
                path.push_back(node);
                node = parent[node];
            }
            path.push_back(source);
            
            std::reverse(path.begin(), path.end());
            return path;
        }
        
        for (uint64_t neighbor : connections_[current]) {
            if (visited.find(neighbor) == visited.end()) {
                visited.insert(neighbor);
                parent[neighbor] = current;
                queue.push(neighbor);
            }
        }
    }
    
    return {};  // No path found
}

// [SEQUENCE: MVP14-74] Create storage container
std::shared_ptr<HousingStorageContainer> HousingStorageManager::CreateContainer(
    HousingStorageType type,
    const std::string& custom_name) {
    
    auto it = container_templates_.find(type);
    if (it == container_templates_.end()) {
        spdlog::warn("[HOUSING_STORAGE] Unknown container type: {}",
                    static_cast<int>(type));
        return nullptr;
    }
    
    StorageContainerProperties props = it->second;
    if (!custom_name.empty()) {
        props.name = custom_name;
    }
    
    uint64_t container_id = next_container_id_++;
    auto container = std::make_shared<HousingStorageContainer>(container_id, props);
    
    all_containers_[container_id] = container;
    
    spdlog::info("[HOUSING_STORAGE] Created {} container with ID {}",
                props.name, container_id);
    
    return container;
}

// [SEQUENCE: MVP14-75] Create storage room
std::shared_ptr<HousingStorageRoom> HousingStorageManager::CreateStorageRoom(
    uint64_t house_id,
    const HousingStorageRoom::StorageRoomConfig& config) {
    
    uint64_t room_id = next_room_id_++;
    auto room = std::make_shared<HousingStorageRoom>(room_id, config);
    
    storage_rooms_[room_id] = room;
    
    spdlog::info("[HOUSING_STORAGE] Created storage room {} for house {}",
                room_id, house_id);
    
    return room;
}

// [SEQUENCE: MVP14-76] Register container template
void HousingStorageManager::RegisterContainerTemplate(HousingStorageType type,
                                                    const StorageContainerProperties& props) {
    container_templates_[type] = props;
    
    spdlog::debug("[HOUSING_STORAGE] Registered template for {} containers",
                 props.name);
}

// [SEQUENCE: MVP14-77] Initialize default templates
void HousingStorageManager::InitializeDefaultTemplates() {
    // Personal chest
    RegisterContainerTemplate(HousingStorageType::PERSONAL_CHEST, {
        .type = HousingStorageType::PERSONAL_CHEST,
        .name = "Personal Chest",
        .base_capacity = 30,
        .restriction = StorageContainerProperties::RestrictionType::NONE,
        .auto_sort = true,
        .shared_access = false
    });
    
    // Shared storage
    RegisterContainerTemplate(HousingStorageType::SHARED_STORAGE, {
        .type = HousingStorageType::SHARED_STORAGE,
        .name = "Shared Storage",
        .base_capacity = 50,
        .restriction = StorageContainerProperties::RestrictionType::NONE,
        .shared_access = true,
        .linked_storage = true
    });
    
    // Wardrobe
    RegisterContainerTemplate(HousingStorageType::WARDROBE, {
        .type = HousingStorageType::WARDROBE,
        .name = "Wardrobe",
        .base_capacity = 40,
        .restriction = StorageContainerProperties::RestrictionType::EQUIPMENT_ONLY,
        .auto_sort = true,
        .preserve_quality = true
    });
    
    // Display case
    RegisterContainerTemplate(HousingStorageType::DISPLAY_CASE, {
        .type = HousingStorageType::DISPLAY_CASE,
        .name = "Display Case",
        .base_capacity = 6,
        .restriction = StorageContainerProperties::RestrictionType::VALUABLES_ONLY,
        .preserve_quality = true,
        .requires_key = true,
        .lock_difficulty = 50
    });
    
    // Bank vault
    RegisterContainerTemplate(HousingStorageType::BANK_VAULT, {
        .type = HousingStorageType::BANK_VAULT,
        .name = "Bank Vault",
        .base_capacity = 100,
        .restriction = StorageContainerProperties::RestrictionType::NONE,
        .preserve_quality = true,
        .linked_storage = true,
        .requires_key = true,
        .lock_difficulty = 100,
        .trap_enabled = true
    });
    
    // Crafting storage
    RegisterContainerTemplate(HousingStorageType::CRAFTING_STORAGE, {
        .type = HousingStorageType::CRAFTING_STORAGE,
        .name = "Crafting Storage",
        .base_capacity = 60,
        .restriction = StorageContainerProperties::RestrictionType::MATERIALS_ONLY,
        .auto_sort = true,
        .linked_storage = true
    });
    
    // Magical chest
    RegisterContainerTemplate(HousingStorageType::MAGICAL_CHEST, {
        .type = HousingStorageType::MAGICAL_CHEST,
        .name = "Magical Chest",
        .base_capacity = 40,
        .restriction = StorageContainerProperties::RestrictionType::NONE,
        .preserve_quality = true,
        .linked_storage = true
    });
}

// [SEQUENCE: MVP14-78] Execute bulk transfer
bool HousingStorageManager::ExecuteBulkTransfer(const BulkTransferRequest& request) {
    auto source_it = all_containers_.find(request.source_container);
    auto target_it = all_containers_.find(request.target_container);
    
    if (source_it == all_containers_.end() || target_it == all_containers_.end()) {
        return false;
    }
    
    auto& source = source_it->second;
    auto& target = target_it->second;
    
    if (request.move_all) {
        // Move all items
        auto items = source->GetAllItems();
        for (const auto& item : items) {
            source->TransferItem(item.item_id, item.quantity, *target);
        }
    } else {
        // Move specific items
        for (const auto& [item_id, quantity] : request.items) {
            source->TransferItem(item_id, quantity, *target);
        }
    }
    
    spdlog::info("[HOUSING_STORAGE] Bulk transfer completed between containers {} and {}",
                request.source_container, request.target_container);
    
    return true;
}

// [SEQUENCE: MVP14-79] Storage utility functions
namespace StorageUtils {

uint32_t CalculateUpgradeCost(HousingStorageType type,
                             uint32_t current_capacity,
                             uint32_t desired_capacity) {
    uint32_t base_cost = 100;
    
    // Type multiplier
    switch (type) {
        case HousingStorageType::BANK_VAULT:
            base_cost *= 5;
            break;
        case HousingStorageType::MAGICAL_CHEST:
            base_cost *= 3;
            break;
        default:
            break;
    }
    
    // Exponential growth
    uint32_t slots_to_add = desired_capacity - current_capacity;
    return base_cost * slots_to_add * (1 + current_capacity / 10);
}

uint32_t GenerateLockDifficulty(uint32_t container_value) {
    // Base difficulty on value
    if (container_value < 1000) {
        return 25;
    } else if (container_value < 10000) {
        return 50;
    } else if (container_value < 100000) {
        return 75;
    } else {
        return 100;
    }
}

bool AttemptLockpicking(uint32_t lock_difficulty,
                       uint32_t player_skill,
                       uint32_t lockpick_quality) {
    // Calculate success chance
    float base_chance = 0.5f;
    float skill_bonus = player_skill * 0.01f;
    float quality_bonus = lockpick_quality * 0.005f;
    float difficulty_penalty = lock_difficulty * 0.01f;
    
    float success_chance = base_chance + skill_bonus + quality_bonus - difficulty_penalty;
    success_chance = std::max(0.05f, std::min(0.95f, success_chance));
    
    // Roll
    return (rand() % 100) < (success_chance * 100);
}

void OptimizeStorageLayout(std::vector<HousingStorageContainer*> containers) {
    // Sort containers by type and fill rate
    std::sort(containers.begin(), containers.end(),
        [](const HousingStorageContainer* a, const HousingStorageContainer* b) {
            float fill_a = static_cast<float>(a->GetUsedSlots()) / a->GetTotalCapacity();
            float fill_b = static_cast<float>(b->GetUsedSlots()) / b->GetTotalCapacity();
            return fill_a < fill_b;
        });
    
    // Consolidate items from less filled containers
    for (size_t i = 0; i < containers.size() - 1; ++i) {
        auto items = containers[i]->GetAllItems();
        
        for (const auto& item : items) {
            for (size_t j = i + 1; j < containers.size(); ++j) {
                if (containers[i]->TransferItem(item.item_id, item.quantity, 
                                              *containers[j])) {
                    break;
                }
            }
        }
    }
}

uint64_t CalculateContainerValue(const HousingStorageContainer& container) {
    uint64_t total_value = 0;
    
    for (const auto& item : container.GetAllItems()) {
        // In real implementation, would get actual item value
        uint64_t item_value = 100;  // Placeholder
        total_value += item_value * item.quantity;
    }
    
    return total_value;
}

} // namespace StorageUtils

} // namespace mmorpg::housing
