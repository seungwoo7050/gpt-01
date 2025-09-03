#include "skill_system.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace mmorpg::game::skills {

// [SEQUENCE: 921] SkillManager - Register skill
void SkillManager::RegisterSkill(const SkillData& skill_data) {
    skill_database_[skill_data.skill_id] = skill_data;
    spdlog::info("Registered skill: {} (ID: {})", skill_data.name, skill_data.skill_id);
}

// [SEQUENCE: 922] SkillManager - Get skill data
const SkillData* SkillManager::GetSkillData(uint32_t skill_id) const {
    auto it = skill_database_.find(skill_id);
    return (it != skill_database_.end()) ? &it->second : nullptr;
}

// [SEQUENCE: 923] SkillManager - Learn skill
bool SkillManager::LearnSkill(uint64_t entity_id, uint32_t skill_id) {
    const auto* skill_data = GetSkillData(skill_id);
    if (!skill_data) {
        spdlog::warn("Attempt to learn unknown skill: {}", skill_id);
        return false;
    }
    
    // Check if already learned
    if (HasSkill(entity_id, skill_id)) {
        spdlog::debug("Entity {} already knows skill {}", entity_id, skill_id);
        return false;
    }
    
    // TODO: Check level requirement, class requirement, etc.
    
    // Create skill instance
    SkillInstance instance;
    instance.skill_id = skill_id;
    instance.current_rank = 1;
    
    entity_skills_[entity_id][skill_id] = instance;
    
    spdlog::info("Entity {} learned skill: {}", entity_id, skill_data->name);
    return true;
}

// [SEQUENCE: 924] SkillManager - Upgrade skill
bool SkillManager::UpgradeSkill(uint64_t entity_id, uint32_t skill_id) {
    if (!HasSkill(entity_id, skill_id)) {
        return false;
    }
    
    const auto* skill_data = GetSkillData(skill_id);
    if (!skill_data) {
        return false;
    }
    
    auto& instance = entity_skills_[entity_id][skill_id];
    if (instance.current_rank >= skill_data->max_rank) {
        return false;
    }
    
    instance.current_rank++;
    spdlog::info("Entity {} upgraded skill {} to rank {}", 
                 entity_id, skill_data->name, instance.current_rank);
    return true;
}

// [SEQUENCE: 925] SkillManager - Has skill
bool SkillManager::HasSkill(uint64_t entity_id, uint32_t skill_id) const {
    auto entity_it = entity_skills_.find(entity_id);
    if (entity_it == entity_skills_.end()) {
        return false;
    }
    
    return entity_it->second.find(skill_id) != entity_it->second.end();
}

