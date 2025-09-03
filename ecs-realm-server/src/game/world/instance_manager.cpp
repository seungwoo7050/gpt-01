#include "instance_manager.h"
#include "map_transition_handler.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace mmorpg::game::world {

// [SEQUENCE: 541] Register instance template
void InstanceManager::RegisterInstanceTemplate(uint32_t template_id, const InstanceConfig& config) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    instance_templates_[template_id] = config;
    spdlog::info("Registered instance template {} - {}", template_id, config.instance_name);
}

// [SEQUENCE: 542] Create new instance
std::optional<uint64_t> InstanceManager::CreateInstance(
    uint32_t template_id,
    InstanceDifficulty difficulty,
    uint64_t leader_id,
    const std::vector<uint64_t>& party_members) {
    
    std::lock_guard<std::mutex> lock(instance_mutex_);
    
    // [SEQUENCE: 543] Validate template exists
    auto template_it = instance_templates_.find(template_id);
    if (template_it == instance_templates_.end()) {
        spdlog::error("Instance template {} not found", template_id);
        return std::nullopt;
    }
    
    const auto& config = template_it->second;
    
    // [SEQUENCE: 544] Check party size
    if (party_members.size() < config.min_players || 
        party_members.size() > config.max_players) {
        spdlog::error("Invalid party size {} for instance {}", 
                     party_members.size(), config.instance_name);
        return std::nullopt;
    }
    
    // [SEQUENCE: 545] Check lockouts for all party members
    for (uint64_t player_id : party_members) {
        if (HasValidLockout(player_id, template_id, difficulty)) {
            spdlog::error("Player {} has active lockout for instance", player_id);
            return std::nullopt;
        }
    }
    
    // [SEQUENCE: 546] Create map instance
    auto& map_manager = MapManager::Instance();
    auto map_instance = map_manager.CreateInstance(config.template_id);
    if (!map_instance) {
        spdlog::error("Failed to create map instance for template {}", template_id);
        return std::nullopt;
    }
    
    // [SEQUENCE: 547] Create instance progress
    uint64_t instance_guid = GenerateInstanceGuid();
    
    InstanceProgress progress;
    progress.instance_guid = instance_guid;
    progress.instance_id = map_instance->GetInstanceId();
    progress.state = InstanceState::ACTIVE;
    progress.difficulty = difficulty;
    progress.created_at = std::chrono::system_clock::now();
    progress.leader_id = leader_id;
    
    // Add allowed players
    progress.allowed_players.insert(party_members.begin(), party_members.end());
    
    // Initialize objectives
    for (const auto& obj : config.objectives) {
        progress.objective_progress[obj.objective_id] = 0;
    }
    
    active_instances_[instance_guid] = progress;
    
    // [SEQUENCE: 548] Schedule reset if needed
    if (config.reset_frequency != ResetFrequency::NEVER) {
        auto reset_time = CalculateResetTime(config.reset_frequency);
        scheduled_resets_.emplace(reset_time, instance_guid);
    }
    
    spdlog::info("Created instance {} (GUID: {}) for {} players", 
                 config.instance_name, instance_guid, party_members.size());
    
    return instance_guid;
}

// [SEQUENCE: 549] Check if player can enter instance
bool InstanceManager::CanEnterInstance(uint64_t player_id, uint64_t instance_guid, std::string& error) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    
    auto it = active_instances_.find(instance_guid);
    if (it == active_instances_.end()) {
        error = "Instance not found";
        return false;
    }
    
    const auto& progress = it->second;
    
    // [SEQUENCE: 550] Check if player is allowed
    if (progress.allowed_players.find(player_id) == progress.allowed_players.end()) {
        error = "Not allowed to enter this instance";
        return false;
    }
    
    // [SEQUENCE: 551] Check instance state
    if (progress.state == InstanceState::EXPIRED || 
        progress.state == InstanceState::RESETTING) {
        error = "Instance is no longer available";
        return false;
    }
    
    // [SEQUENCE: 552] Check if instance is full
    auto& map_manager = MapManager::Instance();
    auto map_instance = map_manager.GetInstance(progress.instance_id);
    if (map_instance && map_instance->IsFull()) {
        error = "Instance is full";
        return false;
    }
    
    return true;
}

