#pragma once

#include "quest_system.h"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>

namespace mmorpg::game::quest {

// [SEQUENCE: MVP7-282] Quest tracker system

// [SEQUENCE: MVP7-283] Tracking event types
enum class TrackingEventType {
    ENTITY_KILLED,
    ITEM_OBTAINED,
    ITEM_USED,
    LOCATION_ENTERED,
    LOCATION_EXITED,
    NPC_TALKED,
    OBJECT_INTERACTED,
    SPELL_CAST,
    DAMAGE_DEALT,
    DAMAGE_TAKEN,
    PLAYER_DEATH,
    TIME_ELAPSED,
    CUSTOM_EVENT
};

// [SEQUENCE: MVP7-284] Tracking event data
struct TrackingEvent {
    TrackingEventType type;
    uint64_t source_entity_id;
    uint32_t target_id = 0;
    int32_t value = 1;
    float x = 0.0f;
    float y = 0.0f; 
    float z = 0.0f;
    std::unordered_map<std::string, std::variant<int32_t, float, std::string>> context;
    std::chrono::steady_clock::time_point timestamp;
};

// [SEQUENCE: MVP7-285] Objective tracker interface
class IObjectiveTracker {
public:
    virtual ~IObjectiveTracker() = default;
    virtual bool ProcessEvent(const TrackingEvent& event, QuestObjective& objective) = 0;
    virtual bool IsObjectiveComplete(const QuestObjective& objective) const = 0;
    virtual float GetProgress(const QuestObjective& objective) const = 0;
    virtual void Reset(QuestObjective& objective) = 0;
};

// [SEQUENCE: MVP7-286] Kill objective tracker
class KillObjectiveTracker : public IObjectiveTracker {
public:
    bool ProcessEvent(const TrackingEvent& event, QuestObjective& objective) override;
    bool IsObjectiveComplete(const QuestObjective& objective) const override;
    float GetProgress(const QuestObjective& objective) const override;
    void Reset(QuestObjective& objective) override;
};

// [SEQUENCE: MVP7-287] Collection objective tracker
class CollectObjectiveTracker : public IObjectiveTracker {
public:
    bool ProcessEvent(const TrackingEvent& event, QuestObjective& objective) override;
    bool IsObjectiveComplete(const QuestObjective& objective) const override;
    float GetProgress(const QuestObjective& objective) const override;
    void Reset(QuestObjective& objective) override;
};

// [SEQUENCE: MVP7-288] Location objective tracker
class LocationObjectiveTracker : public IObjectiveTracker {
public:
    bool ProcessEvent(const TrackingEvent& event, QuestObjective& objective) override;
    bool IsObjectiveComplete(const QuestObjective& objective) const override;
    float GetProgress(const QuestObjective& objective) const override;
    void Reset(QuestObjective& objective) override;
};

// [SEQUENCE: MVP7-289] Timer objective tracker
class TimerObjectiveTracker : public IObjectiveTracker {
private:
    mutable std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> start_times_;
public:
    bool ProcessEvent(const TrackingEvent& event, QuestObjective& objective) override;
    bool IsObjectiveComplete(const QuestObjective& objective) const override;
    float GetProgress(const QuestObjective& objective) const override;
    void Reset(QuestObjective& objective) override;
};

// [SEQUENCE: MVP7-290] Quest tracker factory
class ObjectiveTrackerFactory {
public:
    static std::unique_ptr<IObjectiveTracker> CreateTracker(ObjectiveType type);
};

// [SEQUENCE: MVP7-291] Advanced quest tracker
class AdvancedQuestTracker {
public:
    AdvancedQuestTracker();
    
    // [SEQUENCE: MVP7-292] Process tracking event
    void ProcessEvent(const TrackingEvent& event);
    
    // [SEQUENCE: MVP7-293] Register custom tracker
    void RegisterCustomTracker(ObjectiveType type, std::unique_ptr<IObjectiveTracker> tracker);
    
    // [SEQUENCE: MVP7-294] Batch event processing
    void ProcessEventBatch(const std::vector<TrackingEvent>& events);
    
    // [SEQUENCE: MVP7-295] Subscribe to quest events
    using QuestEventCallback = std::function<void(const QuestEvent&)>;
    void Subscribe(QuestEventType event_type, QuestEventCallback callback);
    
private:
    std::unordered_map<ObjectiveType, std::unique_ptr<IObjectiveTracker>> objective_trackers_;
    std::unordered_map<QuestEventType, std::vector<QuestEventCallback>> event_callbacks_;
    
    // [SEQUENCE: MVP7-296] Initialize default trackers
    void InitializeTrackers();
    
    // [SEQUENCE: MVP7-297] Process event for specific quest
    void ProcessQuestEvent(const TrackingEvent& event, QuestLog& quest_log, uint32_t quest_id, QuestProgress& progress);
    
    // [SEQUENCE: MVP7-298] Fire quest event
    void FireQuestEvent(const QuestEvent& event);
};

// [SEQUENCE: MVP7-299] Quest tracking helpers
class QuestTrackingHelpers {
public:
    static TrackingEvent CreateKillEvent(uint64_t killer_id, uint32_t victim_id);
    static TrackingEvent CreateItemEvent(uint64_t entity_id, uint32_t item_id, int32_t count);
    static TrackingEvent CreateLocationEvent(uint64_t entity_id, float x, float y, float z);
    static bool MeetsObjectiveRequirements(uint64_t entity_id, const QuestObjective& objective);
};

// [SEQUENCE: MVP7-300] Global quest tracker instance
class GlobalQuestTracker {
public:
    static GlobalQuestTracker& Instance();
    AdvancedQuestTracker& GetTracker();
private:
    GlobalQuestTracker() = default;
    AdvancedQuestTracker tracker_;
};

} // namespace mmorpg::game::quest