#pragma once

#include "quest_system.h"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>
#include <algorithm>

namespace mmorpg::game::quest {

// [SEQUENCE: 1404] Quest chain and prerequisite system
// 퀘스트 연계, 전제 조건, 분기를 관리하는 시스템

// [SEQUENCE: 1405] Quest chain types
enum class ChainType {
    LINEAR,             // A -> B -> C
    BRANCHING,          // A -> (B or C) -> D
    PARALLEL,           // A -> (B and C) -> D
    CONDITIONAL,        // A -> B (if condition) or C (else)
    CYCLIC              // Repeatable chain
};

// [SEQUENCE: 1406] Quest node in chain
struct QuestChainNode {
    uint32_t quest_id;
    ChainType chain_type = ChainType::LINEAR;
    
    // Next quests in chain
    std::vector<uint32_t> next_quest_ids;
    
    // Conditions for branching
    std::function<bool(uint64_t)> branch_condition;
    
    // Requirements for this node
    std::vector<uint32_t> prerequisite_quest_ids;
    bool all_prerequisites_required = true;  // false = any prerequisite
    
    // Optional metadata
    std::string description;
    uint32_t recommended_level = 0;
};

// [SEQUENCE: 1407] Quest chain definition
struct QuestChain {
    uint32_t chain_id;
    std::string chain_name;
    std::string description;
    
    // Starting quest(s)
    std::vector<uint32_t> entry_quest_ids;
    
    // All nodes in chain
    std::unordered_map<uint32_t, QuestChainNode> nodes;
    
    // Chain properties
    bool is_main_story = false;
    bool is_repeatable = false;
    uint32_t cooldown_seconds = 0;
    
    // Rewards for completing entire chain
    QuestReward chain_completion_reward;
};

// [SEQUENCE: 1408] Quest dependency graph
class QuestDependencyGraph {
public:
    // [SEQUENCE: 1409] Add quest dependencies
    void AddQuest(uint32_t quest_id, const std::vector<uint32_t>& prerequisites) {
        dependencies_[quest_id] = prerequisites;
        
        // Build reverse dependencies
        for (uint32_t prereq : prerequisites) {
            dependents_[prereq].push_back(quest_id);
        }
    }
    
    // [SEQUENCE: 1410] Check if quest can be started
    bool CanStartQuest(uint32_t quest_id, const std::unordered_set<uint32_t>& completed) const {
        auto deps_it = dependencies_.find(quest_id);
        if (deps_it == dependencies_.end()) {
            return true;  // No dependencies
        }
        
        // Check all prerequisites
        for (uint32_t prereq : deps_it->second) {
            if (completed.find(prereq) == completed.end()) {
                return false;
            }
        }
        
        return true;
    }
    
    // [SEQUENCE: 1411] Get unlocked quests
    std::vector<uint32_t> GetUnlockedQuests(const std::unordered_set<uint32_t>& completed) const {
        std::vector<uint32_t> unlocked;
        
        for (const auto& [quest_id, prerequisites] : dependencies_) {
            if (completed.find(quest_id) != completed.end()) {
                continue;  // Already completed
            }
            
            if (CanStartQuest(quest_id, completed)) {
                unlocked.push_back(quest_id);
            }
        }
        
        return unlocked;
    }
    
    // [SEQUENCE: 1412] Get dependent quests
    std::vector<uint32_t> GetDependentQuests(uint32_t quest_id) const {
        auto deps_it = dependents_.find(quest_id);
        if (deps_it != dependents_.end()) {
            return deps_it->second;
        }
        return {};
    }
    
    // [SEQUENCE: 1413] Topological sort for quest order
    std::vector<uint32_t> GetQuestOrder() const {
        std::vector<uint32_t> result;
        std::unordered_map<uint32_t, int> in_degree;
        
        // Calculate in-degrees
        for (const auto& [quest, prereqs] : dependencies_) {
            if (in_degree.find(quest) == in_degree.end()) {
                in_degree[quest] = 0;
            }
            
            for (uint32_t prereq : prereqs) {
                in_degree[quest]++;
            }
        }
        
        // Find all nodes with no incoming edges
        std::queue<uint32_t> queue;
        for (const auto& [quest, degree] : in_degree) {
            if (degree == 0) {
                queue.push(quest);
            }
        }
        
        // Process nodes
        while (!queue.empty()) {
            uint32_t current = queue.front();
            queue.pop();
            result.push_back(current);
            
            // Reduce in-degree for dependent nodes
            auto deps_it = dependents_.find(current);
            if (deps_it != dependents_.end()) {
                for (uint32_t dependent : deps_it->second) {
                    in_degree[dependent]--;
                    if (in_degree[dependent] == 0) {
                        queue.push(dependent);
                    }
                }
            }
        }
        
        return result;
    }
    
private:
    // quest_id -> prerequisites
    std::unordered_map<uint32_t, std::vector<uint32_t>> dependencies_;
    
