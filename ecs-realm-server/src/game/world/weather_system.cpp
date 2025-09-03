#include "weather_system.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <numeric>

namespace mmorpg::game::world {

// [SEQUENCE: 664] Register a weather zone
void WeatherManager::RegisterWeatherZone(const WeatherZone& zone) {
    weather_zones_[zone.zone_id] = zone;
    spdlog::info("Registered weather zone {} - {} with climate {}", 
                 zone.zone_id, zone.zone_name, zone.climate_type);
}

// [SEQUENCE: 665] Main weather update loop
void WeatherManager::Update(float delta_time) {
    auto now = std::chrono::steady_clock::now();
    
    // Update each weather zone
    for (auto& [zone_id, zone] : weather_zones_) {
        UpdateZoneWeather(zone, delta_time);
    }
    
    last_global_update_ = now;
}

// [SEQUENCE: 666] Update weather for a specific zone
void WeatherManager::UpdateZoneWeather(WeatherZone& zone, float delta_time) {
    auto& state = zone.state;
    
    // [SEQUENCE: 667] Process ongoing weather transition
    if (state.transition_progress < 1.0f && 
        state.target_type != state.current_type) {
        ProcessWeatherTransition(state, delta_time);
    }
    
    // [SEQUENCE: 668] Check if current weather duration expired
    if (state.duration_remaining.count() > 0) {
        state.duration_remaining -= std::chrono::duration_cast<std::chrono::minutes>(
            std::chrono::duration<float>(delta_time / 60.0f)
        );
        
        if (state.duration_remaining.count() <= 0) {
            // Time to change weather
            auto [next_type, next_intensity] = DetermineNextWeather(zone);
            StartWeatherTransition(zone.zone_id, next_type, next_intensity, 
                                 std::chrono::seconds(300));
        }
    }
    
    // [SEQUENCE: 669] Check for extreme weather events
    if (ShouldTriggerExtremeWeather(zone)) {
        // Trigger special weather event
        WeatherType extreme_type = WeatherType::MAGICAL_STORM;
        if (zone.climate_type == "desert") {
            extreme_type = WeatherType::SANDSTORM;
        } else if (zone.climate_type == "arctic") {
            extreme_type = WeatherType::BLIZZARD;
        } else {
            extreme_type = WeatherType::STORM;
        }
        
        StartWeatherTransition(zone.zone_id, extreme_type, 
                             WeatherIntensity::EXTREME, 
                             std::chrono::seconds(180));
        
        spdlog::warn("Extreme weather event triggered in zone {}", zone.zone_id);
    }
}

// [SEQUENCE: 670] Process weather transition
void WeatherManager::ProcessWeatherTransition(WeatherState& state, float delta_time) {
    // Calculate transition speed
    float transition_speed = 1.0f / state.transition_duration.count();
    state.transition_progress += transition_speed * delta_time;
    
    if (state.transition_progress >= 1.0f) {
        // Transition complete
        state.transition_progress = 1.0f;
        state.current_type = state.target_type;
        state.intensity = state.target_intensity;
        state.weather_started = std::chrono::steady_clock::now();
        
        // Update effects
        state.effects = CalculateWeatherEffects(state.current_type, state.intensity);
        
        spdlog::info("Weather transition complete: {} at intensity {}", 
                    static_cast<int>(state.current_type), 
                    static_cast<int>(state.intensity));
    } else {
        // Blend effects during transition
        auto current_effects = CalculateWeatherEffects(state.current_type, state.intensity);
        auto target_effects = CalculateWeatherEffects(state.target_type, state.target_intensity);
        
        // Linear interpolation of effects
        float t = state.transition_progress;
        state.effects.visibility_modifier = 
            current_effects.visibility_modifier * (1 - t) + target_effects.visibility_modifier * t;
        state.effects.movement_speed_modifier = 
            current_effects.movement_speed_modifier * (1 - t) + target_effects.movement_speed_modifier * t;
        state.effects.accuracy_modifier = 
            current_effects.accuracy_modifier * (1 - t) + target_effects.accuracy_modifier * t;
        state.effects.spell_power_modifier = 
            current_effects.spell_power_modifier * (1 - t) + target_effects.spell_power_modifier * t;
        state.effects.damage_per_second = 
            current_effects.damage_per_second * (1 - t) + target_effects.damage_per_second * t;
    }
}

// [SEQUENCE: 671] Get weather at specific location
WeatherState WeatherManager::GetWeatherAt(float x, float y, float z) const {
    // Find zones that contain this position
    std::vector<const WeatherZone*> overlapping_zones;
    std::vector<float> distances;
    
    for (const auto& [zone_id, zone] : weather_zones_) {
        if (x >= zone.min_x && x <= zone.max_x &&
            y >= zone.min_y && y <= zone.max_y &&
            z >= zone.min_z && z <= zone.max_z) {
            overlapping_zones.push_back(&zone);
            
            // Calculate distance to zone center for weighting
            float center_x = (zone.min_x + zone.max_x) / 2;
            float center_y = (zone.min_y + zone.max_y) / 2;
            float center_z = (zone.min_z + zone.max_z) / 2;
            
            float dx = x - center_x;
            float dy = y - center_y;
            float dz = z - center_z;
            float distance = std::sqrt(dx*dx + dy*dy + dz*dz);
            distances.push_back(1.0f / (1.0f + distance));  // Inverse distance weighting
        }
    }
    
    if (overlapping_zones.empty()) {
        // No weather zone - return clear weather
        WeatherState clear_state;
        clear_state.current_type = WeatherType::CLEAR;
        clear_state.intensity = WeatherIntensity::NONE;
        clear_state.effects = WeatherPresets::GetEffects(WeatherType::CLEAR, WeatherIntensity::NONE);
        return clear_state;
    }
    
    if (overlapping_zones.size() == 1) {
        // Single zone - return its weather
        return overlapping_zones[0]->state;
    }
    
    // [SEQUENCE: 672] Multiple zones - blend weather states
    // Normalize weights
    float total_weight = std::accumulate(distances.begin(), distances.end(), 0.0f);
    for (auto& d : distances) {
        d /= total_weight;
    }
    
    std::vector<WeatherState> states;
    for (const auto* zone : overlapping_zones) {
        states.push_back(zone->state);
    }
    
    return BlendWeatherStates(states, distances);
}

// [SEQUENCE: 673] Determine next weather based on pattern
std::pair<WeatherType, WeatherIntensity> WeatherManager::DetermineNextWeather(
    const WeatherZone& zone) const {
    
    const auto& pattern = zone.pattern;
    
    // [SEQUENCE: 674] Apply seasonal modifier
    float seasonal_modifier = 1.0f;
    auto season_it = pattern.seasonal_modifiers.find(current_season_);
    if (season_it != pattern.seasonal_modifiers.end()) {
        seasonal_modifier = season_it->second;
    }
    
    // [SEQUENCE: 675] Calculate probabilities
    std::vector<float> cumulative_probs;
    float total_prob = 0.0f;
    
    for (const auto& weather_chance : pattern.possible_weather) {
        float adjusted_prob = weather_chance.probability * seasonal_modifier;
        total_prob += adjusted_prob;
        cumulative_probs.push_back(total_prob);
    }
    
    // [SEQUENCE: 676] Random selection
    std::uniform_real_distribution<float> dist(0.0f, total_prob);
    float roll = dist(rng_);
    
    for (size_t i = 0; i < pattern.possible_weather.size(); ++i) {
        if (roll <= cumulative_probs[i]) {
            const auto& selected = pattern.possible_weather[i];
            
            // Random intensity within range
            int min_int = static_cast<int>(selected.min_intensity);
            int max_int = static_cast<int>(selected.max_intensity);
            std::uniform_int_distribution<int> intensity_dist(min_int, max_int);
            WeatherIntensity intensity = static_cast<WeatherIntensity>(intensity_dist(rng_));
            
            // Random duration within range
            auto min_dur = selected.min_duration.count();
            auto max_dur = selected.max_duration.count();
            std::uniform_int_distribution<int> duration_dist(min_dur, max_dur);
            
            // Set duration on the state (would need to modify state structure)
            
            return {selected.type, intensity};
        }
    }
    
    // Fallback to clear weather
    return {WeatherType::CLEAR, WeatherIntensity::NONE};
}

// [SEQUENCE: 677] Calculate weather effects based on type and intensity
WeatherEffects WeatherManager::CalculateWeatherEffects(WeatherType type, 
                                                      WeatherIntensity intensity) const {
    WeatherEffects base_effects = WeatherPresets::GetEffects(type, intensity);
    
    // [SEQUENCE: 678] Scale effects by intensity
    float intensity_scale = static_cast<float>(intensity) / 4.0f;  // 0.0 to 1.0
    
    if (intensity != WeatherIntensity::NONE) {
        base_effects.visibility_modifier = 1.0f - ((1.0f - base_effects.visibility_modifier) * intensity_scale);
        base_effects.movement_speed_modifier = 1.0f - ((1.0f - base_effects.movement_speed_modifier) * intensity_scale);
        base_effects.accuracy_modifier = 1.0f - ((1.0f - base_effects.accuracy_modifier) * intensity_scale);
        base_effects.damage_per_second *= intensity_scale;
    }
    
    return base_effects;
}

// [SEQUENCE: 679] Start weather transition
void WeatherManager::StartWeatherTransition(uint32_t zone_id, 
                                           WeatherType target_type,
                                           WeatherIntensity target_intensity,
                                           std::chrono::seconds duration) {
    auto it = weather_zones_.find(zone_id);
    if (it == weather_zones_.end()) {
        spdlog::warn("Weather zone {} not found", zone_id);
        return;
    }
    
    auto& state = it->second.state;
    
    // Don't transition if already at target
    if (state.current_type == target_type && state.intensity == target_intensity) {
        return;
    }
    
    state.target_type = target_type;
    state.target_intensity = target_intensity;
    state.transition_progress = 0.0f;
    state.transition_duration = duration;
    
    spdlog::info("Starting weather transition in zone {} from {} to {}", 
                zone_id, 
                static_cast<int>(state.current_type), 
                static_cast<int>(target_type));
    
    // [SEQUENCE: 680] Trigger weather change event
    auto event_it = weather_events_.find("weather_change");
    if (event_it != weather_events_.end()) {
        for (const auto& handler : event_it->second) {
            handler();
        }
    }
}

// [SEQUENCE: 681] Blend multiple weather states
WeatherState WeatherManager::BlendWeatherStates(const std::vector<WeatherState>& states,
                                              const std::vector<float>& weights) const {
    if (states.empty()) {
        return WeatherState();
    }
    
    if (states.size() == 1) {
        return states[0];
    }
    
    // [SEQUENCE: 682] Find dominant weather type
    size_t dominant_index = 0;
    float max_weight = weights[0];
    for (size_t i = 1; i < weights.size(); ++i) {
        if (weights[i] > max_weight) {
            max_weight = weights[i];
            dominant_index = i;
        }
    }
    
    WeatherState blended = states[dominant_index];
    
    // [SEQUENCE: 683] Blend numerical effects
    blended.effects.visibility_modifier = 0.0f;
    blended.effects.movement_speed_modifier = 0.0f;
    blended.effects.accuracy_modifier = 0.0f;
    blended.effects.spell_power_modifier = 0.0f;
    blended.effects.damage_per_second = 0.0f;
    blended.effects.ambient_light_modifier = 0.0f;
    
    for (size_t i = 0; i < states.size(); ++i) {
        const auto& effects = states[i].effects;
        float w = weights[i];
        
        blended.effects.visibility_modifier += effects.visibility_modifier * w;
        blended.effects.movement_speed_modifier += effects.movement_speed_modifier * w;
        blended.effects.accuracy_modifier += effects.accuracy_modifier * w;
        blended.effects.spell_power_modifier += effects.spell_power_modifier * w;
        blended.effects.damage_per_second += effects.damage_per_second * w;
        blended.effects.ambient_light_modifier += effects.ambient_light_modifier * w;
    }
    
    return blended;
}

// [SEQUENCE: 684] Check if extreme weather should trigger
bool WeatherManager::ShouldTriggerExtremeWeather(const WeatherZone& zone) const {
    // Base chance of extreme weather
    float base_chance = 0.0001f;  // 0.01% per update
    
    // Increase chance if no extreme weather for a long time
    auto now = std::chrono::steady_clock::now();
    auto time_since_start = now - zone.state.weather_started;
    auto hours_elapsed = std::chrono::duration_cast<std::chrono::hours>(time_since_start).count();
    
    float time_modifier = 1.0f + (hours_elapsed * 0.1f);  // +10% per hour
    
    // Check special conditions
    if (zone.pattern.special_weather_condition && 
        zone.pattern.special_weather_condition()) {
        base_chance *= 10.0f;  // 10x chance when special condition met
    }
    
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(rng_) < (base_chance * time_modifier);
}

// [SEQUENCE: 685] Set current season
void WeatherManager::SetSeason(const std::string& season) {
    std::string old_season = current_season_;
    current_season_ = season;
    
    spdlog::info("Season changed from {} to {}", old_season, season);
    
    // Trigger season change events
    auto event_it = weather_events_.find("season_change");
    if (event_it != weather_events_.end()) {
        for (const auto& handler : event_it->second) {
            handler();
        }
    }
}

// [SEQUENCE: 686] Get weather forecast
std::vector<std::pair<WeatherType, float>> WeatherManager::GetWeatherForecast(
    uint32_t zone_id, 
    std::chrono::hours forecast_duration) const {
    
    std::vector<std::pair<WeatherType, float>> forecast;
    
    auto it = weather_zones_.find(zone_id);
    if (it == weather_zones_.end()) {
        return forecast;
    }
    
    const auto& zone = it->second;
    
    // [SEQUENCE: 687] Generate probability distribution for forecast period
    std::unordered_map<WeatherType, float> weather_probs;
    
    for (const auto& weather_chance : zone.pattern.possible_weather) {
        weather_probs[weather_chance.type] += weather_chance.probability;
    }
    
    // Apply seasonal modifiers
    for (auto& [type, prob] : weather_probs) {
        auto season_it = zone.pattern.seasonal_modifiers.find(current_season_);
        if (season_it != zone.pattern.seasonal_modifiers.end()) {
            prob *= season_it->second;
        }
    }
    
    // Normalize and convert to vector
    float total = 0.0f;
    for (const auto& [type, prob] : weather_probs) {
        total += prob;
    }
    
    for (const auto& [type, prob] : weather_probs) {
        forecast.push_back({type, prob / total});
    }
    
    // Sort by probability
    std::sort(forecast.begin(), forecast.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });
    
    return forecast;
}

} // namespace mmorpg::game::world