// [SEQUENCE: MVP4-42] Base combat utilities and interfaces
#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <functional>
#include <chrono>
#include <optional>

namespace mmorpg::game::combat {

enum class DamageType {
    PHYSICAL,    // 물리 피해
    MAGICAL,     // 마법 피해
    TRUE_DAMAGE, // 고정 피해 (방어 무시)
    ELEMENTAL,   // 속성 피해
    POISON,      // 독 피해
    BLEED,       // 출혈 피해
    HOLY,        // 신성 피해
    SHADOW       // 암흑 피해
};

enum class CombatResultType {
    HIT,         // 일반 타격
    CRITICAL,    // 치명타
    MISS,        // 회피
    DODGE,       // 회피 (액티브)
    BLOCK,       // 방어
    PARRY,       // 패리
    RESIST,      // 저항
    IMMUNE,      // 면역
    ABSORB       // 흡수
};

enum class TargetType {
    SELF,           // 자신
    SINGLE_ENEMY,   // 단일 적
    SINGLE_ALLY,    // 단일 아군
    AOE_ENEMY,      // 범위 적
    AOE_ALLY,       // 범위 아군
    CONE,           // 원뿔형
    LINE,           // 직선형
    POINT_BLANK,    // 자신 중심 범위
    GROUND_TARGET   // 지면 지정
};

struct CombatStats {
    // 기본 스탯
    float health = 100.0f;
    float max_health = 100.0f;
    float mana = 100.0f;
    float max_mana = 100.0f;
    
    // 공격 스탯
    float attack_power = 10.0f;
    float spell_power = 10.0f;
    float attack_speed = 1.0f;
    float critical_chance = 0.05f;  // 5%
    float critical_damage = 1.5f;   // 150%
    
    // 방어 스탯
    float armor = 0.0f;
    float magic_resist = 0.0f;
    float dodge_chance = 0.05f;
    float block_chance = 0.0f;
    float parry_chance = 0.0f;
    
    // 속성 저항
    std::unordered_map<DamageType, float> resistances;
    
    // 추가 스탯
    float life_steal = 0.0f;        // 생명력 흡수
    float cooldown_reduction = 0.0f; // 재사용 대기시간 감소
    float movement_speed = 100.0f;   // 이동 속도
};

struct DamageInfo {
    uint64_t attacker_id = 0;
    uint64_t target_id = 0;
    DamageType damage_type = DamageType::PHYSICAL;
    float base_damage = 0.0f;
    float final_damage = 0.0f;
    CombatResultType result = CombatResultType::HIT;
    
    // 추가 정보
    bool is_skill = false;
    uint32_t skill_id = 0;
    std::chrono::steady_clock::time_point timestamp;
    
    // 부가 효과
    std::vector<std::string> applied_effects;
};

class ICombatEntity {
public:
    virtual ~ICombatEntity() = default;
    
    virtual uint64_t GetEntityId() const = 0;
    
    virtual const CombatStats& GetCombatStats() const = 0;
    virtual void ModifyStat(const std::string& stat_name, float value) = 0;
    
    virtual void TakeDamage(float damage) = 0;
    virtual void Heal(float amount) = 0;
    virtual void UseMana(float amount) = 0;
    virtual void RestoreMana(float amount) = 0;
    
    virtual bool IsAlive() const = 0;
    virtual bool CanAttack() const = 0;
    virtual bool CanBeTargeted() const = 0;
    
    virtual void OnDeath() = 0;
    virtual void OnKill(uint64_t victim_id) = 0;
    virtual void OnDamageDealt(const DamageInfo& info) = 0;
    virtual void OnDamageTaken(const DamageInfo& info) = 0;
};

class DamageCalculator {
public:
    static DamageInfo CalculateDamage(
        const ICombatEntity* attacker,
        const ICombatEntity* target,
        float base_damage,
        DamageType damage_type,
        bool is_skill = false,
        uint32_t skill_id = 0
    );
    
    static float ApplyDamageModifiers(
        float base_damage,
        const CombatStats& attacker_stats,
        const CombatStats& target_stats,
        DamageType damage_type
    );
    
    static float CalculateDefense(
        const CombatStats& stats,
        DamageType damage_type
    );
    
    static CombatResultType DetermineCombatResult(
        const CombatStats& attacker_stats,
        const CombatStats& target_stats,
        bool is_skill = false
    );
    
private:
    static bool RollChance(float chance);
    static float CalculateArmorReduction(float armor);
    static float CalculateResistanceReduction(float resistance);
};

class CombatManager {
public:
    static CombatManager& Instance() {
        static CombatManager instance;
        return instance;
    }
    
