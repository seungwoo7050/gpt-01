#pragma once

#include <unordered_map>
#include <vector>
#include <chrono>
#include <random>
#include <cmath>
#include <spdlog/spdlog.h>

namespace mmorpg::game::world {

// [SEQUENCE: 2112] Weather system for environmental effects
// 날씨 시스템 - 환경 효과 구현

// [SEQUENCE: 2113] Weather type enumeration
enum class WeatherType {
    CLEAR,              // 맑음
    PARTLY_CLOUDY,      // 구름 조금
    CLOUDY,             // 흐림
    OVERCAST,           // 완전 흐림
    LIGHT_RAIN,         // 이슬비
    RAIN,               // 비
    HEAVY_RAIN,         // 폭우
    STORM,              // 폭풍
    THUNDERSTORM,       // 뇌우
    SNOW_LIGHT,         // 가벼운 눈
    SNOW,               // 눈
    BLIZZARD,           // 눈보라
    FOG,                // 안개
    HEAVY_FOG,          // 짙은 안개
    SANDSTORM,          // 모래폭풍 (사막)
    ASHFALL             // 화산재 (화산 지역)
};

// [SEQUENCE: 2114] Wind strength
enum class WindStrength {
    CALM,               // 무풍
    LIGHT_BREEZE,       // 산들바람
    MODERATE_BREEZE,    // 보통 바람
    STRONG_BREEZE,      // 강한 바람
    GALE,               // 강풍
    STORM_WIND          // 폭풍
};

// [SEQUENCE: 2115] Season type
enum class Season {
    SPRING,             // 봄
    SUMMER,             // 여름
    AUTUMN,             // 가을
    WINTER              // 겨울
};

// [SEQUENCE: 2116] Weather effects on gameplay
struct WeatherEffects {
    // Visibility
    float visibility_modifier = 1.0f;       // 시야 거리 배수
    float fog_density = 0.0f;              // 안개 밀도 (0-1)
    
    // Movement
    float movement_speed_modifier = 1.0f;   // 이동 속도 배수
    float mount_speed_modifier = 1.0f;      // 탈것 속도 배수
    bool prevents_flying = false;           // 비행 불가
    
    // Combat
    float ranged_accuracy_modifier = 1.0f;  // 원거리 정확도
    float spell_cast_time_modifier = 1.0f;  // 시전 시간 배수
    float fire_damage_modifier = 1.0f;      // 화염 피해 배수
    float frost_damage_modifier = 1.0f;     // 냉기 피해 배수
    float lightning_chance = 0.0f;          // 번개 맞을 확률
    
    // Resources
    float stamina_drain_modifier = 1.0f;    // 스태미나 소모 배수
    float health_regen_modifier = 1.0f;     // 체력 재생 배수
    
    // Special effects
    bool causes_wet_debuff = false;         // 젖음 디버프
    bool causes_frozen_debuff = false;      // 동상 디버프
    bool causes_heat_exhaustion = false;    // 열사병 디버프
    
    // [SEQUENCE: 2117] Apply weather effects to character
    void ApplyToCharacter(uint64_t character_id) const {
        // TODO: Apply modifiers to character stats
    }
};

// [SEQUENCE: 2118] Weather transition
struct WeatherTransition {
    WeatherType from_weather;
    WeatherType to_weather;
    std::chrono::seconds duration;
    float transition_progress = 0.0f;
    
