#include "player_housing.h"
#include "../core/logger.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace mmorpg::housing {

// [SEQUENCE: 3297] Player house constructor
PlayerHouse::PlayerHouse(uint64_t house_id, uint64_t owner_id, const HouseConfig& config)
    : house_id_(house_id)
    , owner_id_(owner_id)
    , config_(config)
    , created_at_(std::chrono::system_clock::now())
    , last_visited_(created_at_)
    , last_modified_(created_at_)
    , last_payment_(created_at_) {
}

// [SEQUENCE: 3298] Initialize house with plot
bool PlayerHouse::Initialize(const HousePlot& plot) {
    plot_ = plot;
    plot_.is_available = false;
    
    // Create default rooms based on house type
    switch (config_.type) {
        case HouseType::ROOM:
            AddRoom({
                .room_id = next_room_id_++,
                .room_name = "Main Room",
                .bounds = BoundingBox{
                    .min = Vector3{0, 0, 0},
                    .max = Vector3{10, 3, 10}
                },
                .floor_number = 1,
                .furniture_limit = 20
            });
            break;
            
        case HouseType::SMALL_HOUSE:
            AddRoom({
                .room_id = next_room_id_++,
                .room_name = "Living Room",
                .bounds = BoundingBox{
                    .min = Vector3{0, 0, 0},
                    .max = Vector3{8, 3, 8}
                },
                .floor_number = 1,
                .furniture_limit = 15
            });
            AddRoom({
                .room_id = next_room_id_++,
                .room_name = "Bedroom",
                .bounds = BoundingBox{
                    .min = Vector3{8, 0, 0},
                    .max = Vector3{14, 3, 8}
                },
                .floor_number = 1,
                .furniture_limit = 10
            });
            break;
            
        // Add more room configurations for other house types
        default:
            break;
    }
    
    // Calculate initial costs
    CalculateUpkeep();
    
    spdlog::info("[HOUSING] Initialized house {} for player {} at plot {}",
                house_id_, owner_id_, plot_.plot_id);
    
    return true;
}

// [SEQUENCE: 3299] Update house state
void PlayerHouse::Update(float delta_time) {
    // Degrade condition over time
    float degradation_rate = 0.01f;  // 1% per day
    float daily_fraction = delta_time / (24.0f * 3600.0f);
    condition_ = std::max(0.0f, condition_ - degradation_rate * daily_fraction);
    
    // Check if monthly payment is due
    auto now = std::chrono::system_clock::now();
    auto time_since_payment = now - last_payment_;
    auto days = std::chrono::duration_cast<std::chrono::hours>(time_since_payment).count() / 24;
    
    if (days >= 30) {
        // Payment is due
        spdlog::warn("[HOUSING] House {} payment due: {} gold rent, {} gold tax",
                    house_id_, monthly_rent_, property_tax_);
    }
}

// [SEQUENCE: 3300] Save house data
void PlayerHouse::Save() {
    // In real implementation, would save to database
    spdlog::debug("[HOUSING] Saving house {} data", house_id_);
}

// [SEQUENCE: 3301] Load house data
void PlayerHouse::Load() {
    // In real implementation, would load from database
    spdlog::debug("[HOUSING] Loading house {} data", house_id_);
}

// [SEQUENCE: 3302] Transfer ownership
bool PlayerHouse::TransferOwnership(uint64_t new_owner_id) {
    if (new_owner_id == owner_id_) {
        return false;
    }
    
    uint64_t old_owner = owner_id_;
    owner_id_ = new_owner_id;
    
    // Clear co-owners and permissions
    co_owners_.clear();
    tenants_.clear();
    access_levels_.clear();
    
    last_modified_ = std::chrono::system_clock::now();
    
    spdlog::info("[HOUSING] House {} ownership transferred from {} to {}",
                house_id_, old_owner, new_owner_id);
    
    return true;
}

