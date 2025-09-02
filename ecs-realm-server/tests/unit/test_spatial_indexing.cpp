#include <gtest/gtest.h>
#include "game/systems/spatial/spatial_grid.h"
#include "game/systems/spatial/spatial_octree.h"
#include "game/components/transform_component.h"
#include <random>
#include <chrono>

using namespace mmorpg::game::systems::spatial;
using namespace mmorpg::game::components;

// [SEQUENCE: MVP3-151] Spatial indexing system tests
class SpatialIndexingTest : public ::testing::Test {
protected:
    std::unique_ptr<SpatialGrid> grid;
    std::unique_ptr<SpatialOctree> octree;
    
    void SetUp() override {
        // Create spatial grid (2D)
        grid = std::make_unique<SpatialGrid>(1000.0f, 1000.0f, 10.0f);
        
        // Create octree (3D)
        octree = std::make_unique<SpatialOctree>(
            Vector3{0, 0, 0},    // Center
            Vector3{500, 100, 500} // Half size
        );
    }
    
    EntityId CreateEntity(float x, float y, float z) {
        static EntityId next_id = 1;
        return next_id++;
    }
};

// [SEQUENCE: MVP3-152] Grid insertion and query tests
TEST_F(SpatialIndexingTest, GridBasicOperations) {
    // Insert entities
    EntityId e1 = CreateEntity(10, 0, 10);
    EntityId e2 = CreateEntity(15, 0, 15);
    EntityId e3 = CreateEntity(100, 0, 100);
    
    grid->Insert(e1, {10, 0, 10});
    grid->Insert(e2, {15, 0, 15});
    grid->Insert(e3, {100, 0, 100});
    
    // Query nearby entities
    auto nearby = grid->QueryRadius({10, 0, 10}, 10.0f);
    
    // Should find e1 and e2 (within 10 units)
    EXPECT_EQ(nearby.size(), 2);
    EXPECT_TRUE(std::find(nearby.begin(), nearby.end(), e1) != nearby.end());
    EXPECT_TRUE(std::find(nearby.begin(), nearby.end(), e2) != nearby.end());
    EXPECT_TRUE(std::find(nearby.begin(), nearby.end(), e3) == nearby.end());
}

// [SEQUENCE: MVP3-153] Grid movement update tests
TEST_F(SpatialIndexingTest, GridMovementUpdate) {
    EntityId entity = CreateEntity(0, 0, 0);
    
    // Initial position
    grid->Insert(entity, {0, 0, 0});
    
    // Move entity
    grid->Update(entity, {0, 0, 0}, {50, 0, 50});
    
    // Old position shouldn't find entity
    auto old_nearby = grid->QueryRadius({0, 0, 0}, 5.0f);
    EXPECT_TRUE(std::find(old_nearby.begin(), old_nearby.end(), entity) == old_nearby.end());
    
    // New position should find entity
    auto new_nearby = grid->QueryRadius({50, 0, 50}, 5.0f);
    EXPECT_TRUE(std::find(new_nearby.begin(), new_nearby.end(), entity) != new_nearby.end());
}

// [SEQUENCE: MVP3-154] Grid boundary tests
TEST_F(SpatialIndexingTest, GridBoundaryHandling) {
    // Test entities at grid boundaries
    std::vector<Vector3> boundary_positions = {
        {0, 0, 0},           // Min corner
        {1000, 0, 1000},     // Max corner
        {500, 0, 0},         // Edge
        {0, 0, 500},         // Edge
        {-10, 0, -10},       // Outside (should clamp)
        {1010, 0, 1010}      // Outside (should clamp)
    };
    
    std::vector<EntityId> entities;
    for (size_t i = 0; i < boundary_positions.size(); ++i) {
        EntityId e = CreateEntity(0, 0, 0);
        entities.push_back(e);
        grid->Insert(e, boundary_positions[i]);
    }
    
    // All entities should be inserted without crashes
    for (size_t i = 0; i < entities.size(); ++i) {
        auto found = grid->QueryRadius(boundary_positions[i], 20.0f);
        EXPECT_FALSE(found.empty());
    }
}

