#pragma once

#include <memory>
#include <vector>
#include <deque>
#include <unordered_map>
#include <chrono>
#include "../core/types.h"
#include "../core/singleton.h"
#include "../player/player.h"
#include "../physics/physics.h"
#include "packet.h"

namespace mmorpg::network {

// [SEQUENCE: MVP13-28] Player input commands
struct PlayerInput {
    uint32_t sequence_number;
    uint32_t tick;
    std::chrono::steady_clock::time_point timestamp;
    
    // Movement
    Vector3 move_direction;
    bool is_jumping{false};
    bool is_sprinting{false};
    bool is_crouching{false};
    
    // Actions
    uint32_t ability_id{0};
    uint64_t target_id{0};
    Vector3 target_position;
    
    // View
    float yaw;
    float pitch;
    
    // Validation
    uint32_t checksum;
};

// [SEQUENCE: MVP13-29] Predicted state
struct PredictedState {
    uint32_t tick;
    Vector3 position;
    Vector3 velocity;
    float rotation;
    
    // Combat state
    float health;
    float mana;
    std::vector<uint32_t> active_buffs;
    std::vector<uint32_t> cooldowns;
    
    // Animation state
    uint32_t animation_id;
    float animation_time;
};

// [SEQUENCE: MVP13-30] Server authoritative state
struct AuthoritativeState {
    uint32_t tick;
    uint32_t last_processed_input;
    PredictedState state;
    std::chrono::steady_clock::time_point timestamp;
};

// [SEQUENCE: MVP13-31] Client prediction system
class ClientPrediction {
public:
    ClientPrediction(uint64_t player_id);
    
    // [SEQUENCE: MVP13-32] Input handling
    void ProcessInput(const PlayerInput& input);
    uint32_t GetNextSequenceNumber() { return next_sequence_++; }
    
    // State prediction
    void PredictMovement(float delta_time);
    void PredictAbility(uint32_t ability_id, uint64_t target_id);
    
    // Server reconciliation
    void ReceiveServerState(const AuthoritativeState& server_state);
    void ReconcileWithServer();
    
    // State access
    PredictedState GetCurrentState() const { return current_state_; }
    Vector3 GetPredictedPosition() const { return current_state_.position; }
    
    // Rollback and replay
    void Rollback(uint32_t to_tick);
    void Replay(uint32_t from_tick, uint32_t to_tick);
    
    // Statistics
    struct PredictionStats {
        uint32_t predictions_made{0};
        uint32_t corrections_applied{0};
        float average_error{0.0f};
        float max_error{0.0f};
        uint32_t mispredictions{0};
    };
    
    PredictionStats GetStats() const { return stats_; }
    
private:
    uint64_t player_id_;
    
    // Current predicted state
    PredictedState current_state_;
    
    // Input buffer
    std::deque<PlayerInput> input_buffer_;
    uint32_t next_sequence_{0};
    
    // State history for rollback
    std::deque<PredictedState> state_history_;
    static constexpr size_t MAX_STATE_HISTORY = 120;  // 2 seconds at 60fps
    
    // Server state
    AuthoritativeState last_server_state_;
    
    // Physics simulation
    std::unique_ptr<physics::CharacterController> character_controller_;
    
    // Statistics
    PredictionStats stats_;
    
    // Helpers
    void ApplyInput(const PlayerInput& input, PredictedState& state);
    float CalculatePredictionError(const PredictedState& predicted,
                                  const PredictedState& actual);
};

// [SEQUENCE: MVP13-33] Smoothing and interpolation
class StateInterpolator {
public:
    // [SEQUENCE: MVP13-34] Interpolation modes
    enum class InterpolationMode {
        LINEAR,         // 선형 보간
        CUBIC,          // 3차 보간
        HERMITE,        // 에르미트 보간
        EXTRAPOLATE     // 외삽
    };
    
    // Add state snapshot
    void AddSnapshot(const PredictedState& state, 
                    std::chrono::steady_clock::time_point timestamp);
    
