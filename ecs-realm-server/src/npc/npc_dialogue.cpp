#include "npc_dialogue.h"
#include "../core/logger.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <sstream>

namespace mmorpg::npc {

// [SEQUENCE: MVP14-15] Dialogue node execution
std::string DialogueNode::Execute(Player& player, NPC& npc) {
    switch (type_) {
        case DialogueNodeType::TEXT:
            // Simple text node, return next node
            return next_node_id_;
            
        case DialogueNodeType::CHOICE:
            // Choice node, wait for player input
            return id_;  // Stay on this node
            
        case DialogueNodeType::CONDITION:
            // Evaluate condition
            if (condition_ && condition_(player, npc)) {
                return true_node_id_;
            } else {
                return false_node_id_;
            }
            
        case DialogueNodeType::ACTION:
            // Execute action
            if (action_) {
                action_(player, npc);
            }
            return next_node_id_;
            
        case DialogueNodeType::END:
            // End of dialogue
            return "";
            
        default:
            return "";
    }
}

// [SEQUENCE: MVP14-16] Dialogue tree node management
void DialogueTree::AddNode(DialogueNodePtr node) {
    if (node) {
        nodes_[node->GetId()] = node;
    }
}

DialogueNodePtr DialogueTree::GetNode(const std::string& node_id) {
    auto it = nodes_.find(node_id);
    return (it != nodes_.end()) ? it->second : nullptr;
}

bool DialogueTree::Validate(std::vector<std::string>& errors) const {
    bool valid = true;
    
    // Check start node exists
    if (nodes_.find(start_node_id_) == nodes_.end()) {
        errors.push_back("Start node '" + start_node_id_ + "' not found");
        valid = false;
    }
    
    // Validate all nodes
    for (const auto& [node_id, node] : nodes_) {
        // Check next nodes exist
        if (!node->GetNextNode().empty() && 
            nodes_.find(node->GetNextNode()) == nodes_.end()) {
            errors.push_back("Node '" + node_id + "' references missing node '" + 
                           node->GetNextNode() + "'");
            valid = false;
        }
        
        // Check choice targets
        if (node->GetType() == DialogueNodeType::CHOICE) {
            for (const auto& choice : node->GetChoices()) {
                if (nodes_.find(choice.next_node_id) == nodes_.end()) {
                    errors.push_back("Choice in node '" + node_id + 
                                   "' references missing node '" + 
                                   choice.next_node_id + "'");
                    valid = false;
                }
            }
        }
    }
    
    return valid;
}

std::string DialogueTree::DebugPrint() const {
    std::stringstream ss;
    ss << "DialogueTree: " << name_ << " (" << id_ << ")\n";
    ss << "Start Node: " << start_node_id_ << "\n";
    ss << "Nodes:\n";
    
    for (const auto& [node_id, node] : nodes_) {
        ss << "  - " << node_id << " [" << static_cast<int>(node->GetType()) << "]";
        if (!node->GetText().empty()) {
            ss << ": " << node->GetText().substr(0, 50) << "...";
        }
        ss << "\n";
    }
    
    return ss.str();
}

// [SEQUENCE: MVP14-17] Dialogue state tracking
void DialogueState::AddToHistory(const std::string& node_id, uint32_t choice_id) {
    history_.emplace_back(node_id, choice_id);
}

float DialogueState::GetDuration() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
    return static_cast<float>(duration.count());
}

// [SEQUENCE: MVP14-18] Dialogue manager - tree management
void DialogueManager::RegisterDialogueTree(DialogueTreePtr tree) {
    if (tree) {
        std::unique_lock lock(mutex_);
        dialogue_trees_[tree->GetId()] = tree;
        spdlog::info("[Dialogue] Registered tree: {} ({})", 
                    tree->GetName(), tree->GetId());
    }
}

DialogueTreePtr DialogueManager::GetDialogueTree(const std::string& tree_id) {
    std::shared_lock lock(mutex_);
    auto it = dialogue_trees_.find(tree_id);
    return (it != dialogue_trees_.end()) ? it->second : nullptr;
}

