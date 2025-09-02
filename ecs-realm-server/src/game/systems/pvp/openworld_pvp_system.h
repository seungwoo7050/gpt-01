#pragma once

#include "core/ecs/system.h"
#include "game/components/pvp_stats_component.h"
#include "game/components/match_component.h"
#include "core/utils/vector3.h"
#include <memory>
#include <unordered_set>

namespace mmorpg::game::systems::pvp {

// [SEQUENCE: MVP5-119] Open world PvP with zones and objectives
class OpenWorldPvPSystem : public core::ecs::System {
public:
    OpenWorldPvPSystem() : System("OpenWorldPvPSystem") {}
    
    // [SEQUENCE: MVP5-120] System lifecycle
    void OnSystemInit() override;
    void OnSystemShutdown() override;
    
    // [SEQUENCE: MVP5-121] System update
    void Update(float delta_time) override;
    
    // [SEQUENCE: MVP5-122] System metadata
    core::ecs::SystemStage GetStage() const override {
        return core::ecs::SystemStage::UPDATE;
    }
    int GetPriority() const override { return 201; }
    
    // [SEQUENCE: MVP5-123] Zone management
    core::ecs::EntityId CreatePvPZone(const std::string& name,
                                     const core::utils::Vector3& min,
                                     const core::utils::Vector3& max);
    
    bool SetZonePvPEnabled(core::ecs::EntityId zone, bool enabled);
    core::ecs::EntityId GetPlayerZone(core::ecs::EntityId player) const;
    
    // [SEQUENCE: MVP5-124] PvP state queries
    bool IsPlayerPvPFlagged(core::ecs::EntityId player) const;
    bool CanAttack(core::ecs::EntityId attacker, core::ecs::EntityId target) const;
    std::vector<core::ecs::EntityId> GetPvPEnabledPlayers() const;
    
    // [SEQUENCE: MVP5-125] Faction warfare
    void SetPlayerFaction(core::ecs::EntityId player, uint32_t faction_id);
    uint32_t GetPlayerFaction(core::ecs::EntityId player) const;
    bool AreFactionsHostile(uint32_t faction1, uint32_t faction2) const;
    
    // [SEQUENCE: MVP5-126] Territory control
    bool StartCapture(core::ecs::EntityId player, core::ecs::EntityId zone);
    bool StopCapture(core::ecs::EntityId player, core::ecs::EntityId zone);
    float GetCaptureProgress(core::ecs::EntityId zone) const;
    
    // [SEQUENCE: MVP5-127] Objective management
    void AddObjective(core::ecs::EntityId zone, uint32_t objective_id,
                     const core::utils::Vector3& position);
    bool CaptureObjective(core::ecs::EntityId player, core::ecs::EntityId zone,
                         uint32_t objective_id);
    
    // [SEQUENCE: MVP5-128] Combat events
    void OnPlayerKilledPlayer(core::ecs::EntityId killer, core::ecs::EntityId victim);
    void OnPlayerAssist(core::ecs::EntityId assister, core::ecs::EntityId victim);
    
private:
    // [SEQUENCE: MVP5-129] PvP state component (internal)
    struct PvPStateComponent {
        bool pvp_flagged = false;
        uint32_t faction_id = 0;
        std::chrono::steady_clock::time_point flag_time;
        std::chrono::steady_clock::time_point last_pvp_action;
        core::ecs::EntityId current_zone = 0;
        std::unordered_set<core::ecs::EntityId> recent_attackers;
    };
    
    // [SEQUENCE: MVP5-130] Zone boundaries
    struct ZoneBounds {
        core::utils::Vector3 min;
        core::utils::Vector3 max;
    };
    
    // [SEQUENCE: MVP5-131] Player states
    std::unordered_map<core::ecs::EntityId, PvPStateComponent> player_states_;
    std::unordered_map<core::ecs::EntityId, ZoneBounds> zone_bounds_;
    
    // [SEQUENCE: MVP5-132] Active zones
    std::vector<core::ecs::EntityId> pvp_zones_;
    std::unordered_map<uint32_t, std::unordered_set<uint32_t>> hostile_factions_;
    
    // [SEQUENCE: MVP5-133] Zone updates
    void UpdatePlayerZones(float delta_time);
    void UpdateZoneCaptures(float delta_time);
    void UpdatePvPFlags(float delta_time);
    
    // [SEQUENCE: MVP5-134] Player zone tracking
    void OnPlayerEnterZone(core::ecs::EntityId player, core::ecs::EntityId zone);
    void OnPlayerLeaveZone(core::ecs::EntityId player, core::ecs::EntityId zone);
    bool IsPlayerInZone(core::ecs::EntityId player, core::ecs::EntityId zone) const;
    
    // [SEQUENCE: MVP5-135] Capture mechanics
    void UpdateCaptureProgress(core::ecs::EntityId zone, float delta_time);
    void OnZoneCaptured(core::ecs::EntityId zone, uint32_t faction_id);
    void OnObjectiveCaptured(core::ecs::EntityId zone, uint32_t objective_id,
                            uint32_t faction_id);
    
    // [SEQUENCE: MVP5-136] PvP rewards
    void GrantHonorKill(core::ecs::EntityId killer, core::ecs::EntityId victim);
    void GrantObjectiveReward(core::ecs::EntityId player, uint32_t objective_type);
    void UpdateKillStreak(core::ecs::EntityId player);
    
    // [SEQUENCE: MVP5-137] Configuration
    struct OpenWorldConfig {
        float pvp_flag_duration = 300.0f;      // 5 minutes
        float zone_update_interval = 1.0f;      // Check zones every second
        float capture_tick_rate = 1.0f;         // Progress per second
        float capture_radius_check = 20.0f;     // Must be within radius
        
        // Honor rewards
        uint32_t honor_per_kill = 50;
        uint32_t honor_per_assist = 25;
        uint32_t honor_per_objective = 100;
        uint32_t honor_diminishing_returns = 5; // Same target worth less
        
        // Faction bonuses
        float faction_damage_bonus = 0.05f;     // 5% damage vs hostile
        float territory_buff_bonus = 0.1f;      // 10% stats in owned territory
    } config_;
    
    // [SEQUENCE: MVP5-138] Statistics
    struct WorldPvPStats {
        uint32_t total_kills = 0;
        uint32_t zones_flipped = 0;
        uint32_t objectives_captured = 0;
        std::unordered_map<uint32_t, uint32_t> faction_kills;
        std::unordered_map<uint32_t, uint32_t> faction_territories;
    } stats_;
    
    // [SEQUENCE: MVP5-139] Spatial integration
    class GridSpatialSystem* spatial_system_ = nullptr;
    
    
    struct KillRecord {
        uint32_t kill_count = 0;
        std::chrono::steady_clock::time_point last_kill_time;
    };
    std::unordered_map<std::pair<core::ecs::EntityId, core::ecs::EntityId>, 
                      KillRecord, 
                      std::hash<std::pair<core::ecs::EntityId, core::ecs::EntityId>>> kill_history_;
};

} // namespace mmorpg::game::systems::pvpme::systems::pvptems::pvpme::systems::pvp