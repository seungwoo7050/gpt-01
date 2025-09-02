#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <any>
#include "../core/types.h"
#include "../npc/npc.h"

namespace mmorpg::ai {

// [SEQUENCE: MVP14-234] Behavior tree node status
enum class NodeStatus {
    IDLE,       // 유휴
    RUNNING,    // 실행 중
    SUCCESS,    // 성공
    FAILURE     // 실패
};

// [SEQUENCE: MVP14-235] Blackboard for shared data
class Blackboard {
public:
    template<typename T>
    void Set(const std::string& key, const T& value) {
        data_[key] = value;
    }
    
    template<typename T>
    T* Get(const std::string& key) {
        auto it = data_.find(key);
        if (it != data_.end()) {
            try {
                return std::any_cast<T>(&it->second);
            } catch (const std::bad_any_cast&) {
                return nullptr;
            }
        }
        return nullptr;
    }
    
    template<typename T>
    T GetValue(const std::string& key, const T& default_value) const {
        auto it = data_.find(key);
        if (it != data_.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (const std::bad_any_cast&) {
                return default_value;
            }
        }
        return default_value;
    }
    
    bool Has(const std::string& key) const {
        return data_.find(key) != data_.end();
    }
    
    void Remove(const std::string& key) {
        data_.erase(key);
    }
    
    void Clear() {
        data_.clear();
    }
    
private:
    std::unordered_map<std::string, std::any> data_;
};

// [SEQUENCE: MVP14-236] Base behavior tree node
class BTNode {
public:
    BTNode(const std::string& name = "BTNode") : name_(name) {}
    virtual ~BTNode() = default;
    
    // Core execution
    virtual NodeStatus Execute(NPC& npc, Blackboard& blackboard) = 0;
    virtual void Reset() { status_ = NodeStatus::IDLE; }
    
    // Node properties
    const std::string& GetName() const { return name_; }
    NodeStatus GetStatus() const { return status_; }
    
    // Debug
    virtual std::string GetDebugInfo() const { return name_; }
    
protected:
    std::string name_;
    NodeStatus status_{NodeStatus::IDLE};
};

using BTNodePtr = std::shared_ptr<BTNode>;

// [SEQUENCE: MVP14-237] Composite nodes
class CompositeNode : public BTNode {
public:
    CompositeNode(const std::string& name) : BTNode(name) {}
    
    void AddChild(BTNodePtr child) {
        children_.push_back(child);
    }
    
    void RemoveChild(BTNodePtr child) {
        children_.erase(
            std::remove(children_.begin(), children_.end(), child),
            children_.end()
        );
    }
    
    const std::vector<BTNodePtr>& GetChildren() const { return children_; }
    
protected:
    std::vector<BTNodePtr> children_;
};

// [SEQUENCE: MVP14-238] Sequence node - executes children in order until one fails
class SequenceNode : public CompositeNode {
public:
    SequenceNode(const std::string& name = "Sequence") : CompositeNode(name) {}
    
    NodeStatus Execute(NPC& npc, Blackboard& blackboard) override;
    void Reset() override;
    
private:
    size_t current_child_{0};
};

// [SEQUENCE: MVP14-239] Selector node - executes children until one succeeds
class SelectorNode : public CompositeNode {
public:
    SelectorNode(const std::string& name = "Selector") : CompositeNode(name) {}
    
    NodeStatus Execute(NPC& npc, Blackboard& blackboard) override;
    void Reset() override;
    
private:
    size_t current_child_{0};
};

// [SEQUENCE: MVP14-240] Parallel node - executes all children simultaneously
class ParallelNode : public CompositeNode {
public:
    enum class Policy {
        REQUIRE_ONE,    // 하나만 성공해도 OK
        REQUIRE_ALL     // 모두 성공해야 OK
    };
    
    ParallelNode(Policy success_policy, Policy failure_policy,
                const std::string& name = "Parallel")
        : CompositeNode(name)
        , success_policy_(success_policy)
        , failure_policy_(failure_policy) {}
    