// [SEQUENCE: 3303] Add co-owner
bool PlayerHouse::AddCoOwner(uint64_t player_id) {
    if (player_id == owner_id_) {
        return false;  // Owner is already owner
    }
    
    if (co_owners_.size() >= config_.max_co_owners) {
        return false;  // Max co-owners reached
    }
    
    auto it = std::find(co_owners_.begin(), co_owners_.end(), player_id);
    if (it != co_owners_.end()) {
        return false;  // Already a co-owner
    }
    
    co_owners_.push_back(player_id);
    access_levels_[player_id] = 2;  // Co-owner access level
    
    spdlog::info("[HOUSING] Added co-owner {} to house {}", player_id, house_id_);
    return true;
}

// [SEQUENCE: 3304] Remove co-owner
bool PlayerHouse::RemoveCoOwner(uint64_t player_id) {
    auto it = std::find(co_owners_.begin(), co_owners_.end(), player_id);
    if (it == co_owners_.end()) {
        return false;
    }
    
    co_owners_.erase(it);
    access_levels_.erase(player_id);
    
    spdlog::info("[HOUSING] Removed co-owner {} from house {}", player_id, house_id_);
    return true;
}

// [SEQUENCE: 3305] Check ownership
bool PlayerHouse::IsOwner(uint64_t player_id) const {
    return player_id == owner_id_ || 
           std::find(co_owners_.begin(), co_owners_.end(), player_id) != co_owners_.end();
}

// [SEQUENCE: 3306] Check access
bool PlayerHouse::HasAccess(uint64_t player_id) const {
    if (IsOwner(player_id)) return true;
    
    auto it = access_levels_.find(player_id);
    return it != access_levels_.end() && it->second > 0;
}

// [SEQUENCE: 3307] Get room by ID
HouseRoom* PlayerHouse::GetRoom(uint32_t room_id) {
    auto it = rooms_.find(room_id);
    if (it != rooms_.end()) {
        return &it->second;
    }
    return nullptr;
}

// [SEQUENCE: 3308] Get all rooms
std::vector<HouseRoom*> PlayerHouse::GetAllRooms() {
    std::vector<HouseRoom*> rooms;
    for (auto& [id, room] : rooms_) {
        rooms.push_back(&room);
    }
    return rooms;
}

// [SEQUENCE: 3309] Add room
bool PlayerHouse::AddRoom(const HouseRoom& room) {
    if (rooms_.size() >= config_.num_rooms) {
        return false;  // Max rooms reached
    }
    
    rooms_[room.room_id] = room;
    last_modified_ = std::chrono::system_clock::now();
    
    spdlog::info("[HOUSING] Added room '{}' to house {}", room.room_name, house_id_);
    return true;
}

// [SEQUENCE: 3310] Place furniture
bool PlayerHouse::PlaceFurniture(uint64_t furniture_id, uint32_t room_id,
                               const Vector3& position, float rotation) {
    auto room = GetRoom(room_id);
    if (!room) {
        return false;
    }
    
    if (room->furniture_ids.size() >= room->furniture_limit) {
        spdlog::warn("[HOUSING] Room {} furniture limit reached", room_id);
        return false;
    }
    
    // Check if position is within room bounds
    if (!room->bounds.Contains(position)) {
        spdlog::warn("[HOUSING] Furniture position outside room bounds");
        return false;
    }
    
    room->furniture_ids.push_back(furniture_id);
    last_modified_ = std::chrono::system_clock::now();
    
    spdlog::debug("[HOUSING] Placed furniture {} in room {} at ({}, {}, {})",
                 furniture_id, room_id, position.x, position.y, position.z);
    
    return true;
}

// [SEQUENCE: 3311] Change wallpaper
bool PlayerHouse::ChangeWallpaper(uint32_t room_id, uint32_t wallpaper_id) {
    auto room = GetRoom(room_id);
    if (!room) {
        return false;
    }
    
    room->wallpaper_id = wallpaper_id;
    last_modified_ = std::chrono::system_clock::now();
    
    spdlog::debug("[HOUSING] Changed wallpaper in room {} to {}", room_id, wallpaper_id);
    return true;
}

