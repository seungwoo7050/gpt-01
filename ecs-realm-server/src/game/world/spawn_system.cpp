#include "spawn_system.h"
#include "core/ecs/components/transform_component.h"
#include "core/ecs/components/health_component.h"
#include "game/components/ai_component.h"
#include "game/components/npc_component.h"
#include <spdlog/spdlog.h>
#include <cmath>

namespace mmorpg::game::world {

// [SEQUENCE: MVP7-106] SpawnManager constructor
SpawnManager::SpawnManager(core::ecs::World& ecs_world)
    : ecs_world_(ecs_world) {
    spdlog::info("SpawnManager initialized");
}

// [SEQUENCE: MVP7-107] Register spawn point for a map
void SpawnManager::RegisterSpawnPoint(uint32_t map_id, const SpawnPoint& spawn_point) {
    map_spawns_[map_id].push_back(spawn_point);
    spawn_registry_[spawn_point.spawn_id] = &map_spawns_[map_id].back();
    
    spdlog::info("Registered spawn point {} for map {} at ({}, {}, {})", 
                 spawn_point.spawn_id, map_id, spawn_point.x, spawn_point.y, spawn_point.z);
}

// [SEQUENCE: MVP7-108] Main update loop for spawn system
void SpawnManager::Update(float delta_time) {
    // Process each map's spawn points
    for (auto& [map_id, spawn_points] : map_spawns_) {
        for (auto& spawn_point : spawn_points) {
            ProcessSpawnPoint(spawn_point, map_id);
        }
    }
    
    // Update patrolling entities
    UpdatePatrolling(delta_time);
    
    // Process wave spawns
    ProcessWaveSpawns();
}

// [SEQUENCE: MVP7-109] Process individual spawn point
void SpawnManager::ProcessSpawnPoint(SpawnPoint& spawn_point, uint32_t map_id) {
    // Skip disabled spawns
    if (disabled_spawns_.count(spawn_point.spawn_id) > 0) {
        return;
    }
    
    // [SEQUENCE: MVP7-110] Remove dead entities from tracking
    auto it = spawn_point.active_entities.begin();
    while (it != spawn_point.active_entities.end()) {
        auto* health = ecs_world_.GetComponent<core::ecs::HealthComponent>(*it);
        if (!health || health->is_dead) {
            it = spawn_point.active_entities.erase(it);
        } else {
            ++it;
        }
    }
    
    // [SEQUENCE: MVP7-111] Check if we should spawn
    if (ShouldSpawn(spawn_point)) {
        // Calculate how many to spawn
        uint32_t current_count = spawn_point.active_entities.size();
        uint32_t desired_count = spawn_point.min_count;
        
        if (spawn_point.max_count > spawn_point.min_count) {
            std::uniform_int_distribution<uint32_t> dist(spawn_point.min_count, spawn_point.max_count);
            desired_count = dist(rng_);
        }
        
        // Apply density multipliers
        float density = global_spawn_rate_;
        if (map_density_multipliers_.count(map_id) > 0) {
            density *= map_density_multipliers_[map_id];
        }
        desired_count = static_cast<uint32_t>(desired_count * density);
        
        // [SEQUENCE: MVP7-112] Spawn missing entities
        while (current_count < desired_count) {
            auto entity_id = SpawnEntity(spawn_point, map_id);
            if (entity_id != core::ecs::INVALID_ENTITY) {
                spawn_point.active_entities.insert(entity_id);
                current_count++;
            } else {
                break;  // Failed to spawn
            }
        }
        
        spawn_point.last_spawn_time = std::chrono::steady_clock::now();
    }
}

// [SEQUENCE: MVP7-113] Check if spawn should occur
bool SpawnManager::ShouldSpawn(const SpawnPoint& spawn_point) const {
    // Check if under minimum count
    if (spawn_point.active_entities.size() >= spawn_point.min_count) {
        return false;
    }
    
    // Check respawn condition
    switch (spawn_point.respawn_condition) {
        case RespawnCondition::TIMER: {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = now - spawn_point.last_spawn_time;
            return elapsed >= spawn_point.respawn_time;
        }
        case RespawnCondition::ON_DEATH:
            return true;  // Spawn immediately if under min count
        case RespawnCondition::CUSTOM:
            return spawn_point.spawn_condition ? spawn_point.spawn_condition() : false;
        default:
            return false;
    }
}

// [SEQUENCE: MVP7-114] Create entity from spawn point
core::ecs::EntityId SpawnManager::SpawnEntity(const SpawnPoint& spawn_point, uint32_t map_id) {
    // Get spawn position
    auto [x, y, z] = CalculateSpawnPosition(spawn_point);
    
    // Create entity from template
    auto entity_id = SpawnTemplateRegistry::Instance().CreateFromTemplate(
        spawn_point.entity_template_id, ecs_world_, x, y, z
    );
    
    if (entity_id == core::ecs::INVALID_ENTITY) {
        spdlog::error("Failed to spawn entity from template {}", spawn_point.entity_template_id);
        return core::ecs::INVALID_ENTITY;
    }
    
    // [SEQUENCE: MVP7-115] Set map information
    auto* transform = ecs_world_.GetComponent<core::ecs::TransformComponent>(entity_id);
    if (transform) {
        transform->map_id = map_id;
        transform->rotation.y = spawn_point.facing;
    }
    
    // [SEQUENCE: MVP7-116] Apply level variance
    if (spawn_point.level_variance > 0) {
        std::uniform_int_distribution<int> level_dist(
            -static_cast<int>(spawn_point.level_variance),
            static_cast<int>(spawn_point.level_variance)
        );
        int level_adjustment = level_dist(rng_);
        
        auto* npc = ecs_world_.GetComponent<NPCComponent>(entity_id);
        if (npc) {
            npc->level = std::max(1u, spawn_point.base_level + level_adjustment);
        }
    }
    
    // Setup behavior
    SetupEntityBehavior(entity_id, spawn_point);
    
    spdlog::debug("Spawned entity {} at ({}, {}, {}) for spawn point {}", 
                  entity_id, x, y, z, spawn_point.spawn_id);
    
    return entity_id;
}

// [SEQUENCE: MVP7-117] Calculate spawn position based on spawn type
std::tuple<float, float, float> SpawnManager::CalculateSpawnPosition(const SpawnPoint& spawn_point) const {
    float x = spawn_point.x;
    float y = spawn_point.y;
    float z = spawn_point.z;
    
    switch (spawn_point.type) {
        case SpawnType::RANDOM_AREA: {
            // Random position within radius
            std::uniform_real_distribution<float> angle_dist(0, 2 * M_PI);
            std::uniform_real_distribution<float> radius_dist(0, spawn_point.radius);
            
            float angle = angle_dist(rng_);
            float radius = radius_dist(rng_);
            
            x += radius * std::cos(angle);
            y += radius * std::sin(angle);
            break;
        }
        case SpawnType::PATH_BASED: {
            // Start at first patrol point if available
            if (!spawn_point.patrol_points.empty()) {
                x = spawn_point.patrol_points[0].first;
                y = spawn_point.patrol_points[0].second;
            }
            break;
        }
        default:
            // Static spawn - use exact position
            break;
    }
    
    return {x, y, z};
}

// [SEQUENCE: MVP7-118] Setup entity behavior after spawning
void SpawnManager::SetupEntityBehavior(core::ecs::EntityId entity_id, const SpawnPoint& spawn_point) {
    auto* ai = ecs_world_.GetComponent<AIComponent>(entity_id);
    if (!ai) return;
    
    // [SEQUENCE: MVP7-119] Set initial behavior
    switch (spawn_point.initial_behavior) {
        case SpawnBehavior::PATROL:
            ai->behavior_state = AIBehaviorState::PATROLLING;
            if (!spawn_point.patrol_points.empty()) {
                PatrolInfo patrol_info;
                patrol_info.current_point = 0;
                patrol_states_[entity_id] = patrol_info;
            }
            break;
        case SpawnBehavior::GUARD:
            ai->behavior_state = AIBehaviorState::GUARDING;
            ai->guard_position = {spawn_point.x, spawn_point.y, spawn_point.z};
            ai->guard_radius = spawn_point.radius;
            break;
        case SpawnBehavior::AGGRESSIVE:
            ai->behavior_state = AIBehaviorState::AGGRESSIVE;
            ai->aggro_radius = spawn_point.aggro_radius * 1.5f;  // Increased aggro range
            break;
        case SpawnBehavior::DEFENSIVE:
            ai->behavior_state = AIBehaviorState::DEFENSIVE;
            ai->aggro_radius = spawn_point.aggro_radius * 0.5f;  // Reduced aggro range
            break;
        default:
            ai->behavior_state = AIBehaviorState::IDLE;
            break;
    }
    
    ai->aggro_radius = spawn_point.aggro_radius;
}

// [SEQUENCE: MVP7-120] Update patrolling entities
void SpawnManager::UpdatePatrolling(float delta_time) {
    auto now = std::chrono::steady_clock::now();
    
    for (auto& [entity_id, patrol_info] : patrol_states_) {
        // Check if paused
        if (now < patrol_info.pause_until) {
            continue;
        }
        
        auto* transform = ecs_world_.GetComponent<core::ecs::TransformComponent>(entity_id);
        auto* ai = ecs_world_.GetComponent<AIComponent>(entity_id);
        if (!transform || !ai || ai->behavior_state != AIBehaviorState::PATROLLING) {
            continue;
        }
        
        // Find the spawn point for this entity
        SpawnPoint* spawn_point = nullptr;
        for (auto& [spawn_id, sp] : spawn_registry_) {
            if (sp->active_entities.count(entity_id) > 0) {
                spawn_point = sp;
                break;
            }
        }
        
        if (!spawn_point || spawn_point->patrol_points.empty()) {
            continue;
        }
        
        // [SEQUENCE: MVP7-121] Move towards current patrol point
        auto& target_point = spawn_point->patrol_points[patrol_info.current_point];
        float dx = target_point.first - transform->position.x;
        float dy = target_point.second - transform->position.y;
        float distance = std::sqrt(dx*dx + dy*dy);
        
        if (distance < 1.0f) {
            // Reached patrol point
            patrol_info.pause_until = now + spawn_point->patrol_pause;
            
            // Move to next point
            if (patrol_info.reverse_direction) {
                if (patrol_info.current_point == 0) {
                    patrol_info.reverse_direction = false;
                    patrol_info.current_point = 1;
                } else {
                    patrol_info.current_point--;
                }
            } else {
                if (patrol_info.current_point == spawn_point->patrol_points.size() - 1) {
                    patrol_info.reverse_direction = true;
                    patrol_info.current_point--;
                } else {
                    patrol_info.current_point++;
                }
            }
        } else {
            // Move towards point
            float move_distance = spawn_point->patrol_speed * delta_time;
            transform->position.x += (dx / distance) * move_distance;
            transform->position.y += (dy / distance) * move_distance;
        }
    }
}

// [SEQUENCE: MVP7-122] Trigger manual spawn
void SpawnManager::TriggerSpawn(uint32_t spawn_id) {
    auto it = spawn_registry_.find(spawn_id);
    if (it == spawn_registry_.end()) {
        spdlog::warn("Spawn point {} not found", spawn_id);
        return;
    }
    
    auto* spawn_point = it->second;
    
    // Find map ID
    uint32_t map_id = 0;
    for (const auto& [mid, spawns] : map_spawns_) {
        for (const auto& sp : spawns) {
            if (sp.spawn_id == spawn_id) {
                map_id = mid;
                break;
            }
        }
        if (map_id != 0) break;
    }
    
    // Force spawn
    auto entity_id = SpawnEntity(*spawn_point, map_id);
    if (entity_id != core::ecs::INVALID_ENTITY) {
        spawn_point->active_entities.insert(entity_id);
    }
}

// [SEQUENCE: MVP7-123] Start wave spawning
void SpawnManager::StartWaveSpawn(uint32_t spawn_id, uint32_t wave_count, std::chrono::seconds wave_interval) {
    WaveSpawnInfo wave_info;
    wave_info.remaining_waves = wave_count;
    wave_info.next_wave_time = std::chrono::steady_clock::now();
    wave_info.interval = wave_interval;
    
    wave_spawns_[spawn_id] = wave_info;
    
    spdlog::info("Started wave spawn for spawn point {} with {} waves", spawn_id, wave_count);
}

// [SEQUENCE: MVP7-124] Process wave spawns
void SpawnManager::ProcessWaveSpawns() {
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = wave_spawns_.begin(); it != wave_spawns_.end();) {
        auto& [spawn_id, wave_info] = *it;
        
        if (now >= wave_info.next_wave_time && wave_info.remaining_waves > 0) {
            // Trigger spawn for this wave
            TriggerSpawn(spawn_id);
            
            wave_info.remaining_waves--;
            wave_info.next_wave_time = now + wave_info.interval;
            
            if (wave_info.remaining_waves == 0) {
                spdlog::info("Wave spawn completed for spawn point {}", spawn_id);
                it = wave_spawns_.erase(it);
                continue;
            }
        }
        ++it;
    }
}

