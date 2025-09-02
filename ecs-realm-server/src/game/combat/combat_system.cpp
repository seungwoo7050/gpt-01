// [SEQUENCE: MVP4-43] Base combat utilities and interfaces implementation
#include "combat_system.h"
#include <spdlog/spdlog.h>
#include <random>
#include <algorithm>
#include <cmath>

namespace mmorpg::game::combat {

static thread_local std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());

DamageInfo DamageCalculator::CalculateDamage(
    const ICombatEntity* attacker,
    const ICombatEntity* target,
    float base_damage,
    DamageType damage_type,
    bool is_skill,
    uint32_t skill_id) {
    
    DamageInfo info;
    info.attacker_id = attacker->GetEntityId();
    info.target_id = target->GetEntityId();
    info.damage_type = damage_type;
    info.base_damage = base_damage;
    info.is_skill = is_skill;
    info.skill_id = skill_id;
    info.timestamp = std::chrono::steady_clock::now();
    
    const auto& attacker_stats = attacker->GetCombatStats();
    const auto& target_stats = target->GetCombatStats();
    
    info.result = DetermineCombatResult(attacker_stats, target_stats, is_skill);
    
    switch (info.result) {
        case CombatResultType::MISS:
        case CombatResultType::DODGE:
        case CombatResultType::IMMUNE:
            info.final_damage = 0.0f;
            break;
            
        case CombatResultType::BLOCK:
            info.final_damage = base_damage * 0.5f;  // 50% damage on block
            break;
            
        case CombatResultType::PARRY:
            info.final_damage = base_damage * 0.25f;  // 25% damage on parry
            break;
            
        case CombatResultType::CRITICAL:
            info.final_damage = base_damage * attacker_stats.critical_damage;
            break;
            
        default:  // HIT, RESIST, ABSORB
            info.final_damage = base_damage;
            break;
    }
    
    if (info.final_damage > 0) {
        info.final_damage = ApplyDamageModifiers(
            info.final_damage, attacker_stats, target_stats, damage_type
        );
    }
    
    return info;
}

float DamageCalculator::ApplyDamageModifiers(
    float base_damage,
    const CombatStats& attacker_stats,
    const CombatStats& target_stats,
    DamageType damage_type) {
    
    float modified_damage = base_damage;
    
    if (damage_type == DamageType::PHYSICAL) {
        modified_damage *= (1.0f + attacker_stats.attack_power / 100.0f);
    } else if (damage_type == DamageType::MAGICAL) {
        modified_damage *= (1.0f + attacker_stats.spell_power / 100.0f);
    }
    
    float defense_reduction = CalculateDefense(target_stats, damage_type);
    modified_damage *= (1.0f - defense_reduction);
    
    auto resist_it = target_stats.resistances.find(damage_type);
    if (resist_it != target_stats.resistances.end()) {
        float resistance = resist_it->second;
        modified_damage *= (1.0f - CalculateResistanceReduction(resistance));
    }
    
    // Minimum 1 damage
    return std::max(1.0f, modified_damage);
}

float DamageCalculator::CalculateDefense(
    const CombatStats& stats,
    DamageType damage_type) {
    
    if (damage_type == DamageType::TRUE_DAMAGE) {
        return 0.0f;  // True damage ignores defense
    }
    
    float defense_value = 0.0f;
    
    if (damage_type == DamageType::PHYSICAL) {
        defense_value = stats.armor;
    } else if (damage_type == DamageType::MAGICAL) {
        defense_value = stats.magic_resist;
    }
    
    return CalculateArmorReduction(defense_value);
}

CombatResultType DamageCalculator::DetermineCombatResult(
    const CombatStats& attacker_stats,
    const CombatStats& target_stats,
    bool is_skill) {
    
    // Check dodge first (can't dodge skills in this implementation)
    if (!is_skill && RollChance(target_stats.dodge_chance)) {
        return CombatResultType::DODGE;
    }
    
    // Check parry (melee only, not for skills)
    if (!is_skill && RollChance(target_stats.parry_chance)) {
        return CombatResultType::PARRY;
    }
    
    // Check block
    if (RollChance(target_stats.block_chance)) {
        return CombatResultType::BLOCK;
    }
    
    // Check critical
    if (RollChance(attacker_stats.critical_chance)) {
        return CombatResultType::CRITICAL;
    }
    
    // Normal hit
    return CombatResultType::HIT;
}

