#include <gtest/gtest.h>
#include "core/ecs/entity.h"
#include "core/ecs/component.h"
#include "core/ecs/world.h"
#include "game/components/transform_component.h"
#include "game/components/health_component.h"

using namespace mmorpg::core::ecs;
using namespace mmorpg::game::components;

// [SEQUENCE: MVP2-58] Basic ECS functionality tests
class ECSSystemTest : public ::testing::Test {
protected:
    std::unique_ptr<World> world;
    
    void SetUp() override {
        world = std::make_unique<World>();
    }
    
    void TearDown() override {
        world.reset();
    }
};

// [SEQUENCE: MVP2-59] Entity creation and destruction tests
TEST_F(ECSSystemTest, EntityCreationAndDestruction) {
    // Create entity
    EntityId entity = world->CreateEntity();
    EXPECT_TRUE(world->IsValid(entity));
    
    // Create multiple entities
    std::vector<EntityId> entities;
    for (int i = 0; i < 1000; ++i) {
        entities.push_back(world->CreateEntity());
    }
    EXPECT_EQ(world->GetEntityCount(), 1001);
    
    // Destroy entity
    world->DestroyEntity(entity);
    EXPECT_FALSE(world->IsValid(entity));
    EXPECT_EQ(world->GetEntityCount(), 1000);
    
    // Destroy all entities
    for (auto e : entities) {
        world->DestroyEntity(e);
    }
    EXPECT_EQ(world->GetEntityCount(), 0);
}

// [SEQUENCE: MVP2-60] Component addition and removal tests
TEST_F(ECSSystemTest, ComponentManagement) {
    EntityId entity = world->CreateEntity();
    
    // Add transform component
    TransformComponent transform;
    transform.position = {100.0f, 200.0f, 300.0f};
    transform.rotation = {0.0f, 90.0f, 0.0f};
    transform.scale = {1.0f, 1.0f, 1.0f};
    
    world->AddComponent(entity, transform);
    EXPECT_TRUE(world->HasComponent<TransformComponent>(entity));
    
    // Get component
    auto* retrievedTransform = world->GetComponent<TransformComponent>(entity);
    ASSERT_NE(retrievedTransform, nullptr);
    EXPECT_FLOAT_EQ(retrievedTransform->position.x, 100.0f);
    EXPECT_FLOAT_EQ(retrievedTransform->position.y, 200.0f);
    EXPECT_FLOAT_EQ(retrievedTransform->position.z, 300.0f);
    
    // Add health component
    HealthComponent health;
    health.current_health = 100.0f;
    health.max_health = 100.0f;
    
    world->AddComponent(entity, health);
    EXPECT_TRUE(world->HasComponent<HealthComponent>(entity));
    
    // Entity should have both components
    EXPECT_TRUE(world->HasComponent<TransformComponent>(entity));
    EXPECT_TRUE(world->HasComponent<HealthComponent>(entity));
    
    // Remove transform component
    world->RemoveComponent<TransformComponent>(entity);
    EXPECT_FALSE(world->HasComponent<TransformComponent>(entity));
    EXPECT_TRUE(world->HasComponent<HealthComponent>(entity));
}

// [SEQUENCE: MVP2-61] Component query tests
TEST_F(ECSSystemTest, ComponentQueries) {
    // Create entities with different component combinations
    std::vector<EntityId> transformOnly;
    std::vector<EntityId> healthOnly;
    std::vector<EntityId> both;
    
    for (int i = 0; i < 10; ++i) {
        EntityId e1 = world->CreateEntity();
        world->AddComponent(e1, TransformComponent{{i, i, i}});
        transformOnly.push_back(e1);
        
        EntityId e2 = world->CreateEntity();
        world->AddComponent(e2, HealthComponent{100.0f, 100.0f});
        healthOnly.push_back(e2);
        
        EntityId e3 = world->CreateEntity();
        world->AddComponent(e3, TransformComponent{{i*10, i*10, i*10}});
        world->AddComponent(e3, HealthComponent{50.0f, 100.0f});
        both.push_back(e3);
    }
    
    // Query entities with transform
    auto withTransform = world->GetEntitiesWith<TransformComponent>();
    EXPECT_EQ(withTransform.size(), 20); // 10 transform-only + 10 both
    
    // Query entities with health
    auto withHealth = world->GetEntitiesWith<HealthComponent>();
    EXPECT_EQ(withHealth.size(), 20); // 10 health-only + 10 both
    
    // Query entities with both components
    auto withBoth = world->GetEntitiesWith<TransformComponent, HealthComponent>();
    EXPECT_EQ(withBoth.size(), 10); // Only entities with both
}