// [SEQUENCE: MVP7-125] SpawnTemplateRegistry - Create from template
core::ecs::EntityId SpawnTemplateRegistry::CreateFromTemplate(uint32_t template_id, 
                                                             core::ecs::World& world,
                                                             float x, float y, float z) {
    auto template_opt = GetTemplate(template_id);
    if (!template_opt) {
        spdlog::error("Template {} not found", template_id);
        return core::ecs::INVALID_ENTITY;
    }
    
    const auto& template_data = *template_opt;
    
    // [SEQUENCE: MVP7-126] Create entity
    auto entity_id = world.CreateEntity();
    
    // Add transform component
    world.AddComponent<core::ecs::TransformComponent>(entity_id, x, y, z);
    
    // Add health component
    world.AddComponent<core::ecs::HealthComponent>(entity_id, 
        template_data.base_health, template_data.base_health);
    
    // Add NPC component
    auto& npc = world.AddComponent<NPCComponent>(entity_id);
    npc.npc_type = template_data.entity_type_id;
    npc.is_elite = template_data.is_elite;
    npc.is_boss = template_data.is_boss;
    npc.loot_table_id = template_data.loot_table_id;
    
    // Add AI component
    auto& ai = world.AddComponent<AIComponent>(entity_id);
    ai.ai_script = template_data.ai_script;
    
    // Run custom initializer
    if (template_data.component_initializer) {
        template_data.component_initializer(entity_id);
    }
    
    return entity_id;
}

