#pragma once

#include <functional>
#include <unordered_map>
#include <vector>
#include <memory>
#include <chrono>
#include <optional>
#include <spdlog/spdlog.h>

namespace mmorpg::game::ai {

// [SEQUENCE: 2179] Basic AI behavior system for NPCs and creatures
// 기본 AI 행동 시스템 - NPC와 크리처를 위한 구현

// [SEQUENCE: 2180] AI state enumeration
enum class AIState {
    IDLE,               // 대기 상태
    PATROL,             // 순찰 중
    COMBAT,             // 전투 중
    FLEEING,            // 도망 중
    DEAD,               // 사망
    RETURNING,          // 귀환 중 (리쉬)
    CASTING,            // 시전 중
    STUNNED,            // 기절
    SEARCHING,          // 탐색 중 (타겟 잃음)
    INTERACTING         // 상호작용 중 (NPC)
};

// [SEQUENCE: 2181] AI behavior type
enum class BehaviorType {
    AGGRESSIVE,         // 공격적 (시야 내 적대적 선공)
    DEFENSIVE,          // 방어적 (공격받으면 반격)
    PASSIVE,            // 수동적 (공격받아도 무시)
    NEUTRAL,            // 중립적 (팩션 기반)
    COWARDLY,           // 겁쟁이 (체력 낮으면 도망)
    HELPER,             // 도우미 (동료 지원)
    GUARD,              // 경비 (위치 사수)
    VENDOR              // 상인 (거래 가능)
};

// [SEQUENCE: 2182] Combat role
enum class CombatRole {
    MELEE_DPS,          // 근접 딜러
    RANGED_DPS,         // 원거리 딜러
    TANK,               // 탱커
    HEALER,             // 힐러
    CASTER,             // 캐스터
    SUPPORT,            // 지원
    HYBRID              // 하이브리드
};

// [SEQUENCE: 2183] Movement type
enum class MovementType {
    STATIONARY,         // 고정
    RANDOM_WALK,        // 무작위 이동
    WAYPOINT_PATROL,    // 경로 순찰
    FOLLOW,             // 따라가기
    CIRCLE_STRAFE,      // 원형 이동
    FLEE_DIRECTION      // 도망 방향
};

// [SEQUENCE: 2184] Target selection priority
struct TargetPriority {
    enum Priority {
        CLOSEST = 0,            // 가장 가까운 적
        LOWEST_HEALTH,          // 체력 가장 낮은 적
        HIGHEST_THREAT,         // 위협 수준 가장 높은 적
        PLAYER_FIRST,           // 플레이어 우선
        HEALER_FIRST,          // 힐러 우선
        CASTER_FIRST,          // 캐스터 우선
        RANDOM                 // 무작위
    };
    
    Priority primary = HIGHEST_THREAT;
    Priority secondary = CLOSEST;
    
    // [SEQUENCE: 2185] Score target for selection
    float ScoreTarget(uint64_t target_id, float distance, float threat, 
                     float health_percent, bool is_player, CombatRole role) const {
        float score = 0.0f;
        
        // Primary priority
        switch (primary) {
            case CLOSEST:
                score += (100.0f - distance) * 10.0f;
                break;
            case LOWEST_HEALTH:
                score += (100.0f - health_percent) * 10.0f;
                break;
            case HIGHEST_THREAT:
                score += threat;
                break;
            case PLAYER_FIRST:
                score += is_player ? 1000.0f : 0.0f;
                break;
            case HEALER_FIRST:
                score += (role == CombatRole::HEALER) ? 1000.0f : 0.0f;
                break;
            case CASTER_FIRST:
                score += (role == CombatRole::CASTER) ? 1000.0f : 0.0f;
                break;
            case RANDOM:
                score += rand() % 100;
                break;
        }
        
        // Secondary priority (lower weight)
        switch (secondary) {
            case CLOSEST:
                score += (100.0f - distance);
                break;
            case LOWEST_HEALTH:
                score += (100.0f - health_percent);
                break;
            case HIGHEST_THREAT:
                score += threat * 0.1f;
                break;
        }
        
        return score;
    }
};

// [SEQUENCE: 2186] AI perception data
struct AIPerception {
    float sight_range = 30.0f;          // 시야 거리
    float sight_angle = 120.0f;         // 시야각 (도)
    float hearing_range = 40.0f;        // 청각 거리
    float aggro_range = 25.0f;          // 선공 거리
    float help_range = 20.0f;           // 도움 요청 거리
    