// [SEQUENCE: MVP14-19] Dialogue session management
DialogueStatePtr DialogueManager::StartDialogue(Player& player, NPC& npc,
                                              const std::string& tree_id) {
    auto tree = GetDialogueTree(tree_id);
    if (!tree) {
        spdlog::error("[Dialogue] Tree not found: {}", tree_id);
        return nullptr;
    }
    
    // End any existing dialogue
    EndDialogue(player.GetId());
    
    // Create new dialogue state
    auto state = std::make_shared<DialogueState>(player.GetId(), npc.GetId(), tree);
    
    std::unique_lock lock(mutex_);
    active_dialogues_[player.GetId()] = state;
    stats_.total_dialogues++;
    
    spdlog::debug("[Dialogue] Started dialogue: player={}, npc={}, tree={}",
                 player.GetId(), npc.GetId(), tree_id);
    
    return state;
}

void DialogueManager::EndDialogue(uint64_t player_id) {
    std::unique_lock lock(mutex_);
    auto it = active_dialogues_.find(player_id);
    if (it != active_dialogues_.end()) {
        auto state = it->second;
        
        // Update statistics
        if (state->GetCurrentNode() == "" || 
            state->GetTree()->GetNode(state->GetCurrentNode())->GetType() == 
            DialogueNodeType::END) {
            stats_.completed_dialogues++;
        } else {
            stats_.abandoned_dialogues++;
        }
        
        float duration = state->GetDuration();
        stats_.average_duration = 
            (stats_.average_duration * (stats_.total_dialogues - 1) + duration) / 
            stats_.total_dialogues;
        
        active_dialogues_.erase(it);
        
        spdlog::debug("[Dialogue] Ended dialogue for player {}", player_id);
    }
}

DialogueStatePtr DialogueManager::GetActiveDialogue(uint64_t player_id) {
    std::shared_lock lock(mutex_);
    auto it = active_dialogues_.find(player_id);
    return (it != active_dialogues_.end()) ? it->second : nullptr;
}

// [SEQUENCE: MVP14-20] Dialogue progression
DialogueManager::DialogueResponse DialogueManager::ContinueDialogue(
    uint64_t player_id) {
    
    auto state = GetActiveDialogue(player_id);
    if (!state) {
        return {"No active dialogue", "", {}, true};
    }
    
    auto tree = state->GetTree();
    auto node = tree->GetNode(state->GetCurrentNode());
    if (!node) {
        EndDialogue(player_id);
        return {"Dialogue error", "", {}, true};
    }
    
    DialogueResponse response;
    response.text = node->GetText();
    response.speaker = node->GetSpeaker();
    
    // Get player and NPC
    auto player = Player::GetPlayer(player_id);
    auto npc = NPC::GetNPC(state->GetNpcId());
    
    if (!player || !npc) {
        EndDialogue(player_id);
        return {"Invalid player or NPC", "", {}, true};
    }
    
    // Handle different node types
    switch (node->GetType()) {
        case DialogueNodeType::TEXT:
        case DialogueNodeType::ACTION:
        case DialogueNodeType::CONDITION: {
            // Execute node and progress
            std::string next_id = node->Execute(*player, *npc);
            if (next_id.empty()) {
                // End of dialogue
                response.is_end = true;
                EndDialogue(player_id);
            } else {
                state->SetCurrentNode(next_id);
                state->AddToHistory(node->GetId());
                
                // If next node is also automatic, continue
                auto next_node = tree->GetNode(next_id);
                if (next_node && (next_node->GetType() == DialogueNodeType::ACTION ||
                                 next_node->GetType() == DialogueNodeType::CONDITION)) {
                    return ContinueDialogue(player_id);
                }
            }
            break;
        }
        
        case DialogueNodeType::CHOICE: {
            // Filter available choices based on requirements
            for (const auto& choice : node->GetChoices()) {
                if (CheckChoiceRequirements(choice, *player)) {
                    response.available_choices.push_back(choice);
                }
            }
            break;
        }
        
        case DialogueNodeType::END:
            response.is_end = true;
            EndDialogue(player_id);
            break;
    }
    
    return response;
}

