#pragma once

#include <memory>
#include <deque>
#include <unordered_map>
#include <chrono>
#include <optional>
#include "../core/types.h"
#include "../core/singleton.h"
#include "../player/player.h"
#include "../combat/combat.h"
#include "client_prediction.h"

namespace mmorpg::network {

// [SEQUENCE: MVP13-51] World snapshot for time rewind
struct WorldSnapshot {
    uint32_t tick;
    std::chrono::steady_clock::time_point timestamp;
    
    // Entity states
    struct EntityState {
        uint64_t entity_id;
        Vector3 position;
        Vector3 velocity;
        float rotation;
        BoundingBox hitbox;
        float health;
        bool is_alive;
    };
    
    std::unordered_map<uint64_t, EntityState> entity_states;
    
    // Projectile states
    struct ProjectileState {
        uint64_t projectile_id;
        Vector3 position;
        Vector3 velocity;
        float radius;
        uint64_t owner_id;
    };
    
    std::vector<ProjectileState> projectile_states;
};

// [SEQUENCE: MVP13-52] Hit validation result
struct HitValidation {
    bool is_valid{false};
    Vector3 impact_point;
    float damage{0.0f};
    uint64_t victim_id{0};
    float confidence{0.0f};  // 0.0 to 1.0
    std::string rejection_reason;
};

// [SEQUENCE: MVP13-53] Lag compensation system
class LagCompensation : public Singleton<LagCompensation> {
public:
    // [SEQUENCE: MVP13-54] Snapshot management
    void RecordSnapshot();
    void SetSnapshotInterval(std::chrono::milliseconds interval);
    
    // Time rewind
    std::optional<WorldSnapshot> GetSnapshotAtTime(
        std::chrono::steady_clock::time_point target_time);
    WorldSnapshot InterpolateSnapshots(
        const WorldSnapshot& before,
        const WorldSnapshot& after,
        std::chrono::steady_clock::time_point target_time);
    
    // [SEQUENCE: MVP13-55] Hit validation
    HitValidation ValidateHit(
        uint64_t attacker_id,
        uint64_t victim_id,
        const Vector3& shot_origin,
        const Vector3& shot_direction,
        float max_range,
        std::chrono::steady_clock::time_point shot_time);
    
    HitValidation ValidateProjectileHit(
        uint64_t projectile_id,
        uint64_t victim_id,
        const Vector3& impact_point,
        std::chrono::steady_clock::time_point impact_time);
    
    // [SEQUENCE: MVP13-56] Movement validation
    bool ValidateMovement(
        uint64_t player_id,
        const Vector3& from_position,
        const Vector3& to_position,
        std::chrono::steady_clock::time_point from_time,
        std::chrono::steady_clock::time_point to_time);
    
    // Player latency tracking
    void UpdatePlayerLatency(uint64_t player_id, float latency_ms);
    float GetPlayerLatency(uint64_t player_id) const;
    
    // Configuration
    void SetMaxRewindTime(std::chrono::milliseconds max_rewind);
    void SetInterpolationEnabled(bool enabled);
    void SetExtrapolationLimit(std::chrono::milliseconds limit);
    
    // Statistics
    struct LagCompensationStats {
        uint64_t total_rewinds{0};
        uint64_t successful_validations{0};
        uint64_t rejected_hits{0};
        float average_rewind_time_ms{0.0f};
        float max_rewind_time_ms{0.0f};
        std::unordered_map<std::string, uint32_t> rejection_reasons;
    };
    
    LagCompensationStats GetStats() const { return stats_; }
    
private:
    friend class Singleton<LagCompensation>;
    LagCompensation();
    
    // Snapshot storage
    std::deque<WorldSnapshot> snapshots_;
    static constexpr size_t MAX_SNAPSHOTS = 300;  // 5 seconds at 60 Hz
    std::chrono::milliseconds snapshot_interval_{16};  // ~60 Hz
    std::chrono::steady_clock::time_point last_snapshot_time_;
    
    // Player latencies
    std::unordered_map<uint64_t, float> player_latencies_;
    
    // Configuration
    std::chrono::milliseconds max_rewind_time_{1000};  // 1 second max
    bool interpolation_enabled_{true};
    std::chrono::milliseconds extrapolation_limit_{200};
    