// [SEQUENCE: 553] Player enters instance
bool InstanceManager::EnterInstance(uint64_t player_id, uint64_t instance_guid) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    
    auto it = active_instances_.find(instance_guid);
    if (it == active_instances_.end()) {
        return false;
    }
    
    auto& progress = it->second;
    
    // [SEQUENCE: 554] Mark first combat if not started
    if (progress.state == InstanceState::ACTIVE && progress.started_at.time_since_epoch().count() == 0) {
        progress.started_at = std::chrono::system_clock::now();
        progress.state = InstanceState::IN_PROGRESS;
    }
    
    // [SEQUENCE: 555] Add to saved players (creates lockout)
    progress.saved_players.insert(player_id);
    CreateInstanceSave(player_id, instance_guid);
    
    spdlog::info("Player {} entered instance {}", player_id, instance_guid);
    return true;
}

// [SEQUENCE: 556] Update objective progress
void InstanceManager::UpdateObjectiveProgress(uint64_t instance_guid, uint32_t objective_id, uint32_t count) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    
    auto inst_it = active_instances_.find(instance_guid);
    if (inst_it == active_instances_.end()) return;
    
    auto& progress = inst_it->second;
    
    // [SEQUENCE: 557] Update progress count
    auto obj_it = progress.objective_progress.find(objective_id);
    if (obj_it != progress.objective_progress.end()) {
        obj_it->second += count;
        
        spdlog::info("Instance {} objective {} progress: {}", 
                    instance_guid, objective_id, obj_it->second);
        
        // Check for completion
        CheckCompletion(instance_guid);
    }
}

// [SEQUENCE: 558] Record boss kill
void InstanceManager::RecordBossKill(uint64_t instance_guid, uint32_t boss_id) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    
    auto inst_it = active_instances_.find(instance_guid);
    if (inst_it == active_instances_.end()) return;
    
    auto& progress = inst_it->second;
    
    // [SEQUENCE: 559] Mark boss as killed
    if (progress.killed_bosses.insert(boss_id).second) {
        spdlog::info("Instance {} boss {} killed", instance_guid, boss_id);
        
        // Update saves for all saved players
        for (uint64_t player_id : progress.saved_players) {
            auto save_it = player_saves_.find(player_id);
            if (save_it != player_saves_.end()) {
                for (auto& save : save_it->second) {
                    if (save.save_id == instance_guid) {
                        save.killed_bosses.insert(boss_id);
                        break;
                    }
                }
            }
        }
        
        // [SEQUENCE: 560] Distribute loot
        DistributeLoot(instance_guid, boss_id);
        
        // Check for completion
        CheckCompletion(instance_guid);
    }
}

// [SEQUENCE: 561] Check if instance is complete
bool InstanceManager::CheckCompletion(uint64_t instance_guid) {
    auto inst_it = active_instances_.find(instance_guid);
    if (inst_it == active_instances_.end()) return false;
    
    auto& progress = inst_it->second;
    
    auto template_it = instance_templates_.find(progress.instance_id);
    if (template_it == instance_templates_.end()) return false;
    
    const auto& config = template_it->second;
    
    // [SEQUENCE: 562] Check all required objectives
    if (AreObjectivesComplete(progress, config)) {
        CompleteInstance(instance_guid);
        return true;
    }
    
    return false;
}

