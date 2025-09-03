#include "day_night_cycle.h"
#include <spdlog/spdlog.h>
#include <sstream>
#include <iomanip>

namespace mmorpg::game::world {

// [SEQUENCE: 718] Initialize day/night cycle with configuration
void DayNightCycle::Initialize(const DayNightConfig& config) {
    config_ = config;
    current_state_.last_update = std::chrono::steady_clock::now();
    
    // Set initial phase
    UpdatePhase();
    
    spdlog::info("Day/Night cycle initialized. Real time per game day: {} minutes", 
                 config_.real_time_per_game_day.count());
}

// [SEQUENCE: 719] Update time progression
void DayNightCycle::Update(float delta_time) {
    if (time_paused_) {
        return;
    }
    
    // [SEQUENCE: 720] Calculate time progression
    // Real seconds to game seconds ratio
    float real_seconds_per_game_day = config_.real_time_per_game_day.count() * 60.0f;
    float game_seconds_per_real_second = (24 * 60 * 60) / real_seconds_per_game_day;
    
    // Update game time
    current_state_.current_second += delta_time * game_seconds_per_real_second;
    
    // [SEQUENCE: 721] Handle time overflow
    while (current_state_.current_second >= 60) {
        current_state_.current_second -= 60;
        current_state_.current_minute++;
        
        if (current_state_.current_minute >= 60) {
            current_state_.current_minute = 0;
            current_state_.current_hour++;
            
            if (current_state_.current_hour >= 24) {
                current_state_.current_hour = 0;
                current_state_.current_day++;
                
                // Reset daily event triggers
                for (auto& event : time_events_) {
                    event.triggered_today = false;
                }
            }
            
            // Update phase when hour changes
            UpdatePhase();
        }
        
        // Check time events every minute
        CheckTimeEvents();
    }
    
    // [SEQUENCE: 722] Update phase progress
    const auto* phase_info = GetPhaseInfo(current_state_.current_phase);
    if (phase_info) {
        float phase_start = phase_info->start_hour;
        float phase_duration = phase_info->duration_hours;
        float current_time = current_state_.current_hour + 
                           (current_state_.current_minute / 60.0f) + 
                           (current_state_.current_second / 3600.0f);
        
        // Handle wrap-around for phases that cross midnight
        if (phase_start + phase_duration > 24) {
            if (current_time < phase_start) {
                current_time += 24;
            }
        }
        
        current_state_.phase_progress = (current_time - phase_start) / phase_duration;
        current_state_.phase_progress = std::clamp(current_state_.phase_progress, 0.0f, 1.0f);
    }
    
    // Update modifiers
    InterpolateModifiers();
    
    current_state_.last_update = std::chrono::steady_clock::now();
}

// [SEQUENCE: 723] Get formatted time string
std::string DayNightCycle::GetTimeString() const {
    std::stringstream ss;
    ss << "Day " << current_state_.current_day 
       << ", " << std::setfill('0') << std::setw(2) << current_state_.current_hour
       << ":" << std::setfill('0') << std::setw(2) << current_state_.current_minute;
    return ss.str();
}

// [SEQUENCE: 724] Set game time manually
void DayNightCycle::SetGameTime(uint32_t day, uint32_t hour, uint32_t minute) {
    current_state_.current_day = day;
    current_state_.current_hour = hour % 24;
    current_state_.current_minute = minute % 60;
    current_state_.current_second = 0;
    
    UpdatePhase();
    
    spdlog::info("Game time set to {}", GetTimeString());
}

// [SEQUENCE: 725] Calculate current phase from hour
TimeOfDay DayNightCycle::CalculatePhase(uint32_t hour) const {
    for (const auto& phase : config_.phase_schedule) {
        uint32_t phase_end = (phase.start_hour + phase.duration_hours) % 24;
        
        // Handle phases that cross midnight
        if (phase.start_hour > phase_end) {
            if (hour >= phase.start_hour || hour < phase_end) {
                return phase.phase;
            }
        } else {
            if (hour >= phase.start_hour && hour < phase_end) {
                return phase.phase;
            }
        }
    }
    
    return TimeOfDay::MORNING;  // Default fallback
}

// [SEQUENCE: 726] Update current phase and trigger events
void DayNightCycle::UpdatePhase() {
    TimeOfDay old_phase = current_state_.current_phase;
    TimeOfDay new_phase = CalculatePhase(current_state_.current_hour);
    
    if (old_phase != new_phase) {
        current_state_.current_phase = new_phase;
        OnPhaseChange(old_phase, new_phase);
    }
}

// [SEQUENCE: 727] Handle phase change events
void DayNightCycle::OnPhaseChange(TimeOfDay old_phase, TimeOfDay new_phase) {
    spdlog::info("Time phase changed from {} to {} at {}", 
                 static_cast<int>(old_phase), 
                 static_cast<int>(new_phase),
                 GetTimeString());
    
    // Trigger registered handlers
    for (const auto& handler : phase_change_handlers_) {
        handler(old_phase, new_phase);
    }
}

// [SEQUENCE: 728] Check and trigger time-based events
void DayNightCycle::CheckTimeEvents() {
    for (auto& event : time_events_) {
        if (!event.triggered_today || event.recurring) {
            if (current_state_.current_hour == event.hour && 
                current_state_.current_minute == event.minute) {
                event.handler();
                event.triggered_today = true;
                
                spdlog::debug("Time event triggered at {}:{}", event.hour, event.minute);
            }
        }
    }
}

// [SEQUENCE: 729] Interpolate gameplay modifiers based on time
void DayNightCycle::InterpolateModifiers() {
    const auto* current_phase = GetPhaseInfo(current_state_.current_phase);
    if (!current_phase) return;
    
    // Find next phase for interpolation
    TimeOfDay next_phase_type = TimeOfDay::DAWN;
    for (size_t i = 0; i < config_.phase_schedule.size(); ++i) {
        if (config_.phase_schedule[i].phase == current_state_.current_phase) {
            size_t next_index = (i + 1) % config_.phase_schedule.size();
            next_phase_type = config_.phase_schedule[next_index].phase;
            break;
        }
    }
    
    const auto* next_phase = GetPhaseInfo(next_phase_type);
    if (!next_phase) return;
    
    // [SEQUENCE: 730] Smooth interpolation between phases
    float t = current_state_.phase_progress;
    
    current_state_.current_spawn_modifier = 
        current_phase->monster_spawn_rate_modifier * (1 - t) + 
        next_phase->monster_spawn_rate_modifier * t;
        
    current_state_.current_aggro_modifier = 
        current_phase->monster_aggro_modifier * (1 - t) + 
        next_phase->monster_aggro_modifier * t;
        
    current_state_.current_exp_modifier = 
        current_phase->experience_modifier * (1 - t) + 
        next_phase->experience_modifier * t;
        
    current_state_.current_drop_modifier = 
        current_phase->drop_rate_modifier * (1 - t) + 
        next_phase->drop_rate_modifier * t;
}

// [SEQUENCE: 731] Get phase information
const DayNightConfig::PhaseInfo* DayNightCycle::GetPhaseInfo(TimeOfDay phase) const {
    for (const auto& phase_info : config_.phase_schedule) {
        if (phase_info.phase == phase) {
            return &phase_info;
        }
    }
    return nullptr;
}

// [SEQUENCE: 732] Register phase change handler
void DayNightCycle::RegisterPhaseChangeHandler(std::function<void(TimeOfDay, TimeOfDay)> handler) {
    phase_change_handlers_.push_back(handler);
}

// [SEQUENCE: 733] Register time event handler
void DayNightCycle::RegisterTimeEventHandler(uint32_t hour, uint32_t minute, 
                                            std::function<void()> handler,
                                            bool recurring) {
    TimeEvent event;
    event.hour = hour % 24;
    event.minute = minute % 60;
    event.handler = handler;
    event.recurring = recurring;
    event.triggered_today = false;
    
    time_events_.push_back(event);
    
    spdlog::info("Registered time event at {}:{} (recurring: {})", 
                 event.hour, event.minute, recurring);
}

// [SEQUENCE: 734] NPCSchedule - Add schedule entry
void NPCSchedule::AddScheduleEntry(uint32_t npc_id, const ScheduleEntry& entry) {
    npc_schedules_[npc_id].push_back(entry);
}

// [SEQUENCE: 735] NPCSchedule - Get current behavior
std::optional<NPCSchedule::ScheduleEntry> NPCSchedule::GetCurrentBehavior(uint32_t npc_id) const {
    auto it = current_behaviors_.find(npc_id);
    if (it != current_behaviors_.end()) {
        return it->second;
    }
    return std::nullopt;
}

// [SEQUENCE: 736] NPCSchedule - Update all schedules
void NPCSchedule::UpdateSchedules(TimeOfDay current_phase) {
    for (const auto& [npc_id, schedule] : npc_schedules_) {
        for (const auto& entry : schedule) {
            if (entry.phase == current_phase) {
                current_behaviors_[npc_id] = entry;
                break;
            }
        }
    }
}

// [SEQUENCE: 737] NightEventManager - Register night event
void NightEventManager::RegisterNightEvent(NightEventType type, 
                                         std::function<void()> start_handler,
                                         std::function<void()> end_handler) {
    NightEvent event;
    event.type = type;
    event.start_handler = start_handler;
    event.end_handler = end_handler;
    event.is_active = false;
    
    night_events_.push_back(event);
}

// [SEQUENCE: 738] NightEventManager - Trigger night start
void NightEventManager::OnNightStart() {
    spdlog::info("Night has fallen - activating night events");
    
    for (auto& event : night_events_) {
        if (!event.is_active && event.start_handler) {
            event.start_handler();
            event.is_active = true;
        }
    }
}

// [SEQUENCE: 739] NightEventManager - Trigger night end
void NightEventManager::OnNightEnd() {
    spdlog::info("Dawn breaks - deactivating night events");
    
    for (auto& event : night_events_) {
        if (event.is_active && event.end_handler) {
            event.end_handler();
            event.is_active = false;
        }
    }
}

// [SEQUENCE: 740] NightEventManager - Check if event is active
bool NightEventManager::IsEventActive(NightEventType type) const {
    for (const auto& event : night_events_) {
        if (event.type == type) {
            return event.is_active;
        }
    }
    return false;
}

} // namespace mmorpg::game::world