// [SEQUENCE: MVP4-41] Threat/Aggro system for enemy AI targeting
#pragma once

#include <unordered_map>
#include <vector>
#include <algorithm>
#include <chrono>
#include <functional>
#include <spdlog/spdlog.h>

namespace mmorpg::game::combat {

enum class ThreatModifierType {
    DAMAGE_DEALT,       // Direct damage
    HEALING_DONE,       // Healing generates threat
    BUFF_APPLIED,       // Buffing allies
    DEBUFF_APPLIED,     // Debuffing enemies
    TAUNT,              // Force target
    DETAUNT,            // Reduce threat
    FADE,               // Temporary threat reduction
    SPECIAL_ABILITY     // Ability-specific threat
};

struct ThreatEvent {
    uint64_t source_id;         // Who generated threat
    ThreatModifierType type;
    float base_value;           // Base threat amount
    float multiplier = 1.0f;    // Class/ability multiplier
    bool is_player = true;      // Player or NPC
    
    std::chrono::system_clock::time_point timestamp;
};

struct ThreatEntry {
    uint64_t entity_id;         // Player/NPC ID
    float threat_value = 0.0f;  // Current threat
    float threat_percent = 0.0f; // Percentage of total
    
    // Modifiers
    float threat_multiplier = 1.0f;     // Permanent multiplier
    float temporary_multiplier = 1.0f;  // Temporary effects
    
    // Special states
    bool is_taunted = false;
    std::chrono::system_clock::time_point taunt_end_time;
    
    bool is_fading = false;
    float fade_amount = 0.0f;
    std::chrono::system_clock::time_point fade_end_time;
    
    // Last update
    std::chrono::system_clock::time_point last_update;
    
    float GetEffectiveThreat() const {
        if (is_taunted) {
            return std::numeric_limits<float>::max();  // Taunted = max threat
        }
        
        float effective = threat_value * threat_multiplier * temporary_multiplier;
        
        if (is_fading) {
            effective -= fade_amount;
        }
        
        return std::max(0.0f, effective);
    }
};

class ThreatTable {
public:
    ThreatTable(uint64_t owner_id) : owner_id_(owner_id) {}
    
    void AddThreat(uint64_t entity_id, float amount, ThreatModifierType type) {
        if (amount < 0) {
            spdlog::warn("Negative threat amount: {}", amount);
            return;
        }
        
        auto& entry = GetOrCreateEntry(entity_id);
        
        // Apply modifiers based on type
        float modified_amount = amount;
        switch (type) {
            case ThreatModifierType::HEALING_DONE:
                modified_amount *= 0.5f;  // Healing generates half threat
                break;
            case ThreatModifierType::BUFF_APPLIED:
                modified_amount *= 0.3f;  // Buffs generate less threat
                break;
            case ThreatModifierType::TAUNT:
                ApplyTaunt(entity_id);
                return;  // Taunt doesn't add numeric threat
            case ThreatModifierType::DETAUNT:
                modified_amount *= -0.5f;  // Reduce threat
                break;
            default:
                break;
        }
        
        // Apply entity modifiers
        modified_amount *= entry.threat_multiplier * entry.temporary_multiplier;
        
        // Add threat
        entry.threat_value += modified_amount;
        entry.last_update = std::chrono::system_clock::now();
        
        // Log significant threat changes
        if (modified_amount > 100) {
            spdlog::debug("Entity {} added {} threat to {}", 
                         entity_id, modified_amount, owner_id_);
        }
        
        UpdateThreatPercentages();
    }
    
    void ReduceThreat(uint64_t entity_id, float amount) {
        auto it = threat_entries_.find(entity_id);
        if (it == threat_entries_.end()) {
            return;
        }
        
        it->second.threat_value = std::max(0.0f, it->second.threat_value - amount);
        it->second.last_update = std::chrono::system_clock::now();
        
        UpdateThreatPercentages();
    }
    
    void MultiplyThreat(uint64_t entity_id, float multiplier) {
        auto it = threat_entries_.find(entity_id);
        if (it == threat_entries_.end()) {
            return;
        }
        
        it->second.threat_value *= multiplier;
        it->second.last_update = std::chrono::system_clock::now();
        
        UpdateThreatPercentages();
    }
    
    void SetThreat(uint64_t entity_id, float value) {
        auto& entry = GetOrCreateEntry(entity_id);
        entry.threat_value = std::max(0.0f, value);
        entry.last_update = std::chrono::system_clock::now();
        
        UpdateThreatPercentages();
    }
    
