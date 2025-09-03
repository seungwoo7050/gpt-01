#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>
#include <chrono>
#include <optional>
#include "../combat/combat_system.h"

namespace mmorpg::game::status {

// [SEQUENCE: 956] Status effect system
// 버프, 디버프, DoT/HoT 등 다양한 상태 효과를 관리하는 시스템

// [SEQUENCE: 957] Effect types
enum class EffectType {
    BUFF,           // 긍정적 효과
    DEBUFF,         // 부정적 효과
    DOT,            // Damage over Time
    HOT,            // Heal over Time
    CROWD_CONTROL,  // 행동 제한 (기절, 침묵 등)
    AURA,           // 오라 효과
    SHIELD,         // 보호막
    TRANSFORM       // 변신
};

// [SEQUENCE: 958] Effect category for dispel/cleanse
enum class EffectCategory {
    MAGIC,          // 마법 효과
    PHYSICAL,       // 물리 효과
    POISON,         // 독
    DISEASE,        // 질병
    CURSE,          // 저주
    BLESSING,       // 축복
    NONE            // 해제 불가
};

// [SEQUENCE: 959] Stat modifiers
enum class StatModifierType {
    FLAT,           // 고정값 증가 (+100)
    PERCENTAGE,     // 퍼센트 증가 (+20%)
    MULTIPLIER      // 곱연산 (x1.5)
};

// [SEQUENCE: 960] Stack behavior
enum class StackBehavior {
    NONE,           // 스택 불가 (새로 적용 시 갱신)
    STACK_DURATION, // 지속 시간 누적
    STACK_INTENSITY,// 효과 강도 누적
    STACK_REFRESH,  // 스택 추가 시 지속 시간 갱신
    UNIQUE_SOURCE   // 시전자별로 별도 스택
};

// [SEQUENCE: 961] Control effects
enum class ControlEffect {
    NONE = 0,
    STUN = 1 << 0,          // 기절 (모든 행동 불가)
    SILENCE = 1 << 1,       // 침묵 (스킬 사용 불가)
    ROOT = 1 << 2,          // 속박 (이동 불가)
    SLOW = 1 << 3,          // 둔화 (이동 속도 감소)
    DISARM = 1 << 4,        // 무장 해제 (자동 공격 불가)
    BLIND = 1 << 5,         // 실명 (명중률 감소)
    FEAR = 1 << 6,          // 공포 (무작위 이동)
    CHARM = 1 << 7,         // 매혹 (적 공격)
    SLEEP = 1 << 8,         // 수면 (피격 시 해제)
    FREEZE = 1 << 9         // 빙결 (행동 불가, 피해 감소)
};

// [SEQUENCE: 962] Status effect definition
struct StatusEffectData {
    uint32_t effect_id = 0;
    std::string name;
    std::string description;
    
    // Basic properties
    EffectType type = EffectType::BUFF;
    EffectCategory category = EffectCategory::MAGIC;
    uint32_t max_stacks = 1;
    StackBehavior stack_behavior = StackBehavior::NONE;
    
    // Duration
    float base_duration = 0.0f;     // Seconds (0 = permanent)
    bool is_channeled = false;       // Removed when caster stops
    
    // Periodic effects
    float tick_interval = 0.0f;      // Seconds between ticks
    float tick_damage = 0.0f;        // Damage per tick
    float tick_healing = 0.0f;       // Healing per tick
    
    // Control effects
    uint32_t control_flags = 0;      // ControlEffect flags
    
    // Stat modifications
    struct StatModifier {
        std::string stat_name;
        float value;
        StatModifierType type = StatModifierType::FLAT;
    };
    std::vector<StatModifier> stat_modifiers;
    
    // Immunity and resistance
    std::vector<EffectCategory> immunity_categories;
    std::vector<uint32_t> immunity_effect_ids;
    
    // Visual/Audio (클라이언트 힌트)
    std::string icon_name;
    std::string particle_effect;
    std::string apply_sound;
    std::string ambient_sound;
    
