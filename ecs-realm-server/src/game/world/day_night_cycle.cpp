#include "day_night_cycle.h"
#include <iomanip>
#include <sstream>

namespace mmorpg::game::world {

// [SEQUENCE: MVP7-107] Constructor
DayNightCycle::DayNightCycle() : game_time_of_day_(std::chrono::hours(6)) {} // Start at 6 AM

// [SEQUENCE: MVP7-108] Set day duration
void DayNightCycle::SetDayDuration(std::chrono::seconds real_time_duration) {
    const float game_day_seconds = 24 * 60 * 60;
    time_scale_ = game_day_seconds / real_time_duration.count();
}

// [SEQUENCE: MVP7-109] Update game time
void DayNightCycle::Update(float delta_time) {
    game_time_of_day_ += std::chrono::duration<float>(delta_time * time_scale_);
    
    // Wrap around after 24 hours
    if (game_time_of_day_ >= std::chrono::hours(24)) {
        game_time_of_day_ -= std::chrono::hours(24);
    }
}

// [SEQUENCE: MVP7-110] Get current time of day
TimeOfDay DayNightCycle::GetCurrentTimeOfDay() const {
    auto hours = std::chrono::duration_cast<std::chrono::hours>(game_time_of_day_).count();
    
    if (hours >= 5 && hours < 7) return TimeOfDay::DAWN;
    if (hours >= 7 && hours < 10) return TimeOfDay::MORNING;
    if (hours >= 10 && hours < 14) return TimeOfDay::MIDDAY;
    if (hours >= 14 && hours < 17) return TimeOfDay::AFTERNOON;
    if (hours >= 17 && hours < 19) return TimeOfDay::DUSK;
    if (hours >= 19 && hours < 22) return TimeOfDay::EVENING;
    if (hours >= 22 || hours < 2) return TimeOfDay::MIDNIGHT;
    return TimeOfDay::LATE_NIGHT;
}

// [SEQUENCE: MVP7-111] Get game time as string
std::string DayNightCycle::GetGameTimeAsString() const {
    auto total_seconds = static_cast<long long>(game_time_of_day_.count());
    auto hours = (total_seconds / 3600) % 24;
    auto minutes = (total_seconds / 60) % 60;
    
    std::stringstream ss;
    ss << std::setfill('0') << std::setw(2) << hours << ":"
       << std::setfill('0') << std::setw(2) << minutes;
    return ss.str();
}

} // namespace mmorpg::game::world
