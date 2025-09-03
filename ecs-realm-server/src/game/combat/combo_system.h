#pragma once

#include <vector>
#include <unordered_map>
#include <chrono>
#include <optional>
#include <queue>
#include <spdlog/spdlog.h>

namespace mmorpg::game::combat {

// [SEQUENCE: 1760] Combo system for chained abilities and attacks
// 콤보 시스템으로 연계 기술 및 공격 관리

// [SEQUENCE: 1761] Combo trigger type
enum class ComboTriggerType {
    ABILITY_USE,        // Specific ability used
    DAMAGE_DEALT,       // Damage threshold reached
    CRITICAL_HIT,       // Critical strike landed
    DODGE_SUCCESS,      // Successfully dodged
    BLOCK_SUCCESS,      // Successfully blocked
    PARRY_SUCCESS,      // Successfully parried
    BUFF_GAINED,        // Specific buff applied
    DEBUFF_APPLIED,     // Debuff on target
    TARGET_HEALTH,      // Target health threshold
    POSITION_BEHIND,    // Behind target
    POSITION_SIDE,      // Side of target
    COMBO_COUNTER       // Nth hit in combo
};

// [SEQUENCE: 1762] Combo node - one step in combo chain
struct ComboNode {
    uint32_t node_id;
    uint32_t ability_id;            // Ability for this node
    
    // Trigger conditions
    ComboTriggerType trigger_type;
    uint32_t trigger_value = 0;     // Context-specific value
    
    // Timing window
    std::chrono::milliseconds window_start{0};    // Min time after previous
    std::chrono::milliseconds window_end{3000};   // Max time after previous
    
    // Requirements
    bool requires_target = true;
    bool requires_same_target = true;
    float max_range = 5.0f;
    
    // Rewards
    float damage_multiplier = 1.0f;
    float resource_refund = 0.0f;   // Mana/energy refund %
    uint32_t bonus_effect_id = 0;   // Additional effect
    
    // Next possible nodes
    std::vector<uint32_t> next_nodes;
    
    // [SEQUENCE: 1763] Check if window is valid
    bool IsInWindow(std::chrono::milliseconds elapsed) const {
        return elapsed >= window_start && elapsed <= window_end;
    }
};

// [SEQUENCE: 1764] Combo chain definition
struct ComboChain {
    uint32_t chain_id;
    std::string chain_name;
    uint32_t class_id;              // Class restriction
    
    // Nodes in this chain
    std::unordered_map<uint32_t, ComboNode> nodes;
    uint32_t start_node_id;         // Entry point
    std::vector<uint32_t> finisher_nodes;  // End points
    
    // Chain properties
    uint32_t max_length = 10;       // Maximum combo length
    bool allow_repetition = false;  // Can repeat nodes
    bool reset_on_miss = true;      // Reset if ability misses
    
    // Rewards for completing chain
    uint32_t completion_buff_id = 0;
    float completion_damage_bonus = 0.0f;
    uint32_t achievement_id = 0;
};

// [SEQUENCE: 1765] Active combo state
struct ActiveCombo {
    uint32_t chain_id;
    uint32_t current_node_id;
    uint32_t combo_count = 0;
    
    // Target tracking
    uint64_t initial_target_id = 0;
    uint64_t current_target_id = 0;
    
    // Timing
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point last_action_time;
    
    // Progress tracking
    std::vector<uint32_t> executed_nodes;
    float total_damage_multiplier = 1.0f;
    
    // [SEQUENCE: 1766] Get elapsed time since last action
    std::chrono::milliseconds GetTimeSinceLastAction() const {
        auto now = std::chrono::system_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_action_time
        );
    }
    
    // [SEQUENCE: 1767] Check if combo expired
    bool IsExpired(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) const {
        return GetTimeSinceLastAction() > timeout;
    }
};

// [SEQUENCE: 1768] Combo progress tracker for a player
class ComboTracker {
public:
    ComboTracker(uint64_t player_id) : player_id_(player_id) {}
    
