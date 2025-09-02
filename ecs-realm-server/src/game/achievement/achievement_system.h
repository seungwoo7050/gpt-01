#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <chrono>
#include <variant>
#include <memory>
#include <spdlog/spdlog.h>

namespace mmorpg::game::achievement {

// [SEQUENCE: 1460] Achievement system
// 업적 시스템으로 플레이어의 게임 내 성취를 추적하고 보상

// [SEQUENCE: 1461] Achievement categories
enum class AchievementCategory {
    GENERAL,          // General gameplay
    COMBAT,           // Combat related
    EXPLORATION,      // World exploration
    SOCIAL,           // Social activities
    COLLECTION,       // Item/mount/pet collection
    PROFESSION,       // Crafting and professions
    PVP,              // Player vs Player
    DUNGEON,          // Dungeon achievements
    RAID,             // Raid achievements
    SEASONAL,         // Time-limited seasonal
    HIDDEN            // Secret achievements
};

// [SEQUENCE: 1462] Achievement trigger types
enum class TriggerType {
    COUNTER,          // Reach a certain count
    UNIQUE_COUNT,     // Collect X unique items
    THRESHOLD,        // Reach a threshold value
    BOOLEAN,          // Simple yes/no
    TIMED,            // Complete within time limit
    CONDITIONAL,      // Complex conditions
    PROGRESSIVE,      // Multi-stage achievement
    META              // Complete other achievements
};

// [SEQUENCE: 1463] Achievement data
struct AchievementData {
    uint32_t achievement_id;
    std::string name;
    std::string description;
    AchievementCategory category;
    
    // Display info
    std::string icon_id;
    bool is_hidden = false;
    uint32_t display_order = 0;
    
    // Requirements
    TriggerType trigger_type;
    std::variant<int32_t, float, bool> target_value;
    
    // Rewards
    uint32_t reward_points = 10;
    uint32_t reward_title_id = 0;
    std::vector<std::pair<uint32_t, uint32_t>> reward_items;  // item_id, count
    
    // Related achievements
    uint32_t parent_achievement_id = 0;  // For progressive achievements
    std::vector<uint32_t> required_achievement_ids;  // For meta achievements
    
    // Time constraints
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    bool is_seasonal = false;
};

// [SEQUENCE: 1464] Achievement progress
struct AchievementProgress {
    uint32_t achievement_id;
    std::variant<int32_t, float, bool> current_value;
    bool is_completed = false;
    std::chrono::system_clock::time_point completion_time;
    
    // For progressive achievements
    uint32_t current_stage = 0;
    std::vector<bool> criteria_completed;
    
    // Progress percentage
    float GetProgress(const AchievementData& data) const {
        if (is_completed) return 1.0f;
        
        if (std::holds_alternative<int32_t>(current_value) && 
            std::holds_alternative<int32_t>(data.target_value)) {
            auto current = std::get<int32_t>(current_value);
            auto target = std::get<int32_t>(data.target_value);
            return target > 0 ? static_cast<float>(current) / target : 0.0f;
        }
        
        return 0.0f;
    }
};

// [SEQUENCE: 1465] Achievement criteria
class IAchievementCriteria {
public:
    virtual ~IAchievementCriteria() = default;
    
    virtual bool CheckProgress(const AchievementProgress& progress,
                              const AchievementData& data) const = 0;
    virtual void UpdateProgress(AchievementProgress& progress,
                               const std::variant<int32_t, float, bool>& value) = 0;
};

// [SEQUENCE: 1466] Counter criteria
class CounterCriteria : public IAchievementCriteria {
public:
    bool CheckProgress(const AchievementProgress& progress,
                      const AchievementData& data) const override {
        if (!std::holds_alternative<int32_t>(progress.current_value) ||
            !std::holds_alternative<int32_t>(data.target_value)) {
            return false;
        }
        
        return std::get<int32_t>(progress.current_value) >= 
               std::get<int32_t>(data.target_value);
    }
    
