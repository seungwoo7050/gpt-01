#pragma once

#include <chrono>
#include <cmath>
#include <unordered_map>
#include <functional>
#include <spdlog/spdlog.h>

namespace mmorpg::game::world {

// [SEQUENCE: 2145] Day/night cycle system for dynamic world lighting
// 낮/밤 주기 시스템 - 동적 월드 조명 구현

// [SEQUENCE: 2146] Time of day enumeration
enum class TimeOfDay {
    DAWN,               // 새벽 (5:00 - 7:00)
    MORNING,            // 아침 (7:00 - 12:00)
    AFTERNOON,          // 오후 (12:00 - 17:00)
    DUSK,               // 황혼 (17:00 - 19:00)
    NIGHT,              // 밤 (19:00 - 5:00)
    MIDNIGHT            // 자정 (23:00 - 1:00)
};

// [SEQUENCE: 2147] Moon phase for night effects
enum class MoonPhase {
    NEW_MOON,           // 신월
    WAXING_CRESCENT,    // 초승달
    FIRST_QUARTER,      // 상현달
    WAXING_GIBBOUS,     // 차가는 반달
    FULL_MOON,          // 보름달
    WANING_GIBBOUS,     // 기우는 반달
    THIRD_QUARTER,      // 하현달
    WANING_CRESCENT     // 그믐달
};

// [SEQUENCE: 2148] Celestial event types
enum class CelestialEvent {
    NONE,
    SOLAR_ECLIPSE,      // 일식
    LUNAR_ECLIPSE,      // 월식
    BLOOD_MOON,         // 붉은 달
    HARVEST_MOON,       // 수확의 달
    AURORA,             // 오로라
    METEOR_SHOWER,      // 유성우
    COMET               // 혜성
};

// [SEQUENCE: 2149] Lighting conditions
struct LightingConditions {
    // Sun properties
    float sun_angle = 0.0f;         // 0-360 degrees
    float sun_intensity = 1.0f;     // 0-1
    struct Color {
        float r, g, b, a;
    } sun_color = {1.0f, 1.0f, 0.9f, 1.0f};
    
    // Moon properties
    float moon_angle = 180.0f;      // 0-360 degrees
    float moon_intensity = 0.2f;    // 0-1
    Color moon_color = {0.8f, 0.8f, 1.0f, 1.0f};
    
    // Ambient lighting
    float ambient_intensity = 0.3f;  // 0-1
    Color ambient_color = {0.5f, 0.5f, 0.6f, 1.0f};
    
    // Sky properties
    Color sky_color = {0.5f, 0.7f, 1.0f, 1.0f};
    Color horizon_color = {1.0f, 0.8f, 0.6f, 1.0f};
    float fog_density = 0.0f;       // 0-1
    
    // Shadow properties
    float shadow_intensity = 0.8f;   // 0-1
    float shadow_length = 1.0f;      // Multiplier
    
    // Special effects
    float star_visibility = 0.0f;    // 0-1
    bool aurora_active = false;
    float aurora_intensity = 0.0f;   // 0-1
};

// [SEQUENCE: 2150] Time-based modifiers
struct TimeBasedModifiers {
    // NPC behavior
    float npc_activity_level = 1.0f;        // Day active NPCs
    float nocturnal_activity_level = 0.0f;  // Night active NPCs
    
    // Creature spawning
    float normal_spawn_rate = 1.0f;
    float undead_spawn_rate = 0.0f;
    float demon_spawn_rate = 0.0f;
    
    // Player modifiers
    float stealth_effectiveness = 1.0f;
    float perception_modifier = 1.0f;
    float rest_bonus_rate = 1.0f;
    
    // Resource gathering
    float herb_visibility = 1.0f;
    float mining_node_sparkle = 1.0f;
    float fishing_bite_rate = 1.0f;
    
    // Combat modifiers
    float critical_strike_bonus = 0.0f;     // Night bonus for rogues
    float holy_power_bonus = 0.0f;          // Day bonus for paladins
    float shadow_power_bonus = 0.0f;        // Night bonus for warlocks
    
    // [SEQUENCE: 2151] Apply time-based buffs to character
    void ApplyToCharacter(uint64_t character_id, uint32_t class_id) const {
        // TODO: Apply class-specific time bonuses
    }
};

// [SEQUENCE: 2152] Day/night configuration for zones
struct ZoneDayNightConfig {
    uint32_t zone_id;
    