// [SEQUENCE: 3312] Pay rent
bool PlayerHouse::PayRent() {
    // In real implementation, would deduct from player's gold
    last_payment_ = std::chrono::system_clock::now();
    
    spdlog::info("[HOUSING] Rent paid for house {}: {} gold", house_id_, monthly_rent_);
    return true;
}

// [SEQUENCE: 3313] Calculate upkeep costs
void PlayerHouse::CalculateUpkeep() {
    // Base rent based on house type
    switch (config_.type) {
        case HouseType::ROOM:
            monthly_rent_ = 1000;
            break;
        case HouseType::SMALL_HOUSE:
            monthly_rent_ = 5000;
            break;
        case HouseType::MEDIUM_HOUSE:
            monthly_rent_ = 15000;
            break;
        case HouseType::LARGE_HOUSE:
            monthly_rent_ = 40000;
            break;
        case HouseType::MANSION:
            monthly_rent_ = 100000;
            break;
        case HouseType::GUILD_HALL:
            monthly_rent_ = 500000;
            break;
    }
    
    // Tier multiplier
    float tier_multiplier = 1.0f + static_cast<float>(config_.tier) * 0.5f;
    monthly_rent_ = static_cast<uint64_t>(monthly_rent_ * tier_multiplier);
    
    // Property tax (10% of rent)
    property_tax_ = monthly_rent_ / 10;
}

// [SEQUENCE: 3314] Get furniture count
uint32_t PlayerHouse::GetFurnitureCount() const {
    uint32_t count = 0;
    for (const auto& [id, room] : rooms_) {
        count += room.furniture_ids.size();
    }
    return count;
}

// [SEQUENCE: 3315] Get house value
uint64_t PlayerHouse::GetValue() const {
    uint64_t base_value = HouseUtils::CalculateBasePrice(config_.type, config_.tier);
    
    // Add value for customizations and furniture
    uint64_t furniture_value = GetFurnitureCount() * 1000;  // Placeholder
    
    // Condition affects value
    float condition_multiplier = condition_ / 100.0f;
    
    return static_cast<uint64_t>((base_value + furniture_value) * condition_multiplier);
}

// [SEQUENCE: 3316] Housing system - Create house
std::shared_ptr<PlayerHouse> HousingSystem::CreateHouse(uint64_t owner_id,
                                                       const HouseConfig& config,
                                                       const HousePlot& plot) {
    std::lock_guard<std::mutex> lock(houses_mutex_);
    
    // Check if player already owns a house
    if (owner_to_house_.find(owner_id) != owner_to_house_.end()) {
        spdlog::warn("[HOUSING] Player {} already owns a house", owner_id);
        return nullptr;
    }
    
    // Create new house
    uint64_t house_id = next_house_id_++;
    auto house = std::make_shared<PlayerHouse>(house_id, owner_id, config);
    
    if (!house->Initialize(plot)) {
        return nullptr;
    }
    
    // Register house
    houses_[house_id] = house;
    owner_to_house_[owner_id] = house_id;
    plot_to_house_[plot.plot_id] = house_id;
    
    spdlog::info("[HOUSING] Created house {} for player {} on plot {}",
                house_id, owner_id, plot.plot_id);
    
    return house;
}

// [SEQUENCE: 3317] Delete house
bool HousingSystem::DeleteHouse(uint64_t house_id) {
    std::lock_guard<std::mutex> lock(houses_mutex_);
    
    auto it = houses_.find(house_id);
    if (it == houses_.end()) {
        return false;
    }
    
    auto house = it->second;
    
    // Clean up mappings
    owner_to_house_.erase(house->GetOwnerId());
    plot_to_house_.erase(house->GetPlot().plot_id);
    
    // Mark plot as available
    auto plot_it = all_plots_.find(house->GetPlot().plot_id);
    if (plot_it != all_plots_.end()) {
        plot_it->second.is_available = true;
    }
    
    houses_.erase(it);
    
    spdlog::info("[HOUSING] Deleted house {}", house_id);
    return true;
}

