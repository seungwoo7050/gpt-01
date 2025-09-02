#include "dialogue_system.h"

namespace mmorpg::game::dialogue {

// [SEQUENCE: MVP7-411] DialogueManager implementation
DialogueManager& DialogueManager::Instance() {
    static DialogueManager instance;
    return instance;
}

void DialogueManager::RegisterDialogueTree(const DialogueTree& tree) {
    dialogue_trees_[tree.tree_id] = tree;
}

const DialogueNode* DialogueManager::StartDialogue(uint64_t player_id, uint32_t tree_id) {
    auto it = dialogue_trees_.find(tree_id);
    if (it == dialogue_trees_.end()) {
        return nullptr;
    }
    const auto& tree = it->second;
    player_dialogue_state_[player_id] = {tree_id, tree.start_node_id};
    return &tree.nodes.at(tree.start_node_id);
}

const DialogueNode* DialogueManager::MakeChoice(uint64_t player_id, uint32_t tree_id, uint32_t current_node_id, uint32_t choice_index) {
    auto player_state_it = player_dialogue_state_.find(player_id);
    if (player_state_it == player_dialogue_state_.end() || player_state_it->second.first != tree_id || player_state_it->second.second != current_node_id) {
        return nullptr; // Player not in this dialogue state
    }

    auto tree_it = dialogue_trees_.find(tree_id);
    if (tree_it == dialogue_trees_.end()) {
        return nullptr;
    }
    const auto& tree = tree_it->second;

    auto node_it = tree.nodes.find(current_node_id);
    if (node_it == tree.nodes.end()) {
        return nullptr;
    }
    const auto& node = node_it->second;

    if (choice_index >= node.choices.size()) {
        return nullptr;
    }

    const auto& choice = node.choices[choice_index];
    player_dialogue_state_[player_id] = {tree_id, choice.next_node_id};
    return &tree.nodes.at(choice.next_node_id);
}

}
