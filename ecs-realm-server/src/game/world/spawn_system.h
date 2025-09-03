#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <random>
#include "core/ecs/world.h"
#include "map_manager.h"

namespace mmorpg::game::world {

// [SEQUENCE: 580] Spawn point types
enum class SpawnType {
    STATIC,         // 고정 위치 스폰
    RANDOM_AREA,    // 영역 내 랜덤 스폰
    PATH_BASED,     // 경로 기반 순찰 스폰
    TRIGGERED,      // 이벤트 트리거 스폰
    WAVE_BASED      // 웨이브 기반 스폰
};

// [SEQUENCE: 581] Spawn behavior after creation
enum class SpawnBehavior {
    IDLE,           // 제자리 대기
    PATROL,         // 순찰
    GUARD,          // 경계
    AGGRESSIVE,     // 즉시 공격
    DEFENSIVE       // 방어적
};

// [SEQUENCE: 582] Respawn conditions
enum class RespawnCondition {
    TIMER,          // 일정 시간 후
    ON_DEATH,       // 죽은 직후
    WORLD_EVENT,    // 월드 이벤트 발생 시
    PLAYER_COUNT,   // 플레이어 수에 따라
    CUSTOM          // 커스텀 조건
};

// [SEQUENCE: 583] Spawn point configuration
struct SpawnPoint {
    uint32_t spawn_id;
    SpawnType type;
    
    // Location data
    float x, y, z;
    float radius = 5.0f;        // 스폰 반경
    float facing = 0.0f;        // 초기 방향
    
    // Entity to spawn
    uint32_t entity_template_id;
    uint32_t min_count = 1;     // 최소 스폰 수
    uint32_t max_count = 1;     // 최대 스폰 수
    
    // Respawn settings
    RespawnCondition respawn_condition = RespawnCondition::TIMER;
    std::chrono::seconds respawn_time{300};  // 5분 기본값
    
    // Level scaling
    uint32_t base_level = 1;
    uint32_t level_variance = 0;  // ±n 레벨 변동
    
    // Behavior
    SpawnBehavior initial_behavior = SpawnBehavior::IDLE;
    float aggro_radius = 20.0f;
    
    // Patrol data (if applicable)
    std::vector<std::pair<float, float>> patrol_points;
    float patrol_speed = 1.0f;
    std::chrono::seconds patrol_pause{5};
    
    // Spawn conditions
    std::function<bool()> spawn_condition;
    
    // Active tracking
    std::unordered_set<core::ecs::EntityId> active_entities;
    std::chrono::steady_clock::time_point last_spawn_time;
};

// [SEQUENCE: 584] Dynamic spawn manager
class SpawnManager {
public:
    SpawnManager(core::ecs::World& ecs_world);
    
    // [SEQUENCE: 585] Register spawn point
    void RegisterSpawnPoint(uint32_t map_id, const SpawnPoint& spawn_point);
    
    // [SEQUENCE: 586] Process spawn updates
    void Update(float delta_time);
    
    // [SEQUENCE: 587] Manual spawn control
    void TriggerSpawn(uint32_t spawn_id);
    void DisableSpawn(uint32_t spawn_id, bool despawn_existing = false);
    void EnableSpawn(uint32_t spawn_id);
    
    // [SEQUENCE: 588] Spawn density control
    void SetSpawnDensity(uint32_t map_id, float multiplier);
    void SetGlobalSpawnRate(float multiplier);
    
    // [SEQUENCE: 589] Event-based spawning
    void RegisterEventSpawn(const std::string& event_name, 
                           uint32_t spawn_id,
                           std::function<void()> on_spawn = nullptr);
    void TriggerEvent(const std::string& event_name);
    
    // [SEQUENCE: 590] Wave spawning
    void StartWaveSpawn(uint32_t spawn_id, 
                       uint32_t wave_count,
                       std::chrono::seconds wave_interval);
    void StopWaveSpawn(uint32_t spawn_id);
    
    // [SEQUENCE: 591] Get spawn information
    std::vector<SpawnPoint*> GetSpawnPointsInRadius(uint32_t map_id, 
                                                    float x, float y, float z, 
                                                    float radius);
    size_t GetActiveEntityCount(uint32_t spawn_id) const;
    
private:
    // [SEQUENCE: 592] Process individual spawn point
    void ProcessSpawnPoint(SpawnPoint& spawn_point, uint32_t map_id);
    
    // [SEQUENCE: 593] Check if spawn should occur
    bool ShouldSpawn(const SpawnPoint& spawn_point) const;
    
