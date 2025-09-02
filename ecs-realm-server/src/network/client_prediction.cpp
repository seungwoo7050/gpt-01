#include "client_prediction.h"
#include "../core/logger.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

namespace mmorpg::network {

// [SEQUENCE: MVP13-43] Client prediction implementation
ClientPrediction::ClientPrediction(uint64_t player_id)
    : player_id_(player_id) {
    
    // Initialize character controller
    character_controller_ = std::make_unique<physics::CharacterController>();
    
    // Initialize current state
    current_state_.tick = 0;
    current_state_.position = Vector3::Zero();
    current_state_.velocity = Vector3::Zero();
    current_state_.rotation = 0.0f;
    current_state_.health = 100.0f;
    current_state_.mana = 100.0f;
    
    spdlog::debug("[Prediction] Client prediction initialized for player {}", player_id);
}

// [SEQUENCE: MVP13-44] Process player input
void ClientPrediction::ProcessInput(const PlayerInput& input) {
    // Store input in buffer
    input_buffer_.push_back(input);
    
    // Limit buffer size
    while (input_buffer_.size() > 120) {  // 2 seconds at 60fps
        input_buffer_.pop_front();
    }
    
    // Apply input to current state
    ApplyInput(input, current_state_);
    
    // Store state in history
    state_history_.push_back(current_state_);
    while (state_history_.size() > MAX_STATE_HISTORY) {
        state_history_.pop_front();
    }
    
    stats_.predictions_made++;
}

void ClientPrediction::ApplyInput(const PlayerInput& input, PredictedState& state) {
    const float delta_time = 1.0f / 60.0f;  // Fixed timestep
    
    // Movement prediction
    Vector3 acceleration = input.move_direction * 20.0f;  // Base acceleration
    
    if (input.is_sprinting) {
        acceleration *= 1.5f;
    } else if (input.is_crouching) {
        acceleration *= 0.5f;
    }
    
    // Update velocity
    state.velocity += acceleration * delta_time;
    
    // Apply friction
    if (input.move_direction.Length() < 0.01f) {
        state.velocity *= 0.9f;  // Deceleration
    }
    
    // Clamp velocity
    float max_speed = input.is_sprinting ? 15.0f : 10.0f;
    if (state.velocity.Length() > max_speed) {
        state.velocity = state.velocity.Normalized() * max_speed;
    }
    
    // Update position
    state.position += state.velocity * delta_time;
    
    // Update rotation
    state.rotation = input.yaw;
    
    // Jumping
    if (input.is_jumping && character_controller_->IsGrounded()) {
        state.velocity.y = 10.0f;  // Jump velocity
    }
    
    // Apply gravity
    if (!character_controller_->IsGrounded()) {
        state.velocity.y -= 9.81f * delta_time;
    }
    
    // Update tick
    state.tick = input.tick;
}

// [SEQUENCE: MVP13-45] Receive server state
void ClientPrediction::ReceiveServerState(const AuthoritativeState& server_state) {
    last_server_state_ = server_state;
    
    // Find the state at server tick
    auto it = std::find_if(state_history_.begin(), state_history_.end(),
                          [&](const PredictedState& state) {
                              return state.tick == server_state.tick;
                          });
    
    if (it != state_history_.end()) {
        // Calculate prediction error
        float error = CalculatePredictionError(*it, server_state.state);
        stats_.average_error = (stats_.average_error * stats_.corrections_applied + error) /
                              (stats_.corrections_applied + 1);
        stats_.max_error = std::max(stats_.max_error, error);
        
        if (error > 0.1f) {  // Significant error
            stats_.mispredictions++;
            ReconcileWithServer();
        }
    }
    
    stats_.corrections_applied++;
}

void ClientPrediction::ReconcileWithServer() {
    // Find the corresponding state in history
    auto history_it = std::find_if(state_history_.begin(), state_history_.end(),
                                  [this](const PredictedState& state) {
                                      return state.tick == last_server_state_.tick;
                                  });
    
    if (history_it == state_history_.end()) {
        spdlog::warn("[Prediction] Server state tick {} not found in history",
                    last_server_state_.tick);
        return;
    }
    
    // Replace with server state
    *history_it = last_server_state_.state;
    
    // Find corresponding input
    auto input_it = std::find_if(input_buffer_.begin(), input_buffer_.end(),
                                [this](const PlayerInput& input) {
                                    return input.sequence_number > 
                                           last_server_state_.last_processed_input;
                                });
    
    if (input_it == input_buffer_.end()) {
        // No unprocessed inputs
        current_state_ = last_server_state_.state;
        return;
    }
    
    // Replay from server state
    PredictedState replay_state = last_server_state_.state;
    
    for (auto it = input_it; it != input_buffer_.end(); ++it) {
        ApplyInput(*it, replay_state);
    }
    
    current_state_ = replay_state;
    
    spdlog::debug("[Prediction] Reconciled with server, replayed {} inputs",
                 std::distance(input_it, input_buffer_.end()));
}

float ClientPrediction::CalculatePredictionError(const PredictedState& predicted,
                                               const PredictedState& actual) {
    return Vector3::Distance(predicted.position, actual.position);
}

// [SEQUENCE: MVP13-46] State interpolator implementation
void StateInterpolator::AddSnapshot(const PredictedState& state,
                                  std::chrono::steady_clock::time_point timestamp) {
    snapshots_.push_back({state, timestamp});
    
    // Keep only recent snapshots
    while (snapshots_.size() > MAX_SNAPSHOTS) {
        snapshots_.pop_front();
    }
}

PredictedState StateInterpolator::GetInterpolatedState(
    std::chrono::steady_clock::time_point target_time,
    InterpolationMode mode) {
    
    if (snapshots_.empty()) {
        return PredictedState{};
    }
    
    if (snapshots_.size() == 1) {
        return snapshots_.front().state;
    }
    
    // Find surrounding snapshots
    StateSnapshot* before = nullptr;
    StateSnapshot* after = nullptr;
    
    for (size_t i = 0; i < snapshots_.size() - 1; ++i) {
        if (snapshots_[i].timestamp <= target_time &&
            snapshots_[i + 1].timestamp > target_time) {
            before = &snapshots_[i];
            after = &snapshots_[i + 1];
            break;
        }
    }
    
    if (!before || !after) {
        // Extrapolation needed
        if (target_time < snapshots_.front().timestamp) {
            return snapshots_.front().state;
        } else {
            return snapshots_.back().state;
        }
    }
    
    // Calculate interpolation factor
    auto duration = std::chrono::duration<float>(
        after->timestamp - before->timestamp).count();
    auto elapsed = std::chrono::duration<float>(
        target_time - before->timestamp).count();
    float t = elapsed / duration;
    
    // Interpolate state
    PredictedState result;
    result.tick = before->state.tick;  // Use earlier tick
    
    // Interpolate position
    result.position = InterpolatePosition(
        before->state.position, after->state.position,
        before->state.velocity, after->state.velocity,
        t, mode);
    
    // Linear interpolation for other values
    result.velocity = before->state.velocity * (1.0f - t) + after->state.velocity * t;
    result.rotation = before->state.rotation * (1.0f - t) + after->state.rotation * t;
    result.health = before->state.health * (1.0f - t) + after->state.health * t;
    result.mana = before->state.mana * (1.0f - t) + after->state.mana * t;
    
    return result;
}

Vector3 StateInterpolator::InterpolatePosition(const Vector3& p0, const Vector3& p1,
                                             const Vector3& v0, const Vector3& v1,
                                             float t, InterpolationMode mode) {
    switch (mode) {
        case InterpolationMode::LINEAR:
            return p0 * (1.0f - t) + p1 * t;
            
        case InterpolationMode::CUBIC: {
            float t2 = t * t;
            float t3 = t2 * t;
            return p0 * (2 * t3 - 3 * t2 + 1) +
                   p1 * (-2 * t3 + 3 * t2) +
                   v0 * (t3 - 2 * t2 + t) +
                   v1 * (t3 - t2);
        }
        
        case InterpolationMode::HERMITE: {
            float t2 = t * t;
            float t3 = t2 * t;
            float h00 = 2 * t3 - 3 * t2 + 1;
            float h10 = t3 - 2 * t2 + t;
            float h01 = -2 * t3 + 3 * t2;
            float h11 = t3 - t2;
            return p0 * h00 + v0 * h10 + p1 * h01 + v1 * h11;
        }
        
        case InterpolationMode::EXTRAPOLATE:
            return p1 + v1 * t;
            
        default:
            return p0 * (1.0f - t) + p1 * t;
    }
}

// [SEQUENCE: MVP13-47] Prediction manager implementation
PredictionManager::PredictionManager() {
    last_tick_time_ = std::chrono::steady_clock::now();
    spdlog::info("[Prediction] Prediction manager initialized");
}

bool PredictionManager::ValidateInput(uint64_t player_id, const PlayerInput& input) {
    // Check sequence number
    if (!IsInputSequenceValid(player_id, input.sequence_number)) {
        global_stats_.invalid_inputs_rejected++;
        return false;
    }
    
    // Validate checksum
    // TODO: Implement checksum validation
    
    // Validate movement
    if (!IsMovementValid(input.move_direction * 10.0f, 1.0f / 60.0f)) {
        global_stats_.invalid_inputs_rejected++;
        return false;
    }
    
    return true;
}

void PredictionManager::ProcessPlayerInput(uint64_t player_id, const PlayerInput& input) {
    if (!ValidateInput(player_id, input)) {
        spdlog::warn("[Prediction] Invalid input from player {}", player_id);
        return;
    }
    
    std::unique_lock lock(mutex_);
    
    // Store input
    input_buffers_[player_id].push_back(input);
    
    // Limit buffer size
    auto& buffer = input_buffers_[player_id];
    while (buffer.size() > 120) {
        buffer.pop_front();
    }
    
    // Update latency tracking
    auto now = std::chrono::steady_clock::now();
    auto latency = std::chrono::duration<float, std::milli>(
        now - input.timestamp).count();
    global_stats_.player_latencies[player_id] = latency;
    
    // Update average
    float total_latency = 0.0f;
    for (const auto& [id, lat] : global_stats_.player_latencies) {
        total_latency += lat;
    }
    global_stats_.average_input_latency = total_latency / 
                                         global_stats_.player_latencies.size();
    
    global_stats_.total_inputs_processed++;
}

void PredictionManager::AdvanceTick() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration<float>(
        now - last_tick_time_).count();
    