    // [SEQUENCE: 2119] Interpolate weather effects
    WeatherEffects InterpolateEffects(const WeatherEffects& from, 
                                     const WeatherEffects& to) const {
        WeatherEffects result;
        
        // Linear interpolation
        auto lerp = [this](float a, float b) {
            return a + (b - a) * transition_progress;
        };
        
        result.visibility_modifier = lerp(from.visibility_modifier, to.visibility_modifier);
        result.fog_density = lerp(from.fog_density, to.fog_density);
        result.movement_speed_modifier = lerp(from.movement_speed_modifier, to.movement_speed_modifier);
        result.ranged_accuracy_modifier = lerp(from.ranged_accuracy_modifier, to.ranged_accuracy_modifier);
        
        // Boolean values use threshold
        if (transition_progress > 0.5f) {
            result.prevents_flying = to.prevents_flying;
            result.causes_wet_debuff = to.causes_wet_debuff;
        } else {
            result.prevents_flying = from.prevents_flying;
            result.causes_wet_debuff = from.causes_wet_debuff;
        }
        
        return result;
    }
};

// [SEQUENCE: 2120] Zone weather configuration
struct ZoneWeatherConfig {
    uint32_t zone_id;
    
    // Climate type affects weather patterns
    enum ClimateType {
        TEMPERATE,      // 온대
        TROPICAL,       // 열대
        DESERT,         // 사막
        ARCTIC,         // 극지
        MOUNTAINOUS,    // 산악
        COASTAL,        // 해안
        VOLCANIC        // 화산
    } climate = ClimateType::TEMPERATE;
    
    // Seasonal variations
    bool has_seasons = true;
    float seasonal_intensity = 1.0f;  // How much seasons affect weather
    
    // Weather probability table per season
    struct WeatherProbability {
        WeatherType weather;
        float probability;  // 0-1
    };
    
    std::unordered_map<Season, std::vector<WeatherProbability>> weather_chances;
    
    // Special weather events
    bool can_have_storms = true;
    bool can_have_extreme_weather = false;
    float extreme_weather_chance = 0.01f;  // 1% default
    
    // Indoor/underground zones
    bool is_indoor = false;
    bool weather_affects_zone = true;
};

// [SEQUENCE: 2121] Current weather state
class WeatherState {
public:
    WeatherState(uint32_t zone_id) : zone_id_(zone_id) {
        current_weather_ = WeatherType::CLEAR;
        wind_strength_ = WindStrength::CALM;
        wind_direction_ = 0.0f;
        temperature_ = 20.0f;  // Celsius
        humidity_ = 0.5f;
        pressure_ = 1013.25f;  // hPa
    }
    
    // [SEQUENCE: 2122] Update weather simulation
    void Update(std::chrono::seconds delta_time) {
        // Update transition if active
        if (is_transitioning_) {
            transition_.transition_progress += 
                delta_time.count() / static_cast<float>(transition_.duration.count());
            
            if (transition_.transition_progress >= 1.0f) {
                // Transition complete
                current_weather_ = transition_.to_weather;
                is_transitioning_ = false;
                UpdateWeatherEffects();
            }
        }
        
        // Update atmospheric conditions
        UpdateAtmosphere(delta_time);
        
        // Random weather events
        if (!is_transitioning_) {
            CheckForWeatherChange();
        }
    }
    
    // [SEQUENCE: 2123] Start weather transition
    void TransitionToWeather(WeatherType new_weather, std::chrono::seconds duration) {
        if (new_weather == current_weather_) return;
        
        transition_.from_weather = current_weather_;
        transition_.to_weather = new_weather;
        transition_.duration = duration;
        transition_.transition_progress = 0.0f;
        is_transitioning_ = true;
        
        spdlog::info("Zone {} transitioning from {} to {} over {}s",
                    zone_id_, 
                    GetWeatherName(current_weather_),
                    GetWeatherName(new_weather),
                    duration.count());
    }
    
    // [SEQUENCE: 2124] Get current effects
    WeatherEffects GetCurrentEffects() const {
        if (is_transitioning_) {
            auto from_effects = GetWeatherEffects(transition_.from_weather);
            auto to_effects = GetWeatherEffects(transition_.to_weather);
            return transition_.InterpolateEffects(from_effects, to_effects);
        }
        return GetWeatherEffects(current_weather_);
    }
    
