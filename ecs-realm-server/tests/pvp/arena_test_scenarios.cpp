#include <gtest/gtest.h>
#include "game/systems/pvp/arena_system.h"
#include "game/components/pvp_stats_component.h"
#include "game/components/health_component.h"
#include "game/components/transform_component.h"
#include "core/ecs/world.h"
#include <thread>

using namespace mmorpg;

// [SEQUENCE: MVP5-196] Test fixture for arena scenarios
class ArenaTestScenarios : public ::testing::Test {
protected:
    std::unique_ptr<core::ecs::World> world;
    game::systems::pvp::ArenaSystem* arena_system;
    
    // [SEQUENCE: MVP5-197] Setup test world
    void SetUp() override {
        world = std::make_unique<core::ecs::World>();
        arena_system = world->RegisterSystem<game::systems::pvp::ArenaSystem>();
        arena_system->OnSystemInit();
    }
    
    // [SEQUENCE: MVP5-198] Create test player with stats
    core::ecs::EntityId CreateTestPlayer(int32_t rating = 1500) {
        auto player = world->CreateEntity();
        
        // Add required components
        game::components::PvPStatsComponent pvp_stats;
        pvp_stats.rating = rating;
        pvp_stats.peak_rating = rating;
        world->AddComponent(player, pvp_stats);
        
        game::components::HealthComponent health;
        health.max_health = 1000.0f;
        health.current_health = 1000.0f;
        health.max_mana = 500.0f;
        health.current_mana = 500.0f;
        world->AddComponent(player, health);
        
        game::components::TransformComponent transform;
        transform.position = {0, 0, 0};
        world->AddComponent(player, transform);
        
        return player;
    }
};

// [SEQUENCE: MVP5-199] Test 1v1 matchmaking
TEST_F(ArenaTestScenarios, Arena1v1Matchmaking) {
    // Create two players with similar ratings
    auto player1 = CreateTestPlayer(1500);
    auto player2 = CreateTestPlayer(1520);
    
    // Queue both players
    EXPECT_TRUE(arena_system->QueueForArena(player1, game::components::MatchType::ARENA_1V1));
    EXPECT_TRUE(arena_system->QueueForArena(player2, game::components::MatchType::ARENA_1V1));
    
    // Simulate matchmaking update
    arena_system->Update(5.1f);  // Trigger matchmaking interval
    
    // Check if match was created
    auto matches = arena_system->GetActiveMatches();
    EXPECT_EQ(matches.size(), 1);
    
    // Verify players are in match
    auto player1_match = arena_system->GetPlayerMatch(player1);
    auto player2_match = arena_system->GetPlayerMatch(player2);
    EXPECT_EQ(player1_match, player2_match);
    EXPECT_NE(player1_match, 0);
}

// [SEQUENCE: MVP5-200] Test rating spread expansion
TEST_F(ArenaTestScenarios, RatingSpreadExpansion) {
    // Create players with large rating gap
    auto high_player = CreateTestPlayer(2000);
    auto low_player = CreateTestPlayer(1500);
    
    // Queue both players
    arena_system->QueueForArena(high_player, game::components::MatchType::ARENA_1V1);
    arena_system->QueueForArena(low_player, game::components::MatchType::ARENA_1V1);
    
    // Initial matchmaking shouldn't match them (500 rating gap)
    arena_system->Update(5.1f);
    EXPECT_EQ(arena_system->GetActiveMatches().size(), 0);
    
    // Simulate long queue time (35 seconds)
    for (int i = 0; i < 7; i++) {
        arena_system->Update(5.1f);
    }
    
    // Now they should match (spread expanded)
    auto matches = arena_system->GetActiveMatches();
    EXPECT_EQ(matches.size(), 1);
}

// [SEQUENCE: MVP5-201] Test 3v3 team formation
TEST_F(ArenaTestScenarios, Arena3v3TeamFormation) {
    // Create 6 players with varying ratings
    std::vector<core::ecs::EntityId> players;
    for (int i = 0; i < 6; i++) {
        players.push_back(CreateTestPlayer(1500 + i * 20));
        arena_system->QueueForArena(players[i], game::components::MatchType::ARENA_3V3);
    }
    
    // Trigger matchmaking
    arena_system->Update(5.1f);
    
    // Verify match created
    auto matches = arena_system->GetActiveMatches();
    EXPECT_EQ(matches.size(), 1);
    
    // Check team balance (alternating assignment)
    auto* match = world->GetComponent<game::components::MatchComponent>(matches[0]);
    ASSERT_NE(match, nullptr);
    EXPECT_EQ(match->teams[0].members.size(), 3);
    EXPECT_EQ(match->teams[1].members.size(), 3);
}