    // Get interpolated state
    PredictedState GetInterpolatedState(
        std::chrono::steady_clock::time_point target_time,
        InterpolationMode mode = InterpolationMode::HERMITE);
    
    // Smoothing
    void EnableSmoothing(bool enable) { smoothing_enabled_ = enable; }
    void SetSmoothingFactor(float factor) { smoothing_factor_ = factor; }
    
private:
    struct StateSnapshot {
        PredictedState state;
        std::chrono::steady_clock::time_point timestamp;
    };
    
    std::deque<StateSnapshot> snapshots_;
    static constexpr size_t MAX_SNAPSHOTS = 10;
    
    bool smoothing_enabled_{true};
    float smoothing_factor_{0.1f};
    
    // Interpolation helpers
    Vector3 InterpolatePosition(const Vector3& p0, const Vector3& p1,
                               const Vector3& v0, const Vector3& v1,
                               float t, InterpolationMode mode);
};

// [SEQUENCE: MVP13-35] Prediction manager (server-side)
class PredictionManager : public Singleton<PredictionManager> {
public:
    // [SEQUENCE: MVP13-36] Input validation
    bool ValidateInput(uint64_t player_id, const PlayerInput& input);
    void ProcessPlayerInput(uint64_t player_id, const PlayerInput& input);
    
    // State management
    void UpdatePlayerState(uint64_t player_id, const PredictedState& state);
    AuthoritativeState GetAuthoritativeState(uint64_t player_id);
    
    // Tick management
    void AdvanceTick();
    uint32_t GetCurrentTick() const { return current_tick_; }
    uint32_t GetTickRate() const { return tick_rate_; }
    
    // Input buffer management
    std::vector<PlayerInput> GetUnprocessedInputs(uint64_t player_id,
                                                 uint32_t since_sequence);
    void AcknowledgeInput(uint64_t player_id, uint32_t sequence_number);
    
    // Anti-cheat
    struct ValidationResult {
        bool valid{true};
        std::string reason;
        float confidence{1.0f};
    };
    
    ValidationResult ValidateMovement(uint64_t player_id,
                                    const Vector3& from,
                                    const Vector3& to,
                                    float delta_time);
    
    // Statistics
    struct GlobalPredictionStats {
        uint64_t total_inputs_processed{0};
        uint64_t invalid_inputs_rejected{0};
        float average_input_latency{0.0f};
        std::unordered_map<uint64_t, float> player_latencies;
    };
    
    GlobalPredictionStats GetGlobalStats() const { return global_stats_; }
    
private:
    friend class Singleton<PredictionManager>;
    PredictionManager();
    
    // Player states
    std::unordered_map<uint64_t, AuthoritativeState> player_states_;
    std::unordered_map<uint64_t, std::deque<PlayerInput>> input_buffers_;
    
    // Tick management
    uint32_t current_tick_{0};
    uint32_t tick_rate_{60};
    std::chrono::steady_clock::time_point last_tick_time_;
    
    // Statistics
    GlobalPredictionStats global_stats_;
    mutable std::shared_mutex mutex_;
    
    // Validation helpers
    bool IsMovementValid(const Vector3& velocity, float delta_time);
    bool IsInputSequenceValid(uint64_t player_id, uint32_t sequence_number);
};

// [SEQUENCE: MVP13-37] Client-side prediction handler
class ClientPredictionHandler {
public:
    ClientPredictionHandler(std::shared_ptr<Connection> connection);
    
    // [SEQUENCE: MVP13-38] Input collection
    void CollectInput();
    void SendInput(const PlayerInput& input);
    
    // Prediction loop
    void Update(float delta_time);
    void FixedUpdate();  // Physics tick
    
    // Server communication
    void OnServerStateReceived(PacketPtr packet);
    void OnInputAcknowledged(uint32_t sequence_number);
    
    // Rendering
    PredictedState GetRenderState();
    Vector3 GetSmoothedPosition();
    
