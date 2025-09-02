#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <functional>
#include <optional>
#include <chrono>
#include "../core/types.h"
#include "../core/singleton.h"
#include "../player/player.h"
#include "npc.h"

namespace mmorpg::npc {

// [SEQUENCE: MVP14-1] Dialogue node types
enum class DialogueNodeType {
    TEXT,           // 단순 텍스트
    CHOICE,         // 선택지
    CONDITION,      // 조건부 분기
    ACTION,         // 액션 실행
    END             // 대화 종료
};

// [SEQUENCE: MVP14-2] Dialogue choice structure
struct DialogueChoice {
    uint32_t id;
    std::string text;
    std::string next_node_id;
    
    // Choice requirements
    struct Requirements {
        uint32_t min_level{0};
        uint32_t min_reputation{0};
        std::vector<uint32_t> required_items;
        std::vector<uint32_t> required_quests;
        std::vector<std::string> required_flags;
    } requirements;
    
    // Choice effects
    struct Effects {
        int32_t reputation_change{0};
        std::vector<std::pair<uint32_t, uint32_t>> give_items;  // item_id, quantity
        std::vector<std::pair<uint32_t, uint32_t>> take_items;  // item_id, quantity
        std::vector<uint32_t> start_quests;
        std::vector<uint32_t> complete_quests;
        std::vector<std::string> set_flags;
    } effects;
};

// [SEQUENCE: MVP14-3] Dialogue node
class DialogueNode {
public:
    DialogueNode(const std::string& id, DialogueNodeType type)
        : id_(id), type_(type) {}
    virtual ~DialogueNode() = default;
    
    // Node properties
    const std::string& GetId() const { return id_; }
    DialogueNodeType GetType() const { return type_; }
    
    // Node data
    void SetText(const std::string& text) { text_ = text; }
    const std::string& GetText() const { return text_; }
    
    void SetSpeaker(const std::string& speaker) { speaker_ = speaker; }
    const std::string& GetSpeaker() const { return speaker_; }
    
    // Choices (for CHOICE nodes)
    void AddChoice(const DialogueChoice& choice) { choices_.push_back(choice); }
    const std::vector<DialogueChoice>& GetChoices() const { return choices_; }
    
    // Next node (for linear progression)
    void SetNextNode(const std::string& node_id) { next_node_id_ = node_id; }
    const std::string& GetNextNode() const { return next_node_id_; }
    
    // Conditions (for CONDITION nodes)
    using ConditionFunc = std::function<bool(const Player&, const NPC&)>;
    void SetCondition(ConditionFunc condition) { condition_ = condition; }
    void SetTrueNode(const std::string& node_id) { true_node_id_ = node_id; }
    void SetFalseNode(const std::string& node_id) { false_node_id_ = node_id; }
    
    // Actions (for ACTION nodes)
    using ActionFunc = std::function<void(Player&, NPC&)>;
    void SetAction(ActionFunc action) { action_ = action; }
    
    // Node execution
    std::string Execute(Player& player, NPC& npc);
    
protected:
    std::string id_;
    DialogueNodeType type_;
    std::string text_;
    std::string speaker_;
    std::string next_node_id_;
    
    // Choice-specific
    std::vector<DialogueChoice> choices_;
    
    // Condition-specific
    ConditionFunc condition_;
    std::string true_node_id_;
    std::string false_node_id_;
    
    // Action-specific
    ActionFunc action_;
};

using DialogueNodePtr = std::shared_ptr<DialogueNode>;

// [SEQUENCE: MVP14-4] Dialogue tree
class DialogueTree {
public:
    DialogueTree(const std::string& id) : id_(id) {}
    
    // Tree properties
    const std::string& GetId() const { return id_; }
    void SetName(const std::string& name) { name_ = name; }
    const std::string& GetName() const { return name_; }
    
    // Node management
    void AddNode(DialogueNodePtr node);
    DialogueNodePtr GetNode(const std::string& node_id);
    void SetStartNode(const std::string& node_id) { start_node_id_ = node_id; }
    
    // Tree validation
    bool Validate(std::vector<std::string>& errors) const;
    