    // Time settings
    float day_length_hours = 24.0f;         // Real hours for full cycle
    float time_acceleration = 12.0f;        // Game time multiplier
    int32_t timezone_offset = 0;            // Hours offset from server time
    
    // Indoor/underground settings
    bool is_indoor = false;                 // No sun/moon
    bool has_artificial_light = false;      // Constant lighting
    float indoor_ambient_light = 0.7f;
    
    // Special conditions
    bool eternal_night = false;             // Cursed zones
    bool eternal_day = false;               // Blessed zones
    bool has_aurora = false;                // Northern zones
    
    // Custom lighting
    std::function<LightingConditions(float)> custom_lighting;
};

// [SEQUENCE: 2153] Day/night state for a zone
class DayNightState {
public:
    DayNightState(uint32_t zone_id, const ZoneDayNightConfig& config)
        : zone_id_(zone_id), config_(config) {
        // Initialize to current time
        UpdateToCurrentTime();
    }
    
    // [SEQUENCE: 2154] Update cycle
    void Update(std::chrono::milliseconds delta_time) {
        if (config_.eternal_day || config_.eternal_night) {
            return;  // No cycle in eternal zones
        }
        
        // Update game time
        float delta_hours = delta_time.count() / 3600000.0f;
        game_time_hours_ += delta_hours * config_.time_acceleration;
        
        // Wrap around 24 hours
        while (game_time_hours_ >= 24.0f) {
            game_time_hours_ -= 24.0f;
            days_elapsed_++;
        }
        
        // Update derived values
        UpdateTimeOfDay();
        UpdateLighting();
        UpdateModifiers();
        
        // Check for special events
        CheckCelestialEvents();
    }
    
    // [SEQUENCE: 2155] Get current lighting
    LightingConditions GetLighting() const {
        if (config_.custom_lighting) {
            return config_.custom_lighting(game_time_hours_);
        }
        return current_lighting_;
    }
    
    // [SEQUENCE: 2156] Get time-based modifiers
    TimeBasedModifiers GetModifiers() const {
        return current_modifiers_;
    }
    
    // [SEQUENCE: 2157] Get formatted time string
    std::string GetTimeString() const {
        int hours = static_cast<int>(game_time_hours_);
        int minutes = static_cast<int>((game_time_hours_ - hours) * 60);
        
        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%02d:%02d", hours, minutes);
        return std::string(buffer);
    }
    
    // [SEQUENCE: 2158] Force time (GM command)
    void SetTime(float hours) {
        game_time_hours_ = std::fmod(hours, 24.0f);
        if (game_time_hours_ < 0) game_time_hours_ += 24.0f;
        
        UpdateTimeOfDay();
        UpdateLighting();
        UpdateModifiers();
        
        spdlog::info("Zone {} time set to {}", zone_id_, GetTimeString());
    }
    
    // [SEQUENCE: 2159] Skip to next time period
    void SkipToNext(TimeOfDay target_time) {
        float target_hour = GetTimeOfDayStartHour(target_time);
        
        if (target_hour <= game_time_hours_) {
            // Next day
            game_time_hours_ = target_hour + 24.0f;
            days_elapsed_++;
        } else {
            game_time_hours_ = target_hour;
        }
        
        // Normalize
        while (game_time_hours_ >= 24.0f) {
            game_time_hours_ -= 24.0f;
        }
        
        UpdateTimeOfDay();
        UpdateLighting();
        UpdateModifiers();
    }
    
    // Getters
    TimeOfDay GetTimeOfDay() const { return current_time_of_day_; }
    float GetGameTimeHours() const { return game_time_hours_; }
    uint32_t GetDaysElapsed() const { return days_elapsed_; }
    MoonPhase GetMoonPhase() const { return current_moon_phase_; }
    CelestialEvent GetActiveEvent() const { return active_celestial_event_; }
    
private:
    uint32_t zone_id_;
    ZoneDayNightConfig config_;
    
    // Time tracking
    float game_time_hours_ = 12.0f;  // Start at noon
    uint32_t days_elapsed_ = 0;
    
    // Current states
    TimeOfDay current_time_of_day_ = TimeOfDay::AFTERNOON;
    MoonPhase current_moon_phase_ = MoonPhase::FULL_MOON;
    CelestialEvent active_celestial_event_ = CelestialEvent::NONE;
    
    // Cached values
    LightingConditions current_lighting_;
    TimeBasedModifiers current_modifiers_;
    