bool DamageCalculator::RollChance(float chance) {
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(rng) < chance;
}

float DamageCalculator::CalculateArmorReduction(float armor) {
    // Armor formula: reduction = armor / (armor + 100)
    // At 100 armor = 50% reduction, 200 armor = 66.7% reduction
    return armor / (armor + 100.0f);
}

float DamageCalculator::CalculateResistanceReduction(float resistance) {
    // Similar to armor but with different scaling
    return resistance / (resistance + 150.0f);
}

void CombatManager::RegisterEntity(std::shared_ptr<ICombatEntity> entity) {
    if (!entity) return;
    
    uint64_t entity_id = entity->GetEntityId();
    entities_[entity_id] = entity;
    
    spdlog::debug("Registered combat entity: {}", entity_id);
}

void CombatManager::UnregisterEntity(uint64_t entity_id) {
    // Clean up threat table
    threat_table_.erase(entity_id);
    for (auto& [other_id, threats] : threat_table_) {
        threats.erase(entity_id);
    }
    
    // Clean up auto-attack
    auto_attacks_.erase(entity_id);
    
    // Clean up combat log
    combat_logs_.erase(entity_id);
    
    // Remove entity
    entities_.erase(entity_id);
    
    spdlog::debug("Unregistered combat entity: {}", entity_id);
}

bool CombatManager::ExecuteAttack(uint64_t attacker_id, uint64_t target_id) {
    auto attacker = GetEntity(attacker_id);
    auto target = GetEntity(target_id);
    
    if (!attacker || !target) {
        return false;
    }
    
    if (!IsValidTarget(attacker_id, target_id)) {
        return false;
    }
    
    // TODO: Check range (requires position component)
    
    const auto& attacker_stats = attacker->GetCombatStats();
    float base_damage = attacker_stats.attack_power;
    
    DamageInfo damage_info = DamageCalculator::CalculateDamage(
        attacker.get(), target.get(), base_damage, DamageType::PHYSICAL
    );
    
    if (damage_info.final_damage > 0) {
        target->TakeDamage(damage_info.final_damage);
        
        if (attacker_stats.life_steal > 0) {
            float heal_amount = damage_info.final_damage * attacker_stats.life_steal;
            attacker->Heal(heal_amount);
        }
    }
    
    AddThreat(attacker_id, target_id, damage_info.final_damage);
    
    LogCombatEvent(damage_info);
    
    attacker->OnDamageDealt(damage_info);
    target->OnDamageTaken(damage_info);
    
    if (!target->IsAlive()) {
        target->OnDeath();
        attacker->OnKill(target_id);
        StopAutoAttack(attacker_id);  // Stop attacking dead target
    }
    
    return true;
}

std::vector<DamageInfo> CombatManager::ExecuteAreaDamage(
    uint64_t attacker_id,
    float center_x, float center_y, float center_z,
    float radius,
    float base_damage,
    DamageType damage_type,
    TargetType target_type) {
    
    std::vector<DamageInfo> results;
    
    auto attacker = GetEntity(attacker_id);
    if (!attacker) {
        return results;
    }
    
    auto targets = GetEntitiesInRange(center_x, center_y, center_z, radius, target_type);
    
    for (uint64_t target_id : targets) {
        if (target_id == attacker_id && target_type != TargetType::SELF) {
            continue;  // Don't hit self unless it's a self-target ability
        }
        
        auto target = GetEntity(target_id);
        if (!target || !target->CanBeTargeted()) {
            continue;
        }
        
        // Calculate damage with falloff based on distance
        // TODO: Get actual positions from position component
        float distance_modifier = 1.0f;  // For now, no falloff
        
        DamageInfo damage_info = DamageCalculator::CalculateDamage(
            attacker.get(), target.get(), 
            base_damage * distance_modifier, 
            damage_type
        );
        
        if (damage_info.final_damage > 0) {
            target->TakeDamage(damage_info.final_damage);
            AddThreat(attacker_id, target_id, damage_info.final_damage * 0.5f);  // Reduced threat for AoE
        }
        
        LogCombatEvent(damage_info);
        results.push_back(damage_info);
        
        // Trigger events
        attacker->OnDamageDealt(damage_info);
        target->OnDamageTaken(damage_info);
        
        if (!target->IsAlive()) {
            target->OnDeath();
            attacker->OnKill(target_id);
        }
    }
    
    return results;
}