// [SEQUENCE: 926] SkillManager - Start cast
SkillCastResult SkillManager::StartCast(uint64_t caster_id, uint32_t skill_id,
                                       uint64_t target_id,
                                       float target_x, float target_y, float target_z) {
    SkillCastResult result;
    
    // [SEQUENCE: 927] Get skill data
    const auto* skill_data = GetSkillData(skill_id);
    if (!skill_data) {
        result.failure_reason = "Unknown skill";
        return result;
    }
    
    // [SEQUENCE: 928] Check if caster knows the skill
    if (!HasSkill(caster_id, skill_id)) {
        result.failure_reason = "Skill not learned";
        return result;
    }
    
    // [SEQUENCE: 929] Get skill instance
    auto& instance = entity_skills_[caster_id][skill_id];
    
    // [SEQUENCE: 930] Check cooldown
    if (instance.is_on_cooldown) {
        auto now = std::chrono::steady_clock::now();
        if (now < instance.cooldown_end) {
            result.failure_reason = "Skill is on cooldown";
            return result;
        }
        instance.is_on_cooldown = false;
    }
    
    // [SEQUENCE: 931] Check if already casting
    if (IsCasting(caster_id) || IsChanneling(caster_id)) {
        result.failure_reason = "Already casting";
        return result;
    }
    
    // [SEQUENCE: 932] Validate cast
    if (!ValidateCast(caster_id, *skill_data, target_id, result)) {
        return result;
    }
    
    // [SEQUENCE: 933] Handle different skill types
    if (skill_data->type == SkillType::INSTANT) {
        // Execute immediately
        ActiveCast cast;
        cast.skill_id = skill_id;
        cast.target_id = target_id;
        cast.target_x = target_x;
        cast.target_y = target_y;
        cast.target_z = target_z;
        
        ExecuteSkill(caster_id, *skill_data, instance, cast);
        
        // Apply cooldown
        auto now = std::chrono::steady_clock::now();
        instance.last_used = now;
        instance.cooldown_end = now + std::chrono::milliseconds(
            static_cast<int>(skill_data->cooldown * 1000)
        );
        instance.is_on_cooldown = true;
        
        // Apply global cooldown
        ApplyGlobalCooldown(caster_id, skill_data->global_cooldown);
        
        result.success = true;
        
    } else if (skill_data->type == SkillType::CAST_TIME) {
        // [SEQUENCE: 934] Start casting
        ActiveCast cast;
        cast.skill_id = skill_id;
        cast.target_id = target_id;
        cast.target_x = target_x;
        cast.target_y = target_y;
        cast.target_z = target_z;
        cast.start_time = std::chrono::steady_clock::now();
        cast.cast_time = skill_data->cast_time;
        cast.is_channeling = false;
        
        active_casts_[caster_id] = cast;
        instance.is_casting = true;
        instance.cast_progress = 0.0f;
        instance.current_target = target_id;
        
        spdlog::debug("Entity {} started casting {}", caster_id, skill_data->name);
        result.success = true;
        
    } else if (skill_data->type == SkillType::CHANNELING) {
        // [SEQUENCE: 935] Start channeling
        ActiveCast cast;
        cast.skill_id = skill_id;
        cast.target_id = target_id;
        cast.target_x = target_x;
        cast.target_y = target_y;
        cast.target_z = target_z;
        cast.start_time = std::chrono::steady_clock::now();
        cast.is_channeling = true;
        cast.channel_time_remaining = skill_data->channel_duration;
        
        active_casts_[caster_id] = cast;
        instance.is_channeling = true;
        instance.channel_progress = 0.0f;
        instance.current_target = target_id;
        
        // Consume initial resource
        ConsumeResource(caster_id, *skill_data);
        
        spdlog::debug("Entity {} started channeling {}", caster_id, skill_data->name);
        result.success = true;
    }
    
    return result;
}

// [SEQUENCE: 936] SkillManager - Cancel cast
void SkillManager::CancelCast(uint64_t caster_id) {
    auto cast_it = active_casts_.find(caster_id);
    if (cast_it == active_casts_.end()) {
        return;
    }
    
    auto skill_it = entity_skills_.find(caster_id);
    if (skill_it != entity_skills_.end()) {
        auto& skill_instance = skill_it->second[cast_it->second.skill_id];
        skill_instance.is_casting = false;
        skill_instance.is_channeling = false;
        skill_instance.cast_progress = 0.0f;
        skill_instance.channel_progress = 0.0f;
    }
    
    active_casts_.erase(cast_it);
    spdlog::debug("Entity {} cancelled cast", caster_id);
}

// [SEQUENCE: 937] SkillManager - Interrupt cast
void SkillManager::InterruptCast(uint64_t caster_id, uint32_t interrupt_flags) {
    auto cast_it = active_casts_.find(caster_id);
    if (cast_it == active_casts_.end()) {
        return;
    }
    
    const auto* skill_data = GetSkillData(cast_it->second.skill_id);
    if (!skill_data) {
        return;
    }
    
    // Check if this interrupt type affects the skill
    if (skill_data->interrupt_flags & interrupt_flags) {
        CancelCast(caster_id);
        spdlog::debug("Entity {} cast interrupted by flags: {}", caster_id, interrupt_flags);
    }
}