    // [SEQUENCE: 2125] Force weather (GM command)
    void ForceWeather(WeatherType weather) {
        current_weather_ = weather;
        is_transitioning_ = false;
        UpdateWeatherEffects();
        
        spdlog::info("Zone {} weather forced to {}", 
                    zone_id_, GetWeatherName(weather));
    }
    
    // Getters
    WeatherType GetCurrentWeather() const { return current_weather_; }
    WindStrength GetWindStrength() const { return wind_strength_; }
    float GetWindDirection() const { return wind_direction_; }
    float GetTemperature() const { return temperature_; }
    float GetHumidity() const { return humidity_; }
    bool IsTransitioning() const { return is_transitioning_; }
    
private:
    uint32_t zone_id_;
    
    // Current conditions
    WeatherType current_weather_;
    WindStrength wind_strength_;
    float wind_direction_;  // 0-360 degrees
    float temperature_;     // Celsius
    float humidity_;        // 0-1
    float pressure_;        // Atmospheric pressure
    
    // Transition state
    bool is_transitioning_ = false;
    WeatherTransition transition_;
    
    // Cached effects
    WeatherEffects current_effects_;
    
    // Random generation
    std::mt19937 rng_{std::random_device{}()};
    
    // [SEQUENCE: 2126] Update atmospheric conditions
    void UpdateAtmosphere(std::chrono::seconds delta_time) {
        // Simulate realistic weather patterns
        std::normal_distribution<float> temp_change(-0.1f, 0.1f);
        temperature_ += temp_change(rng_) * delta_time.count() / 60.0f;
        temperature_ = std::clamp(temperature_, -50.0f, 50.0f);
        
        // Wind changes
        std::normal_distribution<float> wind_change(-5.0f, 5.0f);
        wind_direction_ += wind_change(rng_);
        while (wind_direction_ < 0) wind_direction_ += 360;
        while (wind_direction_ >= 360) wind_direction_ -= 360;
        
        // Humidity affects rain chance
        if (current_weather_ == WeatherType::RAIN || 
            current_weather_ == WeatherType::HEAVY_RAIN) {
            humidity_ = std::min(1.0f, humidity_ + 0.01f * delta_time.count());
        } else {
            humidity_ = std::max(0.0f, humidity_ - 0.005f * delta_time.count());
        }
    }
    
    // [SEQUENCE: 2127] Check for natural weather changes
    void CheckForWeatherChange() {
        // Simple probability-based system
        std::uniform_real_distribution<float> chance(0.0f, 1.0f);
        
        // Higher chance of change in unstable conditions
        float change_probability = 0.001f;  // Base 0.1% per update
        
        if (pressure_ < 1000.0f) change_probability *= 2.0f;  // Low pressure
        if (humidity_ > 0.8f) change_probability *= 1.5f;     // High humidity
        
        if (chance(rng_) < change_probability) {
            // Determine new weather based on current conditions
            WeatherType new_weather = DetermineNextWeather();
            if (new_weather != current_weather_) {
                TransitionToWeather(new_weather, std::chrono::seconds(300));  // 5 min transition
            }
        }
    }
    
