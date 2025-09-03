#pragma once

#include <unordered_map>
#include <chrono>
#include <optional>
#include <functional>
#include <spdlog/spdlog.h>

namespace mmorpg::game::combat {

// [SEQUENCE: 1844] Death and resurrection system for character mortality
// ýLü €\ Ü¤\<\ ­0 ¬Ý ˜¬

// [SEQUENCE: 1845] Death state
enum class DeathState {
    ALIVE,              // Normal state
    DYING,              // Taking fatal damage
    DEAD,               // Fully dead
    SPIRIT,             // Ghost/spirit form
    RESURRECTING,       // In resurrection process
    RELEASED           // Released to graveyard
};

// [SEQUENCE: 1846] Death cause
enum class DeathCause {
    DAMAGE,             // Normal combat damage
    FALLING,            // Fall damage
    DROWNING,           // Underwater too long
    FATIGUE,            // Exhaustion in deep water
    ENVIRONMENTAL,      // Lava, poison zones
    SACRIFICE,          // Intentional death (spell)
    INSTAKILL,          // Boss mechanics
    DISCONNECT         // Died while offline
};

// [SEQUENCE: 1847] Resurrection type
enum class ResurrectionType {
    SPELL,              // Player resurrection spell
    ITEM,               // Resurrection item
    NPC,                // Spirit healer
    GRAVEYARD,          // Graveyard resurrection
    BATTLE_REZ,         // In-combat resurrection
    SELF_REZ,           // Self-resurrection ability
    MASS_REZ,           // Area resurrection
    SOULSTONE          // Pre-cast resurrection
};

// [SEQUENCE: 1848] Death penalties
struct DeathPenalty {
    float durability_loss = 10.0f;      // % durability loss
    float resurrection_sickness = 75.0f;  // % stat reduction
    std::chrono::minutes sickness_duration{10};
    float experience_loss = 0.0f;        // % exp loss (if enabled)
    uint32_t honor_loss = 0;             // PvP honor loss
    bool drop_items = false;             // Drop items on death
    float spirit_speed_bonus = 50.0f;    // % speed in ghost form
};

// [SEQUENCE: 1849] Corpse data
struct Corpse {
    uint64_t corpse_id;
    uint64_t owner_id;
    
    // Location
    float position_x;
    float position_y;
    float position_z;
    uint32_t zone_id;
    uint32_t map_id;
    
    // Timing
    std::chrono::system_clock::time_point death_time;
    std::chrono::system_clock::time_point decay_time;
    
    // Properties
    bool lootable = false;              // Has loot
    bool skinnable = false;             // Can be skinned
    bool resurrectable = true;          // Can be resurrected
    std::vector<uint64_t> items;        // Dropped items
    
    // [SEQUENCE: 1850] Check if corpse can be resurrected
    bool CanResurrect() const {
        if (!resurrectable) return false;
        
        auto now = std::chrono::system_clock::now();
        // Can't resurrect decayed corpses
        if (now >= decay_time) return false;
        
        // Check resurrection timer (prevent spam)
        auto time_since_death = now - death_time;
        if (time_since_death < std::chrono::seconds(2)) {
            return false;  // 2 second minimum
        }
        
        return true;
    }
    
    // [SEQUENCE: 1851] Calculate decay time remaining
    std::chrono::seconds GetDecayTimeRemaining() const {
        auto now = std::chrono::system_clock::now();
        if (now >= decay_time) {
            return std::chrono::seconds(0);
        }
        return std::chrono::duration_cast<std::chrono::seconds>(decay_time - now);
    }
};

// [SEQUENCE: 1852] Resurrection request
struct ResurrectionRequest {
    uint64_t request_id;
    uint64_t caster_id;
    uint64_t target_id;
    ResurrectionType type;
    
    // Resurrection properties
    float health_percent = 50.0f;       // % health on res
    float mana_percent = 20.0f;         // % mana on res
    bool remove_penalties = false;      // Remove res sickness
    
