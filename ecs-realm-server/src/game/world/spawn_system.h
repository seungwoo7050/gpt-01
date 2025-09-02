#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <random>
#include "core/ecs/world.h"
#include "map_manager.h"

namespace mmorpg::game::world {

// [SEQUENCE: MVP7-73] Spawn point types
enum class SpawnType {
    STATIC,
    RANDOM_AREA,
    PATH_BASED,
    TRIGGERED,
    WAVE_BASED
};

// [SEQUENCE: MVP7-74] Spawn behavior after creation
enum class SpawnBehavior {
    IDLE,
    PATROL,
    GUARD,
    AGGRESSIVE,
    DEFENSIVE
};

// [SEQUENCE: MVP7-75] Respawn conditions
enum class RespawnCondition {
    TIMER,
    ON_DEATH,
    WORLD_EVENT,
    PLAYER_COUNT,
    CUSTOM
};

// [SEQUENCE: MVP7-76] Spawn point configuration
struct SpawnPoint {
    uint32_t spawn_id;
    SpawnType type;
    float x, y, z;
    float radius = 5.0f;
    float facing = 0.0f;
    uint32_t entity_template_id;
    uint32_t min_count = 1;
    uint32_t max_count = 1;
    RespawnCondition respawn_condition = RespawnCondition::TIMER;
    std::chrono::seconds respawn_time{300};
    SpawnBehavior initial_behavior = SpawnBehavior::IDLE;
    float aggro_radius = 20.0f;
    std::vector<std::pair<float, float>> patrol_points;
    float patrol_speed = 1.0f;
    std::chrono::seconds patrol_pause{5};
    std::function<bool()> spawn_condition;
    std::unordered_set<core::ecs::EntityId> active_entities;
    std::chrono::steady_clock::time_point last_spawn_time;
};

// [SEQUENCE: MVP7-77] Dynamic spawn manager
class SpawnManager {
public:
    SpawnManager(core::ecs::World& ecs_world);
    
    // [SEQUENCE: MVP7-78] Register spawn point
    void RegisterSpawnPoint(uint32_t map_id, const SpawnPoint& spawn_point);
    
    // [SEQUENCE: MVP7-79] Process spawn updates
    void Update(float delta_time);
    
    // [SEQUENCE: MVP7-80] Manual spawn control
    void TriggerSpawn(uint32_t spawn_id);
    void DisableSpawn(uint32_t spawn_id, bool despawn_existing = false);
    void EnableSpawn(uint32_t spawn_id);
    
    // [SEQUENCE: MVP7-81] Spawn density control
    void SetSpawnDensity(uint32_t map_id, float multiplier);
    void SetGlobalSpawnRate(float multiplier);
    
    // [SEQUENCE: MVP7-82] Event-based spawning
    void RegisterEventSpawn(const std::string& event_name, 
                           uint32_t spawn_id,
                           std::function<void()> on_spawn = nullptr);
    void TriggerEvent(const std::string& event_name);
    
    // [SEQUENCE: MVP7-83] Wave spawning
    void StartWaveSpawn(uint32_t spawn_id, 
                       uint32_t wave_count,
                       std::chrono::seconds wave_interval);
    void StopWaveSpawn(uint32_t spawn_id);
    
    // [SEQUENCE: MVP7-84] Get spawn information
    std::vector<SpawnPoint*> GetSpawnPointsInRadius(uint32_t map_id, 
                                                    float x, float y, float z, 
                                                    float radius);
    size_t GetActiveEntityCount(uint32_t spawn_id) const;
    
private:
    // [SEQUENCE: MVP7-85] Process individual spawn point
    void ProcessSpawnPoint(SpawnPoint& spawn_point, uint32_t map_id);
    
    // [SEQUENCE: MVP7-86] Check if spawn should occur
    bool ShouldSpawn(const SpawnPoint& spawn_point) const;
    
    // [SEQUENCE: MVP7-87] Create entity from spawn point
    core::ecs::EntityId SpawnEntity(const SpawnPoint& spawn_point, uint32_t map_id);
    
    // [SEQUENCE: MVP7-88] Calculate spawn position
    std::tuple<float, float, float> CalculateSpawnPosition(const SpawnPoint& spawn_point) const;
    
    // [SEQUENCE: MVP7-89] Setup entity behavior
    void SetupEntityBehavior(core::ecs::EntityId entity_id, const SpawnPoint& spawn_point);
    
    // [SEQUENCE: MVP7-90] Handle entity death
    void OnEntityDeath(core::ecs::EntityId entity_id, uint32_t spawn_id);
    
