#pragma once

#include <unordered_map>
#include <vector>
#include <chrono>
#include <algorithm>
#include <bitset>
#include <spdlog/spdlog.h>

namespace mmorpg::game::combat {

// [SEQUENCE: 1729] Crowd control system for movement and action restrictions
// 군중 제어 시스템으로 이동 및 행동 제한 관리

// [SEQUENCE: 1730] Crowd control types
enum class CrowdControlType : uint32_t {
    NONE = 0,
    STUN = 1 << 0,           // Cannot move or act
    ROOT = 1 << 1,           // Cannot move
    SILENCE = 1 << 2,        // Cannot cast spells
    DISARM = 1 << 3,         // Cannot use weapons
    FEAR = 1 << 4,           // Forced movement away
    CHARM = 1 << 5,          // Controlled by enemy
    SLEEP = 1 << 6,          // Disabled until damaged
    POLYMORPH = 1 << 7,      // Transformed
    SLOW = 1 << 8,           // Movement speed reduced
    SNARE = 1 << 9,          // Attack speed reduced
    BLIND = 1 << 10,         // Reduced accuracy/vision
    CONFUSE = 1 << 11,       // Random movement
    TAUNT = 1 << 12,         // Forced to attack taunter
    PACIFY = 1 << 13,        // Cannot use abilities
    BANISH = 1 << 14,        // Phased out
    FREEZE = 1 << 15,        // Frozen in ice
    KNOCKBACK = 1 << 16,     // Pushed back
    KNOCKUP = 1 << 17,       // Launched airborne
    SUPPRESS = 1 << 18,      // Cannot use mobility
    GROUNDED = 1 << 19       // Cannot jump/fly
};

// [SEQUENCE: 1731] CC break type
enum class CCBreakType {
    NONE,               // Cannot be broken
    DAMAGE,             // Breaks on damage
    DAMAGE_THRESHOLD,   // Breaks after X damage
    MOVEMENT,           // Breaks on movement attempt
    ACTION,             // Breaks on any action
    TIMER_ONLY          // Only expires with time
};

// [SEQUENCE: 1732] CC immunity type
enum class CCImmunityType {
    NONE,
    TEMPORARY,          // Immunity after CC ends
    PERMANENT,          // Always immune
    DIMINISHING         // Reduced duration each time
};

// [SEQUENCE: 1733] Crowd control effect
struct CrowdControlEffect {
    uint64_t effect_id;
    CrowdControlType type;
    
    // Source info
    uint64_t source_id;         // Who applied the CC
    uint32_t ability_id;        // What ability caused it
    
    // Duration
    std::chrono::milliseconds base_duration;
    std::chrono::milliseconds remaining_duration;
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    
    // Properties
    CCBreakType break_type = CCBreakType::TIMER_ONLY;
    float break_damage_threshold = 0.0f;
    float damage_taken = 0.0f;
    
    // Modifiers
    float slow_percent = 0.0f;      // For SLOW type
    float snare_percent = 0.0f;     // For SNARE type
    
    // Special properties
    bool is_hard_cc = true;         // Completely disables
    bool can_be_cleansed = true;    // Removable by abilities
    uint32_t cleanse_tier = 1;      // Required cleanse level
    
    // [SEQUENCE: 1734] Check if expired
    bool IsExpired() const {
        return std::chrono::system_clock::now() >= end_time;
    }
    
    // [SEQUENCE: 1735] Update remaining duration
    void UpdateDuration() {
        auto now = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - start_time
        );
        remaining_duration = base_duration - elapsed;
    }
    
    // [SEQUENCE: 1736] Check if should break
    bool ShouldBreak(float damage = 0.0f, bool moved = false, bool acted = false) {
        switch (break_type) {
            case CCBreakType::DAMAGE:
                return damage > 0;
            case CCBreakType::DAMAGE_THRESHOLD:
                damage_taken += damage;
                return damage_taken >= break_damage_threshold;
            case CCBreakType::MOVEMENT:
                return moved;
            case CCBreakType::ACTION:
                return acted;
            default:
                return false;
        }
    }
};