    // [SEQUENCE: 2160] Update to current real time
    void UpdateToCurrentTime() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto* tm = std::localtime(&time_t);
        
        // Convert to game time based on acceleration
        float real_hours = tm->tm_hour + tm->tm_min / 60.0f;
        game_time_hours_ = std::fmod(real_hours * config_.time_acceleration + 
                                     config_.timezone_offset, 24.0f);
    }
    
    // [SEQUENCE: 2161] Update time of day
    void UpdateTimeOfDay() {
        if (game_time_hours_ >= 5.0f && game_time_hours_ < 7.0f) {
            current_time_of_day_ = TimeOfDay::DAWN;
        } else if (game_time_hours_ >= 7.0f && game_time_hours_ < 12.0f) {
            current_time_of_day_ = TimeOfDay::MORNING;
        } else if (game_time_hours_ >= 12.0f && game_time_hours_ < 17.0f) {
            current_time_of_day_ = TimeOfDay::AFTERNOON;
        } else if (game_time_hours_ >= 17.0f && game_time_hours_ < 19.0f) {
            current_time_of_day_ = TimeOfDay::DUSK;
        } else if (game_time_hours_ >= 23.0f || game_time_hours_ < 1.0f) {
            current_time_of_day_ = TimeOfDay::MIDNIGHT;
        } else {
            current_time_of_day_ = TimeOfDay::NIGHT;
        }
        
        // Update moon phase (28 day cycle)
        int moon_day = days_elapsed_ % 28;
        if (moon_day < 4) current_moon_phase_ = MoonPhase::NEW_MOON;
        else if (moon_day < 7) current_moon_phase_ = MoonPhase::WAXING_CRESCENT;
        else if (moon_day < 11) current_moon_phase_ = MoonPhase::FIRST_QUARTER;
        else if (moon_day < 14) current_moon_phase_ = MoonPhase::WAXING_GIBBOUS;
        else if (moon_day < 18) current_moon_phase_ = MoonPhase::FULL_MOON;
        else if (moon_day < 21) current_moon_phase_ = MoonPhase::WANING_GIBBOUS;
        else if (moon_day < 25) current_moon_phase_ = MoonPhase::THIRD_QUARTER;
        else current_moon_phase_ = MoonPhase::WANING_CRESCENT;
    }
    
    // [SEQUENCE: 2162] Update lighting conditions
    void UpdateLighting() {
        if (config_.is_indoor) {
            UpdateIndoorLighting();
            return;
        }
        
        if (config_.eternal_day) {
            SetDaylightLighting();
            return;
        }
        
        if (config_.eternal_night) {
            SetNightLighting();
            return;
        }
        
        // Calculate sun position
        float sun_progress = game_time_hours_ / 24.0f;
        current_lighting_.sun_angle = sun_progress * 360.0f - 90.0f;  // -90 at midnight
        
        // Sun intensity curve
        if (game_time_hours_ >= 6.0f && game_time_hours_ <= 18.0f) {
            // Daytime
            float day_progress = (game_time_hours_ - 6.0f) / 12.0f;
            current_lighting_.sun_intensity = std::sin(day_progress * M_PI);
        } else {
            current_lighting_.sun_intensity = 0.0f;
        }
        
        // Moon calculations
        float moon_progress = std::fmod(game_time_hours_ + 12.0f, 24.0f) / 24.0f;
        current_lighting_.moon_angle = moon_progress * 360.0f - 90.0f;
        
        // Moon intensity based on phase
        switch (current_moon_phase_) {
            case MoonPhase::FULL_MOON:
                current_lighting_.moon_intensity = 0.8f;
                break;
            case MoonPhase::NEW_MOON:
                current_lighting_.moon_intensity = 0.05f;
                break;
            default:
                current_lighting_.moon_intensity = 0.3f;
        }
        
        // Only visible at night
        if (game_time_hours_ >= 6.0f && game_time_hours_ <= 18.0f) {
            current_lighting_.moon_intensity *= 0.1f;
        }
        
        // Update colors based on time
        UpdateLightingColors();
        
        // Star visibility
        if (game_time_hours_ >= 20.0f || game_time_hours_ <= 5.0f) {
            current_lighting_.star_visibility = 1.0f - current_lighting_.sun_intensity;
            if (current_moon_phase_ == MoonPhase::FULL_MOON) {
                current_lighting_.star_visibility *= 0.5f;  // Moon washes out stars
            }
        } else {
            current_lighting_.star_visibility = 0.0f;
        }
        
        // Aurora (northern zones at night)
        if (config_.has_aurora && current_lighting_.star_visibility > 0.5f) {
            // Random aurora chance
            current_lighting_.aurora_active = (days_elapsed_ % 7) == 0;
            if (current_lighting_.aurora_active) {
                current_lighting_.aurora_intensity = 0.3f + 0.4f * std::sin(game_time_hours_);
            }
        }
    }
    