    if (elapsed >= 1.0f / tick_rate_) {
        current_tick_++;
        last_tick_time_ = now;
        
        // Process pending inputs for all players
        std::shared_lock lock(mutex_);
        for (auto& [player_id, buffer] : input_buffers_) {
            // Process inputs up to current tick
            while (!buffer.empty() && buffer.front().tick <= current_tick_) {
                // Apply input to player state
                // TODO: Apply physics simulation
                buffer.pop_front();
            }
        }
    }
}

bool PredictionManager::IsMovementValid(const Vector3& velocity, float delta_time) {
    float max_speed = 20.0f;  // Maximum possible speed
    float speed = velocity.Length();
    
    if (speed > max_speed * 1.1f) {  // 10% tolerance
        return false;
    }
    
    // Check acceleration
    // TODO: Compare with previous velocity
    
    return true;
}

bool PredictionManager::IsInputSequenceValid(uint64_t player_id, uint32_t sequence_number) {
    auto it = input_buffers_.find(player_id);
    if (it == input_buffers_.end() || it->second.empty()) {
        return true;  // First input
    }
    
    uint32_t last_sequence = it->second.back().sequence_number;
    
    // Allow some out-of-order packets
    if (sequence_number <= last_sequence && 
        last_sequence - sequence_number > 10) {
        return false;  // Too old
    }
    
    return true;
}