    // quest_id -> quests that depend on it
    std::unordered_map<uint32_t, std::vector<uint32_t>> dependents_;
};

// [SEQUENCE: 1414] Chain progress tracker
class ChainProgressTracker {
public:
    ChainProgressTracker(uint64_t entity_id) : entity_id_(entity_id) {}
    
    // [SEQUENCE: 1415] Start chain
    bool StartChain(uint32_t chain_id) {
        if (active_chains_.find(chain_id) != active_chains_.end()) {
            return false;  // Already active
        }
        
        ChainProgress progress;
        progress.chain_id = chain_id;
        progress.start_time = std::chrono::steady_clock::now();
        
        active_chains_[chain_id] = progress;
        
        spdlog::info("Entity {} started quest chain {}", entity_id_, chain_id);
        return true;
    }
    
    // [SEQUENCE: 1416] Update chain progress
    void UpdateChainProgress(uint32_t chain_id, uint32_t completed_quest_id) {
        auto it = active_chains_.find(chain_id);
        if (it == active_chains_.end()) {
            return;
        }
        
        it->second.completed_quests.insert(completed_quest_id);
        it->second.last_update = std::chrono::steady_clock::now();
        
        spdlog::debug("Chain {} progress: completed quest {}", chain_id, completed_quest_id);
    }
    
    // [SEQUENCE: 1417] Check chain completion
    bool IsChainComplete(uint32_t chain_id, const QuestChain& chain) const {
        auto it = active_chains_.find(chain_id);
        if (it == active_chains_.end()) {
            return false;
        }
        
        // Check if all required quests are completed
        for (const auto& [quest_id, node] : chain.nodes) {
            if (it->second.completed_quests.find(quest_id) == 
                it->second.completed_quests.end()) {
                // Check if this quest is optional
                // TODO: Add optional quest support
                return false;
            }
        }
        
        return true;
    }
    
    // [SEQUENCE: 1418] Get chain progress
    float GetChainProgress(uint32_t chain_id, const QuestChain& chain) const {
        auto it = active_chains_.find(chain_id);
        if (it == active_chains_.end()) {
            return 0.0f;
        }
        
        if (chain.nodes.empty()) {
            return 1.0f;
        }
        
        return static_cast<float>(it->second.completed_quests.size()) / chain.nodes.size();
    }
    
private:
    struct ChainProgress {
        uint32_t chain_id;
        std::unordered_set<uint32_t> completed_quests;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point last_update;
    };
    
    uint64_t entity_id_;
    std::unordered_map<uint32_t, ChainProgress> active_chains_;
};

// [SEQUENCE: 1419] Quest chain manager
class QuestChainManager {
public:
    static QuestChainManager& Instance() {
        static QuestChainManager instance;
        return instance;
    }
    
    // [SEQUENCE: 1420] Register chain
    void RegisterChain(const QuestChain& chain) {
        chains_[chain.chain_id] = chain;
        
        // Build dependency graph
        for (const auto& [quest_id, node] : chain.nodes) {
            dependency_graph_.AddQuest(quest_id, node.prerequisite_quest_ids);
        }
        
        spdlog::info("Registered quest chain: {} (ID: {})", chain.chain_name, chain.chain_id);
    }
    
    // [SEQUENCE: 1421] Get chain
    const QuestChain* GetChain(uint32_t chain_id) const {
        auto it = chains_.find(chain_id);
        return (it != chains_.end()) ? &it->second : nullptr;
    }
    
    // [SEQUENCE: 1422] Process quest completion
    void ProcessQuestCompletion(uint64_t entity_id, uint32_t quest_id) {
        // Find chains containing this quest
        for (const auto& [chain_id, chain] : chains_) {
            if (chain.nodes.find(quest_id) != chain.nodes.end()) {
                ProcessChainQuestCompletion(entity_id, chain_id, quest_id);
            }
        }
        
        // Check for newly unlocked quests
        auto quest_log = QuestManager::Instance().GetQuestLog(entity_id);
        if (!quest_log) {
            return;
        }
        
        std::unordered_set<uint32_t> completed;
        for (uint32_t completed_id : quest_log->GetCompletedQuests()) {
            completed.insert(completed_id);
        }
        
        auto unlocked = dependency_graph_.GetUnlockedQuests(completed);
        for (uint32_t unlocked_id : unlocked) {
            spdlog::debug("Quest {} unlocked for entity {}", unlocked_id, entity_id);
        }
    }
    
    // [SEQUENCE: 1423] Get progress tracker
    std::shared_ptr<ChainProgressTracker> GetProgressTracker(uint64_t entity_id) {
        auto it = progress_trackers_.find(entity_id);
        if (it == progress_trackers_.end()) {
            auto tracker = std::make_shared<ChainProgressTracker>(entity_id);
            progress_trackers_[entity_id] = tracker;
            return tracker;
        }
        return it->second;
    }
    