    bool can_see_stealth = false;       // 은신 감지
    bool can_see_invisible = false;     // 투명 감지
    bool ignore_line_of_sight = false;  // 시야 차단 무시
    
    // [SEQUENCE: 2187] Check if can perceive target
    bool CanPerceive(float distance, float angle, bool is_stealthed, 
                     bool is_invisible, bool has_los) const {
        // 거리 체크
        if (distance > sight_range) {
            return false;
        }
        
        // 시야각 체크
        if (angle > sight_angle / 2.0f) {
            return false;
        }
        
        // 은신/투명 체크
        if (is_stealthed && !can_see_stealth) {
            return false;
        }
        if (is_invisible && !can_see_invisible) {
            return false;
        }
        
        // 시야 차단 체크
        if (!has_los && !ignore_line_of_sight) {
            return false;
        }
        
        return true;
    }
};

// [SEQUENCE: 2188] AI memory for targets
struct AIMemory {
    struct TargetMemory {
        uint64_t target_id;
        float last_known_x, last_known_y, last_known_z;
        std::chrono::system_clock::time_point last_seen;
        float total_damage_dealt = 0.0f;
        float total_damage_taken = 0.0f;
        bool is_player = false;
    };
    
    std::unordered_map<uint64_t, TargetMemory> known_enemies;
    std::unordered_map<uint64_t, TargetMemory> known_allies;
    uint64_t last_attacker_id = 0;
    
    std::chrono::seconds memory_duration{30};  // 기억 유지 시간
    
    // [SEQUENCE: 2189] Update target memory
    void UpdateTargetMemory(uint64_t target_id, float x, float y, float z, bool is_enemy) {
        auto& memory_map = is_enemy ? known_enemies : known_allies;
        auto& memory = memory_map[target_id];
        
        memory.target_id = target_id;
        memory.last_known_x = x;
        memory.last_known_y = y;
        memory.last_known_z = z;
        memory.last_seen = std::chrono::system_clock::now();
    }
    
    // [SEQUENCE: 2190] Forget old targets
    void ForgetOldTargets() {
        auto now = std::chrono::system_clock::now();
        
        auto forget = [&](auto& memory_map) {
            for (auto it = memory_map.begin(); it != memory_map.end();) {
                if (now - it->second.last_seen > memory_duration) {
                    it = memory_map.erase(it);
                } else {
                    ++it;
                }
            }
        };
        
        forget(known_enemies);
        forget(known_allies);
    }
};

// [SEQUENCE: 2191] Basic AI controller
class BasicAIController {
public:
    BasicAIController(uint64_t entity_id, BehaviorType behavior, CombatRole role)
        : entity_id_(entity_id), behavior_type_(behavior), combat_role_(role),
          current_state_(AIState::IDLE) {
        
        // Initialize default perception based on behavior
        InitializePerception();
    }
    
