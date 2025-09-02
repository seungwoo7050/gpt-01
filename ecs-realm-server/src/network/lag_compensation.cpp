#include "lag_compensation.h"
#include "../core/logger.h"
#include "../physics/physics.h"
#include "../world/world_manager.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cmath>

namespace mmorpg::network {

// [SEQUENCE: MVP13-67] Lag compensation constructor
LagCompensation::LagCompensation() {
    last_snapshot_time_ = std::chrono::steady_clock::now();
    spdlog::info("[LagCompensation] System initialized");
}

// [SEQUENCE: MVP13-68] Record world snapshot
void LagCompensation::RecordSnapshot() {
    auto now = std::chrono::steady_clock::now();
    
    // Check if enough time has passed
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_snapshot_time_);
    
    if (elapsed < snapshot_interval_) {
        return;
    }
    
    std::unique_lock lock(mutex_);
    
    // Create new snapshot
    WorldSnapshot snapshot;
    snapshot.tick = world::WorldManager::Instance().GetCurrentTick();
    snapshot.timestamp = now;
    
    // Record all entity states
    auto& world = world::WorldManager::Instance();
    for (const auto& entity : world.GetAllEntities()) {
        WorldSnapshot::EntityState state;
        state.entity_id = entity->GetId();
        state.position = entity->GetPosition();
        state.velocity = entity->GetVelocity();
        state.rotation = entity->GetRotation();
        state.hitbox = entity->GetHitbox();
        state.health = entity->GetHealth();
        state.is_alive = entity->IsAlive();
        
        snapshot.entity_states[state.entity_id] = state;
    }
    
    // Record projectile states
    for (const auto& projectile : world.GetActiveProjectiles()) {
        WorldSnapshot::ProjectileState proj_state;
        proj_state.projectile_id = projectile->GetId();
        proj_state.position = projectile->GetPosition();
        proj_state.velocity = projectile->GetVelocity();
        proj_state.radius = projectile->GetRadius();
        proj_state.owner_id = projectile->GetOwnerId();
        
        snapshot.projectile_states.push_back(proj_state);
    }
    
    // Add to history
    snapshots_.push_back(std::move(snapshot));
    last_snapshot_time_ = now;
    
    // Cleanup old snapshots
    CleanupOldSnapshots();
}

void LagCompensation::CleanupOldSnapshots() {
    auto now = std::chrono::steady_clock::now();
    auto cutoff_time = now - max_rewind_time_ - std::chrono::milliseconds(100);  // Buffer
    
    while (!snapshots_.empty() && snapshots_.front().timestamp < cutoff_time) {
        snapshots_.pop_front();
    }
    
    // Ensure we don't exceed max snapshots
    while (snapshots_.size() > MAX_SNAPSHOTS) {
        snapshots_.pop_front();
    }
}

// [SEQUENCE: MVP13-69] Get snapshot at specific time
std::optional<WorldSnapshot> LagCompensation::GetSnapshotAtTime(
    std::chrono::steady_clock::time_point target_time) {
    
    std::shared_lock lock(mutex_);
    
    if (snapshots_.empty()) {
        return std::nullopt;
    }
    
    // Check if time is valid
    if (!IsTimeValid(target_time)) {
        spdlog::warn("[LagCompensation] Invalid rewind time requested");
        return std::nullopt;
    }
    
    // Find surrounding snapshots
    const WorldSnapshot* before = nullptr;
    const WorldSnapshot* after = nullptr;
    
    for (size_t i = 0; i < snapshots_.size() - 1; ++i) {
        if (snapshots_[i].timestamp <= target_time &&
            snapshots_[i + 1].timestamp > target_time) {
            before = &snapshots_[i];
            after = &snapshots_[i + 1];
            break;
        }
    }
    
    // Exact match
    if (before && before->timestamp == target_time) {
        return *before;
    }
    
    // Interpolation needed
    if (before && after && interpolation_enabled_) {
        return InterpolateSnapshots(*before, *after, target_time);
    }
    
    // Return closest
    if (target_time <= snapshots_.front().timestamp) {
        return snapshots_.front();
    } else if (target_time >= snapshots_.back().timestamp) {
        // Extrapolation with limit
        auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(
            target_time - snapshots_.back().timestamp);
        
        if (time_diff <= extrapolation_limit_) {
            // Simple extrapolation
            WorldSnapshot extrapolated = snapshots_.back();
            float dt = time_diff.count() / 1000.0f;
            
            for (auto& [id, state] : extrapolated.entity_states) {
                state.position += state.velocity * dt;
            }
            
            return extrapolated;
        }
        
        return snapshots_.back();
    }
    
    return std::nullopt;
}