    void ApplyTaunt(uint64_t entity_id, std::chrono::seconds duration = std::chrono::seconds(3)) {
        auto& entry = GetOrCreateEntry(entity_id);
        entry.is_taunted = true;
        entry.taunt_end_time = std::chrono::system_clock::now() + duration;
        
        // Give slight threat lead to maintain aggro after taunt
        if (!threat_entries_.empty()) {
            float max_threat = GetHighestThreat();
            entry.threat_value = max_threat * 1.1f;
        }
        
        spdlog::info("Entity {} taunted NPC {} for {}s", 
                    entity_id, owner_id_, duration.count());
    }
    
    void ApplyFade(uint64_t entity_id, float amount, std::chrono::seconds duration) {
        auto it = threat_entries_.find(entity_id);
        if (it == threat_entries_.end()) {
            return;
        }
        
        it->second.is_fading = true;
        it->second.fade_amount = amount;
        it->second.fade_end_time = std::chrono::system_clock::now() + duration;
    }
    
    void SetThreatModifier(uint64_t entity_id, float modifier) {
        auto& entry = GetOrCreateEntry(entity_id);
        entry.threat_multiplier = modifier;
    }
    
    void SetTemporaryModifier(uint64_t entity_id, float modifier) {
        auto& entry = GetOrCreateEntry(entity_id);
        entry.temporary_multiplier = modifier;
    }
    
    uint64_t GetCurrentTarget() {
        UpdateExpiredEffects();
        
        uint64_t target = 0;
        float highest_threat = -1.0f;
        
        for (const auto& [entity_id, entry] : threat_entries_) {
            float effective_threat = entry.GetEffectiveThreat();
            
            if (effective_threat > highest_threat) {
                highest_threat = effective_threat;
                target = entity_id;
            }
        }
        
        return target;
    }
    
    std::vector<std::pair<uint64_t, float>> GetThreatList() {
        UpdateExpiredEffects();
        UpdateThreatPercentages();
        
        std::vector<std::pair<uint64_t, float>> list;
        
        for (const auto& [entity_id, entry] : threat_entries_) {
            if (entry.GetEffectiveThreat() > 0) {
                list.emplace_back(entity_id, entry.threat_percent);
            }
        }
        
        // Sort by threat percentage (highest first)
        std::sort(list.begin(), list.end(),
            [](const auto& a, const auto& b) {
                return a.second > b.second;
            });
        
        return list;
    }
    
    void RemoveEntity(uint64_t entity_id) {
        threat_entries_.erase(entity_id);
        UpdateThreatPercentages();
    }
    
    void ClearThreat() {
        threat_entries_.clear();
        spdlog::info("Threat table cleared for NPC {}", owner_id_);
    }
    
    void DropOutOfCombatThreat() {
        // Remove entries with no recent activity
        auto cutoff_time = std::chrono::system_clock::now() - std::chrono::seconds(10);
        
        std::erase_if(threat_entries_, [cutoff_time](const auto& pair) {
            return pair.second.last_update < cutoff_time;
        });
        
        UpdateThreatPercentages();
    }
    
    // Queries
    float GetThreat(uint64_t entity_id) const {
        auto it = threat_entries_.find(entity_id);
        return (it != threat_entries_.end()) ? it->second.GetEffectiveThreat() : 0.0f;
    }
    
    bool HasThreat(uint64_t entity_id) const {
        return GetThreat(entity_id) > 0.0f;
    }
    
    bool IsEmpty() const {
        return threat_entries_.empty();
    }
    
    size_t GetSize() const {
        return threat_entries_.size();
    }
    
private:
    uint64_t owner_id_;  // NPC that owns this threat table
    std::unordered_map<uint64_t, ThreatEntry> threat_entries_;
    
    ThreatEntry& GetOrCreateEntry(uint64_t entity_id) {
        auto it = threat_entries_.find(entity_id);
        if (it == threat_entries_.end()) {
            ThreatEntry entry;
            entry.entity_id = entity_id;
            entry.last_update = std::chrono::system_clock::now();
            threat_entries_[entity_id] = entry;
            return threat_entries_[entity_id];
        }
        return it->second;
    }
    
    void UpdateExpiredEffects() {
        auto now = std::chrono::system_clock::now();
        
        for (auto& [entity_id, entry] : threat_entries_) {
            // Check taunt expiration
            if (entry.is_taunted && now > entry.taunt_end_time) {
                entry.is_taunted = false;
            }
            
            // Check fade expiration
            if (entry.is_fading && now > entry.fade_end_time) {
                entry.is_fading = false;
                entry.fade_amount = 0.0f;
            }
        }
    }
    