    // [SEQUENCE: 2192] Main AI update
    void Update(float delta_time) {
        // Update timers
        UpdateTimers(delta_time);
        
        // Forget old memories
        memory_.ForgetOldTargets();
        
        // State machine update
        switch (current_state_) {
            case AIState::IDLE:
                UpdateIdle();
                break;
                
            case AIState::PATROL:
                UpdatePatrol();
                break;
                
            case AIState::COMBAT:
                UpdateCombat();
                break;
                
            case AIState::FLEEING:
                UpdateFleeing();
                break;
                
            case AIState::RETURNING:
                UpdateReturning();
                break;
                
            case AIState::CASTING:
                UpdateCasting();
                break;
                
            case AIState::SEARCHING:
                UpdateSearching();
                break;
                
            case AIState::INTERACTING:
                UpdateInteracting();
                break;
                
            default:
                break;
        }
        
        // Check for state transitions
        CheckStateTransitions();
    }
    
    // [SEQUENCE: 2193] Handle damage event
    void OnDamageTaken(uint64_t attacker_id, float damage) {
        memory_.last_attacker_id = attacker_id;
        memory_.known_enemies[attacker_id].total_damage_taken += damage;
        
        // Enter combat if not already
        if (current_state_ != AIState::COMBAT && current_state_ != AIState::FLEEING) {
            EnterCombat(attacker_id);
        }
        
        // Check if should flee
        if (behavior_type_ == BehaviorType::COWARDLY) {
            float health_percent = GetHealthPercent();
            if (health_percent < flee_health_threshold_) {
                ChangeState(AIState::FLEEING);
            }
        }
        
        // Call for help
        if (ShouldCallForHelp()) {
            CallForHelp(attacker_id);
        }
    }
    
    // [SEQUENCE: 2194] Set home position
    void SetHomePosition(float x, float y, float z) {
        home_x_ = x;
        home_y_ = y;
        home_z_ = z;
        has_home_ = true;
    }
    
    // [SEQUENCE: 2195] Add patrol waypoint
    void AddPatrolWaypoint(float x, float y, float z, float wait_time = 0.0f) {
        patrol_waypoints_.push_back({x, y, z, wait_time});
    }
    
    // [SEQUENCE: 2196] Force state change
    void ForceState(AIState new_state) {
        ChangeState(new_state);
    }
    
    // [SEQUENCE: 2197] Set combat abilities
    void AddAbility(uint32_t ability_id, float cooldown, float range, 
                   float min_range = 0.0f, float priority = 1.0f) {
        abilities_.push_back({
            ability_id, cooldown, range, min_range, priority, 0.0f
        });
    }
    
    // Getters
    AIState GetCurrentState() const { return current_state_; }
    uint64_t GetCurrentTarget() const { return current_target_; }
    BehaviorType GetBehaviorType() const { return behavior_type_; }
    
private:
    uint64_t entity_id_;
    BehaviorType behavior_type_;
    CombatRole combat_role_;
    AIState current_state_;
    AIState previous_state_ = AIState::IDLE;
    
    // Perception
    AIPerception perception_;
    AIMemory memory_;
    
    // Combat
    uint64_t current_target_ = 0;
    float max_chase_distance_ = 50.0f;
    float flee_health_threshold_ = 0.2f;  // 20% HP
    TargetPriority target_priority_;
    
    // Movement
    float home_x_ = 0, home_y_ = 0, home_z_ = 0;
    bool has_home_ = false;
    float leash_range_ = 40.0f;
    
    // Patrol
    struct Waypoint {
        float x, y, z;
        float wait_time;
    };
    std::vector<Waypoint> patrol_waypoints_;
    size_t current_waypoint_ = 0;
    float waypoint_wait_timer_ = 0.0f;
    
    // Abilities
    struct AIAbility {
        uint32_t ability_id;
        float cooldown;
        float range;
        float min_range;
        float priority;
        float cooldown_remaining;
    };
    std::vector<AIAbility> abilities_;
    
    // Timers
    float search_timer_ = 0.0f;
    float flee_timer_ = 0.0f;
    float cast_timer_ = 0.0f;
    uint32_t casting_ability_ = 0;
    