// [SEQUENCE: MVP13-48] Client prediction handler implementation
ClientPredictionHandler::ClientPredictionHandler(std::shared_ptr<Connection> connection)
    : connection_(connection) {
    
    uint64_t player_id = connection->GetPlayerId();
    prediction_ = std::make_unique<ClientPrediction>(player_id);
    interpolator_ = std::make_unique<StateInterpolator>();
}

void ClientPredictionHandler::Update(float delta_time) {
    if (!prediction_enabled_) {
        return;
    }
    
    // Accumulate time for fixed timestep
    accumulated_time_ += delta_time;
    
    // Run fixed updates
    while (accumulated_time_ >= fixed_timestep_) {
        FixedUpdate();
        accumulated_time_ -= fixed_timestep_;
    }
    
    // Add current state to interpolator
    auto state = prediction_->GetCurrentState();
    interpolator_->AddSnapshot(state, std::chrono::steady_clock::now());
}

void ClientPredictionHandler::FixedUpdate() {
    // Collect and process input
    CollectInput();
    
    // Send input to server
    SendInput(current_input_);
    
    // Predict next state
    prediction_->ProcessInput(current_input_);
}

void ClientPredictionHandler::CollectInput() {
    // TODO: Collect input from input system
    current_input_.sequence_number = prediction_->GetNextSequenceNumber();
    current_input_.tick = PredictionManager::Instance().GetCurrentTick();
    current_input_.timestamp = std::chrono::steady_clock::now();
    
    // Example input collection
    // current_input_.move_direction = GetMovementInput();
    // current_input_.is_jumping = IsJumpPressed();
    // etc.
}