    // [SEQUENCE: 2128] Determine next weather state
    WeatherType DetermineNextWeather() {
        // Weather transition logic based on current state
        std::vector<std::pair<WeatherType, float>> possibilities;
        
        switch (current_weather_) {
            case WeatherType::CLEAR:
                possibilities = {
                    {WeatherType::PARTLY_CLOUDY, 0.6f},
                    {WeatherType::CLOUDY, 0.3f},
                    {WeatherType::FOG, 0.1f}
                };
                break;
                
            case WeatherType::PARTLY_CLOUDY:
                possibilities = {
                    {WeatherType::CLEAR, 0.3f},
                    {WeatherType::CLOUDY, 0.5f},
                    {WeatherType::OVERCAST, 0.2f}
                };
                break;
                
            case WeatherType::CLOUDY:
                possibilities = {
                    {WeatherType::PARTLY_CLOUDY, 0.2f},
                    {WeatherType::OVERCAST, 0.4f},
                    {WeatherType::LIGHT_RAIN, 0.3f},
                    {WeatherType::RAIN, 0.1f}
                };
                break;
                
            case WeatherType::RAIN:
                possibilities = {
                    {WeatherType::LIGHT_RAIN, 0.3f},
                    {WeatherType::HEAVY_RAIN, 0.2f},
                    {WeatherType::CLOUDY, 0.4f},
                    {WeatherType::STORM, 0.1f}
                };
                break;
                
            default:
                // Default to clearing up
                possibilities = {{WeatherType::CLOUDY, 1.0f}};
                break;
        }
        
        // Weighted random selection
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        float roll = dist(rng_);
        float cumulative = 0.0f;
        
        for (const auto& [weather, chance] : possibilities) {
            cumulative += chance;
            if (roll <= cumulative) {
                return weather;
            }
        }
        
        return current_weather_;  // No change
    }
    
    // [SEQUENCE: 2129] Update weather effects
    void UpdateWeatherEffects() {
        current_effects_ = GetWeatherEffects(current_weather_);
    }
    
    // [SEQUENCE: 2130] Get effects for weather type
    static WeatherEffects GetWeatherEffects(WeatherType weather) {
        WeatherEffects effects;
        
        switch (weather) {
            case WeatherType::CLEAR:
                // No modifiers
                break;
                
            case WeatherType::RAIN:
                effects.visibility_modifier = 0.8f;
                effects.movement_speed_modifier = 0.95f;
                effects.fire_damage_modifier = 0.9f;
                effects.causes_wet_debuff = true;
                break;
                
            case WeatherType::HEAVY_RAIN:
                effects.visibility_modifier = 0.6f;
                effects.movement_speed_modifier = 0.85f;
                effects.ranged_accuracy_modifier = 0.8f;
                effects.fire_damage_modifier = 0.7f;
                effects.causes_wet_debuff = true;
                break;
                
            case WeatherType::STORM:
                effects.visibility_modifier = 0.5f;
                effects.movement_speed_modifier = 0.7f;
                effects.mount_speed_modifier = 0.6f;
                effects.prevents_flying = true;
                effects.ranged_accuracy_modifier = 0.6f;
                effects.fire_damage_modifier = 0.5f;
                effects.causes_wet_debuff = true;
                break;
                
            case WeatherType::THUNDERSTORM:
                effects.visibility_modifier = 0.4f;
                effects.movement_speed_modifier = 0.7f;
                effects.prevents_flying = true;
                effects.ranged_accuracy_modifier = 0.5f;
                effects.fire_damage_modifier = 0.4f;
                effects.lightning_chance = 0.001f;  // 0.1% per second
                effects.causes_wet_debuff = true;
                break;
                
            case WeatherType::SNOW:
                effects.visibility_modifier = 0.7f;
                effects.movement_speed_modifier = 0.8f;
                effects.frost_damage_modifier = 1.2f;
                effects.stamina_drain_modifier = 1.2f;
                break;
                
            case WeatherType::BLIZZARD:
                effects.visibility_modifier = 0.3f;
                effects.movement_speed_modifier = 0.5f;
                effects.prevents_flying = true;
                effects.frost_damage_modifier = 1.5f;
                effects.stamina_drain_modifier = 1.5f;
                effects.causes_frozen_debuff = true;
                break;
                
            case WeatherType::FOG:
                effects.visibility_modifier = 0.5f;
                effects.fog_density = 0.7f;
                effects.ranged_accuracy_modifier = 0.7f;
                break;
                
            case WeatherType::HEAVY_FOG:
                effects.visibility_modifier = 0.2f;
                effects.fog_density = 0.95f;
                effects.ranged_accuracy_modifier = 0.4f;
                break;
                
            case WeatherType::SANDSTORM:
                effects.visibility_modifier = 0.3f;
                effects.movement_speed_modifier = 0.6f;
                effects.prevents_flying = true;
                effects.stamina_drain_modifier = 1.3f;
                effects.health_regen_modifier = 0.8f;
                break;
                
            default:
                break;
        }
        
        return effects;
    }
    