    // [SEQUENCE: 2198] Initialize perception based on behavior
    void InitializePerception() {
        switch (behavior_type_) {
            case BehaviorType::AGGRESSIVE:
                perception_.sight_range = 35.0f;
                perception_.aggro_range = 30.0f;
                break;
                
            case BehaviorType::DEFENSIVE:
                perception_.sight_range = 25.0f;
                perception_.aggro_range = 0.0f;  // Don't aggro
                break;
                
            case BehaviorType::PASSIVE:
                perception_.sight_range = 20.0f;
                perception_.aggro_range = 0.0f;
                perception_.help_range = 0.0f;  // Don't help
                break;
                
            case BehaviorType::GUARD:
                perception_.sight_range = 40.0f;
                perception_.can_see_stealth = true;
                break;
                
            default:
                break;
        }
    }
    
    // [SEQUENCE: 2199] State updates
    void UpdateIdle() {
        // Look for enemies if aggressive
        if (behavior_type_ == BehaviorType::AGGRESSIVE) {
            auto enemies = ScanForEnemies();
            if (!enemies.empty()) {
                auto best_target = SelectBestTarget(enemies);
                if (best_target) {
                    EnterCombat(best_target.value());
                }
            }
        }
        
        // Start patrolling if has waypoints
        if (!patrol_waypoints_.empty() && rand() % 100 < 10) {  // 10% chance per update
            ChangeState(AIState::PATROL);
        }
    }
    
    void UpdatePatrol() {
        if (patrol_waypoints_.empty()) {
            ChangeState(AIState::IDLE);
            return;
        }
        
        // Wait at waypoint
        if (waypoint_wait_timer_ > 0) {
            return;
        }
        
        // Move to current waypoint
        const auto& waypoint = patrol_waypoints_[current_waypoint_];
        float distance = GetDistanceToPoint(waypoint.x, waypoint.y, waypoint.z);
        
        if (distance < 2.0f) {
            // Reached waypoint
            waypoint_wait_timer_ = waypoint.wait_time;
            current_waypoint_ = (current_waypoint_ + 1) % patrol_waypoints_.size();
        } else {
            // Move towards waypoint
            MoveToPosition(waypoint.x, waypoint.y, waypoint.z);
        }
        
        // Check for enemies while patrolling
        if (behavior_type_ == BehaviorType::AGGRESSIVE || 
            behavior_type_ == BehaviorType::GUARD) {
            auto enemies = ScanForEnemies();
            if (!enemies.empty()) {
                auto best_target = SelectBestTarget(enemies);
                if (best_target) {
                    EnterCombat(best_target.value());
                }
            }
        }
    }
    
    void UpdateCombat() {
        // Validate target
        if (!IsValidTarget(current_target_)) {
            // Lost target, search for it
            ChangeState(AIState::SEARCHING);
            return;
        }
        
        // Check leash
        if (has_home_ && GetDistanceFromHome() > leash_range_) {
            ChangeState(AIState::RETURNING);
            return;
        }
        
        // Update target position in memory
        auto target_pos = GetTargetPosition(current_target_);
        memory_.UpdateTargetMemory(current_target_, target_pos.x, target_pos.y, 
                                  target_pos.z, true);
        
        // Combat behavior based on role
        switch (combat_role_) {
            case CombatRole::MELEE_DPS:
            case CombatRole::TANK:
                UpdateMeleeCombat();
                break;
                
            case CombatRole::RANGED_DPS:
            case CombatRole::CASTER:
                UpdateRangedCombat();
                break;
                
            case CombatRole::HEALER:
                UpdateHealerCombat();
                break;
                
            default:
                UpdateMeleeCombat();  // Default to melee
                break;
        }
    }
    