// [SEQUENCE: 3318] Get house by ID
std::shared_ptr<PlayerHouse> HousingSystem::GetHouse(uint64_t house_id) {
    std::lock_guard<std::mutex> lock(houses_mutex_);
    
    auto it = houses_.find(house_id);
    if (it != houses_.end()) {
        return it->second;
    }
    
    return nullptr;
}

// [SEQUENCE: 3319] Get house by owner
std::shared_ptr<PlayerHouse> HousingSystem::GetHouseByOwner(uint64_t owner_id) {
    std::lock_guard<std::mutex> lock(houses_mutex_);
    
    auto it = owner_to_house_.find(owner_id);
    if (it != owner_to_house_.end()) {
        return houses_[it->second];
    }
    
    return nullptr;
}

// [SEQUENCE: 3320] Get available plots
std::vector<HousePlot> HousingSystem::GetAvailablePlots(const std::string& zone) {
    std::lock_guard<std::mutex> lock(plots_mutex_);
    
    std::vector<HousePlot> available;
    
    for (const auto& [id, plot] : all_plots_) {
        if (plot.is_available) {
            if (zone.empty() || plot.zone_name == zone) {
                available.push_back(plot);
            }
        }
    }
    
    return available;
}

// [SEQUENCE: 3321] Purchase plot
bool HousingSystem::PurchasePlot(uint64_t player_id, uint64_t plot_id) {
    std::lock_guard<std::mutex> lock(plots_mutex_);
    
    auto it = all_plots_.find(plot_id);
    if (it == all_plots_.end() || !it->second.is_available) {
        return false;
    }
    
    // In real implementation, would check player's gold and deduct cost
    
    it->second.is_available = false;
    
    spdlog::info("[HOUSING] Player {} purchased plot {}", player_id, plot_id);
    return true;
}

// [SEQUENCE: 3322] Enter house
bool HousingSystem::EnterHouse(uint64_t player_id, uint64_t house_id) {
    auto house = GetHouse(house_id);
    if (!house) {
        return false;
    }
    
    if (!house->HasAccess(player_id)) {
        spdlog::warn("[HOUSING] Player {} denied access to house {}", player_id, house_id);
        return false;
    }
    
    // Track visitor
    HouseVisitorManager::RecordVisit(house_id, player_id);
    
    spdlog::debug("[HOUSING] Player {} entered house {}", player_id, house_id);
    return true;
}

// [SEQUENCE: 3323] Update housing system
void HousingSystem::Update(float delta_time) {
    std::lock_guard<std::mutex> lock(houses_mutex_);
    
    for (auto& [id, house] : houses_) {
        house->Update(delta_time);
    }
}

// [SEQUENCE: 3324] Get housing statistics
HousingSystem::HousingStats HousingSystem::GetStatistics() const {
    std::lock_guard<std::mutex> houses_lock(houses_mutex_);
    std::lock_guard<std::mutex> plots_lock(plots_mutex_);
    
    HousingStats stats;
    stats.total_houses = houses_.size();
    
    uint32_t available = 0;
    for (const auto& [id, plot] : all_plots_) {
        if (plot.is_available) {
            available++;
        }
    }
    
    stats.available_plots = available;
    stats.occupied_plots = all_plots_.size() - available;
    
    // Count by type
    for (const auto& [id, house] : houses_) {
        stats.houses_by_type[house->GetType()]++;
        stats.houses_by_zone[house->GetPlot().zone_name]++;
        stats.total_property_value += house->GetValue();
    }
    
    return stats;
}