    // [SEQUENCE: MVP7-91] Update patrol movement
    void UpdatePatrolling(float delta_time);
    
    // [SEQUENCE: MVP7-92] Process wave spawns
    void ProcessWaveSpawns();
    
    core::ecs::World& ecs_world_;
    std::unordered_map<uint32_t, std::vector<SpawnPoint>> map_spawns_;
    std::unordered_map<uint32_t, SpawnPoint*> spawn_registry_;
    
    // Spawn control
    std::unordered_set<uint32_t> disabled_spawns_;
    std::unordered_map<uint32_t, float> map_density_multipliers_;
    float global_spawn_rate_ = 1.0f;
    
    // Event spawns
    std::unordered_map<std::string, std::vector<uint32_t>> event_spawns_;
    
    // Wave spawns
    struct WaveSpawnInfo {
        uint32_t remaining_waves;
        std::chrono::steady_clock::time_point next_wave_time;
        std::chrono::seconds interval;
    };
    std::unordered_map<uint32_t, WaveSpawnInfo> wave_spawns_;
    
    // Patrol tracking
    struct PatrolInfo {
        size_t current_point = 0;
        bool reverse_direction = false;
        std::chrono::steady_clock::time_point pause_until;
    };
    std::unordered_map<core::ecs::EntityId, PatrolInfo> patrol_states_;
    
    std::mt19937 rng_{std::random_device{}()};
};

// [SEQUENCE: MVP7-93] Spawn templates for different entity types
class SpawnTemplateRegistry {
public:
    struct EntityTemplate {
        std::string template_name;
        uint32_t entity_type_id;
        std::function<void(core::ecs::EntityId)> component_initializer;
        std::string ai_script;
        float base_health = 100.0f;
        float base_damage = 10.0f;
        uint32_t loot_table_id = 0;
        float loot_chance = 1.0f;
        bool is_elite = false;
        bool is_boss = false;
        bool is_rare = false;
    };
    
    static SpawnTemplateRegistry& Instance();
    
    // [SEQUENCE: MVP7-94] Register entity template
    void RegisterTemplate(uint32_t template_id, const EntityTemplate& template_data);
    
    // [SEQUENCE: MVP7-95] Get template for spawning
    std::optional<EntityTemplate> GetTemplate(uint32_t template_id) const;
    
    // [SEQUENCE: MVP7-96] Create entity from template
    core::ecs::EntityId CreateFromTemplate(uint32_t template_id, 
                                          core::ecs::World& world,
                                          float x, float y, float z);
    
private:
    std::unordered_map<uint32_t, EntityTemplate> templates_;
};

// [SEQUENCE: MVP7-97] Special spawn types implementation
class SpecialSpawnHandler {
public:
    // [SEQUENCE: MVP7-98] Rare spawn with special conditions
    static SpawnPoint CreateRareSpawn(uint32_t entity_template_id,
                                     float x, float y, float z,
                                     float spawn_chance = 0.01f,
                                     std::chrono::hours respawn_time = std::chrono::hours(24));
    
    // [SEQUENCE: MVP7-99] Boss spawn with announcement
    static SpawnPoint CreateBossSpawn(uint32_t boss_template_id,
                                     float x, float y, float z,
                                     const std::string& announcement_text);
    
    // [SEQUENCE: MVP7-100] Treasure goblin style spawn (runs away)
    static SpawnPoint CreateTreasureSpawn(uint32_t treasure_template_id,
                                         float x, float y, float z,
                                         float escape_speed = 2.0f);
    
    // [SEQUENCE: MVP7-101] Invasion spawn (large scale event)
    static std::vector<SpawnPoint> CreateInvasionSpawns(
        uint32_t map_id,
        const std::vector<uint32_t>& enemy_templates,
        float center_x, float center_y,
        float radius,
        uint32_t total_enemies);
};

// [SEQUENCE: MVP7-102] Spawn density manager for population control
class SpawnDensityController {
public:
    // [SEQUENCE: MVP7-103] Calculate optimal spawn density
    static float CalculateOptimalDensity(uint32_t map_id,
                                        size_t player_count,
                                        float map_size);
    
    // [SEQUENCE: MVP7-104] Adjust spawns based on server load
    static void AdjustForServerLoad(SpawnManager& spawn_manager,
                                   float cpu_usage,
                                   size_t total_entities);
    
    // [SEQUENCE: MVP7-105] Dynamic spawn scaling
    static void EnableDynamicScaling(SpawnManager& spawn_manager,
                                    float min_density = 0.5f,
                                    float max_density = 2.0f);
};

} // namespace mmorpg::game::world