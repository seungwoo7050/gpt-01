#include "combo_system.h"
#include "../skills/skill_system.h"
#include "../status/status_effect_system.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace mmorpg::game::combat {

// [SEQUENCE: 1215] ComboController - Process input
bool ComboController::ProcessInput(ComboInput input) {
    auto now = std::chrono::steady_clock::now();
    
    // Initialize combo if idle
    if (state_ == ComboState::IDLE) {
        state_ = ComboState::IN_PROGRESS;
        progress_.combo_start_time = now;
        progress_.current_node = combo_tree_root_;
        
        spdlog::debug("Entity {} started combo with input {}", 
                     entity_id_, static_cast<int>(input));
    }
    
    // Check if we have a valid node
    if (!progress_.current_node) {
        CancelCombo();
        return false;
    }
    
    // Check time window
    if (!progress_.input_history.empty()) {
        auto time_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - progress_.last_input_time).count() / 1000.0f;
        
        if (time_since_last > progress_.current_node->time_window) {
            spdlog::debug("Entity {} combo timed out ({}s > {}s window)", 
                         entity_id_, time_since_last, progress_.current_node->time_window);
            CancelCombo();
            return false;
        }
    }
    
    // Find next node
    auto next_it = progress_.current_node->next_nodes.find(input);
    if (next_it == progress_.current_node->next_nodes.end()) {
        spdlog::debug("Entity {} invalid combo input {}", 
                     entity_id_, static_cast<int>(input));
        CancelCombo();
        return false;
    }
    
    // Progress to next node
    progress_.current_node = next_it->second;
    progress_.input_history.push_back(input);
    progress_.last_input_time = now;
    
    // Check if combo completed
    if (progress_.current_node->combo_id != 0) {
        progress_.current_combo_id = progress_.current_node->combo_id;
        CheckComboCompletion();
    }
    
    spdlog::debug("Entity {} combo progressed with input {} (history size: {})", 
                 entity_id_, static_cast<int>(input), progress_.input_history.size());
    
    return true;
}

// [SEQUENCE: 1216] ComboController - Cancel combo
void ComboController::CancelCombo() {
    if (state_ != ComboState::IDLE) {
        spdlog::debug("Entity {} cancelled combo", entity_id_);
        state_ = ComboState::IDLE;
        progress_.Reset();
    }
}

// [SEQUENCE: 1217] ComboController - Interrupt combo
void ComboController::InterruptCombo() {
    if (state_ == ComboState::IN_PROGRESS) {
        spdlog::debug("Entity {} combo interrupted", entity_id_);
        state_ = ComboState::FAILED;
        progress_.Reset();
    }
}

// [SEQUENCE: 1218] ComboController - Register hit
void ComboController::RegisterHit(uint64_t target_id, float damage) {
    if (state_ == ComboState::IN_PROGRESS) {
        progress_.hit_count++;
        progress_.accumulated_damage += damage;
        
        spdlog::debug("Entity {} combo hit #{} on target {} for {} damage", 
                     entity_id_, progress_.hit_count, target_id, damage);
    }
}

// [SEQUENCE: 1219] ComboController - Try finish combo
bool ComboController::TryFinishCombo() {
    if (state_ != ComboState::IN_PROGRESS || progress_.current_combo_id == 0) {
        return false;
    }
    
    // Check if current node is a finisher
    if (progress_.current_node && progress_.current_node->is_finisher) {
        state_ = ComboState::FINISHING;
        ApplyComboEffects(progress_.current_combo_id);
        state_ = ComboState::COMPLETED;
        
        spdlog::info("Entity {} completed combo {} with {} hits for {} total damage", 
                    entity_id_, progress_.current_combo_id, 
                    progress_.hit_count, progress_.accumulated_damage);
        
        // Record statistics
        ComboEvent event{
            entity_id_,
            progress_.current_combo_id,
            progress_.hit_count,
            progress_.accumulated_damage,
            std::chrono::steady_clock::now()
        };
        // TODO: Send to statistics manager
        
        progress_.Reset();
        state_ = ComboState::IDLE;
        return true;
    }
    
    return false;
}

// [SEQUENCE: 1220] ComboController - Set available combos
void ComboController::SetAvailableCombos(const std::vector<uint32_t>& combo_ids) {
    available_combos_.clear();
    available_combos_.insert(combo_ids.begin(), combo_ids.end());
    
    spdlog::debug("Entity {} has {} available combos", entity_id_, available_combos_.size());
}