    NodeStatus Execute(NPC& npc, Blackboard& blackboard) override;
    void Reset() override;
    
private:
    Policy success_policy_;
    Policy failure_policy_;
    std::vector<NodeStatus> child_status_;
};

// [SEQUENCE: MVP14-241] Decorator nodes
class DecoratorNode : public BTNode {
public:
    DecoratorNode(BTNodePtr child, const std::string& name = "Decorator")
        : BTNode(name), child_(child) {}
    
protected:
    BTNodePtr child_;
};

// [SEQUENCE: MVP14-242] Repeater decorator - repeats child execution
class RepeaterNode : public DecoratorNode {
public:
    RepeaterNode(BTNodePtr child, int repeat_count = -1,
                const std::string& name = "Repeater")
        : DecoratorNode(child, name)
        , repeat_count_(repeat_count) {}
    
    NodeStatus Execute(NPC& npc, Blackboard& blackboard) override;
    void Reset() override;
    
private:
    int repeat_count_;  // -1 for infinite
    int current_count_{0};
};

// [SEQUENCE: MVP14-243] Inverter decorator - inverts child result
class InverterNode : public DecoratorNode {
public:
    InverterNode(BTNodePtr child, const std::string& name = "Inverter")
        : DecoratorNode(child, name) {}
    
    NodeStatus Execute(NPC& npc, Blackboard& blackboard) override;
};

// [SEQUENCE: MVP14-244] Condition decorator - executes child only if condition is true
class ConditionNode : public DecoratorNode {
public:
    using ConditionFunc = std::function<bool(NPC&, Blackboard&)>;
    
    ConditionNode(BTNodePtr child, ConditionFunc condition,
                 const std::string& name = "Condition")
        : DecoratorNode(child, name)
        , condition_(condition) {}
    
    NodeStatus Execute(NPC& npc, Blackboard& blackboard) override;
    
private:
    ConditionFunc condition_;
};

// [SEQUENCE: MVP14-245] Leaf nodes - actual behaviors
class LeafNode : public BTNode {
public:
    LeafNode(const std::string& name = "Leaf") : BTNode(name) {}
};

// [SEQUENCE: MVP14-246] Action node - executes a function
class ActionNode : public LeafNode {
public:
    using ActionFunc = std::function<NodeStatus(NPC&, Blackboard&)>;
    
    ActionNode(ActionFunc action, const std::string& name = "Action")
        : LeafNode(name), action_(action) {}
    
    NodeStatus Execute(NPC& npc, Blackboard& blackboard) override {
        if (action_) {
            status_ = action_(npc, blackboard);
            return status_;
        }
        return NodeStatus::FAILURE;
    }
    
private:
    ActionFunc action_;
};

// [SEQUENCE: MVP14-247] Common behavior nodes
namespace BehaviorNodes {
    // Movement behaviors
    BTNodePtr MoveToTarget(const std::string& target_key);
    BTNodePtr PatrolPath(const std::vector<Vector3>& waypoints);
    BTNodePtr Wander(float radius);
    BTNodePtr Flee(const std::string& threat_key, float safe_distance);
    
    // Combat behaviors
    BTNodePtr Attack(const std::string& target_key);
    BTNodePtr UseSkill(uint32_t skill_id, const std::string& target_key);
    BTNodePtr Heal(float health_threshold);
    BTNodePtr FindTarget(float search_radius, const std::string& target_key);
    
    // Social behaviors
    BTNodePtr Greet(float interaction_radius);
    BTNodePtr Trade();
    BTNodePtr OfferQuest(uint32_t quest_id);
    BTNodePtr ReactToPlayer(const std::string& reaction_type);
    
    // Utility behaviors
    BTNodePtr Wait(float duration);
    BTNodePtr SetBlackboardValue(const std::string& key, const std::any& value);
    BTNodePtr Log(const std::string& message);
    
    // Condition checks
    BTNodePtr HasTarget(const std::string& target_key);
    BTNodePtr IsHealthLow(float threshold);
    BTNodePtr IsPlayerNearby(float radius);
    BTNodePtr HasLineOfSight(const std::string& target_key);
}

// [SEQUENCE: MVP14-248] Behavior tree builder
class BehaviorTreeBuilder {
public:
    BehaviorTreeBuilder& Sequence(const std::string& name = "Sequence");
    BehaviorTreeBuilder& Selector(const std::string& name = "Selector");
    BehaviorTreeBuilder& Parallel(ParallelNode::Policy success_policy,
                                 ParallelNode::Policy failure_policy,
                                 const std::string& name = "Parallel");
    
