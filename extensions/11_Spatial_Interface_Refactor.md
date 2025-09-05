# Extension 11: Spatial Partitioning Interface Generalization

## 1. Objective

This guide details how to refactor the existing spatial partitioning systems (`GridSpatialSystem` and `OctreeSpatialSystem`) to adhere to a common interface. The goal is to apply the Strategy Pattern, decoupling game logic from the specific implementation of the spatial system. This will allow a game world (or `Room`) to select the most appropriate spatial system (e.g., a 2D grid for flat terrain, a 3D octree for multi-level dungeons) at runtime without changing the code that uses it.

## 2. Analysis of Current Implementation

*   **Duplicate APIs**: `GridSpatialSystem` and `OctreeSpatialSystem` have very similar public methods, such as `GetEntitiesInRadius` and `GetEntitiesInView`. This code duplication is a sign that a common abstraction is missing.
*   **Tight Coupling**: Any system that needs to perform a spatial query must know *exactly* which type of system it is talking to (`GridSpatialSystem` or `OctreeSpatialSystem`). This makes it difficult to switch between them or to use different systems for different maps.
*   **No Common Interface**: Both classes inherit from `core::ecs::optimized::System`, but this base class does not provide any of the spatial query methods. There is no contract that enforces the common API.

## 3. Proposed Implementation

### Step 3.1: Define the `ISpatialSystem` Interface

First, we create a new header file for a pure abstract class (`interface`) that defines the common contract for all spatial partitioning systems.

**New File (`ecs-realm-server/src/game/systems/i_spatial_system.h`):**
```cpp
#pragma once

#include "core/ecs/types.h"
#include "core/utils/vector3.h"
#include <vector>

namespace mmorpg::game::systems {

// This interface defines the contract for all spatial partitioning systems.
class ISpatialSystem {
public:
    virtual ~ISpatialSystem() = default;

    // The core spatial query function that all systems must implement.
    virtual std::vector<core::ecs::EntityId> GetEntitiesInRadius(
        const core::utils::Vector3& center, float radius) const = 0;

    // Other common queries can also be added here.
    virtual std::vector<core::ecs::EntityId> GetEntitiesInView(
        core::ecs::EntityId observer, float view_distance) const = 0;
};

}
```

### Step 3.2: Refactor Existing Systems to Implement the Interface

Now, we modify `GridSpatialSystem` and `OctreeSpatialSystem` to inherit from our new interface. Since they already have methods with matching signatures, the changes are mostly in the class definition.

**Modified File (`ecs-realm-server/src/game/systems/grid_spatial_system.h`):**
```diff
#pragma once

#include "core/ecs/optimized/system.h"
#include "game/world/grid/world_grid.h"
+ #include "game/systems/i_spatial_system.h" // [NEW]
#include <memory>

namespace mmorpg::game::systems {

- class GridSpatialSystem : public core::ecs::optimized::System {
+ // [MODIFIED] Inherit from the new interface
+ class GridSpatialSystem : public core::ecs::optimized::System, public ISpatialSystem {
 public:
     // ... constructor ...
     
     // ... lifecycle methods ...
     
-    // [REMOVED FROM PUBLIC] These are now part of the interface contract
-    std::vector<core::ecs::EntityId> GetEntitiesInRadius(
-        const core::utils::Vector3& center, float radius) const;
-    
-    std::vector<core::ecs::EntityId> GetEntitiesInView(
-        core::ecs::EntityId observer, float view_distance) const;
+
+    // [MODIFIED] Use 'override' to ensure we are correctly implementing the interface
+    std::vector<core::ecs::EntityId> GetEntitiesInRadius(
+        const core::utils::Vector3& center, float radius) const override;
+    
+    std::vector<core::ecs::EntityId> GetEntitiesInView(
+        core::ecs::EntityId observer, float view_distance) const override;

     // ... other public methods ...
 };

}
```
*Apply the same changes to `OctreeSpatialSystem.h`, inheriting from `ISpatialSystem` and adding the `override` keyword to the method declarations.*