    // Removal conditions
    bool remove_on_damage = false;   // 피격 시 제거
    bool remove_on_action = false;   // 행동 시 제거
    bool persist_through_death = false; // 사망 후 유지
};

// [SEQUENCE: 963] Active status effect instance
struct StatusEffectInstance {
    uint32_t effect_id = 0;
    uint64_t caster_id = 0;
    
    // Timing
    std::chrono::steady_clock::time_point apply_time;
    std::chrono::steady_clock::time_point expire_time;
    std::chrono::steady_clock::time_point last_tick;
    
    // Stack info
    uint32_t current_stacks = 1;
    float stack_multiplier = 1.0f;
    
    // State
    bool is_active = true;
    bool is_hidden = false;          // 숨김 효과 (표시 안 됨)
    
    // Power scaling
    float power_coefficient = 1.0f;   // 시전자 능력치 기반 계수
    
    // Custom data
    std::unordered_map<std::string, float> custom_values;
};

// [SEQUENCE: 964] Status effect manager
class StatusEffectManager {
public:
    static StatusEffectManager& Instance() {
        static StatusEffectManager instance;
        return instance;
    }
    
    // [SEQUENCE: 965] Register effect definitions
    void RegisterEffect(const StatusEffectData& effect_data);
    const StatusEffectData* GetEffectData(uint32_t effect_id) const;
    
    // [SEQUENCE: 966] Apply effects
    bool ApplyEffect(uint64_t target_id, uint32_t effect_id, 
                    uint64_t caster_id = 0, float duration_modifier = 1.0f);
    bool ApplyEffectStacks(uint64_t target_id, uint32_t effect_id,
                          uint64_t caster_id, uint32_t stack_count);
    
    // [SEQUENCE: 967] Remove effects
    void RemoveEffect(uint64_t target_id, uint32_t effect_id, uint64_t caster_id = 0);
    void RemoveAllEffects(uint64_t target_id);
    void RemoveEffectsByCategory(uint64_t target_id, EffectCategory category);
    void RemoveDebuffs(uint64_t target_id, uint32_t count = 1);
    
    // [SEQUENCE: 968] Dispel/Purge
    uint32_t DispelMagic(uint64_t target_id, bool friendly, uint32_t count = 1);
    uint32_t CleansePoisonDisease(uint64_t target_id);
    uint32_t RemoveCurse(uint64_t target_id);
    
    // [SEQUENCE: 969] Query effects
    std::vector<StatusEffectInstance> GetActiveEffects(uint64_t target_id) const;
    bool HasEffect(uint64_t target_id, uint32_t effect_id) const;
    bool HasEffectCategory(uint64_t target_id, EffectCategory category) const;
    uint32_t GetEffectStacks(uint64_t target_id, uint32_t effect_id) const;
    
    // [SEQUENCE: 970] Control effect queries
    bool IsStunned(uint64_t target_id) const;
    bool IsSilenced(uint64_t target_id) const;
    bool IsRooted(uint64_t target_id) const;
    uint32_t GetControlFlags(uint64_t target_id) const;
    
    // [SEQUENCE: 971] Immunity checks
    bool IsImmuneToEffect(uint64_t target_id, uint32_t effect_id) const;
    bool IsImmuneToCategory(uint64_t target_id, EffectCategory category) const;
    
    // [SEQUENCE: 972] Update system
    void Update(float delta_time);
    
    // [SEQUENCE: 973] Effect modification
    void ExtendDuration(uint64_t target_id, uint32_t effect_id, float seconds);
    void ModifyStacks(uint64_t target_id, uint32_t effect_id, int32_t stack_change);
    
    // [SEQUENCE: 974] Get total stat modifiers
    float GetTotalStatModifier(uint64_t target_id, const std::string& stat_name) const;
    
private:
    StatusEffectManager() = default;
    
    // [SEQUENCE: 975] Effect database
    std::unordered_map<uint32_t, StatusEffectData> effect_database_;
    
    // [SEQUENCE: 976] Active effects per entity
    std::unordered_map<uint64_t, std::vector<StatusEffectInstance>> active_effects_;
    
    // [SEQUENCE: 977] Immunity tracking
    struct ImmunityInfo {
        std::vector<EffectCategory> category_immunities;
        std::vector<uint32_t> effect_id_immunities;
        std::chrono::steady_clock::time_point immunity_expire;
    };
    std::unordered_map<uint64_t, ImmunityInfo> immunities_;
    
