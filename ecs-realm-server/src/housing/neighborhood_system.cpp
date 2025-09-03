#include "neighborhood_system.h"
#include "../core/logger.h"
#include "../player/player.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

namespace mmorpg::housing {

// [SEQUENCE: 3574] Neighborhood constructor
Neighborhood::Neighborhood(const Properties& props)
    : properties_(props) {
    
    // Generate layout based on type
    switch (props.type) {
        case NeighborhoodType::RESIDENTIAL:
            layout_ = NeighborhoodUtils::GenerateSuburbanLayout(props.max_houses);
            break;
        case NeighborhoodType::COMMERCIAL:
        case NeighborhoodType::ARTISAN:
            layout_ = NeighborhoodUtils::GenerateUrbanLayout(props.max_houses);
            break;
        default:
            layout_ = NeighborhoodUtils::GenerateRuralLayout(props.max_houses);
            break;
    }
    
    CalculateDesirability();
}

// [SEQUENCE: 3575] Get available plot
Neighborhood::Plot* Neighborhood::GetAvailablePlot(HouseType type) const {
    for (auto& plot : layout_.plots) {
        if (!plot.is_occupied && plot.allowed_type == type) {
            return const_cast<Plot*>(&plot);
        }
    }
    
    // Try any compatible plot
    for (auto& plot : layout_.plots) {
        if (!plot.is_occupied) {
            return const_cast<Plot*>(&plot);
        }
    }
    
    return nullptr;
}

// [SEQUENCE: 3576] Reserve plot for player
bool Neighborhood::ReservePlot(uint32_t plot_id, uint64_t player_id) {
    if (plot_id >= layout_.plots.size()) {
        return false;
    }
    
    auto& plot = layout_.plots[plot_id];
    if (plot.is_occupied) {
        return false;
    }
    
    plot.is_occupied = true;
    plot.owner_id = player_id;
    properties_.current_houses++;
    
    InvalidateNeighborCache();
    UpdateDesirability();
    
    spdlog::info("[NEIGHBORHOOD] Plot {} reserved for player {} in neighborhood {}",
                plot_id, player_id, properties_.neighborhood_id);
    
    return true;
}

// [SEQUENCE: 3577] Release plot
bool Neighborhood::ReleasePlot(uint32_t plot_id) {
    if (plot_id >= layout_.plots.size()) {
        return false;
    }
    
    auto& plot = layout_.plots[plot_id];
    if (!plot.is_occupied) {
        return false;
    }
    
    plot.is_occupied = false;
    plot.house_id = 0;
    plot.owner_id = 0;
    properties_.current_houses--;
    
    InvalidateNeighborCache();
    UpdateDesirability();
    
    return true;
}

// [SEQUENCE: 3578] Get neighbor houses
std::vector<uint64_t> Neighborhood::GetNeighborHouses(uint64_t house_id,
                                                     float radius) const {
    // Check cache
    auto now = std::chrono::steady_clock::now();
    if (now < cache_expiry_) {
        auto it = neighbor_cache_.find(house_id);
        if (it != neighbor_cache_.end()) {
            return it->second;
        }
    }
    
    std::vector<uint64_t> neighbors;
    
    // Find source plot
    Plot* source_plot = nullptr;
    for (const auto& plot : layout_.plots) {
        if (plot.house_id == house_id) {
            source_plot = const_cast<Plot*>(&plot);
            break;
        }
    }
    
    if (!source_plot) {
        return neighbors;
    }
    
    // Find neighbors within radius
    for (const auto& plot : layout_.plots) {
        if (plot.is_occupied && plot.house_id != house_id) {
            float dist = Vector3::Distance(source_plot->position, plot.position);
            if (dist <= radius) {
                neighbors.push_back(plot.house_id);
            }
        }
    }
    
    // Cache result
    neighbor_cache_[house_id] = neighbors;
    cache_expiry_ = now + std::chrono::minutes(5);
    
    return neighbors;
}

// [SEQUENCE: 3579] Update desirability score
void Neighborhood::UpdateDesirability() {
    properties_.desirability_score = NeighborhoodUtils::CalculateDesirability(
        properties_.features,
        properties_.current_houses,
        properties_.average_property_value
    );
}

// [SEQUENCE: 3580] Add community feature
void Neighborhood::AddCommunityFeature(const std::string& feature_type,
                                     const Vector3& position) {
    NeighborhoodLayout::CommonArea area;
    area.name = feature_type;
    area.area_type = feature_type;
    area.bounds = BoundingBox(position - Vector3(10, 0, 10),
                             position + Vector3(10, 5, 10));
    
    layout_.common_areas.push_back(area);
    
    // Update features
    if (feature_type == "park") {
        properties_.features.has_park = true;
    } else if (feature_type == "market") {
        properties_.features.has_market = true;
    }
    
    UpdateDesirability();
    
    spdlog::info("[NEIGHBORHOOD] Added {} to neighborhood {} at ({}, {}, {})",
                feature_type, properties_.neighborhood_id,
                position.x, position.y, position.z);
}

// [SEQUENCE: 3581] Start seasonal event
void Neighborhood::StartSeasonalEvent(const std::string& event_type) {
    active_event_ = event_type;
    event_end_time_ = std::chrono::system_clock::now() + std::chrono::hours(24);
    
    spdlog::info("[NEIGHBORHOOD] Started {} event in neighborhood {}",
                event_type, properties_.neighborhood_id);
}

// [SEQUENCE: 3582] Get neighborhood statistics
Neighborhood::Statistics Neighborhood::GetStatistics() const {
    Statistics stats;
    stats.total_residents = properties_.current_houses;
    
    // Calculate total property value
    for (const auto& plot : layout_.plots) {
        if (plot.is_occupied) {
            stats.total_property_value += NeighborhoodUtils::CalculatePropertyValue(
                plot, *this);
        }
    }
    
    return stats;
}

// [SEQUENCE: 3583] Create neighborhood
std::shared_ptr<Neighborhood> NeighborhoodManager::CreateNeighborhood(
    const std::string& name,
    NeighborhoodType type,
    const Vector3& location) {
    
    Neighborhood::Properties props;
    props.neighborhood_id = next_neighborhood_id_++;
    props.name = name;
    props.type = type;
    props.center_position = location;
    
    // Set features based on type
    switch (type) {
        case NeighborhoodType::COMMERCIAL:
            props.features.has_market = true;
            props.features.has_bank = true;
            props.max_houses = 100;
            break;
            
        case NeighborhoodType::ARTISAN:
            props.features.has_crafting_hub = true;
            props.max_houses = 40;
            break;
            
        case NeighborhoodType::NOBLE:
            props.features.has_fountain = true;
            props.features.has_park = true;
            props.features.has_special_npcs = true;
            props.max_houses = 20;
            break;
            
        case NeighborhoodType::WATERFRONT:
            props.features.has_ocean_view = true;
            props.max_houses = 30;
            break;
            
        default:
            props.max_houses = 50;
            break;
    }
    
    auto neighborhood = std::make_shared<Neighborhood>(props);
    RegisterNeighborhood(neighborhood);
    
    spdlog::info("[NEIGHBORHOOD] Created {} neighborhood '{}' with ID {}",
                static_cast<int>(type), name, props.neighborhood_id);
    
    return neighborhood;
}

// [SEQUENCE: 3584] Register neighborhood
void NeighborhoodManager::RegisterNeighborhood(
    std::shared_ptr<Neighborhood> neighborhood) {
    
    neighborhoods_[neighborhood->properties_.neighborhood_id] = neighborhood;
    
    // Add to zone index
    zone_neighborhoods_[neighborhood->properties_.world_zone_id].push_back(
        neighborhood->properties_.neighborhood_id);
}

// [SEQUENCE: 3585] Find best neighborhood
std::shared_ptr<Neighborhood> NeighborhoodManager::FindBestNeighborhood(
    HouseType house_type,
    uint64_t budget) {
    
    std::shared_ptr<Neighborhood> best = nullptr;
    float best_score = 0.0f;
    
    for (const auto& [id, neighborhood] : neighborhoods_) {
        // Check if has available plots
        if (neighborhood->GetAvailablePlot(house_type) == nullptr) {
            continue;
        }
        
        // Calculate score based on desirability and affordability
        float desirability = neighborhood->GetDesirabilityScore() / 100.0f;
        float affordability = 1.0f;
        
        if (neighborhood->properties_.average_property_value > 0) {
            affordability = std::min(1.0f, 
                budget / static_cast<float>(neighborhood->properties_.average_property_value));
        }
        
        float score = desirability * 0.6f + affordability * 0.4f;
        
        if (score > best_score) {
            best_score = score;
            best = neighborhood;
        }
    }
    
    return best;
}

// [SEQUENCE: 3586] Allocate plot
NeighborhoodManager::PlotAllocation NeighborhoodManager::AllocatePlot(
    uint64_t player_id,
    HouseType type,
    NeighborhoodType preferred_type) {
    
    PlotAllocation allocation;
    
    // First try preferred type
    auto neighborhoods = GetNeighborhoodsByType(preferred_type);
    for (auto& neighborhood : neighborhoods) {
        auto* plot = neighborhood->GetAvailablePlot(type);
        if (plot) {
            allocation.neighborhood_id = neighborhood->properties_.neighborhood_id;
            allocation.plot_id = plot->plot_id;
            allocation.plot_info = plot;
            
            if (neighborhood->ReservePlot(plot->plot_id, player_id)) {
                return allocation;
            }
        }
    }
    
    // Try any neighborhood
    for (const auto& [id, neighborhood] : neighborhoods_) {
        auto* plot = neighborhood->GetAvailablePlot(type);
        if (plot) {
            allocation.neighborhood_id = neighborhood->properties_.neighborhood_id;
            allocation.plot_id = plot->plot_id;
            allocation.plot_info = plot;
            
            if (neighborhood->ReservePlot(plot->plot_id, player_id)) {
                return allocation;
            }
        }
    }
    
    // No plots available
    allocation.neighborhood_id = 0;
    return allocation;
}

// [SEQUENCE: 3587] Update all neighborhoods
void NeighborhoodManager::UpdateAllNeighborhoods() {
    for (auto& [id, neighborhood] : neighborhoods_) {
        neighborhood->UpdateDesirability();
        
        // Check and end expired events
        if (neighborhood->IsEventActive()) {
            auto now = std::chrono::system_clock::now();
            if (now >= neighborhood->event_end_time_) {
                neighborhood->EndSeasonalEvent();
            }
        }
    }
}

// [SEQUENCE: 3588] Record neighbor interaction
void CommunityInteraction::RecordInteraction(uint64_t player1, uint64_t player2,
                                           const std::string& interaction_type) {
    // Ensure consistent ordering
    if (player1 > player2) {
        std::swap(player1, player2);
    }
    
    auto key = std::make_pair(player1, player2);
    auto& relation = relationships_[key];
    
    relation.player_id_1 = player1;
    relation.player_id_2 = player2;
    relation.last_interaction = std::chrono::system_clock::now();
    
    // Update relationship points
    int32_t points = 0;
    if (interaction_type == "chat") {
        points = 1;
    } else if (interaction_type == "help") {
        points = 5;
    } else if (interaction_type == "gift") {
        points = 10;
    } else if (interaction_type == "visit") {
        points = 2;
    }
    
    relation.relationship_points += points;
    
    // Update relationship type
    if (relation.relationship_points >= 100) {
        relation.type = RelationshipType::BEST_FRIEND;
    } else if (relation.relationship_points >= 50) {
        relation.type = RelationshipType::FRIEND;
    } else if (relation.relationship_points >= 20) {
        relation.type = RelationshipType::NEIGHBOR;
    } else if (relation.relationship_points >= 5) {
        relation.type = RelationshipType::ACQUAINTANCE;
    }
    
    spdlog::debug("[NEIGHBORHOOD] Recorded {} interaction between {} and {}",
                 interaction_type, player1, player2);
}

// [SEQUENCE: 3589] Get relationship type
CommunityInteraction::RelationshipType CommunityInteraction::GetRelationship(
    uint64_t player1, uint64_t player2) const {
    
    if (player1 > player2) {
        std::swap(player1, player2);
    }
    
    auto key = std::make_pair(player1, player2);
    auto it = relationships_.find(key);
    
    if (it != relationships_.end()) {
        return it->second.type;
    }
    
    return RelationshipType::STRANGER;
}

// [SEQUENCE: 3590] Update reputation
void CommunityInteraction::UpdateReputation(uint64_t player_id,
                                          const std::string& action) {
    auto& rep = reputations_[player_id];
    
    if (action == "help_neighbor") {
        rep.helpful_score += 2;
    } else if (action == "organize_event") {
        rep.event_participation += 5;
        rep.friendly_score += 2;
    } else if (action == "attend_event") {
        rep.event_participation += 1;
    } else if (action == "decorate_house") {
        rep.decoration_score += 1;
    } else if (action == "friendly_chat") {
        rep.friendly_score += 1;
    }
    
    spdlog::debug("[NEIGHBORHOOD] Updated reputation for player {} after {}",
                 player_id, action);
}

// [SEQUENCE: 3591] Add neighborhood service
void NeighborhoodServices::AddService(uint32_t neighborhood_id,
                                    const ServicePoint& service) {
    services_[neighborhood_id].push_back(service);
    
    spdlog::info("[NEIGHBORHOOD] Added {} service to neighborhood {}",
                service.service_type, neighborhood_id);
}

// [SEQUENCE: 3592] Find nearest service
NeighborhoodServices::ServicePoint* NeighborhoodServices::FindNearestService(
    const Vector3& position,
    const std::string& service_type) {
    
    ServicePoint* nearest = nullptr;
    float min_distance = std::numeric_limits<float>::max();
    
    for (auto& [neighborhood_id, services] : services_) {
        for (auto& service : services) {
            if (service.service_type == service_type && service.is_available) {
                float dist = Vector3::Distance(position, service.position);
                if (dist < min_distance) {
                    min_distance = dist;
                    nearest = &service;
                }
            }
        }
    }
    
    return nearest;
}

// [SEQUENCE: 3593] Create community event
uint32_t NeighborhoodEvents::CreateEvent(const CommunityEvent& event) {
    uint32_t event_id = next_event_id_++;
    
    CommunityEvent new_event = event;
    new_event.event_id = event_id;
    
    events_[event_id] = new_event;
    neighborhood_events_[event.neighborhood_id].push_back(event_id);
    
    spdlog::info("[NEIGHBORHOOD] Created event '{}' (ID: {}) in neighborhood {}",
                event.name, event_id, event.neighborhood_id);
    
    return event_id;
}

// [SEQUENCE: 3594] Register for event
bool NeighborhoodEvents::RegisterForEvent(uint32_t event_id, uint64_t player_id) {
    auto it = events_.find(event_id);
    if (it == events_.end()) {
        return false;
    }
    
    auto& event = it->second;
    
    // Check if already registered
    if (std::find(event.participants.begin(), event.participants.end(), 
                  player_id) != event.participants.end()) {
        return false;
    }
    
    // Check capacity
    if (event.participants.size() >= event.max_participants) {
        return false;
    }
    
    event.participants.push_back(player_id);
    
    spdlog::debug("[NEIGHBORHOOD] Player {} registered for event {}",
                 player_id, event_id);
    
    return true;
}

// [SEQUENCE: 3595] Start block party event
void NeighborhoodEvents::StartBlockParty(uint32_t neighborhood_id) {
    CommunityEvent event;
    event.name = "Block Party";
    event.description = "A fun neighborhood gathering!";
    event.neighborhood_id = neighborhood_id;
    event.start_time = std::chrono::system_clock::now();
    event.end_time = event.start_time + std::chrono::hours(3);
    event.max_participants = 100;
    
    // Add rewards
    event.rewards.push_back({"reputation", 10});
    event.rewards.push_back({"gold", 50});
    
    CreateEvent(event);
}

// [SEQUENCE: 3596] Neighborhood utility functions
namespace NeighborhoodUtils {

float GetDistanceBetweenHouses(uint64_t house1_id, uint64_t house2_id) {
    // In real implementation, would look up house positions
    return 0.0f;
}

float CalculatePropertyValue(const Neighborhood::Plot& plot,
                           const Neighborhood& neighborhood) {
    float base_value = 10000.0f;
    
    // Size bonus
    float size_mult = plot.dimensions.x * plot.dimensions.z / 100.0f;
    base_value *= size_mult;
    
    // Location bonuses
    if (plot.is_corner_lot) {
        base_value *= 1.2f;
    }
    if (plot.has_water_access) {
        base_value *= 1.5f;
    }
    
    // Neighborhood desirability
    float desirability_mult = neighborhood.GetDesirabilityScore() / 50.0f;
    base_value *= desirability_mult;
    
    return base_value;
}

float CalculateDesirability(const NeighborhoodFeatures& features,
                          uint32_t house_count,
                          float average_property_value) {
    float score = 50.0f;  // Base score
    
    // Feature bonuses
    if (features.has_market) score += 10;
    if (features.has_crafting_hub) score += 8;
    if (features.has_guild_hall) score += 5;
    if (features.has_park) score += 15;
    if (features.has_fountain) score += 5;
    if (features.has_teleporter) score += 10;
    if (features.has_bank) score += 10;
    
    // Special features
    if (features.has_ocean_view) score += 20;
    if (features.has_mountain_view) score += 15;
    if (features.has_special_npcs) score += 10;
    if (features.has_seasonal_events) score += 5;
    
    // Occupancy penalty for overcrowding
    if (house_count > 30) {
        score -= (house_count - 30) * 0.5f;
    }
    
    // Property value influence
    if (average_property_value > 50000) {
        score += 10;
    }
    
    return std::clamp(score, 0.0f, 100.0f);
}

NeighborhoodLayout GenerateSuburbanLayout(uint32_t plot_count) {
    NeighborhoodLayout layout;
    
    // Grid layout with curved roads
    float plot_size = 20.0f;
    float road_width = 5.0f;
    float block_size = plot_size * 4 + road_width;
    
    uint32_t grid_size = static_cast<uint32_t>(std::sqrt(plot_count)) + 1;
    
    for (uint32_t y = 0; y < grid_size; ++y) {
        for (uint32_t x = 0; x < grid_size; ++x) {
            if (layout.plots.size() >= plot_count) break;
            
            Neighborhood::Plot plot;
            plot.plot_id = layout.plots.size();
            plot.position = Vector3(
                x * (plot_size + road_width),
                0,
                y * (plot_size + road_width)
            );
            plot.dimensions = Vector3(plot_size, 10, plot_size);
            plot.rotation = 0;
            plot.allowed_type = HouseType::ROOM;
            plot.has_garden_space = true;
            plot.is_corner_lot = (x == 0 || x == grid_size - 1) && 
                                (y == 0 || y == grid_size - 1);
            
            layout.plots.push_back(plot);
        }
    }
    
    // Add roads
    for (uint32_t i = 0; i <= grid_size; ++i) {
        // Horizontal roads
        NeighborhoodLayout::Road h_road;
        h_road.path_points.push_back(Vector3(0, 0, i * (plot_size + road_width)));
        h_road.path_points.push_back(Vector3(grid_size * (plot_size + road_width), 
                                           0, i * (plot_size + road_width)));
        h_road.width = road_width;
        h_road.is_main_road = (i % 4 == 0);
        layout.roads.push_back(h_road);
        
        // Vertical roads
        NeighborhoodLayout::Road v_road;
        v_road.path_points.push_back(Vector3(i * (plot_size + road_width), 0, 0));
        v_road.path_points.push_back(Vector3(i * (plot_size + road_width), 0, 
                                           grid_size * (plot_size + road_width)));
        v_road.width = road_width;
        v_road.is_main_road = (i % 4 == 0);
        layout.roads.push_back(v_road);
    }
    
    // Add common areas
    Vector3 center(grid_size * plot_size / 2, 0, grid_size * plot_size / 2);
    
    NeighborhoodLayout::CommonArea park;
    park.name = "Central Park";
    park.area_type = "park";
    park.bounds = BoundingBox(center - Vector3(30, 0, 30),
                             center + Vector3(30, 10, 30));
    layout.common_areas.push_back(park);
    
    return layout;
}

} // namespace NeighborhoodUtils

} // namespace mmorpg::housing