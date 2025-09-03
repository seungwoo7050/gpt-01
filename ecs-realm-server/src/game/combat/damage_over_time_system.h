#pragma once

#include <unordered_map>
#include <vector>
#include <chrono>
#include <memory>
#include <functional>
#include <spdlog/spdlog.h>

namespace mmorpg::game::combat {

// [SEQUENCE: 1786] Damage over time (DoT) system for periodic damage effects
// 지속 피해(DoT) 시스템으로 주기적 피해 효과 관리

// [SEQUENCE: 1787] DoT damage type
enum class DotDamageType {
    PHYSICAL,       // Bleed, Deep Wounds
    FIRE,           // Burn, Ignite
    FROST,          // Frostbite, Chill
    NATURE,         // Poison, Disease
    SHADOW,         // Corruption, Curse
    HOLY,           // Purge (for undead)
    ARCANE,         // Arcane burn
    CHAOS           // Mixed damage
};

// [SEQUENCE: 1788] DoT stacking behavior
enum class DotStackingType {
    NONE,           // Cannot stack, refreshes duration
    STACK_DAMAGE,   // Each stack adds damage
    STACK_DURATION, // Each application extends duration
    STACK_BOTH,     // Stacks both damage and duration
    UNIQUE_SOURCE,  // One per caster
    REPLACE_WEAKER  // Only stronger effects apply
};

// [SEQUENCE: 1789] DoT spread type
enum class DotSpreadType {
    NONE,           // Does not spread
    ON_DEATH,       // Spreads when target dies
    ON_DAMAGE,      // Chance to spread on tick
    ON_PROXIMITY,   // Spreads to nearby targets
    PANDEMIC        // Refreshing adds 30% of remaining duration
};

// [SEQUENCE: 1790] DoT effect definition
struct DotEffect {
    uint32_t effect_id;
    std::string effect_name;
    DotDamageType damage_type;
    
    // Damage calculation
    float base_damage;              // Per tick
    float spell_power_scaling;      // SP coefficient
    float attack_power_scaling;     // AP coefficient
    bool can_crit = true;           // Can ticks crit?
    
    // Timing
    std::chrono::milliseconds tick_interval{1000};  // Default 1 second
    uint32_t max_ticks = 0;         // 0 = duration-based
    std::chrono::milliseconds base_duration{0};     // If tick-based
    
    // Stacking
    DotStackingType stacking_type = DotStackingType::NONE;
    uint32_t max_stacks = 1;
    float stack_damage_modifier = 1.0f;  // Per stack
    
    // Special properties
    DotSpreadType spread_type = DotSpreadType::NONE;
    float spread_chance = 0.0f;     // 0-100%
    float spread_range = 5.0f;      // Meters
    uint32_t max_spread_targets = 3;
    
    // Modifiers
    float haste_affects_ticks = true;    // Haste reduces interval
    float pandemic_extension = 0.3f;     // Extension ratio
    bool removes_on_damage = false;      // Like sleep
    
    // Additional effects
    uint32_t debuff_id = 0;         // Applied debuff
    std::function<void(uint64_t)> on_tick_callback;
    std::function<void(uint64_t)> on_expire_callback;
};

// [SEQUENCE: 1791] Active DoT instance
class DotInstance {
public:
    DotInstance(uint64_t instance_id, const DotEffect& effect,
                uint64_t source_id, uint64_t target_id,
                float snapshot_sp, float snapshot_ap)
        : instance_id_(instance_id),
          effect_(effect),
          source_id_(source_id),
          target_id_(target_id),
          snapshot_spell_power_(snapshot_sp),
          snapshot_attack_power_(snapshot_ap) {
        
        start_time_ = std::chrono::system_clock::now();
        last_tick_time_ = start_time_;
        
        // Calculate actual duration with haste
        float haste_mod = GetHasteModifier();
        actual_tick_interval_ = std::chrono::milliseconds(
            static_cast<int64_t>(effect.tick_interval.count() / haste_mod)
        );
        
        if (effect.max_ticks > 0) {
            // Tick-based duration
            remaining_ticks_ = effect.max_ticks;
            end_time_ = start_time_ + (actual_tick_interval_ * effect.max_ticks);
        } else {
            // Time-based duration
            end_time_ = start_time_ + effect.base_duration;
            remaining_ticks_ = static_cast<uint32_t>(
                effect.base_duration.count() / actual_tick_interval_.count()
            );
        }
        
        next_tick_time_ = start_time_ + actual_tick_interval_;
    }
    