    // [SEQUENCE: 2131] Get weather name
    static std::string GetWeatherName(WeatherType weather) {
        switch (weather) {
            case WeatherType::CLEAR: return "Clear";
            case WeatherType::PARTLY_CLOUDY: return "Partly Cloudy";
            case WeatherType::CLOUDY: return "Cloudy";
            case WeatherType::RAIN: return "Rain";
            case WeatherType::HEAVY_RAIN: return "Heavy Rain";
            case WeatherType::STORM: return "Storm";
            case WeatherType::THUNDERSTORM: return "Thunderstorm";
            case WeatherType::SNOW: return "Snow";
            case WeatherType::BLIZZARD: return "Blizzard";
            case WeatherType::FOG: return "Fog";
            case WeatherType::HEAVY_FOG: return "Heavy Fog";
            case WeatherType::SANDSTORM: return "Sandstorm";
            default: return "Unknown";
        }
    }
};

// [SEQUENCE: 2132] Weather manager
class WeatherManager {
public:
    static WeatherManager& Instance() {
        static WeatherManager instance;
        return instance;
    }
    
    // [SEQUENCE: 2133] Register zone weather
    void RegisterZone(uint32_t zone_id, const ZoneWeatherConfig& config) {
        zone_configs_[zone_id] = config;
        zone_weather_[zone_id] = std::make_unique<WeatherState>(zone_id);
        
        // Set initial weather based on climate
        auto initial_weather = DetermineInitialWeather(config);
        zone_weather_[zone_id]->ForceWeather(initial_weather);
        
        spdlog::info("Registered weather for zone {} with {} climate",
                    zone_id, GetClimateName(config.climate));
    }
    
    // [SEQUENCE: 2134] Update all zones
    void Update() {
        auto now = std::chrono::system_clock::now();
        auto delta = now - last_update_;
        last_update_ = now;
        
        auto delta_seconds = std::chrono::duration_cast<std::chrono::seconds>(delta);
        
        // Update each zone's weather
        for (auto& [zone_id, weather_state] : zone_weather_) {
            weather_state->Update(delta_seconds);
            
            // Apply seasonal changes
            if (now - last_season_check_ > std::chrono::hours(1)) {
                CheckSeasonalWeather(zone_id);
            }
        }
        
        if (now - last_season_check_ > std::chrono::hours(1)) {
            last_season_check_ = now;
        }
        
        // Process lightning strikes
        ProcessLightningStrikes();
    }
    
    // [SEQUENCE: 2135] Get weather for zone
    WeatherState* GetZoneWeather(uint32_t zone_id) {
        auto it = zone_weather_.find(zone_id);
        return (it != zone_weather_.end()) ? it->second.get() : nullptr;
    }
    
    // [SEQUENCE: 2136] Get current season
    Season GetCurrentSeason() const {
        // Simple season calculation (Northern Hemisphere)
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto* tm = std::localtime(&time_t);
        
        int month = tm->tm_mon;  // 0-11
        
        if (month >= 2 && month <= 4) return Season::SPRING;       // Mar-May
        if (month >= 5 && month <= 7) return Season::SUMMER;       // Jun-Aug
        if (month >= 8 && month <= 10) return Season::AUTUMN;      // Sep-Nov
        return Season::WINTER;                                      // Dec-Feb
    }
    