    void UpdateProgress(AchievementProgress& progress,
                       const std::variant<int32_t, float, bool>& value) override {
        if (std::holds_alternative<int32_t>(value)) {
            int32_t current = 0;
            if (std::holds_alternative<int32_t>(progress.current_value)) {
                current = std::get<int32_t>(progress.current_value);
            }
            progress.current_value = current + std::get<int32_t>(value);
        }
    }
};

// [SEQUENCE: 1467] Achievement tracker
class AchievementTracker {
public:
    AchievementTracker(uint64_t entity_id) : entity_id_(entity_id) {}
    
    // [SEQUENCE: 1468] Track achievement progress
    void TrackProgress(uint32_t achievement_id, 
                      const std::variant<int32_t, float, bool>& value) {
        auto it = progress_.find(achievement_id);
        if (it == progress_.end()) {
            // Initialize progress
            AchievementProgress prog;
            prog.achievement_id = achievement_id;
            prog.current_value = value;
            progress_[achievement_id] = prog;
        } else {
            // Update existing progress
            auto* criteria = GetCriteria(achievement_id);
            if (criteria) {
                criteria->UpdateProgress(it->second, value);
            }
        }
        
        // Check for completion
        CheckCompletion(achievement_id);
    }
    
    // [SEQUENCE: 1469] Check if achievement is completed
    bool IsCompleted(uint32_t achievement_id) const {
        auto it = progress_.find(achievement_id);
        return it != progress_.end() && it->second.is_completed;
    }
    
    // [SEQUENCE: 1470] Get achievement progress
    const AchievementProgress* GetProgress(uint32_t achievement_id) const {
        auto it = progress_.find(achievement_id);
        return it != progress_.end() ? &it->second : nullptr;
    }
    
    // [SEQUENCE: 1471] Get completed achievements
    std::vector<uint32_t> GetCompletedAchievements() const {
        std::vector<uint32_t> completed;
        for (const auto& [id, prog] : progress_) {
            if (prog.is_completed) {
                completed.push_back(id);
            }
        }
        return completed;
    }
    
    // [SEQUENCE: 1472] Get achievement points
    uint32_t GetTotalPoints() const {
        uint32_t total = 0;
        for (const auto& [id, prog] : progress_) {
            if (prog.is_completed) {
                // TODO: Get achievement data and add points
                total += 10;  // Default points
            }
        }
        return total;
    }
    
    // [SEQUENCE: 1473] Get achievements by category
    std::vector<uint32_t> GetAchievementsByCategory(AchievementCategory category) const {
        std::vector<uint32_t> achievements;
        // TODO: Filter by category
        return achievements;
    }
    
private:
    uint64_t entity_id_;
    std::unordered_map<uint32_t, AchievementProgress> progress_;
    
    // [SEQUENCE: 1474] Check for achievement completion
    void CheckCompletion(uint32_t achievement_id) {
        auto it = progress_.find(achievement_id);
        if (it == progress_.end() || it->second.is_completed) {
            return;
        }
        
        // TODO: Get achievement data
        // const auto* data = AchievementManager::Instance().GetAchievement(achievement_id);
        // if (!data) return;
        
        // auto* criteria = GetCriteria(achievement_id);
        // if (criteria && criteria->CheckProgress(it->second, *data)) {
        //     CompleteAchievement(achievement_id);
        // }
    }
    
    // [SEQUENCE: 1475] Complete achievement
    void CompleteAchievement(uint32_t achievement_id) {
        auto& prog = progress_[achievement_id];
        prog.is_completed = true;
        prog.completion_time = std::chrono::system_clock::now();
        
        spdlog::info("Entity {} completed achievement {}", entity_id_, achievement_id);
        
        // TODO: Grant rewards
        // TODO: Check for meta achievements
        // TODO: Fire achievement event
    }
    
    // [SEQUENCE: 1476] Get criteria for achievement
    IAchievementCriteria* GetCriteria(uint32_t achievement_id) {
        // TODO: Return appropriate criteria based on achievement type
        static CounterCriteria counter_criteria;
        return &counter_criteria;
    }
};

// [SEQUENCE: 1477] Achievement event types
enum class AchievementEventType {
    // Combat events
    ENEMY_KILLED,
    DAMAGE_DEALT,
    DAMAGE_TAKEN,
    HEALING_DONE,
    DEATH,
    
    // Exploration
    ZONE_DISCOVERED,
    LOCATION_REACHED,
    DISTANCE_TRAVELED,
    