    // Location (for accepting)
    float res_position_x;
    float res_position_y;
    float res_position_z;
    
    // Timing
    std::chrono::system_clock::time_point expire_time;
    
    bool IsExpired() const {
        return std::chrono::system_clock::now() >= expire_time;
    }
};

// [SEQUENCE: 1853] Spirit healer
struct SpiritHealer {
    uint64_t healer_id;
    float position_x;
    float position_y;
    float position_z;
    float interaction_range = 20.0f;
    
    // Resurrection cost
    uint32_t gold_cost = 0;
    float durability_penalty = 25.0f;   // Additional penalty
    std::chrono::minutes sickness_duration{10};
};

// [SEQUENCE: 1854] Death manager for entity
class DeathManager {
public:
    DeathManager(uint64_t entity_id) : entity_id_(entity_id) {}
    
    // [SEQUENCE: 1855] Process death
    void ProcessDeath(DeathCause cause, uint64_t killer_id = 0) {
        if (state_ != DeathState::ALIVE && state_ != DeathState::DYING) {
            return;  // Already dead
        }
        
        state_ = DeathState::DYING;
        death_cause_ = cause;
        killer_id_ = killer_id;
        death_time_ = std::chrono::system_clock::now();
        
        // Transition to dead
        state_ = DeathState::DEAD;
        
        // Create corpse
        CreateCorpse();
        
        // Apply death penalties
        ApplyDeathPenalties();
        
        // Log death
        spdlog::info("Entity {} died from {} (killer: {})", 
                    entity_id_, static_cast<int>(cause), killer_id);
        
        // Trigger callbacks
        if (on_death_callback_) {
            on_death_callback_(entity_id_, cause, killer_id);
        }
    }
    
    // [SEQUENCE: 1856] Release spirit
    void ReleaseSpirit() {
        if (state_ != DeathState::DEAD) {
            return;
        }
        
        state_ = DeathState::SPIRIT;
        spirit_release_time_ = std::chrono::system_clock::now();
        
        // Move to nearest graveyard
        auto graveyard = FindNearestGraveyard();
        if (graveyard) {
            // Teleport to graveyard
            TeleportToGraveyard(*graveyard);
        }
        
        // Apply spirit form
        ApplySpiritForm();
        
        spdlog::info("Entity {} released spirit", entity_id_);
    }
    
    // [SEQUENCE: 1857] Request resurrection
    uint64_t CreateResurrectionRequest(uint64_t caster_id, ResurrectionType type,
                                       float health_pct = 50.0f, float mana_pct = 20.0f) {
        if (state_ != DeathState::DEAD && state_ != DeathState::SPIRIT) {
            return 0;  // Not dead
        }
        
        // Check if corpse can be resurrected
        if (corpse_ && !corpse_->CanResurrect()) {
            return 0;
        }
        
        static std::atomic<uint64_t> request_id_counter{1};
        
        ResurrectionRequest request;
        request.request_id = request_id_counter++;
        request.caster_id = caster_id;
        request.target_id = entity_id_;
        request.type = type;
        request.health_percent = health_pct;
        request.mana_percent = mana_pct;
        request.expire_time = std::chrono::system_clock::now() + std::chrono::minutes(1);
        
        // Set resurrection location
        if (corpse_) {
            request.res_position_x = corpse_->position_x;
            request.res_position_y = corpse_->position_y;
            request.res_position_z = corpse_->position_z;
        }
        
        pending_resurrections_[request.request_id] = request;
        
        spdlog::info("Resurrection request {} created for entity {} by {}", 
                    request.request_id, entity_id_, caster_id);
        
        return request.request_id;
    }
    
    // [SEQUENCE: 1858] Accept resurrection
    bool AcceptResurrection(uint64_t request_id) {
        auto it = pending_resurrections_.find(request_id);
        if (it == pending_resurrections_.end()) {
            return false;
        }
        
        const auto& request = it->second;
        if (request.IsExpired()) {
            pending_resurrections_.erase(it);
            return false;
        }
        
        // Start resurrection
        state_ = DeathState::RESURRECTING;
        resurrection_time_ = std::chrono::system_clock::now();
        
        // Apply resurrection
        PerformResurrection(request);
        
        // Remove request
        pending_resurrections_.erase(it);
        
        return true;
    }
    