    // [SEQUENCE: 1424] Check prerequisites
    bool CheckPrerequisites(uint64_t entity_id, uint32_t quest_id) const {
        auto quest_log = QuestManager::Instance().GetQuestLog(entity_id);
        if (!quest_log) {
            return false;
        }
        
        std::unordered_set<uint32_t> completed;
        for (uint32_t completed_id : quest_log->GetCompletedQuests()) {
            completed.insert(completed_id);
        }
        
        return dependency_graph_.CanStartQuest(quest_id, completed);
    }
    
    // [SEQUENCE: 1425] Get recommended quest order
    std::vector<uint32_t> GetRecommendedQuestOrder() const {
        return dependency_graph_.GetQuestOrder();
    }
    
private:
    QuestChainManager() = default;
    
    // Quest chains
    std::unordered_map<uint32_t, QuestChain> chains_;
    
    // Global dependency graph
    QuestDependencyGraph dependency_graph_;
    
    // Progress trackers per entity
    std::unordered_map<uint64_t, std::shared_ptr<ChainProgressTracker>> progress_trackers_;
    
    // [SEQUENCE: 1426] Process chain quest completion
    void ProcessChainQuestCompletion(uint64_t entity_id, uint32_t chain_id, uint32_t quest_id) {
        auto tracker = GetProgressTracker(entity_id);
        tracker->UpdateChainProgress(chain_id, quest_id);
        
        const auto* chain = GetChain(chain_id);
        if (!chain) {
            return;
        }
        
        // Check for next quests in chain
        auto node_it = chain->nodes.find(quest_id);
        if (node_it != chain->nodes.end()) {
            const auto& node = node_it->second;
            
            // Handle different chain types
            switch (node.chain_type) {
                case ChainType::LINEAR:
                    if (!node.next_quest_ids.empty()) {
                        // Auto-start next quest
                        auto quest_log = QuestManager::Instance().GetQuestLog(entity_id);
                        if (quest_log && quest_log->CanAcceptQuest(node.next_quest_ids[0])) {
                            quest_log->AcceptQuest(node.next_quest_ids[0]);
                        }
                    }
                    break;
                    
                case ChainType::BRANCHING:
                    // Player must choose
                    spdlog::debug("Entity {} reached branching point in chain {}", 
                                 entity_id, chain_id);
                    break;
                    
                case ChainType::CONDITIONAL:
                    if (node.branch_condition && !node.next_quest_ids.empty()) {
                        uint32_t next_quest = node.branch_condition(entity_id) 
                            ? node.next_quest_ids[0] 
                            : (node.next_quest_ids.size() > 1 ? node.next_quest_ids[1] : 0);
                        
                        if (next_quest != 0) {
                            auto quest_log = QuestManager::Instance().GetQuestLog(entity_id);
                            if (quest_log && quest_log->CanAcceptQuest(next_quest)) {
                                quest_log->AcceptQuest(next_quest);
                            }
                        }
                    }
                    break;
            }
        }
        
        // Check chain completion
        if (tracker->IsChainComplete(chain_id, *chain)) {
            CompleteChain(entity_id, chain_id);
        }
    }
    
    // [SEQUENCE: 1427] Complete chain
    void CompleteChain(uint64_t entity_id, uint32_t chain_id) {
        const auto* chain = GetChain(chain_id);
        if (!chain) {
            return;
        }
        
        spdlog::info("Entity {} completed quest chain {}: {}", 
                    entity_id, chain_id, chain->chain_name);
        
        // TODO: Grant chain completion rewards
        // RewardManager::Instance().GrantRewards(entity_id, chain->chain_completion_reward);
    }
};

// [SEQUENCE: 1428] Quest chain builder
class QuestChainBuilder {
public:
    QuestChainBuilder& WithId(uint32_t id) {
        chain_.chain_id = id;
        return *this;
    }
    
    QuestChainBuilder& WithName(const std::string& name) {
        chain_.chain_name = name;
        return *this;
    }
    
    QuestChainBuilder& AddLinearQuest(uint32_t quest_id, uint32_t next_quest_id = 0) {
        QuestChainNode node;
        node.quest_id = quest_id;
        node.chain_type = ChainType::LINEAR;
        if (next_quest_id != 0) {
            node.next_quest_ids.push_back(next_quest_id);
        }
        
        chain_.nodes[quest_id] = node;
        
        if (chain_.entry_quest_ids.empty()) {
            chain_.entry_quest_ids.push_back(quest_id);
        }
        
        return *this;
    }
    
    QuestChainBuilder& AddBranchingQuest(uint32_t quest_id, 
                                         const std::vector<uint32_t>& branches) {
        QuestChainNode node;
        node.quest_id = quest_id;
        node.chain_type = ChainType::BRANCHING;
        node.next_quest_ids = branches;
        
        chain_.nodes[quest_id] = node;
        return *this;
    }
    
    QuestChain Build() {
        return chain_;
    }
    
private:
    QuestChain chain_;
};

} // namespace mmorpg::game::quest