    void UpdateFleeing() {
        flee_timer_ -= 0.1f;  // Assume 0.1s update
        
        if (flee_timer_ <= 0 || GetHealthPercent() > 0.5f) {
            // Stop fleeing
            ChangeState(AIState::RETURNING);
            return;
        }
        
        // Run away from last attacker
        if (memory_.last_attacker_id != 0) {
            auto attacker_pos = GetTargetPosition(memory_.last_attacker_id);
            float angle = GetAngleFrom(attacker_pos.x, attacker_pos.y);
            float flee_angle = angle + M_PI;  // Opposite direction
            
            float flee_x = GetPositionX() + cos(flee_angle) * 10.0f;
            float flee_y = GetPositionY() + sin(flee_angle) * 10.0f;
            
            MoveToPosition(flee_x, flee_y, GetPositionZ());
        }
    }
    
    void UpdateReturning() {
        if (!has_home_) {
            ChangeState(AIState::IDLE);
            return;
        }
        
        float distance = GetDistanceFromHome();
        
        if (distance < 2.0f) {
            // Reached home
            ChangeState(AIState::IDLE);
            ResetThreat();  // Clear threat table
        } else {
            // Move towards home
            MoveToPosition(home_x_, home_y_, home_z_);
            
            // Regenerate while returning
            RegenerateHealth(0.05f);  // 5% per second
        }
    }
    
    void UpdateSearching() {
        search_timer_ += 0.1f;
        
        if (search_timer_ > 5.0f) {
            // Give up searching
            ChangeState(AIState::RETURNING);
            return;
        }
        
        // Look for last known position
        if (current_target_ != 0) {
            auto it = memory_.known_enemies.find(current_target_);
            if (it != memory_.known_enemies.end()) {
                const auto& last_pos = it->second;
                MoveToPosition(last_pos.last_known_x, last_pos.last_known_y, 
                             last_pos.last_known_z);
            }
        }
        
        // Scan for new enemies
        auto enemies = ScanForEnemies();
        if (!enemies.empty()) {
            auto best_target = SelectBestTarget(enemies);
            if (best_target) {
                EnterCombat(best_target.value());
            }
        }
    }
    
    void UpdateCasting() {
        cast_timer_ -= 0.1f;
        
        if (cast_timer_ <= 0) {
            // Cast complete
            ExecuteAbility(casting_ability_, current_target_);
            ChangeState(previous_state_);
        }
        
        // Check for interruption
        // (Would be triggered by damage or other events)
    }
    
    void UpdateInteracting() {
        // NPC interaction state
        // Would handle dialog, trading, etc.
    }
    
    // [SEQUENCE: 2200] State transition checks
    void CheckStateTransitions() {
        // Global transitions that can happen from any state
        
        // Death check
        if (GetHealthPercent() <= 0 && current_state_ != AIState::DEAD) {
            ChangeState(AIState::DEAD);
            return;
        }
        
        // Stunned check
        if (IsStunned() && current_state_ != AIState::STUNNED) {
            previous_state_ = current_state_;
            ChangeState(AIState::STUNNED);
            return;
        }
        
        // Recovery from stun
        if (!IsStunned() && current_state_ == AIState::STUNNED) {
            ChangeState(previous_state_);
        }
    }
    
    // [SEQUENCE: 2201] Combat helpers
    void UpdateMeleeCombat() {
        float distance = GetDistanceToTarget(current_target_);
        
        if (distance > 5.0f) {
            // Chase target
            ChaseTarget(current_target_);
        } else {
            // In melee range, use abilities
            auto ability = SelectBestAbility(distance);
            if (ability) {
                UseAbility(ability.value());
            } else {
                // Basic attack
                BasicAttack(current_target_);
            }
        }
    }
    
    void UpdateRangedCombat() {
        float distance = GetDistanceToTarget(current_target_);
        float optimal_range = 20.0f;  // Optimal range for ranged
        
        if (distance < 8.0f) {
            // Too close, back up
            MoveAwayFrom(current_target_, optimal_range);
        } else if (distance > 30.0f) {
            // Too far, move closer
            MoveTowardsTarget(current_target_, optimal_range);
        } else {
            // Good range, use abilities
            auto ability = SelectBestAbility(distance);
            if (ability) {
                UseAbility(ability.value());
            } else {
                // Basic ranged attack
                RangedAttack(current_target_);
            }
        }
    }
    