std::vector<uint64_t> CombatManager::GetEntitiesInRange(
    float center_x, float center_y, float center_z,
    float radius,
    TargetType filter) {
    
    std::vector<uint64_t> result;
    
    // TODO: This should use spatial indexing from world management
    // For now, check all entities
    for (const auto& [entity_id, entity] : entities_) {
        if (!entity->CanBeTargeted()) {
            continue;
        }
        
        // TODO: Get actual position from position component
        // For now, assume all entities are in range for testing
        result.push_back(entity_id);
    }
    
    return result;
}

void CombatManager::AddThreat(uint64_t attacker_id, uint64_t target_id, float threat) {
    if (threat <= 0) return;
    
    auto& threat_info = threat_table_[target_id][attacker_id];
    threat_info.threat_value += threat;
    threat_info.last_update = std::chrono::steady_clock::now();
    
    spdlog::debug("Added {} threat from {} to {}", threat, attacker_id, target_id);
}

float CombatManager::GetThreat(uint64_t attacker_id, uint64_t target_id) const {
    auto target_it = threat_table_.find(target_id);
    if (target_it == threat_table_.end()) {
        return 0.0f;
    }
    
    auto attacker_it = target_it->second.find(attacker_id);
    if (attacker_it == target_it->second.end()) {
        return 0.0f;
    }
    
    return attacker_it->second.threat_value;
}

uint64_t CombatManager::GetHighestThreatTarget(uint64_t entity_id) const {
    auto it = threat_table_.find(entity_id);
    if (it == threat_table_.end() || it->second.empty()) {
        return 0;
    }
    
    uint64_t highest_threat_id = 0;
    float highest_threat = 0.0f;
    
    for (const auto& [attacker_id, threat_info] : it->second) {
        if (threat_info.threat_value > highest_threat) {
            highest_threat = threat_info.threat_value;
            highest_threat_id = attacker_id;
        }
    }
    
    return highest_threat_id;
}

void CombatManager::LogCombatEvent(const DamageInfo& info) {
    // Log for attacker
    combat_logs_[info.attacker_id].push_back(info);
    if (combat_logs_[info.attacker_id].size() > 1000) {
        combat_logs_[info.attacker_id].erase(combat_logs_[info.attacker_id].begin());
    }
    
    // Log for target
    combat_logs_[info.target_id].push_back(info);
    if (combat_logs_[info.target_id].size() > 1000) {
        combat_logs_[info.target_id].erase(combat_logs_[info.target_id].begin());
    }
    
    // Debug log
    spdlog::debug("Combat: {} dealt {} damage to {} ({})", 
                  info.attacker_id, info.final_damage, info.target_id,
                  static_cast<int>(info.result));
}

std::vector<DamageInfo> CombatManager::GetCombatLog(uint64_t entity_id, size_t max_entries) const {
    auto it = combat_logs_.find(entity_id);
    if (it == combat_logs_.end()) {
        return {};
    }
    
    const auto& log = it->second;
    if (log.size() <= max_entries) {
        return log;
    }
    
    // Return last max_entries
    return std::vector<DamageInfo>(log.end() - max_entries, log.end());
}