    // [SEQUENCE: 2163] Update lighting colors
    void UpdateLightingColors() {
        auto& sun = current_lighting_.sun_color;
        auto& sky = current_lighting_.sky_color;
        auto& horizon = current_lighting_.horizon_color;
        auto& ambient = current_lighting_.ambient_color;
        
        switch (current_time_of_day_) {
            case TimeOfDay::DAWN:
                // Pink/orange dawn
                sun = {1.0f, 0.6f, 0.4f, 1.0f};
                sky = {0.4f, 0.5f, 0.8f, 1.0f};
                horizon = {1.0f, 0.7f, 0.5f, 1.0f};
                ambient = {0.3f, 0.3f, 0.4f, 1.0f};
                current_lighting_.fog_density = 0.2f;
                break;
                
            case TimeOfDay::MORNING:
                // Bright morning
                sun = {1.0f, 0.95f, 0.8f, 1.0f};
                sky = {0.5f, 0.7f, 1.0f, 1.0f};
                horizon = {0.8f, 0.85f, 1.0f, 1.0f};
                ambient = {0.5f, 0.5f, 0.5f, 1.0f};
                current_lighting_.fog_density = 0.1f;
                break;
                
            case TimeOfDay::AFTERNOON:
                // Bright white sun
                sun = {1.0f, 1.0f, 0.95f, 1.0f};
                sky = {0.4f, 0.6f, 1.0f, 1.0f};
                horizon = {0.7f, 0.8f, 1.0f, 1.0f};
                ambient = {0.6f, 0.6f, 0.6f, 1.0f};
                current_lighting_.fog_density = 0.05f;
                break;
                
            case TimeOfDay::DUSK:
                // Orange/red sunset
                sun = {1.0f, 0.5f, 0.2f, 1.0f};
                sky = {0.4f, 0.3f, 0.5f, 1.0f};
                horizon = {1.0f, 0.4f, 0.2f, 1.0f};
                ambient = {0.4f, 0.3f, 0.3f, 1.0f};
                current_lighting_.fog_density = 0.15f;
                break;
                
            case TimeOfDay::NIGHT:
                // Dark blue night
                sun = {0.0f, 0.0f, 0.0f, 0.0f};
                sky = {0.05f, 0.05f, 0.2f, 1.0f};
                horizon = {0.1f, 0.1f, 0.3f, 1.0f};
                ambient = {0.1f, 0.1f, 0.15f, 1.0f};
                current_lighting_.fog_density = 0.25f;
                break;
                
            case TimeOfDay::MIDNIGHT:
                // Very dark
                sun = {0.0f, 0.0f, 0.0f, 0.0f};
                sky = {0.02f, 0.02f, 0.1f, 1.0f};
                horizon = {0.05f, 0.05f, 0.15f, 1.0f};
                ambient = {0.05f, 0.05f, 0.1f, 1.0f};
                current_lighting_.fog_density = 0.3f;
                break;
        }
        
        // Shadow properties
        current_lighting_.shadow_intensity = current_lighting_.sun_intensity;
        if (current_lighting_.sun_intensity > 0) {
            // Longer shadows at dawn/dusk
            float sun_height = std::sin((game_time_hours_ - 6.0f) / 12.0f * M_PI);
            current_lighting_.shadow_length = 1.0f / std::max(0.1f, sun_height);
            current_lighting_.shadow_length = std::min(current_lighting_.shadow_length, 10.0f);
        }
    }
    