// [SEQUENCE: 938] SkillManager - Toggle skill
bool SkillManager::ToggleSkill(uint64_t caster_id, uint32_t skill_id) {
    const auto* skill_data = GetSkillData(skill_id);
    if (!skill_data || skill_data->type != SkillType::TOGGLE) {
        return false;
    }
    
    if (!HasSkill(caster_id, skill_id)) {
        return false;
    }
    
    auto& instance = entity_skills_[caster_id][skill_id];
    
    if (instance.is_toggled) {
        // Turn off
        instance.is_toggled = false;
        auto& toggles = active_toggles_[caster_id];
        toggles.erase(std::remove(toggles.begin(), toggles.end(), skill_id), toggles.end());
        
        spdlog::debug("Entity {} toggled off skill {}", caster_id, skill_data->name);
    } else {
        // Check resource
        if (!CheckResourceCost(caster_id, *skill_data)) {
            return false;
        }
        
        // Turn on
        instance.is_toggled = true;
        active_toggles_[caster_id].push_back(skill_id);
        
        spdlog::debug("Entity {} toggled on skill {}", caster_id, skill_data->name);
    }
    
    return true;
}

// [SEQUENCE: 939] SkillManager - Update
void SkillManager::Update(float delta_time) {
    // Update casts
    std::vector<uint64_t> completed_casts;
    
    for (auto& [caster_id, cast] : active_casts_) {
        if (cast.is_channeling) {
            UpdateChanneling(caster_id, cast, delta_time);
            if (cast.channel_time_remaining <= 0) {
                completed_casts.push_back(caster_id);
            }
        } else {
            UpdateCastProgress(caster_id, cast, delta_time);
            
            auto& instance = entity_skills_[caster_id][cast.skill_id];
            if (instance.cast_progress >= 1.0f) {
                // Cast completed
                const auto* skill_data = GetSkillData(cast.skill_id);
                if (skill_data) {
                    ExecuteSkill(caster_id, *skill_data, instance, cast);
                    
                    // Apply cooldown
                    auto now = std::chrono::steady_clock::now();
                    instance.last_used = now;
                    instance.cooldown_end = now + std::chrono::milliseconds(
                        static_cast<int>(skill_data->cooldown * 1000)
                    );
                    instance.is_on_cooldown = true;
                    
                    ApplyGlobalCooldown(caster_id, skill_data->global_cooldown);
                }
                
                completed_casts.push_back(caster_id);
            }
        }
    }
    
    // Remove completed casts
    for (uint64_t caster_id : completed_casts) {
        CancelCast(caster_id);
    }
    
    // Update toggles
    UpdateToggles(delta_time);
}

// [SEQUENCE: 940] SkillManager - Is on cooldown
bool SkillManager::IsOnCooldown(uint64_t entity_id, uint32_t skill_id) const {
    auto entity_it = entity_skills_.find(entity_id);
    if (entity_it == entity_skills_.end()) {
        return false;
    }
    
    auto skill_it = entity_it->second.find(skill_id);
    if (skill_it == entity_it->second.end()) {
        return false;
    }
    
    if (!skill_it->second.is_on_cooldown) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    return now < skill_it->second.cooldown_end;
}

// [SEQUENCE: 941] SkillManager - Get cooldown remaining
float SkillManager::GetCooldownRemaining(uint64_t entity_id, uint32_t skill_id) const {
    auto entity_it = entity_skills_.find(entity_id);
    if (entity_it == entity_skills_.end()) {
        return 0.0f;
    }
    
    auto skill_it = entity_it->second.find(skill_id);
    if (skill_it == entity_it->second.end()) {
        return 0.0f;
    }
    
    if (!skill_it->second.is_on_cooldown) {
        return 0.0f;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
        skill_it->second.cooldown_end - now
    ).count();
    
    return std::max(0.0f, remaining / 1000.0f);
}

