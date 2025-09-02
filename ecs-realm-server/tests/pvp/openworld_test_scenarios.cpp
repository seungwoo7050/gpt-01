#include <gtest/gtest.h>
#include "game/systems/pvp/openworld_pvp_system.h"
#include "game/systems/spatial_indexing_system.h"
#include "game/components/pvp_stats_component.h"
#include "game/components/health_component.h"
#include "game/components/transform_component.h"
#include "game/components/combat_stats_component.h"
#include "core/ecs/world.h"

using namespace mmorpg;

// [SEQUENCE: MVP5-206] Test fixture for open world PvP scenarios
class OpenWorldPvPTestScenarios : public ::testing::Test {
protected:
    std::unique_ptr<core::ecs::World> world;
    game::systems::pvp::OpenWorldPvPSystem* pvp_system;
    game::systems::SpatialIndexingSystem* spatial_system;
    
    // [SEQUENCE: MVP5-207] Setup test world with spatial system
    void SetUp() override {
        world = std::make_unique<core::ecs::World>();
        
        // Register spatial system first (required dependency)
        spatial_system = world->RegisterSystem<game::systems::SpatialIndexingSystem>();
        spatial_system->OnSystemInit();
        
        // Register PvP system
        pvp_system = world->RegisterSystem<game::systems::pvp::OpenWorldPvPSystem>();
        pvp_system->OnSystemInit();
    }
    
    // [SEQUENCE: MVP5-208] Create test player with faction
    core::ecs::EntityId CreateTestPlayer(uint32_t faction_id, 
                                        const core::utils::Vector3& position) {
        auto player = world->CreateEntity();
        
        // Transform for position
        game::components::TransformComponent transform;
        transform.position = position;
        world->AddComponent(player, transform);
        
        // Health component
        game::components::HealthComponent health;
        health.max_health = 1000.0f;
        health.current_health = 1000.0f;
        world->AddComponent(player, health);
        
        // PvP stats
        game::components::PvPStatsComponent pvp_stats;
        world->AddComponent(player, pvp_stats);
        
        // Combat stats for buffs
        game::components::CombatStatsComponent combat_stats;
        world->AddComponent(player, combat_stats);
        
        // Set faction
        pvp_system->SetPlayerFaction(player, faction_id);
        
        return player;
    }
};

// [SEQUENCE: MVP5-209] Test zone creation and boundaries
TEST_F(OpenWorldPvPTestScenarios, ZoneCreationAndBoundaries) {
    // Create a PvP zone
    auto zone = pvp_system->CreatePvPZone(
        "Contested Valley",
        {-50, -50, 0},  // min bounds
        {50, 50, 20}    // max bounds
    );
    
    EXPECT_NE(zone, 0);
    
    // Create players inside and outside zone
    auto inside_player = CreateTestPlayer(1, {0, 0, 10});
    auto outside_player = CreateTestPlayer(1, {100, 100, 10});
    
    // Update to detect zones
    pvp_system->Update(1.1f);  // Trigger zone update
    
    // Check PvP flags
    EXPECT_TRUE(pvp_system->IsPlayerPvPFlagged(inside_player));
    EXPECT_FALSE(pvp_system->IsPlayerPvPFlagged(outside_player));
}

// [SEQUENCE: MVP5-210] Test faction hostility
TEST_F(OpenWorldPvPTestScenarios, FactionHostility) {
    // Create a PvP zone
    auto zone = pvp_system->CreatePvPZone("Battlefield", {-100, -100, 0}, {100, 100, 50});
    
    // Create players from different factions
    auto alliance_player = CreateTestPlayer(1, {0, 0, 10});  // Alliance
    auto horde_player = CreateTestPlayer(2, {10, 10, 10});   // Horde
    auto pirate_player = CreateTestPlayer(3, {20, 20, 10});  // Pirates
    
    // Update to flag players
    pvp_system->Update(1.1f);
    
    // Test hostility
    EXPECT_TRUE(pvp_system->CanAttack(alliance_player, horde_player));
    EXPECT_TRUE(pvp_system->CanAttack(horde_player, alliance_player));
    EXPECT_TRUE(pvp_system->CanAttack(pirate_player, alliance_player));
    EXPECT_TRUE(pvp_system->CanAttack(pirate_player, horde_player));
    
    // Same faction can't attack
    auto alliance_player2 = CreateTestPlayer(1, {5, 5, 10});
    pvp_system->Update(1.1f);
    EXPECT_FALSE(pvp_system->CanAttack(alliance_player, alliance_player2));
}