    // [SEQUENCE: 2164] Update time-based modifiers
    void UpdateModifiers() {
        auto& mods = current_modifiers_;
        
        // NPC activity
        if (game_time_hours_ >= 6.0f && game_time_hours_ <= 22.0f) {
            mods.npc_activity_level = 1.0f;
            mods.nocturnal_activity_level = 0.1f;
        } else {
            mods.npc_activity_level = 0.2f;
            mods.nocturnal_activity_level = 1.0f;
        }
        
        // Creature spawning
        if (current_time_of_day_ == TimeOfDay::NIGHT || 
            current_time_of_day_ == TimeOfDay::MIDNIGHT) {
            mods.normal_spawn_rate = 0.5f;
            mods.undead_spawn_rate = 2.0f;
            mods.demon_spawn_rate = 1.5f;
        } else {
            mods.normal_spawn_rate = 1.0f;
            mods.undead_spawn_rate = 0.2f;
            mods.demon_spawn_rate = 0.5f;
        }
        
        // Player modifiers
        if (current_time_of_day_ == TimeOfDay::NIGHT) {
            mods.stealth_effectiveness = 1.5f;
            mods.perception_modifier = 0.8f;
        } else {
            mods.stealth_effectiveness = 1.0f;
            mods.perception_modifier = 1.0f;
        }
        
        // Rest bonus at inns
        if (game_time_hours_ >= 22.0f || game_time_hours_ <= 6.0f) {
            mods.rest_bonus_rate = 2.0f;  // Double rest at night
        } else {
            mods.rest_bonus_rate = 1.0f;
        }
        
        // Resource visibility
        mods.herb_visibility = current_lighting_.sun_intensity;
        mods.mining_node_sparkle = 1.0f - current_lighting_.sun_intensity * 0.5f;
        
        // Fishing
        if (current_time_of_day_ == TimeOfDay::DAWN || 
            current_time_of_day_ == TimeOfDay::DUSK) {
            mods.fishing_bite_rate = 1.5f;  // Better fishing at dawn/dusk
        } else {
            mods.fishing_bite_rate = 1.0f;
        }
        
        // Class-specific bonuses
        if (current_time_of_day_ == TimeOfDay::NIGHT || 
            current_time_of_day_ == TimeOfDay::MIDNIGHT) {
            mods.critical_strike_bonus = 0.02f;  // +2% crit for rogues
            mods.shadow_power_bonus = 0.1f;      // +10% shadow damage
            mods.holy_power_bonus = -0.05f;      // -5% holy power
        } else if (current_time_of_day_ == TimeOfDay::AFTERNOON) {
            mods.holy_power_bonus = 0.1f;        // +10% holy power at noon
            mods.shadow_power_bonus = -0.05f;    // -5% shadow power
        }
    }
    
    // [SEQUENCE: 2165] Indoor lighting
    void UpdateIndoorLighting() {
        current_lighting_.sun_intensity = 0.0f;
        current_lighting_.moon_intensity = 0.0f;
        current_lighting_.ambient_intensity = config_.indoor_ambient_light;
        current_lighting_.ambient_color = {0.9f, 0.8f, 0.7f, 1.0f};  // Warm indoor light
        current_lighting_.star_visibility = 0.0f;
        current_lighting_.fog_density = 0.0f;
        current_lighting_.shadow_intensity = 0.3f;
        current_lighting_.shadow_length = 1.0f;
    }
    
    // [SEQUENCE: 2166] Set daylight (eternal day)
    void SetDaylightLighting() {
        current_lighting_.sun_angle = 45.0f;  // Mid-morning angle
        current_lighting_.sun_intensity = 0.9f;
        current_lighting_.sun_color = {1.0f, 0.95f, 0.8f, 1.0f};
        current_lighting_.moon_intensity = 0.0f;
        current_lighting_.ambient_intensity = 0.6f;
        current_lighting_.sky_color = {0.5f, 0.7f, 1.0f, 1.0f};
        current_lighting_.star_visibility = 0.0f;
    }
    
    // [SEQUENCE: 2167] Set night (eternal night)
    void SetNightLighting() {
        current_lighting_.sun_intensity = 0.0f;
        current_lighting_.moon_angle = 45.0f;
        current_lighting_.moon_intensity = 0.5f;
        current_lighting_.moon_color = {0.8f, 0.8f, 1.0f, 1.0f};
        current_lighting_.ambient_intensity = 0.15f;
        current_lighting_.sky_color = {0.05f, 0.05f, 0.2f, 1.0f};
        current_lighting_.star_visibility = 1.0f;
    }
    