// [SEQUENCE: MVP7-127] Create rare spawn
SpawnPoint SpecialSpawnHandler::CreateRareSpawn(uint32_t entity_template_id,
                                               float x, float y, float z,
                                               float spawn_chance,
                                               std::chrono::hours respawn_time) {
    SpawnPoint spawn;
    spawn.spawn_id = std::hash<std::string>{}(std::to_string(entity_template_id) + "_rare_" + std::to_string(x));
    spawn.type = SpawnType::STATIC;
    spawn.x = x;
    spawn.y = y;
    spawn.z = z;
    spawn.entity_template_id = entity_template_id;
    spawn.min_count = 0;  // May not spawn
    spawn.max_count = 1;
    spawn.respawn_condition = RespawnCondition::CUSTOM;
    spawn.respawn_time = std::chrono::duration_cast<std::chrono::seconds>(respawn_time);
    
    // [SEQUENCE: MVP7-128] Custom spawn condition based on chance
    spawn.spawn_condition = [spawn_chance]() {
        static std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return dist(rng) < spawn_chance;
    };
    
    return spawn;
}

// [SEQUENCE: MVP7-129] Calculate optimal spawn density
float SpawnDensityController::CalculateOptimalDensity(uint32_t map_id,
                                                     size_t player_count,
                                                     float map_size) {
    // Base density: 1 mob per 100 square units
    float base_density = map_size / 100.0f;
    
    // Scale with player count (more players = more mobs)
    float player_multiplier = 1.0f + (player_count * 0.1f);
    
    // Cap at reasonable limits
    float optimal_density = base_density * player_multiplier;
    return std::clamp(optimal_density, 0.5f, 3.0f);
}

} // namespace mmorpg::game::world