    // Statistics
    LagCompensationStats stats_;
    mutable std::shared_mutex mutex_;
    
    // Helper methods
    void CleanupOldSnapshots();
    bool IsTimeValid(std::chrono::steady_clock::time_point time) const;
    float CalculateHitProbability(const Vector3& origin,
                                 const Vector3& target,
                                 float target_radius,
                                 float weapon_spread);
};

// [SEQUENCE: MVP13-57] Rewind context for temporary state
class RewindContext {
public:
    RewindContext(std::chrono::steady_clock::time_point target_time);
    ~RewindContext();
    
    // Get rewound entity state
    std::optional<WorldSnapshot::EntityState> GetEntityState(uint64_t entity_id) const;
    
    // Perform operations in rewound time
    bool PerformRaycast(const Vector3& origin,
                       const Vector3& direction,
                       float max_distance,
                       uint64_t ignore_entity,
                       RaycastHit& hit);
    
    bool CheckCollision(const BoundingBox& box,
                       uint64_t ignore_entity,
                       std::vector<uint64_t>& colliding_entities);
    
private:
    std::optional<WorldSnapshot> snapshot_;
    std::chrono::steady_clock::time_point target_time_;
};

// [SEQUENCE: MVP13-58] Hit registration system
class HitRegistration {
public:
    // [SEQUENCE: MVP13-59] Hit request from client
    struct HitRequest {
        uint64_t request_id;
        uint64_t attacker_id;
        uint64_t weapon_id;
        Vector3 shot_origin;
        Vector3 shot_direction;
        float timestamp;  // Client time
        uint32_t sequence_number;
    };
    
    // Process hit request with lag compensation
    HitValidation ProcessHitRequest(const HitRequest& request);
    
    // Validate melee hit
    HitValidation ValidateMeleeHit(
        uint64_t attacker_id,
        uint64_t victim_id,
        const Vector3& attack_position,
        float attack_range,
        float client_timestamp);
    
    // Area damage validation
    std::vector<HitValidation> ValidateAreaDamage(
        uint64_t attacker_id,
        const Vector3& center,
        float radius,
        float client_timestamp);
    
private:
    // Hit validation helpers
    bool IsAngleValid(const Vector3& attack_direction,
                     const Vector3& to_target,
                     float max_angle);
    
    bool IsDistanceValid(const Vector3& from,
                        const Vector3& to,
                        float max_distance,
                        float tolerance = 0.1f);
};

// [SEQUENCE: MVP13-60] Interpolation utilities
namespace InterpolationUtils {
    // Entity state interpolation
    WorldSnapshot::EntityState InterpolateEntityState(
        const WorldSnapshot::EntityState& state1,
        const WorldSnapshot::EntityState& state2,
        float t);
    
    // Position extrapolation
    Vector3 ExtrapolatePosition(
        const Vector3& position,
        const Vector3& velocity,
        float delta_time);
    
    // Hitbox interpolation
    BoundingBox InterpolateHitbox(
        const BoundingBox& box1,
        const BoundingBox& box2,
        float t);
    
    // Time calculation
    float CalculateInterpolationFactor(
        std::chrono::steady_clock::time_point t1,
        std::chrono::steady_clock::time_point t2,
        std::chrono::steady_clock::time_point target);
}

// [SEQUENCE: MVP13-61] Favor the shooter settings
struct FavorTheShooterSettings {
    float max_rewind_time_ms{1000.0f};      // Maximum time to rewind
    float hit_tolerance{0.1f};               // Hit box expansion
    float movement_tolerance{0.2f};          // Movement validation tolerance
    float max_extrapolation_ms{200.0f};     // Maximum extrapolation time
    bool enable_client_side_hit{true};       // Trust client hits
    float lag_threshold_ms{150.0f};          // High lag threshold
    float confidence_threshold{0.7f};        // Minimum confidence for hit
};

// [SEQUENCE: MVP13-62] Advanced lag compensation manager
class AdvancedLagCompensation {
public:
    AdvancedLagCompensation();
    
