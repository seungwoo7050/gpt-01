#pragma once

#include "quest_system.h"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>

namespace mmorpg::game::quest {

// [SEQUENCE: 1364] Quest tracker system
// 퀘스트 목표 추적과 진행 상황을 효율적으로 관리하는 시스템

// [SEQUENCE: 1365] Tracking event types
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

// [SEQUENCE: 1366] Tracking event data
struct TrackingEvent {
    TrackingEventType type;
    uint64_t source_entity_id;
    uint32_t target_id = 0;         // Monster/Item/NPC ID
    int32_t value = 1;              // Count, damage amount, etc.
    
    // Location data
    float x = 0.0f;
    float y = 0.0f; 
    float z = 0.0f;
    
    // Additional context
    std::unordered_map<std::string, std::variant<int32_t, float, std::string>> context;
    
    std::chrono::steady_clock::time_point timestamp;
};

// [SEQUENCE: 1367] Objective tracker interface
class IObjectiveTracker {
public:
    virtual ~IObjectiveTracker() = default;
    
    virtual bool ProcessEvent(const TrackingEvent& event, QuestObjective& objective) = 0;
    virtual bool IsObjectiveComplete(const QuestObjective& objective) const = 0;
    virtual float GetProgress(const QuestObjective& objective) const = 0;
    virtual void Reset(QuestObjective& objective) = 0;
};

// [SEQUENCE: 1368] Kill objective tracker
class KillObjectiveTracker : public IObjectiveTracker {
public:
    bool ProcessEvent(const TrackingEvent& event, QuestObjective& objective) override {
        if (event.type != TrackingEventType::ENTITY_KILLED) {
            return false;
        }
        
        if (event.target_id == objective.target_id) {
            objective.current_count = std::min(
                objective.current_count + event.value,
                objective.target_count
            );
            return true;
        }
        
        return false;
    }
    
    bool IsObjectiveComplete(const QuestObjective& objective) const override {
        return objective.current_count >= objective.target_count;
    }
    
    float GetProgress(const QuestObjective& objective) const override {
        if (objective.target_count == 0) return 1.0f;
        return static_cast<float>(objective.current_count) / objective.target_count;
    }
    
    void Reset(QuestObjective& objective) override {
        objective.current_count = 0;
    }
};

// [SEQUENCE: 1369] Collection objective tracker
class CollectObjectiveTracker : public IObjectiveTracker {
public:
    bool ProcessEvent(const TrackingEvent& event, QuestObjective& objective) override {
        if (event.type != TrackingEventType::ITEM_OBTAINED) {
            return false;
        }
        
        if (event.target_id == objective.target_id) {
            objective.current_count = std::min(
                objective.current_count + event.value,
                objective.target_count
            );
            return true;
        }
        
        return false;
    }
    
    bool IsObjectiveComplete(const QuestObjective& objective) const override {
        return objective.current_count >= objective.target_count;
    }
    
    float GetProgress(const QuestObjective& objective) const override {
        if (objective.target_count == 0) return 1.0f;
        return static_cast<float>(objective.current_count) / objective.target_count;
    }
    
    void Reset(QuestObjective& objective) override {
        objective.current_count = 0;
    }
};

// [SEQUENCE: 1370] Location objective tracker
class LocationObjectiveTracker : public IObjectiveTracker {
public:
    bool ProcessEvent(const TrackingEvent& event, QuestObjective& objective) override {
        if (event.type != TrackingEventType::LOCATION_ENTERED) {
            return false;
        }
        
        // Calculate distance to target location
        float dx = event.x - objective.target_x;
        float dy = event.y - objective.target_y;
        float dz = event.z - objective.target_z;
        float distance = std::sqrt(dx*dx + dy*dy + dz*dz);
        
        if (distance <= objective.radius) {
            objective.current_count = objective.target_count;  // Instant complete
            return true;
        }
        
        return false;
    }
    
    bool IsObjectiveComplete(const QuestObjective& objective) const override {
        return objective.current_count >= objective.target_count;
    }
    
    float GetProgress(const QuestObjective& objective) const override {
        return objective.current_count > 0 ? 1.0f : 0.0f;
    }
    