### Step 3.3: Use the Interface in the Game World

The `OptimizedWorld` (or the `Room` class from Ext. 08) should no longer hold a raw pointer to a specific system type. Instead, it should hold a pointer to the `ISpatialSystem` interface.

**Conceptual Code (`ecs-realm-server/src/game/room/room.h`):**
```cpp
#include "core/ecs/optimized/optimized_world.h"
#include "game/systems/i_spatial_system.h" // [NEW]

class Room {
public:
    // [MODIFIED] The constructor can decide which system to create
    Room(std::string id, bool use_octree) {
        m_world = std::make_unique<core::ecs::optimized::OptimizedWorld>();
        if (use_octree) {
            m_spatial_system = m_world->RegisterSystem<OctreeSpatialSystem>();
        } else {
            m_spatial_system = m_world->RegisterSystem<GridSpatialSystem>();
        }
    }

    // This method can now perform queries without knowing the underlying implementation
    void DoProximityChecks() {
        for (auto entity : m_players) {
            // The call is the same regardless of grid or octree
            auto nearby = m_spatial_system->GetEntitiesInRadius(entity.position, 10.0f);
            // ... do something with nearby entities
        }
    }

private:
    std::unique_ptr<core::ecs::optimized::OptimizedWorld> m_world;
    // [MODIFIED] Store a pointer to the interface, not the concrete class
    std::shared_ptr<ISpatialSystem> m_spatial_system;
    // ...
};
```

## 4. Rationale for Design

*   **Strategy Pattern**: This refactoring is a textbook example of the Strategy Pattern. We have defined a family of algorithms (Grid, Octree), encapsulated each one, and made them interchangeable. The `Room` class (the context) can now be configured with a concrete strategy at runtime.
*   **Decoupling**: Game logic that requires spatial information (like AI, combat, etc.) no longer needs to depend on `GridSpatialSystem` or `OctreeSpatialSystem`. It only needs to know about the `ISpatialSystem` interface. This dramatically reduces coupling and makes the codebase easier to maintain and extend.
*   **Flexibility**: It is now trivial to add a new spatial partitioning system (e.g., a `KDTreeSpatialSystem`). You would simply create the new class, implement the `ISpatialSystem` interface, and the `Room` could start using it immediately with a small change to its construction logic.
*   **Clearer Intent**: The `ISpatialSystem` interface serves as explicit documentation. It clearly defines the essential capabilities that a spatial partitioning system must provide to the rest of the game engine.

## 5. Testing Guide

Since this is a pure refactoring, the goal of testing is to ensure no functionality has been broken.

1.  **Compile-Time Verification**: The first and most important test is to ensure the project still compiles. If you have correctly used the `override` keyword, the compiler will fail if your method signatures in the concrete classes do not exactly match the interface.
2.  **Re-run Existing Tests**: If you have unit tests for `GridSpatialSystem` and `OctreeSpatialSystem`, they should continue to pass without any changes, as the internal logic of the classes has not been altered.
3.  **Interface-Based Test**: Create a new test that verifies the polymorphic behavior.

    ```cpp
    #include <gtest/gtest.h>
    #include "game/systems/i_spatial_system.h"
    #include "game/systems/grid_spatial_system.h"
    #include "game/systems/octree_spatial_system.h"

    // Helper function that takes the interface
    void TestQuery(const ISpatialSystem& system) {
        // This function doesn't know or care if it's a grid or octree
        auto entities = system.GetEntitiesInRadius({0,0,0}, 10.0f);
        // ... perform assertions
    }

    TEST(SpatialInterfaceTest, PolymorphicCalls) {
        // Create instances of the concrete types
        GridSpatialSystem grid_system;
        OctreeSpatialSystem octree_system;

        // Pass them to the helper function via the interface
        TestQuery(grid_system);
        TestQuery(octree_system);
    }
    ```
    This test proves that both systems can be used interchangeably through the common `ISpatialSystem` interface.
