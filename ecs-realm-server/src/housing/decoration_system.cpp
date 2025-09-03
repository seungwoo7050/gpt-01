#include "decoration_system.h"
#include "../core/logger.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

namespace mmorpg::housing {

// [SEQUENCE: 3364] Placed decoration constructor
PlacedDecoration::PlacedDecoration(uint64_t instance_id, const DecorationItem& item)
    : instance_id_(instance_id)
    , item_(item) {
}

// [SEQUENCE: 3365] Set decoration scale
void PlacedDecoration::SetScale(const Vector3& scale) {
    // Clamp to allowed scale range
    scale_.x = std::clamp(scale.x, item_.min_scale.x, item_.max_scale.x);
    scale_.y = std::clamp(scale.y, item_.min_scale.y, item_.max_scale.y);
    scale_.z = std::clamp(scale.z, item_.min_scale.z, item_.max_scale.z);
}

// [SEQUENCE: 3366] Set material variant
void PlacedDecoration::SetMaterialVariant(uint32_t variant_index) {
    if (variant_index < item_.material_variants.size()) {
        material_variant_ = variant_index;
    }
}

// [SEQUENCE: 3367] Set emissive intensity
void PlacedDecoration::SetEmissiveIntensity(float intensity) {
    if (item_.emits_light) {
        emissive_intensity_ = std::max(0.0f, intensity);
    }
}

// [SEQUENCE: 3368] Set custom data
void PlacedDecoration::SetCustomData(const std::string& key, const std::string& value) {
    custom_data_[key] = value;
}

std::string PlacedDecoration::GetCustomData(const std::string& key) const {
    auto it = custom_data_.find(key);
    return it != custom_data_.end() ? it->second : "";
}

// [SEQUENCE: 3369] Get world bounds
BoundingBox PlacedDecoration::GetWorldBounds() const {
    // Transform local bounds to world space
    BoundingBox world_bounds;
    
    // Apply scale
    world_bounds.min = item_.bounds.min * scale_;
    world_bounds.max = item_.bounds.max * scale_;
    
    // Apply rotation (simplified - in real implementation would use quaternion)
    // For now, just translate
    world_bounds.min += position_;
    world_bounds.max += position_;
    
    return world_bounds;
}

// [SEQUENCE: 3370] Check collision
bool PlacedDecoration::CheckCollision(const PlacedDecoration& other) const {
    auto bounds_a = GetWorldBounds();
    auto bounds_b = other.GetWorldBounds();
    
    return bounds_a.Intersects(bounds_b);
}

// [SEQUENCE: 3371] Validate decoration placement
DecorationPlacementValidator::ValidationResult 
DecorationPlacementValidator::ValidatePlacement(
    const DecorationItem& item,
    const Vector3& position,
    const HouseRoom& room,
    const std::vector<PlacedDecoration*>& existing_decorations) {
    
    ValidationResult result;
    
    // Check if position is within room bounds
    if (!room.bounds.Contains(position)) {
        result.errors.push_back("Position is outside room bounds");
        return result;
    }
    
    // Check placement rules
    if (!CheckPlacementRules(item, position, room)) {
        result.errors.push_back("Placement rules violated");
        return result;
    }
    
    // Check surface requirement
    if (!CheckSurfaceRequirement(item, position, room)) {
        result.errors.push_back("Surface requirement not met");
        return result;
    }
    
    // Create temporary decoration for bounds checking
    PlacedDecoration temp_decoration(0, item);
    temp_decoration.SetPosition(position);
    
    // Check overlap
    if (!CheckOverlap(temp_decoration.GetWorldBounds(), existing_decorations)) {
        result.errors.push_back("Overlaps with existing decoration");
        return result;
    }
    
    // Check furniture limit
    if (existing_decorations.size() >= room.furniture_limit) {
        result.warnings.push_back("Room is near furniture limit");
    }
    
    result.is_valid = result.errors.empty();
    return result;
}

// [SEQUENCE: 3372] Check surface requirement
bool DecorationPlacementValidator::CheckSurfaceRequirement(
    const DecorationItem& item,
    const Vector3& position,
    const HouseRoom& room) {
    
    for (auto rule : item.placement_rules) {
        if (rule == PlacementRule::SURFACE_REQUIRED) {
            // Check if position is on any surface
            return IsOnFloor(position, room) || 
                   IsOnWall(position, room) || 
                   IsOnCeiling(position, room);
        }
    }
    
    return true;
}

// [SEQUENCE: 3373] Check overlap
bool DecorationPlacementValidator::CheckOverlap(
    const BoundingBox& bounds,
    const std::vector<PlacedDecoration*>& existing_decorations) {
    
    for (const auto* decoration : existing_decorations) {
        if (decoration && bounds.Intersects(decoration->GetWorldBounds())) {
            // Check if item allows overlap
            bool no_overlap = false;
            for (auto rule : decoration->GetItem().placement_rules) {
                if (rule == PlacementRule::NO_OVERLAP) {
                    no_overlap = true;
                    break;
                }
            }
            
            if (no_overlap) {
                return false;
            }
        }
    }
    
    return true;
}

// [SEQUENCE: 3374] Check placement rules
bool DecorationPlacementValidator::CheckPlacementRules(
    const DecorationItem& item,
    const Vector3& position,
    const HouseRoom& room) {
    
    for (auto rule : item.placement_rules) {
        switch (rule) {
            case PlacementRule::FLOOR_ONLY:
                if (!IsOnFloor(position, room)) return false;
                break;
                
            case PlacementRule::WALL_ONLY:
                if (!IsOnWall(position, room)) return false;
                break;
                
            case PlacementRule::CEILING_ONLY:
                if (!IsOnCeiling(position, room)) return false;
                break;
                
            case PlacementRule::INDOOR_ONLY:
                // Check if room is indoor (simplified)
                if (room.room_name.find("Garden") != std::string::npos) {
                    return false;
                }
                break;
                
            case PlacementRule::OUTDOOR_ONLY:
                // Check if room is outdoor
                if (room.room_name.find("Garden") == std::string::npos) {
                    return false;
                }
                break;
                
            default:
                break;
        }
    }
    
    return true;
}

// [SEQUENCE: 3375] Check if position is on floor
bool DecorationPlacementValidator::IsOnFloor(const Vector3& position, 
                                            const HouseRoom& room) {
    const float floor_threshold = 0.1f;
    return std::abs(position.y - room.bounds.min.y) < floor_threshold;
}

// [SEQUENCE: 3376] Check if position is on wall
bool DecorationPlacementValidator::IsOnWall(const Vector3& position,
                                           const HouseRoom& room) {
    const float wall_threshold = 0.1f;
    
    return (std::abs(position.x - room.bounds.min.x) < wall_threshold ||
            std::abs(position.x - room.bounds.max.x) < wall_threshold ||
            std::abs(position.z - room.bounds.min.z) < wall_threshold ||
            std::abs(position.z - room.bounds.max.z) < wall_threshold);
}

// [SEQUENCE: 3377] Check if position is on ceiling
bool DecorationPlacementValidator::IsOnCeiling(const Vector3& position,
                                              const HouseRoom& room) {
    const float ceiling_threshold = 0.1f;
    return std::abs(position.y - room.bounds.max.y) < ceiling_threshold;
}

// [SEQUENCE: 3378] Theme data
std::unordered_map<std::string, DecorationTheme::ThemeData> DecorationTheme::themes_ = {
    {"modern", {
        .name = "Modern",
        .description = "Clean lines and minimalist design",
        .primary_colors = {{0.9f, 0.9f, 0.9f, 1.0f}, {0.2f, 0.2f, 0.2f, 1.0f}},
        .accent_colors = {{0.0f, 0.5f, 1.0f, 1.0f}},
        .furniture_ids = {1001, 1002, 1003},
        .lighting_ids = {2001, 2002},
        .ambient_light_level = 0.7f,
        .ambient_light_color = {1.0f, 1.0f, 1.0f, 1.0f}
    }},
    
    {"rustic", {
        .name = "Rustic",
        .description = "Warm wood tones and cozy atmosphere",
        .primary_colors = {{0.6f, 0.4f, 0.2f, 1.0f}, {0.8f, 0.6f, 0.4f, 1.0f}},
        .accent_colors = {{0.8f, 0.2f, 0.0f, 1.0f}},
        .furniture_ids = {1101, 1102, 1103},
        .lighting_ids = {2101, 2102},
        .ambient_light_level = 0.5f,
        .ambient_light_color = {1.0f, 0.9f, 0.8f, 1.0f}
    }},
    
    {"fantasy", {
        .name = "Fantasy",
        .description = "Magical and whimsical decorations",
        .primary_colors = {{0.5f, 0.0f, 0.8f, 1.0f}, {0.0f, 0.8f, 0.8f, 1.0f}},
        .accent_colors = {{1.0f, 0.8f, 0.0f, 1.0f}},
        .furniture_ids = {1201, 1202, 1203},
        .lighting_ids = {2201, 2202},
        .ambient_light_level = 0.4f,
        .ambient_light_color = {0.8f, 0.8f, 1.0f, 1.0f}
    }}
};

// [SEQUENCE: 3379] Get available themes
std::vector<DecorationTheme::ThemeData> DecorationTheme::GetAvailableThemes() {
    std::vector<ThemeData> themes;
    for (const auto& [name, data] : themes_) {
        themes.push_back(data);
    }
    return themes;
}

// [SEQUENCE: 3380] Apply theme to house
void DecorationTheme::ApplyTheme(PlayerHouse& house, const std::string& theme_name) {
    auto it = themes_.find(theme_name);
    if (it == themes_.end()) {
        return;
    }
    
    const auto& theme = it->second;
    
    // Apply to all rooms
    for (auto* room : house.GetAllRooms()) {
        // Set lighting
        room->lighting_level = theme.ambient_light_level;
        
        // Apply material variants to existing decorations
        // (Would need decoration manager integration)
    }
    
    spdlog::info("[DECORATION] Applied theme '{}' to house {}", 
                theme_name, house.GetHouseId());
}

// [SEQUENCE: 3381] Apply light effect
void DecorationEffects::ApplyLightEffect(PlacedDecoration& decoration,
                                        const LightEffect& effect) {
    if (!decoration.GetItem().emits_light) {
        return;
    }
    
    // In real implementation, would update rendering system
    decoration.SetCustomData("light_color", 
        std::to_string(effect.color.r) + "," +
        std::to_string(effect.color.g) + "," +
        std::to_string(effect.color.b));
    
    decoration.SetCustomData("light_intensity", std::to_string(effect.intensity));
    decoration.SetCustomData("light_radius", std::to_string(effect.radius));
    decoration.SetCustomData("light_flicker", effect.flicker ? "true" : "false");
    
    spdlog::debug("[DECORATION] Applied light effect to decoration {}",
                 decoration.GetInstanceId());
}

// [SEQUENCE: 3382] Register decoration item
void DecorationCatalog::RegisterItem(const DecorationItem& item) {
    items_[item.item_id] = item;
    items_by_category_[item.category].push_back(item.item_id);
    
    spdlog::debug("[DECORATION] Registered item {} in category {}",
                 item.name, static_cast<int>(item.category));
}

// [SEQUENCE: 3383] Get decoration item
DecorationItem* DecorationCatalog::GetItem(uint32_t item_id) {
    auto it = items_.find(item_id);
    return it != items_.end() ? &it->second : nullptr;
}

// [SEQUENCE: 3384] Get items by category
std::vector<DecorationItem> DecorationCatalog::GetItemsByCategory(
    DecorationCategory category) {
    
    std::vector<DecorationItem> result;
    
    auto it = items_by_category_.find(category);
    if (it != items_by_category_.end()) {
        for (uint32_t item_id : it->second) {
            if (auto* item = GetItem(item_id)) {
                result.push_back(*item);
            }
        }
    }
    
    return result;
}

// [SEQUENCE: 3385] Search decoration items
std::vector<DecorationItem> DecorationCatalog::SearchItems(const std::string& query) {
    std::vector<DecorationItem> results;
    
    std::string lower_query = query;
    std::transform(lower_query.begin(), lower_query.end(), 
                  lower_query.begin(), ::tolower);
    
    for (const auto& [id, item] : items_) {
        std::string lower_name = item.name;
        std::transform(lower_name.begin(), lower_name.end(),
                      lower_name.begin(), ::tolower);
        
        if (lower_name.find(lower_query) != std::string::npos) {
            results.push_back(item);
        }
    }
    
    return results;
}

// [SEQUENCE: 3386] Filter decoration items
std::vector<DecorationItem> DecorationCatalog::FilterItems(
    const FilterCriteria& criteria) {
    
    std::vector<DecorationItem> results;
    
    for (const auto& [id, item] : items_) {
        bool matches = true;
        
        // Category filter
        if (criteria.category && item.category != *criteria.category) {
            matches = false;
        }
        
        // Interactive filter
        if (criteria.interactive_only && *criteria.interactive_only && 
            !item.is_interactive) {
            matches = false;
        }
        
        // Light emitting filter
        if (criteria.light_emitting_only && *criteria.light_emitting_only &&
            !item.emits_light) {
            matches = false;
        }
        
        // Size filter
        if (criteria.max_size) {
            float item_size = std::max({item.bounds.GetSize().x,
                                      item.bounds.GetSize().y,
                                      item.bounds.GetSize().z});
            if (item_size > *criteria.max_size) {
                matches = false;
            }
        }
        
        if (matches) {
            results.push_back(item);
        }
    }
    
    return results;
}

// [SEQUENCE: 3387] House decoration manager constructor
HouseDecorationManager::HouseDecorationManager(PlayerHouse& house)
    : house_(house) {
}

// [SEQUENCE: 3388] Place decoration
std::shared_ptr<PlacedDecoration> HouseDecorationManager::PlaceDecoration(
    uint32_t item_id,
    uint32_t room_id,
    const Vector3& position,
    const Quaternion& rotation) {
    
    // Get item from catalog
    auto* item = DecorationCatalog::Instance().GetItem(item_id);
    if (!item) {
        spdlog::warn("[DECORATION] Item {} not found in catalog", item_id);
        return nullptr;
    }
    
    // Get room
    auto* room = house_.GetRoom(room_id);
    if (!room) {
        spdlog::warn("[DECORATION] Room {} not found", room_id);
        return nullptr;
    }
    
    // Get existing decorations in room
    std::vector<PlacedDecoration*> existing;
    for (uint64_t dec_id : decorations_by_room_[room_id]) {
        if (auto it = decorations_.find(dec_id); it != decorations_.end()) {
            existing.push_back(it->second.get());
        }
    }
    
    // Validate placement
    auto validation = DecorationPlacementValidator::ValidatePlacement(
        *item, position, *room, existing);
    
    if (!validation.is_valid) {
        for (const auto& error : validation.errors) {
            spdlog::warn("[DECORATION] Placement error: {}", error);
        }
        return nullptr;
    }
    
    // Create decoration
    uint64_t instance_id = next_instance_id_++;
    auto decoration = std::make_shared<PlacedDecoration>(instance_id, *item);
    decoration->SetPosition(position);
    decoration->SetRotation(rotation);
    
    // Store decoration
    decorations_[instance_id] = decoration;
    decorations_by_room_[room_id].push_back(instance_id);
    
    spdlog::info("[DECORATION] Placed {} in room {} at ({}, {}, {})",
                item->name, room_id, position.x, position.y, position.z);
    
    return decoration;
}

// [SEQUENCE: 3389] Remove decoration
bool HouseDecorationManager::RemoveDecoration(uint64_t instance_id) {
    auto it = decorations_.find(instance_id);
    if (it == decorations_.end()) {
        return false;
    }
    
    // Remove from room list
    for (auto& [room_id, decoration_ids] : decorations_by_room_) {
        decoration_ids.erase(
            std::remove(decoration_ids.begin(), decoration_ids.end(), instance_id),
            decoration_ids.end()
        );
    }
    
    decorations_.erase(it);
    
    spdlog::info("[DECORATION] Removed decoration {}", instance_id);
    return true;
}

// [SEQUENCE: 3390] Get room decorations
std::vector<std::shared_ptr<PlacedDecoration>> 
HouseDecorationManager::GetRoomDecorations(uint32_t room_id) {
    std::vector<std::shared_ptr<PlacedDecoration>> result;
    
    auto it = decorations_by_room_.find(room_id);
    if (it != decorations_by_room_.end()) {
        for (uint64_t dec_id : it->second) {
            if (auto dec_it = decorations_.find(dec_id); 
                dec_it != decorations_.end()) {
                result.push_back(dec_it->second);
            }
        }
    }
    
    return result;
}

// [SEQUENCE: 3391] Get decoration statistics
HouseDecorationManager::DecorationStats HouseDecorationManager::GetStatistics() const {
    DecorationStats stats;
    stats.total_decorations = decorations_.size();
    
    for (const auto& [id, decoration] : decorations_) {
        const auto& item = decoration->GetItem();
        
        stats.by_category[item.category]++;
        
        if (item.is_interactive) {
            stats.interactive_count++;
        }
        
        if (item.emits_light) {
            stats.light_sources++;
        }
        
        stats.total_value += DecorationUtils::CalculateDecorationValue(item);
    }
    
    return stats;
}

// [SEQUENCE: 3392] Get seasonal items
std::vector<DecorationItem> SeasonalDecorationManager::GetSeasonalItems(Season season) {
    std::vector<DecorationItem> items;
    
    // Filter catalog for seasonal items
    auto all_seasonal = DecorationCatalog::Instance().GetItemsByCategory(
        DecorationCategory::SEASONAL);
    
    for (const auto& item : all_seasonal) {
        // Check if item matches season (would check item metadata)
        items.push_back(item);
    }
    
    return items;
}

// [SEQUENCE: 3393] Auto-decorate for season
void SeasonalDecorationManager::AutoDecorateForSeason(PlayerHouse& house, Season season) {
    HouseDecorationManager dec_manager(house);
    
    // Get seasonal items
    auto seasonal_items = GetSeasonalItems(season);
    
    // Place seasonal decorations in appropriate rooms
    for (auto* room : house.GetAllRooms()) {
        // Example: Place wreath on door during winter
        if (season == Season::WINTER && room->room_name == "Entrance") {
            for (const auto& item : seasonal_items) {
                if (item.name.find("Wreath") != std::string::npos) {
                    dec_manager.PlaceDecoration(item.item_id, room->room_id,
                                              {0, 2, 0});  // Door position
                    break;
                }
            }
        }
    }
    
    spdlog::info("[DECORATION] Auto-decorated house {} for season {}",
                house.GetHouseId(), static_cast<int>(season));
}

// [SEQUENCE: 3394] Decoration interactions
std::unordered_map<uint32_t, 
    std::unordered_map<std::string, DecorationInteractionHandler::InteractionCallback>>
    DecorationInteractionHandler::interactions_;

void DecorationInteractionHandler::RegisterInteraction(
    uint32_t item_id,
    const std::string& action_name,
    InteractionCallback callback) {
    
    interactions_[item_id][action_name] = callback;
    
    spdlog::debug("[DECORATION] Registered interaction '{}' for item {}",
                 action_name, item_id);
}

// [SEQUENCE: 3395] Execute interaction
bool DecorationInteractionHandler::ExecuteInteraction(
    Player& player,
    PlacedDecoration& decoration,
    const std::string& action_name) {
    
    auto it = interactions_.find(decoration.GetItemId());
    if (it == interactions_.end()) {
        return false;
    }
    
    auto action_it = it->second.find(action_name);
    if (action_it == it->second.end()) {
        return false;
    }
    
    // Execute callback
    action_it->second(player, decoration);
    
    spdlog::info("[DECORATION] Player {} interacted with decoration {} ({})",
                player.GetId(), decoration.GetInstanceId(), action_name);
    
    return true;
}

// [SEQUENCE: 3396] Decoration utility functions
namespace DecorationUtils {

bool IsValidPosition(const Vector3& position, const BoundingBox& room_bounds) {
    return room_bounds.Contains(position);
}

Vector3 SnapToGrid(const Vector3& position, float grid_size) {
    return Vector3{
        std::round(position.x / grid_size) * grid_size,
        std::round(position.y / grid_size) * grid_size,
        std::round(position.z / grid_size) * grid_size
    };
}

Vector3 SnapToSurface(const Vector3& position, const HouseRoom& room) {
    Vector3 snapped = position;
    
    // Find nearest surface
    float min_dist = std::numeric_limits<float>::max();
    
    // Check floor
    float floor_dist = std::abs(position.y - room.bounds.min.y);
    if (floor_dist < min_dist) {
        min_dist = floor_dist;
        snapped.y = room.bounds.min.y;
    }
    
    // Check walls
    float wall_dists[] = {
        std::abs(position.x - room.bounds.min.x),
        std::abs(position.x - room.bounds.max.x),
        std::abs(position.z - room.bounds.min.z),
        std::abs(position.z - room.bounds.max.z)
    };
    
    for (int i = 0; i < 4; ++i) {
        if (wall_dists[i] < min_dist) {
            min_dist = wall_dists[i];
            snapped = position;
            
            switch (i) {
                case 0: snapped.x = room.bounds.min.x; break;
                case 1: snapped.x = room.bounds.max.x; break;
                case 2: snapped.z = room.bounds.min.z; break;
                case 3: snapped.z = room.bounds.max.z; break;
            }
        }
    }
    
    return snapped;
}

Color BlendColors(const Color& a, const Color& b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return Color{
        a.r * (1 - t) + b.r * t,
        a.g * (1 - t) + b.g * t,
        a.b * (1 - t) + b.b * t,
        a.a * (1 - t) + b.a * t
    };
}

float CalculateLightAttenuation(float distance, float radius) {
    if (distance >= radius) return 0.0f;
    
    float normalized = distance / radius;
    return 1.0f - normalized * normalized;  // Quadratic falloff
}

uint64_t CalculateDecorationValue(const DecorationItem& item, float condition) {
    // Base value calculation (placeholder)
    uint64_t base_value = 100;
    
    // Category multiplier
    switch (item.category) {
        case DecorationCategory::SPECIAL:
            base_value *= 10;
            break;
        case DecorationCategory::SEASONAL:
            base_value *= 5;
            break;
        case DecorationCategory::FURNITURE:
            base_value *= 3;
            break;
        default:
            break;
    }
    
    // Feature bonuses
    if (item.emits_light) base_value *= 1.5f;
    if (item.is_interactive) base_value *= 2.0f;
    if (item.has_animation) base_value *= 1.5f;
    if (item.has_particle_effect) base_value *= 2.0f;
    
    // Apply condition
    return static_cast<uint64_t>(base_value * condition);
}

} // namespace DecorationUtils

} // namespace mmorpg::housing