    // [SEQUENCE: 1792] Process tick
    struct TickResult {
        bool should_tick = false;
        float damage = 0.0f;
        bool is_crit = false;
        bool should_spread = false;
        bool expired = false;
    };
    
    TickResult ProcessTick() {
        TickResult result;
        auto now = std::chrono::system_clock::now();
        
        // Check expiration
        if (now >= end_time_ || remaining_ticks_ == 0) {
            result.expired = true;
            return result;
        }
        
        // Check tick timing
        if (now >= next_tick_time_) {
            result.should_tick = true;
            
            // Calculate damage
            result.damage = CalculateDamage();
            
            // Check crit
            if (effect_.can_crit && RollCrit()) {
                result.is_crit = true;
                result.damage *= GetCritMultiplier();
            }
            
            // Apply stack modifier
            result.damage *= (1.0f + (current_stacks_ - 1) * effect_.stack_damage_modifier);
            
            // Check spread
            if (effect_.spread_type == DotSpreadType::ON_DAMAGE) {
                float roll = static_cast<float>(rand() % 100);
                if (roll < effect_.spread_chance) {
                    result.should_spread = true;
                }
            }
            
            // Update timing
            last_tick_time_ = now;
            next_tick_time_ = now + actual_tick_interval_;
            remaining_ticks_--;
            total_damage_ += result.damage;
            tick_count_++;
            
            // Callback
            if (effect_.on_tick_callback) {
                effect_.on_tick_callback(target_id_);
            }
        }
        
        return result;
    }
    
    // [SEQUENCE: 1793] Refresh DoT (for pandemic mechanics)
    void Refresh(float new_sp = -1, float new_ap = -1) {
        auto now = std::chrono::system_clock::now();
        
        // Update snapshots if provided
        if (new_sp >= 0) snapshot_spell_power_ = new_sp;
        if (new_ap >= 0) snapshot_attack_power_ = new_ap;
        
        if (effect_.spread_type == DotSpreadType::PANDEMIC) {
            // Add portion of remaining duration
            auto remaining = end_time_ - now;
            auto pandemic_bonus = std::chrono::duration_cast<std::chrono::milliseconds>(
                remaining * effect_.pandemic_extension
            );
            
            // Reset to full duration + pandemic bonus
            if (effect_.max_ticks > 0) {
                end_time_ = now + (actual_tick_interval_ * effect_.max_ticks) + pandemic_bonus;
                remaining_ticks_ = effect_.max_ticks + 
                    static_cast<uint32_t>(pandemic_bonus.count() / actual_tick_interval_.count());
            } else {
                end_time_ = now + effect_.base_duration + pandemic_bonus;
                remaining_ticks_ = static_cast<uint32_t>(
                    (effect_.base_duration + pandemic_bonus).count() / actual_tick_interval_.count()
                );
            }
        } else {
            // Simple refresh
            start_time_ = now;
            if (effect_.max_ticks > 0) {
                remaining_ticks_ = effect_.max_ticks;
                end_time_ = now + (actual_tick_interval_ * effect_.max_ticks);
            } else {
                end_time_ = now + effect_.base_duration;
                remaining_ticks_ = static_cast<uint32_t>(
                    effect_.base_duration.count() / actual_tick_interval_.count()
                );
            }
            next_tick_time_ = now + actual_tick_interval_;
        }
    }
    
    // [SEQUENCE: 1794] Add stack
    bool AddStack() {
        if (current_stacks_ < effect_.max_stacks) {
            current_stacks_++;
            
            if (effect_.stacking_type == DotStackingType::STACK_DURATION ||
                effect_.stacking_type == DotStackingType::STACK_BOTH) {
                // Extend duration
                end_time_ += actual_tick_interval_;
                remaining_ticks_++;
            }
            
            return true;
        }
        return false;
    }
    