    // Social
    FRIEND_ADDED,
    GUILD_JOINED,
    CHAT_MESSAGE,
    TRADE_COMPLETED,
    
    // Collection
    ITEM_ACQUIRED,
    MOUNT_ACQUIRED,
    PET_ACQUIRED,
    ACHIEVEMENT_EARNED,
    
    // Progression
    LEVEL_REACHED,
    SKILL_LEARNED,
    QUEST_COMPLETED,
    DUNGEON_COMPLETED,
    
    // Custom events
    CUSTOM
};

// [SEQUENCE: 1478] Achievement event
struct AchievementEvent {
    AchievementEventType type;
    uint64_t entity_id;
    std::unordered_map<std::string, std::variant<int32_t, float, std::string>> data;
    std::chrono::system_clock::time_point timestamp;
};

// [SEQUENCE: 1479] Achievement manager
class AchievementManager {
public:
    static AchievementManager& Instance() {
        static AchievementManager instance;
        return instance;
    }
    
    // [SEQUENCE: 1480] Register achievement
    void RegisterAchievement(const AchievementData& achievement) {
        achievements_[achievement.achievement_id] = achievement;
        
        // Index by category
        category_index_[achievement.category].push_back(achievement.achievement_id);
        
        spdlog::info("Registered achievement: {} (ID: {})", 
                    achievement.name, achievement.achievement_id);
    }
    
    // [SEQUENCE: 1481] Get achievement data
    const AchievementData* GetAchievement(uint32_t achievement_id) const {
        auto it = achievements_.find(achievement_id);
        return it != achievements_.end() ? &it->second : nullptr;
    }
    
    // [SEQUENCE: 1482] Process achievement event
    void ProcessEvent(const AchievementEvent& event) {
        // Get all achievements that might be affected by this event
        auto affected = GetAffectedAchievements(event.type);
        
        for (uint32_t achievement_id : affected) {
            ProcessEventForAchievement(event, achievement_id);
        }
    }
    
    // [SEQUENCE: 1483] Get player tracker
    std::shared_ptr<AchievementTracker> GetTracker(uint64_t entity_id) {
        auto it = trackers_.find(entity_id);
        if (it == trackers_.end()) {
            auto tracker = std::make_shared<AchievementTracker>(entity_id);
            trackers_[entity_id] = tracker;
            return tracker;
        }
        return it->second;
    }
    
    // [SEQUENCE: 1484] Get achievements by category
    std::vector<uint32_t> GetAchievementsByCategory(AchievementCategory category) const {
        auto it = category_index_.find(category);
        return it != category_index_.end() ? it->second : std::vector<uint32_t>{};
    }
    
    // [SEQUENCE: 1485] Check seasonal achievements
    void CheckSeasonalAchievements() {
        auto now = std::chrono::system_clock::now();
        
        for (const auto& [id, achievement] : achievements_) {
            if (achievement.is_seasonal) {
                bool is_active = now >= achievement.start_time && 
                               now <= achievement.end_time;
                
                if (is_active) {
                    // TODO: Make achievement available
                } else {
                    // TODO: Hide achievement
                }
            }
        }
    }
    
private:
    AchievementManager() = default;
    
    // Achievement data
    std::unordered_map<uint32_t, AchievementData> achievements_;
    
    // Indexes
    std::unordered_map<AchievementCategory, std::vector<uint32_t>> category_index_;
    std::unordered_map<AchievementEventType, std::vector<uint32_t>> event_index_;
    
    // Player trackers
    std::unordered_map<uint64_t, std::shared_ptr<AchievementTracker>> trackers_;
    
    // [SEQUENCE: 1486] Get achievements affected by event type
    std::vector<uint32_t> GetAffectedAchievements(AchievementEventType event_type) {
        // TODO: Return achievements that track this event type
        return {};
    }
    