    // [SEQUENCE: 2137] Trigger weather event
    void TriggerWeatherEvent(uint32_t zone_id, WeatherType weather,
                           std::chrono::seconds duration = std::chrono::seconds(3600)) {
        auto* zone_weather = GetZoneWeather(zone_id);
        if (zone_weather) {
            zone_weather->TransitionToWeather(weather, std::chrono::seconds(300));
            
            // Schedule return to normal
            scheduled_events_.emplace(
                std::chrono::system_clock::now() + duration,
                zone_id
            );
            
            spdlog::info("Weather event {} triggered in zone {} for {}s",
                        static_cast<int>(weather), zone_id, duration.count());
        }
    }
    
    // [SEQUENCE: 2138] Force weather (GM command)
    void ForceWeather(uint32_t zone_id, WeatherType weather) {
        auto* zone_weather = GetZoneWeather(zone_id);
        if (zone_weather) {
            zone_weather->ForceWeather(weather);
        }
    }
    
    // [SEQUENCE: 2139] Get weather forecast
    struct WeatherForecast {
        WeatherType predicted_weather;
        float probability;
        std::chrono::system_clock::time_point when;
    };
    
    std::vector<WeatherForecast> GetForecast(uint32_t zone_id, 
                                            std::chrono::hours duration) {
        // Simple forecast based on current conditions
        // In production, this would use complex weather simulation
        std::vector<WeatherForecast> forecast;
        
        auto* weather = GetZoneWeather(zone_id);
        if (!weather) return forecast;
        
        // Placeholder forecast
        forecast.push_back({
            weather->GetCurrentWeather(),
            1.0f,
            std::chrono::system_clock::now()
        });
        
        return forecast;
    }
    
    // [SEQUENCE: 2140] Initialize default weather patterns
    void InitializeDefaultWeatherPatterns() {
        // Elwynn Forest - Temperate climate
        ZoneWeatherConfig elwynn;
        elwynn.zone_id = 1;
        elwynn.climate = ZoneWeatherConfig::TEMPERATE;
        elwynn.has_seasons = true;
        
        // Spring weather chances
        elwynn.weather_chances[Season::SPRING] = {
            {WeatherType::CLEAR, 0.3f},
            {WeatherType::PARTLY_CLOUDY, 0.3f},
            {WeatherType::CLOUDY, 0.2f},
            {WeatherType::LIGHT_RAIN, 0.15f},
            {WeatherType::RAIN, 0.05f}
        };
        
        // Summer weather chances
        elwynn.weather_chances[Season::SUMMER] = {
            {WeatherType::CLEAR, 0.5f},
            {WeatherType::PARTLY_CLOUDY, 0.3f},
            {WeatherType::CLOUDY, 0.1f},
            {WeatherType::LIGHT_RAIN, 0.08f},
            {WeatherType::THUNDERSTORM, 0.02f}
        };
        
        RegisterZone(1, elwynn);
        
        // Dun Morogh - Mountainous/Arctic
        ZoneWeatherConfig dun_morogh;
        dun_morogh.zone_id = 2;
        dun_morogh.climate = ZoneWeatherConfig::ARCTIC;
        dun_morogh.has_seasons = true;
        
        dun_morogh.weather_chances[Season::WINTER] = {
            {WeatherType::SNOW_LIGHT, 0.3f},
            {WeatherType::SNOW, 0.4f},
            {WeatherType::BLIZZARD, 0.1f},
            {WeatherType::CLOUDY, 0.2f}
        };
        
        RegisterZone(2, dun_morogh);
        
        // Tanaris - Desert
        ZoneWeatherConfig tanaris;
        tanaris.zone_id = 3;
        tanaris.climate = ZoneWeatherConfig::DESERT;
        tanaris.has_seasons = false;
        
        tanaris.weather_chances[Season::SUMMER] = {
            {WeatherType::CLEAR, 0.8f},
            {WeatherType::SANDSTORM, 0.15f},
            {WeatherType::PARTLY_CLOUDY, 0.05f}
        };
        
        RegisterZone(3, tanaris);
        
        spdlog::info("Initialized default weather patterns");
    }
    
private:
    WeatherManager() = default;
    