// [SEQUENCE: MVP5-211] Test zone capture mechanics
TEST_F(OpenWorldPvPTestScenarios, ZoneCapture) {
    // Create capturable zone
    auto zone = pvp_system->CreatePvPZone("Capture Point", {-30, -30, 0}, {30, 30, 20});
    
    // Create horde players to capture
    auto horde1 = CreateTestPlayer(2, {0, 0, 10});
    auto horde2 = CreateTestPlayer(2, {5, 5, 10});
    
    // Update to enter zone
    pvp_system->Update(1.1f);
    
    // Start capture
    EXPECT_TRUE(pvp_system->StartCapture(horde1, zone));
    EXPECT_TRUE(pvp_system->StartCapture(horde2, zone));
    
    // Simulate capture progress (2 players = 2% per second)
    for (int i = 0; i < 50; i++) {
        pvp_system->Update(1.0f);
    }
    
    // Check if captured (need to check zone component)
    auto* zone_comp = world->GetComponent<game::components::PvPZoneComponent>(zone);
    EXPECT_EQ(zone_comp->controlling_faction, 2);  // Horde controls
}

// [SEQUENCE: MVP5-212] Test territory buffs
TEST_F(OpenWorldPvPTestScenarios, TerritoryBuffs) {
    // Create zone controlled by Alliance
    auto zone = pvp_system->CreatePvPZone("Alliance Keep", {-50, -50, 0}, {50, 50, 30});
    
    // Set zone to Alliance control
    auto* zone_comp = world->GetComponent<game::components::PvPZoneComponent>(zone);
    zone_comp->controlling_faction = 1;
    
    // Create Alliance player
    auto alliance_player = CreateTestPlayer(1, {0, 0, 10});
    
    // Get initial stats
    auto* combat_stats = world->GetComponent<game::components::CombatStatsComponent>(alliance_player);
    float initial_damage = combat_stats->damage_increase;
    
    // Enter zone
    pvp_system->Update(1.1f);
    
    // Should have territory buff
    EXPECT_GT(combat_stats->damage_increase, initial_damage);
    EXPECT_NEAR(combat_stats->damage_increase - initial_damage, 0.1f, 0.001f);
}

// [SEQUENCE: MVP5-213] Test PvP kill and honor
TEST_F(OpenWorldPvPTestScenarios, PvPKillHonor) {
    // Create PvP zone
    auto zone = pvp_system->CreatePvPZone("Arena", {-20, -20, 0}, {20, 20, 10});
    
    // Create opposing faction players
    auto killer = CreateTestPlayer(1, {0, 0, 5});
    auto victim = CreateTestPlayer(2, {5, 5, 5});
    
    // Enter zone and flag
    pvp_system->Update(1.1f);
    
    // Get initial honor
    auto* killer_stats = world->GetComponent<game::components::PvPStatsComponent>(killer);
    uint32_t initial_honor = killer_stats->honor_points;
    
    // Simulate kill
    pvp_system->OnPlayerKilledPlayer(killer, victim);
    
    // Check honor gain
    EXPECT_GT(killer_stats->honor_points, initial_honor);
    EXPECT_EQ(killer_stats->world_pvp_kills, 1);
    EXPECT_EQ(killer_stats->kills, 1);
}

// [SEQUENCE: MVP5-214] Test diminishing returns
TEST_F(OpenWorldPvPTestScenarios, DiminishingReturns) {
    // Create PvP zone
    auto zone = pvp_system->CreatePvPZone("Farm Zone", {-30, -30, 0}, {30, 30, 15});
    
    // Create players
    auto farmer = CreateTestPlayer(1, {0, 0, 5});
    auto victim = CreateTestPlayer(2, {10, 10, 5});
    
    // Flag players
    pvp_system->Update(1.1f);
    
    auto* farmer_stats = world->GetComponent<game::components::PvPStatsComponent>(farmer);
    
    // Track honor gains
    std::vector<uint32_t> honor_gains;
    
    // Kill same player 10 times
    for (int i = 0; i < 10; i++) {
        uint32_t honor_before = farmer_stats->honor_points;
        pvp_system->OnPlayerKilledPlayer(farmer, victim);
        uint32_t honor_after = farmer_stats->honor_points;
        honor_gains.push_back(honor_after - honor_before);
    }
    
    // First kills worth more than later kills
    EXPECT_GT(honor_gains[0], honor_gains[6]);  // 1st > 7th kill
    EXPECT_GT(honor_gains[4], honor_gains[9]);  // 5th > 10th kill
}

