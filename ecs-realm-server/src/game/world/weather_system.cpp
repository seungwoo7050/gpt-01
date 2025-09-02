#include "weather_system.h"
#include <fstream>
#include <nlohmann/json.hpp>

namespace mmorpg::game::world {

using json = nlohmann::json;

// [SEQUENCE: MVP7-96] Update loop for weather system
void WeatherSystem::Update(float delta_time) {
    auto now = std::chrono::steady_clock::now();
    if (now - last_update_time_ < std::chrono::minutes(5)) {
        return; // Update weather every 5 minutes
    }
    
    last_update_time_ = now;
    
    for (auto& zone : zones_) {
        UpdateZoneWeather(zone);
    }
}

// [SEQUENCE: MVP7-97] Update weather for a single zone
void WeatherSystem::UpdateZoneWeather(WeatherZone& zone) {
    // Simple probability-based weather change
    float roll = static_cast<float>(rand()) / RAND_MAX;
    float cumulative_prob = 0.0f;
    
    for (const auto& weather_prob : zone.weather_probabilities) {
        cumulative_prob += weather_prob.second;
        if (roll <= cumulative_prob) {
            active_weather_[zone.id] = {weather_prob.first, std::chrono::steady_clock::now() + std::chrono::hours(1), 1.0f};
            break;
        }
    }
}

// [SEQUENCE: MVP7-98] Get current weather effect at a position
WeatherEffect WeatherSystem::GetCurrentEffect(float x, float y, float z) const {
    // Find which zone the position is in and return its effect
    // This is a simplified version. A real implementation would handle transitions between zones.
    for (const auto& zone : zones_) {
        if (x >= zone.boundary.min_x && x <= zone.boundary.max_x &&
            y >= zone.boundary.min_y && y <= zone.boundary.max_y) {
            
            auto it = active_weather_.find(zone.id);
            if (it != active_weather_.end()) {
                // Here you would map WeatherType to a specific WeatherEffect
                // For simplicity, returning a default effect.
                return WeatherEffect();
            }
        }
    }
    return WeatherEffect(); // Default clear weather
}

// [SEQUENCE: MVP7-99] Load weather zones from a JSON file
void WeatherSystem::LoadWeatherZones(const std::string& file_path) {
    std::ifstream f(file_path);
    json data = json::parse(f);
    
    for (const auto& item : data["weather_zones"]) {
        WeatherZone zone;
        zone.id = item.at("id").get<uint32_t>();
        // ... load boundary and probabilities ...
        zones_.push_back(zone);
    }
}

// [SEQUENCE: MVP7-100] Force a weather change
void WeatherSystem::ForceWeather(uint32_t zone_id, WeatherType type, std::chrono::seconds duration) {
    active_weather_[zone_id] = {type, std::chrono::steady_clock::now() + duration, 1.0f};
}

} // namespace mmorpg::game::world
