#include <gtest/gtest.h>
#include "game/systems/pvp/arena_system.h"
#include "game/systems/pvp/openworld_pvp_system.h"
#include "game/systems/spatial_indexing_system.h"
#include "game/components/pvp_stats_component.h"
#include "core/ecs/world.h"

using namespace mmorpg;

// [SEQUENCE: MVP5-218] Integration test for both PvP systems
class PvPIntegrationTest : public ::testing::Test {
protected:
    std::unique_ptr<core::ecs::World> world;
    game::systems::pvp::ArenaSystem* arena_system;
    game::systems::pvp::OpenWorldPvPSystem* openworld_system;
    
    // [SEQUENCE: MVP5-219] Setup both systems
    void SetUp() override {
        world = std::make_unique<core::ecs::World>();
        
        // Register systems
        auto* spatial = world->RegisterSystem<game::systems::SpatialIndexingSystem>();
        spatial->OnSystemInit();
        
        arena_system = world->RegisterSystem<game::systems::pvp::ArenaSystem>();
        arena_system->OnSystemInit();
        
        openworld_system = world->RegisterSystem<game::systems::pvp::OpenWorldPvPSystem>();
        openworld_system->OnSystemInit();
    }
    
    // [SEQUENCE: MVP5-220] Create player with all components
    core::ecs::EntityId CreateFullPlayer(uint32_t faction = 1, int32_t rating = 1500) {
        auto player = world->CreateEntity();
        
        // All required components
        game::components::TransformComponent transform;
        transform.position = {0, 0, 0};
        world->AddComponent(player, transform);
        
        game::components::HealthComponent health;
        health.max_health = 1000.0f;
        health.current_health = 1000.0f;
        world->AddComponent(player, health);
        
        game::components::PvPStatsComponent pvp_stats;
        pvp_stats.rating = rating;
        pvp_stats.peak_rating = rating;
        world->AddComponent(player, pvp_stats);
        
        game::components::CombatStatsComponent combat_stats;
        world->AddComponent(player, combat_stats);
        
        // Set faction for open world
        openworld_system->SetPlayerFaction(player, faction);
        
        return player;
    }
};

// [SEQUENCE: MVP5-221] Test shared PvP stats
TEST_F(PvPIntegrationTest, SharedPvPStats) {
    auto player = CreateFullPlayer();
    
    // Queue for arena
    arena_system->QueueForArena(player, game::components::MatchType::ARENA_1V1);
    
    // Enter PvP zone
    auto zone = openworld_system->CreatePvPZone("Mixed Zone", {-50, -50, 0}, {50, 50, 20});
    openworld_system->Update(1.1f);
    
    // Get PvP stats - should be same component
    auto* pvp_stats = world->GetComponent<game::components::PvPStatsComponent>(player);
    
    // Stats should track both
    EXPECT_TRUE(pvp_stats->in_queue);  // Arena queue
    EXPECT_TRUE(openworld_system->IsPlayerPvPFlagged(player));  // World PvP
}

// [SEQUENCE: MVP5-222] Test honor accumulation from both systems
TEST_F(PvPIntegrationTest, CombinedHonorSystem) {
    // Create players
    auto player1 = CreateFullPlayer(1, 1500);
    auto player2 = CreateFullPlayer(2, 1500);
    
    auto* pvp_stats = world->GetComponent<game::components::PvPStatsComponent>(player1);
    uint32_t initial_honor = pvp_stats->honor_points;
    
    // Earn honor from arena win
    std::vector<core::ecs::EntityId> team1 = {player1};
    std::vector<core::ecs::EntityId> team2 = {player2};
    
    auto match = arena_system->CreateMatch(
        game::components::MatchType::ARENA_1V1, team1, team2);
    arena_system->EndMatch(match, 1);  // Team 1 wins
    
    uint32_t arena_honor = pvp_stats->honor_points - initial_honor;
    EXPECT_GT(arena_honor, 0);
    
    // Earn honor from world PvP
    auto zone = openworld_system->CreatePvPZone("Honor Zone", {-30, -30, 0}, {30, 30, 15});
    openworld_system->Update(1.1f);
    
    uint32_t before_kill = pvp_stats->honor_points;
    openworld_system->OnPlayerKilledPlayer(player1, player2);
    
    uint32_t world_honor = pvp_stats->honor_points - before_kill;
    EXPECT_GT(world_honor, 0);
    
    // Total honor from both systems
    EXPECT_EQ(pvp_stats->honor_points, initial_honor + arena_honor + world_honor);
}