    // [SEQUENCE: 1769] Start a new combo
    bool StartCombo(uint32_t chain_id, uint32_t ability_id, uint64_t target_id) {
        // Check if chain exists
        auto* chain = GetComboChain(chain_id);
        if (!chain) {
            return false;
        }
        
        // Find starting node
        if (chain->start_node_id != 0) {
            // Specific start required
            auto node_it = chain->nodes.find(chain->start_node_id);
            if (node_it == chain->nodes.end() || 
                node_it->second.ability_id != ability_id) {
                return false;
            }
        } else {
            // Any node can start
            bool found = false;
            for (const auto& [node_id, node] : chain->nodes) {
                if (node.ability_id == ability_id) {
                    found = true;
                    active_combo_ = ActiveCombo{};
                    active_combo_.chain_id = chain_id;
                    active_combo_.current_node_id = node_id;
                    break;
                }
            }
            if (!found) return false;
        }
        
        // Initialize combo
        active_combo_.initial_target_id = target_id;
        active_combo_.current_target_id = target_id;
        active_combo_.start_time = std::chrono::system_clock::now();
        active_combo_.last_action_time = active_combo_.start_time;
        active_combo_.combo_count = 1;
        active_combo_.executed_nodes.push_back(active_combo_.current_node_id);
        
        spdlog::info("Player {} started combo {} with ability {}", 
                    player_id_, chain_id, ability_id);
        
        return true;
    }
    
    // [SEQUENCE: 1770] Continue combo with next ability
    std::optional<ComboNode> ContinueCombo(uint32_t ability_id, uint64_t target_id,
                                           ComboTriggerType trigger = ComboTriggerType::ABILITY_USE) {
        if (!HasActiveCombo()) {
            return std::nullopt;
        }
        
        // Check if combo expired
        if (active_combo_.IsExpired()) {
            ResetCombo();
            return std::nullopt;
        }
        
        auto* chain = GetComboChain(active_combo_.chain_id);
        if (!chain) {
            ResetCombo();
            return std::nullopt;
        }
        
        // Get current node
        auto current_it = chain->nodes.find(active_combo_.current_node_id);
        if (current_it == chain->nodes.end()) {
            ResetCombo();
            return std::nullopt;
        }
        
        // Check timing window
        auto elapsed = active_combo_.GetTimeSinceLastAction();
        
        // Find valid next node
        for (uint32_t next_id : current_it->second.next_nodes) {
            auto next_it = chain->nodes.find(next_id);
            if (next_it == chain->nodes.end()) continue;
            
            const auto& next_node = next_it->second;
            
            // Check ability match
            if (next_node.ability_id != ability_id) continue;
            
            // Check trigger type
            if (next_node.trigger_type != trigger) continue;
            
            // Check timing window
            if (!next_node.IsInWindow(elapsed)) continue;
            
            // Check target requirements
            if (next_node.requires_same_target && 
                target_id != active_combo_.current_target_id) {
                continue;
            }
            
            // Valid continuation found
            active_combo_.current_node_id = next_id;
            active_combo_.current_target_id = target_id;
            active_combo_.last_action_time = std::chrono::system_clock::now();
            active_combo_.combo_count++;
            active_combo_.executed_nodes.push_back(next_id);
            active_combo_.total_damage_multiplier *= next_node.damage_multiplier;
            
            // Check if finisher
            if (std::find(chain->finisher_nodes.begin(), 
                         chain->finisher_nodes.end(), 
                         next_id) != chain->finisher_nodes.end()) {
                OnComboComplete();
            }
            
            spdlog::debug("Player {} continued combo to node {} (count: {})", 
                         player_id_, next_id, active_combo_.combo_count);
            
            return next_node;
        }
        
        // No valid continuation - check if reset required
        if (chain->reset_on_miss) {
            ResetCombo();
        }
        
        return std::nullopt;
    }
    