    // [SEQUENCE: 1795] Force expire
    void ForceExpire() {
        remaining_ticks_ = 0;
        end_time_ = std::chrono::system_clock::now();
        
        if (effect_.on_expire_callback) {
            effect_.on_expire_callback(target_id_);
        }
    }
    
    // Getters
    uint64_t GetInstanceId() const { return instance_id_; }
    uint32_t GetEffectId() const { return effect_.effect_id; }
    uint64_t GetSourceId() const { return source_id_; }
    uint64_t GetTargetId() const { return target_id_; }
    uint32_t GetCurrentStacks() const { return current_stacks_; }
    uint32_t GetRemainingTicks() const { return remaining_ticks_; }
    float GetTotalDamage() const { return total_damage_; }
    
    bool IsExpired() const {
        return std::chrono::system_clock::now() >= end_time_ || remaining_ticks_ == 0;
    }
    
    std::chrono::milliseconds GetRemainingDuration() const {
        auto now = std::chrono::system_clock::now();
        if (now >= end_time_) return std::chrono::milliseconds(0);
        return std::chrono::duration_cast<std::chrono::milliseconds>(end_time_ - now);
    }
    
private:
    uint64_t instance_id_;
    DotEffect effect_;
    uint64_t source_id_;
    uint64_t target_id_;
    
    // Snapshot values (for damage calculation)
    float snapshot_spell_power_;
    float snapshot_attack_power_;
    
    // Timing
    std::chrono::system_clock::time_point start_time_;
    std::chrono::system_clock::time_point end_time_;
    std::chrono::system_clock::time_point last_tick_time_;
    std::chrono::system_clock::time_point next_tick_time_;
    std::chrono::milliseconds actual_tick_interval_;
    
    // State
    uint32_t current_stacks_ = 1;
    uint32_t remaining_ticks_;
    uint32_t tick_count_ = 0;
    float total_damage_ = 0.0f;
    
    // [SEQUENCE: 1796] Calculate damage per tick
    float CalculateDamage() {
        float damage = effect_.base_damage;
        damage += snapshot_spell_power_ * effect_.spell_power_scaling;
        damage += snapshot_attack_power_ * effect_.attack_power_scaling;
        
        // Apply target resistances, armor, etc.
        // TODO: Get from combat system
        
        return damage;
    }
    
    // [SEQUENCE: 1797] Helper functions
    float GetHasteModifier() {
        // TODO: Get actual haste from source
        return 1.0f;
    }
    
    bool RollCrit() {
        // TODO: Get actual crit chance
        float crit_chance = 20.0f;
        return (rand() % 100) < crit_chance;
    }
    
    float GetCritMultiplier() {
        // TODO: Get actual crit multiplier
        return 2.0f;
    }
};

// [SEQUENCE: 1798] DoT manager for a single entity
class DotManager {
public:
    DotManager(uint64_t entity_id) : entity_id_(entity_id) {}
    
    // [SEQUENCE: 1799] Apply DoT effect
    uint64_t ApplyDot(const DotEffect& effect, uint64_t source_id,
                     float spell_power, float attack_power) {
        uint64_t instance_id = GenerateInstanceId();
        
        // Check stacking rules
        if (effect.stacking_type == DotStackingType::UNIQUE_SOURCE) {
            // Remove existing from same source
            RemoveDotsFromSource(source_id, effect.effect_id);
        } else if (effect.stacking_type == DotStackingType::REPLACE_WEAKER) {
            // Check if stronger exists
            auto existing = GetStrongestDot(effect.effect_id);
            if (existing && existing->GetTotalDamage() > 
                effect.base_damage * effect.max_ticks) {
                return 0;  // Don't apply weaker
            }
        }
        
        // Check for refresh
        auto existing_same = GetDotByEffectAndSource(effect.effect_id, source_id);
        if (existing_same) {
            if (effect.stacking_type == DotStackingType::STACK_DAMAGE ||
                effect.stacking_type == DotStackingType::STACK_BOTH) {
                existing_same->AddStack();
            } else {
                existing_same->Refresh(spell_power, attack_power);
            }
            return existing_same->GetInstanceId();
        }
        
        // Create new instance
        auto dot_instance = std::make_unique<DotInstance>(
            instance_id, effect, source_id, entity_id_,
            spell_power, attack_power
        );
        
        active_dots_[instance_id] = std::move(dot_instance);
        
        spdlog::debug("Applied DoT {} to entity {} from source {}",
                     effect.effect_name, entity_id_, source_id);
        
        return instance_id;
    }
    
