#pragma once

#include <string>
#include <vector>
#include <chrono>
#include "core/singleton.h"
#include "core/types.h"

namespace mmorpg::game::world {

// [SEQUENCE: MVP7-88] Weather types
enum class WeatherType {
    CLEAR,
    CLOUDY,
    FOG,
    RAIN_LIGHT,
    RAIN_HEAVY,
    STORM,
    SNOW_LIGHT,
    SNOW_HEAVY,
    BLIZZARD,
    SANDSTORM
};

// [SEQUENCE: MVP7-89] Gameplay effects of weather
struct WeatherEffect {
    float visibility_modifier = 1.0f;
    float movement_speed_modifier = 1.0f;
    float accuracy_modifier = 1.0f;
    float damage_per_second = 0.0f;
    core::types::DamageType damage_type = core::types::DamageType::NONE;
};

// [SEQUENCE: MVP7-90] Weather zone configuration
struct WeatherZone {
    uint32_t id;
    core::types::AABB boundary;
    std::vector<std::pair<WeatherType, float>> weather_probabilities;
};

// [SEQUENCE: MVP7-91] Weather system manager
class WeatherSystem : public Singleton<WeatherSystem> {
public:
    // [SEQUENCE: MVP7-92] Update loop for weather changes
    void Update(float delta_time);
    
    // [SEQUENCE: MVP7-93] Get current weather effect at a position
    WeatherEffect GetCurrentEffect(float x, float y, float z) const;
    
    // [SEQUENCE: MVP7-94] Load weather zones from data
    void LoadWeatherZones(const std::string& file_path);
    
    // [SEQUENCE: MVP7-95] Force a weather change for an event
    void ForceWeather(uint32_t zone_id, WeatherType type, std::chrono::seconds duration);

private:
    friend class Singleton<WeatherSystem>;
    WeatherSystem() = default;

    struct ActiveWeather {
        WeatherType type;
        std::chrono::steady_clock::time_point end_time;
        float intensity;
    };

    void UpdateZoneWeather(WeatherZone& zone);

    std::vector<WeatherZone> zones_;
    std::unordered_map<uint32_t, ActiveWeather> active_weather_;
    std::chrono::steady_clock::time_point last_update_time_;
};

} // namespace mmorpg::game::world