    void UpdateHealerCombat() {
        // Priority: Heal allies > Damage enemies
        
        // Check for injured allies
        auto injured_allies = ScanForInjuredAllies();
        if (!injured_allies.empty()) {
            // Heal most injured ally
            auto most_injured = GetMostInjuredAlly(injured_allies);
            auto heal_ability = SelectHealingAbility();
            if (heal_ability) {
                current_target_ = most_injured;
                UseAbility(heal_ability.value());
                return;
            }
        }
        
        // No healing needed, do damage
        UpdateRangedCombat();
    }
    
    // [SEQUENCE: 2202] Helper methods
    void ChangeState(AIState new_state) {
        if (new_state == current_state_) return;
        
        OnExitState(current_state_);
        previous_state_ = current_state_;
        current_state_ = new_state;
        OnEnterState(new_state);
        
        spdlog::debug("AI {} changed state from {} to {}", 
                     entity_id_, static_cast<int>(previous_state_), 
                     static_cast<int>(new_state));
    }
    
    void OnEnterState(AIState state) {
        switch (state) {
            case AIState::COMBAT:
                // Alert nearby allies
                AlertNearbyAllies(current_target_);
                break;
                
            case AIState::FLEEING:
                flee_timer_ = 10.0f;  // Flee for 10 seconds
                DropThreat(0.5f);  // Drop 50% threat
                break;
                
            case AIState::SEARCHING:
                search_timer_ = 0.0f;
                break;
                
            case AIState::RETURNING:
                current_target_ = 0;
                SetMovementSpeed(1.5f);  // Move faster when returning
                break;
                
            default:
                break;
        }
    }
    
    void OnExitState(AIState state) {
        switch (state) {
            case AIState::RETURNING:
                SetMovementSpeed(1.0f);  // Normal speed
                break;
                
            case AIState::PATROL:
                waypoint_wait_timer_ = 0.0f;
                break;
                
            default:
                break;
        }
    }
    
    void EnterCombat(uint64_t target_id) {
        current_target_ = target_id;
        ChangeState(AIState::COMBAT);
    }
    
    void UpdateTimers(float delta_time) {
        // Update ability cooldowns
        for (auto& ability : abilities_) {
            if (ability.cooldown_remaining > 0) {
                ability.cooldown_remaining -= delta_time;
            }
        }
        
        // Update waypoint wait timer
        if (waypoint_wait_timer_ > 0) {
            waypoint_wait_timer_ -= delta_time;
        }
    }
    
    bool ShouldCallForHelp() const {
        return perception_.help_range > 0 && 
               GetHealthPercent() < 0.5f;  // Call for help at 50% HP
    }
    
    std::optional<uint32_t> SelectBestAbility(float target_distance) {
        std::optional<uint32_t> best_ability;
        float best_priority = -1.0f;
        
        for (auto& ability : abilities_) {
            // Check cooldown
            if (ability.cooldown_remaining > 0) continue;
            
            // Check range
            if (target_distance < ability.min_range || 
                target_distance > ability.range) continue;
            
            // Check other conditions (mana, etc.)
            if (!CanUseAbility(ability.ability_id)) continue;
            
            // Select highest priority
            if (ability.priority > best_priority) {
                best_priority = ability.priority;
                best_ability = ability.ability_id;
            }
        }
        
        return best_ability;
    }
    
    void UseAbility(uint32_t ability_id) {
        // Find ability
        for (auto& ability : abilities_) {
            if (ability.ability_id == ability_id) {
                // Start casting
                casting_ability_ = ability_id;
                cast_timer_ = GetAbilityCastTime(ability_id);
                ability.cooldown_remaining = ability.cooldown;
                
                if (cast_timer_ > 0) {
                    ChangeState(AIState::CASTING);
                } else {
                    // Instant cast
                    ExecuteAbility(ability_id, current_target_);
                }
                break;
            }
        }
    }
    