void CombatManager::StartAutoAttack(uint64_t attacker_id, uint64_t target_id) {
    auto attacker = GetEntity(attacker_id);
    auto target = GetEntity(target_id);
    
    if (!attacker || !target || !IsValidTarget(attacker_id, target_id)) {
        return;
    }
    
    auto_attacks_[attacker_id] = {target_id, 0.0f, true};
    
    spdlog::debug("Started auto-attack: {} -> {}", attacker_id, target_id);
}

void CombatManager::StopAutoAttack(uint64_t attacker_id) {
    auto_attacks_.erase(attacker_id);
    spdlog::debug("Stopped auto-attack for {}", attacker_id);
}

void CombatManager::UpdateAutoAttacks(float delta_time) {
    std::vector<uint64_t> to_remove;
    
    for (auto& [attacker_id, auto_attack] : auto_attacks_) {
        if (!auto_attack.is_active) {
            continue;
        }
        
        auto attacker = GetEntity(attacker_id);
        if (!attacker || !attacker->CanAttack()) {
            to_remove.push_back(attacker_id);
            continue;
        }
        
        // Update attack timer
        auto_attack.time_since_last_attack += delta_time;
        
        // Check if it's time to attack
        float attack_speed = attacker->GetCombatStats().attack_speed;
        float attack_interval = 1.0f / attack_speed;
        
        if (auto_attack.time_since_last_attack >= attack_interval) {
            // Execute attack
            if (ExecuteAttack(attacker_id, auto_attack.target_id)) {
                auto_attack.time_since_last_attack = 0.0f;
            } else {
                // Attack failed, stop auto-attack
                to_remove.push_back(attacker_id);
            }
        }
    }
    
    // Clean up stopped auto-attacks
    for (uint64_t id : to_remove) {
        StopAutoAttack(id);
    }
}

std::shared_ptr<ICombatEntity> CombatManager::GetEntity(uint64_t entity_id) const {
    auto it = entities_.find(entity_id);
    return (it != entities_.end()) ? it->second : nullptr;
}

bool CombatManager::IsValidTarget(uint64_t attacker_id, uint64_t target_id) const {
    if (attacker_id == target_id) {
        return false;  // Can't attack self
    }
    
    auto attacker = GetEntity(attacker_id);
    auto target = GetEntity(target_id);
    
    if (!attacker || !target) {
        return false;
    }
    
    if (!attacker->CanAttack() || !target->CanBeTargeted() || !target->IsAlive()) {
        return false;
    }
    
    // TODO: Check faction, PvP flags, etc.
    
    return true;
}

void CombatEventHandler::RegisterDamageHandler(DamageHandler handler) {
    damage_handlers_.push_back(handler);
}

void CombatEventHandler::RegisterDeathHandler(DeathHandler handler) {
    death_handlers_.push_back(handler);
}

void CombatEventHandler::OnDamage(const DamageInfo& info) {
    for (const auto& handler : damage_handlers_) {
        handler(info);
    }
}

void CombatEventHandler::OnDeath(uint64_t entity_id) {
    for (const auto& handler : death_handlers_) {
        handler(entity_id);
    }
}

bool CombatUtils::IsBehindTarget(float attacker_x, float attacker_y,
                                float target_x, float target_y, float target_facing) {
    // Calculate angle between attacker and target
    float dx = attacker_x - target_x;
    float dy = attacker_y - target_y;
    float angle_to_attacker = std::atan2(dy, dx);
    
    // Normalize angles
    while (angle_to_attacker < 0) angle_to_attacker += 2 * M_PI;
    while (angle_to_attacker > 2 * M_PI) angle_to_attacker -= 2 * M_PI;
    while (target_facing < 0) target_facing += 2 * M_PI;
    while (target_facing > 2 * M_PI) target_facing -= 2 * M_PI;
    
    // Calculate angle difference
    float angle_diff = std::abs(angle_to_attacker - (target_facing + M_PI));
    if (angle_diff > M_PI) angle_diff = 2 * M_PI - angle_diff;
    
    // Behind if within 90 degrees of back
    return angle_diff < (M_PI / 2);
}

} // namespace mmorpg::game::combat