DialogueManager::DialogueResponse DialogueManager::MakeChoice(
    uint64_t player_id, uint32_t choice_id) {
    
    auto state = GetActiveDialogue(player_id);
    if (!state) {
        return {"No active dialogue", "", {}, true};
    }
    
    auto tree = state->GetTree();
    auto node = tree->GetNode(state->GetCurrentNode());
    if (!node || node->GetType() != DialogueNodeType::CHOICE) {
        return {"Invalid dialogue state", "", {}, true};
    }
    
    // Find the chosen option
    const DialogueChoice* selected_choice = nullptr;
    for (const auto& choice : node->GetChoices()) {
        if (choice.id == choice_id) {
            selected_choice = &choice;
            break;
        }
    }
    
    if (!selected_choice) {
        return {"Invalid choice", "", {}, false};
    }
    
    // Get player and NPC
    auto player = Player::GetPlayer(player_id);
    auto npc = NPC::GetNPC(state->GetNpcId());
    
    if (!player || !npc) {
        EndDialogue(player_id);
        return {"Invalid player or NPC", "", {}, true};
    }
    
    // Check requirements again
    if (!CheckChoiceRequirements(*selected_choice, *player)) {
        return {"Requirements not met", "", {}, false};
    }
    
    // Apply choice effects
    ApplyChoiceEffects(*selected_choice, *player, *npc);
    
    // Record choice in history
    state->AddToHistory(node->GetId(), choice_id);
    
    // Update statistics
    {
        std::unique_lock lock(mutex_);
        stats_.choice_frequency[choice_id]++;
    }
    
    // Move to next node
    state->SetCurrentNode(selected_choice->next_node_id);
    
    // Continue dialogue
    return ContinueDialogue(player_id);
}

// [SEQUENCE: MVP14-21] Choice requirements and effects
bool DialogueManager::CheckChoiceRequirements(const DialogueChoice& choice,
                                            const Player& player) {
    const auto& req = choice.requirements;
    
    // Check level
    if (player.GetLevel() < req.min_level) {
        return false;
    }
    
    // Check reputation
    if (player.GetReputation() < req.min_reputation) {
        return false;
    }
    
    // Check items
    for (uint32_t item_id : req.required_items) {
        if (!player.HasItem(item_id)) {
            return false;
        }
    }
    
    // Check quests
    for (uint32_t quest_id : req.required_quests) {
        if (!player.HasCompletedQuest(quest_id)) {
            return false;
        }
    }
    
    // Check flags
    auto state = GetActiveDialogue(player.GetId());
    if (state) {
        for (const auto& flag : req.required_flags) {
            if (!state->HasFlag(flag)) {
                return false;
            }
        }
    }
    
    return true;
}

void DialogueManager::ApplyChoiceEffects(const DialogueChoice& choice,
                                       Player& player, NPC& npc) {
    const auto& effects = choice.effects;
    
    // Change reputation
    if (effects.reputation_change != 0) {
        player.ChangeReputation(effects.reputation_change);
    }
    
    // Give items
    for (const auto& [item_id, quantity] : effects.give_items) {
        player.GiveItem(item_id, quantity);
    }
    
    // Take items
    for (const auto& [item_id, quantity] : effects.take_items) {
        player.RemoveItem(item_id, quantity);
    }
    
    // Start quests
    for (uint32_t quest_id : effects.start_quests) {
        player.StartQuest(quest_id);
    }
    
    // Complete quests
    for (uint32_t quest_id : effects.complete_quests) {
        player.CompleteQuest(quest_id);
    }
    
    // Set flags
    auto state = GetActiveDialogue(player.GetId());
    if (state) {
        for (const auto& flag : effects.set_flags) {
            state->SetFlag(flag);
        }
    }
}

// [SEQUENCE: MVP14-22] Global conditions and actions
void DialogueManager::RegisterGlobalCondition(const std::string& name,
                                            GlobalConditionFunc condition) {
    std::unique_lock lock(mutex_);
    global_conditions_[name] = condition;
    spdlog::debug("[Dialogue] Registered global condition: {}", name);
}

bool DialogueManager::CheckCondition(const std::string& name,
                                   const Player& player, const NPC& npc) {
    std::shared_lock lock(mutex_);
    auto it = global_conditions_.find(name);
    if (it != global_conditions_.end() && it->second) {
        return it->second(player, npc);
    }
    return false;
}

void DialogueManager::RegisterGlobalAction(const std::string& name,
                                         GlobalActionFunc action) {
    std::unique_lock lock(mutex_);
    global_actions_[name] = action;
    spdlog::debug("[Dialogue] Registered global action: {}", name);
}

