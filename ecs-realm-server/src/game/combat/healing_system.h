#pragma once

#include <unordered_map>
#include <vector>
#include <chrono>
#include <algorithm>
#include <optional>
#include <spdlog/spdlog.h>

namespace mmorpg::game::combat {

// [SEQUENCE: 1813] Healing system for health restoration and support
// 치유 시스템으로 체력 회복 및 지원 관리

// [SEQUENCE: 1814] Healing type
enum class HealingType {
    DIRECT,         // Instant healing
    HOT,            // Heal over time
    SHIELD,         // Damage absorption
    LIFESTEAL,      // Damage to healing
    REGEN,          // Natural regeneration
    CHANNELED,      // Continuous healing
    SMART,          // Targets lowest health
    CHAIN,          // Jumps between targets
    SPLASH          // Area healing
};

// [SEQUENCE: 1815] Healing school
enum class HealingSchool {
    HOLY,           // Divine healing
    NATURE,         // Natural healing
    SHADOW,         // Dark healing (undead)
    ARCANE,         // Magical healing
    PHYSICAL,       // Bandages, potions
    ELEMENTAL       // Elemental restoration
};

// [SEQUENCE: 1816] Healing modifier type
enum class HealModifierType {
    FLAT_BONUS,         // +X healing
    PERCENT_BONUS,      // +X% healing
    CRIT_CHANCE,        // +X% crit chance
    CRIT_BONUS,         // +X% crit healing
    CAST_TIME,          // -X% cast time
    MANA_COST,          // -X% mana cost
    RANGE,              // +X range
    TARGET_COUNT        // +X targets
};

// [SEQUENCE: 1817] Healing event for calculations
struct HealingEvent {
    uint64_t healer_id;
    uint64_t target_id;
    uint32_t spell_id;
    HealingType heal_type;
    HealingSchool school;
    
    // Base values
    float base_heal;
    float spell_power_coeff;
    float versatility_bonus;
    
    // Modifiers
    bool can_crit = true;
    float crit_chance = 0.0f;
    float crit_multiplier = 1.5f;
    
    // Calculated values
    float final_heal = 0.0f;
    float effective_heal = 0.0f;  // Actual health restored
    float overheal = 0.0f;         // Wasted healing
    bool was_crit = false;
    
    // Timestamp
    std::chrono::system_clock::time_point timestamp;
};

// [SEQUENCE: 1818] Heal over time (HoT) effect
struct HealOverTime {
    uint64_t hot_id;
    uint64_t healer_id;
    uint64_t target_id;
    uint32_t spell_id;
    
    // Healing values
    float heal_per_tick;
    float spell_power_snapshot;     // SP at cast time
    
    // Timing
    std::chrono::milliseconds tick_interval;
    uint32_t remaining_ticks;
    std::chrono::system_clock::time_point next_tick;
    std::chrono::system_clock::time_point expire_time;
    
    // Properties
    bool can_crit = false;          // Can ticks crit?
    float crit_chance = 0.0f;
    bool refreshable = true;        // Can be refreshed?
    bool pandemic = true;           // Uses pandemic mechanic?
    
    // Stats
    float total_healing = 0.0f;
    uint32_t crit_count = 0;
    
    // [SEQUENCE: 1819] Process tick
    std::optional<float> ProcessTick() {
        auto now = std::chrono::system_clock::now();
        if (now < next_tick || remaining_ticks == 0) {
            return std::nullopt;
        }
        
        float heal = heal_per_tick;
        bool is_crit = false;
        
        if (can_crit && (rand() % 100) < crit_chance) {
            heal *= 1.5f;
            is_crit = true;
            crit_count++;
        }
        
        next_tick = now + tick_interval;
        remaining_ticks--;
        total_healing += heal;
        
        return heal;
    }
    