// [SEQUENCE: MVP13-70] Interpolate between snapshots
WorldSnapshot LagCompensation::InterpolateSnapshots(
    const WorldSnapshot& before,
    const WorldSnapshot& after,
    std::chrono::steady_clock::time_point target_time) {
    
    float t = InterpolationUtils::CalculateInterpolationFactor(
        before.timestamp, after.timestamp, target_time);
    
    WorldSnapshot interpolated;
    interpolated.tick = before.tick;  // Use earlier tick
    interpolated.timestamp = target_time;
    
    // Interpolate entity states
    for (const auto& [id, before_state] : before.entity_states) {
        auto after_it = after.entity_states.find(id);
        if (after_it != after.entity_states.end()) {
            interpolated.entity_states[id] = InterpolationUtils::InterpolateEntityState(
                before_state, after_it->second, t);
        } else {
            // Entity disappeared, use last known state
            interpolated.entity_states[id] = before_state;
        }
    }
    
    // Handle projectiles (no interpolation, too fast)
    interpolated.projectile_states = t < 0.5f ? 
        before.projectile_states : after.projectile_states;
    
    return interpolated;
}

// [SEQUENCE: MVP13-71] Validate hit with lag compensation
HitValidation LagCompensation::ValidateHit(
    uint64_t attacker_id,
    uint64_t victim_id,
    const Vector3& shot_origin,
    const Vector3& shot_direction,
    float max_range,
    std::chrono::steady_clock::time_point shot_time) {
    
    HitValidation result;
    
    // Get attacker latency
    float latency = GetPlayerLatency(attacker_id);
    if (latency < 0) {
        result.rejection_reason = "Unknown player latency";
        return result;
    }
    
    // Calculate server time when shot was fired
    auto server_shot_time = shot_time - std::chrono::milliseconds(static_cast<int>(latency));
    
    // Get world state at that time
    auto snapshot_opt = GetSnapshotAtTime(server_shot_time);
    if (!snapshot_opt) {
        result.rejection_reason = "No snapshot available";
        return result;
    }
    
    const auto& snapshot = *snapshot_opt;
    
    // Find victim state
    auto victim_it = snapshot.entity_states.find(victim_id);
    if (victim_it == snapshot.entity_states.end()) {
        result.rejection_reason = "Victim not found in snapshot";
        return result;
    }
    
    const auto& victim_state = victim_it->second;
    
    // Check if victim was alive
    if (!victim_state.is_alive) {
        result.rejection_reason = "Victim was already dead";
        return result;
    }
    
    // Perform raycast in rewound world
    physics::RaycastHit hit;
    bool hit_something = physics::Raycast(
        shot_origin,
        shot_direction,
        max_range,
        victim_state.hitbox,
        hit);
    
    if (!hit_something) {
        result.rejection_reason = "Raycast missed";
        return result;
    }
    
    // Validate hit distance
    float hit_distance = Vector3::Distance(shot_origin, hit.point);
    if (hit_distance > max_range) {
        result.rejection_reason = "Hit beyond max range";
        return result;
    }
    
    // Calculate confidence based on latency
    float confidence = 1.0f - (latency / 1000.0f);  // Lower confidence with higher latency
    confidence = std::clamp(confidence, 0.0f, 1.0f);
    
    // Success
    result.is_valid = true;
    result.impact_point = hit.point;
    result.victim_id = victim_id;
    result.confidence = confidence;
    
    // Update statistics
    stats_.total_rewinds++;
    stats_.successful_validations++;
    
    auto rewind_time = std::chrono::duration<float, std::milli>(
        std::chrono::steady_clock::now() - server_shot_time).count();
    stats_.average_rewind_time_ms = 
        (stats_.average_rewind_time_ms * (stats_.total_rewinds - 1) + rewind_time) / 
        stats_.total_rewinds;
    stats_.max_rewind_time_ms = std::max(stats_.max_rewind_time_ms, rewind_time);
    
    spdlog::debug("[LagCompensation] Hit validated: attacker={}, victim={}, latency={}ms",
                 attacker_id, victim_id, latency);
    
    return result;
}