// [SEQUENCE: 3325] House template data
std::unordered_map<std::string, HouseTemplate::TemplateData> HouseTemplate::templates_ = {
    {"starter_room", {
        .name = "Starter Room",
        .type = HouseType::ROOM,
        .tier = HouseTier::BASIC,
        .config = HouseUtils::CreateDefaultConfig(HouseType::ROOM, HouseTier::BASIC),
        .default_rooms = {{
            .room_id = 1,
            .room_name = "Main Room",
            .bounds = BoundingBox{{0, 0, 0}, {10, 3, 10}},
            .floor_number = 1,
            .furniture_limit = 20
        }},
        .base_price = 50000,
        .required_level = 10
    }},
    
    {"cottage", {
        .name = "Cozy Cottage",
        .type = HouseType::SMALL_HOUSE,
        .tier = HouseTier::STANDARD,
        .config = HouseUtils::CreateDefaultConfig(HouseType::SMALL_HOUSE, HouseTier::STANDARD),
        .default_rooms = {
            {
                .room_id = 1,
                .room_name = "Living Room",
                .bounds = BoundingBox{{0, 0, 0}, {8, 3, 8}},
                .floor_number = 1,
                .furniture_limit = 15
            },
            {
                .room_id = 2,
                .room_name = "Bedroom",
                .bounds = BoundingBox{{8, 0, 0}, {14, 3, 8}},
                .floor_number = 1,
                .furniture_limit = 10
            }
        },
        .base_price = 200000,
        .required_level = 30
    }}
};

// [SEQUENCE: 3326] Get house template
HouseTemplate::TemplateData HouseTemplate::GetTemplate(const std::string& template_name) {
    auto it = templates_.find(template_name);
    if (it != templates_.end()) {
        return it->second;
    }
    
    // Return default template
    return templates_["starter_room"];
}

// [SEQUENCE: 3327] Get available upgrades
std::vector<HouseUpgradeSystem::UpgradeOption> HouseUpgradeSystem::GetAvailableUpgrades(
    const PlayerHouse& house) {
    
    std::vector<UpgradeOption> upgrades;
    
    // Tier upgrade
    if (house.GetTier() < HouseTier::LUXURY) {
        upgrades.push_back({
            .name = "tier_upgrade",
            .description = "Upgrade house to next tier",
            .cost = GetTierUpgradeCost(house.GetTier()),
            .required_items = {},
            .required_level = 50,
            .apply_upgrade = [](PlayerHouse& h) { /* Upgrade tier */ }
        });
    }
    
    // Room expansions
    auto config = house.GetConfig();
    if (house.GetAllRooms().size() < 10) {
        upgrades.push_back({
            .name = "add_room",
            .description = "Add an additional room",
            .cost = 100000,
            .required_items = {{1001, 50}, {1002, 30}},  // Wood, Stone
            .required_level = 40,
            .apply_upgrade = [](PlayerHouse& h) { /* Add room */ }
        });
    }
    
    return upgrades;
}

// [SEQUENCE: 3328] Get tier upgrade cost
uint64_t HouseUpgradeSystem::GetTierUpgradeCost(HouseTier current_tier) {
    switch (current_tier) {
        case HouseTier::BASIC: return 500000;
        case HouseTier::STANDARD: return 1500000;
        case HouseTier::DELUXE: return 5000000;
        case HouseTier::PREMIUM: return 15000000;
        default: return 0;
    }
}

// [SEQUENCE: 3329] Visitor history tracking
std::unordered_map<uint64_t, std::vector<HouseVisitorManager::VisitorInfo>> 
    HouseVisitorManager::visitor_history_;

void HouseVisitorManager::RecordVisit(uint64_t house_id, uint64_t visitor_id) {
    auto now = std::chrono::system_clock::now();
    
    auto& history = visitor_history_[house_id];
    
    // Find existing visitor record
    auto it = std::find_if(history.begin(), history.end(),
                          [visitor_id](const VisitorInfo& info) {
                              return info.player_id == visitor_id;
                          });
    
    if (it != history.end()) {
        it->visit_count++;
        it->entry_time = now;
    } else {
        history.push_back({
            .player_id = visitor_id,
            .player_name = "Player" + std::to_string(visitor_id),  // Get from player system
            .entry_time = now,
            .duration = std::chrono::seconds(0),
            .visit_count = 1
        });
    }
}

