#include "ai_behavior_system.h"
#include "../combat/combat_system.h"
#include "../skills/skill_system.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <random>

namespace mmorpg::game::ai {

// [SEQUENCE: 1052] Random number generator for AI decisions
static thread_local std::mt19937 ai_rng(std::chrono::steady_clock::now().time_since_epoch().count());

// [SEQUENCE: 1053] CompositeNode - Execute
BehaviorStatus CompositeNode::Execute(uint64_t entity_id,
                                    const AIPerception& perception,
                                    AIMemory& memory,
                                    float delta_time) {
    if (children_.empty()) {
        return BehaviorStatus::SUCCESS;
    }
    
    // [SEQUENCE: 1054] Sequence node logic
    if (type_ == BehaviorNodeType::SEQUENCE) {
        for (; current_child_ < children_.size(); ++current_child_) {
            BehaviorStatus status = children_[current_child_]->Execute(
                entity_id, perception, memory, delta_time
            );
            
            if (status == BehaviorStatus::RUNNING) {
                return BehaviorStatus::RUNNING;
            } else if (status == BehaviorStatus::FAILURE) {
                current_child_ = 0;
                return BehaviorStatus::FAILURE;
            }
        }
        current_child_ = 0;
        return BehaviorStatus::SUCCESS;
    }
    
    // [SEQUENCE: 1055] Selector node logic
    else if (type_ == BehaviorNodeType::SELECTOR) {
        for (; current_child_ < children_.size(); ++current_child_) {
            BehaviorStatus status = children_[current_child_]->Execute(
                entity_id, perception, memory, delta_time
            );
            
            if (status == BehaviorStatus::RUNNING) {
                return BehaviorStatus::RUNNING;
            } else if (status == BehaviorStatus::SUCCESS) {
                current_child_ = 0;
                return BehaviorStatus::SUCCESS;
            }
        }
        current_child_ = 0;
        return BehaviorStatus::FAILURE;
    }
    
    // [SEQUENCE: 1056] Parallel node logic
    else if (type_ == BehaviorNodeType::PARALLEL) {
        bool any_running = false;
        bool any_success = false;
        
        for (auto& child : children_) {
            BehaviorStatus status = child->Execute(entity_id, perception, memory, delta_time);
            if (status == BehaviorStatus::RUNNING) {
                any_running = true;
            } else if (status == BehaviorStatus::SUCCESS) {
                any_success = true;
            }
        }
        
        if (any_running) return BehaviorStatus::RUNNING;
        if (any_success) return BehaviorStatus::SUCCESS;
        return BehaviorStatus::FAILURE;
    }
    
    return BehaviorStatus::FAILURE;
}

// [SEQUENCE: 1057] CompositeNode - Reset
void CompositeNode::Reset() {
    current_child_ = 0;
    for (auto& child : children_) {
        child->Reset();
    }
}

// [SEQUENCE: 1058] DecoratorNode - Execute
BehaviorStatus DecoratorNode::Execute(uint64_t entity_id,
                                    const AIPerception& perception,
                                    AIMemory& memory,
                                    float delta_time) {
    if (!child_) {
        return BehaviorStatus::FAILURE;
    }
    
    // Check condition
    if (!condition_(perception, memory)) {
        return BehaviorStatus::FAILURE;
    }
    
    // Execute child if condition passes
    return child_->Execute(entity_id, perception, memory, delta_time);
}

// [SEQUENCE: 1059] AttackTargetAction - Execute
BehaviorStatus AttackTargetAction::Execute(uint64_t entity_id,
                                         const AIPerception& perception,
                                         AIMemory& memory,
                                         float delta_time) {
    // Check if we have a target
    if (perception.highest_threat_target == 0) {
        return BehaviorStatus::FAILURE;
    }
    
    // Execute attack
    auto& combat_mgr = combat::CombatManager::Instance();
    if (combat_mgr.ExecuteAttack(entity_id, perception.highest_threat_target)) {
        memory.recent_actions.push_back("attack");
        return BehaviorStatus::SUCCESS;
    }
    
    return BehaviorStatus::FAILURE;
}

// [SEQUENCE: 1060] MoveToTargetAction - Execute
BehaviorStatus MoveToTargetAction::Execute(uint64_t entity_id,
                                         const AIPerception& perception,
                                         AIMemory& memory,
                                         float delta_time) {
    if (perception.highest_threat_target == 0) {
        return BehaviorStatus::FAILURE;
    }
    
    // TODO: Get positions from position component
    // TODO: Calculate path to target
    // TODO: Move along path
    
    // For now, assume we're moving
    memory.recent_actions.push_back("move_to_target");
    return BehaviorStatus::RUNNING;
}

// [SEQUENCE: 1061] FleeAction - Execute
BehaviorStatus FleeAction::Execute(uint64_t entity_id,
                                 const AIPerception& perception,
                                 AIMemory& memory,
                                 float delta_time) {
    if (perception.visible_enemies.empty()) {
        return BehaviorStatus::SUCCESS;
    }
    
    // TODO: Calculate flee direction (opposite of nearest enemy)
    // TODO: Move in flee direction
    
    memory.recent_actions.push_back("flee");
    memory.flags["is_fleeing"] = true;
    
    return BehaviorStatus::RUNNING;
}

// [SEQUENCE: 1062] PatrolAction - Execute
BehaviorStatus PatrolAction::Execute(uint64_t entity_id,
                                   const AIPerception& perception,
                                   AIMemory& memory,
                                   float delta_time) {
    if (patrol_points_.empty()) {
        return BehaviorStatus::FAILURE;
    }
    
    // Get current patrol point
    size_t& current_point = memory.current_patrol_point;
    if (current_point >= patrol_points_.size()) {
        current_point = 0;
    }
    
    const auto& target_point = patrol_points_[current_point];
    
    // TODO: Get current position
    // TODO: Check if reached patrol point
    // TODO: Move towards patrol point
    
    // For now, simulate reaching point
    static int steps = 0;
    if (++steps > 10) {
        steps = 0;
        
        // Move to next point
        if (memory.patrol_forward) {
            current_point++;
            if (current_point >= patrol_points_.size() - 1) {
                current_point = patrol_points_.size() - 1;
                memory.patrol_forward = false;
            }
        } else {
            if (current_point > 0) {
                current_point--;
            } else {
                memory.patrol_forward = true;
            }
        }
    }
    
    memory.recent_actions.push_back("patrol");
    return BehaviorStatus::RUNNING;
}

// [SEQUENCE: 1063] UseSkillAction - Execute
BehaviorStatus UseSkillAction::Execute(uint64_t entity_id,
                                     const AIPerception& perception,
                                     AIMemory& memory,
                                     float delta_time) {
    if (perception.highest_threat_target == 0) {
        return BehaviorStatus::FAILURE;
    }
    
    // Check if skill is available
    auto& skill_mgr = skills::SkillManager::Instance();
    if (skill_mgr.IsOnCooldown(entity_id, skill_id_)) {
        return BehaviorStatus::FAILURE;
    }
    
    // Cast skill
    auto result = skill_mgr.StartCast(entity_id, skill_id_, perception.highest_threat_target);
    if (result.success) {
        memory.recent_actions.push_back("use_skill_" + std::to_string(skill_id_));
        return BehaviorStatus::SUCCESS;
    }
    
    return BehaviorStatus::FAILURE;
}

// [SEQUENCE: 1064] AIController - Update
void AIController::Update(float delta_time) {
    // Update perception periodically
    perception_update_timer_ += delta_time;
    if (perception_update_timer_ >= PERCEPTION_UPDATE_INTERVAL) {
        UpdatePerception();
        perception_update_timer_ = 0.0f;
    }
    
    // Update behavior tree periodically
    behavior_update_timer_ += delta_time;
    if (behavior_update_timer_ >= BEHAVIOR_UPDATE_INTERVAL) {
        if (behavior_tree_ && current_state_ != AIState::DEAD) {
            behavior_tree_->Execute(entity_id_, perception_, memory_, delta_time);
        }
        behavior_update_timer_ = 0.0f;
    }
    
    // State-specific updates
    switch (current_state_) {
        case AIState::COMBAT:
            // Check if still have valid targets
            if (perception_.visible_enemies.empty() && perception_.highest_threat_target == 0) {
                SetState(AIState::RETURNING);
            }
            break;
            
        case AIState::RETURNING:
            // Check if returned to spawn
            if (perception_.distance_to_spawn < 2.0f) {
                SetState(AIState::IDLE);
            }
            break;
            
        case AIState::FLEEING:
            // Check if safe to stop fleeing
            if (perception_.health_percentage > 0.5f || perception_.visible_enemies.empty()) {
                SetState(AIState::COMBAT);
                memory_.flags["is_fleeing"] = false;
            }
            break;
    }
}

// [SEQUENCE: 1065] AIController - Set state
void AIController::SetState(AIState state) {
    if (current_state_ == state) {
        return;
    }
    
    spdlog::debug("AI {} changing state from {} to {}", 
                  entity_id_, static_cast<int>(current_state_), static_cast<int>(state));
    
    current_state_ = state;
    
    // Reset behavior tree on state change
    if (behavior_tree_) {
        behavior_tree_->Reset();
    }
}

// [SEQUENCE: 1066] AIController - Update perception
void AIController::UpdatePerception() {
    // TODO: Get actual perception data from spatial systems
    
    // Update threat information
    auto& combat_mgr = combat::CombatManager::Instance();
    perception_.highest_threat_target = combat_mgr.GetHighestThreatTarget(entity_id_);
    perception_.highest_threat_value = combat_mgr.GetThreat(perception_.highest_threat_target, entity_id_);
    
    // TODO: Update visible entities
    // TODO: Update environmental awareness
    // TODO: Update health/mana status
}

// [SEQUENCE: 1067] AIController - On damaged
void AIController::OnDamaged(uint64_t attacker_id, float damage) {
    memory_.last_attacker_id = attacker_id;
    memory_.last_combat_time = std::chrono::steady_clock::now();
    
    // Enter combat if not already
    if (current_state_ == AIState::IDLE || current_state_ == AIState::PATROL) {
        SetState(AIState::COMBAT);
    }
    
    // Flee if health is low and personality allows
    if (perception_.health_percentage < 0.2f && personality_ == AIPersonality::COWARDLY) {
        SetState(AIState::FLEEING);
    }
}

// [SEQUENCE: 1068] AIController - Set respawn position
void AIController::SetRespawnPosition(float x, float y, float z) {
    spawn_x_ = x;
    spawn_y_ = y;
    spawn_z_ = z;
}

// [SEQUENCE: 1069] AIManager - Create AI
std::shared_ptr<AIController> AIManager::CreateAI(uint64_t entity_id, AIPersonality personality) {
    auto controller = std::make_shared<AIController>(entity_id, personality);
    ai_controllers_[entity_id] = controller;
    
    spdlog::info("Created AI controller for entity {} with personality {}", 
                 entity_id, static_cast<int>(personality));
    
    return controller;
}

// [SEQUENCE: 1070] AIManager - Remove AI
void AIManager::RemoveAI(uint64_t entity_id) {
    ai_controllers_.erase(entity_id);
    spdlog::debug("Removed AI controller for entity {}", entity_id);
}

// [SEQUENCE: 1071] AIManager - Get AI
std::shared_ptr<AIController> AIManager::GetAI(uint64_t entity_id) const {
    auto it = ai_controllers_.find(entity_id);
    return (it != ai_controllers_.end()) ? it->second : nullptr;
}

// [SEQUENCE: 1072] AIManager - Update
void AIManager::Update(float delta_time) {
    for (auto& [entity_id, controller] : ai_controllers_) {
        controller->Update(delta_time);
    }
}

// [SEQUENCE: 1073] AIManager - Notify damage
void AIManager::NotifyDamage(uint64_t victim_id, uint64_t attacker_id, float damage) {
    // Notify victim
    auto victim_ai = GetAI(victim_id);
    if (victim_ai) {
        victim_ai->OnDamaged(attacker_id, damage);
    }
    
    // Notify nearby allies
    // TODO: Get nearby entities and notify ally damage
}

// [SEQUENCE: 1074] BehaviorTreeBuilder - Create aggressive melee
std::shared_ptr<IBehaviorNode> BehaviorTreeBuilder::CreateAggressiveMelee() {
    auto root = std::make_shared<CompositeNode>(BehaviorNodeType::SELECTOR);
    
    // Combat sequence
    auto combat_sequence = std::make_shared<CompositeNode>(BehaviorNodeType::SEQUENCE);
    combat_sequence->AddChild(std::make_shared<DecoratorNode>(
        std::make_shared<CompositeNode>(BehaviorNodeType::SEQUENCE),
        HasTarget()
    ));
    
    auto combat_behavior = std::make_shared<CompositeNode>(BehaviorNodeType::SEQUENCE);
    combat_behavior->AddChild(std::make_shared<MoveToTargetAction>(2.0f));  // Melee range
    combat_behavior->AddChild(std::make_shared<AttackTargetAction>());
    
    combat_sequence->AddChild(combat_behavior);
    root->AddChild(combat_sequence);
    
    // Patrol when no target
    auto patrol_points = std::vector<std::pair<float, float>>{
        {0, 0}, {10, 0}, {10, 10}, {0, 10}
    };
    root->AddChild(std::make_shared<PatrolAction>(patrol_points));
    
    return root;
}

// [SEQUENCE: 1075] BehaviorTreeBuilder - Has target condition
std::function<bool(const AIPerception&, const AIMemory&)> 
BehaviorTreeBuilder::HasTarget() {
    return [](const AIPerception& perception, const AIMemory& memory) {
        return perception.highest_threat_target != 0;
    };
}

// [SEQUENCE: 1076] BehaviorTreeBuilder - Health below condition
std::function<bool(const AIPerception&, const AIMemory&)> 
BehaviorTreeBuilder::HealthBelow(float percentage) {
    return [percentage](const AIPerception& perception, const AIMemory& memory) {
        return perception.health_percentage < percentage;
    };
}

} // namespace mmorpg::game::ai