    // [SEQUENCE: 1820] Refresh with pandemic
    void Refresh(float new_sp_snapshot) {
        auto now = std::chrono::system_clock::now();
        spell_power_snapshot = new_sp_snapshot;
        
        if (pandemic && remaining_ticks > 0) {
            // Add 30% of remaining duration
            uint32_t bonus_ticks = static_cast<uint32_t>(remaining_ticks * 0.3f);
            remaining_ticks += bonus_ticks;
        }
        
        // Reset to full duration
        remaining_ticks = std::max(remaining_ticks, GetMaxTicks());
        expire_time = now + (tick_interval * remaining_ticks);
    }
    
    bool IsExpired() const {
        return remaining_ticks == 0 || 
               std::chrono::system_clock::now() >= expire_time;
    }
    
private:
    uint32_t GetMaxTicks() const {
        // TODO: Get from spell data
        return 8;  // Default 8 ticks
    }
};

// [SEQUENCE: 1821] Absorption shield
struct AbsorptionShield {
    uint64_t shield_id;
    uint64_t caster_id;
    uint64_t target_id;
    uint32_t spell_id;
    
    // Shield properties
    float max_absorb;
    float remaining_absorb;
    HealingSchool school;
    
    // Duration
    std::chrono::system_clock::time_point expire_time;
    
    // Restrictions
    std::vector<DamageType> absorbed_types;  // Empty = all types
    float absorb_percent = 1.0f;              // % of damage absorbed
    
    // [SEQUENCE: 1822] Absorb damage
    float AbsorbDamage(float damage, DamageType type) {
        // Check if this damage type is absorbed
        if (!absorbed_types.empty()) {
            if (std::find(absorbed_types.begin(), absorbed_types.end(), type) 
                == absorbed_types.end()) {
                return 0.0f;
            }
        }
        
        float to_absorb = damage * absorb_percent;
        float absorbed = std::min(to_absorb, remaining_absorb);
        remaining_absorb -= absorbed;
        
        return absorbed;
    }
    
    bool IsExpired() const {
        return remaining_absorb <= 0 || 
               std::chrono::system_clock::now() >= expire_time;
    }
    
    float GetAbsorbPercent() const {
        return remaining_absorb / max_absorb;
    }
};

// [SEQUENCE: 1823] Healing modifier
struct HealingModifier {
    HealModifierType type;
    float value;
    std::chrono::system_clock::time_point expire_time;
    uint32_t source_spell_id = 0;
    
    bool IsExpired() const {
        return std::chrono::system_clock::now() >= expire_time;
    }
};

// [SEQUENCE: 1824] Healing target manager
class HealingTarget {
public:
    HealingTarget(uint64_t entity_id) : entity_id_(entity_id) {}
    
    // [SEQUENCE: 1825] Apply direct healing
    HealingEvent ReceiveHealing(const HealingEvent& event) {
        HealingEvent result = event;
        
        // Get current and max health
        float current_hp = GetCurrentHealth();
        float max_hp = GetMaxHealth();
        float missing_hp = max_hp - current_hp;
        
        // Apply healing modifiers
        float healing_mod = CalculateHealingModifier(event.school);
        result.final_heal = event.base_heal * healing_mod;
        
        // Check for crit
        if (event.can_crit && RollCrit(event.crit_chance)) {
            result.was_crit = true;
            result.final_heal *= event.crit_multiplier;
        }
        
        // Calculate effective healing and overheal
        result.effective_heal = std::min(result.final_heal, missing_hp);
        result.overheal = result.final_heal - result.effective_heal;
        
        // Apply the healing
        SetCurrentHealth(current_hp + result.effective_heal);
        
        // Update statistics
        total_healing_received_ += result.effective_heal;
        total_overheal_ += result.overheal;
        
        // Log significant heals
        if (result.effective_heal > 0) {
            spdlog::debug("Entity {} healed for {} ({}% overheal)",
                         entity_id_, result.effective_heal,
                         (result.overheal / result.final_heal) * 100);
        }
        
        return result;
    }
    
