#pragma once

#include "core/ecs/optimized/system.h"
#include "game/components/guild_component.h"
#include "core/utils/vector3.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <chrono>

namespace mmorpg::game::systems::guild {

// [SEQUENCE: MVP5-7] Defines the system for managing instanced guild-vs-guild warfare.
class GuildWarInstancedSystem : public core::ecs::optimized::System {
public:
    GuildWarInstancedSystem() = default;
    
    void Update(float delta_time) override;
    
    // [SEQUENCE: MVP5-8] Public API for managing guild wars and player participation.
    struct GuildWarDeclaration {
        uint32_t attacker_guild_id;
        uint32_t defender_guild_id;
        std::chrono::steady_clock::time_point declaration_time;
        std::chrono::steady_clock::time_point war_start_time;
        bool accepted = false;
    };
    
    struct GuildWarInstance {
        uint64_t instance_id;
        uint32_t attacker_guild_id;
        uint32_t defender_guild_id;
        
        std::vector<core::ecs::EntityId> attacker_participants;
        std::vector<core::ecs::EntityId> defender_participants;
        std::unordered_map<core::ecs::EntityId, core::utils::Vector3> original_positions;
        
        struct Objective {
            uint32_t objective_id;
            std::string name;
            core::utils::Vector3 position;
            uint32_t controlling_guild = 0;
            float capture_progress = 0.0f;
            uint32_t point_value = 100;
        };
        std::vector<Objective> objectives;
        
        uint32_t attacker_score = 0;
        uint32_t defender_score = 0;
        uint32_t attacker_kills = 0;
        uint32_t defender_kills = 0;
        
        enum class InstanceState {
            PREPARING,      // Waiting for players
            ACTIVE,         // War in progress
            ENDING,         // Victory achieved
            CLEANUP         // Returning players
        };
        InstanceState state = InstanceState::PREPARING;
        
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point end_time;
        float remaining_time = 3600.0f; // 1 hour default
        
        std::string map_name = "guild_war_fortress";
        core::utils::Vector3 attacker_spawn{-500, 0, 0};
        core::utils::Vector3 defender_spawn{500, 0, 0};
    };
    
    bool DeclareWar(uint32_t attacker_guild_id, uint32_t defender_guild_id);
    bool AcceptWarDeclaration(uint32_t guild_id);
    bool DeclineWarDeclaration(uint32_t guild_id);
    
    uint64_t CreateWarInstance(uint32_t attacker_guild_id, uint32_t defender_guild_id);
    bool JoinWarInstance(core::ecs::EntityId player, uint64_t instance_id);
    bool LeaveWarInstance(core::ecs::EntityId player);
    
    bool IsGuildAtWar(uint32_t guild_id) const;
    uint64_t GetActiveWarInstance(uint32_t guild_id) const;
    std::vector<GuildWarDeclaration> GetPendingDeclarations(uint32_t guild_id) const;
    
    bool CaptureObjective(core::ecs::EntityId player, uint32_t objective_id);
    float GetObjectiveCaptureProgress(uint64_t instance_id, uint32_t objective_id) const;
    
    void OnPlayerKilledInWar(core::ecs::EntityId killer, core::ecs::EntityId victim);
    void OnObjectiveCaptured(uint64_t instance_id, uint32_t objective_id, uint32_t guild_id);
    
private:
    // [SEQUENCE: MVP5-9] Private helper methods for internal war management.
    void UpdateWarInstances(float delta_time);
    void UpdateInstanceState(GuildWarInstance& instance, float delta_time);
    void UpdateObjectiveCapture(GuildWarInstance& instance, float delta_time);
    void TeleportPlayerToInstance(core::ecs::EntityId player, GuildWarInstance& instance);
    void ReturnPlayerFromInstance(core::ecs::EntityId player, GuildWarInstance& instance);
    void GrantWarRewards(const GuildWarInstance& instance);
    bool CheckVictoryConditions(GuildWarInstance& instance);
    void EndWarInstance(GuildWarInstance& instance, uint32_t winner_guild_id);

    // [SEQUENCE: MVP5-10] Private member variables for system state and configuration.
    std::unordered_map<uint32_t, std::vector<GuildWarDeclaration>> pending_declarations_;
    std::unordered_map<uint64_t, std::unique_ptr<GuildWarInstance>> active_instances_;
    std::unordered_map<uint32_t, uint64_t> guild_to_instance_;
    std::unordered_map<core::ecs::EntityId, uint64_t> player_to_instance_;
    
    uint64_t next_instance_id_ = 1;

    struct InstancedWarConfig {
        uint32_t min_participants = 20;        // Minimum per side
        uint32_t max_participants = 100;       // Maximum per side
        float declaration_expire_time = 3600;  // 1 hour to accept
        float preparation_time = 300;          // 5 minutes to gather
        float war_duration = 3600;             // 1 hour max
        uint32_t score_limit = 1000;           // First to reach wins
        
        // Scoring
        uint32_t points_per_kill = 10;
        uint32_t points_per_objective_tick = 5;  // Per second held
        float objective_tick_rate = 1.0f;
    } config_;
    
    struct WarStatistics {
        uint32_t total_wars = 0;
        uint32_t active_wars = 0;
        uint32_t largest_war_size = 0;
        std::unordered_map<uint32_t, uint32_t> guild_victories;
    } stats_;
};

} // namespace mmorpg::game::systems::guild