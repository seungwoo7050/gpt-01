#include "quest_system.h"
#include "../items/item_system.h"
#include "../stats/character_stats.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace mmorpg::game::quest {

// [SEQUENCE: 1337] QuestLog - Can accept quest
bool QuestLog::CanAcceptQuest(uint32_t quest_id) const {
    // Check if already active
    if (active_quests_.find(quest_id) != active_quests_.end()) {
        return false;
    }
    
    // Check quest limit
    if (active_quests_.size() >= MAX_ACTIVE_QUESTS) {
        spdlog::warn("Entity {} cannot accept quest {}: quest limit reached", 
                     entity_id_, quest_id);
        return false;
    }
    
    // Get quest definition
    const auto* quest_def = QuestManager::Instance().GetQuest(quest_id);
    if (!quest_def) {
        return false;
    }
    
    // Check if already completed (for non-repeatable)
    if (!quest_def->is_repeatable && HasCompletedQuest(quest_id)) {
        return false;
    }
    
    // Check cooldown for repeatable quests
    if (quest_def->is_repeatable && quest_def->cooldown_seconds > 0) {
        auto it = quest_completion_counts_.find(quest_id);
        if (it != quest_completion_counts_.end()) {
            // TODO: Check last completion time
        }
    }
    
    // Check requirements
    return MeetsRequirements(*quest_def);
}

// [SEQUENCE: 1338] QuestLog - Accept quest
bool QuestLog::AcceptQuest(uint32_t quest_id) {
    if (!CanAcceptQuest(quest_id)) {
        return false;
    }
    
    const auto* quest_def = QuestManager::Instance().GetQuest(quest_id);
    if (!quest_def) {
        return false;
    }
    
    // Create progress entry
    QuestProgress progress;
    progress.quest_id = quest_id;
    progress.state = QuestState::ACTIVE;
    progress.start_time = std::chrono::steady_clock::now();
    
    // Initialize objectives
    InitializeObjectives(progress, *quest_def);
    
    // Add to active quests
    active_quests_[quest_id] = progress;
    
    spdlog::info("Entity {} accepted quest {}: {}", 
                 entity_id_, quest_id, quest_def->name);
    
    return true;
}

// [SEQUENCE: 1339] QuestLog - Abandon quest
bool QuestLog::AbandonQuest(uint32_t quest_id) {
    auto it = active_quests_.find(quest_id);
    if (it == active_quests_.end()) {
        return false;
    }
    
    it->second.state = QuestState::ABANDONED;
    active_quests_.erase(it);
    
    spdlog::info("Entity {} abandoned quest {}", entity_id_, quest_id);
    return true;
}

// [SEQUENCE: 1340] QuestLog - Complete quest
bool QuestLog::CompleteQuest(uint32_t quest_id) {
    auto it = active_quests_.find(quest_id);
    if (it == active_quests_.end()) {
        return false;
    }
    
    auto& progress = it->second;
    if (!progress.IsComplete()) {
        spdlog::warn("Entity {} trying to complete incomplete quest {}", 
                     entity_id_, quest_id);
        return false;
    }
    
    const auto* quest_def = QuestManager::Instance().GetQuest(quest_id);
    if (!quest_def) {
        return false;
    }
    
    // Mark as completed
    progress.state = QuestState::COMPLETED;
    progress.complete_time = std::chrono::steady_clock::now();
    progress.completion_count++;
    
    // Add to completed history
    completed_quests_.insert(quest_id);
    quest_completion_counts_[quest_id]++;
    
    // Grant rewards (handled elsewhere)
    progress.state = QuestState::REWARDED;
    
    // Remove from active
    active_quests_.erase(it);
    
    spdlog::info("Entity {} completed quest {}: {}", 
                 entity_id_, quest_id, quest_def->name);
    
    // Process quest chain
    if (quest_def->next_quest_id != 0) {
        QuestManager::Instance().ProcessQuestChain(entity_id_, quest_id);
    }
    
    return true;
}