    // [SEQUENCE: 1826] Add HoT effect
    uint64_t AddHealOverTime(const HealOverTime& hot) {
        // Check for existing HoT from same spell
        auto existing = FindHotBySpell(hot.spell_id, hot.healer_id);
        if (existing) {
            existing->Refresh(hot.spell_power_snapshot);
            return existing->hot_id;
        }
        
        // Add new HoT
        active_hots_[hot.hot_id] = hot;
        return hot.hot_id;
    }
    
    // [SEQUENCE: 1827] Add absorption shield
    uint64_t AddShield(const AbsorptionShield& shield) {
        // Check stacking rules
        // Some shields stack, some don't
        auto existing = FindShieldBySpell(shield.spell_id);
        if (existing) {
            if (shield.remaining_absorb > existing->remaining_absorb) {
                // Replace with stronger shield
                existing->remaining_absorb = shield.remaining_absorb;
                existing->max_absorb = shield.max_absorb;
                existing->expire_time = shield.expire_time;
            }
            return existing->shield_id;
        }
        
        active_shields_[shield.shield_id] = shield;
        total_shield_value_ += shield.remaining_absorb;
        return shield.shield_id;
    }
    
    // [SEQUENCE: 1828] Process incoming damage through shields
    float ProcessDamageWithShields(float damage, DamageType type) {
        float remaining_damage = damage;
        std::vector<uint64_t> depleted_shields;
        
        // Process shields in order (newest first)
        for (auto& [id, shield] : active_shields_) {
            if (shield.IsExpired()) {
                depleted_shields.push_back(id);
                continue;
            }
            
            float absorbed = shield.AbsorbDamage(remaining_damage, type);
            remaining_damage -= absorbed;
            total_absorbed_ += absorbed;
            
            if (shield.remaining_absorb <= 0) {
                depleted_shields.push_back(id);
            }
            
            if (remaining_damage <= 0) {
                break;
            }
        }
        
        // Remove depleted shields
        for (uint64_t id : depleted_shields) {
            RemoveShield(id);
        }
        
        return remaining_damage;
    }
    
    // [SEQUENCE: 1829] Update all effects
    void Update() {
        UpdateHots();
        UpdateShields();
        CleanupExpiredModifiers();
    }
    
    // [SEQUENCE: 1830] Add healing modifier
    void AddModifier(const HealingModifier& modifier) {
        healing_modifiers_.push_back(modifier);
    }
    
    // Query methods
    float GetTotalShieldValue() const {
        float total = 0.0f;
        for (const auto& [id, shield] : active_shields_) {
            if (!shield.IsExpired()) {
                total += shield.remaining_absorb;
            }
        }
        return total;
    }
    
    std::vector<const HealOverTime*> GetActiveHots() const {
        std::vector<const HealOverTime*> hots;
        for (const auto& [id, hot] : active_hots_) {
            if (!hot.IsExpired()) {
                hots.push_back(&hot);
            }
        }
        return hots;
    }
    
    float GetHealingModifier(HealingSchool school) const {
        return CalculateHealingModifier(school);
    }
    
private:
    uint64_t entity_id_;
    
    // Active effects
    std::unordered_map<uint64_t, HealOverTime> active_hots_;
    std::unordered_map<uint64_t, AbsorptionShield> active_shields_;
    std::vector<HealingModifier> healing_modifiers_;
    
    // Statistics
    float total_healing_received_ = 0.0f;
    float total_overheal_ = 0.0f;
    float total_absorbed_ = 0.0f;
    float total_shield_value_ = 0.0f;
    
