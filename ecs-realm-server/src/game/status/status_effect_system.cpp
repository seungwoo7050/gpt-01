#include "status_effect_system.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace mmorpg::game::status {

// [SEQUENCE: 987] StatusEffectManager - Register effect
void StatusEffectManager::RegisterEffect(const StatusEffectData& effect_data) {
    effect_database_[effect_data.effect_id] = effect_data;
    spdlog::info("Registered status effect: {} (ID: {})", effect_data.name, effect_data.effect_id);
}

// [SEQUENCE: 988] StatusEffectManager - Get effect data
const StatusEffectData* StatusEffectManager::GetEffectData(uint32_t effect_id) const {
    auto it = effect_database_.find(effect_id);
    return (it != effect_database_.end()) ? &it->second : nullptr;
}

// [SEQUENCE: 989] StatusEffectManager - Apply effect
bool StatusEffectManager::ApplyEffect(uint64_t target_id, uint32_t effect_id,
                                     uint64_t caster_id, float duration_modifier) {
    const auto* effect_data = GetEffectData(effect_id);
    if (!effect_data) {
        spdlog::warn("Attempt to apply unknown effect: {}", effect_id);
        return false;
    }
    
    // [SEQUENCE: 990] Check immunity
    if (IsImmuneToEffect(target_id, effect_id) || 
        IsImmuneToCategory(target_id, effect_data->category)) {
        spdlog::debug("Target {} is immune to effect {}", target_id, effect_id);
        return false;
    }
    
    // [SEQUENCE: 991] Check if can apply
    if (!CanApplyEffect(target_id, *effect_data)) {
        return false;
    }
    
    // [SEQUENCE: 992] Handle stacking
    auto* existing = FindEffect(target_id, effect_id, 
                               effect_data->stack_behavior == StackBehavior::UNIQUE_SOURCE ? caster_id : 0);
    
    if (existing) {
        switch (effect_data->stack_behavior) {
            case StackBehavior::NONE:
                // Refresh duration
                existing->expire_time = std::chrono::steady_clock::now() + 
                    std::chrono::milliseconds(static_cast<int>(effect_data->base_duration * duration_modifier * 1000));
                return true;
                
            case StackBehavior::STACK_DURATION:
                // Add to duration
                existing->expire_time += std::chrono::milliseconds(
                    static_cast<int>(effect_data->base_duration * duration_modifier * 1000));
                return true;
                
            case StackBehavior::STACK_INTENSITY:
                // Add stack if not at max
                if (existing->current_stacks < effect_data->max_stacks) {
                    existing->current_stacks++;
                    existing->stack_multiplier = static_cast<float>(existing->current_stacks);
                }
                return true;
                
            case StackBehavior::STACK_REFRESH:
                // Add stack and refresh duration
                if (existing->current_stacks < effect_data->max_stacks) {
                    existing->current_stacks++;
                    existing->stack_multiplier = static_cast<float>(existing->current_stacks);
                }
                existing->expire_time = std::chrono::steady_clock::now() + 
                    std::chrono::milliseconds(static_cast<int>(effect_data->base_duration * duration_modifier * 1000));
                return true;
        }
    }
    
    // [SEQUENCE: 993] Create new effect instance
    StatusEffectInstance instance;
    instance.effect_id = effect_id;
    instance.caster_id = caster_id;
    instance.apply_time = std::chrono::steady_clock::now();
    instance.last_tick = instance.apply_time;
    
    if (effect_data->base_duration > 0) {
        instance.expire_time = instance.apply_time + 
            std::chrono::milliseconds(static_cast<int>(effect_data->base_duration * duration_modifier * 1000));
    } else {
        // Permanent effect
        instance.expire_time = std::chrono::steady_clock::time_point::max();
    }
    
    // [SEQUENCE: 994] Apply stat modifiers
    ApplyStatModifiers(target_id, *effect_data, instance);
    
    // [SEQUENCE: 995] Apply immunities granted by this effect
    if (!effect_data->immunity_categories.empty() || !effect_data->immunity_effect_ids.empty()) {
        auto& immunity = immunities_[target_id];
        immunity.category_immunities.insert(immunity.category_immunities.end(),
                                          effect_data->immunity_categories.begin(),
                                          effect_data->immunity_categories.end());
        immunity.effect_id_immunities.insert(immunity.effect_id_immunities.end(),
                                           effect_data->immunity_effect_ids.begin(),
                                           effect_data->immunity_effect_ids.end());
    }
    
    // Add to active effects
    active_effects_[target_id].push_back(instance);
    
    spdlog::debug("Applied effect {} to target {} from caster {}", 
                  effect_data->name, target_id, caster_id);
    
    // TODO: Trigger effect applied event
    
    return true;
}