    // [SEQUENCE: 1771] Get current combo state
    const ActiveCombo* GetActiveCombo() const {
        return HasActiveCombo() ? &active_combo_ : nullptr;
    }
    
    // [SEQUENCE: 1772] Reset combo
    void ResetCombo() {
        if (HasActiveCombo()) {
            spdlog::info("Player {} combo reset (count was {})", 
                        player_id_, active_combo_.combo_count);
        }
        active_combo_ = ActiveCombo{};
    }
    
    // [SEQUENCE: 1773] Get possible next abilities
    std::vector<uint32_t> GetPossibleNextAbilities() const {
        std::vector<uint32_t> abilities;
        
        if (!HasActiveCombo()) {
            return abilities;
        }
        
        auto* chain = GetComboChain(active_combo_.chain_id);
        if (!chain) {
            return abilities;
        }
        
        auto current_it = chain->nodes.find(active_combo_.current_node_id);
        if (current_it == chain->nodes.end()) {
            return abilities;
        }
        
        // Get all possible next abilities
        for (uint32_t next_id : current_it->second.next_nodes) {
            auto next_it = chain->nodes.find(next_id);
            if (next_it != chain->nodes.end()) {
                abilities.push_back(next_it->second.ability_id);
            }
        }
        
        return abilities;
    }
    
    // [SEQUENCE: 1774] Update combo (call periodically)
    void Update() {
        if (HasActiveCombo() && active_combo_.IsExpired()) {
            ResetCombo();
        }
    }
    
    bool HasActiveCombo() const {
        return active_combo_.chain_id != 0;
    }
    
    uint32_t GetComboCount() const {
        return HasActiveCombo() ? active_combo_.combo_count : 0;
    }
    
    float GetDamageMultiplier() const {
        return HasActiveCombo() ? active_combo_.total_damage_multiplier : 1.0f;
    }
    
private:
    uint64_t player_id_;
    ActiveCombo active_combo_;
    
    // [SEQUENCE: 1775] Get combo chain definition
    ComboChain* GetComboChain(uint32_t chain_id) const;  // Implemented by ComboManager
    
    // [SEQUENCE: 1776] Handle combo completion
    void OnComboComplete() {
        auto* chain = GetComboChain(active_combo_.chain_id);
        if (!chain) return;
        
        spdlog::info("Player {} completed combo {} (length: {})", 
                    player_id_, chain->chain_name, active_combo_.combo_count);
        
        // TODO: Apply completion rewards
        // - Buff application
        // - Achievement progress
        // - Bonus effects
    }
};

// [SEQUENCE: 1777] Combo manager
class ComboManager {
public:
    static ComboManager& Instance() {
        static ComboManager instance;
        return instance;
    }
    
    // [SEQUENCE: 1778] Initialize combo chains
    void Initialize() {
        LoadComboChains();
        spdlog::info("Combo system initialized with {} chains", combo_chains_.size());
    }
    
    // [SEQUENCE: 1779] Get or create tracker
    std::shared_ptr<ComboTracker> GetTracker(uint64_t player_id) {
        auto it = player_trackers_.find(player_id);
        if (it == player_trackers_.end()) {
            auto tracker = std::make_shared<ComboTracker>(player_id);
            player_trackers_[player_id] = tracker;
            return tracker;
        }
        return it->second;
    }
    
    // [SEQUENCE: 1780] Process ability use
    std::optional<ComboNode> ProcessAbility(uint64_t player_id, uint32_t ability_id, 
                                           uint64_t target_id) {
        auto tracker = GetTracker(player_id);
        
        // Check if continuing existing combo
        if (tracker->HasActiveCombo()) {
            auto result = tracker->ContinueCombo(ability_id, target_id);
            if (result.has_value()) {
                return result;
            }
        }
        
        // Try to start new combo
        for (const auto& [chain_id, chain] : combo_chains_) {
            // Check class restriction
            if (chain.class_id != 0 && !IsClassMatch(player_id, chain.class_id)) {
                continue;
            }
            
            if (tracker->StartCombo(chain_id, ability_id, target_id)) {
                // Return the starting node
                auto node_it = chain.nodes.find(tracker->GetActiveCombo()->current_node_id);
                if (node_it != chain.nodes.end()) {
                    return node_it->second;
                }
            }
        }
        
        return std::nullopt;
    }
    