// [SEQUENCE: 1737] Diminishing returns tracker
class DiminishingReturns {
public:
    // [SEQUENCE: 1738] Add CC application
    void AddApplication(CrowdControlType type) {
        auto& dr = dr_stacks_[type];
        dr.count++;
        dr.last_application = std::chrono::system_clock::now();
        
        // Reset after window
        if (dr.count > 3) {
            dr.count = 1;  // Reset to first application
        }
    }
    
    // [SEQUENCE: 1739] Calculate duration modifier
    float GetDurationModifier(CrowdControlType type) {
        CleanupExpired();
        
        auto it = dr_stacks_.find(type);
        if (it == dr_stacks_.end()) {
            return 1.0f;  // Full duration
        }
        
        // Standard DR: 100% -> 50% -> 25% -> Immune
        switch (it->second.count) {
            case 0:
            case 1: return 1.0f;
            case 2: return 0.5f;
            case 3: return 0.25f;
            default: return 0.0f;  // Immune
        }
    }
    
    // [SEQUENCE: 1740] Check if immune
    bool IsImmune(CrowdControlType type) {
        auto it = dr_stacks_.find(type);
        return it != dr_stacks_.end() && it->second.count >= 4;
    }
    
private:
    struct DRStack {
        uint32_t count = 0;
        std::chrono::system_clock::time_point last_application;
    };
    
    std::unordered_map<CrowdControlType, DRStack> dr_stacks_;
    static constexpr auto DR_RESET_TIME = std::chrono::seconds(18);
    
    // [SEQUENCE: 1741] Clean up expired DR stacks
    void CleanupExpired() {
        auto now = std::chrono::system_clock::now();
        
        std::erase_if(dr_stacks_, [now, this](const auto& pair) {
            auto elapsed = now - pair.second.last_application;
            return elapsed > DR_RESET_TIME;
        });
    }
};

// [SEQUENCE: 1742] Crowd control state for an entity
class CrowdControlState {
public:
    // [SEQUENCE: 1743] Apply crowd control
    bool ApplyCC(const CrowdControlEffect& effect) {
        // Check immunity
        if (IsImmuneTo(effect.type)) {
            spdlog::info("Entity immune to CC type {}", static_cast<uint32_t>(effect.type));
            return false;
        }
        
        // Apply diminishing returns
        float dr_modifier = diminishing_returns_.GetDurationModifier(effect.type);
        if (dr_modifier <= 0.0f) {
            spdlog::info("Entity has full DR immunity to CC type {}", 
                        static_cast<uint32_t>(effect.type));
            return false;
        }
        
        // Create modified effect
        CrowdControlEffect modified_effect = effect;
        modified_effect.base_duration = std::chrono::milliseconds(
            static_cast<int64_t>(effect.base_duration.count() * dr_modifier)
        );
        modified_effect.remaining_duration = modified_effect.base_duration;
        modified_effect.end_time = modified_effect.start_time + modified_effect.base_duration;
        
        // Add to active effects
        active_effects_[effect.effect_id] = modified_effect;
        
        // Track DR
        diminishing_returns_.AddApplication(effect.type);
        
        // Update state flags
        UpdateStateFlags();
        
        spdlog::debug("Applied CC {} with {}% duration", 
                     static_cast<uint32_t>(effect.type), dr_modifier * 100);
        
        return true;
    }
    
    // [SEQUENCE: 1744] Remove crowd control
    bool RemoveCC(uint64_t effect_id) {
        auto it = active_effects_.find(effect_id);
        if (it == active_effects_.end()) {
            return false;
        }
        
        // Grant temporary immunity if applicable
        GrantImmunity(it->second.type);
        
        active_effects_.erase(it);
        UpdateStateFlags();
        
        return true;
    }
    
    // [SEQUENCE: 1745] Cleanse crowd control
    uint32_t CleanseCC(uint32_t cleanse_level = 1, uint32_t max_count = 1) {
        std::vector<uint64_t> to_remove;
        
        for (const auto& [id, effect] : active_effects_) {
            if (effect.can_be_cleansed && effect.cleanse_tier <= cleanse_level) {
                to_remove.push_back(id);
                if (to_remove.size() >= max_count) {
                    break;
                }
            }
        }
        
        for (uint64_t id : to_remove) {
            RemoveCC(id);
        }
        
        return static_cast<uint32_t>(to_remove.size());
    }
    