// [SEQUENCE: MVP13-72] Movement validation
bool LagCompensation::ValidateMovement(
    uint64_t player_id,
    const Vector3& from_position,
    const Vector3& to_position,
    std::chrono::steady_clock::time_point from_time,
    std::chrono::steady_clock::time_point to_time) {
    
    // Get snapshots at both times
    auto from_snapshot = GetSnapshotAtTime(from_time);
    auto to_snapshot = GetSnapshotAtTime(to_time);
    
    if (!from_snapshot || !to_snapshot) {
        return false;  // Can't validate without data
    }
    
    // Check player state at start
    auto from_it = from_snapshot->entity_states.find(player_id);
    if (from_it == from_snapshot->entity_states.end()) {
        return false;
    }
    
    // Calculate time delta
    auto time_delta = std::chrono::duration<float>(
        to_time - from_time).count();
    
    // Calculate distance moved
    float distance = Vector3::Distance(from_position, to_position);
    float speed = distance / time_delta;
    
    // Check against maximum possible speed
    const float MAX_SPEED = 20.0f;  // m/s
    const float TOLERANCE = 1.1f;   // 10% tolerance
    
    if (speed > MAX_SPEED * TOLERANCE) {
        spdlog::warn("[LagCompensation] Movement validation failed: speed {} > max {}",
                    speed, MAX_SPEED);
        return false;
    }
    
    // TODO: Add collision checks along path
    
    return true;
}

bool LagCompensation::IsTimeValid(std::chrono::steady_clock::time_point time) const {
    auto now = std::chrono::steady_clock::now();
    
    // Can't be in the future
    if (time > now) {
        return false;
    }
    
    // Can't be too far in the past
    auto age = std::chrono::duration_cast<std::chrono::milliseconds>(now - time);
    if (age > max_rewind_time_) {
        return false;
    }
    
    return true;
}

// [SEQUENCE: MVP13-73] Player latency tracking
void LagCompensation::UpdatePlayerLatency(uint64_t player_id, float latency_ms) {
    std::unique_lock lock(mutex_);
    player_latencies_[player_id] = latency_ms;
}

float LagCompensation::GetPlayerLatency(uint64_t player_id) const {
    std::shared_lock lock(mutex_);
    auto it = player_latencies_.find(player_id);
    return (it != player_latencies_.end()) ? it->second : -1.0f;
}

// [SEQUENCE: MVP13-74] Rewind context implementation
RewindContext::RewindContext(std::chrono::steady_clock::time_point target_time)
    : target_time_(target_time) {
    
    snapshot_ = LagCompensation::Instance().GetSnapshotAtTime(target_time);
    if (!snapshot_) {
        spdlog::warn("[RewindContext] Failed to get snapshot for rewind");
    }
}

RewindContext::~RewindContext() {
    // Restore world state if needed
    // For now, we're just reading, not modifying
}

