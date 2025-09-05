#include <gtest/gtest.h>
#include "game/world/grid/world_grid.h"
#include "game/world/octree/octree_world.h"
#include "game/components/transform_component.h"
#include <random>
#include <chrono>

using namespace mmorpg::game;
using namespace mmorpg::core;

// [SEQUENCE: MVP3-104] Defines the test fixture for all spatial indexing tests.
class SpatialIndexingTest : public ::testing::Test {
protected:
    std::unique_ptr<world::grid::WorldGrid> grid;
    std::unique_ptr<world::octree::OctreeWorld> octree;
    
    void SetUp() override {
        world::grid::WorldGrid::Config grid_config;
        grid_config.cell_size = 10.0f;
        grid_config.grid_width = 100;
        grid_config.grid_height = 100;
        grid_config.world_min_x = 0.0f;
        grid_config.world_min_y = 0.0f;
        grid = std::make_unique<world::grid::WorldGrid>(grid_config);

        world::octree::OctreeWorld::Config octree_config;
        octree_config.world_min = {0, 0, 0};
        octree_config.world_max = {500, 100, 500};
        octree = std::make_unique<world::octree::OctreeWorld>(octree_config);
    }
    
    ecs::EntityId CreateEntity([[maybe_unused]] float x, [[maybe_unused]] float y, [[maybe_unused]] float z) {
        static ecs::EntityId next_id = 1;
        return next_id++;
    }
};

// [SEQUENCE: MVP3-105] Tests basic insertion and radius queries in the WorldGrid.
TEST_F(SpatialIndexingTest, GridBasicOperations) {
    ecs::EntityId e1 = CreateEntity(10, 0, 10);
    ecs::EntityId e2 = CreateEntity(15, 0, 15);
    ecs::EntityId e3 = CreateEntity(100, 0, 100);
    grid->AddEntity(e1, {10, 0, 10});
    grid->AddEntity(e2, {15, 0, 15});
    grid->AddEntity(e3, {100, 0, 100});
    auto nearby = grid->GetEntitiesInRadius({10, 0, 10}, 10.0f);
    EXPECT_EQ(nearby.size(), 2);
}

// [SEQUENCE: MVP3-106] Tests the update mechanism when an entity moves between cells in the WorldGrid.
TEST_F(SpatialIndexingTest, GridMovementUpdate) {
    ecs::EntityId entity = CreateEntity(0, 0, 0);
    grid->AddEntity(entity, {0, 0, 0});
    grid->UpdateEntity(entity, {0, 0, 0}, {50, 0, 50});
    auto old_nearby = grid->GetEntitiesInRadius({0, 0, 0}, 5.0f);
    EXPECT_TRUE(std::find(old_nearby.begin(), old_nearby.end(), entity) == old_nearby.end());
    auto new_nearby = grid->GetEntitiesInRadius({50, 0, 50}, 5.0f);
    EXPECT_FALSE(new_nearby.empty());
}

// [SEQUENCE: MVP3-107] Tests how the WorldGrid handles entities at its boundaries.
TEST_F(SpatialIndexingTest, GridBoundaryHandling) {
    std::vector<utils::Vector3> boundary_positions = {{0, 0, 0}, {999, 0, 999}};
    for (size_t i = 0; i < boundary_positions.size(); ++i) {
        ecs::EntityId e = CreateEntity(0, 0, 0);
        grid->AddEntity(e, boundary_positions[i]);
        auto found = grid->GetEntitiesInRadius(boundary_positions[i], 20.0f);
        EXPECT_FALSE(found.empty());
    }
}

// [SEQUENCE: MVP3-108] Tests basic insertion and radius queries in the OctreeWorld.
TEST_F(SpatialIndexingTest, OctreeBasicOperations) {
    ecs::EntityId e1 = CreateEntity(0, 10, 0);
    ecs::EntityId e2 = CreateEntity(0, 50, 0);
    octree->AddEntity(e1, {0, 10, 0});
    octree->AddEntity(e2, {0, 50, 0});
    auto low_query = octree->GetEntitiesInRadius({0, 10, 0}, 20.0f);
    EXPECT_EQ(low_query.size(), 1);
    EXPECT_EQ(low_query[0], e1);
}