void DialogueManager::ExecuteAction(const std::string& name,
                                  Player& player, NPC& npc) {
    std::shared_lock lock(mutex_);
    auto it = global_actions_.find(name);
    if (it != global_actions_.end() && it->second) {
        it->second(player, npc);
    }
}

// [SEQUENCE: MVP14-23] Dialogue builder implementation
DialogueBuilder& DialogueBuilder::Name(const std::string& name) {
    tree_->SetName(name);
    return *this;
}

DialogueBuilder& DialogueBuilder::Text(const std::string& node_id,
                                      const std::string& speaker,
                                      const std::string& text,
                                      const std::string& next_node) {
    auto node = std::make_shared<DialogueNode>(node_id, DialogueNodeType::TEXT);
    node->SetSpeaker(speaker);
    node->SetText(text);
    node->SetNextNode(next_node);
    tree_->AddNode(node);
    current_node_ = node;
    return *this;
}

DialogueBuilder& DialogueBuilder::Choice(const std::string& node_id,
                                        const std::string& speaker,
                                        const std::string& text) {
    auto node = std::make_shared<DialogueNode>(node_id, DialogueNodeType::CHOICE);
    node->SetSpeaker(speaker);
    node->SetText(text);
    tree_->AddNode(node);
    current_node_ = node;
    choice_map_.clear();
    return *this;
}

DialogueBuilder& DialogueBuilder::AddOption(uint32_t choice_id,
                                           const std::string& text,
                                           const std::string& next_node) {
    if (current_node_ && current_node_->GetType() == DialogueNodeType::CHOICE) {
        DialogueChoice choice;
        choice.id = choice_id;
        choice.text = text;
        choice.next_node_id = next_node;
        current_node_->AddChoice(choice);
        
        // Store pointer for later modification
        auto& choices = const_cast<std::vector<DialogueChoice>&>(
            current_node_->GetChoices());
        choice_map_[choice_id] = &choices.back();
    }
    return *this;
}

DialogueBuilder& DialogueBuilder::Condition(const std::string& node_id,
                                           const std::string& condition_name,
                                           const std::string& true_node,
                                           const std::string& false_node) {
    auto node = std::make_shared<DialogueNode>(node_id, DialogueNodeType::CONDITION);
    
    // Set condition using global condition
    node->SetCondition([condition_name](const Player& player, const NPC& npc) {
        return DialogueManager::Instance().CheckCondition(condition_name, player, npc);
    });
    
    node->SetTrueNode(true_node);
    node->SetFalseNode(false_node);
    tree_->AddNode(node);
    current_node_ = node;
    return *this;
}

DialogueBuilder& DialogueBuilder::Action(const std::string& node_id,
                                        const std::string& action_name,
                                        const std::string& next_node) {
    auto node = std::make_shared<DialogueNode>(node_id, DialogueNodeType::ACTION);
    
    // Set action using global action
    node->SetAction([action_name](Player& player, NPC& npc) {
        DialogueManager::Instance().ExecuteAction(action_name, player, npc);
    });
    
    node->SetNextNode(next_node);
    tree_->AddNode(node);
    current_node_ = node;
    return *this;
}

DialogueBuilder& DialogueBuilder::End(const std::string& node_id,
                                     const std::string& text) {
    auto node = std::make_shared<DialogueNode>(node_id, DialogueNodeType::END);
    if (!text.empty()) {
        node->SetText(text);
    }
    tree_->AddNode(node);
    current_node_ = node;
    return *this;
}

// [SEQUENCE: MVP14-24] Requirements and effects
DialogueBuilder& DialogueBuilder::RequireLevel(uint32_t choice_id, uint32_t min_level) {
    if (auto it = choice_map_.find(choice_id); it != choice_map_.end()) {
        it->second->requirements.min_level = min_level;
    }
    return *this;
}

DialogueBuilder& DialogueBuilder::RequireItem(uint32_t choice_id, uint32_t item_id) {
    if (auto it = choice_map_.find(choice_id); it != choice_map_.end()) {
        it->second->requirements.required_items.push_back(item_id);
    }
    return *this;
}

DialogueBuilder& DialogueBuilder::GiveItem(uint32_t choice_id, uint32_t item_id,
                                          uint32_t quantity) {
    if (auto it = choice_map_.find(choice_id); it != choice_map_.end()) {
        it->second->effects.give_items.emplace_back(item_id, quantity);
    }
    return *this;
}

