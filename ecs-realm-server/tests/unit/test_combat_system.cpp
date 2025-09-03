#include <gtest/gtest.h>
#include "core/ecs/optimized/optimized_world.h"
#include "game/systems/combat/targeted_combat_system.h"
#include "game/systems/combat/action_combat_system.h"
#include "game/systems/grid_spatial_system.h"
#include "game/components/health_component.h"
#include "game/components/combat_stats_component.h"
#include "game/components/skill_component.h"
#include "game/components/target_component.h"
#include "game/components/transform_component.h"
#include "game/components/projectile_component.h"
#include "game/components/dodge_component.h"

using namespace mmorpg::game::systems::combat;
using namespace mmorpg::game::components;
using namespace mmorpg::core::ecs::optimized;
using namespace mmorpg::core::ecs;

// [SEQUENCE: MVP4-16]
class CombatSystemTest : public ::testing::Test {
protected:
    std::unique_ptr<OptimizedWorld> world;
    std::shared_ptr<TargetedCombatSystem> targeted_combat_system;
    std::shared_ptr<ActionCombatSystem> action_combat_system;

    void SetUp() override {
        world = std::make_unique<OptimizedWorld>();

        world->RegisterComponent<HealthComponent>();
        world->RegisterComponent<CombatStatsComponent>();
        world->RegisterComponent<SkillComponent>();
        world->RegisterComponent<TargetComponent>();
        world->RegisterComponent<TransformComponent>();
        world->RegisterComponent<ProjectileComponent>();
        world->RegisterComponent<DodgeComponent>();

        targeted_combat_system = world->RegisterSystem<TargetedCombatSystem>();
        action_combat_system = world->RegisterSystem<ActionCombatSystem>();
        auto spatial_system = world->RegisterSystem<mmorpg::game::systems::GridSpatialSystem>();

        {
            ComponentMask signature;
            signature.set(world->GetComponentType<TargetComponent>());
            signature.set(world->GetComponentType<CombatStatsComponent>());
            world->SetSystemSignature<TargetedCombatSystem>(signature);
        }
        {
            ComponentMask signature;
            signature.set(world->GetComponentType<CombatStatsComponent>());
            world->SetSystemSignature<ActionCombatSystem>(signature);
        }
        {
            ComponentMask signature;
            signature.set(world->GetComponentType<TransformComponent>());
            world->SetSystemSignature<mmorpg::game::systems::GridSpatialSystem>(signature);
        }

        targeted_combat_system->SetSpatialSystem(spatial_system.get());
        action_combat_system->SetSpatialSystem(spatial_system.get());
    }

    EntityId CreateCombatEntity(float health, float attack, float defense) {
        EntityId entity = world->CreateEntity();
        world->AddComponent(entity, HealthComponent{health, health});
        world->AddComponent(entity, CombatStatsComponent{attack, 10.0f, 0.1f, 1.5f, 1.0f, 1.0f, defense});
        world->AddComponent(entity, SkillComponent{});
        world->AddComponent(entity, TargetComponent{});
        world->AddComponent(entity, TransformComponent{});
        return entity;
    }
};

// [SEQUENCE: MVP4-17]
TEST_F(CombatSystemTest, BasicDamage) {
    EntityId attacker = CreateCombatEntity(100, 50, 20);
    EntityId defender = CreateCombatEntity(100, 30, 30);

    auto& target_comp = world->GetComponent<TargetComponent>(attacker);
    target_comp.current_target = defender;
    target_comp.auto_attacking = true;

    targeted_combat_system->Update(0.1f);

    auto& defender_health = world->GetComponent<HealthComponent>(defender);
    EXPECT_LT(defender_health.current_hp, defender_health.max_hp);
}

// [SEQUENCE: MVP4-18]
TEST_F(CombatSystemTest, CriticalHitDamage) {
    EntityId attacker = CreateCombatEntity(100, 50, 20);
    EntityId defender = CreateCombatEntity(100, 30, 30);

    auto& attacker_stats = world->GetComponent<CombatStatsComponent>(attacker);
    attacker_stats.critical_chance = 1.0f;
    attacker_stats.critical_damage = 2.0f;

    auto& target_comp = world->GetComponent<TargetComponent>(attacker);
    target_comp.current_target = defender;
    target_comp.auto_attacking = true;

    targeted_combat_system->Update(0.1f);

    auto& defender_health = world->GetComponent<HealthComponent>(defender);
    EXPECT_LT(defender_health.current_hp, 85);
}

// [SEQUENCE: MVP4-19]
TEST_F(CombatSystemTest, CombatExecution) {
    EntityId attacker = CreateCombatEntity(100, 50, 20);
    EntityId defender = CreateCombatEntity(100, 30, 30);

    auto& target_comp = world->GetComponent<TargetComponent>(attacker);
    target_comp.current_target = defender;
    target_comp.auto_attacking = true;

    for (int i = 0; i < 5; ++i) {
        world->Update(0.1f);
    }

    auto& defender_health = world->GetComponent<HealthComponent>(defender);
    EXPECT_LT(defender_health.current_hp, defender_health.max_hp);
}

// [SEQUENCE: MVP4-20]
TEST_F(CombatSystemTest, DeathHandling) {
    EntityId attacker = CreateCombatEntity(100, 50, 20);
    EntityId defender = CreateCombatEntity(10, 30, 30);

    auto& target_comp = world->GetComponent<TargetComponent>(attacker);
    target_comp.current_target = defender;
    target_comp.auto_attacking = true;

    for (int i = 0; i < 10; ++i) {
        world->Update(0.1f);
    }

    auto& defender_health = world->GetComponent<HealthComponent>(defender);
    EXPECT_TRUE(defender_health.is_dead);
}