    // [SEQUENCE: 1746] Handle damage for break checks
    void OnDamageTaken(float damage) {
        std::vector<uint64_t> to_remove;
        
        for (auto& [id, effect] : active_effects_) {
            if (effect.ShouldBreak(damage)) {
                to_remove.push_back(id);
            }
        }
        
        for (uint64_t id : to_remove) {
            RemoveCC(id);
            spdlog::debug("CC {} broken by damage", id);
        }
    }
    
    // [SEQUENCE: 1747] Update active effects
    void Update() {
        // Remove expired effects
        std::vector<uint64_t> expired;
        
        for (auto& [id, effect] : active_effects_) {
            effect.UpdateDuration();
            if (effect.IsExpired()) {
                expired.push_back(id);
            }
        }
        
        for (uint64_t id : expired) {
            RemoveCC(id);
        }
        
        // Update immunities
        UpdateImmunities();
    }
    
    // [SEQUENCE: 1748] Check if can perform action
    bool CanMove() const {
        return !HasCCType(CrowdControlType::STUN) &&
               !HasCCType(CrowdControlType::ROOT) &&
               !HasCCType(CrowdControlType::SLEEP) &&
               !HasCCType(CrowdControlType::FREEZE);
    }
    
    bool CanCast() const {
        return !HasCCType(CrowdControlType::STUN) &&
               !HasCCType(CrowdControlType::SILENCE) &&
               !HasCCType(CrowdControlType::SLEEP) &&
               !HasCCType(CrowdControlType::POLYMORPH);
    }
    
    bool CanAttack() const {
        return !HasCCType(CrowdControlType::STUN) &&
               !HasCCType(CrowdControlType::DISARM) &&
               !HasCCType(CrowdControlType::SLEEP) &&
               !HasCCType(CrowdControlType::PACIFY);
    }
    
    bool CanUseAbilities() const {
        return !HasCCType(CrowdControlType::STUN) &&
               !HasCCType(CrowdControlType::SLEEP) &&
               !HasCCType(CrowdControlType::PACIFY);
    }
    
    // [SEQUENCE: 1749] Get movement speed modifier
    float GetMovementSpeedModifier() const {
        float modifier = 1.0f;
        
        for (const auto& [id, effect] : active_effects_) {
            if (effect.type == CrowdControlType::SLOW) {
                modifier *= (1.0f - effect.slow_percent / 100.0f);
            }
        }
        
        return modifier;
    }
    
    // [SEQUENCE: 1750] Get attack speed modifier
    float GetAttackSpeedModifier() const {
        float modifier = 1.0f;
        
        for (const auto& [id, effect] : active_effects_) {
            if (effect.type == CrowdControlType::SNARE) {
                modifier *= (1.0f - effect.snare_percent / 100.0f);
            }
        }
        
        return modifier;
    }
    
    // Queries
    bool HasCC() const { return !active_effects_.empty(); }
    bool HasHardCC() const {
        return std::any_of(active_effects_.begin(), active_effects_.end(),
            [](const auto& pair) { return pair.second.is_hard_cc; });
    }
    
    bool HasCCType(CrowdControlType type) const {
        return (current_cc_flags_ & static_cast<uint32_t>(type)) != 0;
    }
    
    std::vector<CrowdControlEffect> GetActiveEffects() const {
        std::vector<CrowdControlEffect> effects;
        for (const auto& [id, effect] : active_effects_) {
            effects.push_back(effect);
        }
        return effects;
    }
    
    // [SEQUENCE: 1751] Grant temporary immunity
    void GrantImmunity(CrowdControlType type, std::chrono::seconds duration = std::chrono::seconds(2)) {
        immunity_timers_[type] = std::chrono::system_clock::now() + duration;
    }
    
private:
    std::unordered_map<uint64_t, CrowdControlEffect> active_effects_;
    DiminishingReturns diminishing_returns_;
    uint32_t current_cc_flags_ = 0;
    
    // Immunity tracking
    std::unordered_map<CrowdControlType, std::chrono::system_clock::time_point> immunity_timers_;
    