    // [SEQUENCE: 1859] Decline resurrection
    void DeclineResurrection(uint64_t request_id) {
        pending_resurrections_.erase(request_id);
        spdlog::info("Entity {} declined resurrection {}", entity_id_, request_id);
    }
    
    // [SEQUENCE: 1860] Spirit healer resurrection
    bool ResurrectAtSpiritHealer(const SpiritHealer& healer) {
        if (state_ != DeathState::SPIRIT) {
            return false;
        }
        
        // Check range
        float distance = CalculateDistance(healer.position_x, healer.position_y, healer.position_z);
        if (distance > healer.interaction_range) {
            return false;
        }
        
        // Check gold
        if (!HasGold(healer.gold_cost)) {
            return false;
        }
        
        // Deduct gold
        DeductGold(healer.gold_cost);
        
        // Apply extra penalties
        death_penalty_.durability_loss += healer.durability_penalty;
        death_penalty_.sickness_duration = healer.sickness_duration;
        
        // Resurrect at healer location
        ResurrectionRequest request;
        request.type = ResurrectionType::GRAVEYARD;
        request.health_percent = 50.0f;
        request.mana_percent = 0.0f;
        request.res_position_x = healer.position_x;
        request.res_position_y = healer.position_y;
        request.res_position_z = healer.position_z;
        
        PerformResurrection(request);
        
        return true;
    }
    
    // [SEQUENCE: 1861] Reclaim corpse (run back)
    bool ReclaimCorpse() {
        if (state_ != DeathState::SPIRIT || !corpse_) {
            return false;
        }
        
        // Check distance to corpse
        float distance = CalculateDistanceToCorpse();
        const float reclaim_range = 30.0f;
        
        if (distance > reclaim_range) {
            return false;
        }
        
        // Resurrect at corpse with penalties
        ResurrectionRequest request;
        request.type = ResurrectionType::GRAVEYARD;
        request.health_percent = 50.0f;
        request.mana_percent = 50.0f;
        request.res_position_x = corpse_->position_x;
        request.res_position_y = corpse_->position_y;
        request.res_position_z = corpse_->position_z;
        
        PerformResurrection(request);
        
        return true;
    }
    
    // [SEQUENCE: 1862] Update death state
    void Update() {
        // Clean expired resurrection requests
        std::erase_if(pending_resurrections_, [](const auto& pair) {
            return pair.second.IsExpired();
        });
        
        // Update corpse decay
        if (corpse_ && std::chrono::system_clock::now() >= corpse_->decay_time) {
            DecayCorpse();
        }
        
        // Update resurrection sickness
        if (has_resurrection_sickness_) {
            auto now = std::chrono::system_clock::now();
            if (now >= sickness_end_time_) {
                RemoveResurrectionSickness();
            }
        }
    }
    
    // Query methods
    DeathState GetState() const { return state_; }
    bool IsDead() const { return state_ == DeathState::DEAD || state_ == DeathState::SPIRIT; }
    bool IsAlive() const { return state_ == DeathState::ALIVE; }
    bool IsSpirit() const { return state_ == DeathState::SPIRIT; }
    
    const Corpse* GetCorpse() const { return corpse_.get(); }
    
    std::vector<ResurrectionRequest> GetPendingResurrections() const {
        std::vector<ResurrectionRequest> requests;
        for (const auto& [id, req] : pending_resurrections_) {
            if (!req.IsExpired()) {
                requests.push_back(req);
            }
        }
        return requests;
    }
    
    // Callbacks
    void SetDeathCallback(std::function<void(uint64_t, DeathCause, uint64_t)> callback) {
        on_death_callback_ = callback;
    }
    