    BehaviorTreeBuilder& Repeat(int count = -1, const std::string& name = "Repeater");
    BehaviorTreeBuilder& Invert(const std::string& name = "Inverter");
    BehaviorTreeBuilder& Condition(std::function<bool(NPC&, Blackboard&)> condition,
                                  const std::string& name = "Condition");
    
    BehaviorTreeBuilder& Action(std::function<NodeStatus(NPC&, Blackboard&)> action,
                               const std::string& name = "Action");
    
    BehaviorTreeBuilder& Node(BTNodePtr node);
    BehaviorTreeBuilder& End();  // End current composite
    
    BTNodePtr Build();
    
private:
    BTNodePtr root_;
    std::vector<std::shared_ptr<CompositeNode>> composites_;
};

// [SEQUENCE: MVP14-249] Behavior tree instance
class BehaviorTree {
public:
    BehaviorTree(BTNodePtr root) : root_(root) {}
    
    // [SEQUENCE: MVP14-250] Tree execution
    NodeStatus Tick(NPC& npc);
    void Reset();
    
    // Blackboard access
    Blackboard& GetBlackboard() { return blackboard_; }
    const Blackboard& GetBlackboard() const { return blackboard_; }
    
    // Debug
    std::string GetDebugTree() const;
    std::vector<std::string> GetExecutionPath() const { return execution_path_; }
    
private:
    BTNodePtr root_;
    Blackboard blackboard_;
    std::vector<std::string> execution_path_;
    
    void RecordExecution(const std::string& node_name);
};

// [SEQUENCE: MVP14-251] Behavior tree factory
class BehaviorTreeFactory {
public:
    static BehaviorTreeFactory& Instance() {
        static BehaviorTreeFactory instance;
        return instance;
    }
    
    // [SEQUENCE: MVP14-252] Tree registration
    void RegisterTree(const std::string& name, 
                     std::function<BTNodePtr()> creator);
    
    BTNodePtr CreateTree(const std::string& name);
    
    // Predefined tree templates
    void RegisterCommonTrees();
    
private:
    BehaviorTreeFactory() = default;
    
    std::unordered_map<std::string, std::function<BTNodePtr()>> tree_creators_;
};

// [SEQUENCE: MVP14-253] Common AI behaviors
namespace CommonBehaviors {
    // Guard behavior
    BTNodePtr CreateGuardBehavior(const Vector3& guard_position, float radius);
    
    // Merchant behavior
    BTNodePtr CreateMerchantBehavior();
    
    // Aggressive mob behavior
    BTNodePtr CreateAggressiveMobBehavior(float aggro_radius);
    
    // Passive mob behavior
    BTNodePtr CreatePassiveMobBehavior();
    
    // Quest giver behavior
    BTNodePtr CreateQuestGiverBehavior(const std::vector<uint32_t>& quest_ids);
    
    // Patrol behavior
    BTNodePtr CreatePatrolBehavior(const std::vector<Vector3>& waypoints);
    
    // Boss behavior
    BTNodePtr CreateBossBehavior(const std::vector<uint32_t>& phase_skills);
}

// [SEQUENCE: MVP14-254] Behavior tree utilities
namespace BTUtils {
    // Tree visualization
    std::string VisualizeTree(BTNodePtr root, int indent = 0);
    
    // Tree validation
    bool ValidateTree(BTNodePtr root, std::vector<std::string>& errors);
    
    // Tree cloning
    BTNodePtr CloneTree(BTNodePtr root);
    
    // Performance profiling
    struct NodeProfile {
        std::string node_name;
        uint32_t execution_count{0};
        float total_time_ms{0.0f};
        float average_time_ms{0.0f};
    };
    
    std::vector<NodeProfile> ProfileTree(BehaviorTree& tree, 
                                        NPC& npc,
                                        uint32_t iterations);
}

} // namespace mmorpg::ai