    std::unordered_map<uint32_t, ZoneWeatherConfig> zone_configs_;
    std::unordered_map<uint32_t, std::unique_ptr<WeatherState>> zone_weather_;
    
    // Timing
    std::chrono::system_clock::time_point last_update_{std::chrono::system_clock::now()};
    std::chrono::system_clock::time_point last_season_check_{std::chrono::system_clock::now()};
    
    // Scheduled weather events
    std::priority_queue<std::pair<std::chrono::system_clock::time_point, uint32_t>,
                       std::vector<std::pair<std::chrono::system_clock::time_point, uint32_t>>,
                       std::greater<>> scheduled_events_;
    
    // [SEQUENCE: 2141] Determine initial weather
    WeatherType DetermineInitialWeather(const ZoneWeatherConfig& config) {
        switch (config.climate) {
            case ZoneWeatherConfig::TEMPERATE:
                return WeatherType::PARTLY_CLOUDY;
            case ZoneWeatherConfig::TROPICAL:
                return WeatherType::CLOUDY;
            case ZoneWeatherConfig::DESERT:
                return WeatherType::CLEAR;
            case ZoneWeatherConfig::ARCTIC:
                return WeatherType::SNOW_LIGHT;
            case ZoneWeatherConfig::COASTAL:
                return WeatherType::PARTLY_CLOUDY;
            case ZoneWeatherConfig::VOLCANIC:
                return WeatherType::ASHFALL;
            default:
                return WeatherType::CLEAR;
        }
    }
    
    // [SEQUENCE: 2142] Check seasonal weather changes
    void CheckSeasonalWeather(uint32_t zone_id) {
        auto config_it = zone_configs_.find(zone_id);
        if (config_it == zone_configs_.end() || !config_it->second.has_seasons) {
            return;
        }
        
        const auto& config = config_it->second;
        Season current_season = GetCurrentSeason();
        
        auto season_it = config.weather_chances.find(current_season);
        if (season_it == config.weather_chances.end()) {
            return;
        }
        
        // Random chance to change weather based on seasonal patterns
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        
        if (dist(gen) < 0.1f) {  // 10% chance per hour
            // Select weather based on seasonal probabilities
            float roll = dist(gen);
            float cumulative = 0.0f;
            
            for (const auto& weather_prob : season_it->second) {
                cumulative += weather_prob.probability;
                if (roll <= cumulative) {
                    auto* weather = GetZoneWeather(zone_id);
                    if (weather && weather->GetCurrentWeather() != weather_prob.weather) {
                        weather->TransitionToWeather(weather_prob.weather, 
                                                   std::chrono::seconds(600));
                    }
                    break;
                }
            }
        }
    }
    
    // [SEQUENCE: 2143] Process lightning strikes
    void ProcessLightningStrikes() {
        for (const auto& [zone_id, weather] : zone_weather_) {
            if (weather->GetCurrentWeather() == WeatherType::THUNDERSTORM) {
                auto effects = weather->GetCurrentEffects();
                if (effects.lightning_chance > 0) {
                    // TODO: Random lightning strikes in zone
                    // This would damage players/creatures in random locations
                }
            }
        }
    }
    
    // [SEQUENCE: 2144] Get climate name
    static std::string GetClimateName(ZoneWeatherConfig::ClimateType climate) {
        switch (climate) {
            case ZoneWeatherConfig::TEMPERATE: return "Temperate";
            case ZoneWeatherConfig::TROPICAL: return "Tropical";
            case ZoneWeatherConfig::DESERT: return "Desert";
            case ZoneWeatherConfig::ARCTIC: return "Arctic";
            case ZoneWeatherConfig::MOUNTAINOUS: return "Mountainous";
            case ZoneWeatherConfig::COASTAL: return "Coastal";
            case ZoneWeatherConfig::VOLCANIC: return "Volcanic";
            default: return "Unknown";
        }
    }
};

} // namespace mmorpg::game::world