    // Debug
    std::string DebugPrint() const;
    
private:
    std::string id_;
    std::string name_;
    std::string start_node_id_{"start"};
    std::unordered_map<std::string, DialogueNodePtr> nodes_;
};

using DialogueTreePtr = std::shared_ptr<DialogueTree>;

// [SEQUENCE: MVP14-5] Dialogue state tracking
class DialogueState {
public:
    DialogueState(uint64_t player_id, uint64_t npc_id, DialogueTreePtr tree)
        : player_id_(player_id)
        , npc_id_(npc_id)
        , dialogue_tree_(tree)
        , start_time_(std::chrono::steady_clock::now()) {}
    
    // State properties
    uint64_t GetPlayerId() const { return player_id_; }
    uint64_t GetNpcId() const { return npc_id_; }
    DialogueTreePtr GetTree() const { return dialogue_tree_; }
    
    // Current node
    void SetCurrentNode(const std::string& node_id) { current_node_id_ = node_id; }
    const std::string& GetCurrentNode() const { return current_node_id_; }
    
    // History tracking
    void AddToHistory(const std::string& node_id, uint32_t choice_id = 0);
    const std::vector<std::pair<std::string, uint32_t>>& GetHistory() const { 
        return history_; 
    }
    
    // State flags
    void SetFlag(const std::string& flag) { flags_.insert(flag); }
    bool HasFlag(const std::string& flag) const {
        return flags_.find(flag) != flags_.end();
    }
    
    // Timing
    std::chrono::steady_clock::time_point GetStartTime() const { return start_time_; }
    float GetDuration() const;
    
private:
    uint64_t player_id_;
    uint64_t npc_id_;
    DialogueTreePtr dialogue_tree_;
    std::string current_node_id_{"start"};
    
    std::vector<std::pair<std::string, uint32_t>> history_;  // node_id, choice_id
    std::unordered_set<std::string> flags_;
    std::chrono::steady_clock::time_point start_time_;
};

using DialogueStatePtr = std::shared_ptr<DialogueState>;

// [SEQUENCE: MVP14-6] Dialogue manager
class DialogueManager : public Singleton<DialogueManager> {
public:
    // [SEQUENCE: MVP14-7] Dialogue tree management
    void RegisterDialogueTree(DialogueTreePtr tree);
    DialogueTreePtr GetDialogueTree(const std::string& tree_id);
    
    // [SEQUENCE: MVP14-8] Dialogue session management
    DialogueStatePtr StartDialogue(Player& player, NPC& npc, 
                                  const std::string& tree_id);
    void EndDialogue(uint64_t player_id);
    DialogueStatePtr GetActiveDialogue(uint64_t player_id);
    
    // [SEQUENCE: MVP14-9] Dialogue progression
    struct DialogueResponse {
        std::string text;
        std::string speaker;
        std::vector<DialogueChoice> available_choices;
        bool is_end{false};
    };
    
    DialogueResponse ContinueDialogue(uint64_t player_id);
    DialogueResponse MakeChoice(uint64_t player_id, uint32_t choice_id);
    
    // [SEQUENCE: MVP14-10] Dialogue conditions
    using GlobalConditionFunc = std::function<bool(const Player&, const NPC&)>;
    void RegisterGlobalCondition(const std::string& name, 
                                GlobalConditionFunc condition);
    bool CheckCondition(const std::string& name, 
                       const Player& player, const NPC& npc);
    
    // [SEQUENCE: MVP14-11] Dialogue actions
    using GlobalActionFunc = std::function<void(Player&, NPC&)>;
    void RegisterGlobalAction(const std::string& name, GlobalActionFunc action);
    void ExecuteAction(const std::string& name, Player& player, NPC& npc);
    
    // Statistics
    struct DialogueStats {
        uint32_t total_dialogues{0};
        uint32_t completed_dialogues{0};
        uint32_t abandoned_dialogues{0};
        float average_duration{0.0f};
        std::unordered_map<std::string, uint32_t> popular_trees;
        std::unordered_map<uint32_t, uint32_t> choice_frequency;
    };
    
    DialogueStats GetStats() const { return stats_; }
    
private:
    friend class Singleton<DialogueManager>;
    DialogueManager() = default;
    
    // Dialogue trees
    std::unordered_map<std::string, DialogueTreePtr> dialogue_trees_;
    
    // Active dialogues
    std::unordered_map<uint64_t, DialogueStatePtr> active_dialogues_;
    
    // Global conditions and actions
    std::unordered_map<std::string, GlobalConditionFunc> global_conditions_;
    std::unordered_map<std::string, GlobalActionFunc> global_actions_;
    