// [SEQUENCE: 1341] QuestLog - Update objective progress
void QuestLog::UpdateObjectiveProgress(uint32_t quest_id, uint32_t objective_id, int32_t delta) {
    auto it = active_quests_.find(quest_id);
    if (it == active_quests_.end() || it->second.state != QuestState::ACTIVE) {
        return;
    }
    
    auto& progress = it->second;
    
    // Find objective
    for (auto& obj : progress.objectives) {
        if (obj.objective_id == objective_id) {
            uint32_t old_count = obj.current_count;
            obj.current_count = std::max(0, static_cast<int32_t>(obj.current_count) + delta);
            obj.current_count = std::min(obj.current_count, obj.target_count);
            
            if (old_count != obj.current_count) {
                progress.last_update = std::chrono::steady_clock::now();
                
                spdlog::debug("Quest {} objective {} progress: {}/{}", 
                             quest_id, objective_id, obj.current_count, obj.target_count);
                
                // Check if quest is now complete
                if (progress.IsComplete()) {
                    const auto* quest_def = QuestManager::Instance().GetQuest(quest_id);
                    if (quest_def && quest_def->auto_complete) {
                        CompleteQuest(quest_id);
                    }
                }
            }
            break;
        }
    }
}

// [SEQUENCE: 1342] QuestLog - Update progress by type
void QuestLog::UpdateProgress(ObjectiveType type, uint32_t target_id, int32_t count) {
    for (auto& [quest_id, progress] : active_quests_) {
        if (progress.state != QuestState::ACTIVE) {
            continue;
        }
        
        for (auto& obj : progress.objectives) {
            if (obj.type == type && obj.target_id == target_id && !obj.IsComplete()) {
                UpdateObjectiveProgress(quest_id, obj.objective_id, count);
            }
        }
    }
}

// [SEQUENCE: 1343] QuestLog - Get quest progress
QuestProgress* QuestLog::GetQuestProgress(uint32_t quest_id) {
    auto it = active_quests_.find(quest_id);
    return (it != active_quests_.end()) ? &it->second : nullptr;
}

const QuestProgress* QuestLog::GetQuestProgress(uint32_t quest_id) const {
    auto it = active_quests_.find(quest_id);
    return (it != active_quests_.end()) ? &it->second : nullptr;
}

// [SEQUENCE: 1344] QuestLog - Get active quests
std::vector<uint32_t> QuestLog::GetActiveQuests() const {
    std::vector<uint32_t> result;
    result.reserve(active_quests_.size());
    
    for (const auto& [quest_id, progress] : active_quests_) {
        result.push_back(quest_id);
    }
    
    return result;
}

// [SEQUENCE: 1345] QuestLog - Get completed quests
std::vector<uint32_t> QuestLog::GetCompletedQuests() const {
    return std::vector<uint32_t>(completed_quests_.begin(), completed_quests_.end());
}

// [SEQUENCE: 1346] QuestLog - Has completed quest
bool QuestLog::HasCompletedQuest(uint32_t quest_id) const {
    return completed_quests_.find(quest_id) != completed_quests_.end();
}

// [SEQUENCE: 1347] QuestLog - Meets requirements
bool QuestLog::MeetsRequirements(const QuestDefinition& quest) const {
    const auto& req = quest.requirements;
    
    // TODO: Get player level from character stats
    // if (player_level < req.min_level || 
    //     (req.max_level > 0 && player_level > req.max_level)) {
    //     return false;
    // }
    
    // Check prerequisite quests
    for (uint32_t prereq_id : req.required_quests) {
        if (!HasCompletedQuest(prereq_id)) {
            return false;
        }
    }
    
    // TODO: Check items, skills, class, faction
    
    // Custom requirement check
    if (req.custom_check && !req.custom_check(entity_id_)) {
        return false;
    }
    
    return true;
}

// [SEQUENCE: 1348] QuestLog - Initialize objectives
void QuestLog::InitializeObjectives(QuestProgress& progress, const QuestDefinition& quest) {
    progress.objectives = quest.objectives;
    
    // Reset all counts
    for (auto& obj : progress.objectives) {
        obj.current_count = 0;
    }
}

