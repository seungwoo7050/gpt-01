// [SEQUENCE: MVP2-20] Unit Tests (`tests/unit/`)
#include <gtest/gtest.h>
#include "core/ecs/optimized/optimized_world.h"
#include "core/ecs/optimized/system.h"
#include "core/ecs/types.h"
#include "game/components/transform_component.h"
#include "game/components/health_component.h"

// Use the correct namespaces
using namespace mmorpg::core::ecs;
using namespace mmorpg::core::ecs::optimized;
using namespace mmorpg::game::components;

// Test fixture for the OptimizedWorld
class ECSSystemTest : public ::testing::Test {
protected:
    std::unique_ptr<OptimizedWorld> world;
    
    void SetUp() override {
        world = std::make_unique<OptimizedWorld>();
    }
    
    void TearDown() override {
        world.reset();
    }
};

// [SEQUENCE: MVP2-32] Entity creation and destruction tests
TEST_F(ECSSystemTest, EntityCreationAndDestruction) {
    EntityId entity = world->CreateEntity();
    EXPECT_TRUE(world->IsValid(entity));
    std::vector<EntityId> entities;
    for (int i = 0; i < 1000; ++i) {
        entities.push_back(world->CreateEntity());
    }
    EXPECT_EQ(world->GetEntityCount(), 1001);
    world->DestroyEntity(entity);
    EXPECT_FALSE(world->IsValid(entity));
    EXPECT_EQ(world->GetEntityCount(), 1000);
    for (auto e : entities) {
        world->DestroyEntity(e);
    }
    EXPECT_EQ(world->GetEntityCount(), 0);
}

// [SEQUENCE: MVP2-33] Component addition and removal tests
TEST_F(ECSSystemTest, ComponentManagement) {
    EntityId entity = world->CreateEntity();
    TransformComponent transform;
    transform.position = {100.0f, 200.0f, 300.0f};
    world->AddComponent(entity, transform);
    EXPECT_TRUE(world->HasComponent<TransformComponent>(entity));
    auto& retrievedTransform = world->GetComponent<TransformComponent>(entity);
    ASSERT_NE(&retrievedTransform, nullptr);
    EXPECT_FLOAT_EQ(retrievedTransform.position.x, 100.0f);
    HealthComponent health;
    health.current_hp = 100.0f;
    world->AddComponent(entity, health);
    EXPECT_TRUE(world->HasComponent<HealthComponent>(entity));
    world->RemoveComponent<TransformComponent>(entity);
    EXPECT_FALSE(world->HasComponent<TransformComponent>(entity));
    EXPECT_TRUE(world->HasComponent<HealthComponent>(entity));
}

// [SEQUENCE: MVP2-34] Component query tests
TEST_F(ECSSystemTest, ComponentQueries) {
    for (int i = 0; i < 10; ++i) {
        EntityId e1 = world->CreateEntity();
        world->AddComponent(e1, TransformComponent{{(float)i, (float)i, (float)i}});
        EntityId e2 = world->CreateEntity();
        world->AddComponent(e2, HealthComponent{100.0f, 100.0f});
        EntityId e3 = world->CreateEntity();
        world->AddComponent(e3, TransformComponent{{(float)i*10, (float)i*10, (float)i*10}});
        world->AddComponent(e3, HealthComponent{50.0f, 100.0f});
    }
    auto withTransform = world->GetEntitiesWith<TransformComponent>();
    EXPECT_EQ(withTransform.size(), 20);
    auto withHealth = world->GetEntitiesWith<HealthComponent>();
    EXPECT_EQ(withHealth.size(), 20);
    auto withBoth = world->GetEntitiesWith<TransformComponent, HealthComponent>();
    EXPECT_EQ(withBoth.size(), 10);
}

// [SEQUENCE: MVP2-35] System update order tests
TEST_F(ECSSystemTest, SystemUpdateOrder) {
    std::vector<int> updateOrder;
    class TestSystem1 : public System { public: std::vector<int>* order; void Update([[maybe_unused]] float dt) override { order->push_back(1); } };
    class TestSystem2 : public System { public: std::vector<int>* order; void Update([[maybe_unused]] float dt) override { order->push_back(2); } };
    class TestSystem3 : public System { public: std::vector<int>* order; void Update([[maybe_unused]] float dt) override { order->push_back(3); } };
    auto sys1 = world->RegisterSystem<TestSystem1>(); sys1->order = &updateOrder;
    auto sys2 = world->RegisterSystem<TestSystem2>(); sys2->order = &updateOrder;
    auto sys3 = world->RegisterSystem<TestSystem3>(); sys3->order = &updateOrder;
    world->Update(0.016f);
    ASSERT_EQ(updateOrder.size(), 3);
    EXPECT_EQ(updateOrder[0], 1);
    EXPECT_EQ(updateOrder[1], 2);
    EXPECT_EQ(updateOrder[2], 3);
}

// [SEQUENCE: MVP2-36] Performance stress test
TEST_F(ECSSystemTest, PerformanceStressTest) {
    const int ENTITY_COUNT = 10000;
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        EntityId e = world->CreateEntity();
        if (i % 2 == 0) {
            world->AddComponent(e, TransformComponent{{(float)i, (float)i, (float)i}});
            world->AddComponent(e, HealthComponent{100.0f, 100.0f});
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(duration.count(), 1000);
}

// [SEQUENCE: MVP2-37] Component data integrity test
TEST_F(ECSSystemTest, ComponentDataIntegrity) {
    const int ENTITY_COUNT = 100;
    std::vector<EntityId> entities;
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        EntityId e = world->CreateEntity();
        entities.push_back(e);
        world->AddComponent(e, TransformComponent{{(float)i, (float)i*2, (float)i*3}});
        world->AddComponent(e, HealthComponent{(float)i, (float)i*2});
    }
    for (int i = 0; i < ENTITY_COUNT; ++i) {
        auto& transform = world->GetComponent<TransformComponent>(entities[i]);
        auto& health = world->GetComponent<HealthComponent>(entities[i]);
        EXPECT_FLOAT_EQ(transform.position.x, float(i));
        EXPECT_FLOAT_EQ(health.current_hp, float(i));
    }
}

// [SEQUENCE: MVP2-38] Entity recycling test
TEST_F(ECSSystemTest, EntityRecycling) {
    std::vector<EntityId> firstBatch;
    for (int i = 0; i < 100; ++i) firstBatch.push_back(world->CreateEntity());
    for (auto e : firstBatch) world->DestroyEntity(e);
    std::vector<EntityId> secondBatch;
    for (int i = 0; i < 100; ++i) secondBatch.push_back(world->CreateEntity());
    for (auto e : firstBatch) EXPECT_FALSE(world->IsValid(e));
    for (auto e : secondBatch) EXPECT_TRUE(world->IsValid(e));
}