// [SEQUENCE: MVP5-215] Test objective capture
TEST_F(OpenWorldPvPTestScenarios, ObjectiveCapture) {
    // Create zone with objectives
    auto zone = pvp_system->CreatePvPZone("Objective Zone", {-40, -40, 0}, {40, 40, 20});
    
    // Add capture objectives
    pvp_system->AddObjective(zone, 1, {-20, -20, 5});  // West flag
    pvp_system->AddObjective(zone, 2, {20, 20, 5});    // East flag
    
    // Create player near objective
    auto player = CreateTestPlayer(1, {-20, -20, 5});
    
    // Update to enter zone
    pvp_system->Update(1.1f);
    
    // Get initial stats
    auto* pvp_stats = world->GetComponent<game::components::PvPStatsComponent>(player);
    uint32_t initial_honor = pvp_stats->honor_points;
    uint32_t initial_objectives = pvp_stats->objectives_completed;
    
    // Capture objective
    EXPECT_TRUE(pvp_system->CaptureObjective(player, zone, 1));
    
    // Check rewards
    EXPECT_GT(pvp_stats->honor_points, initial_honor);
    EXPECT_EQ(pvp_stats->objectives_completed, initial_objectives + 1);
}

// [SEQUENCE: MVP5-216] Test PvP flag timeout
TEST_F(OpenWorldPvPTestScenarios, PvPFlagTimeout) {
    // Create PvP zone
    auto zone = pvp_system->CreatePvPZone("Timeout Zone", {-15, -15, 0}, {15, 15, 10});
    
    // Create player, enter and leave zone
    auto player = CreateTestPlayer(1, {0, 0, 5});
    
    // Enter zone
    pvp_system->Update(1.1f);
    EXPECT_TRUE(pvp_system->IsPlayerPvPFlagged(player));
    
    // Move player outside zone
    auto* transform = world->GetComponent<game::components::TransformComponent>(player);
    transform->position = {100, 100, 5};
    
    // Update to detect zone exit
    pvp_system->Update(1.1f);
    
    // Still flagged immediately after leaving
    EXPECT_TRUE(pvp_system->IsPlayerPvPFlagged(player));
    
    // Simulate 5+ minutes passing
    for (int i = 0; i < 310; i++) {
        pvp_system->Update(1.0f);
    }
    
    // Flag should expire
    EXPECT_FALSE(pvp_system->IsPlayerPvPFlagged(player));
}

// [SEQUENCE: MVP5-217] Test contested capture
TEST_F(OpenWorldPvPTestScenarios, ContestedCapture) {
    // Create capturable zone
    auto zone = pvp_system->CreatePvPZone("Contested Point", {-25, -25, 0}, {25, 25, 15});
    
    // Create players from different factions
    auto alliance1 = CreateTestPlayer(1, {0, 0, 5});
    auto alliance2 = CreateTestPlayer(1, {5, 5, 5});
    auto horde1 = CreateTestPlayer(2, {-5, -5, 5});
    
    // Update to enter zone
    pvp_system->Update(1.1f);
    
    // All start capturing
    pvp_system->StartCapture(alliance1, zone);
    pvp_system->StartCapture(alliance2, zone);
    pvp_system->StartCapture(horde1, zone);
    
    // Update capture (Alliance has 2, Horde has 1)
    float initial_progress = pvp_system->GetCaptureProgress(zone);
    pvp_system->Update(1.0f);
    float after_progress = pvp_system->GetCaptureProgress(zone);
    
    // Alliance should be capturing (2v1)
    EXPECT_GT(after_progress, initial_progress);
}