    // [SEQUENCE: 1752] Update state flags
    void UpdateStateFlags() {
        current_cc_flags_ = 0;
        for (const auto& [id, effect] : active_effects_) {
            current_cc_flags_ |= static_cast<uint32_t>(effect.type);
        }
    }
    
    // [SEQUENCE: 1753] Check immunity
    bool IsImmuneTo(CrowdControlType type) const {
        auto it = immunity_timers_.find(type);
        if (it != immunity_timers_.end()) {
            return std::chrono::system_clock::now() < it->second;
        }
        return false;
    }
    
    // [SEQUENCE: 1754] Update immunities
    void UpdateImmunities() {
        auto now = std::chrono::system_clock::now();
        std::erase_if(immunity_timers_, [now](const auto& pair) {
            return now >= pair.second;
        });
    }
};

// [SEQUENCE: 1755] Crowd control manager
class CrowdControlManager {
public:
    static CrowdControlManager& Instance() {
        static CrowdControlManager instance;
        return instance;
    }
    
    // [SEQUENCE: 1756] Apply crowd control
    bool ApplyCC(uint64_t target_id, const CrowdControlEffect& effect) {
        auto& state = GetOrCreateState(target_id);
        
        bool success = state.ApplyCC(effect);
        
        if (success) {
            // Log for debugging
            spdlog::info("Applied {} to entity {} for {}ms",
                        static_cast<uint32_t>(effect.type),
                        target_id,
                        effect.base_duration.count());
            
            // TODO: Notify client for visual effects
        }
        
        return success;
    }
    
    // [SEQUENCE: 1757] Create standard CC effects
    static CrowdControlEffect CreateStun(uint64_t source_id, uint32_t ability_id, 
                                        std::chrono::milliseconds duration) {
        CrowdControlEffect effect;
        effect.effect_id = GenerateEffectId();
        effect.type = CrowdControlType::STUN;
        effect.source_id = source_id;
        effect.ability_id = ability_id;
        effect.base_duration = duration;
        effect.remaining_duration = duration;
        effect.start_time = std::chrono::system_clock::now();
        effect.end_time = effect.start_time + duration;
        effect.break_type = CCBreakType::NONE;
        effect.is_hard_cc = true;
        return effect;
    }
    
    static CrowdControlEffect CreateRoot(uint64_t source_id, uint32_t ability_id,
                                        std::chrono::milliseconds duration) {
        auto effect = CreateStun(source_id, ability_id, duration);
        effect.type = CrowdControlType::ROOT;
        effect.break_type = CCBreakType::DAMAGE_THRESHOLD;
        effect.break_damage_threshold = 100.0f;
        return effect;
    }
    
    static CrowdControlEffect CreateSlow(uint64_t source_id, uint32_t ability_id,
                                        std::chrono::milliseconds duration,
                                        float slow_percent) {
        auto effect = CreateStun(source_id, ability_id, duration);
        effect.type = CrowdControlType::SLOW;
        effect.slow_percent = slow_percent;
        effect.is_hard_cc = false;
        effect.break_type = CCBreakType::NONE;
        return effect;
    }
    
    // [SEQUENCE: 1758] Update all CC states
    void UpdateAll() {
        for (auto& [entity_id, state] : entity_states_) {
            state.Update();
        }
        
        // Clean up empty states
        std::erase_if(entity_states_, [](const auto& pair) {
            return !pair.second.HasCC();
        });
    }
    
    // [SEQUENCE: 1759] Get entity CC state
    CrowdControlState* GetState(uint64_t entity_id) {
        auto it = entity_states_.find(entity_id);
        return (it != entity_states_.end()) ? &it->second : nullptr;
    }
    
private:
    CrowdControlManager() = default;
    
    std::unordered_map<uint64_t, CrowdControlState> entity_states_;
    static std::atomic<uint64_t> next_effect_id_;
    
    CrowdControlState& GetOrCreateState(uint64_t entity_id) {
        return entity_states_[entity_id];
    }
    
    static uint64_t GenerateEffectId() {
        return next_effect_id_++;
    }
};

std::atomic<uint64_t> CrowdControlManager::next_effect_id_{1};

} // namespace mmorpg::game::combat