    void UpdateThreatPercentages() {
        float total_threat = 0.0f;
        
        // Calculate total threat
        for (const auto& [entity_id, entry] : threat_entries_) {
            total_threat += entry.GetEffectiveThreat();
        }
        
        // Calculate percentages
        if (total_threat > 0) {
            for (auto& [entity_id, entry] : threat_entries_) {
                entry.threat_percent = (entry.GetEffectiveThreat() / total_threat) * 100.0f;
            }
        } else {
            for (auto& [entity_id, entry] : threat_entries_) {
                entry.threat_percent = 0.0f;
            }
        }
    }
    
    float GetHighestThreat() const {
        float highest = 0.0f;
        for (const auto& [entity_id, entry] : threat_entries_) {
            highest = std::max(highest, entry.GetEffectiveThreat());
        }
        return highest;
    }
};

class ThreatManager {
public:
    static ThreatManager& Instance() {
        static ThreatManager instance;
        return instance;
    }
    
    std::shared_ptr<ThreatTable> GetThreatTable(uint64_t npc_id) {
        auto it = threat_tables_.find(npc_id);
        if (it == threat_tables_.end()) {
            auto table = std::make_shared<ThreatTable>(npc_id);
            threat_tables_[npc_id] = table;
            return table;
        }
        return it->second;
    }
    
    void RemoveThreatTable(uint64_t npc_id) {
        threat_tables_.erase(npc_id);
    }
    
    void AddDamageThreat(uint64_t npc_id, uint64_t attacker_id, float damage) {
        auto table = GetThreatTable(npc_id);
        
        // Base threat = damage * threat modifier
        float threat = damage * GetDamageThreatModifier(attacker_id);
        
        table->AddThreat(attacker_id, threat, ThreatModifierType::DAMAGE_DEALT);
    }
    
    void AddHealingThreat(uint64_t healer_id, uint64_t target_id, float healing) {
        // Healing generates threat on all enemies attacking the target
        for (const auto& [npc_id, table] : threat_tables_) {
            if (table->HasThreat(target_id)) {
                table->AddThreat(healer_id, healing, ThreatModifierType::HEALING_DONE);
            }
        }
    }
    
    void AddAbilityThreat(uint64_t npc_id, uint64_t caster_id, 
                         float base_threat, ThreatModifierType type) {
        auto table = GetThreatTable(npc_id);
        table->AddThreat(caster_id, base_threat, type);
    }
    
    void TransferThreat(uint64_t npc_id, uint64_t from_id, uint64_t to_id, float percent) {
        auto table = GetThreatTable(npc_id);
        
        float from_threat = table->GetThreat(from_id);
        float transfer_amount = from_threat * (percent / 100.0f);
        
        table->ReduceThreat(from_id, transfer_amount);
        table->AddThreat(to_id, transfer_amount, ThreatModifierType::SPECIAL_ABILITY);
    }
    
    void CleanupInactiveTables() {
        std::vector<uint64_t> to_remove;
        
        for (const auto& [npc_id, table] : threat_tables_) {
            table->DropOutOfCombatThreat();
            
            if (table->IsEmpty()) {
                to_remove.push_back(npc_id);
            }
        }
        
        for (uint64_t npc_id : to_remove) {
            threat_tables_.erase(npc_id);
        }
    }
    
private:
    ThreatManager() = default;
    
    std::unordered_map<uint64_t, std::shared_ptr<ThreatTable>> threat_tables_;
    
    float GetDamageThreatModifier(uint64_t entity_id) {
        // TODO: Get class-based threat modifier
        // Tank classes: 2.0x
        // DPS classes: 1.0x
        // Healer classes: 0.8x
        return 1.0f;
    }
};

class ThreatUtils {
public:
    // Calculate threat reduction needed to not pull aggro
    static float CalculateThreatReduction(float current_threat, float tank_threat) {
        // Standard threshold: 110% for melee, 130% for ranged
        float threshold = 1.3f;  // Assume ranged
        float safe_threat = tank_threat * threshold * 0.9f;  // 90% of threshold
        
        return std::max(0.0f, current_threat - safe_threat);
    }
    
    // Check if entity will pull aggro
    static bool WillPullAggro(float entity_threat, float tank_threat, bool is_melee) {
        float threshold = is_melee ? 1.1f : 1.3f;
        return entity_threat > (tank_threat * threshold);
    }
    
    // Calculate threat percentage of tank's threat
    static float GetThreatPercentOfTank(float entity_threat, float tank_threat) {
        if (tank_threat <= 0) return 0.0f;
        return (entity_threat / tank_threat) * 100.0f;
    }
};

} // namespace mmorpg::game::combat