    void Reset(QuestObjective& objective) override {
        objective.current_count = 0;
    }
};

// [SEQUENCE: 1371] Timer objective tracker
class TimerObjectiveTracker : public IObjectiveTracker {
private:
    mutable std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> start_times_;
    
public:
    bool ProcessEvent(const TrackingEvent& event, QuestObjective& objective) override {
        if (event.type == TrackingEventType::TIME_ELAPSED) {
            // Timer objectives are checked in GetProgress
            return true;
        }
        return false;
    }
    
    bool IsObjectiveComplete(const QuestObjective& objective) const override {
        return GetProgress(objective) >= 1.0f;
    }
    
    float GetProgress(const QuestObjective& objective) const override {
        // Initialize timer if not started
        if (start_times_.find(objective.objective_id) == start_times_.end()) {
            start_times_[objective.objective_id] = std::chrono::steady_clock::now();
        }
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            now - start_times_[objective.objective_id]
        ).count();
        
        if (objective.time_limit_seconds > 0) {
            return std::min(1.0f, static_cast<float>(elapsed) / objective.time_limit_seconds);
        }
        
        return 0.0f;
    }
    
    void Reset(QuestObjective& objective) override {
        start_times_.erase(objective.objective_id);
        objective.current_count = 0;
    }
};

// [SEQUENCE: 1372] Quest tracker factory
class ObjectiveTrackerFactory {
public:
    static std::unique_ptr<IObjectiveTracker> CreateTracker(ObjectiveType type) {
        switch (type) {
            case ObjectiveType::KILL:
                return std::make_unique<KillObjectiveTracker>();
            case ObjectiveType::COLLECT:
                return std::make_unique<CollectObjectiveTracker>();
            case ObjectiveType::REACH_LOCATION:
                return std::make_unique<LocationObjectiveTracker>();
            case ObjectiveType::TIMER:
                return std::make_unique<TimerObjectiveTracker>();
            // TODO: Implement other trackers
            default:
                return nullptr;
        }
    }
};

// [SEQUENCE: 1373] Advanced quest tracker
class AdvancedQuestTracker {
public:
    AdvancedQuestTracker() {
        InitializeTrackers();
    }
    
    // [SEQUENCE: 1374] Process tracking event
    void ProcessEvent(const TrackingEvent& event) {
        // Get quest logs for the entity
        auto quest_log = QuestManager::Instance().GetQuestLog(event.source_entity_id);
        if (!quest_log) {
            return;
        }
        
        // Process event for all active quests
        auto active_quests = quest_log->GetActiveQuests();
        for (uint32_t quest_id : active_quests) {
            auto* progress = quest_log->GetQuestProgress(quest_id);
            if (!progress || progress->state != QuestState::ACTIVE) {
                continue;
            }
            
            ProcessQuestEvent(event, *quest_log, quest_id, *progress);
        }
    }
    
    // [SEQUENCE: 1375] Register custom tracker
    void RegisterCustomTracker(ObjectiveType type, 
                              std::unique_ptr<IObjectiveTracker> tracker) {
        objective_trackers_[type] = std::move(tracker);
    }
    
    // [SEQUENCE: 1376] Batch event processing
    void ProcessEventBatch(const std::vector<TrackingEvent>& events) {
        for (const auto& event : events) {
            ProcessEvent(event);
        }
    }
    
    // [SEQUENCE: 1377] Subscribe to quest events
    using QuestEventCallback = std::function<void(const QuestEvent&)>;
    void Subscribe(QuestEventType event_type, QuestEventCallback callback) {
        event_callbacks_[event_type].push_back(callback);
    }
    
private:
    // Objective trackers by type
    std::unordered_map<ObjectiveType, std::unique_ptr<IObjectiveTracker>> objective_trackers_;
    
    // Event callbacks
    std::unordered_map<QuestEventType, std::vector<QuestEventCallback>> event_callbacks_;
    