// [SEQUENCE: 1349] QuestManager - Register quest
void QuestManager::RegisterQuest(const QuestDefinition& quest) {
    quest_definitions_[quest.quest_id] = quest;
    
    // Update indices
    quests_by_type_[quest.type].push_back(quest.quest_id);
    
    if (quest.start_npc_id != 0) {
        quests_by_npc_[quest.start_npc_id].push_back(quest.quest_id);
    }
    
    spdlog::info("Registered quest: {} (ID: {})", quest.name, quest.quest_id);
}

// [SEQUENCE: 1350] QuestManager - Get quest
const QuestDefinition* QuestManager::GetQuest(uint32_t quest_id) const {
    auto it = quest_definitions_.find(quest_id);
    return (it != quest_definitions_.end()) ? &it->second : nullptr;
}

// [SEQUENCE: 1351] QuestManager - Create quest log
std::shared_ptr<QuestLog> QuestManager::CreateQuestLog(uint64_t entity_id) {
    auto quest_log = std::make_shared<QuestLog>(entity_id);
    quest_logs_[entity_id] = quest_log;
    
    spdlog::debug("Created quest log for entity {}", entity_id);
    return quest_log;
}

// [SEQUENCE: 1352] QuestManager - Get quest log
std::shared_ptr<QuestLog> QuestManager::GetQuestLog(uint64_t entity_id) const {
    auto it = quest_logs_.find(entity_id);
    return (it != quest_logs_.end()) ? it->second : nullptr;
}

// [SEQUENCE: 1353] QuestManager - Remove quest log
void QuestManager::RemoveQuestLog(uint64_t entity_id) {
    quest_logs_.erase(entity_id);
    spdlog::debug("Removed quest log for entity {}", entity_id);
}

// [SEQUENCE: 1354] QuestManager - Get available quests
std::vector<uint32_t> QuestManager::GetAvailableQuests(uint64_t entity_id) const {
    std::vector<uint32_t> result;
    
    auto quest_log = GetQuestLog(entity_id);
    if (!quest_log) {
        return result;
    }
    
    for (const auto& [quest_id, quest_def] : quest_definitions_) {
        if (quest_log->CanAcceptQuest(quest_id)) {
            result.push_back(quest_id);
        }
    }
    
    return result;
}

// [SEQUENCE: 1355] QuestManager - Get quests by type
std::vector<uint32_t> QuestManager::GetQuestsByType(QuestType type) const {
    auto it = quests_by_type_.find(type);
    return (it != quests_by_type_.end()) ? it->second : std::vector<uint32_t>();
}

// [SEQUENCE: 1356] QuestManager - Get quests by NPC
std::vector<uint32_t> QuestManager::GetQuestsByNPC(uint32_t npc_id) const {
    auto it = quests_by_npc_.find(npc_id);
    return (it != quests_by_npc_.end()) ? it->second : std::vector<uint32_t>();
}

// [SEQUENCE: 1357] QuestManager - On monster killed
void QuestManager::OnMonsterKilled(uint64_t killer_id, uint32_t monster_id) {
    auto quest_log = GetQuestLog(killer_id);
    if (!quest_log) {
        return;
    }
    
    quest_log->UpdateProgress(ObjectiveType::KILL, monster_id, 1);
}

// [SEQUENCE: 1358] QuestManager - On item collected
void QuestManager::OnItemCollected(uint64_t collector_id, uint32_t item_id, uint32_t count) {
    auto quest_log = GetQuestLog(collector_id);
    if (!quest_log) {
        return;
    }
    
    quest_log->UpdateProgress(ObjectiveType::COLLECT, item_id, count);
}