    // [SEQUENCE: 594] Create entity from spawn point
    core::ecs::EntityId SpawnEntity(const SpawnPoint& spawn_point, uint32_t map_id);
    
    // [SEQUENCE: 595] Calculate spawn position
    std::tuple<float, float, float> CalculateSpawnPosition(const SpawnPoint& spawn_point) const;
    
    // [SEQUENCE: 596] Setup entity behavior
    void SetupEntityBehavior(core::ecs::EntityId entity_id, const SpawnPoint& spawn_point);
    
    // [SEQUENCE: 597] Handle entity death
    void OnEntityDeath(core::ecs::EntityId entity_id, uint32_t spawn_id);
    
    // [SEQUENCE: 598] Update patrol movement
    void UpdatePatrolling(float delta_time);
    
    // [SEQUENCE: 599] Process wave spawns
    void ProcessWaveSpawns();
    
    core::ecs::World& ecs_world_;
    std::unordered_map<uint32_t, std::vector<SpawnPoint>> map_spawns_;  // map_id -> spawns
    std::unordered_map<uint32_t, SpawnPoint*> spawn_registry_;          // spawn_id -> spawn
    
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

// [SEQUENCE: 600] Spawn templates for different entity types
class SpawnTemplateRegistry {
public:
    struct EntityTemplate {
        std::string template_name;
        uint32_t entity_type_id;
        
        // Components to add
        std::function<void(core::ecs::EntityId)> component_initializer;
        
        // AI behavior
        std::string ai_script;
        float base_health = 100.0f;
        float base_damage = 10.0f;
        
        // Loot
        uint32_t loot_table_id = 0;
        float loot_chance = 1.0f;
        
        // Special properties
        bool is_elite = false;
        bool is_boss = false;
        bool is_rare = false;
    };
    
    static SpawnTemplateRegistry& Instance() {
        static SpawnTemplateRegistry instance;
        return instance;
    }
    
    // [SEQUENCE: 601] Register entity template
    void RegisterTemplate(uint32_t template_id, const EntityTemplate& template_data);
    
    // [SEQUENCE: 602] Get template for spawning
    std::optional<EntityTemplate> GetTemplate(uint32_t template_id) const;
    
    // [SEQUENCE: 603] Create entity from template
    core::ecs::EntityId CreateFromTemplate(uint32_t template_id, 
                                          core::ecs::World& world,
                                          float x, float y, float z);
    
private:
    std::unordered_map<uint32_t, EntityTemplate> templates_;
};

// [SEQUENCE: 604] Special spawn types implementation
class SpecialSpawnHandler {
public:
    // [SEQUENCE: 605] Rare spawn with special conditions
    static SpawnPoint CreateRareSpawn(uint32_t entity_template_id,
                                     float x, float y, float z,
                                     float spawn_chance = 0.01f,
                                     std::chrono::hours respawn_time = std::chrono::hours(24));
    
    // [SEQUENCE: 606] Boss spawn with announcement
    static SpawnPoint CreateBossSpawn(uint32_t boss_template_id,
                                     float x, float y, float z,
                                     const std::string& announcement_text);
    
    // [SEQUENCE: 607] Treasure goblin style spawn (runs away)
    static SpawnPoint CreateTreasureSpawn(uint32_t treasure_template_id,
                                         float x, float y, float z,
                                         float escape_speed = 2.0f);
    
    // [SEQUENCE: 608] Invasion spawn (large scale event)
    static std::vector<SpawnPoint> CreateInvasionSpawns(
        uint32_t map_id,
        const std::vector<uint32_t>& enemy_templates,
        float center_x, float center_y,
        float radius,
        uint32_t total_enemies);
};

// [SEQUENCE: 609] Spawn density manager for population control
class SpawnDensityController {
public:
    // [SEQUENCE: 610] Calculate optimal spawn density
    static float CalculateOptimalDensity(uint32_t map_id,
                                        size_t player_count,
                                        float map_size);
    
    // [SEQUENCE: 611] Adjust spawns based on server load
    static void AdjustForServerLoad(SpawnManager& spawn_manager,
                                   float cpu_usage,
                                   size_t total_entities);
    
    // [SEQUENCE: 612] Dynamic spawn scaling
    static void EnableDynamicScaling(SpawnManager& spawn_manager,
                                    float min_density = 0.5f,
                                    float max_density = 2.0f);
};

} // namespace mmorpg::game::world