    void SetResurrectionCallback(std::function<void(uint64_t, ResurrectionType)> callback) {
        on_resurrection_callback_ = callback;
    }
    
private:
    uint64_t entity_id_;
    DeathState state_ = DeathState::ALIVE;
    DeathCause death_cause_;
    uint64_t killer_id_ = 0;
    
    // Timing
    std::chrono::system_clock::time_point death_time_;
    std::chrono::system_clock::time_point spirit_release_time_;
    std::chrono::system_clock::time_point resurrection_time_;
    
    // Death data
    std::unique_ptr<Corpse> corpse_;
    DeathPenalty death_penalty_;
    std::unordered_map<uint64_t, ResurrectionRequest> pending_resurrections_;
    
    // Resurrection sickness
    bool has_resurrection_sickness_ = false;
    std::chrono::system_clock::time_point sickness_end_time_;
    
    // Callbacks
    std::function<void(uint64_t, DeathCause, uint64_t)> on_death_callback_;
    std::function<void(uint64_t, ResurrectionType)> on_resurrection_callback_;
    
    // [SEQUENCE: 1863] Create corpse
    void CreateCorpse() {
        static std::atomic<uint64_t> corpse_id_counter{1};
        
        corpse_ = std::make_unique<Corpse>();
        corpse_->corpse_id = corpse_id_counter++;
        corpse_->owner_id = entity_id_;
        corpse_->death_time = death_time_;
        corpse_->decay_time = death_time_ + std::chrono::minutes(5);  // 5 min decay
        
        // Get position from entity
        GetEntityPosition(corpse_->position_x, corpse_->position_y, corpse_->position_z);
        corpse_->zone_id = GetEntityZone();
        corpse_->map_id = GetEntityMap();
        
        // Check if should drop items
        if (death_penalty_.drop_items) {
            // TODO: Drop items
        }
    }
    
    // [SEQUENCE: 1864] Apply death penalties
    void ApplyDeathPenalties() {
        // Durability loss
        if (death_penalty_.durability_loss > 0) {
            ApplyDurabilityLoss(death_penalty_.durability_loss);
        }
        
        // Experience loss
        if (death_penalty_.experience_loss > 0) {
            ApplyExperienceLoss(death_penalty_.experience_loss);
        }
        
        // Honor loss (PvP)
        if (death_penalty_.honor_loss > 0 && IsPvPDeath()) {
            ApplyHonorLoss(death_penalty_.honor_loss);
        }
    }
    
    // [SEQUENCE: 1865] Perform resurrection
    void PerformResurrection(const ResurrectionRequest& request) {
        // Restore health/mana
        SetHealthPercent(request.health_percent);
        SetManaPercent(request.mana_percent);
        
        // Move to resurrection location
        TeleportTo(request.res_position_x, request.res_position_y, request.res_position_z);
        
        // Apply resurrection sickness
        if (!request.remove_penalties && 
            request.type != ResurrectionType::BATTLE_REZ) {
            ApplyResurrectionSickness();
        }
        
        // Remove spirit form
        RemoveSpiritForm();
        
        // Update state
        state_ = DeathState::ALIVE;
        
        // Clear corpse
        corpse_.reset();
        
        spdlog::info("Entity {} resurrected by {} (type: {})", 
                    entity_id_, request.caster_id, static_cast<int>(request.type));
        
        // Trigger callback
        if (on_resurrection_callback_) {
            on_resurrection_callback_(entity_id_, request.type);
        }
    }
    
    // [SEQUENCE: 1866] Apply resurrection sickness
    void ApplyResurrectionSickness() {
        has_resurrection_sickness_ = true;
        sickness_end_time_ = std::chrono::system_clock::now() + death_penalty_.sickness_duration;
        
        // Apply stat reduction
        ApplyStatModifier(-death_penalty_.resurrection_sickness);
    }
    
    // [SEQUENCE: 1867] Remove resurrection sickness
    void RemoveResurrectionSickness() {
        has_resurrection_sickness_ = false;
        
        // Remove stat reduction
        RemoveStatModifier();
        
        spdlog::info("Resurrection sickness removed from entity {}", entity_id_);
    }
    