    // Statistics
    DialogueStats stats_;
    mutable std::shared_mutex mutex_;
    
    // Helper methods
    bool CheckChoiceRequirements(const DialogueChoice& choice, 
                                const Player& player);
    void ApplyChoiceEffects(const DialogueChoice& choice, 
                          Player& player, NPC& npc);
};

// [SEQUENCE: MVP14-12] Dialogue builder
class DialogueBuilder {
public:
    DialogueBuilder(const std::string& tree_id) {
        tree_ = std::make_shared<DialogueTree>(tree_id);
    }
    
    // Tree properties
    DialogueBuilder& Name(const std::string& name);
    
    // Node creation
    DialogueBuilder& Text(const std::string& node_id, 
                         const std::string& speaker,
                         const std::string& text,
                         const std::string& next_node = "");
    
    DialogueBuilder& Choice(const std::string& node_id,
                           const std::string& speaker,
                           const std::string& text);
    
    DialogueBuilder& AddOption(uint32_t choice_id,
                              const std::string& text,
                              const std::string& next_node);
    
    DialogueBuilder& Condition(const std::string& node_id,
                              const std::string& condition_name,
                              const std::string& true_node,
                              const std::string& false_node);
    
    DialogueBuilder& Action(const std::string& node_id,
                           const std::string& action_name,
                           const std::string& next_node = "");
    
    DialogueBuilder& End(const std::string& node_id,
                        const std::string& text = "");
    
    // Requirements and effects
    DialogueBuilder& RequireLevel(uint32_t choice_id, uint32_t min_level);
    DialogueBuilder& RequireItem(uint32_t choice_id, uint32_t item_id);
    DialogueBuilder& RequireQuest(uint32_t choice_id, uint32_t quest_id);
    DialogueBuilder& RequireFlag(uint32_t choice_id, const std::string& flag);
    
    DialogueBuilder& GiveItem(uint32_t choice_id, uint32_t item_id, 
                             uint32_t quantity = 1);
    DialogueBuilder& TakeItem(uint32_t choice_id, uint32_t item_id, 
                             uint32_t quantity = 1);
    DialogueBuilder& StartQuest(uint32_t choice_id, uint32_t quest_id);
    DialogueBuilder& CompleteQuest(uint32_t choice_id, uint32_t quest_id);
    DialogueBuilder& SetFlag(uint32_t choice_id, const std::string& flag);
    DialogueBuilder& ChangeReputation(uint32_t choice_id, int32_t amount);
    
    // Build
    DialogueTreePtr Build();
    
private:
    DialogueTreePtr tree_;
    DialogueNodePtr current_node_;
    std::unordered_map<uint32_t, DialogueChoice*> choice_map_;
};

// [SEQUENCE: MVP14-13] Common dialogue patterns
namespace DialoguePatterns {
    // Merchant dialogue
    DialogueTreePtr CreateMerchantDialogue(const std::string& merchant_name,
                                          const std::vector<uint32_t>& items);
    
    // Quest giver dialogue
    DialogueTreePtr CreateQuestDialogue(const std::string& npc_name,
                                       uint32_t quest_id,
                                       const std::string& quest_intro,
                                       const std::string& quest_accept,
                                       const std::string& quest_decline);
    
    // Guard dialogue
    DialogueTreePtr CreateGuardDialogue(const std::string& location_name,
                                       uint32_t required_pass_item = 0);
    
    // Innkeeper dialogue
    DialogueTreePtr CreateInnkeeperDialogue(uint32_t room_cost,
                                           const std::string& inn_name);
    
    // Trainer dialogue
    DialogueTreePtr CreateTrainerDialogue(const std::string& trainer_name,
                                         const std::string& skill_type,
                                         const std::vector<uint32_t>& skills);
}

// [SEQUENCE: MVP14-14] Dialogue utilities
namespace DialogueUtils {
    // Text formatting
    std::string FormatDialogueText(const std::string& text, 
                                  const Player& player,
                                  const NPC& npc);
    
    // Localization
    std::string LocalizeDialogue(const std::string& key, 
                                const std::string& language = "en");
    
    // Validation
    bool ValidateDialogueTree(DialogueTreePtr tree,
                            std::vector<std::string>& errors);
    
    // Export/Import
    std::string ExportDialogueToXML(DialogueTreePtr tree);
    DialogueTreePtr ImportDialogueFromXML(const std::string& xml_data);
}

} // namespace mmorpg::npc