// [SEQUENCE: MVP3-155] Octree insertion and query tests
TEST_F(SpatialIndexingTest, OctreeBasicOperations) {
    // Insert entities at various heights
    EntityId e1 = CreateEntity(0, 10, 0);
    EntityId e2 = CreateEntity(0, 50, 0);
    EntityId e3 = CreateEntity(0, 90, 0);
    
    octree->Insert(e1, {0, 10, 0});
    octree->Insert(e2, {0, 50, 0});
    octree->Insert(e3, {0, 90, 0});
    
    // Query at different heights
    auto low_query = octree->QueryRadius({0, 10, 0}, 20.0f);
    auto mid_query = octree->QueryRadius({0, 50, 0}, 20.0f);
    auto high_query = octree->QueryRadius({0, 90, 0}, 20.0f);
    
    // Each query should find only nearby entity
    EXPECT_EQ(low_query.size(), 1);
    EXPECT_EQ(low_query[0], e1);
    
    EXPECT_EQ(mid_query.size(), 1);
    EXPECT_EQ(mid_query[0], e2);
    
    EXPECT_EQ(high_query.size(), 1);
    EXPECT_EQ(high_query[0], e3);
}

// [SEQUENCE: MVP3-156] Octree subdivision tests
TEST_F(SpatialIndexingTest, OctreeSubdivision) {
    // Insert many entities to trigger subdivision
    std::vector<EntityId> entities;
    
    for (int i = 0; i < 100; ++i) {
        EntityId e = CreateEntity(0, 0, 0);
        entities.push_back(e);
        
        // Cluster entities in one octant to force subdivision
        float x = 10.0f + (i % 10);
        float y = 10.0f + (i / 10);
        float z = 10.0f;
        
        octree->Insert(e, {x, y, z});
    }
    
    // Query should still work correctly after subdivision
    auto results = octree->QueryRadius({15, 15, 10}, 20.0f);
    EXPECT_GT(results.size(), 50); // Should find many entities
}

// [SEQUENCE: MVP3-157] Performance comparison test
TEST_F(SpatialIndexingTest, PerformanceComparison) {
    const int ENTITY_COUNT = 1000;
    const int QUERY_COUNT = 100;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> pos_dist(0.0f, 500.0f);
    
    // Insert entities in both systems
    std::vector<EntityId> entities;
    std::vector<Vector3> positions;
    
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        EntityId e = CreateEntity(0, 0, 0);
        Vector3 pos = {pos_dist(gen), pos_dist(gen) * 0.2f, pos_dist(gen)};
        
        entities.push_back(e);
        positions.push_back(pos);
        
        grid->Insert(e, pos);
        octree->Insert(e, pos);
    }
    
    // Time grid queries
    auto grid_start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < QUERY_COUNT; ++i) {
        Vector3 query_pos = {pos_dist(gen), 0, pos_dist(gen)};
        grid->QueryRadius(query_pos, 50.0f);
    }
    
    auto grid_end = std::chrono::high_resolution_clock::now();
    auto grid_time = std::chrono::duration_cast<std::chrono::microseconds>(grid_end - grid_start);
    
    // Time octree queries
    auto octree_start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < QUERY_COUNT; ++i) {
        Vector3 query_pos = {pos_dist(gen), pos_dist(gen) * 0.2f, pos_dist(gen)};
        octree->QueryRadius(query_pos, 50.0f);
    }
    
    auto octree_end = std::chrono::high_resolution_clock::now();
    auto octree_time = std::chrono::duration_cast<std::chrono::microseconds>(octree_end - octree_start);
    
    // Both should be reasonably fast
    EXPECT_LT(grid_time.count(), 10000); // Less than 10ms for 100 queries
    EXPECT_LT(octree_time.count(), 10000);
    
    // Log performance for analysis
    std::cout << "Grid query time: " << grid_time.count() << " us\n";
    std::cout << "Octree query time: " << octree_time.count() << " us\n";
}

// [SEQUENCE: MVP3-158] Spatial query accuracy test
TEST_F(SpatialIndexingTest, QueryAccuracy) {
    // Create a known pattern of entities
    std::vector<std::pair<EntityId, Vector3>> test_entities;
    
    // Create a grid pattern
    for (int x = 0; x <= 100; x += 10) {
        for (int z = 0; z <= 100; z += 10) {
            EntityId e = CreateEntity(x, 0, z);
            Vector3 pos = {float(x), 0, float(z)};
            
            test_entities.push_back({e, pos});
            grid->Insert(e, pos);
        }
    }
    
    // Test query at center with specific radius
    Vector3 center = {50, 0, 50};
    float radius = 25.0f;
    
    auto results = grid->QueryRadius(center, radius);
    
    // Manually verify results
    int expected_count = 0;
    for (const auto& [entity, pos] : test_entities) {
        float dist = std::sqrt(
            (pos.x - center.x) * (pos.x - center.x) +
            (pos.z - center.z) * (pos.z - center.z)
        );
        
        if (dist <= radius) {
            expected_count++;
            // Entity should be in results
            EXPECT_TRUE(std::find(results.begin(), results.end(), entity) != results.end());
        }
    }
    
    EXPECT_EQ(results.size(), expected_count);
}

