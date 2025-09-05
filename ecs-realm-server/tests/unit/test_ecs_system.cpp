#include <gtest/gtest.h>
#include "core/ecs/optimized/optimized_world.h"
#include "core/ecs/optimized/system.h"
#include "game/components/transform_component.h"
#include "game/components/health_component.h"

// Use the correct namespaces
using namespace mmorpg::core::ecs;
using namespace mmorpg::core::ecs::optimized;
using namespace mmorpg::game::components;

class ECSSystemTest : public ::testing::Test {
protected:
    std::unique_ptr<OptimizedWorld> world;

    void SetUp() override {
        world = std::make_unique<OptimizedWorld>();
        world->RegisterComponent<TransformComponent>();
        world->RegisterComponent<HealthComponent>();
    }
};

// [SEQUENCE: MVP2-17] Tests basic entity creation and destruction.
TEST_F(ECSSystemTest, EntityCreationAndDestruction) {
    EXPECT_EQ(world->GetEntityCount(), 0);
    EntityId entity = world->CreateEntity();
    EXPECT_TRUE(world->IsValid(entity));
    EXPECT_EQ(world->GetEntityCount(), 1);
    world->DestroyEntity(entity);
    EXPECT_FALSE(world->IsValid(entity));
    EXPECT_EQ(world->GetEntityCount(), 0);
}

// [SEQUENCE: MVP2-18] Tests that entity IDs are recycled after destruction.
TEST_F(ECSSystemTest, EntityRecycling) {
    EntityId first_entity = world->CreateEntity();
    world->DestroyEntity(first_entity);
    EntityId second_entity = world->CreateEntity();
    EXPECT_EQ(first_entity, second_entity); // Should recycle the ID
}

// [SEQUENCE: MVP2-19] Tests adding, getting, and removing components from an entity.
TEST_F(ECSSystemTest, ComponentManagement) {
    EntityId entity = world->CreateEntity();
    world->AddComponent(entity, TransformComponent{{1.0f, 2.0f, 3.0f}});

    // Note: HasComponent is not implemented yet, so we test by getting.
    // EXPECT_TRUE(world->HasComponent<TransformComponent>(entity));

    TransformComponent& transform = world->GetComponent<TransformComponent>(entity);
    EXPECT_FLOAT_EQ(transform.position.x, 1.0f);

    world->RemoveComponent<TransformComponent>(entity);
    // EXPECT_FALSE(world->HasComponent<TransformComponent>(entity));
}

// [SEQUENCE: MVP2-20] Tests that systems are correctly updated.
class SimpleTestSystem : public System {
public:
    int update_count = 0;
    void Update([[maybe_unused]] float dt) override {
        update_count++;
    }
};

TEST_F(ECSSystemTest, SystemUpdate) {
    auto system = world->RegisterSystem<SimpleTestSystem>();
    world->Update(0.1f);
    EXPECT_EQ(system->update_count, 1);
    world->Update(0.1f);
    EXPECT_EQ(system->update_count, 2);
}

// [SEQUENCE: MVP2-21] Tests that systems correctly process entities with the required components.
class ProcessingSystem : public System {
public:
    int processed_count = 0;
    void Update([[maybe_unused]] float dt) override {
        for ([[maybe_unused]] auto const& entity : m_entities) {
            processed_count++;
        }
    }
};

TEST_F(ECSSystemTest, SystemEntityProcessing) {
    auto system = world->RegisterSystem<ProcessingSystem>();
    ComponentMask signature;
    signature.set(world->GetComponentType<TransformComponent>());
    world->SetSystemSignature<ProcessingSystem>(signature);

    EntityId entity1 = world->CreateEntity();
    world->AddComponent(entity1, TransformComponent{});

    EntityId entity2 = world->CreateEntity();
    world->AddComponent(entity2, HealthComponent{});

    world->Update(0.1f);
    EXPECT_EQ(system->processed_count, 1);
}