// [SEQUENCE: 3330] House utility functions
namespace HouseUtils {

HouseConfig CreateDefaultConfig(HouseType type, HouseTier tier) {
    HouseConfig config;
    config.type = type;
    config.tier = tier;
    
    // Set defaults based on type
    switch (type) {
        case HouseType::ROOM:
            config.max_furniture_count = 30;
            config.max_storage_slots = 20;
            config.num_rooms = 1;
            config.total_area = 100.0f;
            break;
            
        case HouseType::SMALL_HOUSE:
            config.max_furniture_count = 75;
            config.max_storage_slots = 50;
            config.num_rooms = 3;
            config.total_area = 200.0f;
            config.has_garden = true;
            break;
            
        case HouseType::MEDIUM_HOUSE:
            config.max_furniture_count = 150;
            config.max_storage_slots = 100;
            config.num_rooms = 5;
            config.num_floors = 2;
            config.total_area = 400.0f;
            config.has_garden = true;
            config.has_balcony = true;
            break;
            
        case HouseType::LARGE_HOUSE:
            config.max_furniture_count = 300;
            config.max_storage_slots = 200;
            config.num_rooms = 8;
            config.num_floors = 2;
            config.total_area = 800.0f;
            config.has_garden = true;
            config.has_balcony = true;
            config.has_basement = true;
            config.max_co_owners = 2;
            break;
            
        case HouseType::MANSION:
            config.max_furniture_count = 500;
            config.max_storage_slots = 500;
            config.num_rooms = 12;
            config.num_floors = 3;
            config.total_area = 1500.0f;
            config.has_garden = true;
            config.has_balcony = true;
            config.has_basement = true;
            config.has_workshop = true;
            config.max_co_owners = 5;
            config.max_vendors = 3;
            break;
            
        case HouseType::GUILD_HALL:
            config.max_furniture_count = 1000;
            config.max_storage_slots = 1000;
            config.num_rooms = 20;
            config.num_floors = 3;
            config.total_area = 3000.0f;
            config.has_garden = true;
            config.has_basement = true;
            config.has_workshop = true;
            config.max_co_owners = 10;
            config.max_vendors = 5;
            config.max_guests = 100;
            break;
    }
    
    // Apply tier bonuses
    float tier_bonus = 1.0f + static_cast<float>(tier) * 0.2f;
    config.max_furniture_count = static_cast<uint32_t>(config.max_furniture_count * tier_bonus);
    config.max_storage_slots = static_cast<uint32_t>(config.max_storage_slots * tier_bonus);
    
    return config;
}

uint64_t CalculateBasePrice(HouseType type, HouseTier tier) {
    uint64_t base_price = 0;
    
    switch (type) {
        case HouseType::ROOM: base_price = 50000; break;
        case HouseType::SMALL_HOUSE: base_price = 200000; break;
        case HouseType::MEDIUM_HOUSE: base_price = 800000; break;
        case HouseType::LARGE_HOUSE: base_price = 2000000; break;
        case HouseType::MANSION: base_price = 8000000; break;
        case HouseType::GUILD_HALL: base_price = 50000000; break;
    }
    
    // Tier multiplier
    float tier_multiplier = 1.0f + static_cast<float>(tier) * 0.5f;
    
    return static_cast<uint64_t>(base_price * tier_multiplier);
}

bool ValidateFurniturePlacement(const HouseRoom& room,
                              const Vector3& position,
                              const BoundingBox& furniture_bounds) {
    // Check if furniture fits within room bounds
    BoundingBox placed_bounds{
        position + furniture_bounds.min,
        position + furniture_bounds.max
    };
    
    return room.bounds.Contains(placed_bounds);
}

} // namespace HouseUtils

} // namespace mmorpg::housing