    void RegisterEntity(std::shared_ptr<ICombatEntity> entity);
    void UnregisterEntity(uint64_t entity_id);
    
    bool ExecuteAttack(uint64_t attacker_id, uint64_t target_id);
    
    bool ExecuteSkill(uint64_t caster_id, uint32_t skill_id, 
                     uint64_t target_id = 0,
                     float target_x = 0, float target_y = 0, float target_z = 0);
    
    std::vector<DamageInfo> ExecuteAreaDamage(
        uint64_t attacker_id,
        float center_x, float center_y, float center_z,
        float radius,
        float base_damage,
        DamageType damage_type,
        TargetType target_type = TargetType::AOE_ENEMY
    );
    
    std::vector<uint64_t> GetEntitiesInRange(
        float center_x, float center_y, float center_z,
        float radius,
        TargetType filter = TargetType::AOE_ENEMY
    );
    
    void AddThreat(uint64_t attacker_id, uint64_t target_id, float threat);
    float GetThreat(uint64_t attacker_id, uint64_t target_id) const;
    uint64_t GetHighestThreatTarget(uint64_t entity_id) const;
    
    void LogCombatEvent(const DamageInfo& info);
    std::vector<DamageInfo> GetCombatLog(uint64_t entity_id, size_t max_entries = 100) const;
    
    void StartAutoAttack(uint64_t attacker_id, uint64_t target_id);
    void StopAutoAttack(uint64_t attacker_id);
    void UpdateAutoAttacks(float delta_time);
    
private:
    CombatManager() = default;
    
    std::unordered_map<uint64_t, std::shared_ptr<ICombatEntity>> entities_;
    
    struct ThreatInfo {
        float threat_value = 0.0f;
        std::chrono::steady_clock::time_point last_update;
    };
    std::unordered_map<uint64_t, std::unordered_map<uint64_t, ThreatInfo>> threat_table_;
    
    struct AutoAttackInfo {
        uint64_t target_id;
        float time_since_last_attack = 0.0f;
        bool is_active = true;
    };
    std::unordered_map<uint64_t, AutoAttackInfo> auto_attacks_;
    
    std::unordered_map<uint64_t, std::vector<DamageInfo>> combat_logs_;
    
    std::shared_ptr<ICombatEntity> GetEntity(uint64_t entity_id) const;
    bool IsValidTarget(uint64_t attacker_id, uint64_t target_id) const;
    bool IsInRange(const ICombatEntity* attacker, const ICombatEntity* target, float range) const;
};

namespace CombatFormulas {
    constexpr float GetBaseDamageMultiplier(int attacker_level, int target_level) {
        int level_diff = attacker_level - target_level;
        if (level_diff > 10) return 1.5f;
        if (level_diff > 5) return 1.2f;
        if (level_diff < -10) return 0.5f;
        if (level_diff < -5) return 0.8f;
        return 1.0f;
    }
    
    constexpr float GetHitChance(float attack_rating, float defense_rating) {
        // Base 95% hit chance, modified by ratings
        float base_hit = 0.95f;
        float rating_diff = attack_rating - defense_rating;
        return std::clamp(base_hit + (rating_diff * 0.001f), 0.1f, 1.0f);
    }
    
    constexpr float GetCriticalMultiplier(float crit_damage_stat) {
        return 1.5f + (crit_damage_stat * 0.01f);  // Base 150% + bonus
    }
}

class CombatEventHandler {
public:
    using DamageHandler = std::function<void(const DamageInfo&)>;
    using DeathHandler = std::function<void(uint64_t)>;
    
    void RegisterDamageHandler(DamageHandler handler);
    void RegisterDeathHandler(DeathHandler handler);
    
    void OnDamage(const DamageInfo& info);
    void OnDeath(uint64_t entity_id);
    
private:
    std::vector<DamageHandler> damage_handlers_;
    std::vector<DeathHandler> death_handlers_;
};

namespace CombatUtils {
    // Check if position is behind target
    bool IsBehindTarget(float attacker_x, float attacker_y, 
                       float target_x, float target_y, float target_facing);
    
    // Check line of sight for combat
    bool HasCombatLineOfSight(uint64_t attacker_id, uint64_t target_id);
    
    // Calculate combat range including entity sizes
    float GetCombatRange(const ICombatEntity* attacker, const ICombatEntity* target);
}

} // namespace mmorpg::game::combat