    // [SEQUENCE: 1378] Initialize default trackers
    void InitializeTrackers() {
        objective_trackers_[ObjectiveType::KILL] = std::make_unique<KillObjectiveTracker>();
        objective_trackers_[ObjectiveType::COLLECT] = std::make_unique<CollectObjectiveTracker>();
        objective_trackers_[ObjectiveType::REACH_LOCATION] = std::make_unique<LocationObjectiveTracker>();
        objective_trackers_[ObjectiveType::TIMER] = std::make_unique<TimerObjectiveTracker>();
    }
    
    // [SEQUENCE: 1379] Process event for specific quest
    void ProcessQuestEvent(const TrackingEvent& event, QuestLog& quest_log,
                          uint32_t quest_id, QuestProgress& progress) {
        bool any_progress = false;
        
        for (auto& objective : progress.objectives) {
            auto tracker_it = objective_trackers_.find(objective.type);
            if (tracker_it == objective_trackers_.end()) {
                continue;
            }
            
            auto& tracker = tracker_it->second;
            if (tracker->ProcessEvent(event, objective)) {
                any_progress = true;
                
                // Fire objective progress event
                FireQuestEvent(QuestEvent{
                    QuestEventType::OBJECTIVE_PROGRESS,
                    event.source_entity_id,
                    quest_id,
                    objective.objective_id,
                    event.timestamp
                });
                
                // Check if objective completed
                if (tracker->IsObjectiveComplete(objective)) {
                    FireQuestEvent(QuestEvent{
                        QuestEventType::OBJECTIVE_COMPLETED,
                        event.source_entity_id,
                        quest_id,
                        objective.objective_id,
                        event.timestamp
                    });
                }
            }
        }
        
        // Check if quest completed
        if (any_progress && progress.IsComplete()) {
            const auto* quest_def = QuestManager::Instance().GetQuest(quest_id);
            if (quest_def && quest_def->auto_complete) {
                quest_log.CompleteQuest(quest_id);
                
                FireQuestEvent(QuestEvent{
                    QuestEventType::QUEST_COMPLETED,
                    event.source_entity_id,
                    quest_id,
                    0,
                    event.timestamp
                });
            }
        }
    }
    
    // [SEQUENCE: 1380] Fire quest event
    void FireQuestEvent(const QuestEvent& event) {
        auto callbacks_it = event_callbacks_.find(event.type);
        if (callbacks_it != event_callbacks_.end()) {
            for (const auto& callback : callbacks_it->second) {
                callback(event);
            }
        }
    }
};

// [SEQUENCE: 1381] Quest tracking helpers
class QuestTrackingHelpers {
public:
    // Convert game events to tracking events
    static TrackingEvent CreateKillEvent(uint64_t killer_id, uint32_t victim_id) {
        return TrackingEvent{
            TrackingEventType::ENTITY_KILLED,
            killer_id,
            victim_id,
            1,
            0, 0, 0,
            {},
            std::chrono::steady_clock::now()
        };
    }
    
    static TrackingEvent CreateItemEvent(uint64_t entity_id, uint32_t item_id, int32_t count) {
        return TrackingEvent{
            TrackingEventType::ITEM_OBTAINED,
            entity_id,
            item_id,
            count,
            0, 0, 0,
            {},
            std::chrono::steady_clock::now()
        };
    }
    
    static TrackingEvent CreateLocationEvent(uint64_t entity_id, float x, float y, float z) {
        return TrackingEvent{
            TrackingEventType::LOCATION_ENTERED,
            entity_id,
            0,
            1,
            x, y, z,
            {},
            std::chrono::steady_clock::now()
        };
    }
    
    // Check if entity meets objective requirements
    static bool MeetsObjectiveRequirements(uint64_t entity_id, const QuestObjective& objective) {
        // TODO: Check class, level, faction requirements
        return true;
    }
};

// [SEQUENCE: 1382] Global quest tracker instance
class GlobalQuestTracker {
public:
    static GlobalQuestTracker& Instance() {
        static GlobalQuestTracker instance;
        return instance;
    }
    
    AdvancedQuestTracker& GetTracker() { return tracker_; }
    
private:
    GlobalQuestTracker() = default;
    AdvancedQuestTracker tracker_;
};

} // namespace mmorpg::game::quest