// [SEQUENCE: 1221] ComboController - Is combo available
bool ComboController::IsComboAvailable(uint32_t combo_id) const {
    return available_combos_.find(combo_id) != available_combos_.end();
}

// [SEQUENCE: 1222] ComboController - Update
void ComboController::Update(float delta_time) {
    if (state_ != ComboState::IN_PROGRESS) {
        return;
    }
    
    time_since_last_input_ += delta_time;
    
    // Check total combo time limit
    auto now = std::chrono::steady_clock::now();
    auto combo_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - progress_.combo_start_time).count() / 1000.0f;
    
    const auto* combo_def = ComboManager::Instance().GetCombo(progress_.current_combo_id);
    if (combo_def && combo_duration > combo_def->total_time_limit) {
        spdlog::debug("Entity {} combo exceeded total time limit", entity_id_);
        CancelCombo();
        return;
    }
    
    // Check input window timeout
    if (!progress_.input_history.empty() && progress_.current_node) {
        auto time_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - progress_.last_input_time).count() / 1000.0f;
        
        if (time_since_last > progress_.current_node->time_window) {
            CancelCombo();
        }
    }
}

// [SEQUENCE: 1223] ComboController - Get time until timeout
float ComboController::GetTimeUntilTimeout() const {
    if (state_ != ComboState::IN_PROGRESS || !progress_.current_node) {
        return 0.0f;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto time_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - progress_.last_input_time).count() / 1000.0f;
    
    return std::max(0.0f, progress_.current_node->time_window - time_since_last);
}

// [SEQUENCE: 1224] ComboController - Check combo completion
void ComboController::CheckComboCompletion() {
    if (progress_.current_node && progress_.current_node->combo_id != 0) {
        // Check if this is a complete combo
        const auto* combo_def = ComboManager::Instance().GetCombo(progress_.current_node->combo_id);
        if (combo_def && progress_.input_history.size() >= combo_def->input_sequence.size()) {
            // Verify input sequence matches
            bool matches = true;
            for (size_t i = 0; i < combo_def->input_sequence.size(); ++i) {
                if (progress_.input_history[i] != combo_def->input_sequence[i]) {
                    matches = false;
                    break;
                }
            }
            
            if (matches) {
                progress_.current_combo_id = progress_.current_node->combo_id;
                if (progress_.current_node->is_finisher) {
                    TryFinishCombo();
                }
            }
        }
    }
}

// [SEQUENCE: 1225] ComboController - Apply combo effects
void ComboController::ApplyComboEffects(uint32_t combo_id) {
    const auto* combo_def = ComboManager::Instance().GetCombo(combo_id);
    if (!combo_def) {
        return;
    }
    
    // Apply damage multiplier (handled by combat system)
    // Apply resource cost reduction (handled by skill system)
    
    // Apply bonus effect
    if (combo_def->bonus_effect_id != 0) {
        status::StatusEffectManager::Instance().ApplyEffect(
            entity_id_, combo_def->bonus_effect_id, entity_id_
        );
    }
    
    spdlog::info("Applied combo {} effects to entity {}", combo_id, entity_id_);
}

// [SEQUENCE: 1226] ComboManager - Register combo
void ComboManager::RegisterCombo(const ComboDefinition& combo) {
    combo_definitions_[combo.combo_id] = combo;
    AddComboToTree(combo);
    
    spdlog::info("Registered combo: {} (ID: {})", combo.combo_name, combo.combo_id);
}

// [SEQUENCE: 1227] ComboManager - Get combo
const ComboDefinition* ComboManager::GetCombo(uint32_t combo_id) const {
    auto it = combo_definitions_.find(combo_id);
    return (it != combo_definitions_.end()) ? &it->second : nullptr;
}

// [SEQUENCE: 1228] ComboManager - Build combo tree
void ComboManager::BuildComboTree() {
    combo_tree_root_ = std::make_shared<ComboNode>();
    combo_tree_root_->input = ComboInput::LIGHT_ATTACK;  // Placeholder
    
    // Add all registered combos to tree
    for (const auto& [id, combo] : combo_definitions_) {
        AddComboToTree(combo);
    }
    
    spdlog::info("Built combo tree with {} combos", combo_definitions_.size());
}