    // [SEQUENCE: 1487] Process event for specific achievement
    void ProcessEventForAchievement(const AchievementEvent& event, uint32_t achievement_id) {
        auto tracker = GetTracker(event.entity_id);
        if (!tracker) return;
        
        const auto* achievement = GetAchievement(achievement_id);
        if (!achievement) return;
        
        // Extract relevant value from event
        std::variant<int32_t, float, bool> value;
        
        switch (event.type) {
            case AchievementEventType::ENEMY_KILLED:
                value = int32_t(1);
                break;
            case AchievementEventType::LEVEL_REACHED:
                if (event.data.find("level") != event.data.end()) {
                    value = std::get<int32_t>(event.data.at("level"));
                }
                break;
            // Handle other event types...
            default:
                break;
        }
        
        tracker->TrackProgress(achievement_id, value);
    }
};

// [SEQUENCE: 1488] Achievement builder
class AchievementBuilder {
public:
    AchievementBuilder& WithId(uint32_t id) {
        achievement_.achievement_id = id;
        return *this;
    }
    
    AchievementBuilder& WithName(const std::string& name) {
        achievement_.name = name;
        return *this;
    }
    
    AchievementBuilder& WithDescription(const std::string& desc) {
        achievement_.description = desc;
        return *this;
    }
    
    AchievementBuilder& WithCategory(AchievementCategory category) {
        achievement_.category = category;
        return *this;
    }
    
    AchievementBuilder& WithTrigger(TriggerType type, 
                                   const std::variant<int32_t, float, bool>& target) {
        achievement_.trigger_type = type;
        achievement_.target_value = target;
        return *this;
    }
    
    AchievementBuilder& WithRewardPoints(uint32_t points) {
        achievement_.reward_points = points;
        return *this;
    }
    
    AchievementBuilder& WithRewardTitle(uint32_t title_id) {
        achievement_.reward_title_id = title_id;
        return *this;
    }
    
    AchievementBuilder& WithRewardItem(uint32_t item_id, uint32_t count) {
        achievement_.reward_items.emplace_back(item_id, count);
        return *this;
    }
    
    AchievementBuilder& AsHidden() {
        achievement_.is_hidden = true;
        return *this;
    }
    
    AchievementBuilder& AsSeasonal(std::chrono::system_clock::time_point start,
                                  std::chrono::system_clock::time_point end) {
        achievement_.is_seasonal = true;
        achievement_.start_time = start;
        achievement_.end_time = end;
        return *this;
    }
    
    AchievementData Build() {
        return achievement_;
    }
    
private:
    AchievementData achievement_;
};

// [SEQUENCE: 1489] Achievement event helpers
class AchievementEventHelpers {
public:
    static AchievementEvent CreateKillEvent(uint64_t killer_id, uint32_t enemy_type) {
        AchievementEvent event;
        event.type = AchievementEventType::ENEMY_KILLED;
        event.entity_id = killer_id;
        event.data["enemy_type"] = enemy_type;
        event.timestamp = std::chrono::system_clock::now();
        return event;
    }
    
    static AchievementEvent CreateLevelEvent(uint64_t entity_id, int32_t level) {
        AchievementEvent event;
        event.type = AchievementEventType::LEVEL_REACHED;
        event.entity_id = entity_id;
        event.data["level"] = level;
        event.timestamp = std::chrono::system_clock::now();
        return event;
    }
    
    static AchievementEvent CreateQuestEvent(uint64_t entity_id, uint32_t quest_id) {
        AchievementEvent event;
        event.type = AchievementEventType::QUEST_COMPLETED;
        event.entity_id = entity_id;
        event.data["quest_id"] = static_cast<int32_t>(quest_id);
        event.timestamp = std::chrono::system_clock::now();
        return event;
    }
    
    static AchievementEvent CreateZoneEvent(uint64_t entity_id, uint32_t zone_id) {
        AchievementEvent event;
        event.type = AchievementEventType::ZONE_DISCOVERED;
        event.entity_id = entity_id;
        event.data["zone_id"] = static_cast<int32_t>(zone_id);
        event.timestamp = std::chrono::system_clock::now();
        return event;
    }
};

// [SEQUENCE: 1490] Meta achievement tracker
class MetaAchievementTracker {
public:
    // Check if all required achievements are completed
    static bool CheckMetaAchievement(const AchievementData& meta,
                                    const AchievementTracker& tracker) {
        if (meta.trigger_type != TriggerType::META) {
            return false;
        }
        
        for (uint32_t required_id : meta.required_achievement_ids) {
            if (!tracker.IsCompleted(required_id)) {
                return false;
            }
        }
        
        return true;
    }
};

} // namespace mmorpg::game::achievement