std::optional<WorldSnapshot::EntityState> RewindContext::GetEntityState(
    uint64_t entity_id) const {
    
    if (!snapshot_) {
        return std::nullopt;
    }
    
    auto it = snapshot_->entity_states.find(entity_id);
    if (it != snapshot_->entity_states.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

bool RewindContext::PerformRaycast(
    const Vector3& origin,
    const Vector3& direction,
    float max_distance,
    uint64_t ignore_entity,
    physics::RaycastHit& hit) {
    
    if (!snapshot_) {
        return false;
    }
    
    // Check against all entities in snapshot
    float closest_distance = max_distance;
    bool hit_something = false;
    
    for (const auto& [id, state] : snapshot_->entity_states) {
        if (id == ignore_entity || !state.is_alive) {
            continue;
        }
        
        physics::RaycastHit temp_hit;
        if (physics::RaycastBox(origin, direction, state.hitbox, temp_hit)) {
            float distance = Vector3::Distance(origin, temp_hit.point);
            if (distance < closest_distance) {
                closest_distance = distance;
                hit = temp_hit;
                hit.entity_id = id;
                hit_something = true;
            }
        }
    }
    
    return hit_something;
}

// [SEQUENCE: MVP13-75] Hit registration implementation
HitValidation HitRegistration::ProcessHitRequest(const HitRequest& request) {
    // Convert client timestamp to server time
    float client_latency = LagCompensation::Instance().GetPlayerLatency(request.attacker_id);
    auto server_time = LagCompensationUtils::ClientToServerTime(
        request.timestamp, client_latency);
    
    // Validate with lag compensation
    return LagCompensation::Instance().ValidateHit(
        request.attacker_id,
        0,  // Target will be found by raycast
        request.shot_origin,
        request.shot_direction,
        100.0f,  // Max range from weapon
        server_time);
}

HitValidation HitRegistration::ValidateMeleeHit(
    uint64_t attacker_id,
    uint64_t victim_id,
    const Vector3& attack_position,
    float attack_range,
    float client_timestamp) {
    
    HitValidation result;
    
    // Get client latency
    float latency = LagCompensation::Instance().GetPlayerLatency(attacker_id);
    auto server_time = LagCompensationUtils::ClientToServerTime(
        client_timestamp, latency);
    
    // Create rewind context
    RewindContext rewind(server_time);
    
    // Get victim state
    auto victim_state = rewind.GetEntityState(victim_id);
    if (!victim_state) {
        result.rejection_reason = "Victim not found";
        return result;
    }
    
    // Check distance
    float distance = Vector3::Distance(attack_position, victim_state->position);
    if (!IsDistanceValid(attack_position, victim_state->position, attack_range)) {
        result.rejection_reason = "Out of melee range";
        return result;
    }
    
    // Check if victim was alive
    if (!victim_state->is_alive) {
        result.rejection_reason = "Victim was dead";
        return result;
    }
    
    // Success
    result.is_valid = true;
    result.victim_id = victim_id;
    result.impact_point = victim_state->position;
    result.confidence = 1.0f - (latency / 500.0f);  // Lower confidence with lag
    
    return result;
}

bool HitRegistration::IsDistanceValid(
    const Vector3& from,
    const Vector3& to,
    float max_distance,
    float tolerance) {
    
    float distance = Vector3::Distance(from, to);
    return distance <= max_distance * (1.0f + tolerance);
}

// [SEQUENCE: MVP13-76] Interpolation utilities
namespace InterpolationUtils {

WorldSnapshot::EntityState InterpolateEntityState(
    const WorldSnapshot::EntityState& state1,
    const WorldSnapshot::EntityState& state2,
    float t) {
    
    WorldSnapshot::EntityState result;
    
    // Interpolate position and velocity
    result.position = state1.position * (1.0f - t) + state2.position * t;
    result.velocity = state1.velocity * (1.0f - t) + state2.velocity * t;
    result.rotation = state1.rotation * (1.0f - t) + state2.rotation * t;
    
    // Interpolate hitbox
    result.hitbox = InterpolateHitbox(state1.hitbox, state2.hitbox, t);
    
    // Use latest values for discrete properties
    result.entity_id = state1.entity_id;
    result.health = state2.health;
    result.is_alive = state2.is_alive;
    
    return result;
}

BoundingBox InterpolateHitbox(
    const BoundingBox& box1,
    const BoundingBox& box2,
    float t) {
    
    BoundingBox result;
    result.min = box1.min * (1.0f - t) + box2.min * t;
    result.max = box1.max * (1.0f - t) + box2.max * t;
    return result;
}

float CalculateInterpolationFactor(
    std::chrono::steady_clock::time_point t1,
    std::chrono::steady_clock::time_point t2,
    std::chrono::steady_clock::time_point target) {
    
    auto total_duration = std::chrono::duration<float>(t2 - t1).count();
    auto elapsed = std::chrono::duration<float>(target - t1).count();
    
    if (total_duration <= 0.0f) {
        return 0.0f;
    }
    
    return std::clamp(elapsed / total_duration, 0.0f, 1.0f);
}

} // namespace InterpolationUtils

// [SEQUENCE: MVP13-77] Advanced lag compensation
AdvancedLagCompensation::AdvancedLagCompensation() {
    // Initialize with default settings
    settings_.max_rewind_time_ms = 1000.0f;
    settings_.hit_tolerance = 0.1f;
    settings_.movement_tolerance = 0.2f;
    settings_.max_extrapolation_ms = 200.0f;
    settings_.enable_client_side_hit = true;
    settings_.lag_threshold_ms = 150.0f;
    settings_.confidence_threshold = 0.7f;
}

AdvancedLagCompensation::PredictedHit AdvancedLagCompensation::PredictTargetPosition(
    uint64_t target_id,
    float shooter_latency,
    float target_latency) {
    
    PredictedHit prediction;
    
    // Check cache first
    auto cache_it = prediction_cache_.find(target_id);
    if (cache_it != prediction_cache_.end()) {
        auto age = std::chrono::steady_clock::now() - cache_it->second.cache_time;
        if (age < std::chrono::milliseconds(100)) {  // 100ms cache
            return cache_it->second.prediction;
        }
    }
    
    // Get current target state
    auto& world = world::WorldManager::Instance();
    auto target = world.GetEntity(target_id);
    if (!target) {
        return prediction;
    }
    
    // Calculate total latency
    float total_latency = (shooter_latency + target_latency) / 2.0f;
    float predict_time = total_latency / 1000.0f;  // Convert to seconds
    
    // Predict future position
    prediction.predicted_position = target->GetPosition() + 
                                  target->GetVelocity() * predict_time;
    prediction.predicted_velocity = target->GetVelocity();  // Assume constant
    prediction.time_offset = predict_time;
    
    // Calculate probability based on target movement
    float speed = target->GetVelocity().Length();
    if (speed < 1.0f) {
        prediction.probability = 0.95f;  // Stationary target
    } else if (speed < 5.0f) {
        prediction.probability = 0.8f;   // Walking
    } else if (speed < 10.0f) {
        prediction.probability = 0.6f;   // Running
    } else {
        prediction.probability = 0.4f;   // Sprinting
    }
    
    // Adjust for latency
    prediction.probability *= (1.0f - total_latency / 1000.0f);
    
    // Cache result
    prediction_cache_[target_id] = {
        target_id,
        prediction,
        std::chrono::steady_clock::now()
    };
    
    return prediction;
}

HitValidation AdvancedLagCompensation::ValidateWithPrediction(
    const HitRequest& request,
    const PredictedHit& prediction) {
    
    HitValidation result;
    
    // Basic validation first
    result = LagCompensation::Instance().ValidateHit(
        request.attacker_id,
        0,  // Will be determined
        request.shot_origin,
        request.shot_direction,
        100.0f,
        std::chrono::steady_clock::now());
    
    if (!result.is_valid) {
        return result;
    }
    
    // Apply prediction confidence
    result.confidence *= prediction.probability;
    
    // Check against confidence threshold
    if (result.confidence < settings_.confidence_threshold) {
        result.is_valid = false;
        result.rejection_reason = "Confidence too low";
        LagCompensation::Instance().stats_.rejected_hits++;
    }
    
    return result;
}

// [SEQUENCE: MVP13-78] Lag compensation utilities
namespace LagCompensationUtils {

std::chrono::steady_clock::time_point ClientToServerTime(
    float client_timestamp,
    float client_latency) {
    
    // Current server time
    auto now = std::chrono::steady_clock::now();
    
    // Subtract latency to get when event actually happened
    auto latency_ms = std::chrono::milliseconds(static_cast<int>(client_latency));
    return now - latency_ms;
}

bool IsTimestampValid(
    float client_timestamp,
    float server_time,
    float max_difference) {
    
    float diff = std::abs(client_timestamp - server_time);
    return diff <= max_difference;
}

float CalculateSpreadAtDistance(
    float base_spread,
    float distance) {
    
    // Convert spread angle to radius at distance
    float spread_radians = base_spread * (M_PI / 180.0f);
    return distance * std::tan(spread_radians);
}

} // namespace LagCompensationUtils

// [SEQUENCE: MVP13-79] Rollback networking implementation
void RollbackNetworking::AdvanceFrame() {
    current_frame_++;
    
    // Create state for this frame
    RollbackState state;
    state.frame = current_frame_;
    state.snapshot = CreateCurrentSnapshot();
    
    // Collect inputs for this frame
    for (auto& [player_id, buffer] : input_buffers_) {
        if (!buffer.empty() && buffer.front().tick == current_frame_) {
            state.inputs[player_id] = buffer.front();
            buffer.pop_front();
        } else {
            // Predict input if missing
            state.inputs[player_id] = PredictInput(player_id, current_frame_);
        }
    }
    
    // Store state
    state_history_.push_back(state);
    
    // Limit history size
    while (state_history_.size() > MAX_ROLLBACK_FRAMES) {
        state_history_.pop_front();
    }
    
    // Simulate frame
    SimulateFrame(current_frame_);
}

void RollbackNetworking::Rollback(uint32_t to_frame) {
    if (to_frame >= current_frame_) {
        return;  // Can't rollback to future
    }
    
    // Find state to rollback to
    for (const auto& state : state_history_) {
        if (state.frame == to_frame) {
            // Restore world state
            RestoreWorldState(state.snapshot);
            confirmed_frame_ = to_frame;
            break;
        }
    }
}

void RollbackNetworking::Resimulate(uint32_t from_frame, uint32_t to_frame) {
    for (uint32_t frame = from_frame + 1; frame <= to_frame; ++frame) {
        // Find inputs for this frame
        for (const auto& state : state_history_) {
            if (state.frame == frame) {
                ApplyInputs(state.inputs);
                SimulateFrame(frame);
                break;
            }
        }
    }
}

PlayerInput RollbackNetworking::PredictInput(uint64_t player_id, uint32_t frame) {
    // Simple prediction: use last known input
    auto it = input_buffers_.find(player_id);
    if (it != input_buffers_.end() && !it->second.empty()) {
        PlayerInput predicted = it->second.back();
        predicted.tick = frame;
        return predicted;
    }
    
    // Return empty input
    PlayerInput empty;
    empty.tick = frame;
    return empty;
}

void RollbackNetworking::SimulateFrame(uint32_t frame) {
    // Apply physics simulation
    physics::PhysicsWorld::Instance().Step(1.0f / 60.0f);
    
    // Process combat events
    combat::CombatSystem::Instance().Update(1.0f / 60.0f);
}

} // namespace mmorpg::network