    // Configuration
    void SetPredictionEnabled(bool enabled) { prediction_enabled_ = enabled; }
    void SetInputBufferSize(size_t size) { max_input_buffer_size_ = size; }
    
private:
    std::shared_ptr<Connection> connection_;
    std::unique_ptr<ClientPrediction> prediction_;
    std::unique_ptr<StateInterpolator> interpolator_;
    
    // Configuration
    bool prediction_enabled_{true};
    size_t max_input_buffer_size_{120};
    
    // Timing
    float accumulated_time_{0.0f};
    float fixed_timestep_{1.0f / 60.0f};
    
    // Input state
    PlayerInput current_input_;
    uint32_t last_acknowledged_input_{0};
};

// [SEQUENCE: MVP13-39] Prediction utilities
namespace PredictionUtils {
    // Input compression
    std::vector<uint8_t> CompressInput(const PlayerInput& input);
    PlayerInput DecompressInput(const std::vector<uint8_t>& data);
    
    // State validation
    bool IsStateValid(const PredictedState& state);
    PredictedState SanitizeState(const PredictedState& state);
    
    // Error correction
    Vector3 ApplyErrorCorrection(const Vector3& current,
                                const Vector3& target,
                                float correction_rate);
    
    // Prediction helpers
    Vector3 PredictPosition(const Vector3& position,
                          const Vector3& velocity,
                          const Vector3& acceleration,
                          float delta_time);
    
    // Debug visualization
    struct PredictionDebugInfo {
        std::vector<Vector3> predicted_path;
        std::vector<Vector3> actual_path;
        std::vector<float> error_magnitudes;
        uint32_t rollback_count;
    };
    
    PredictionDebugInfo GenerateDebugInfo(const ClientPrediction& prediction);
}

// [SEQUENCE: MVP13-40] Ability prediction
class AbilityPredictor {
public:
    // [SEQUENCE: MVP13-41] Ability prediction result
    struct PredictionResult {
        bool can_cast{false};
        float cast_time{0.0f};
        float cooldown_remaining{0.0f};
        uint32_t mana_cost{0};
        std::vector<uint64_t> affected_targets;
        Vector3 predicted_effect_position;
    };
    
    // Predict ability execution
    PredictionResult PredictAbility(uint32_t ability_id,
                                  uint64_t caster_id,
                                  uint64_t target_id,
                                  const Vector3& target_position);
    
    // Update cooldowns
    void UpdateCooldowns(float delta_time);
    void SetCooldown(uint32_t ability_id, float duration);
    
    // Mana prediction
    bool PredictManaCost(uint32_t ability_id, float current_mana);
    
private:
    std::unordered_map<uint32_t, float> cooldowns_;
    std::unordered_map<uint32_t, AbilityData> ability_database_;
};

// [SEQUENCE: MVP13-42] Movement predictor
class MovementPredictor {
public:
    // Movement constraints
    struct MovementConstraints {
        float max_walk_speed{5.0f};
        float max_run_speed{10.0f};
        float max_sprint_speed{15.0f};
        float acceleration{20.0f};
        float deceleration{30.0f};
        float jump_height{2.0f};
        float gravity{-9.81f};
        float air_control{0.3f};
    };
    
    // Predict movement
    Vector3 PredictMovement(const Vector3& position,
                          const Vector3& velocity,
                          const PlayerInput& input,
                          float delta_time,
                          const MovementConstraints& constraints);
    
    // Ground detection
    bool PredictGrounded(const Vector3& position, const Vector3& velocity);
    
    // Collision prediction
    Vector3 PredictCollisionResponse(const Vector3& position,
                                   const Vector3& desired_movement);
    
private:
    // Physics helpers
    Vector3 ApplyGravity(const Vector3& velocity, float delta_time, float gravity);
    Vector3 ApplyFriction(const Vector3& velocity, float friction_coefficient);
    Vector3 ClampVelocity(const Vector3& velocity, float max_speed);
};

} // namespace mmorpg::network
