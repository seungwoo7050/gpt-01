#pragma once

#include "quest_system.h"
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <queue>
#include <algorithm>

namespace mmorpg::game::quest {

// [SEQUENCE: MVP7-309] Quest chain and prerequisite system

// [SEQUENCE: MVP7-310] Quest chain types
enum class ChainType {
    LINEAR,
    BRANCHING,
    PARALLEL,
    CONDITIONAL,
    CYCLIC
};

// [SEQUENCE: MVP7-311] Quest node in chain
struct QuestChainNode {
    uint32_t quest_id;
    ChainType chain_type = ChainType::LINEAR;
    std::vector<uint32_t> next_quest_ids;
    std::function<bool(uint64_t)> branch_condition;
    std::vector<uint32_t> prerequisite_quest_ids;
    bool all_prerequisites_required = true;
    std::string description;
    uint32_t recommended_level = 0;
};

// [SEQUENCE: MVP7-312] Quest chain definition
struct QuestChain {
    uint32_t chain_id;
    std::string chain_name;
    std::string description;
    std::vector<uint32_t> entry_quest_ids;
    std::unordered_map<uint32_t, QuestChainNode> nodes;
    bool is_main_story = false;
    bool is_repeatable = false;
    uint32_t cooldown_seconds = 0;
    QuestReward chain_completion_reward;
};

// [SEQUENCE: MVP7-313] Quest dependency graph
class QuestDependencyGraph {
public:
    // [SEQUENCE: MVP7-314] Add quest dependencies
    void AddQuest(uint32_t quest_id, const std::vector<uint32_t>& prerequisites);
    
    // [SEQUENCE: MVP7-315] Check if quest can be started
    bool CanStartQuest(uint32_t quest_id, const std::unordered_set<uint32_t>& completed) const;
    
    // [SEQUENCE: MVP7-316] Get unlocked quests
    std::vector<uint32_t> GetUnlockedQuests(const std::unordered_set<uint32_t>& completed) const;
    
    // [SEQUENCE: MVP7-317] Get dependent quests
    std::vector<uint32_t> GetDependentQuests(uint32_t quest_id) const;
    
    // [SEQUENCE: MVP7-318] Topological sort for quest order
    std::vector<uint32_t> GetQuestOrder() const;
    
private:
    std::unordered_map<uint32_t, std::vector<uint32_t>> dependencies_;
    std::unordered_map<uint32_t, std::vector<uint32_t>> dependents_;
};

// [SEQUENCE: MVP7-319] Chain progress tracker
class ChainProgressTracker {
public:
    ChainProgressTracker(uint64_t entity_id) : entity_id_(entity_id) {}
    
    // [SEQUENCE: MVP7-320] Start chain
    bool StartChain(uint32_t chain_id);
    
    // [SEQUENCE: MVP7-321] Update chain progress
    void UpdateChainProgress(uint32_t chain_id, uint32_t completed_quest_id);
    
    // [SEQUENCE: MVP7-322] Check chain completion
    bool IsChainComplete(uint32_t chain_id, const QuestChain& chain) const;
    
    // [SEQUENCE: MVP7-323] Get chain progress
    float GetChainProgress(uint32_t chain_id, const QuestChain& chain) const;
    
private:
    struct ChainProgress;
    uint64_t entity_id_;
    std::unordered_map<uint32_t, ChainProgress> active_chains_;
};

// [SEQUENCE: MVP7-324] Quest chain manager
class QuestChainManager {
public:
    static QuestChainManager& Instance();
    
    // [SEQUENCE: MVP7-325] Register chain
    void RegisterChain(const QuestChain& chain);
    
    // [SEQUENCE: MVP7-326] Get chain
    const QuestChain* GetChain(uint32_t chain_id) const;
    
    // [SEQUENCE: MVP7-327] Process quest completion
    void ProcessQuestCompletion(uint64_t entity_id, uint32_t quest_id);
    
    // [SEQUENCE: MVP7-328] Get progress tracker
    std::shared_ptr<ChainProgressTracker> GetProgressTracker(uint64_t entity_id);
    
    // [SEQUENCE: MVP7-329] Check prerequisites
    bool CheckPrerequisites(uint64_t entity_id, uint32_t quest_id) const;
    
    // [SEQUENCE: MVP7-330] Get recommended quest order
    std::vector<uint32_t> GetRecommendedQuestOrder() const;
    
private:
    QuestChainManager() = default;
    std::unordered_map<uint32_t, QuestChain> chains_;
    QuestDependencyGraph dependency_graph_;
    std::unordered_map<uint64_t, std::shared_ptr<ChainProgressTracker>> progress_trackers_;
    
    // [SEQUENCE: MVP7-331] Process chain quest completion
    void ProcessChainQuestCompletion(uint64_t entity_id, uint32_t chain_id, uint32_t quest_id);
    
    // [SEQUENCE: MVP7-332] Complete chain
    void CompleteChain(uint64_t entity_id, uint32_t chain_id);
};

// [SEQUENCE: MVP7-333] Quest chain builder
class QuestChainBuilder {
public:
    QuestChainBuilder& WithId(uint32_t id);
    QuestChainBuilder& WithName(const std::string& name);
    QuestChainBuilder& AddLinearQuest(uint32_t quest_id, uint32_t next_quest_id = 0);
    QuestChainBuilder& AddBranchingQuest(uint32_t quest_id, const std::vector<uint32_t>& branches);
    QuestChain Build();
private:
    QuestChain chain_;
};

} // namespace mmorpg::game::quest