// [SEQUENCE: 563] Complete instance
void InstanceManager::CompleteInstance(uint64_t instance_guid) {
    auto inst_it = active_instances_.find(instance_guid);
    if (inst_it == active_instances_.end()) return;
    
    auto& progress = inst_it->second;
    
    if (progress.state == InstanceState::COMPLETED) return;
    
    progress.state = InstanceState::COMPLETED;
    progress.completed_at = std::chrono::system_clock::now();
    
    // [SEQUENCE: 564] Calculate completion time
    auto duration = std::chrono::duration_cast<std::chrono::minutes>(
        progress.completed_at - progress.started_at
    );
    
    spdlog::info("Instance {} completed in {} minutes", 
                instance_guid, duration.count());
    
    // [SEQUENCE: 565] Handle Mythic+ completion
    if (progress.mythic_level > 0) {
        auto template_it = instance_templates_.find(progress.instance_id);
        if (template_it != instance_templates_.end()) {
            const auto& config = template_it->second;
            bool in_time = duration <= config.time_limit;
            CompleteMythicPlus(instance_guid, in_time);
        }
    }
    
    // [SEQUENCE: 566] Schedule soft reset
    auto template_it = instance_templates_.find(progress.instance_id);
    if (template_it != instance_templates_.end()) {
        const auto& config = template_it->second;
        if (config.soft_reset_time.count() > 0) {
            auto reset_time = std::chrono::system_clock::now() + config.soft_reset_time;
            scheduled_resets_.emplace(reset_time, instance_guid);
        }
    }
}

// [SEQUENCE: 567] Create instance save for player
void InstanceManager::CreateInstanceSave(uint64_t player_id, uint64_t instance_guid) {
    auto inst_it = active_instances_.find(instance_guid);
    if (inst_it == active_instances_.end()) return;
    
    const auto& progress = inst_it->second;
    
    InstanceSave save;
    save.save_id = instance_guid;
    save.player_id = player_id;
    save.instance_template_id = progress.instance_id;
    save.difficulty = progress.difficulty;
    save.killed_bosses = progress.killed_bosses;
    
    // [SEQUENCE: 568] Calculate lockout expiration
    auto template_it = instance_templates_.find(progress.instance_id);
    if (template_it != instance_templates_.end()) {
        save.locked_until = CalculateResetTime(template_it->second.reset_frequency);
    }
    
    player_saves_[player_id].push_back(save);
    
    spdlog::info("Created instance save for player {} in instance {}", 
                player_id, instance_guid);
}

// [SEQUENCE: 569] Check if player has valid lockout
bool InstanceManager::HasValidLockout(uint64_t player_id, uint32_t template_id, InstanceDifficulty difficulty) {
    auto it = player_saves_.find(player_id);
    if (it == player_saves_.end()) return false;
    
    auto now = std::chrono::system_clock::now();
    
    for (const auto& save : it->second) {
        if (save.instance_template_id == template_id && 
            save.difficulty == difficulty &&
            save.locked_until > now &&
            !save.is_expired) {
            return true;
        }
    }
    
    return false;
}

// [SEQUENCE: 570] Generate unique instance GUID
uint64_t InstanceManager::GenerateInstanceGuid() {
    return next_instance_guid_.fetch_add(1);
}

// [SEQUENCE: 571] Calculate reset time based on frequency
std::chrono::system_clock::time_point InstanceManager::CalculateResetTime(ResetFrequency frequency) {
    using namespace std::chrono;
    
    auto now = system_clock::now();
    auto time_t_now = system_clock::to_time_t(now);
    std::tm tm_now = *std::localtime(&time_t_now);
    
    switch (frequency) {
        case ResetFrequency::DAILY: {
            // Next day at 6 AM server time
            tm_now.tm_hour = 6;
            tm_now.tm_min = 0;
            tm_now.tm_sec = 0;
            tm_now.tm_mday += 1;
            break;
        }
        case ResetFrequency::WEEKLY: {
            // Next Tuesday at 6 AM server time
            int days_until_tuesday = (2 - tm_now.tm_wday + 7) % 7;
            if (days_until_tuesday == 0) days_until_tuesday = 7;
            tm_now.tm_hour = 6;
            tm_now.tm_min = 0;
            tm_now.tm_sec = 0;
            tm_now.tm_mday += days_until_tuesday;
            break;
        }
        case ResetFrequency::MONTHLY: {
            // First day of next month at 6 AM
            tm_now.tm_hour = 6;
            tm_now.tm_min = 0;
            tm_now.tm_sec = 0;
            tm_now.tm_mday = 1;
            tm_now.tm_mon += 1;
            break;
        }
        default:
            return now + hours(24);  // Default to 24 hours
    }
    
    return system_clock::from_time_t(std::mktime(&tm_now));
}

