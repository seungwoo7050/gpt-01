#include <gtest/gtest.h>
#include "game/systems/combat/combat_system.h"
#include "game/components/combat_component.h"
#include "game/components/health_component.h"
#include "game/components/stats_component.h"
#include "core/ecs/world.h"

using namespace mmorpg::game::systems;
using namespace mmorpg::game::components;
using namespace mmorpg::core::ecs;

// [SEQUENCE: MVP4-44] Combat system unit tests
class CombatSystemTest : public ::testing::Test {
protected:
    std::unique_ptr<World> world;
    CombatSystem* combat_system;
    
    EntityId CreateCombatEntity(float health, float attack, float defense) {
        EntityId entity = world->CreateEntity();
        
        HealthComponent health_comp;
        health_comp.current_health = health;
        health_comp.max_health = health;
        world->AddComponent(entity, health_comp);
        
        StatsComponent stats;
        stats.attack_power = attack;
        stats.defense = defense;
        stats.critical_chance = 0.1f;
        stats.critical_damage = 1.5f;
        world->AddComponent(entity, stats);
        
        CombatComponent combat;
        combat.in_combat = false;
        combat.last_combat_time = 0.0f;
        world->AddComponent(entity, combat);
        
        return entity;
    }
    
    void SetUp() override {
        world = std::make_unique<World>();
        combat_system = world->RegisterSystem<CombatSystem>();
        combat_system->OnSystemInit();
    }
};

// [SEQUENCE: MVP4-45] Basic damage calculation tests
TEST_F(CombatSystemTest, BasicDamageCalculation) {
    EntityId attacker = CreateCombatEntity(100, 50, 20);
    EntityId defender = CreateCombatEntity(100, 30, 30);
    
    // Calculate damage
    float damage = combat_system->CalculateDamage(attacker, defender, SkillType::BASIC_ATTACK);
    
    // Damage should be: attack_power * skill_modifier - defense
    // 50 * 1.0 - 30 = 20 (before any random modifiers)
    EXPECT_GT(damage, 10.0f); // At least some damage
    EXPECT_LT(damage, 100.0f); // Not excessive
}

// [SEQUENCE: MVP4-46] Critical hit tests
TEST_F(CombatSystemTest, CriticalHitCalculation) {
    EntityId attacker = CreateCombatEntity(100, 50, 20);
    EntityId defender = CreateCombatEntity(100, 30, 30);
    
    // Set 100% crit chance for testing
    auto* stats = world->GetComponent<StatsComponent>(attacker);
    stats->critical_chance = 1.0f;
    stats->critical_damage = 2.0f;
    
    float damage = combat_system->CalculateDamage(attacker, defender, SkillType::BASIC_ATTACK);
    
    // Should be critical hit (double damage)
    EXPECT_GT(damage, 30.0f); // Higher than non-crit
}

// [SEQUENCE: MVP4-47] Combat execution tests
TEST_F(CombatSystemTest, ExecuteCombat) {
    EntityId attacker = CreateCombatEntity(100, 50, 20);
    EntityId defender = CreateCombatEntity(100, 30, 30);
    
    // Execute attack
    CombatResult result = combat_system->ExecuteAttack(attacker, defender, SkillType::BASIC_ATTACK);
    
    EXPECT_TRUE(result.success);
    EXPECT_GT(result.damage_dealt, 0.0f);
    EXPECT_EQ(result.attacker, attacker);
    EXPECT_EQ(result.target, defender);
    
    // Check defender health decreased
    auto* defender_health = world->GetComponent<HealthComponent>(defender);
    EXPECT_LT(defender_health->current_health, defender_health->max_health);
}

// [SEQUENCE: MVP4-48] Death handling tests
TEST_F(CombatSystemTest, DeathHandling) {
    EntityId attacker = CreateCombatEntity(100, 200, 20); // High attack
    EntityId defender = CreateCombatEntity(10, 30, 0); // Low health, no defense
    
    // Execute attack that should kill defender
    CombatResult result = combat_system->ExecuteAttack(attacker, defender, SkillType::BASIC_ATTACK);
    
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.target_died);
    
    // Check defender is dead
    auto* defender_health = world->GetComponent<HealthComponent>(defender);
    EXPECT_EQ(defender_health->current_health, 0.0f);
    EXPECT_TRUE(defender_health->is_dead);
}

// [SEQUENCE: MVP4-49] Skill cooldown tests
TEST_F(CombatSystemTest, SkillCooldowns) {
    EntityId attacker = CreateCombatEntity(100, 50, 20);
    EntityId defender = CreateCombatEntity(100, 30, 30);
    
    // Use skill with cooldown
    combat_system->ExecuteAttack(attacker, defender, SkillType::POWER_STRIKE);
    
    // Try to use same skill immediately
    CombatResult result = combat_system->ExecuteAttack(attacker, defender, SkillType::POWER_STRIKE);
    
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error_code, CombatError::SKILL_ON_COOLDOWN);
    
    // Simulate time passing
    combat_system->Update(5.0f); // 5 seconds
    
    // Should be able to use skill again
    result = combat_system->ExecuteAttack(attacker, defender, SkillType::POWER_STRIKE);
    EXPECT_TRUE(result.success);
}

