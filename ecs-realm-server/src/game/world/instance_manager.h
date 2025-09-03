#pragma once

#include <memory>
#include <unordered_map>
#include <chrono>
#include <optional>
#include "map_manager.h"
#include "core/ecs/world.h"

namespace mmorpg::game::world {

// [SEQUENCE: 512] Instance difficulty levels
enum class InstanceDifficulty {
    NORMAL = 0,
    HARD = 1,
    HEROIC = 2,
    MYTHIC = 3,
    MYTHIC_PLUS = 4  // With scaling difficulty
};

// [SEQUENCE: 513] Instance reset frequency
enum class ResetFrequency {
    NEVER,      // One-time instances
    DAILY,      // Reset at daily reset time
    WEEKLY,     // Reset on weekly reset day
    MONTHLY,    // Reset on first day of month
    ON_LEAVE    // Reset when all players leave
};

// [SEQUENCE: 514] Instance state tracking
enum class InstanceState {
    ACTIVE,         // Players can enter
    IN_PROGRESS,    // Combat has started, no new entries
    COMPLETED,      // All objectives done
    RESETTING,      // Being reset
    EXPIRED         // Scheduled for deletion
};

// [SEQUENCE: 515] Instance configuration
struct InstanceConfig {
    uint32_t template_id;           // Base instance template
    std::string instance_name;
    InstanceDifficulty difficulty;
    ResetFrequency reset_frequency;
    
    // Player limits
    uint32_t min_players = 1;
    uint32_t max_players = 5;
    uint32_t recommended_level = 60;
    uint32_t min_item_level = 0;    // Gear score requirement
    
    // Time limits
    std::chrono::minutes time_limit{0};      // 0 = no limit
    std::chrono::minutes soft_reset_time{30}; // Time before reset after completion
    
    // Loot settings
    uint32_t loot_mode = 0;         // Personal, Group, Master loot
    float bonus_loot_chance = 0.0f;  // Extra loot chance
    
    // Objectives
    struct Objective {
        uint32_t objective_id;
        std::string description;
        bool required = true;       // Required for completion
        uint32_t target_count = 1;  // Kill X bosses, collect Y items
    };
    std::vector<Objective> objectives;
    
    // Boss information
    struct BossInfo {
        uint32_t boss_id;
        std::string boss_name;
        float spawn_x, spawn_y, spawn_z;
        uint32_t loot_table_id;
        bool is_final_boss = false;
    };
    std::vector<BossInfo> bosses;
};

// [SEQUENCE: 516] Instance progress tracking
struct InstanceProgress {
    uint64_t instance_guid;         // Unique instance identifier
    uint32_t instance_id;           // Map instance ID
    InstanceState state = InstanceState::ACTIVE;
    InstanceDifficulty difficulty;
    
    // Timing
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point started_at;  // First combat
    std::chrono::system_clock::time_point completed_at;
    std::chrono::system_clock::time_point reset_at;
    
    // Players
    std::unordered_set<uint64_t> allowed_players;  // Can enter
    std::unordered_set<uint64_t> saved_players;    // Have save/lockout
    uint64_t leader_id = 0;         // Instance leader
    
    // Progress
    std::unordered_map<uint32_t, uint32_t> objective_progress;
    std::unordered_set<uint32_t> killed_bosses;
    uint32_t wipe_count = 0;        // Total party wipes
    
    // Mythic+ specific
    uint32_t mythic_level = 0;      // M+ keystone level
    float time_multiplier = 1.0f;   // Speed run bonus
    uint32_t deaths_count = 0;      // Affects rewards
};

// [SEQUENCE: 517] Instance save/lockout information
struct InstanceSave {
    uint64_t save_id;
    uint64_t player_id;
    uint32_t instance_template_id;
    InstanceDifficulty difficulty;
    
    // Lockout info
    std::chrono::system_clock::time_point locked_until;
    std::unordered_set<uint32_t> killed_bosses;
    bool is_expired = false;
};

// [SEQUENCE: 518] Instance manager for dungeon/raid instances
class InstanceManager {
public:
    static InstanceManager& Instance() {
        static InstanceManager instance;
        return instance;
    }
    
    // [SEQUENCE: 519] Register instance template
    void RegisterInstanceTemplate(uint32_t template_id, const InstanceConfig& config);
    
    // [SEQUENCE: 520] Create new instance
    std::optional<uint64_t> CreateInstance(
        uint32_t template_id,
        InstanceDifficulty difficulty,
        uint64_t leader_id,
        const std::vector<uint64_t>& party_members
    );
    