// [SEQUENCE: 996] StatusEffectManager - Remove effect
void StatusEffectManager::RemoveEffect(uint64_t target_id, uint32_t effect_id, uint64_t caster_id) {
    auto target_it = active_effects_.find(target_id);
    if (target_it == active_effects_.end()) {
        return;
    }
    
    auto& effects = target_it->second;
    auto remove_it = std::remove_if(effects.begin(), effects.end(),
        [effect_id, caster_id](const StatusEffectInstance& instance) {
            return instance.effect_id == effect_id && 
                   (caster_id == 0 || instance.caster_id == caster_id);
        });
    
    // Process removal
    for (auto it = remove_it; it != effects.end(); ++it) {
        const auto* effect_data = GetEffectData(it->effect_id);
        if (effect_data) {
            RemoveStatModifiers(target_id, *effect_data, *it);
            OnEffectExpired(target_id, *it);
        }
    }
    
    effects.erase(remove_it, effects.end());
    
    // Clean up if no effects remain
    if (effects.empty()) {
        active_effects_.erase(target_it);
    }
}

// [SEQUENCE: 997] StatusEffectManager - Remove all effects
void StatusEffectManager::RemoveAllEffects(uint64_t target_id) {
    auto target_it = active_effects_.find(target_id);
    if (target_it == active_effects_.end()) {
        return;
    }
    
    // Remove stat modifiers for all effects
    for (const auto& instance : target_it->second) {
        const auto* effect_data = GetEffectData(instance.effect_id);
        if (effect_data) {
            RemoveStatModifiers(target_id, *effect_data, instance);
            OnEffectExpired(target_id, instance);
        }
    }
    
    active_effects_.erase(target_it);
    immunities_.erase(target_id);
}

// [SEQUENCE: 998] StatusEffectManager - Dispel magic
uint32_t StatusEffectManager::DispelMagic(uint64_t target_id, bool friendly, uint32_t count) {
    auto target_it = active_effects_.find(target_id);
    if (target_it == active_effects_.end()) {
        return 0;
    }
    
    uint32_t dispelled = 0;
    auto& effects = target_it->second;
    
    // Collect dispellable effects
    std::vector<size_t> dispel_indices;
    for (size_t i = 0; i < effects.size() && dispelled < count; ++i) {
        const auto* effect_data = GetEffectData(effects[i].effect_id);
        if (!effect_data || effect_data->category != EffectCategory::MAGIC) {
            continue;
        }
        
        // Friendly dispel removes debuffs, hostile dispel removes buffs
        if ((friendly && effect_data->type == EffectType::DEBUFF) ||
            (!friendly && effect_data->type == EffectType::BUFF)) {
            dispel_indices.push_back(i);
            dispelled++;
        }
    }
    
    // Remove in reverse order to maintain indices
    for (auto it = dispel_indices.rbegin(); it != dispel_indices.rend(); ++it) {
        const auto& instance = effects[*it];
        const auto* effect_data = GetEffectData(instance.effect_id);
        if (effect_data) {
            RemoveStatModifiers(target_id, *effect_data, instance);
            OnEffectExpired(target_id, instance);
        }
        effects.erase(effects.begin() + *it);
    }
    
    return dispelled;
}