// [SEQUENCE: MVP5-202] Test match victory conditions
TEST_F(ArenaTestScenarios, MatchVictoryByElimination) {
    // Create and start a 1v1 match
    auto player1 = CreateTestPlayer(1500);
    auto player2 = CreateTestPlayer(1500);
    
    std::vector<core::ecs::EntityId> team1 = {player1};
    std::vector<core::ecs::EntityId> team2 = {player2};
    
    auto match_entity = arena_system->CreateMatch(
        game::components::MatchType::ARENA_1V1, team1, team2);
    
    arena_system->StartMatch(match_entity);
    
    // Simulate countdown
    for (int i = 0; i < 11; i++) {
        arena_system->Update(1.0f);
    }
    
    // Kill player2
    auto* health = world->GetComponent<game::components::HealthComponent>(player2);
    health->current_health = 0;
    health->is_dead = true;
    
    // Update to check victory
    arena_system->Update(0.1f);
    
    // Match should be ending
    auto* match = world->GetComponent<game::components::MatchComponent>(match_entity);
    EXPECT_EQ(match->state, game::components::MatchState::ENDING);
    EXPECT_EQ(match->winning_team_id, 1);  // Team 1 wins
}

// [SEQUENCE: MVP5-203] Test rating changes
TEST_F(ArenaTestScenarios, RatingCalculation) {
    // Create players with different ratings
    auto winner = CreateTestPlayer(1600);
    auto loser = CreateTestPlayer(1400);
    
    // Record initial ratings
    auto* winner_stats = world->GetComponent<game::components::PvPStatsComponent>(winner);
    auto* loser_stats = world->GetComponent<game::components::PvPStatsComponent>(loser);
    
    int32_t winner_initial = winner_stats->rating;
    int32_t loser_initial = loser_stats->rating;
    
    // Create and complete match
    std::vector<core::ecs::EntityId> team1 = {winner};
    std::vector<core::ecs::EntityId> team2 = {loser};
    
    auto match_entity = arena_system->CreateMatch(
        game::components::MatchType::ARENA_1V1, team1, team2);
    
    // Simulate match completion
    arena_system->EndMatch(match_entity, 1);  // Team 1 wins
    
    // Check rating changes
    EXPECT_GT(winner_stats->rating, winner_initial);  // Winner gains rating
    EXPECT_LT(loser_stats->rating, loser_initial);    // Loser loses rating
    
    // Higher rated player should gain less
    int32_t winner_gain = winner_stats->rating - winner_initial;
    EXPECT_LT(winner_gain, 32);  // Less than full K-factor
}

// [SEQUENCE: MVP5-204] Test leave queue
TEST_F(ArenaTestScenarios, LeaveQueueBehavior) {
    auto player = CreateTestPlayer(1500);
    
    // Queue and verify
    EXPECT_TRUE(arena_system->QueueForArena(player, game::components::MatchType::ARENA_2V2));
    
    auto* pvp_stats = world->GetComponent<game::components::PvPStatsComponent>(player);
    EXPECT_TRUE(pvp_stats->in_queue);
    
    // Leave queue
    EXPECT_TRUE(arena_system->LeaveQueue(player));
    EXPECT_FALSE(pvp_stats->in_queue);
    
    // Can't leave again
    EXPECT_FALSE(arena_system->LeaveQueue(player));
}

// [SEQUENCE: MVP5-205] Test concurrent matches
TEST_F(ArenaTestScenarios, ConcurrentMatches) {
    // Create 8 players for two 2v2 matches
    std::vector<core::ecs::EntityId> players;
    for (int i = 0; i < 8; i++) {
        players.push_back(CreateTestPlayer(1500));
        arena_system->QueueForArena(players[i], game::components::MatchType::ARENA_2V2);
    }
    
    // Trigger matchmaking
    arena_system->Update(5.1f);
    
    // Should create 2 matches
    auto matches = arena_system->GetActiveMatches();
    EXPECT_EQ(matches.size(), 2);
    
    // All players should be in matches
    for (auto player : players) {
        EXPECT_NE(arena_system->GetPlayerMatch(player), 0);
    }
}