    // [SEQUENCE: 2168] Check for celestial events
    void CheckCelestialEvents() {
        // Reset event
        active_celestial_event_ = CelestialEvent::NONE;
        
        // Solar eclipse (new moon at noon)
        if (current_moon_phase_ == MoonPhase::NEW_MOON &&
            game_time_hours_ >= 11.0f && game_time_hours_ <= 13.0f &&
            days_elapsed_ % 180 == 0) {  // Every 180 days
            
            active_celestial_event_ = CelestialEvent::SOLAR_ECLIPSE;
            current_lighting_.sun_intensity *= 0.1f;
            current_lighting_.ambient_intensity *= 0.3f;
        }
        
        // Lunar eclipse (full moon at midnight)
        if (current_moon_phase_ == MoonPhase::FULL_MOON &&
            (game_time_hours_ >= 23.0f || game_time_hours_ <= 1.0f) &&
            days_elapsed_ % 120 == 60) {  // Every 120 days, offset from solar
            
            active_celestial_event_ = CelestialEvent::LUNAR_ECLIPSE;
            current_lighting_.moon_color = {0.8f, 0.3f, 0.2f, 1.0f};  // Red moon
        }
        
        // Blood moon (rare full moon)
        if (current_moon_phase_ == MoonPhase::FULL_MOON &&
            days_elapsed_ % 365 == 300) {  // Once a year
            
            active_celestial_event_ = CelestialEvent::BLOOD_MOON;
            current_lighting_.moon_color = {1.0f, 0.2f, 0.1f, 1.0f};
            current_lighting_.moon_intensity = 1.0f;
        }
        
        // Meteor shower (summer nights)
        if (current_time_of_day_ == TimeOfDay::NIGHT &&
            days_elapsed_ % 365 >= 180 && days_elapsed_ % 365 <= 210) {
            
            if (days_elapsed_ % 5 == 0) {  // Every 5 days in summer
                active_celestial_event_ = CelestialEvent::METEOR_SHOWER;
            }
        }
    }
    
    // [SEQUENCE: 2169] Helper to get time of day start hour
    float GetTimeOfDayStartHour(TimeOfDay tod) const {
        switch (tod) {
            case TimeOfDay::DAWN: return 5.0f;
            case TimeOfDay::MORNING: return 7.0f;
            case TimeOfDay::AFTERNOON: return 12.0f;
            case TimeOfDay::DUSK: return 17.0f;
            case TimeOfDay::NIGHT: return 19.0f;
            case TimeOfDay::MIDNIGHT: return 23.0f;
            default: return 12.0f;
        }
    }
};

// [SEQUENCE: 2170] Day/night cycle manager
class DayNightManager {
public:
    static DayNightManager& Instance() {
        static DayNightManager instance;
        return instance;
    }
    
    // [SEQUENCE: 2171] Register zone day/night config
    void RegisterZone(uint32_t zone_id, const ZoneDayNightConfig& config) {
        zone_configs_[zone_id] = config;
        zone_states_[zone_id] = std::make_unique<DayNightState>(zone_id, config);
        
        spdlog::info("Registered day/night cycle for zone {}", zone_id);
    }
    
    // [SEQUENCE: 2172] Update all zones
    void Update() {
        auto now = std::chrono::steady_clock::now();
        auto delta = now - last_update_;
        last_update_ = now;
        
        auto delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>(delta);
        
        // Update each zone
        for (auto& [zone_id, state] : zone_states_) {
            state->Update(delta_ms);
        }
        
        // Process scheduled events
        ProcessScheduledEvents();
    }
    
    // [SEQUENCE: 2173] Get zone day/night state
    DayNightState* GetZoneState(uint32_t zone_id) {
        auto it = zone_states_.find(zone_id);
        return (it != zone_states_.end()) ? it->second.get() : nullptr;
    }
    
    // [SEQUENCE: 2174] Schedule sunrise event
    void ScheduleSunriseEvent(uint32_t zone_id, std::function<void()> callback) {
        sunrise_callbacks_[zone_id].push_back(callback);
    }
    
    // [SEQUENCE: 2175] Schedule sunset event
    void ScheduleSunsetEvent(uint32_t zone_id, std::function<void()> callback) {
        sunset_callbacks_[zone_id].push_back(callback);
    }
    
    // [SEQUENCE: 2176] Get server time info
    struct ServerTimeInfo {
        std::string server_time;
        std::unordered_map<uint32_t, std::string> zone_times;
        std::unordered_map<uint32_t, TimeOfDay> zone_periods;
    };
    
    ServerTimeInfo GetServerTimeInfo() const {
        ServerTimeInfo info;
        
        // Get real server time
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto* tm = std::localtime(&time_t);
        
        char buffer[32];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm);
        info.server_time = buffer;
        