// [SEQUENCE: 999] StatusEffectManager - Get active effects
std::vector<StatusEffectInstance> StatusEffectManager::GetActiveEffects(uint64_t target_id) const {
    auto it = active_effects_.find(target_id);
    return (it != active_effects_.end()) ? it->second : std::vector<StatusEffectInstance>{};
}

// [SEQUENCE: 1000] StatusEffectManager - Has effect
bool StatusEffectManager::HasEffect(uint64_t target_id, uint32_t effect_id) const {
    auto target_it = active_effects_.find(target_id);
    if (target_it == active_effects_.end()) {
        return false;
    }
    
    return std::any_of(target_it->second.begin(), target_it->second.end(),
        [effect_id](const StatusEffectInstance& instance) {
            return instance.effect_id == effect_id && instance.is_active;
        });
}

// [SEQUENCE: 1001] StatusEffectManager - Get control flags
uint32_t StatusEffectManager::GetControlFlags(uint64_t target_id) const {
    auto target_it = active_effects_.find(target_id);
    if (target_it == active_effects_.end()) {
        return 0;
    }
    
    uint32_t flags = 0;
    for (const auto& instance : target_it->second) {
        if (!instance.is_active) continue;
        
        const auto* effect_data = GetEffectData(instance.effect_id);
        if (effect_data) {
            flags |= effect_data->control_flags;
        }
    }
    
    return flags;
}

// [SEQUENCE: 1002] StatusEffectManager - Is stunned
bool StatusEffectManager::IsStunned(uint64_t target_id) const {
    return GetControlFlags(target_id) & static_cast<uint32_t>(ControlEffect::STUN);
}

// [SEQUENCE: 1003] StatusEffectManager - Is silenced
bool StatusEffectManager::IsSilenced(uint64_t target_id) const {
    return GetControlFlags(target_id) & static_cast<uint32_t>(ControlEffect::SILENCE);
}

// [SEQUENCE: 1004] StatusEffectManager - Is rooted
bool StatusEffectManager::IsRooted(uint64_t target_id) const {
    return GetControlFlags(target_id) & static_cast<uint32_t>(ControlEffect::ROOT);
}

// [SEQUENCE: 1005] StatusEffectManager - Update
void StatusEffectManager::Update(float delta_time) {
    auto now = std::chrono::steady_clock::now();
    std::vector<std::pair<uint64_t, uint32_t>> to_remove;
    
    for (auto& [target_id, effects] : active_effects_) {
        for (auto& instance : effects) {
            if (!instance.is_active) continue;
            
            const auto* effect_data = GetEffectData(instance.effect_id);
            if (!effect_data) continue;
            
            // [SEQUENCE: 1006] Check expiration
            if (instance.expire_time != std::chrono::steady_clock::time_point::max() &&
                now >= instance.expire_time) {
                instance.is_active = false;
                to_remove.emplace_back(target_id, instance.effect_id);
                continue;
            }
            
            // [SEQUENCE: 1007] Process periodic effects
            if (effect_data->tick_interval > 0) {
                auto time_since_tick = std::chrono::duration<float>(now - instance.last_tick).count();
                if (time_since_tick >= effect_data->tick_interval) {
                    ProcessTick(target_id, instance, *effect_data, delta_time);
                    instance.last_tick = now;
                }
            }
        }
    }
    
    // Remove expired effects
    for (const auto& [target_id, effect_id] : to_remove) {
        RemoveEffect(target_id, effect_id);
    }
}