    std::vector<uint64_t> ScanForEnemies() {
        // This would interface with the spatial system
        // For now, return placeholder
        return {};
    }
    
    std::optional<uint64_t> SelectBestTarget(const std::vector<uint64_t>& enemies) {
        if (enemies.empty()) return std::nullopt;
        
        uint64_t best_target = 0;
        float best_score = -1.0f;
        
        for (uint64_t enemy : enemies) {
            float distance = GetDistanceToTarget(enemy);
            float threat = GetThreatLevel(enemy);
            float health_percent = GetTargetHealthPercent(enemy);
            bool is_player = IsPlayer(enemy);
            CombatRole role = GetTargetRole(enemy);
            
            float score = target_priority_.ScoreTarget(
                enemy, distance, threat, health_percent, is_player, role
            );
            
            if (score > best_score) {
                best_score = score;
                best_target = enemy;
            }
        }
        
        return best_target;
    }
    
    // Placeholder methods that would interface with other systems
    float GetHealthPercent() const { return 1.0f; }
    float GetDistanceToTarget(uint64_t target) const { return 10.0f; }
    float GetDistanceToPoint(float x, float y, float z) const { return 0.0f; }
    float GetDistanceFromHome() const { return 0.0f; }
    bool IsValidTarget(uint64_t target) const { return true; }
    bool IsStunned() const { return false; }
    void CallForHelp(uint64_t enemy) {}
    void AlertNearbyAllies(uint64_t enemy) {}
    void ResetThreat() {}
    void DropThreat(float percent) {}
    void RegenerateHealth(float percent) {}
    void MoveToPosition(float x, float y, float z) {}
    void ChaseTarget(uint64_t target) {}
    void MoveAwayFrom(uint64_t target, float distance) {}
    void MoveTowardsTarget(uint64_t target, float distance) {}
    void BasicAttack(uint64_t target) {}
    void RangedAttack(uint64_t target) {}
    void ExecuteAbility(uint32_t ability, uint64_t target) {}
    bool CanUseAbility(uint32_t ability) const { return true; }
    float GetAbilityCastTime(uint32_t ability) const { return 0.0f; }
    void SetMovementSpeed(float multiplier) {}
    float GetThreatLevel(uint64_t target) const { return 0.0f; }
    float GetTargetHealthPercent(uint64_t target) const { return 1.0f; }
    bool IsPlayer(uint64_t target) const { return false; }
    CombatRole GetTargetRole(uint64_t target) const { return CombatRole::MELEE_DPS; }
    std::vector<uint64_t> ScanForInjuredAllies() { return {}; }
    uint64_t GetMostInjuredAlly(const std::vector<uint64_t>& allies) { return 0; }
    std::optional<uint32_t> SelectHealingAbility() { return std::nullopt; }
    float GetPositionX() const { return 0.0f; }
    float GetPositionY() const { return 0.0f; }
    float GetPositionZ() const { return 0.0f; }
    struct Position { float x, y, z; };
    Position GetTargetPosition(uint64_t target) const { return {0, 0, 0}; }
    float GetAngleFrom(float x, float y) const { return 0.0f; }
};

// [SEQUENCE: 2203] AI behavior factory
class AIBehaviorFactory {
public:
    static std::unique_ptr<BasicAIController> CreateAI(
        uint64_t entity_id, BehaviorType behavior, CombatRole role) {
        
        auto ai = std::make_unique<BasicAIController>(entity_id, behavior, role);
        
        // Configure based on role
        switch (role) {
            case CombatRole::TANK:
                ai->AddAbility(1001, 8.0f, 5.0f, 0.0f, 2.0f);   // Taunt
                ai->AddAbility(1002, 15.0f, 5.0f, 0.0f, 1.5f);  // Shield Wall
                break;
                
            case CombatRole::HEALER:
                ai->AddAbility(2001, 0.0f, 30.0f, 0.0f, 3.0f);   // Heal
                ai->AddAbility(2002, 10.0f, 30.0f, 0.0f, 2.0f);  // Group Heal
                ai->AddAbility(2003, 1.5f, 25.0f, 0.0f, 1.0f);   // Damage Spell
                break;
                
            case CombatRole::RANGED_DPS:
                ai->AddAbility(3001, 0.0f, 30.0f, 8.0f, 1.0f);   // Shot
                ai->AddAbility(3002, 10.0f, 30.0f, 8.0f, 2.0f);  // Power Shot
                ai->AddAbility(3003, 20.0f, 20.0f, 0.0f, 1.5f);  // Trap
                break;
                
            case CombatRole::CASTER:
                ai->AddAbility(4001, 0.0f, 30.0f, 0.0f, 1.0f);   // Bolt
                ai->AddAbility(4002, 8.0f, 25.0f, 0.0f, 2.5f);   // Blast
                ai->AddAbility(4003, 15.0f, 20.0f, 0.0f, 3.0f);  // AoE
                break;
                
            default:
                // Melee DPS
                ai->AddAbility(5001, 0.0f, 5.0f, 0.0f, 1.0f);    // Strike
                ai->AddAbility(5002, 6.0f, 5.0f, 0.0f, 2.0f);    // Power Strike
                break;
        }
        
        return ai;
    }
    