    // [SEQUENCE: 1868] Decay corpse
    void DecayCorpse() {
        if (!corpse_) return;
        
        // Drop any remaining items
        if (!corpse_->items.empty()) {
            // TODO: Create loot container at corpse location
        }
        
        corpse_.reset();
        spdlog::info("Corpse for entity {} decayed", entity_id_);
    }
    
    // [SEQUENCE: 1869] Apply spirit form
    void ApplySpiritForm() {
        // Increase movement speed
        ApplySpeedModifier(death_penalty_.spirit_speed_bonus);
        
        // Make untargetable
        SetUntargetable(true);
        
        // Apply visual effect
        ApplySpiritVisual();
    }
    
    // [SEQUENCE: 1870] Remove spirit form
    void RemoveSpiritForm() {
        RemoveSpeedModifier();
        SetUntargetable(false);
        RemoveSpiritVisual();
    }
    
    // Helper methods (to be implemented by entity system)
    void GetEntityPosition(float& x, float& y, float& z) const;
    uint32_t GetEntityZone() const;
    uint32_t GetEntityMap() const;
    float CalculateDistance(float x, float y, float z) const;
    float CalculateDistanceToCorpse() const;
    bool HasGold(uint32_t amount) const;
    void DeductGold(uint32_t amount);
    void SetHealthPercent(float percent);
    void SetManaPercent(float percent);
    void TeleportTo(float x, float y, float z);
    void TeleportToGraveyard(const SpiritHealer& graveyard);
    void ApplyDurabilityLoss(float percent);
    void ApplyExperienceLoss(float percent);
    void ApplyHonorLoss(uint32_t amount);
    void ApplyStatModifier(float percent);
    void RemoveStatModifier();
    void ApplySpeedModifier(float percent);
    void RemoveSpeedModifier();
    void SetUntargetable(bool untargetable);
    void ApplySpiritVisual();
    void RemoveSpiritVisual();
    bool IsPvPDeath() const;
    const SpiritHealer* FindNearestGraveyard() const;
};

// [SEQUENCE: 1871] Global death system
class DeathSystem {
public:
    static DeathSystem& Instance() {
        static DeathSystem instance;
        return instance;
    }
    
    // [SEQUENCE: 1872] Initialize system
    void Initialize() {
        LoadSpiritHealers();
        LoadDeathPenalties();
        spdlog::info("Death system initialized with {} spirit healers", 
                    spirit_healers_.size());
    }
    
    // [SEQUENCE: 1873] Get or create death manager
    std::shared_ptr<DeathManager> GetManager(uint64_t entity_id) {
        auto it = entity_managers_.find(entity_id);
        if (it == entity_managers_.end()) {
            auto manager = std::make_shared<DeathManager>(entity_id);
            entity_managers_[entity_id] = manager;
            return manager;
        }
        return it->second;
    }
    
    // [SEQUENCE: 1874] Process entity death
    void ProcessDeath(uint64_t entity_id, DeathCause cause, uint64_t killer_id = 0) {
        auto manager = GetManager(entity_id);
        manager->ProcessDeath(cause, killer_id);
        
        // Handle special death types
        if (cause == DeathCause::FALLING) {
            // Instant release for fall deaths
            manager->ReleaseSpirit();
        }
    }
    
    // [SEQUENCE: 1875] Find nearby spirit healers
    std::vector<const SpiritHealer*> FindNearbyHealers(float x, float y, float z, 
                                                       float range = 1000.0f) const {
        std::vector<const SpiritHealer*> nearby;
        
        for (const auto& [id, healer] : spirit_healers_) {
            float dx = healer.position_x - x;
            float dy = healer.position_y - y;
            float dz = healer.position_z - z;
            float distance = std::sqrt(dx*dx + dy*dy + dz*dz);
            
            if (distance <= range) {
                nearby.push_back(&healer);
            }
        }
        
        return nearby;
    }
    