// [SEQUENCE: MVP4-50] Combat state management tests
TEST_F(CombatSystemTest, CombatStateManagement) {
    EntityId attacker = CreateCombatEntity(100, 50, 20);
    EntityId defender = CreateCombatEntity(100, 30, 30);
    
    // Initially not in combat
    auto* attacker_combat = world->GetComponent<CombatComponent>(attacker);
    auto* defender_combat = world->GetComponent<CombatComponent>(defender);
    
    EXPECT_FALSE(attacker_combat->in_combat);
    EXPECT_FALSE(defender_combat->in_combat);
    
    // Enter combat
    combat_system->ExecuteAttack(attacker, defender, SkillType::BASIC_ATTACK);
    
    EXPECT_TRUE(attacker_combat->in_combat);
    EXPECT_TRUE(defender_combat->in_combat);
    EXPECT_EQ(attacker_combat->current_target, defender);
    EXPECT_EQ(defender_combat->current_target, attacker);
    
    // Simulate combat timeout
    combat_system->Update(10.0f); // 10 seconds
    
    // Should exit combat after timeout
    EXPECT_FALSE(attacker_combat->in_combat);
    EXPECT_FALSE(defender_combat->in_combat);
}

// [SEQUENCE: MVP4-51] Damage mitigation tests
TEST_F(CombatSystemTest, DamageMitigation) {
    EntityId attacker = CreateCombatEntity(100, 50, 20);
    EntityId tank = CreateCombatEntity(200, 30, 100); // High defense
    
    // Add damage reduction buff to tank
    BuffComponent buff;
    buff.damage_reduction = 0.5f; // 50% damage reduction
    buff.duration = 10.0f;
    world->AddComponent(tank, buff);
    
    CombatResult result = combat_system->ExecuteAttack(attacker, tank, SkillType::BASIC_ATTACK);
    
    // Damage should be significantly reduced
    EXPECT_LT(result.damage_dealt, 20.0f); // Less than base damage
}

// [SEQUENCE: MVP4-52] Area of effect damage tests
TEST_F(CombatSystemTest, AreaOfEffectDamage) {
    EntityId caster = CreateCombatEntity(100, 50, 20);
    
    // Create multiple targets
    std::vector<EntityId> targets;
    for (int i = 0; i < 5; ++i) {
        targets.push_back(CreateCombatEntity(100, 30, 20));
        
        // Position targets near each other
        TransformComponent transform;
        transform.position = {float(i), 0, 0};
        world->AddComponent(targets[i], transform);
    }
    
    // Execute AoE attack
    std::vector<CombatResult> results = combat_system->ExecuteAreaAttack(
        caster, 
        {2.5f, 0, 0}, // Center position
        5.0f, // Radius
        SkillType::FIREBALL
    );
    
    // All targets should be hit
    EXPECT_EQ(results.size(), 5);
    
    for (const auto& result : results) {
        EXPECT_TRUE(result.success);
        EXPECT_GT(result.damage_dealt, 0.0f);
    }
}

// [SEQUENCE: MVP4-53] Healing tests
TEST_F(CombatSystemTest, HealingMechanics) {
    EntityId healer = CreateCombatEntity(100, 30, 20);
    EntityId injured = CreateCombatEntity(50, 30, 20); // Half health
    
    // Add healing power to healer
    auto* healer_stats = world->GetComponent<StatsComponent>(healer);
    healer_stats->healing_power = 40.0f;
    
    // Execute heal
    HealResult result = combat_system->ExecuteHeal(healer, injured, SkillType::BASIC_HEAL);
    
    EXPECT_TRUE(result.success);
    EXPECT_GT(result.amount_healed, 0.0f);
    
    // Check target health increased
    auto* target_health = world->GetComponent<HealthComponent>(injured);
    EXPECT_GT(target_health->current_health, 50.0f);
    EXPECT_LE(target_health->current_health, target_health->max_health);
}

// [SEQUENCE: MVP4-54] Combat immunity tests
TEST_F(CombatSystemTest, CombatImmunity) {
    EntityId attacker = CreateCombatEntity(100, 50, 20);
    EntityId defender = CreateCombatEntity(100, 30, 30);
    
    // Add immunity to defender
    auto* defender_combat = world->GetComponent<CombatComponent>(defender);
    defender_combat->is_immune = true;
    defender_combat->immunity_end_time = 5.0f;
    
    // Try to attack immune target
    CombatResult result = combat_system->ExecuteAttack(attacker, defender, SkillType::BASIC_ATTACK);
    
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.error_code, CombatError::TARGET_IMMUNE);
    
    // Check no damage was dealt
    auto* defender_health = world->GetComponent<HealthComponent>(defender);
    EXPECT_EQ(defender_health->current_health, defender_health->max_health);
    
    // Simulate time passing
    combat_system->Update(6.0f);
    
    // Immunity should have expired
    result = combat_system->ExecuteAttack(attacker, defender, SkillType::BASIC_ATTACK);
    EXPECT_TRUE(result.success);
}

// [SEQUENCE: MVP4-55] Combo system tests
TEST_F(CombatSystemTest, ComboSystem) {
    EntityId attacker = CreateCombatEntity(100, 50, 20);
    EntityId defender = CreateCombatEntity(200, 30, 30);
    
    // Execute combo sequence
    combat_system->ExecuteAttack(attacker, defender, SkillType::QUICK_STRIKE);
    combat_system->ExecuteAttack(attacker, defender, SkillType::QUICK_STRIKE);
    CombatResult combo_finisher = combat_system->ExecuteAttack(attacker, defender, SkillType::COMBO_FINISHER);
    
    // Combo finisher should deal bonus damage
    EXPECT_TRUE(combo_finisher.success);
    EXPECT_TRUE(combo_finisher.is_combo);
    EXPECT_GT(combo_finisher.damage_dealt, 50.0f); // Higher than normal attack
    
    // Combo counter should reset
    auto* attacker_combat = world->GetComponent<CombatComponent>(attacker);
    EXPECT_EQ(attacker_combat->combo_count, 0);
}