// [SEQUENCE: 942] SkillManager - Reset cooldown
void SkillManager::ResetCooldown(uint64_t entity_id, uint32_t skill_id) {
    auto entity_it = entity_skills_.find(entity_id);
    if (entity_it == entity_skills_.end()) {
        return;
    }
    
    auto skill_it = entity_it->second.find(skill_id);
    if (skill_it == entity_it->second.end()) {
        return;
    }
    
    skill_it->second.is_on_cooldown = false;
}

// [SEQUENCE: 943] SkillManager - Is casting
bool SkillManager::IsCasting(uint64_t entity_id) const {
    auto it = active_casts_.find(entity_id);
    return it != active_casts_.end() && !it->second.is_channeling;
}

// [SEQUENCE: 944] SkillManager - Is channeling
bool SkillManager::IsChanneling(uint64_t entity_id) const {
    auto it = active_casts_.find(entity_id);
    return it != active_casts_.end() && it->second.is_channeling;
}

// [SEQUENCE: 945] SkillManager - Register skill effect
void SkillManager::RegisterSkillEffect(uint32_t skill_id, std::shared_ptr<ISkillEffect> effect) {
    skill_effects_[skill_id].push_back(effect);
}

// [SEQUENCE: 946] SkillManager - Validate cast
bool SkillManager::ValidateCast(uint64_t caster_id, const SkillData& skill,
                               uint64_t target_id, SkillCastResult& result) {
    // Check resource cost
    if (!CheckResourceCost(caster_id, skill)) {
        result.failure_reason = "Not enough resource";
        return false;
    }
    
    // Check target requirement
    switch (skill.target_requirement) {
        case SkillTargetRequirement::REQUIRES_ENEMY:
            if (target_id == 0 || target_id == caster_id) {
                result.failure_reason = "Requires enemy target";
                return false;
            }
            break;
            
        case SkillTargetRequirement::REQUIRES_ALLY:
            // TODO: Check faction
            if (target_id == 0) {
                result.failure_reason = "Requires ally target";
                return false;
            }
            break;
            
        case SkillTargetRequirement::REQUIRES_SELF:
            if (target_id != caster_id) {
                result.failure_reason = "Can only target self";
                return false;
            }
            break;
    }
    
    // TODO: Check range, line of sight, etc.
    
    return true;
}

// [SEQUENCE: 947] SkillManager - Check resource cost
bool SkillManager::CheckResourceCost(uint64_t caster_id, const SkillData& skill) {
    // TODO: Get entity's resource from combat component
    // For now, assume enough resource
    return true;
}

// [SEQUENCE: 948] SkillManager - Consume resource
void SkillManager::ConsumeResource(uint64_t caster_id, const SkillData& skill) {
    // TODO: Actually consume resource from entity
    spdlog::debug("Entity {} consumed {} {} for skill", 
                  caster_id, skill.resource_cost, static_cast<int>(skill.resource_type));
}

// [SEQUENCE: 949] SkillManager - Execute skill
void SkillManager::ExecuteSkill(uint64_t caster_id, const SkillData& skill,
                               const SkillInstance& instance, const ActiveCast& cast) {
    spdlog::debug("Entity {} executed skill: {}", caster_id, skill.name);
    
    // Consume resource (for non-channeling skills)
    if (skill.type != SkillType::CHANNELING) {
        ConsumeResource(caster_id, skill);
    }
    
    // Apply skill effects
    auto effect_it = skill_effects_.find(skill.skill_id);
    if (effect_it != skill_effects_.end()) {
        for (const auto& effect : effect_it->second) {
            if (skill.target_type == combat::TargetType::SINGLE_ENEMY ||
                skill.target_type == combat::TargetType::SINGLE_ALLY ||
                skill.target_type == combat::TargetType::SELF) {
                effect->OnApply(caster_id, cast.target_id, instance.current_rank);
            } else {
                // AoE effects
                auto targets = SkillUtils::GetAoETargets(
                    caster_id, cast.target_x, cast.target_y, cast.target_z,
                    skill.radius, skill.target_type
                );
                
                for (uint64_t target_id : targets) {
                    effect->OnApply(caster_id, target_id, instance.current_rank);
                }
            }
        }
    }
    
    // Direct damage/healing (if no custom effects)
    if (skill.base_damage > 0) {
        combat::CombatManager::Instance().ExecuteAttack(caster_id, cast.target_id);
    }
}