    // [SEQUENCE: 1800] Process all DoTs
    struct ProcessResult {
        float total_damage = 0.0f;
        std::vector<uint64_t> expired_dots;
        std::vector<std::pair<uint32_t, uint64_t>> spread_targets;  // effect_id, source_id
    };
    
    ProcessResult ProcessDots() {
        ProcessResult result;
        
        for (auto& [id, dot] : active_dots_) {
            auto tick_result = dot->ProcessTick();
            
            if (tick_result.should_tick) {
                result.total_damage += tick_result.damage;
                
                if (tick_result.should_spread) {
                    result.spread_targets.emplace_back(
                        dot->GetEffectId(), dot->GetSourceId()
                    );
                }
            }
            
            if (tick_result.expired) {
                result.expired_dots.push_back(id);
            }
        }
        
        // Remove expired
        for (uint64_t id : result.expired_dots) {
            active_dots_.erase(id);
        }
        
        return result;
    }
    
    // [SEQUENCE: 1801] Remove specific DoT
    void RemoveDot(uint64_t instance_id) {
        auto it = active_dots_.find(instance_id);
        if (it != active_dots_.end()) {
            it->second->ForceExpire();
            active_dots_.erase(it);
        }
    }
    
    // [SEQUENCE: 1802] Remove all DoTs
    void RemoveAllDots() {
        for (auto& [id, dot] : active_dots_) {
            dot->ForceExpire();
        }
        active_dots_.clear();
    }
    
    // [SEQUENCE: 1803] Dispel DoTs
    uint32_t DispelDots(DotDamageType type, uint32_t max_count = 1) {
        std::vector<uint64_t> to_remove;
        
        for (const auto& [id, dot] : active_dots_) {
            if (GetDotEffect(dot->GetEffectId())->damage_type == type) {
                to_remove.push_back(id);
                if (to_remove.size() >= max_count) break;
            }
        }
        
        for (uint64_t id : to_remove) {
            RemoveDot(id);
        }
        
        return static_cast<uint32_t>(to_remove.size());
    }
    
    // Query methods
    std::vector<const DotInstance*> GetActiveDots() const {
        std::vector<const DotInstance*> dots;
        for (const auto& [id, dot] : active_dots_) {
            dots.push_back(dot.get());
        }
        return dots;
    }
    
    bool HasDot(uint32_t effect_id) const {
        return std::any_of(active_dots_.begin(), active_dots_.end(),
            [effect_id](const auto& pair) {
                return pair.second->GetEffectId() == effect_id;
            });
    }
    
private:
    uint64_t entity_id_;
    std::unordered_map<uint64_t, std::unique_ptr<DotInstance>> active_dots_;
    static std::atomic<uint64_t> next_instance_id_;
    
    uint64_t GenerateInstanceId() {
        return next_instance_id_++;
    }
    
    // [SEQUENCE: 1804] Helper methods
    void RemoveDotsFromSource(uint64_t source_id, uint32_t effect_id) {
        std::erase_if(active_dots_, [source_id, effect_id](const auto& pair) {
            return pair.second->GetSourceId() == source_id &&
                   pair.second->GetEffectId() == effect_id;
        });
    }
    
    DotInstance* GetDotByEffectAndSource(uint32_t effect_id, uint64_t source_id) {
        for (auto& [id, dot] : active_dots_) {
            if (dot->GetEffectId() == effect_id && 
                dot->GetSourceId() == source_id) {
                return dot.get();
            }
        }
        return nullptr;
    }
    