// [SEQUENCE: 1229] ComboManager - Add combo to tree
void ComboManager::AddComboToTree(const ComboDefinition& combo) {
    if (!combo_tree_root_) {
        combo_tree_root_ = std::make_shared<ComboNode>();
    }
    
    auto current_node = combo_tree_root_;
    
    // Build path through tree for this combo
    for (size_t i = 0; i < combo.input_sequence.size(); ++i) {
        ComboInput input = combo.input_sequence[i];
        bool is_last = (i == combo.input_sequence.size() - 1);
        
        // Find or create next node
        auto& next_node = current_node->next_nodes[input];
        if (!next_node) {
            next_node = std::make_shared<ComboNode>();
            next_node->input = input;
            next_node->time_window = 0.5f;  // Default window
        }
        
        // Set combo ID on final node
        if (is_last) {
            next_node->combo_id = combo.combo_id;
            next_node->is_finisher = true;
            next_node->damage_multiplier = combo.damage_multiplier;
        }
        
        current_node = next_node;
    }
}

// [SEQUENCE: 1230] ComboManager - Create controller
std::shared_ptr<ComboController> ComboManager::CreateController(uint64_t entity_id) {
    auto controller = std::make_shared<ComboController>(entity_id);
    controller->combo_tree_root_ = combo_tree_root_;
    controllers_[entity_id] = controller;
    
    spdlog::debug("Created combo controller for entity {}", entity_id);
    return controller;
}

// [SEQUENCE: 1231] ComboManager - Get controller
std::shared_ptr<ComboController> ComboManager::GetController(uint64_t entity_id) const {
    auto it = controllers_.find(entity_id);
    return (it != controllers_.end()) ? it->second : nullptr;
}

// [SEQUENCE: 1232] ComboManager - Remove controller
void ComboManager::RemoveController(uint64_t entity_id) {
    controllers_.erase(entity_id);
    spdlog::debug("Removed combo controller for entity {}", entity_id);
}

// [SEQUENCE: 1233] ComboManager - Update
void ComboManager::Update(float delta_time) {
    for (auto& [entity_id, controller] : controllers_) {
        controller->Update(delta_time);
    }
}

// [SEQUENCE: 1234] ComboManager - Get combos for class
std::vector<uint32_t> ComboManager::GetCombosForClass(uint32_t class_id) const {
    std::vector<uint32_t> result;
    
    for (const auto& [combo_id, combo_def] : combo_definitions_) {
        if (combo_def.required_class == 0 || combo_def.required_class == class_id) {
            result.push_back(combo_id);
        }
    }
    
    return result;
}

// [SEQUENCE: 1235] ComboManager - Get combos for level
std::vector<uint32_t> ComboManager::GetCombosForLevel(uint32_t level) const {
    std::vector<uint32_t> result;
    
    for (const auto& [combo_id, combo_def] : combo_definitions_) {
        if (level >= combo_def.min_level) {
            result.push_back(combo_id);
        }
    }
    
    return result;
}

// [SEQUENCE: 1236] ComboStatistics - Record combo execution
void ComboStatistics::RecordComboExecution(const ComboEvent& event) {
    // Global stats
    combo_executions_[event.combo_id]++;
    combo_attempts_[event.combo_id]++;
    
    // Player stats
    auto& player_stats = player_stats_[event.entity_id];
    player_stats.total_combos++;
    player_stats.combo_usage[event.combo_id]++;
    player_stats.longest_combo = std::max(player_stats.longest_combo, 
                                         static_cast<uint32_t>(event.hit_count));
}

// [SEQUENCE: 1237] ComboStatistics - Record combo failure
void ComboStatistics::RecordComboFailure(uint64_t entity_id, uint32_t partial_combo_id) {
    combo_attempts_[partial_combo_id]++;
    player_stats_[entity_id].failed_combos++;
}

// [SEQUENCE: 1238] ComboStatistics - Get combo execution count
uint32_t ComboStatistics::GetComboExecutionCount(uint32_t combo_id) const {
    auto it = combo_executions_.find(combo_id);
    return (it != combo_executions_.end()) ? it->second : 0;
}

// [SEQUENCE: 1239] ComboStatistics - Get combo success rate
float ComboStatistics::GetComboSuccessRate(uint32_t combo_id) const {
    auto exec_it = combo_executions_.find(combo_id);
    auto attempt_it = combo_attempts_.find(combo_id);
    
    if (attempt_it == combo_attempts_.end() || attempt_it->second == 0) {
        return 0.0f;
    }
    
    uint32_t executions = (exec_it != combo_executions_.end()) ? exec_it->second : 0;
    return static_cast<float>(executions) / attempt_it->second;
}

// [SEQUENCE: 1240] ComboStatistics - Get player combo count
uint32_t ComboStatistics::GetPlayerComboCount(uint64_t entity_id) const {
    auto it = player_stats_.find(entity_id);
    return (it != player_stats_.end()) ? it->second.total_combos : 0;
}

} // namespace mmorpg::game::combat