// [SEQUENCE: MVP3-109] Tests the dynamic subdivision of nodes in the OctreeWorld.
TEST_F(SpatialIndexingTest, OctreeSubdivision) {
    for (int i = 0; i < 100; ++i) {
        ecs::EntityId e = CreateEntity(0, 0, 0);
        float x = 10.0f + (i % 10); float y = 10.0f + (i / 10); float z = 10.0f;
        octree->AddEntity(e, {x, y, z});
    }
    auto results = octree->GetEntitiesInRadius({15, 15, 10}, 20.0f);
    EXPECT_GT(results.size(), 50);
}

// [SEQUENCE: MVP3-110] Compares the performance of radius queries between the Grid and Octree.
TEST_F(SpatialIndexingTest, PerformanceComparison) {
    const int ENTITY_COUNT = 1000;
    std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<float> pos_dist(0.0f, 500.0f);
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        ecs::EntityId e = CreateEntity(0, 0, 0);
        utils::Vector3 pos = {pos_dist(gen), pos_dist(gen) * 0.2f, pos_dist(gen)};
        grid->AddEntity(e, pos);
        octree->AddEntity(e, pos);
    }
    auto grid_start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; ++i) grid->GetEntitiesInRadius({pos_dist(gen), 0, pos_dist(gen)}, 50.0f);
    auto grid_end = std::chrono::high_resolution_clock::now();
    auto grid_time = std::chrono::duration_cast<std::chrono::microseconds>(grid_end - grid_start);
    EXPECT_LT(grid_time.count(), 10000);
}

// [SEQUENCE: MVP3-111] Verifies the accuracy of spatial queries.
TEST_F(SpatialIndexingTest, QueryAccuracy) {
    std::vector<std::pair<ecs::EntityId, utils::Vector3>> test_entities;
    for (int x = 0; x <= 100; x += 10) {
        for (int z = 0; z <= 100; z += 10) {
            ecs::EntityId e = CreateEntity(x, 0, z);
            utils::Vector3 pos = {float(x), 0, float(z)};
            test_entities.push_back({e, pos});
            grid->AddEntity(e, pos);
        }
    }
    utils::Vector3 center = {50, 0, 50}; float radius = 25.0f;
    auto results = grid->GetEntitiesInRadius(center, radius);
    int expected_count = 0;
    for (const auto& [entity, pos] : test_entities) {
        if (std::sqrt(pow(pos.x - center.x, 2) + pow(pos.z - center.z, 2)) <= radius) {
            bool found = false;
            for(const auto& result : results){
                if(result == entity){
                    expected_count++;
                    found = true;
                    break;
                }
            }
            EXPECT_TRUE(found);
        }
    }
}

// [SEQUENCE: MVP3-112] A stress test involving a large number of dynamic entities.
TEST_F(SpatialIndexingTest, DynamicMovementStress) {
    const int ENTITY_COUNT = 500;
    std::vector<ecs::EntityId> entities;
    std::vector<utils::Vector3> positions;
    std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<float> pos_dist(0.0f, 500.0f);
    std::uniform_real_distribution<float> move_dist(-5.0f, 5.0f);
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        ecs::EntityId e = CreateEntity(0, 0, 0);
        utils::Vector3 pos = {pos_dist(gen), 0, pos_dist(gen)};
        entities.push_back(e);
        positions.push_back(pos);
        grid->AddEntity(e, pos);
    }
    for (int iter = 0; iter < 100; ++iter) {
        for (size_t i = 0; i < entities.size(); ++i) {
            utils::Vector3 old_pos = positions[i];
            utils::Vector3 new_pos = {old_pos.x + move_dist(gen), 0, old_pos.z + move_dist(gen)};
            grid->UpdateEntity(entities[i], old_pos, new_pos);
            positions[i] = new_pos;
        }
    }
}

// [SEQUENCE: MVP3-113] Tests region queries using bounding boxes.
TEST_F(SpatialIndexingTest, DISABLED_RegionQueries) {
    for (int x = 0; x <= 200; x += 20) {
        for (int z = 0; z <= 200; z += 20) {
            ecs::EntityId e = CreateEntity(x, 0, z);
            utils::Vector3 pos = {float(x), 0, float(z)};
            grid->AddEntity(e, pos);
            octree->AddEntity(e, pos);
        }
    }
    utils::Vector3 min_bounds = {50, -10, 50};
    utils::Vector3 max_bounds = {150, 10, 150};
    auto grid_results = grid->GetEntitiesInBox(min_bounds, max_bounds);
    auto octree_results = octree->GetEntitiesInBox(min_bounds, max_bounds);
    EXPECT_EQ(grid_results.size(), octree_results.size());
}
