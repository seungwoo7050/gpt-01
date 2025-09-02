#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace mmorpg::game::dialogue {

// [SEQUENCE: MVP7-400] Dialogue system foundation

// [SEQUENCE: MVP7-401] Dialogue node type
enum class DialogueNodeType {
    STATEMENT,      // NPC 대사
    PLAYER_CHOICE,  // 플레이어 선택지
    CONDITION,      // 조건 분기
    ACTION,         // 액션 수행 (e.g., 퀘스트 시작)
    END             // 대화 종료
};

// [SEQUENCE: MVP7-402] Dialogue node condition
struct DialogueCondition {
    // ... (e.g., check quest state, player level, items)
    std::function<bool(uint64_t)> check;
};

// [SEQUENCE: MVP7-403] Dialogue node action
struct DialogueAction {
    // ... (e.g., start quest, give item, set flag)
    std::function<void(uint64_t)> execute;
};

// [SEQUENCE: MVP7-404] Player choice
struct PlayerChoice {
    std::string text;
    uint32_t next_node_id;
    DialogueCondition condition;
};

// [SEQUENCE: MVP7-405] Dialogue node
struct DialogueNode {
    uint32_t node_id;
    DialogueNodeType type;
    std::string text; // NPC 대사
    uint32_t npc_id;
    
    std::vector<PlayerChoice> choices;
    DialogueCondition condition;
    DialogueAction action;
    uint32_t next_node_id; // For STATEMENT, CONDITION, ACTION
};

// [SEQUENCE: MVP7-406] Dialogue tree
struct DialogueTree {
    uint32_t tree_id;
    std::string name;
    uint32_t start_node_id;
    std::unordered_map<uint32_t, DialogueNode> nodes;
};

// [SEQUENCE: MVP7-407] Dialogue manager
class DialogueManager {
public:
    static DialogueManager& Instance();
    
    // [SEQUENCE: MVP7-408] Register a dialogue tree
    void RegisterDialogueTree(const DialogueTree& tree);
    
    // [SEQUENCE: MVP7-409] Start a dialogue
    const DialogueNode* StartDialogue(uint64_t player_id, uint32_t tree_id);
    
    // [SEQUENCE: MVP7-410] Make a choice
    const DialogueNode* MakeChoice(uint64_t player_id, uint32_t tree_id, uint32_t current_node_id, uint32_t choice_index);

private:
    DialogueManager() = default;
    std::unordered_map<uint32_t, DialogueTree> dialogue_trees_;
    // Track player's current position in a dialogue
    std::unordered_map<uint64_t, std::pair<uint32_t, uint32_t>> player_dialogue_state_;
};

}
