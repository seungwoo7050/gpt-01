#pragma once

#include "core/ecs/optimized/system.h"
#include "game/components/guild_component.h"
#include "core/utils/vector3.h"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <chrono>

namespace mmorpg::game::systems::guild {

// [SEQUENCE: MVP5-29] Defines the system for managing seamless guild wars that occur in the main game world.
class GuildWarSeamlessSystem : public core::ecs::optimized::System {
public:
    GuildWarSeamlessSystem() = default;
    
    void Update(float delta_time) override;
    
    // [SEQUENCE: MVP5-30] Public API for managing seamless guild wars, territories, and combat.
    struct SeamlessWar {
        uint32_t war_id;
        uint32_t guild_a_id;
        uint32_t guild_b_id;
        
        enum class WarPhase {
            DECLARATION,    // 24 hour warning
            PREPARATION,    // 1 hour before start
            ACTIVE,        // War ongoing
            RESOLUTION     // Post-war cooldown
        };
        WarPhase phase = WarPhase::DECLARATION;
        
        std::chrono::steady_clock::time_point declaration_time;
        std::chrono::steady_clock::time_point war_start_time;
        std::chrono::steady_clock::time_point war_end_time;
        
        struct Territory {
            uint32_t territory_id;
            std::string name;
            core::utils::Vector3 center;
            float radius;
            uint32_t controlling_guild = 0;
            float control_percentage = 0.0f;
            uint32_t resource_value = 100;  // Resources per hour
        };
        std::vector<Territory> contested_territories;
        
        uint32_t guild_a_kills = 0;
        uint32_t guild_b_kills = 0;
        uint32_t guild_a_deaths = 0;
        uint32_t guild_b_deaths = 0;
        std::unordered_map<uint32_t, float> territory_control_time;
        
        std::unordered_set<core::ecs::EntityId> guild_a_participants;
        std::unordered_set<core::ecs::EntityId> guild_b_participants;
        std::unordered_map<core::ecs::EntityId, uint32_t> player_war_score;
    };
    
    bool DeclareSeamlessWar(uint32_t guild_a, uint32_t guild_b, 
                           const std::vector<uint32_t>& contested_territory_ids);
    bool RespondToWarDeclaration(uint32_t guild_id, uint32_t war_id, bool accept);
    
    void RegisterTerritory(uint32_t territory_id, const std::string& name,
                          const core::utils::Vector3& center, float radius);
    bool ClaimTerritory(uint32_t guild_id, uint32_t territory_id);
    uint32_t GetTerritoryController(uint32_t territory_id) const;
    
    bool IsGuildInWar(uint32_t guild_id) const;
    std::vector<uint32_t> GetActiveWars(uint32_t guild_id) const;
    const SeamlessWar* GetWarInfo(uint32_t war_id) const;
    
    bool IsInWarZone(core::ecs::EntityId player) const;
    bool CanAttackInWar(core::ecs::EntityId attacker, core::ecs::EntityId target) const;
    void OnWarKill(core::ecs::EntityId killer, core::ecs::EntityId victim);
    
    void UpdateTerritoryControl(uint32_t war_id, uint32_t territory_id);
    float GetTerritoryControlPercentage(uint32_t war_id, uint32_t territory_id) const;
    
    struct TerritoryResources {
        uint32_t gold_per_hour = 1000;
        uint32_t materials_per_hour = 500;
        uint32_t honor_per_hour = 100;
    };
    void DistributeTerritoryResources();
    
private:
    // [SEQUENCE: MVP5-31] Private helper methods for internal war management.
    void UpdateWars(float delta_time);
    void UpdateWarPhases();
    void UpdateTerritoryBattles(float delta_time);
    void UpdatePlayerTerritories();
    void StartWar(SeamlessWar& war);
    void EndWar(SeamlessWar& war);
    void DetermineWarVictor(const SeamlessWar& war);
    void DistributeWarRewards(const SeamlessWar& war);
    bool IsPlayerInTerritory(core::ecs::EntityId player, uint32_t territory_id) const;
    void UpdatePlayerWarParticipation(core::ecs::EntityId player, uint32_t war_id);
    void NotifyGuildMembers(uint32_t guild_id, const std::string& message);

    // [SEQUENCE: MVP5-32] Private member variables for system state and configuration.
    uint32_t next_war_id_ = 1;
    std::unordered_map<uint32_t, std::unique_ptr<SeamlessWar>> active_wars_;
    std::unordered_map<uint32_t, std::vector<uint32_t>> guild_wars_;  // guild_id -> war_ids
    
    struct TerritoryInfo {
        uint32_t territory_id;
        std::string name;
        core::utils::Vector3 center;
        float radius;
        uint32_t current_owner = 0;
        TerritoryResources resources;
        std::unordered_set<uint32_t> claimed_by_guilds;
    };
    std::unordered_map<uint32_t, TerritoryInfo> territories_;
    
    std::unordered_map<core::ecs::EntityId, uint32_t> player_in_territory_;
    std::unordered_map<uint32_t, std::unordered_set<core::ecs::EntityId>> territory_players_;
    
    struct SeamlessWarConfig {
        // Timing
        float declaration_duration = 86400.0f;  // 24 hours
        float preparation_duration = 3600.0f;   // 1 hour
        float war_duration = 10800.0f;          // 3 hours
        float resolution_duration = 3600.0f;    // 1 hour cooldown
        
        // Territory control
        float capture_rate = 1.0f;              // % per second per player
        float max_capture_rate = 10.0f;         // Max % per second
        uint32_t min_players_to_capture = 5;    // Minimum to start capture
        
        // Scoring
        uint32_t points_per_kill = 1;
        uint32_t points_per_territory_minute = 10;
        float territory_control_threshold = 60.0f;  // % needed to control
        
        // Limits
        uint32_t max_concurrent_wars = 3;       // Per guild
        uint32_t max_territories_per_war = 5;   // Limit scope
    } config_;
    
    struct WarStatistics {
        uint32_t total_wars_declared = 0;
        uint32_t wars_completed = 0;
        uint32_t territories_changed_hands = 0;
        std::unordered_map<uint32_t, uint32_t> guild_war_victories;
        std::unordered_map<uint32_t, uint32_t> guild_territories_owned;
    } stats_;
};

} // namespace mmorpg::game::systems::guild