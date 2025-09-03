#include "behavior_tree.h"
#include "../core/logger.h"
#include <spdlog/spdlog.h>
#include <sstream>
#include <chrono>

namespace mmorpg::ai {

// [SEQUENCE: 3618] Sequence node implementation
NodeStatus SequenceNode::Execute(NPC& npc, Blackboard& blackboard) {
    if (children_.empty()) {
        return NodeStatus::SUCCESS;
    }
    
    while (current_child_ < children_.size()) {
        NodeStatus child_status = children_[current_child_]->Execute(npc, blackboard);
        
        if (child_status == NodeStatus::RUNNING) {
            return NodeStatus::RUNNING;
        }
        
        if (child_status == NodeStatus::FAILURE) {
            current_child_ = 0;
            return NodeStatus::FAILURE;
        }
        
        // Child succeeded, move to next
        current_child_++;
    }
    
    // All children succeeded
    current_child_ = 0;
    return NodeStatus::SUCCESS;
}

void SequenceNode::Reset() {
    BTNode::Reset();
    current_child_ = 0;
    for (auto& child : children_) {
        child->Reset();
    }
}

// [SEQUENCE: 3619] Selector node implementation
NodeStatus SelectorNode::Execute(NPC& npc, Blackboard& blackboard) {
    if (children_.empty()) {
        return NodeStatus::FAILURE;
    }
    
    while (current_child_ < children_.size()) {
        NodeStatus child_status = children_[current_child_]->Execute(npc, blackboard);
        
        if (child_status == NodeStatus::RUNNING) {
            return NodeStatus::RUNNING;
        }
        
        if (child_status == NodeStatus::SUCCESS) {
            current_child_ = 0;
            return NodeStatus::SUCCESS;
        }
        
        // Child failed, try next
        current_child_++;
    }
    
    // All children failed
    current_child_ = 0;
    return NodeStatus::FAILURE;
}

void SelectorNode::Reset() {
    BTNode::Reset();
    current_child_ = 0;
    for (auto& child : children_) {
        child->Reset();
    }
}

// [SEQUENCE: 3620] Parallel node implementation
NodeStatus ParallelNode::Execute(NPC& npc, Blackboard& blackboard) {
    if (children_.empty()) {
        return NodeStatus::SUCCESS;
    }
    
    // Initialize status tracking
    if (child_status_.size() != children_.size()) {
        child_status_.resize(children_.size(), NodeStatus::IDLE);
    }
    
    uint32_t success_count = 0;
    uint32_t failure_count = 0;
    uint32_t running_count = 0;
    
    // Execute all children
    for (size_t i = 0; i < children_.size(); ++i) {
        if (child_status_[i] == NodeStatus::SUCCESS ||
            child_status_[i] == NodeStatus::FAILURE) {
            continue;  // Already finished
        }
        
        child_status_[i] = children_[i]->Execute(npc, blackboard);
        
        switch (child_status_[i]) {
            case NodeStatus::SUCCESS:
                success_count++;
                break;
            case NodeStatus::FAILURE:
                failure_count++;
                break;
            case NodeStatus::RUNNING:
                running_count++;
                break;
            default:
                break;
        }
    }
    
    // Check success policy
    if (success_policy_ == Policy::REQUIRE_ALL) {
        if (success_count == children_.size()) {
            return NodeStatus::SUCCESS;
        }
    } else {  // REQUIRE_ONE
        if (success_count > 0) {
            return NodeStatus::SUCCESS;
        }
    }
    
    // Check failure policy
    if (failure_policy_ == Policy::REQUIRE_ALL) {
        if (failure_count == children_.size()) {
            return NodeStatus::FAILURE;
        }
    } else {  // REQUIRE_ONE
        if (failure_count > 0) {
            return NodeStatus::FAILURE;
        }
    }
    
    // Still running
    if (running_count > 0) {
        return NodeStatus::RUNNING;
    }
    
    // Default based on policies
    return NodeStatus::SUCCESS;
}

void ParallelNode::Reset() {
    BTNode::Reset();
    child_status_.clear();
    for (auto& child : children_) {
        child->Reset();
    }
}

// [SEQUENCE: 3621] Repeater node implementation
NodeStatus RepeaterNode::Execute(NPC& npc, Blackboard& blackboard) {
    if (!child_) {
        return NodeStatus::FAILURE;
    }
    
    // Infinite repeat
    if (repeat_count_ == -1) {
        NodeStatus status = child_->Execute(npc, blackboard);
        if (status != NodeStatus::RUNNING) {
            child_->Reset();
        }
        return NodeStatus::RUNNING;  // Always running for infinite repeat
    }
    
    // Limited repeat
    while (current_count_ < repeat_count_) {
        NodeStatus status = child_->Execute(npc, blackboard);
        
        if (status == NodeStatus::RUNNING) {
            return NodeStatus::RUNNING;
        }
        
        if (status == NodeStatus::FAILURE) {
            current_count_ = 0;
            return NodeStatus::FAILURE;
        }
        
        // Success, continue repeating
        current_count_++;
        child_->Reset();
    }
    
    current_count_ = 0;
    return NodeStatus::SUCCESS;
}

void RepeaterNode::Reset() {
    DecoratorNode::Reset();
    current_count_ = 0;
    if (child_) {
        child_->Reset();
    }
}

// [SEQUENCE: 3622] Inverter node implementation
NodeStatus InverterNode::Execute(NPC& npc, Blackboard& blackboard) {
    if (!child_) {
        return NodeStatus::FAILURE;
    }
    
    NodeStatus status = child_->Execute(npc, blackboard);
    
    switch (status) {
        case NodeStatus::SUCCESS:
            return NodeStatus::FAILURE;
        case NodeStatus::FAILURE:
            return NodeStatus::SUCCESS;
        default:
            return status;
    }
}

// [SEQUENCE: 3623] Condition node implementation
NodeStatus ConditionNode::Execute(NPC& npc, Blackboard& blackboard) {
    if (!child_ || !condition_) {
        return NodeStatus::FAILURE;
    }
    
    if (condition_(npc, blackboard)) {
        return child_->Execute(npc, blackboard);
    }
    
    return NodeStatus::FAILURE;
}

// [SEQUENCE: 3624] Common behavior node implementations
namespace BehaviorNodes {

BTNodePtr MoveToTarget(const std::string& target_key) {
    return std::make_shared<ActionNode>(
        [target_key](NPC& npc, Blackboard& blackboard) -> NodeStatus {
            auto* target_pos = blackboard.Get<Vector3>(target_key);
            if (!target_pos) {
                return NodeStatus::FAILURE;
            }
            
            // Check if already at target
            float distance = Vector3::Distance(npc.GetPosition(), *target_pos);
            if (distance < 1.0f) {
                return NodeStatus::SUCCESS;
            }
            
            // Move towards target
            npc.MoveTo(*target_pos);
            return NodeStatus::RUNNING;
        },
        "MoveToTarget"
    );
}

BTNodePtr PatrolPath(const std::vector<Vector3>& waypoints) {
    return std::make_shared<ActionNode>(
        [waypoints](NPC& npc, Blackboard& blackboard) -> NodeStatus {
            if (waypoints.empty()) {
                return NodeStatus::FAILURE;
            }
            
            // Get current waypoint index
            int current_index = blackboard.GetValue("patrol_index", 0);
            
            // Move to current waypoint
            const Vector3& target = waypoints[current_index];
            float distance = Vector3::Distance(npc.GetPosition(), target);
            
            if (distance < 1.0f) {
                // Reached waypoint, move to next
                current_index = (current_index + 1) % waypoints.size();
                blackboard.Set("patrol_index", current_index);
            }
            
            npc.MoveTo(target);
            return NodeStatus::RUNNING;
        },
        "PatrolPath"
    );
}

BTNodePtr Attack(const std::string& target_key) {
    return std::make_shared<ActionNode>(
        [target_key](NPC& npc, Blackboard& blackboard) -> NodeStatus {
            auto* target_id = blackboard.Get<uint64_t>(target_key + "_id");
            if (!target_id) {
                return NodeStatus::FAILURE;
            }
            
            // Check if in attack range
            auto* target_pos = blackboard.Get<Vector3>(target_key + "_pos");
            if (!target_pos) {
                return NodeStatus::FAILURE;
            }
            
            float distance = Vector3::Distance(npc.GetPosition(), *target_pos);
            if (distance > npc.GetAttackRange()) {
                return NodeStatus::FAILURE;
            }
            
            // Perform attack
            npc.Attack(*target_id);
            return NodeStatus::SUCCESS;
        },
        "Attack"
    );
}

BTNodePtr FindTarget(float search_radius, const std::string& target_key) {
    return std::make_shared<ActionNode>(
        [search_radius, target_key](NPC& npc, Blackboard& blackboard) -> NodeStatus {
            // Find nearest enemy
            auto enemies = npc.GetNearbyEnemies(search_radius);
            if (enemies.empty()) {
                blackboard.Remove(target_key + "_id");
                blackboard.Remove(target_key + "_pos");
                return NodeStatus::FAILURE;
            }
            
            // Store target info
            blackboard.Set(target_key + "_id", enemies[0].entity_id);
            blackboard.Set(target_key + "_pos", enemies[0].position);
            
            return NodeStatus::SUCCESS;
        },
        "FindTarget"
    );
}

BTNodePtr Wait(float duration) {
    return std::make_shared<ActionNode>(
        [duration](NPC& npc, Blackboard& blackboard) -> NodeStatus {
            std::string wait_key = "wait_start_" + std::to_string(npc.GetId());
            
            if (!blackboard.Has(wait_key)) {
                blackboard.Set(wait_key, 
                    std::chrono::steady_clock::now());
                return NodeStatus::RUNNING;
            }
            
            auto* start_time = blackboard.Get<std::chrono::steady_clock::time_point>(wait_key);
            if (!start_time) {
                return NodeStatus::FAILURE;
            }
            
            auto elapsed = std::chrono::steady_clock::now() - *start_time;
            if (std::chrono::duration<float>(elapsed).count() >= duration) {
                blackboard.Remove(wait_key);
                return NodeStatus::SUCCESS;
            }
            
            return NodeStatus::RUNNING;
        },
        "Wait"
    );
}

BTNodePtr IsHealthLow(float threshold) {
    return std::make_shared<ActionNode>(
        [threshold](NPC& npc, Blackboard& blackboard) -> NodeStatus {
            float health_percent = npc.GetHealthPercent();
            return health_percent < threshold ? 
                NodeStatus::SUCCESS : NodeStatus::FAILURE;
        },
        "IsHealthLow"
    );
}

BTNodePtr IsPlayerNearby(float radius) {
    return std::make_shared<ActionNode>(
        [radius](NPC& npc, Blackboard& blackboard) -> NodeStatus {
            auto players = npc.GetNearbyPlayers(radius);
            if (!players.empty()) {
                blackboard.Set("nearby_player_id", players[0].entity_id);
                blackboard.Set("nearby_player_pos", players[0].position);
                return NodeStatus::SUCCESS;
            }
            return NodeStatus::FAILURE;
        },
        "IsPlayerNearby"
    );
}

} // namespace BehaviorNodes

// [SEQUENCE: 3625] Behavior tree builder implementation
BehaviorTreeBuilder& BehaviorTreeBuilder::Sequence(const std::string& name) {
    auto node = std::make_shared<SequenceNode>(name);
    
    if (!root_) {
        root_ = node;
    } else if (!composites_.empty()) {
        composites_.back()->AddChild(node);
    }
    
    composites_.push_back(node);
    return *this;
}

BehaviorTreeBuilder& BehaviorTreeBuilder::Selector(const std::string& name) {
    auto node = std::make_shared<SelectorNode>(name);
    
    if (!root_) {
        root_ = node;
    } else if (!composites_.empty()) {
        composites_.back()->AddChild(node);
    }
    
    composites_.push_back(node);
    return *this;
}

BehaviorTreeBuilder& BehaviorTreeBuilder::Action(
    std::function<NodeStatus(NPC&, Blackboard&)> action,
    const std::string& name) {
    
    auto node = std::make_shared<ActionNode>(action, name);
    
    if (!root_) {
        root_ = node;
    } else if (!composites_.empty()) {
        composites_.back()->AddChild(node);
    }
    
    return *this;
}

BehaviorTreeBuilder& BehaviorTreeBuilder::Node(BTNodePtr node) {
    if (!root_) {
        root_ = node;
    } else if (!composites_.empty()) {
        composites_.back()->AddChild(node);
    }
    
    return *this;
}

BehaviorTreeBuilder& BehaviorTreeBuilder::End() {
    if (!composites_.empty()) {
        composites_.pop_back();
    }
    return *this;
}

BTNodePtr BehaviorTreeBuilder::Build() {
    return root_;
}

// [SEQUENCE: 3626] Behavior tree execution
NodeStatus BehaviorTree::Tick(NPC& npc) {
    if (!root_) {
        return NodeStatus::FAILURE;
    }
    
    execution_path_.clear();
    NodeStatus status = root_->Execute(npc, blackboard_);
    
    return status;
}

void BehaviorTree::Reset() {
    if (root_) {
        root_->Reset();
    }
    execution_path_.clear();
}

void BehaviorTree::RecordExecution(const std::string& node_name) {
    execution_path_.push_back(node_name);
}

// [SEQUENCE: 3627] Common behavior implementations
namespace CommonBehaviors {

BTNodePtr CreateGuardBehavior(const Vector3& guard_position, float radius) {
    BehaviorTreeBuilder builder;
    
    return builder
        .Selector("GuardBehavior")
            // Combat sequence
            .Sequence("CombatSequence")
                .Node(BehaviorNodes::FindTarget(radius, "threat"))
                .Node(BehaviorNodes::MoveToTarget("threat_pos"))
                .Node(BehaviorNodes::Attack("threat"))
            .End()
            // Return to post
            .Sequence("ReturnToPost")
                .Action([guard_position](NPC& npc, Blackboard& blackboard) {
                    blackboard.Set("guard_post", guard_position);
                    return NodeStatus::SUCCESS;
                }, "SetGuardPost")
                .Node(BehaviorNodes::MoveToTarget("guard_post"))
                .Node(BehaviorNodes::Wait(2.0f))
            .End()
        .End()
        .Build();
}

BTNodePtr CreateAggressiveMobBehavior(float aggro_radius) {
    BehaviorTreeBuilder builder;
    
    return builder
        .Selector("AggressiveMob")
            // Low health retreat
            .Sequence("Retreat")
                .Node(BehaviorNodes::IsHealthLow(0.2f))
                .Action([](NPC& npc, Blackboard& blackboard) {
                    // Find safe position
                    Vector3 retreat_pos = npc.GetPosition() + Vector3(10, 0, 10);
                    blackboard.Set("retreat_pos", retreat_pos);
                    return NodeStatus::SUCCESS;
                }, "FindRetreatPos")
                .Node(BehaviorNodes::MoveToTarget("retreat_pos"))
            .End()
            // Attack sequence
            .Sequence("AttackSequence")
                .Node(BehaviorNodes::FindTarget(aggro_radius, "enemy"))
                .Node(BehaviorNodes::MoveToTarget("enemy_pos"))
                .Node(BehaviorNodes::Attack("enemy"))
            .End()
            // Idle wander
            .Node(BehaviorNodes::Wander(5.0f))
        .End()
        .Build();
}

BTNodePtr CreateMerchantBehavior() {
    BehaviorTreeBuilder builder;
    
    return builder
        .Parallel(ParallelNode::Policy::REQUIRE_ONE,
                 ParallelNode::Policy::REQUIRE_ALL,
                 "MerchantBehavior")
            // Greet nearby players
            .Sequence("GreetSequence")
                .Node(BehaviorNodes::IsPlayerNearby(5.0f))
                .Node(BehaviorNodes::Greet(5.0f))
                .Node(BehaviorNodes::Wait(10.0f))  // Cooldown
            .End()
            // Handle trades
            .Node(BehaviorNodes::Trade())
            // Idle animation
            .Sequence("IdleSequence")
                .Node(BehaviorNodes::Wait(5.0f))
                .Action([](NPC& npc, Blackboard& blackboard) {
                    npc.PlayAnimation("merchant_idle");
                    return NodeStatus::SUCCESS;
                }, "PlayIdleAnim")
            .End()
        .End()
        .Build();
}

} // namespace CommonBehaviors

// [SEQUENCE: 3628] Behavior tree factory implementation
void BehaviorTreeFactory::RegisterTree(const std::string& name,
                                     std::function<BTNodePtr()> creator) {
    tree_creators_[name] = creator;
    spdlog::debug("[AI] Registered behavior tree: {}", name);
}

BTNodePtr BehaviorTreeFactory::CreateTree(const std::string& name) {
    auto it = tree_creators_.find(name);
    if (it != tree_creators_.end()) {
        return it->second();
    }
    
    spdlog::warn("[AI] Behavior tree not found: {}", name);
    return nullptr;
}

void BehaviorTreeFactory::RegisterCommonTrees() {
    RegisterTree("guard", []() {
        return CommonBehaviors::CreateGuardBehavior(Vector3::Zero(), 10.0f);
    });
    
    RegisterTree("aggressive_mob", []() {
        return CommonBehaviors::CreateAggressiveMobBehavior(15.0f);
    });
    
    RegisterTree("merchant", []() {
        return CommonBehaviors::CreateMerchantBehavior();
    });
}

// [SEQUENCE: 3629] Behavior tree utilities
namespace BTUtils {

std::string VisualizeTree(BTNodePtr root, int indent) {
    if (!root) {
        return "";
    }
    
    std::stringstream ss;
    std::string spaces(indent * 2, ' ');
    
    ss << spaces << "- " << root->GetName() 
       << " [" << root->GetDebugInfo() << "]\n";
    
    if (auto composite = std::dynamic_pointer_cast<CompositeNode>(root)) {
        for (const auto& child : composite->GetChildren()) {
            ss << VisualizeTree(child, indent + 1);
        }
    } else if (auto decorator = std::dynamic_pointer_cast<DecoratorNode>(root)) {
        if (decorator->child_) {
            ss << VisualizeTree(decorator->child_, indent + 1);
        }
    }
    
    return ss.str();
}

bool ValidateTree(BTNodePtr root, std::vector<std::string>& errors) {
    if (!root) {
        errors.push_back("Root node is null");
        return false;
    }
    
    bool valid = true;
    
    if (auto composite = std::dynamic_pointer_cast<CompositeNode>(root)) {
        if (composite->GetChildren().empty()) {
            errors.push_back(composite->GetName() + " has no children");
            valid = false;
        }
        
        for (const auto& child : composite->GetChildren()) {
            valid &= ValidateTree(child, errors);
        }
    } else if (auto decorator = std::dynamic_pointer_cast<DecoratorNode>(root)) {
        if (!decorator->child_) {
            errors.push_back(decorator->GetName() + " has no child");
            valid = false;
        } else {
            valid &= ValidateTree(decorator->child_, errors);
        }
    }
    
    return valid;
}

} // namespace BTUtils

} // namespace mmorpg::ai