// [SEQUENCE: MVP5-223] Test player state transitions
TEST_F(PvPIntegrationTest, PlayerStateTransitions) {
    auto player = CreateFullPlayer();
    
    // Start in world PvP
    auto zone = openworld_system->CreatePvPZone("State Zone", {-20, -20, 0}, {20, 20, 10});
    openworld_system->Update(1.1f);
    EXPECT_TRUE(openworld_system->IsPlayerPvPFlagged(player));
    
    // Queue for arena while in PvP zone
    EXPECT_TRUE(arena_system->QueueForArena(player, game::components::MatchType::ARENA_2V2));
    
    // Still flagged for world PvP
    EXPECT_TRUE(openworld_system->IsPlayerPvPFlagged(player));
    
    // Can leave queue
    EXPECT_TRUE(arena_system->LeaveQueue(player));
}

// [SEQUENCE: MVP5-224] Test kill/death tracking across systems
TEST_F(PvPIntegrationTest, UnifiedKillTracking) {
    auto killer = CreateFullPlayer(1);
    auto victim1 = CreateFullPlayer(2);
    auto victim2 = CreateFullPlayer(2);
    
    auto* killer_stats = world->GetComponent<game::components::PvPStatsComponent>(killer);
    
    // Arena kill
    arena_system->OnPlayerDeath(victim1, killer);
    EXPECT_EQ(killer_stats->kills, 1);
    
    // World PvP kill
    auto zone = openworld_system->CreatePvPZone("Kill Zone", {-15, -15, 0}, {15, 15, 8});
    openworld_system->Update(1.1f);
    openworld_system->OnPlayerKilledPlayer(killer, victim2);
    
    // Total kills from both systems
    EXPECT_EQ(killer_stats->kills, 2);
    EXPECT_EQ(killer_stats->world_pvp_kills, 1);  // Specific to world PvP
}

// [SEQUENCE: MVP5-225] Test rating impacts
TEST_F(PvPIntegrationTest, RatingSystemIntegration) {
    // High rated player
    auto veteran = CreateFullPlayer(1, 2000);
    auto* vet_stats = world->GetComponent<game::components::PvPStatsComponent>(veteran);
    
    // Rating affects arena matchmaking
    auto newbie = CreateFullPlayer(1, 1000);
    
    arena_system->QueueForArena(veteran, game::components::MatchType::ARENA_1V1);
    arena_system->QueueForArena(newbie, game::components::MatchType::ARENA_1V1);
    
    // Shouldn't match immediately (1000 rating difference)
    arena_system->Update(5.1f);
    EXPECT_EQ(arena_system->GetActiveMatches().size(), 0);
    
    // Rating visible in world PvP (for future features like titles)
    EXPECT_EQ(vet_stats->rating, 2000);
    EXPECT_EQ(vet_stats->peak_rating, 2000);
}

// [SEQUENCE: MVP5-226] Performance test with many players
TEST_F(PvPIntegrationTest, ScalabilityTest) {
    // Create PvP zone
    auto zone = openworld_system->CreatePvPZone("Large Battle", {-100, -100, 0}, {100, 100, 50});
    
    // Create 100 players
    std::vector<core::ecs::EntityId> players;
    for (int i = 0; i < 100; i++) {
        uint32_t faction = (i % 2) + 1;  // Alternate factions
        auto player = CreateFullPlayer(faction, 1500 + (i * 5));
        
        // Position in zone
        auto* transform = world->GetComponent<game::components::TransformComponent>(player);
        transform->position = {
            static_cast<float>(i % 10 * 10 - 50),
            static_cast<float>(i / 10 * 10 - 50),
            5.0f
        };
        
        players.push_back(player);
    }
    
    // Measure update time
    auto start = std::chrono::high_resolution_clock::now();
    
    // Update both systems
    openworld_system->Update(1.0f);
    arena_system->Update(1.0f);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should handle 100 players efficiently
    EXPECT_LT(duration.count(), 50);  // Less than 50ms
    
    // All players should be flagged
    for (auto player : players) {
        EXPECT_TRUE(openworld_system->IsPlayerPvPFlagged(player));
    }
}

// [SEQUENCE: MVP5-227] Test system priorities
TEST_F(PvPIntegrationTest, SystemUpdateOrder) {
    // Verify update priorities
    EXPECT_LT(arena_system->GetPriority(), openworld_system->GetPriority());
    
    // Both are in UPDATE stage
    EXPECT_EQ(arena_system->GetStage(), core::ecs::SystemStage::UPDATE);
    EXPECT_EQ(openworld_system->GetStage(), core::ecs::SystemStage::UPDATE);
}