    // [SEQUENCE: 2204] Create guard AI
    static std::unique_ptr<BasicAIController> CreateGuardAI(uint64_t entity_id) {
        auto ai = CreateAI(entity_id, BehaviorType::GUARD, CombatRole::TANK);
        
        // Guards have enhanced perception
        ai->AddAbility(6001, 30.0f, 30.0f, 0.0f, 3.0f);  // Call Reinforcements
        
        return ai;
    }
    
    // [SEQUENCE: 2205] Create vendor AI
    static std::unique_ptr<BasicAIController> CreateVendorAI(uint64_t entity_id) {
        return CreateAI(entity_id, BehaviorType::VENDOR, CombatRole::SUPPORT);
    }
};

// [SEQUENCE: 2206] AI manager singleton
class AIManager {
public:
    static AIManager& Instance() {
        static AIManager instance;
        return instance;
    }
    
    // [SEQUENCE: 2207] Register AI controller
    void RegisterAI(uint64_t entity_id, std::unique_ptr<BasicAIController> ai) {
        ai_controllers_[entity_id] = std::move(ai);
        
        spdlog::info("Registered AI controller for entity {}", entity_id);
    }
    
    // [SEQUENCE: 2208] Update all AIs
    void UpdateAll(float delta_time) {
        for (auto& [entity_id, ai] : ai_controllers_) {
            if (ai) {
                ai->Update(delta_time);
            }
        }
    }
    
    // [SEQUENCE: 2209] Get AI controller
    BasicAIController* GetAI(uint64_t entity_id) {
        auto it = ai_controllers_.find(entity_id);
        return (it != ai_controllers_.end()) ? it->second.get() : nullptr;
    }
    
    // [SEQUENCE: 2210] Remove AI controller
    void RemoveAI(uint64_t entity_id) {
        ai_controllers_.erase(entity_id);
    }
    
    // [SEQUENCE: 2211] Handle global AI event
    void OnGlobalEvent(const std::string& event_type, uint64_t source_id) {
        if (event_type == "player_detected") {
            // Alert nearby guards
            for (auto& [entity_id, ai] : ai_controllers_) {
                if (ai->GetBehaviorType() == BehaviorType::GUARD) {
                    // Check distance and alert
                }
            }
        }
    }
    
private:
    AIManager() = default;
    
    std::unordered_map<uint64_t, std::unique_ptr<BasicAIController>> ai_controllers_;
};

} // namespace mmorpg::game::ai