void ClientPredictionHandler::SendInput(const PlayerInput& input) {
    auto packet = std::make_shared<Packet>(PacketType::PLAYER_INPUT);
    
    // Compress input
    auto compressed = PredictionUtils::CompressInput(input);
    packet->SetData(compressed);
    
    // Send with high priority
    connection_->Send(packet);
}

void ClientPredictionHandler::OnServerStateReceived(PacketPtr packet) {
    // Deserialize server state
    AuthoritativeState server_state;
    // TODO: Deserialize from packet
    
    // Apply to prediction
    prediction_->ReceiveServerState(server_state);
}

PredictedState ClientPredictionHandler::GetRenderState() {
    // Get interpolated state for smooth rendering
    auto target_time = std::chrono::steady_clock::now() - 
                      std::chrono::milliseconds(100);  // Render 100ms in the past
    
    return interpolator_->GetInterpolatedState(
        target_time, StateInterpolator::InterpolationMode::HERMITE);
}

// [SEQUENCE: MVP13-49] Prediction utilities implementation
namespace PredictionUtils {

std::vector<uint8_t> CompressInput(const PlayerInput& input) {
    // Simple compression: pack bits for boolean values
    std::vector<uint8_t> compressed;
    
    // Write sequence and tick
    compressed.resize(sizeof(uint32_t) * 2);
    memcpy(compressed.data(), &input.sequence_number, sizeof(uint32_t));
    memcpy(compressed.data() + sizeof(uint32_t), &input.tick, sizeof(uint32_t));
    
    // Pack movement direction (quantized)
    // TODO: Implement proper compression
    
    // Pack flags
    uint8_t flags = 0;
    if (input.is_jumping) flags |= 0x01;
    if (input.is_sprinting) flags |= 0x02;
    if (input.is_crouching) flags |= 0x04;
    compressed.push_back(flags);
    
    return compressed;
}

Vector3 ApplyErrorCorrection(const Vector3& current,
                           const Vector3& target,
                           float correction_rate) {
    Vector3 error = target - current;
    return current + error * correction_rate;
}

Vector3 PredictPosition(const Vector3& position,
                      const Vector3& velocity,
                      const Vector3& acceleration,
                      float delta_time) {
    // Standard kinematic equation: p = p0 + v0*t + 0.5*a*t^2
    return position + 
           velocity * delta_time + 
           acceleration * (0.5f * delta_time * delta_time);
}

} // namespace PredictionUtils

// [SEQUENCE: MVP13-50] Movement predictor implementation
Vector3 MovementPredictor::PredictMovement(
    const Vector3& position,
    const Vector3& velocity,
    const PlayerInput& input,
    float delta_time,
    const MovementConstraints& constraints) {
    
    Vector3 new_velocity = velocity;
    
    // Apply input acceleration
    Vector3 desired_velocity = input.move_direction * 
        (input.is_sprinting ? constraints.max_sprint_speed : constraints.max_run_speed);
    
    // Acceleration towards desired velocity
    Vector3 velocity_diff = desired_velocity - new_velocity;
    float accel = input.move_direction.Length() > 0.01f ? 
                  constraints.acceleration : constraints.deceleration;
    
    new_velocity += velocity_diff.Normalized() * accel * delta_time;
    
    // Apply gravity if not grounded
    if (!PredictGrounded(position, velocity)) {
        new_velocity.y += constraints.gravity * delta_time;
        
        // Air control
        new_velocity.x *= (1.0f - constraints.air_control);
        new_velocity.z *= (1.0f - constraints.air_control);
    }
    
    // Clamp velocity
    new_velocity = ClampVelocity(new_velocity, constraints.max_sprint_speed);
    
    // Predict new position
    Vector3 new_position = position + new_velocity * delta_time;
    
    // Apply collision
    new_position = PredictCollisionResponse(position, new_position - position);
    
    return new_position;
}

bool MovementPredictor::PredictGrounded(const Vector3& position, const Vector3& velocity) {
    // Simple ground check
    const float ground_height = 0.0f;
    const float ground_threshold = 0.1f;
    
    return position.y <= ground_height + ground_threshold && velocity.y <= 0.0f;
}

Vector3 MovementPredictor::ClampVelocity(const Vector3& velocity, float max_speed) {
    float speed = velocity.Length();
    if (speed > max_speed) {
        return velocity.Normalized() * max_speed;
    }
    return velocity;
}

} // namespace mmorpg::network