DialogueBuilder& DialogueBuilder::StartQuest(uint32_t choice_id, uint32_t quest_id) {
    if (auto it = choice_map_.find(choice_id); it != choice_map_.end()) {
        it->second->effects.start_quests.push_back(quest_id);
    }
    return *this;
}

DialogueTreePtr DialogueBuilder::Build() {
    // Validate tree
    std::vector<std::string> errors;
    if (!tree_->Validate(errors)) {
        for (const auto& error : errors) {
            spdlog::error("[Dialogue] Build error: {}", error);
        }
    }
    
    return tree_;
}

// [SEQUENCE: MVP14-25] Common dialogue patterns
namespace DialoguePatterns {

DialogueTreePtr CreateMerchantDialogue(const std::string& merchant_name,
                                      const std::vector<uint32_t>& items) {
    DialogueBuilder builder("merchant_" + merchant_name);
    
    builder.Name(merchant_name + " Dialogue")
        .Text("start", merchant_name, 
              "Welcome to my shop! What can I do for you?", "main_menu")
        .Choice("main_menu", merchant_name, "How can I help you?")
            .AddOption(1, "I'd like to see your wares.", "show_items")
            .AddOption(2, "Tell me about this place.", "about_place")
            .AddOption(3, "Goodbye.", "farewell")
        .Text("show_items", merchant_name,
              "Here's what I have for sale today.", "trade_action")
        .Action("trade_action", "open_merchant_ui", "main_menu")
        .Text("about_place", merchant_name,
              "This is a fine establishment! We've been here for generations.",
              "main_menu")
        .End("farewell", "Come back anytime!");
    
    return builder.Build();
}

DialogueTreePtr CreateQuestDialogue(const std::string& npc_name,
                                  uint32_t quest_id,
                                  const std::string& quest_intro,
                                  const std::string& quest_accept,
                                  const std::string& quest_decline) {
    DialogueBuilder builder("quest_" + std::to_string(quest_id));
    
    builder.Name(npc_name + " Quest Dialogue")
        .Condition("start", "has_quest_" + std::to_string(quest_id),
                  "already_has", "check_completed")
        .Text("already_has", npc_name,
              "You're already working on this task. How's it going?", "end")
        .Condition("check_completed", "completed_quest_" + std::to_string(quest_id),
                  "already_completed", "offer_quest")
        .Text("already_completed", npc_name,
              "You've already helped me with this. Thank you!", "end")
        .Text("offer_quest", npc_name, quest_intro, "quest_choice")
        .Choice("quest_choice", npc_name, "Will you help me?")
            .AddOption(1, "Yes, I'll help you.", "accept_quest")
            .AddOption(2, "Not right now.", "decline_quest")
            .StartQuest(1, quest_id)
        .Text("accept_quest", npc_name, quest_accept, "end")
        .Text("decline_quest", npc_name, quest_decline, "end")
        .End("end");
    
    return builder.Build();
}

} // namespace DialoguePatterns

// [SEQUENCE: MVP14-26] Dialogue utilities
namespace DialogueUtils {

std::string FormatDialogueText(const std::string& text,
                              const Player& player,
                              const NPC& npc) {
    std::string formatted = text;
    
    // Replace placeholders
    size_t pos = 0;
    while ((pos = formatted.find("{player_name}", pos)) != std::string::npos) {
        formatted.replace(pos, 13, player.GetName());
        pos += player.GetName().length();
    }
    
    pos = 0;
    while ((pos = formatted.find("{npc_name}", pos)) != std::string::npos) {
        formatted.replace(pos, 10, npc.GetName());
        pos += npc.GetName().length();
    }
    
    pos = 0;
    while ((pos = formatted.find("{player_class}", pos)) != std::string::npos) {
        formatted.replace(pos, 14, player.GetClassName());
        pos += player.GetClassName().length();
    }
    
    return formatted;
}

bool ValidateDialogueTree(DialogueTreePtr tree,
                         std::vector<std::string>& errors) {
    if (!tree) {
        errors.push_back("Tree is null");
        return false;
    }
    
    return tree->Validate(errors);
}

} // namespace DialogueUtils


} // namespace mmorpg::npc