    // [SEQUENCE: 1831] Update HoTs
    void UpdateHots() {
        std::vector<uint64_t> expired;
        
        for (auto& [id, hot] : active_hots_) {
            if (hot.IsExpired()) {
                expired.push_back(id);
                continue;
            }
            
            auto heal = hot.ProcessTick();
            if (heal.has_value()) {
                HealingEvent event;
                event.healer_id = hot.healer_id;
                event.target_id = hot.target_id;
                event.spell_id = hot.spell_id;
                event.heal_type = HealingType::HOT;
                event.base_heal = heal.value();
                event.timestamp = std::chrono::system_clock::now();
                
                ReceiveHealing(event);
            }
        }
        
        for (uint64_t id : expired) {
            active_hots_.erase(id);
        }
    }
    
    // [SEQUENCE: 1832] Update shields
    void UpdateShields() {
        std::vector<uint64_t> expired;
        total_shield_value_ = 0.0f;
        
        for (auto& [id, shield] : active_shields_) {
            if (shield.IsExpired()) {
                expired.push_back(id);
            } else {
                total_shield_value_ += shield.remaining_absorb;
            }
        }
        
        for (uint64_t id : expired) {
            active_shields_.erase(id);
        }
    }
    
    // [SEQUENCE: 1833] Clean expired modifiers
    void CleanupExpiredModifiers() {
        std::erase_if(healing_modifiers_, [](const auto& mod) {
            return mod.IsExpired();
        });
    }
    
    // [SEQUENCE: 1834] Calculate healing modifier
    float CalculateHealingModifier(HealingSchool school) const {
        float modifier = 1.0f;
        
        for (const auto& mod : healing_modifiers_) {
            if (mod.type == HealModifierType::FLAT_BONUS) {
                modifier += mod.value;
            } else if (mod.type == HealModifierType::PERCENT_BONUS) {
                modifier *= (1.0f + mod.value / 100.0f);
            }
        }
        
        return modifier;
    }
    
    // Helper methods
    HealOverTime* FindHotBySpell(uint32_t spell_id, uint64_t healer_id) {
        for (auto& [id, hot] : active_hots_) {
            if (hot.spell_id == spell_id && hot.healer_id == healer_id) {
                return &hot;
            }
        }
        return nullptr;
    }
    
    AbsorptionShield* FindShieldBySpell(uint32_t spell_id) {
        for (auto& [id, shield] : active_shields_) {
            if (shield.spell_id == spell_id) {
                return &shield;
            }
        }
        return nullptr;
    }
    
    void RemoveShield(uint64_t shield_id) {
        active_shields_.erase(shield_id);
    }
    
    // [SEQUENCE: 1835] Health access (implemented by entity system)
    float GetCurrentHealth() const;
    float GetMaxHealth() const;
    void SetCurrentHealth(float health);
    
    bool RollCrit(float crit_chance) const {
        return (rand() % 10000) < (crit_chance * 100);
    }
};

// [SEQUENCE: 1836] Healing manager
class HealingManager {
public:
    static HealingManager& Instance() {
        static HealingManager instance;
        return instance;
    }
    
    // [SEQUENCE: 1837] Process healing spell
    HealingEvent ProcessHeal(uint64_t healer_id, uint64_t target_id,
                            uint32_t spell_id, float base_heal,
                            HealingType type = HealingType::DIRECT) {
        HealingEvent event;
        event.healer_id = healer_id;
        event.target_id = target_id;
        event.spell_id = spell_id;
        event.heal_type = type;
        event.base_heal = base_heal;
        event.timestamp = std::chrono::system_clock::now();
        
        // Get spell data
        // TODO: Load from spell database
        event.school = HealingSchool::HOLY;
        event.spell_power_coeff = 0.8f;
        event.can_crit = true;
        event.crit_chance = GetHealerCritChance(healer_id);
        
        // Apply spell power
        float spell_power = GetHealerSpellPower(healer_id);
        event.base_heal += spell_power * event.spell_power_coeff;
        
        // Get target manager
        auto target_manager = GetOrCreateTarget(target_id);
        
        // Apply healing
        auto result = target_manager->ReceiveHealing(event);
        
        // Generate threat
        if (result.effective_heal > 0) {
            GenerateHealingThreat(healer_id, result.effective_heal);
        }
        
        return result;
    }
    