    // [SEQUENCE: 978] Helper functions
    bool CanApplyEffect(uint64_t target_id, const StatusEffectData& effect) const;
    void ApplyStatModifiers(uint64_t target_id, const StatusEffectData& effect, 
                           const StatusEffectInstance& instance);
    void RemoveStatModifiers(uint64_t target_id, const StatusEffectData& effect,
                            const StatusEffectInstance& instance);
    void ProcessTick(uint64_t target_id, StatusEffectInstance& instance,
                    const StatusEffectData& effect, float delta_time);
    void OnEffectExpired(uint64_t target_id, const StatusEffectInstance& instance);
    StatusEffectInstance* FindEffect(uint64_t target_id, uint32_t effect_id, 
                                   uint64_t caster_id = 0);
};

// [SEQUENCE: 979] Effect factory for common effects
class StatusEffectFactory {
public:
    // [SEQUENCE: 980] Create stat buff
    static StatusEffectData CreateStatBuff(
        uint32_t effect_id,
        const std::string& name,
        const std::string& stat_name,
        float value,
        StatModifierType mod_type,
        float duration
    );
    
    // [SEQUENCE: 981] Create DoT effect
    static StatusEffectData CreateDoT(
        uint32_t effect_id,
        const std::string& name,
        float damage_per_tick,
        float tick_interval,
        float duration,
        EffectCategory category = EffectCategory::MAGIC
    );
    
    // [SEQUENCE: 982] Create HoT effect
    static StatusEffectData CreateHoT(
        uint32_t effect_id,
        const std::string& name,
        float heal_per_tick,
        float tick_interval,
        float duration
    );
    
    // [SEQUENCE: 983] Create control effect
    static StatusEffectData CreateControlEffect(
        uint32_t effect_id,
        const std::string& name,
        ControlEffect control_type,
        float duration,
        bool breaks_on_damage = false
    );
    
    // [SEQUENCE: 984] Create shield effect
    static StatusEffectData CreateShield(
        uint32_t effect_id,
        const std::string& name,
        float absorb_amount,
        float duration
    );
};

// [SEQUENCE: 985] Common effect IDs
namespace CommonEffects {
    // Buffs
    constexpr uint32_t ATTACK_POWER_BUFF = 1001;
    constexpr uint32_t DEFENSE_BUFF = 1002;
    constexpr uint32_t HASTE_BUFF = 1003;
    constexpr uint32_t REGENERATION = 1004;
    
    // Debuffs
    constexpr uint32_t WEAKNESS = 2001;
    constexpr uint32_t SLOW = 2002;
    constexpr uint32_t POISON = 2003;
    constexpr uint32_t BLEED = 2004;
    
    // Control
    constexpr uint32_t STUN = 3001;
    constexpr uint32_t SILENCE = 3002;
    constexpr uint32_t ROOT = 3003;
    constexpr uint32_t FEAR = 3004;
}

// [SEQUENCE: 986] Effect event handlers
class StatusEffectEventHandler {
public:
    using EffectApplyHandler = std::function<void(uint64_t, uint32_t, uint64_t)>;
    using EffectRemoveHandler = std::function<void(uint64_t, uint32_t)>;
    using EffectTickHandler = std::function<void(uint64_t, uint32_t, float)>;
    
    void RegisterApplyHandler(EffectApplyHandler handler);
    void RegisterRemoveHandler(EffectRemoveHandler handler);
    void RegisterTickHandler(EffectTickHandler handler);
    
    void OnEffectApplied(uint64_t target_id, uint32_t effect_id, uint64_t caster_id);
    void OnEffectRemoved(uint64_t target_id, uint32_t effect_id);
    void OnEffectTick(uint64_t target_id, uint32_t effect_id, float damage_or_heal);
    
private:
    std::vector<EffectApplyHandler> apply_handlers_;
    std::vector<EffectRemoveHandler> remove_handlers_;
    std::vector<EffectTickHandler> tick_handlers_;
};

} // namespace mmorpg::game::status