// [SEQUENCE: MVP3-159] Dynamic entity movement stress test
TEST_F(SpatialIndexingTest, DynamicMovementStress) {
    const int ENTITY_COUNT = 500;
    const int UPDATE_ITERATIONS = 100;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> pos_dist(0.0f, 500.0f);
    std::uniform_real_distribution<float> move_dist(-5.0f, 5.0f);
    
    // Create entities
    std::vector<EntityId> entities;
    std::vector<Vector3> positions;
    
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        EntityId e = CreateEntity(0, 0, 0);
        Vector3 pos = {pos_dist(gen), 0, pos_dist(gen)};
        
        entities.push_back(e);
        positions.push_back(pos);
        
        grid->Insert(e, pos);
    }
    
    // Simulate movement over time
    for (int iter = 0; iter < UPDATE_ITERATIONS; ++iter) {
        for (size_t i = 0; i < entities.size(); ++i) {
            Vector3 old_pos = positions[i];
            Vector3 new_pos = {
                old_pos.x + move_dist(gen),
                0,
                old_pos.z + move_dist(gen)
            };
            
            // Clamp to grid bounds
            new_pos.x = std::max(0.0f, std::min(500.0f, new_pos.x));
            new_pos.z = std::max(0.0f, std::min(500.0f, new_pos.z));
            
            grid->Update(entities[i], old_pos, new_pos);
            positions[i] = new_pos;
        }
        
        // Perform some queries to ensure consistency
        if (iter % 10 == 0) {
            Vector3 query_pos = {pos_dist(gen), 0, pos_dist(gen)};
            auto results = grid->QueryRadius(query_pos, 30.0f);
            
            // Verify all results are within radius
            for (EntityId e : results) {
                auto it = std::find(entities.begin(), entities.end(), e);
                ASSERT_TRUE(it != entities.end());
                
                size_t idx = std::distance(entities.begin(), it);
                Vector3 entity_pos = positions[idx];
                
                float dist = std::sqrt(
                    (entity_pos.x - query_pos.x) * (entity_pos.x - query_pos.x) +
                    (entity_pos.z - query_pos.z) * (entity_pos.z - query_pos.z)
                );
                
                EXPECT_LE(dist, 30.0f);
            }
        }
    }
}

// [SEQUENCE: MVP3-160] Region query tests
TEST_F(SpatialIndexingTest, RegionQueries) {
    // Create entities in known positions
    std::vector<std::pair<EntityId, Vector3>> entities;
    
    for (int x = 0; x <= 200; x += 20) {
        for (int z = 0; z <= 200; z += 20) {
            EntityId e = CreateEntity(x, 0, z);
            Vector3 pos = {float(x), 0, float(z)};
            
            entities.push_back({e, pos});
            grid->Insert(e, pos);
            octree->Insert(e, {float(x), 0, float(z)});
        }
    }
    
    // Test rectangular region query
    Vector3 min_bounds = {50, -10, 50};
    Vector3 max_bounds = {150, 10, 150};
    
    auto grid_results = grid->QueryRegion(min_bounds, max_bounds);
    auto octree_results = octree->QueryBox(min_bounds, max_bounds);
    
    // Both should return same entities
    EXPECT_EQ(grid_results.size(), octree_results.size());
    
    // Verify all results are within bounds
    for (EntityId e : grid_results) {
        auto it = std::find_if(entities.begin(), entities.end(),
            [e](const auto& pair) { return pair.first == e; });
        
        ASSERT_TRUE(it != entities.end());
        
        Vector3 pos = it->second;
        EXPECT_GE(pos.x, min_bounds.x);
        EXPECT_LE(pos.x, max_bounds.x);
        EXPECT_GE(pos.z, min_bounds.z);
        EXPECT_LE(pos.z, max_bounds.z);
    }
}