// [SEQUENCE: 572] Check if all objectives are complete
bool InstanceManager::AreObjectivesComplete(const InstanceProgress& progress, const InstanceConfig& config) {
    for (const auto& obj : config.objectives) {
        if (!obj.required) continue;
        
        auto it = progress.objective_progress.find(obj.objective_id);
        if (it == progress.objective_progress.end() || it->second < obj.target_count) {
            return false;
        }
    }
    
    // [SEQUENCE: 573] Check if all bosses killed
    for (const auto& boss : config.bosses) {
        if (progress.killed_bosses.find(boss.boss_id) == progress.killed_bosses.end()) {
            return false;
        }
    }
    
    return true;
}

// [SEQUENCE: 574] Start Mythic+ instance
void InstanceManager::StartMythicPlus(uint64_t instance_guid, uint32_t keystone_level) {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    
    auto it = active_instances_.find(instance_guid);
    if (it == active_instances_.end()) return;
    
    auto& progress = it->second;
    progress.mythic_level = keystone_level;
    progress.difficulty = InstanceDifficulty::MYTHIC_PLUS;
    
    // [SEQUENCE: 575] Scale difficulty based on level
    ScaleMythicPlusDifficulty(instance_guid, keystone_level);
    
    spdlog::info("Started Mythic+ level {} for instance {}", 
                keystone_level, instance_guid);
}

// [SEQUENCE: 576] Process scheduled resets
void InstanceManager::ProcessScheduledResets() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    
    auto now = std::chrono::system_clock::now();
    
    auto it = scheduled_resets_.begin();
    while (it != scheduled_resets_.end() && it->first <= now) {
        ResetInstance(it->second);
        it = scheduled_resets_.erase(it);
    }
}

// [SEQUENCE: 577] Reset instance
void InstanceManager::ResetInstance(uint64_t instance_guid) {
    auto it = active_instances_.find(instance_guid);
    if (it == active_instances_.end()) return;
    
    auto& progress = it->second;
    progress.state = InstanceState::RESETTING;
    
    spdlog::info("Resetting instance {}", instance_guid);
    
    // Mark instance as expired
    progress.state = InstanceState::EXPIRED;
    
    // Expire all player saves
    for (uint64_t player_id : progress.saved_players) {
        auto save_it = player_saves_.find(player_id);
        if (save_it != player_saves_.end()) {
            for (auto& save : save_it->second) {
                if (save.save_id == instance_guid) {
                    save.is_expired = true;
                    break;
                }
            }
        }
    }
}

// [SEQUENCE: 578] Distribute loot placeholder
void InstanceManager::DistributeLoot(uint64_t instance_guid, uint32_t boss_id) {
    // TODO: Implement loot distribution based on loot mode
    spdlog::debug("Distributing loot for boss {} in instance {}", 
                 boss_id, instance_guid);
}

// [SEQUENCE: 579] Scale Mythic+ difficulty placeholder
void InstanceManager::ScaleMythicPlusDifficulty(uint64_t instance_guid, uint32_t level) {
    // TODO: Implement scaling of monster HP/damage based on M+ level
    // Typical scaling: +8% HP/damage per level
    float scaling_factor = 1.0f + (level * 0.08f);
    spdlog::debug("Scaling instance {} to M+ level {} ({}% increase)", 
                 instance_guid, level, (scaling_factor - 1.0f) * 100);
}

} // namespace mmorpg::game::world