// [SEQUENCE: MVP2-62] System update order tests
TEST_F(ECSSystemTest, SystemUpdateOrder) {
    // Track update order
    std::vector<int> updateOrder;
    
    class TestSystem1 : public System {
    public:
        std::vector<int>* order;
        void Update(float deltaTime) override {
            order->push_back(1);
        }
    };
    
    class TestSystem2 : public System {
    public:
        std::vector<int>* order;
        void Update(float deltaTime) override {
            order->push_back(2);
        }
    };
    
    class TestSystem3 : public System {
    public:
        std::vector<int>* order;
        void Update(float deltaTime) override {
            order->push_back(3);
        }
    };
    
    // Register systems in specific order
    auto* sys1 = world->RegisterSystem<TestSystem1>();
    auto* sys2 = world->RegisterSystem<TestSystem2>();
    auto* sys3 = world->RegisterSystem<TestSystem3>();
    
    sys1->order = &updateOrder;
    sys2->order = &updateOrder;
    sys3->order = &updateOrder;
    
    // Update world
    world->Update(0.016f);
    
    // Verify update order
    ASSERT_EQ(updateOrder.size(), 3);
    EXPECT_EQ(updateOrder[0], 1);
    EXPECT_EQ(updateOrder[1], 2);
    EXPECT_EQ(updateOrder[2], 3);
}

// [SEQUENCE: MVP2-63] Performance stress test
TEST_F(ECSSystemTest, PerformanceStressTest) {
    const int ENTITY_COUNT = 10000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Create many entities
    std::vector<EntityId> entities;
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        EntityId e = world->CreateEntity();
        entities.push_back(e);
        
        // Add components to half of them
        if (i % 2 == 0) {
            world->AddComponent(e, TransformComponent{{i, i, i}});
            world->AddComponent(e, HealthComponent{100.0f, 100.0f});
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Should create 10k entities in reasonable time
    EXPECT_LT(duration.count(), 1000); // Less than 1 second
    
    // Test query performance
    start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 100; ++i) {
        auto result = world->GetEntitiesWith<TransformComponent, HealthComponent>();
        EXPECT_EQ(result.size(), ENTITY_COUNT / 2);
    }
    
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // 100 queries should be fast
    EXPECT_LT(duration.count(), 100); // Less than 100ms
}

// [SEQUENCE: MVP2-64] Component data integrity test
TEST_F(ECSSystemTest, ComponentDataIntegrity) {
    const int ENTITY_COUNT = 100;
    std::vector<EntityId> entities;
    
    // Create entities with unique data
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        EntityId e = world->CreateEntity();
        entities.push_back(e);
        
        TransformComponent transform;
        transform.position = {float(i), float(i * 2), float(i * 3)};
        transform.rotation = {float(i * 0.1f), float(i * 0.2f), float(i * 0.3f)};
        
        HealthComponent health;
        health.current_health = float(i);
        health.max_health = float(i * 2);
        
        world->AddComponent(e, transform);
        world->AddComponent(e, health);
    }
    
    // Verify all data is correct
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        auto* transform = world->GetComponent<TransformComponent>(entities[i]);
        auto* health = world->GetComponent<HealthComponent>(entities[i]);
        
        ASSERT_NE(transform, nullptr);
        ASSERT_NE(health, nullptr);
        
        EXPECT_FLOAT_EQ(transform->position.x, float(i));
        EXPECT_FLOAT_EQ(transform->position.y, float(i * 2));
        EXPECT_FLOAT_EQ(transform->position.z, float(i * 3));
        
        EXPECT_FLOAT_EQ(health->current_health, float(i));
        EXPECT_FLOAT_EQ(health->max_health, float(i * 2));
    }
}

// [SEQUENCE: MVP2-65] Entity recycling test
TEST_F(ECSSystemTest, EntityRecycling) {
    std::vector<EntityId> firstBatch;
    
    // Create and destroy entities
    for (int i = 0; i < 100; ++i) {
        EntityId e = world->CreateEntity();
        firstBatch.push_back(e);
    }
    
    for (auto e : firstBatch) {
        world->DestroyEntity(e);
    }
    
    // Create new entities - should reuse IDs
    std::vector<EntityId> secondBatch;
    for (int i = 0; i < 100; ++i) {
        EntityId e = world->CreateEntity();
        secondBatch.push_back(e);
    }
    
    // Old IDs should be invalid
    for (auto e : firstBatch) {
        EXPECT_FALSE(world->IsValid(e));
    }
    
    // New IDs should be valid
    for (auto e : secondBatch) {
        EXPECT_TRUE(world->IsValid(e));
    }
}