        // Get each zone's game time
        for (const auto& [zone_id, state] : zone_states_) {
            info.zone_times[zone_id] = state->GetTimeString();
            info.zone_periods[zone_id] = state->GetTimeOfDay();
        }
        
        return info;
    }
    
    // [SEQUENCE: 2177] Initialize default zones
    void InitializeDefaultZones() {
        // Elwynn Forest - Normal day/night
        ZoneDayNightConfig elwynn;
        elwynn.zone_id = 1;
        elwynn.day_length_hours = 2.0f;  // 2 real hours = 24 game hours
        elwynn.time_acceleration = 12.0f;
        RegisterZone(1, elwynn);
        
        // Duskwood - Always darker
        ZoneDayNightConfig duskwood;
        duskwood.zone_id = 10;
        duskwood.day_length_hours = 2.0f;
        duskwood.time_acceleration = 12.0f;
        duskwood.custom_lighting = [](float hours) {
            LightingConditions lighting;
            // Always twilight in Duskwood
            lighting.sun_intensity = 0.3f;
            lighting.ambient_intensity = 0.2f;
            lighting.fog_density = 0.4f;
            lighting.sky_color = {0.3f, 0.2f, 0.4f, 1.0f};
            return lighting;
        };
        RegisterZone(10, duskwood);
        
        // Ironforge - Indoor zone
        ZoneDayNightConfig ironforge;
        ironforge.zone_id = 20;
        ironforge.is_indoor = true;
        ironforge.has_artificial_light = true;
        ironforge.indoor_ambient_light = 0.8f;
        RegisterZone(20, ironforge);
        
        // Shadowmoon Valley - Eternal night
        ZoneDayNightConfig shadowmoon;
        shadowmoon.zone_id = 30;
        shadowmoon.eternal_night = true;
        RegisterZone(30, shadowmoon);
        
        // Crystalsong Forest - Has aurora
        ZoneDayNightConfig crystalsong;
        crystalsong.zone_id = 40;
        crystalsong.day_length_hours = 2.0f;
        crystalsong.has_aurora = true;
        RegisterZone(40, crystalsong);
        
        spdlog::info("Initialized default day/night zones");
    }
    
private:
    DayNightManager() = default;
    
    std::unordered_map<uint32_t, ZoneDayNightConfig> zone_configs_;
    std::unordered_map<uint32_t, std::unique_ptr<DayNightState>> zone_states_;
    
    // Event callbacks
    std::unordered_map<uint32_t, std::vector<std::function<void()>>> sunrise_callbacks_;
    std::unordered_map<uint32_t, std::vector<std::function<void()>>> sunset_callbacks_;
    
    // Timing
    std::chrono::steady_clock::time_point last_update_{std::chrono::steady_clock::now()};
    
    // Track previous states for events
    std::unordered_map<uint32_t, TimeOfDay> previous_time_of_day_;
    
    // [SEQUENCE: 2178] Process scheduled events
    void ProcessScheduledEvents() {
        for (const auto& [zone_id, state] : zone_states_) {
            TimeOfDay current = state->GetTimeOfDay();
            
            auto prev_it = previous_time_of_day_.find(zone_id);
            if (prev_it != previous_time_of_day_.end()) {
                TimeOfDay previous = prev_it->second;
                
                // Check for sunrise (night -> dawn)
                if ((previous == TimeOfDay::NIGHT || previous == TimeOfDay::MIDNIGHT) &&
                    current == TimeOfDay::DAWN) {
                    
                    auto callbacks_it = sunrise_callbacks_.find(zone_id);
                    if (callbacks_it != sunrise_callbacks_.end()) {
                        for (const auto& callback : callbacks_it->second) {
                            callback();
                        }
                    }
                    
                    spdlog::info("Sunrise in zone {}", zone_id);
                }
                
                // Check for sunset (afternoon -> dusk)
                if (previous == TimeOfDay::AFTERNOON && current == TimeOfDay::DUSK) {
                    auto callbacks_it = sunset_callbacks_.find(zone_id);
                    if (callbacks_it != sunset_callbacks_.end()) {
                        for (const auto& callback : callbacks_it->second) {
                            callback();
                        }
                    }
                    
                    spdlog::info("Sunset in zone {}", zone_id);
                }
            }
            
            previous_time_of_day_[zone_id] = current;
        }
    }
};

} // namespace mmorpg::game::world