    // [SEQUENCE: 1781] Update all trackers
    void UpdateAll() {
        for (auto& [player_id, tracker] : player_trackers_) {
            tracker->Update();
        }
        
        // Clean up inactive trackers
        std::erase_if(player_trackers_, [](const auto& pair) {
            return !pair.second->HasActiveCombo();
        });
    }
    
    // [SEQUENCE: 1782] Get combo chain
    ComboChain* GetComboChain(uint32_t chain_id) {
        auto it = combo_chains_.find(chain_id);
        return (it != combo_chains_.end()) ? &it->second : nullptr;
    }
    
private:
    ComboManager() = default;
    
    std::unordered_map<uint32_t, ComboChain> combo_chains_;
    std::unordered_map<uint64_t, std::shared_ptr<ComboTracker>> player_trackers_;
    
    // [SEQUENCE: 1783] Load combo chains from data
    void LoadComboChains() {
        // Example: Warrior combo chain
        ComboChain warrior_combo;
        warrior_combo.chain_id = 1;
        warrior_combo.chain_name = "Blade Dance";
        warrior_combo.class_id = 1;  // Warrior
        warrior_combo.max_length = 5;
        
        // Node 1: Opening Strike
        ComboNode node1;
        node1.node_id = 1;
        node1.ability_id = 100;  // Strike
        node1.trigger_type = ComboTriggerType::ABILITY_USE;
        node1.damage_multiplier = 1.0f;
        node1.next_nodes = {2, 3};
        warrior_combo.nodes[1] = node1;
        warrior_combo.start_node_id = 1;
        
        // Node 2: Follow-up Slash
        ComboNode node2;
        node2.node_id = 2;
        node2.ability_id = 101;  // Slash
        node2.trigger_type = ComboTriggerType::ABILITY_USE;
        node2.window_start = std::chrono::milliseconds(500);
        node2.window_end = std::chrono::milliseconds(2000);
        node2.damage_multiplier = 1.2f;
        node2.next_nodes = {4};
        warrior_combo.nodes[2] = node2;
        
        // Node 3: Alternative - Shield Bash
        ComboNode node3;
        node3.node_id = 3;
        node3.ability_id = 102;  // Shield Bash
        node3.trigger_type = ComboTriggerType::ABILITY_USE;
        node3.window_start = std::chrono::milliseconds(500);
        node3.window_end = std::chrono::milliseconds(2000);
        node3.damage_multiplier = 0.8f;
        node3.bonus_effect_id = 1;  // Stun
        node3.next_nodes = {4};
        warrior_combo.nodes[3] = node3;
        
        // Node 4: Finisher - Whirlwind
        ComboNode node4;
        node4.node_id = 4;
        node4.ability_id = 103;  // Whirlwind
        node4.trigger_type = ComboTriggerType::ABILITY_USE;
        node4.window_start = std::chrono::milliseconds(800);
        node4.window_end = std::chrono::milliseconds(3000);
        node4.damage_multiplier = 1.5f;
        node4.resource_refund = 50.0f;  // 50% resource refund
        warrior_combo.nodes[4] = node4;
        warrior_combo.finisher_nodes = {4};
        
        combo_chains_[1] = warrior_combo;
        
        // TODO: Load from database/config
    }
    
    // [SEQUENCE: 1784] Check class match
    bool IsClassMatch(uint64_t player_id, uint32_t class_id) {
        // TODO: Get player class
        return true;
    }
};

// [SEQUENCE: 1785] Helper to get tracker from ComboChain
ComboChain* ComboTracker::GetComboChain(uint32_t chain_id) const {
    return ComboManager::Instance().GetComboChain(chain_id);
}

} // namespace mmorpg::game::combat