# Extension 12: Event Bus Introduction

## 1. Objective

This guide introduces a major architectural refactoring: the implementation of an **Event Bus**. The goal is to decouple systems from one another, moving from a direct-call model to a publish-subscribe pattern. Instead of one system knowing about and directly calling methods on another system, it will simply fire an event into the bus. Other systems can subscribe to these events and react accordingly, without any knowledge of the event's originator. This dramatically improves modularity and makes the codebase easier to extend and test.

## 2. Analysis of Current Implementation

*   **Tight Coupling**: As seen in `TargetedCombatSystem.h`, systems sometimes have direct pointers to other specific system classes (e.g., `GridSpatialSystem*`). This creates a rigid dependency.
*   **Implicit Dependencies**: When a player dies in `TargetedCombatSystem::OnDeath`, other systems need to react (e.g., an `ExperienceSystem` should award XP, a `LootSystem` should drop items, a `GuildWarSystem` might update scores). In the current architecture, the `TargetedCombatSystem` would need to be modified to get pointers to all these other systems and call them directly. This is not scalable and leads to "spaghetti code."

An Event Bus solves this by inverting the dependency: systems no longer need to know about each other, only about the events they are interested in.

## 3. Proposed Implementation

### Step 3.1: Design the `EventBus` and Event Structs

We will create a simple, type-safe event bus. It will map an event type to a list of callbacks.

**New File (`ecs-realm-server/src/core/events/event_bus.h`):**
```cpp
#pragma once

#include <functional>
#include <unordered_map>
#include <vector>
#include <any>
#include <typeindex>

namespace mmorpg::core::events {

// A generic subscriber handle for unsubscribing later (optional but good practice)
using SubscriberId = size_t;

class EventBus {
public:
    // Subscribe to an event of type T. The callback takes a const T&.
    template<typename T>
    static void Subscribe(std::function<void(const T&)> callback) {
        GetInstance().subscribers_[typeid(T)].push_back([callback](const std::any& event) {
            callback(std::any_cast<const T&>(event));
        });
    }

    // Publish an event. Any subscribers will be called.
    template<typename T>
    static void Publish(const T& event) {
        if (GetInstance().subscribers_.count(typeid(T))) {
            for (const auto& callback : GetInstance().subscribers_.at(typeid(T))) {
                callback(event);
            }
        }
    }

private:
    static EventBus& GetInstance() {
        static EventBus instance;
        return instance;
    }

    std::unordered_map<std::type_index, std::vector<std::function<void(const std::any&)>>> subscribers_;
};

}
```

Next, define a concrete event struct.

**New File (`ecs-realm-server/src/game/events/combat_events.h`):**
```cpp
#pragma once

#include "core/ecs/types.h"

namespace mmorpg::game::events {

struct PlayerDiedEvent {
    core::ecs::EntityId victim;
    core::ecs::EntityId killer;
    // Add other relevant info, like victim's level, etc.
};

}
```

### Step 3.2: Refactor `TargetedCombatSystem` to Publish Events

Modify the combat system to fire a `PlayerDiedEvent` instead of just containing the logic within itself.

**Modified File (`ecs-realm-server/src/game/systems/combat/targeted_combat_system.cpp`):**
```cpp
#include "core/events/event_bus.h" // [NEW]
#include "game/events/combat_events.h" // [NEW]

// ... in TargetedCombatSystem::OnDeath
void TargetedCombatSystem::OnDeath(core::ecs::EntityId entity) {
    // ... existing logic for handling death (e.g., setting state)

    // [MODIFIED] Publish an event to notify other systems
    // We need to know who the killer was. Assume we can get this from a damage map.
    // core::ecs::EntityId killer = FindKillerFor(entity);
    core::ecs::EntityId killer = 0; // Placeholder

    core::events::EventBus::Publish(game::events::PlayerDiedEvent{entity, killer});
}
```

### Step 3.3: Create a Subscriber System

Now, we can create a completely new system that listens for the `PlayerDiedEvent` without the combat system ever knowing it exists.

**New File (`ecs-realm-server/src/game/systems/experience_system.h`):**
```cpp
#pragma once

#include "core/ecs/optimized/system.h"

namespace mmorpg::game::systems {

class ExperienceSystem : public core::ecs::optimized::System {
public:
    ExperienceSystem();
};

}
```

**New File (`ecs-realm-server/src/game/systems/experience_system.cpp`):**
```cpp
#include "experience_system.h"
#include "core/events/event_bus.h"
#include "game/events/combat_events.h"
#include <iostream> // For logging/debugging

namespace mmorpg::game::systems {

ExperienceSystem::ExperienceSystem() {
    // [NEW] Subscribe to the PlayerDiedEvent in the constructor
    core::events::EventBus::Subscribe<events::PlayerDiedEvent>([this](const events::PlayerDiedEvent& event) {
        this->OnPlayerDied(event);
    });
}

void ExperienceSystem::OnPlayerDied(const events::PlayerDiedEvent& event) {
    // This method is called when any player dies
    std::cout << "Player " << event.victim << " was killed by " << event.killer << ". Awarding XP!" << std::endl;
    
    // TODO: Add logic to calculate and award experience points to the killer.
}

}
```
Finally, remember to register the new `ExperienceSystem` in your `Room` or `OptimizedWorld` setup and add the new files to `CMakeLists.txt`.

## 4. Rationale for Design

*   **Inversion of Control**: The Event Bus inverts the control flow. Instead of the `TargetedCombatSystem` controlling which other systems to call, it simply announces what happened. The `ExperienceSystem` and other systems are now in control of what they want to react to. This is a core principle of modular design.
*   **Extreme Decoupling**: The `TargetedCombatSystem` has zero knowledge of the `ExperienceSystem`. It doesn't include its header, hold a pointer to it, or even know it exists. This means you can add, remove, or modify the `ExperienceSystem` without ever touching the combat code.
*   **Scalability**: You can now easily add more listener systems. A `LootSystem` can subscribe to `PlayerDiedEvent` to handle item drops. A `GuildSystem` can subscribe to it to track kills in a guild war. A `LoggingSystem` can subscribe to it to record all player deaths. All of these can be added without a single change to the `TargetedCombatSystem`.
*   **Testability**: You can test the `ExperienceSystem` in isolation by simply publishing a mock `PlayerDiedEvent` from your test code. You don't need to set up a full combat scenario to verify its logic.

## 5. Testing Guide

1.  **Unit Test the Event Bus**: Write a test to ensure the publish/subscribe mechanism works.
    ```cpp
    #include <gtest/gtest.h>
    #include "core/events/event_bus.h"

    struct TestEvent { int value; };

    TEST(EventBusTest, PubSub) {
        int received_value = 0;
        core::events::EventBus::Subscribe<TestEvent>([&](const TestEvent& event) {
            received_value = event.value;
        });

        core::events::EventBus::Publish(TestEvent{42});

        EXPECT_EQ(received_value, 42);
    }
    ```
2.  **Integration Test the Systems**:
    *   In an integration test, set up a world with a `TargetedCombatSystem` and an `ExperienceSystem`.
    *   Simulate a scenario where one entity "kills" another, causing the `OnDeath` method in the combat system to be called.
    *   Verify that the `OnPlayerDied` method in the `ExperienceSystem` is subsequently called. You can do this by checking logs, or by having the `ExperienceSystem` modify a component that the test can inspect.