    // [SEQUENCE: MVP13-63] Predictive lag compensation
    struct PredictedHit {
        Vector3 predicted_position;
        Vector3 predicted_velocity;
        float probability;
        float time_offset;
    };
    
    PredictedHit PredictTargetPosition(
        uint64_t target_id,
        float shooter_latency,
        float target_latency);
    
    // Fairness balancing
    void SetFairnessBias(float bias);  // 0.0 = favor defender, 1.0 = favor shooter
    
    // Advanced validation
    HitValidation ValidateWithPrediction(
        const HitRequest& request,
        const PredictedHit& prediction);
    
    // Lag spike handling
    void HandleLagSpike(uint64_t player_id, float spike_duration_ms);
    bool IsPlayerLagging(uint64_t player_id) const;
    
    // Settings
    void UpdateSettings(const FavorTheShooterSettings& settings);
    FavorTheShooterSettings GetSettings() const { return settings_; }
    
private:
    FavorTheShooterSettings settings_;
    
    // Lag spike tracking
    struct LagSpikeInfo {
        std::chrono::steady_clock::time_point start_time;
        float duration_ms;
        uint32_t spike_count;
    };
    
    std::unordered_map<uint64_t, LagSpikeInfo> lag_spikes_;
    
    // Fairness bias (0.0 to 1.0)
    float fairness_bias_{0.5f};
    
    // Prediction cache
    struct PredictionCache {
        uint64_t target_id;
        PredictedHit prediction;
        std::chrono::steady_clock::time_point cache_time;
    };
    
    std::unordered_map<uint64_t, PredictionCache> prediction_cache_;
};

// [SEQUENCE: MVP13-64] Rollback networking
class RollbackNetworking {
public:
    // [SEQUENCE: MVP13-65] Rollback state
    struct RollbackState {
        uint32_t frame;
        std::unordered_map<uint64_t, PlayerInput> inputs;
        WorldSnapshot snapshot;
        std::vector<combat::CombatEvent> events;
    };
    
    // Frame management
    void AdvanceFrame();
    uint32_t GetCurrentFrame() const { return current_frame_; }
    
    // Input handling
    void ReceiveInput(uint64_t player_id, const PlayerInput& input, uint32_t frame);
    void ConfirmInput(uint64_t player_id, uint32_t frame);
    
    // Rollback and resimulation
    void Rollback(uint32_t to_frame);
    void Resimulate(uint32_t from_frame, uint32_t to_frame);
    
    // State synchronization
    void BroadcastConfirmedState(uint32_t frame);
    void ReceiveConfirmedState(const RollbackState& state);
    
    // Prediction
    PlayerInput PredictInput(uint64_t player_id, uint32_t frame);
    
private:
    uint32_t current_frame_{0};
    uint32_t confirmed_frame_{0};
    
    // State history
    std::deque<RollbackState> state_history_;
    static constexpr size_t MAX_ROLLBACK_FRAMES = 8;  // ~133ms at 60fps
    
    // Input buffers
    std::unordered_map<uint64_t, std::deque<PlayerInput>> input_buffers_;
    
    // Simulation
    void SimulateFrame(uint32_t frame);
    void ApplyInputs(const std::unordered_map<uint64_t, PlayerInput>& inputs);
};

// [SEQUENCE: MVP13-66] Lag compensation utilities
namespace LagCompensationUtils {
    // Calculate actual server time from client timestamp
    std::chrono::steady_clock::time_point ClientToServerTime(
        float client_timestamp,
        float client_latency);
    
    // Validate timestamp is reasonable
    bool IsTimestampValid(
        float client_timestamp,
        float server_time,
        float max_difference);
    
    // Calculate hit cone at distance
    float CalculateSpreadAtDistance(
        float base_spread,
        float distance);
    
    // Debug visualization
    struct LagCompensationDebug {
        std::vector<Vector3> rewind_positions;
        std::vector<BoundingBox> hitboxes;
        std::vector<std::pair<Vector3, Vector3>> ray_traces;
        float rewind_time_ms;
    };
    
    LagCompensationDebug GenerateDebugInfo(
        const HitRequest& request,
        const WorldSnapshot& snapshot);
}

} // namespace mmorpg::network