    // [SEQUENCE: 521] Check if player can enter instance
    bool CanEnterInstance(uint64_t player_id, uint64_t instance_guid, std::string& error);
    
    // [SEQUENCE: 522] Player enters instance
    bool EnterInstance(uint64_t player_id, uint64_t instance_guid);
    
    // [SEQUENCE: 523] Player leaves instance
    void LeaveInstance(uint64_t player_id, uint64_t instance_guid);
    
    // [SEQUENCE: 524] Update instance progress
    void UpdateObjectiveProgress(uint64_t instance_guid, uint32_t objective_id, uint32_t count = 1);
    void RecordBossKill(uint64_t instance_guid, uint32_t boss_id);
    void RecordWipe(uint64_t instance_guid);
    
    // [SEQUENCE: 525] Check and handle instance completion
    bool CheckCompletion(uint64_t instance_guid);
    void CompleteInstance(uint64_t instance_guid);
    
    // [SEQUENCE: 526] Instance reset handling
    void ScheduleReset(uint64_t instance_guid);
    void ResetInstance(uint64_t instance_guid);
    void ProcessScheduledResets();
    
    // [SEQUENCE: 527] Save/lockout management
    void CreateInstanceSave(uint64_t player_id, uint64_t instance_guid);
    std::optional<InstanceSave> GetPlayerSave(uint64_t player_id, uint32_t template_id, InstanceDifficulty difficulty);
    bool HasValidLockout(uint64_t player_id, uint32_t template_id, InstanceDifficulty difficulty);
    
    // [SEQUENCE: 528] Get instance information
    std::optional<InstanceProgress> GetInstanceProgress(uint64_t instance_guid);
    std::vector<InstanceProgress> GetPlayerInstances(uint64_t player_id);
    
    // [SEQUENCE: 529] Mythic+ specific
    void StartMythicPlus(uint64_t instance_guid, uint32_t keystone_level);
    void CompleteMythicPlus(uint64_t instance_guid, bool in_time);
    float CalculateMythicPlusScore(uint64_t instance_guid);
    
    // [SEQUENCE: 530] Cleanup expired instances
    void CleanupExpiredInstances();
    
private:
    InstanceManager() = default;
    
    // [SEQUENCE: 531] Generate unique instance GUID
    uint64_t GenerateInstanceGuid();
    
    // [SEQUENCE: 532] Calculate reset time based on frequency
    std::chrono::system_clock::time_point CalculateResetTime(ResetFrequency frequency);
    
    // [SEQUENCE: 533] Check if all objectives completed
    bool AreObjectivesComplete(const InstanceProgress& progress, const InstanceConfig& config);
    
    // [SEQUENCE: 534] Distribute loot to players
    void DistributeLoot(uint64_t instance_guid, uint32_t boss_id);
    
    // [SEQUENCE: 535] Scale instance difficulty for Mythic+
    void ScaleMythicPlusDifficulty(uint64_t instance_guid, uint32_t level);
    
    // Data storage
    std::unordered_map<uint32_t, InstanceConfig> instance_templates_;
    std::unordered_map<uint64_t, InstanceProgress> active_instances_;
    std::unordered_map<uint64_t, std::vector<InstanceSave>> player_saves_;  // player_id -> saves
    
    // Reset scheduling
    std::multimap<std::chrono::system_clock::time_point, uint64_t> scheduled_resets_;
    
    // Thread safety
    mutable std::mutex instance_mutex_;
    std::atomic<uint64_t> next_instance_guid_{1};
};

// [SEQUENCE: 536] Instance event handler
class InstanceEventHandler {
public:
    // [SEQUENCE: 537] Called when instance is created
    virtual void OnInstanceCreated(uint64_t instance_guid, const InstanceConfig& config) {}
    
    // [SEQUENCE: 538] Called when player enters/leaves
    virtual void OnPlayerEntered(uint64_t instance_guid, uint64_t player_id) {}
    virtual void OnPlayerLeft(uint64_t instance_guid, uint64_t player_id) {}
    
    // [SEQUENCE: 539] Called on progress updates
    virtual void OnBossKilled(uint64_t instance_guid, uint32_t boss_id) {}
    virtual void OnObjectiveComplete(uint64_t instance_guid, uint32_t objective_id) {}
    virtual void OnInstanceComplete(uint64_t instance_guid) {}
    
    // [SEQUENCE: 540] Called on wipes/resets
    virtual void OnPartyWipe(uint64_t instance_guid) {}
    virtual void OnInstanceReset(uint64_t instance_guid) {}
};

} // namespace mmorpg::game::world