// [SEQUENCE: MVP4-21]
TEST_F(CombatSystemTest, SkillCooldown) {
    EntityId entity = CreateCombatEntity(100, 50, 20);
    auto& skill_comp = world->GetComponent<SkillComponent>(entity);

    Skill test_skill;
    test_skill.id = 1;
    test_skill.cooldown = 1.0f;
    skill_comp.skills[test_skill.id] = test_skill;

    targeted_combat_system->UseSkill(entity, test_skill.id);

    EXPECT_TRUE(skill_comp.skills[test_skill.id].on_cooldown);

    world->Update(0.5f);
    EXPECT_TRUE(skill_comp.skills[test_skill.id].on_cooldown);

    world->Update(0.6f);
    EXPECT_FALSE(skill_comp.skills[test_skill.id].on_cooldown);
}

// [SEQUENCE: MVP4-22]
TEST_F(CombatSystemTest, CombatStateManagement) {
    EntityId attacker = CreateCombatEntity(100, 50, 20);
    EntityId defender = CreateCombatEntity(100, 30, 30);

    auto& target_comp = world->GetComponent<TargetComponent>(attacker);
    target_comp.current_target = defender;
    target_comp.auto_attacking = true;

    world->Update(0.1f);

    EXPECT_TRUE(targeted_combat_system->IsInCombat(attacker));
    EXPECT_TRUE(targeted_combat_system->IsInCombat(defender));

    target_comp.auto_attacking = false;

    for (int i = 0; i < 60; ++i) {
        world->Update(0.1f);
    }

    EXPECT_FALSE(targeted_combat_system->IsInCombat(attacker));
    EXPECT_FALSE(targeted_combat_system->IsInCombat(defender));
}

// [SEQUENCE: MVP4-23]
TEST_F(CombatSystemTest, DamageMitigation) {
    EntityId attacker = CreateCombatEntity(100, 50, 20);
    EntityId defender = CreateCombatEntity(100, 30, 100); // High armor

    auto& target_comp = world->GetComponent<TargetComponent>(attacker);
    target_comp.current_target = defender;
    target_comp.auto_attacking = true;

    targeted_combat_system->Update(0.1f);

    auto& defender_health = world->GetComponent<HealthComponent>(defender);
    // Health should be high due to mitigation
    EXPECT_GT(defender_health.current_hp, 95);
}

// [SEQUENCE: MVP4-24]
TEST_F(CombatSystemTest, AreaOfEffectDamage) {
    EntityId attacker = CreateCombatEntity(100, 50, 20);
    EntityId defender1 = CreateCombatEntity(100, 30, 30);
    EntityId defender2 = CreateCombatEntity(100, 30, 30);
    EntityId defender3 = CreateCombatEntity(100, 30, 30);

    auto& transform1 = world->GetComponent<TransformComponent>(defender1);
    transform1.position = {1, 0, 0};
    auto& transform2 = world->GetComponent<TransformComponent>(defender2);
    transform2.position = {2, 0, 0};
    auto& transform3 = world->GetComponent<TransformComponent>(defender3);
    transform3.position = {10, 0, 0}; // Out of range

    action_combat_system->UseAreaSkill(attacker, 1, {1.5, 0, 0});

    world->Update(0.1f);

    auto& health1 = world->GetComponent<HealthComponent>(defender1);
    auto& health2 = world->GetComponent<HealthComponent>(defender2);
    auto& health3 = world->GetComponent<HealthComponent>(defender3);

    EXPECT_LT(health1.current_hp, 100.0f);
    EXPECT_LT(health2.current_hp, 100.0f);
    EXPECT_EQ(health3.current_hp, 100.0f);
}

// [SEQUENCE: MVP4-25]
TEST_F(CombatSystemTest, Healing) {
    EntityId target = CreateCombatEntity(100, 30, 30);

    auto& target_health = world->GetComponent<HealthComponent>(target);
    target_health.current_hp = 50;

    // In a real scenario, a healing skill would be used.
    // For this test, we'll simulate the effect of a healing skill.
    targeted_combat_system->ApplyHealing(target, 20);

    EXPECT_EQ(target_health.current_hp, 70);
}

// [SEQUENCE: MVP4-26]
TEST_F(CombatSystemTest, CombatImmunity) {
    EntityId attacker = CreateCombatEntity(100, 50, 20);
    EntityId defender = CreateCombatEntity(100, 30, 30);

    // Make defender immune
    auto& defender_stats = world->GetComponent<CombatStatsComponent>(defender);
    defender_stats.damage_reduction = 1.0f; // 100% damage reduction

    auto& target_comp = world->GetComponent<TargetComponent>(attacker);
    target_comp.current_target = defender;
    target_comp.auto_attacking = true;

    targeted_combat_system->Update(0.1f);

    auto& defender_health = world->GetComponent<HealthComponent>(defender);
    EXPECT_EQ(defender_health.current_hp, 100.0f);
}

// [SEQUENCE: MVP4-27]
TEST_F(CombatSystemTest, ComboSystem) {
    EntityId attacker = CreateCombatEntity(100, 50, 20);
    EntityId defender = CreateCombatEntity(100, 30, 30);

    // Simulate a 3-hit combo
    action_combat_system->UseMeleeSwing(attacker, {1,0,0});
    world->Update(0.1f);
    action_combat_system->UseMeleeSwing(attacker, {1,0,0});
    world->Update(0.1f);
    action_combat_system->UseMeleeSwing(attacker, {1,0,0});
    world->Update(0.1f);

    auto& defender_health = world->GetComponent<HealthComponent>(defender);
    // Expect high damage due to combo
    EXPECT_LT(defender_health.current_hp, 70);
}