// [SEQUENCE: 1008] StatusEffectManager - Get total stat modifier
float StatusEffectManager::GetTotalStatModifier(uint64_t target_id, const std::string& stat_name) const {
    auto target_it = active_effects_.find(target_id);
    if (target_it == active_effects_.end()) {
        return 0.0f;
    }
    
    float flat_bonus = 0.0f;
    float percent_bonus = 0.0f;
    float multiplier = 1.0f;
    
    for (const auto& instance : target_it->second) {
        if (!instance.is_active) continue;
        
        const auto* effect_data = GetEffectData(instance.effect_id);
        if (!effect_data) continue;
        
        for (const auto& modifier : effect_data->stat_modifiers) {
            if (modifier.stat_name != stat_name) continue;
            
            float value = modifier.value * instance.stack_multiplier;
            
            switch (modifier.type) {
                case StatModifierType::FLAT:
                    flat_bonus += value;
                    break;
                case StatModifierType::PERCENTAGE:
                    percent_bonus += value;
                    break;
                case StatModifierType::MULTIPLIER:
                    multiplier *= value;
                    break;
            }
        }
    }
    
    // Calculate final modifier
    // Formula: (base + flat) * (1 + percent/100) * multiplier
    // This returns the total bonus to be added to base stat
    return flat_bonus + (percent_bonus / 100.0f) * multiplier;
}

// [SEQUENCE: 1009] StatusEffectManager - Can apply effect
bool StatusEffectManager::CanApplyEffect(uint64_t target_id, const StatusEffectData& effect) const {
    // Check control immunity during certain effects
    if (effect.control_flags != 0) {
        // Some effects grant control immunity
        auto target_it = active_effects_.find(target_id);
        if (target_it != active_effects_.end()) {
            for (const auto& instance : target_it->second) {
                const auto* existing_effect = GetEffectData(instance.effect_id);
                if (existing_effect && existing_effect->control_flags & static_cast<uint32_t>(ControlEffect::STUN)) {
                    // Already stunned, can't apply more control effects
                    // (This is a simplified rule, games may have different mechanics)
                }
            }
        }
    }
    
    return true;
}

// [SEQUENCE: 1010] StatusEffectManager - Apply stat modifiers
void StatusEffectManager::ApplyStatModifiers(uint64_t target_id, const StatusEffectData& effect,
                                            const StatusEffectInstance& instance) {
    // TODO: Actually modify entity stats through combat system
    for (const auto& modifier : effect.stat_modifiers) {
        float value = modifier.value * instance.stack_multiplier;
        spdlog::debug("Applied {} {} to stat {} for entity {}", 
                     value, static_cast<int>(modifier.type), modifier.stat_name, target_id);
    }
}

// [SEQUENCE: 1011] StatusEffectManager - Remove stat modifiers
void StatusEffectManager::RemoveStatModifiers(uint64_t target_id, const StatusEffectData& effect,
                                             const StatusEffectInstance& instance) {
    // TODO: Actually remove modifications from entity stats
    for (const auto& modifier : effect.stat_modifiers) {
        float value = modifier.value * instance.stack_multiplier;
        spdlog::debug("Removed {} {} from stat {} for entity {}", 
                     value, static_cast<int>(modifier.type), modifier.stat_name, target_id);
    }
}

// [SEQUENCE: 1012] StatusEffectManager - Process tick
void StatusEffectManager::ProcessTick(uint64_t target_id, StatusEffectInstance& instance,
                                     const StatusEffectData& effect, float delta_time) {
    // Apply tick damage
    if (effect.tick_damage > 0) {
        float damage = effect.tick_damage * instance.stack_multiplier * instance.power_coefficient;
        // TODO: Apply damage through combat system
        spdlog::debug("Effect {} ticked for {} damage on entity {}", 
                     effect.name, damage, target_id);
    }
    
    // Apply tick healing
    if (effect.tick_healing > 0) {
        float healing = effect.tick_healing * instance.stack_multiplier * instance.power_coefficient;
        // TODO: Apply healing through combat system
        spdlog::debug("Effect {} ticked for {} healing on entity {}", 
                     effect.name, healing, target_id);
    }
}

