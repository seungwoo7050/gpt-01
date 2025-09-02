#pragma once

#include <chrono>
#include "core/singleton.h"

namespace mmorpg::game::world {

// [SEQUENCE: MVP7-101] Time of day enum
enum class TimeOfDay {
    DAWN,
    MORNING,
    MIDDAY,
    AFTERNOON,
    DUSK,
    EVENING,
    MIDNIGHT,
    LATE_NIGHT
};

// [SEQUENCE: MVP7-102] Day/Night cycle manager
class DayNightCycle : public Singleton<DayNightCycle> {
public:
    // [SEQUENCE: MVP7-103] Set the duration of a full game day in real-time
    void SetDayDuration(std::chrono::seconds real_time_duration);
    
    // [SEQUENCE: MVP7-104] Update the game time
    void Update(float delta_time);
    
    // [SEQUENCE: MVP7-105] Get the current time of day
    TimeOfDay GetCurrentTimeOfDay() const;
    
    // [SEQUENCE: MVP7-106] Get the current game time as a string (HH:MM)
    std::string GetGameTimeAsString() const;

private:
    friend class Singleton<DayNightCycle>;
    DayNightCycle();

    std::chrono::duration<float> game_time_of_day_;
    float time_scale_ = 1.0f; // real seconds to game seconds
};

} // namespace mmorpg::game::world