// [SEQUENCE: 1359] QuestManager - On location reached
void QuestManager::OnLocationReached(uint64_t entity_id, float x, float y, float z) {
    auto quest_log = GetQuestLog(entity_id);
    if (!quest_log) {
        return;
    }
    
    // Check all active quests for location objectives
    for (uint32_t quest_id : quest_log->GetActiveQuests()) {
        auto* progress = quest_log->GetQuestProgress(quest_id);
        if (!progress || progress->state != QuestState::ACTIVE) {
            continue;
        }
        
        for (auto& obj : progress->objectives) {
            if (obj.type == ObjectiveType::REACH_LOCATION && !obj.IsComplete()) {
                // Check distance
                float dx = x - obj.target_x;
                float dy = y - obj.target_y;
                float dz = z - obj.target_z;
                float distance = std::sqrt(dx*dx + dy*dy + dz*dz);
                
                if (distance <= obj.radius) {
                    quest_log->UpdateObjectiveProgress(quest_id, obj.objective_id, 1);
                }
            }
        }
    }
}

// [SEQUENCE: 1360] QuestManager - On NPC interaction
void QuestManager::OnNPCInteraction(uint64_t entity_id, uint32_t npc_id) {
    auto quest_log = GetQuestLog(entity_id);
    if (!quest_log) {
        return;
    }
    
    // Update INTERACT objectives
    quest_log->UpdateProgress(ObjectiveType::INTERACT, npc_id, 1);
    
    // Check for quest completion at this NPC
    for (uint32_t quest_id : quest_log->GetActiveQuests()) {
        auto* progress = quest_log->GetQuestProgress(quest_id);
        if (!progress || !progress->IsComplete()) {
            continue;
        }
        
        const auto* quest_def = GetQuest(quest_id);
        if (quest_def && quest_def->end_npc_id == npc_id) {
            quest_log->CompleteQuest(quest_id);
        }
    }
}

// [SEQUENCE: 1361] QuestManager - Process quest chain
void QuestManager::ProcessQuestChain(uint64_t entity_id, uint32_t completed_quest_id) {
    const auto* completed_quest = GetQuest(completed_quest_id);
    if (!completed_quest || completed_quest->next_quest_id == 0) {
        return;
    }
    
    auto quest_log = GetQuestLog(entity_id);
    if (!quest_log) {
        return;
    }
    
    // Auto-accept next quest in chain
    if (quest_log->CanAcceptQuest(completed_quest->next_quest_id)) {
        quest_log->AcceptQuest(completed_quest->next_quest_id);
        
        spdlog::info("Entity {} auto-accepted chain quest {}", 
                     entity_id, completed_quest->next_quest_id);
    }
}

// [SEQUENCE: 1362] QuestManager - Reset daily quests
void QuestManager::ResetDailyQuests() {
    spdlog::info("Resetting daily quests...");
    
    // Get all daily quests
    auto daily_quests = GetQuestsByType(QuestType::DAILY);
    
    for (auto& [entity_id, quest_log] : quest_logs_) {
        // Remove active daily quests
        auto active_quests = quest_log->GetActiveQuests();
        for (uint32_t quest_id : active_quests) {
            const auto* quest_def = GetQuest(quest_id);
            if (quest_def && quest_def->type == QuestType::DAILY) {
                quest_log->AbandonQuest(quest_id);
            }
        }
        
        // Reset completion counts for daily quests
        // TODO: Implement daily reset tracking
    }
}

// [SEQUENCE: 1363] QuestManager - Reset weekly quests
void QuestManager::ResetWeeklyQuests() {
    spdlog::info("Resetting weekly quests...");
    
    // Similar to daily reset but for weekly quests
    auto weekly_quests = GetQuestsByType(QuestType::WEEKLY);
    
    for (auto& [entity_id, quest_log] : quest_logs_) {
        auto active_quests = quest_log->GetActiveQuests();
        for (uint32_t quest_id : active_quests) {
            const auto* quest_def = GetQuest(quest_id);
            if (quest_def && quest_def->type == QuestType::WEEKLY) {
                quest_log->AbandonQuest(quest_id);
            }
        }
    }
}

} // namespace mmorpg::game::quest