// [SEQUENCE: 1013] StatusEffectManager - Find effect
StatusEffectInstance* StatusEffectManager::FindEffect(uint64_t target_id, uint32_t effect_id,
                                                     uint64_t caster_id) {
    auto target_it = active_effects_.find(target_id);
    if (target_it == active_effects_.end()) {
        return nullptr;
    }
    
    for (auto& instance : target_it->second) {
        if (instance.effect_id == effect_id && instance.is_active &&
            (caster_id == 0 || instance.caster_id == caster_id)) {
            return &instance;
        }
    }
    
    return nullptr;
}

// [SEQUENCE: 1014] StatusEffectManager - On effect expired
void StatusEffectManager::OnEffectExpired(uint64_t target_id, const StatusEffectInstance& instance) {
    const auto* effect_data = GetEffectData(instance.effect_id);
    if (!effect_data) return;
    
    spdlog::debug("Effect {} expired on entity {}", effect_data->name, target_id);
    
    // Remove immunities granted by this effect
    if (!effect_data->immunity_categories.empty() || !effect_data->immunity_effect_ids.empty()) {
        auto immunity_it = immunities_.find(target_id);
        if (immunity_it != immunities_.end()) {
            auto& immunity = immunity_it->second;
            
            // Remove category immunities
            for (const auto& cat : effect_data->immunity_categories) {
                immunity.category_immunities.erase(
                    std::remove(immunity.category_immunities.begin(), 
                               immunity.category_immunities.end(), cat),
                    immunity.category_immunities.end()
                );
            }
            
            // Remove effect ID immunities
            for (const auto& id : effect_data->immunity_effect_ids) {
                immunity.effect_id_immunities.erase(
                    std::remove(immunity.effect_id_immunities.begin(),
                               immunity.effect_id_immunities.end(), id),
                    immunity.effect_id_immunities.end()
                );
            }
            
            // Clean up if empty
            if (immunity.category_immunities.empty() && immunity.effect_id_immunities.empty()) {
                immunities_.erase(immunity_it);
            }
        }
    }
}

// [SEQUENCE: 1015] StatusEffectFactory - Create stat buff
StatusEffectData StatusEffectFactory::CreateStatBuff(
    uint32_t effect_id,
    const std::string& name,
    const std::string& stat_name,
    float value,
    StatModifierType mod_type,
    float duration) {
    
    StatusEffectData effect;
    effect.effect_id = effect_id;
    effect.name = name;
    effect.type = EffectType::BUFF;
    effect.category = EffectCategory::MAGIC;
    effect.base_duration = duration;
    
    StatusEffectData::StatModifier modifier;
    modifier.stat_name = stat_name;
    modifier.value = value;
    modifier.type = mod_type;
    effect.stat_modifiers.push_back(modifier);
    
    return effect;
}

// [SEQUENCE: 1016] StatusEffectFactory - Create DoT
StatusEffectData StatusEffectFactory::CreateDoT(
    uint32_t effect_id,
    const std::string& name,
    float damage_per_tick,
    float tick_interval,
    float duration,
    EffectCategory category) {
    
    StatusEffectData effect;
    effect.effect_id = effect_id;
    effect.name = name;
    effect.type = EffectType::DOT;
    effect.category = category;
    effect.base_duration = duration;
    effect.tick_interval = tick_interval;
    effect.tick_damage = damage_per_tick;
    
    return effect;
}

// [SEQUENCE: 1017] StatusEffectFactory - Create control effect
StatusEffectData StatusEffectFactory::CreateControlEffect(
    uint32_t effect_id,
    const std::string& name,
    ControlEffect control_type,
    float duration,
    bool breaks_on_damage) {
    
    StatusEffectData effect;
    effect.effect_id = effect_id;
    effect.name = name;
    effect.type = EffectType::CROWD_CONTROL;
    effect.category = EffectCategory::MAGIC;
    effect.base_duration = duration;
    effect.control_flags = static_cast<uint32_t>(control_type);
    effect.remove_on_damage = breaks_on_damage;
    
    return effect;
}

} // namespace mmorpg::game::status