    // [SEQUENCE: 1876] Update all managers
    void UpdateAll() {
        for (auto& [entity_id, manager] : entity_managers_) {
            manager->Update();
        }
        
        // Clean up managers for alive entities
        std::erase_if(entity_managers_, [](const auto& pair) {
            return pair.second->IsAlive() && 
                   pair.second->GetPendingResurrections().empty();
        });
    }
    
    // [SEQUENCE: 1877] Get death penalties
    const DeathPenalty& GetDeathPenalty() const {
        return death_penalty_;
    }
    
private:
    DeathSystem() = default;
    
    std::unordered_map<uint64_t, std::shared_ptr<DeathManager>> entity_managers_;
    std::unordered_map<uint64_t, SpiritHealer> spirit_healers_;
    DeathPenalty death_penalty_;
    
    // [SEQUENCE: 1878] Load spirit healers
    void LoadSpiritHealers() {
        // Example spirit healers
        SpiritHealer healer1;
        healer1.healer_id = 1;
        healer1.position_x = 100.0f;
        healer1.position_y = 100.0f;
        healer1.position_z = 10.0f;
        healer1.gold_cost = 100;
        spirit_healers_[1] = healer1;
        
        // TODO: Load from world data
    }
    
    // [SEQUENCE: 1879] Load death penalties
    void LoadDeathPenalties() {
        // Default penalties
        death_penalty_.durability_loss = 10.0f;
        death_penalty_.resurrection_sickness = 75.0f;
        death_penalty_.sickness_duration = std::chrono::minutes(10);
        death_penalty_.spirit_speed_bonus = 50.0f;
        
        // TODO: Load from config
    }
};

// [SEQUENCE: 1880] Stub implementations
void DeathManager::GetEntityPosition(float& x, float& y, float& z) const {
    // TODO: Get from entity system
    x = y = z = 0.0f;
}

uint32_t DeathManager::GetEntityZone() const { return 1; }
uint32_t DeathManager::GetEntityMap() const { return 1; }

float DeathManager::CalculateDistance(float x, float y, float z) const {
    float ex, ey, ez;
    GetEntityPosition(ex, ey, ez);
    float dx = x - ex, dy = y - ey, dz = z - ez;
    return std::sqrt(dx*dx + dy*dy + dz*dz);
}

float DeathManager::CalculateDistanceToCorpse() const {
    if (!corpse_) return 999999.0f;
    return CalculateDistance(corpse_->position_x, corpse_->position_y, corpse_->position_z);
}

bool DeathManager::HasGold(uint32_t amount) const { return true; }
void DeathManager::DeductGold(uint32_t amount) {}
void DeathManager::SetHealthPercent(float percent) {}
void DeathManager::SetManaPercent(float percent) {}
void DeathManager::TeleportTo(float x, float y, float z) {}
void DeathManager::TeleportToGraveyard(const SpiritHealer& graveyard) {}
void DeathManager::ApplyDurabilityLoss(float percent) {}
void DeathManager::ApplyExperienceLoss(float percent) {}
void DeathManager::ApplyHonorLoss(uint32_t amount) {}
void DeathManager::ApplyStatModifier(float percent) {}
void DeathManager::RemoveStatModifier() {}
void DeathManager::ApplySpeedModifier(float percent) {}
void DeathManager::RemoveSpeedModifier() {}
void DeathManager::SetUntargetable(bool untargetable) {}
void DeathManager::ApplySpiritVisual() {}
void DeathManager::RemoveSpiritVisual() {}
bool DeathManager::IsPvPDeath() const { return killer_id_ > 0; }

const SpiritHealer* DeathManager::FindNearestGraveyard() const {
    float x, y, z;
    GetEntityPosition(x, y, z);
    auto healers = DeathSystem::Instance().FindNearbyHealers(x, y, z);
    return healers.empty() ? nullptr : healers[0];
}

} // namespace mmorpg::game::combat