    // [SEQUENCE: 1838] Apply HoT
    uint64_t ApplyHealOverTime(uint64_t healer_id, uint64_t target_id,
                               uint32_t spell_id, float heal_per_tick,
                               std::chrono::milliseconds interval,
                               uint32_t ticks) {
        static std::atomic<uint64_t> hot_id_counter{1};
        
        HealOverTime hot;
        hot.hot_id = hot_id_counter++;
        hot.healer_id = healer_id;
        hot.target_id = target_id;
        hot.spell_id = spell_id;
        hot.heal_per_tick = heal_per_tick;
        hot.spell_power_snapshot = GetHealerSpellPower(healer_id);
        hot.tick_interval = interval;
        hot.remaining_ticks = ticks;
        hot.next_tick = std::chrono::system_clock::now() + interval;
        hot.expire_time = std::chrono::system_clock::now() + (interval * ticks);
        hot.crit_chance = GetHealerCritChance(healer_id);
        
        auto target_manager = GetOrCreateTarget(target_id);
        return target_manager->AddHealOverTime(hot);
    }
    
    // [SEQUENCE: 1839] Apply shield
    uint64_t ApplyShield(uint64_t caster_id, uint64_t target_id,
                        uint32_t spell_id, float absorb_amount,
                        std::chrono::seconds duration) {
        static std::atomic<uint64_t> shield_id_counter{1};
        
        AbsorptionShield shield;
        shield.shield_id = shield_id_counter++;
        shield.caster_id = caster_id;
        shield.target_id = target_id;
        shield.spell_id = spell_id;
        shield.max_absorb = absorb_amount;
        shield.remaining_absorb = absorb_amount;
        shield.school = HealingSchool::HOLY;  // TODO: From spell data
        shield.expire_time = std::chrono::system_clock::now() + duration;
        
        auto target_manager = GetOrCreateTarget(target_id);
        return target_manager->AddShield(shield);
    }
    
    // [SEQUENCE: 1840] Update all healing effects
    void UpdateAll() {
        for (auto& [entity_id, manager] : target_managers_) {
            manager->Update();
        }
        
        // Clean up managers with no active effects
        std::erase_if(target_managers_, [](const auto& pair) {
            return pair.second->GetActiveHots().empty() && 
                   pair.second->GetTotalShieldValue() <= 0;
        });
    }
    
    // [SEQUENCE: 1841] Get target manager
    std::shared_ptr<HealingTarget> GetOrCreateTarget(uint64_t entity_id) {
        auto it = target_managers_.find(entity_id);
        if (it == target_managers_.end()) {
            auto manager = std::make_shared<HealingTarget>(entity_id);
            target_managers_[entity_id] = manager;
            return manager;
        }
        return it->second;
    }
    
private:
    HealingManager() = default;
    
    std::unordered_map<uint64_t, std::shared_ptr<HealingTarget>> target_managers_;
    
    // [SEQUENCE: 1842] Helper methods
    float GetHealerSpellPower(uint64_t healer_id) {
        // TODO: Get from stats system
        return 1000.0f;
    }
    
    float GetHealerCritChance(uint64_t healer_id) {
        // TODO: Get from stats system
        return 25.0f;
    }
    
    void GenerateHealingThreat(uint64_t healer_id, float healing) {
        // Healing generates 0.5 threat per point healed
        // TODO: Integrate with threat system
        float threat = healing * 0.5f;
    }
};

// [SEQUENCE: 1843] Stub implementations for health access
float HealingTarget::GetCurrentHealth() const {
    // TODO: Get from entity/stats system
    return 1000.0f;
}

float HealingTarget::GetMaxHealth() const {
    // TODO: Get from entity/stats system
    return 2000.0f;
}

void HealingTarget::SetCurrentHealth(float health) {
    // TODO: Update entity/stats system
}

} // namespace mmorpg::game::combat