    DotInstance* GetStrongestDot(uint32_t effect_id) {
        DotInstance* strongest = nullptr;
        float max_damage = 0.0f;
        
        for (auto& [id, dot] : active_dots_) {
            if (dot->GetEffectId() == effect_id) {
                float potential_damage = dot->GetTotalDamage() / dot->GetRemainingTicks();
                if (potential_damage > max_damage) {
                    max_damage = potential_damage;
                    strongest = dot.get();
                }
            }
        }
        
        return strongest;
    }
    
    // [SEQUENCE: 1805] Get effect definition
    const DotEffect* GetDotEffect(uint32_t effect_id) const;  // Implemented by DotSystem
};

std::atomic<uint64_t> DotManager::next_instance_id_{1};

// [SEQUENCE: 1806] Global DoT system
class DotSystem {
public:
    static DotSystem& Instance() {
        static DotSystem instance;
        return instance;
    }
    
    // [SEQUENCE: 1807] Initialize effects
    void Initialize() {
        LoadDotEffects();
        spdlog::info("DoT system initialized with {} effects", dot_effects_.size());
    }
    
    // [SEQUENCE: 1808] Get or create manager
    std::shared_ptr<DotManager> GetManager(uint64_t entity_id) {
        auto it = entity_managers_.find(entity_id);
        if (it == entity_managers_.end()) {
            auto manager = std::make_shared<DotManager>(entity_id);
            entity_managers_[entity_id] = manager;
            return manager;
        }
        return it->second;
    }
    
    // [SEQUENCE: 1809] Get effect definition
    const DotEffect* GetEffect(uint32_t effect_id) const {
        auto it = dot_effects_.find(effect_id);
        return (it != dot_effects_.end()) ? &it->second : nullptr;
    }
    
    // [SEQUENCE: 1810] Process all DoTs
    void ProcessAll() {
        for (auto& [entity_id, manager] : entity_managers_) {
            auto result = manager->ProcessDots();
            
            if (result.total_damage > 0) {
                // TODO: Apply damage through combat system
            }
            
            // Handle spreading
            for (const auto& [effect_id, source_id] : result.spread_targets) {
                // TODO: Find nearby targets and apply DoT
            }
        }
        
        // Clean up empty managers
        std::erase_if(entity_managers_, [](const auto& pair) {
            return pair.second->GetActiveDots().empty();
        });
    }
    
private:
    DotSystem() = default;
    
    std::unordered_map<uint32_t, DotEffect> dot_effects_;
    std::unordered_map<uint64_t, std::shared_ptr<DotManager>> entity_managers_;
    
    // [SEQUENCE: 1811] Load effect definitions
    void LoadDotEffects() {
        // Example: Bleed effect
        DotEffect bleed;
        bleed.effect_id = 1;
        bleed.effect_name = "Bleed";
        bleed.damage_type = DotDamageType::PHYSICAL;
        bleed.base_damage = 50.0f;
        bleed.attack_power_scaling = 0.3f;
        bleed.tick_interval = std::chrono::milliseconds(1000);
        bleed.max_ticks = 5;
        bleed.stacking_type = DotStackingType::STACK_DAMAGE;
        bleed.max_stacks = 5;
        bleed.stack_damage_modifier = 0.2f;
        dot_effects_[1] = bleed;
        
        // Example: Ignite effect
        DotEffect ignite;
        ignite.effect_id = 2;
        ignite.effect_name = "Ignite";
        ignite.damage_type = DotDamageType::FIRE;
        ignite.base_damage = 100.0f;
        ignite.spell_power_scaling = 0.5f;
        ignite.tick_interval = std::chrono::milliseconds(2000);
        ignite.base_duration = std::chrono::milliseconds(8000);
        ignite.spread_type = DotSpreadType::ON_DEATH;
        ignite.spread_chance = 100.0f;
        ignite.spread_range = 10.0f;
        dot_effects_[2] = ignite;
        
        // TODO: Load from database
    }
};

// [SEQUENCE: 1812] Helper for manager
const DotEffect* DotManager::GetDotEffect(uint32_t effect_id) const {
    return DotSystem::Instance().GetEffect(effect_id);
}

} // namespace mmorpg::game::combat