// [SEQUENCE: 950] SkillManager - Update cast progress
void SkillManager::UpdateCastProgress(uint64_t caster_id, ActiveCast& cast, float delta_time) {
    auto& instance = entity_skills_[caster_id][cast.skill_id];
    
    float elapsed = std::chrono::duration<float>(
        std::chrono::steady_clock::now() - cast.start_time
    ).count();
    
    instance.cast_progress = std::min(1.0f, elapsed / cast.cast_time);
}

// [SEQUENCE: 951] SkillManager - Update channeling
void SkillManager::UpdateChanneling(uint64_t caster_id, ActiveCast& cast, float delta_time) {
    cast.channel_time_remaining -= delta_time;
    
    const auto* skill_data = GetSkillData(cast.skill_id);
    if (!skill_data) {
        return;
    }
    
    auto& instance = entity_skills_[caster_id][cast.skill_id];
    instance.channel_progress = 1.0f - (cast.channel_time_remaining / skill_data->channel_duration);
    
    // Consume resource per second
    if (skill_data->resource_cost_per_second > 0) {
        // TODO: Check and consume resource
        // If not enough resource, cancel channel
    }
    
    // Apply channeling effects
    auto effect_it = skill_effects_.find(cast.skill_id);
    if (effect_it != skill_effects_.end()) {
        for (const auto& effect : effect_it->second) {
            effect->OnChannelTick(caster_id, cast.target_id, delta_time, instance.current_rank);
        }
    }
}

// [SEQUENCE: 952] SkillManager - Update toggles
void SkillManager::UpdateToggles(float delta_time) {
    for (const auto& [entity_id, toggle_skills] : active_toggles_) {
        for (uint32_t skill_id : toggle_skills) {
            const auto* skill_data = GetSkillData(skill_id);
            if (!skill_data) {
                continue;
            }
            
            // Check and consume resource per second
            if (skill_data->resource_cost_per_second > 0) {
                // TODO: Check and consume resource
                // If not enough, toggle off
            }
        }
    }
}

// [SEQUENCE: 953] DamageSkillEffect - OnApply
void DamageSkillEffect::OnApply(uint64_t caster_id, uint64_t target_id, uint32_t skill_rank) {
    float total_damage = base_damage_ + (damage_per_rank_ * (skill_rank - 1));
    
    // Use combat manager to apply damage
    auto& combat_mgr = combat::CombatManager::Instance();
    combat_mgr.ExecuteAttack(caster_id, target_id);  // TODO: Pass skill damage
}

// [SEQUENCE: 954] SkillFactory - Create damage skill
SkillData SkillFactory::CreateDamageSkill(
    uint32_t skill_id,
    const std::string& name,
    float base_damage,
    combat::DamageType damage_type,
    float range,
    float cooldown,
    float mana_cost) {
    
    SkillData skill;
    skill.skill_id = skill_id;
    skill.name = name;
    skill.type = SkillType::INSTANT;
    skill.target_requirement = SkillTargetRequirement::REQUIRES_ENEMY;
    skill.target_type = combat::TargetType::SINGLE_ENEMY;
    skill.range = range;
    skill.base_damage = base_damage;
    skill.damage_type = damage_type;
    skill.cooldown = cooldown;
    skill.resource_type = ResourceType::MANA;
    skill.resource_cost = mana_cost;
    
    return skill;
}

// [SEQUENCE: 955] SkillUtils - Get AoE targets
std::vector<uint64_t> SkillUtils::GetAoETargets(
    uint64_t caster_id,
    float center_x, float center_y, float center_z,
    float radius, combat::TargetType target_type) {
    
    // Use combat manager's method
    return combat::CombatManager::Instance().GetEntitiesInRange(
        center_x, center_y, center_z, radius, target_type
    );
}

} // namespace mmorpg::game::skills