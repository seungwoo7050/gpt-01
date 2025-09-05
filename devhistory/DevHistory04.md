# MVP 4: Combat System Refactoring

## Introduction

MVP 4 marks a crucial refactoring of the game's combat mechanics to align with the project's core ECS architecture. The primary goal was to move away from a legacy, singleton-based approach towards a flexible, data-driven design using components and systems. This creates a robust foundation for both traditional target-based combat and modern action-based combat, ensuring maintainability and extensibility.

This document details the logical progression of this refactoring, starting with the definition of data-oriented components, followed by the implementation of the systems that process them, integration into the build system, and finally, the creation of unit tests to validate the new architecture.

---

## Combat Components (`ecs-realm-server/src/game/components/`)

The first step was to define the data structures for all combat-related state.

*   `[SEQUENCE: MVP4-1]` `combat_stats_component.h`: A comprehensive, data-only component for storing all combat-related statistics.
*   `[SEQUENCE: MVP4-2]` `skill_component.h`: Manages an entity's skills, cooldowns, and casting state.
*   `[SEQUENCE: MVP4-3]` `target_component.h`: Holds all data related to an entity's target.
*   `[SEQUENCE: MVP4-4]` `projectile_component.h`: Represents a projectile in the world, storing its owner, trajectory, and combat properties.
*   `[SEQUENCE: MVP4-5]` `dodge_component.h`: Manages the state of an entity's dodge roll, including its duration and invulnerability frames.
*   `[SEQUENCE: MVP4-6]` `combat_component.h`: This component is deprecated. Its logic was removed, and its state variables were moved to more appropriate components like SkillComponent and TargetComponent.

---

## ECS Combat Systems (`ecs-realm-server/src/game/systems/combat/`)

Next, the systems were implemented to process the component data and enact combat logic.

### `targeted_combat_system.h`
*   `[SEQUENCE: MVP4-7]` Defines the system for handling traditional, target-based combat mechanics.
*   `[SEQUENCE: MVP4-8]` Public API for managing combat state, skills, and targets.
*   `[SEQUENCE: MVP4-9]` Private helper methods for internal combat logic.
*   `[SEQUENCE: MVP4-10]` Private member variables for system state and configuration.

### `targeted_combat_system.cpp`
*   `[SEQUENCE: MVP4-11]` Implements the destructor.
*   `[SEQUENCE: MVP4-12]` Implements the main update loop for the combat system.
*   `[SEQUENCE: MVP4-13]` Processes auto-attacks for all relevant entities.
*   `[SEQUENCE: MVP4-14]` Executes a single auto-attack from an attacker to a target.
*   `[SEQUENCE: MVP4-15]` Calculates the final damage based on attacker and defender stats.
*   `[SEQUENCE: MVP4-16]` Applies damage to a target's HealthComponent.
*   `[SEQUENCE: MVP4-17]` Applies healing to a target's HealthComponent.
*   `[SEQUENCE: MVP4-18]` Validates if a target is still attackable.
*   `[SEQUENCE: MVP4-19]` Checks if two entities are within a specified range.
*   `[SEQUENCE: MVP4-20]` Handles the death of an entity.
*   `[SEQUENCE: MVP4-21]` Handles the end of combat for an entity.
*   `[SEQUENCE: MVP4-22]` Clears the current target of an entity.
*   `[SEQUENCE: MVP4-23]` Handles the use of a skill by an entity.
*   `[SEQUENCE: MVP4-24]` Placeholder for processing skill casts.
*   `[SEQUENCE: MVP4-25]` Processes skill cooldowns for all entities.
*   `[SEQUENCE: MVP4-26]` Placeholder for updating target validation.
*   `[SEQUENCE: MVP4-27]` Handles the start of combat for an entity.
*   `[SEQUENCE: MVP4-28]` Placeholder for cleaning up invalid targets.
*   `[SEQUENCE: MVP4-29]` Checks if an entity is currently in combat.

### `action_combat_system.h`
*   `[SEQUENCE: MVP4-30]` Defines the system for handling action-oriented, non-target-based combat.
*   `[SEQUENCE: MVP4-31]` Public API for managing action combat, skills, and movement.
*   `[SEQUENCE: MVP4-32]` Private helper methods for internal action combat logic.
*   `[SEQUENCE: MVP4-33]` Private member variables for system state and configuration.

### `action_combat_system.cpp`
*   `[SEQUENCE: MVP4-34]` Implements the constructor and destructor.
*   `[SEQUENCE: MVP4-35]` Implements the main update loop for the action combat system.
*   `[SEQUENCE: MVP4-36]` Updates the position of all projectiles and removes them if they expire.
*   `[SEQUENCE: MVP4-37]` Checks for collisions between a projectile and other entities.
*   `[SEQUENCE: MVP4-38]` Calculates damage for action combat, similar to targeted combat.
*   `[SEQUENCE: MVP4-39]` Applies damage to a target, checking for invulnerability.
*   `[SEQUENCE: MVP4-40]` Validates if a target is valid for action combat.
*   `[SEQUENCE: MVP4-41]` Handles the event of a successful hit.
*   `[SEQUENCE: MVP4-42]` Handles the event of a successful dodge.
*   `[SEQUENCE: MVP4-43]` Handles the death of an entity in action combat.
*   `[SEQUENCE: MVP4-44]` Checks if an entity is invulnerable (e.g., during a dodge).
*   `[SEQUENCE: MVP4-45]` Placeholder for processing skill casts.
*   `[SEQUENCE: MVP4-46]` Placeholder for processing skill cooldowns.
*   `[SEQUENCE: MVP4-47]` Updates the state of dodging entities.
*   `[SEQUENCE: MVP4-48]` Recharges dodge charges over time.
*   `[SEQUENCE: MVP4-49]` Implements the logic for an area-of-effect skill.
*   `[SEQUENCE: MVP4-50]` Implements the logic for a melee swing.

---

## Build System (`ecs-realm-server/CMakeLists.txt`)

*   `[SEQUENCE: MVP4-51]` Adds the combat system source files to the mmorpg_game library.
*   `[SEQUENCE: MVP4-52]` Adds the combat system unit tests to the test executable.

---

## Unit Tests (`ecs-realm-server/tests/unit/test_combat_system.cpp`)

*   `[SEQUENCE: MVP4-53]` Defines the test fixture for all combat system tests.
*   `[SEQUENCE: MVP4-54]` Tests basic damage calculation.
*   `[SEQUENCE: MVP4-55]` Tests critical hit damage calculation.
*   `[SEQUENCE: MVP4-56]` Tests that combat proceeds over multiple updates.
*   `[SEQUENCE: MVP4-57]` Tests that entities are correctly marked as dead.
*   `[SEQUENCE: MVP4-58]` Tests skill cooldown mechanics.
*   `[SEQUENCE: MVP4-59]` Tests entering and exiting the combat state.
*   `[SEQUENCE: MVP4-60]` Tests damage mitigation from armor.
*   `[SEQUENCE: MVP4-61]` Tests area-of-effect damage.
*   `[SEQUENCE: MVP4-62]` Tests healing functionality.
*   `[SEQUENCE: MVP4-63]` Tests combat immunity via damage reduction.
*   `[SEQUENCE: MVP4-64]` Tests a simulated combo system.

---

## Build Verification

After completing all the refactoring and implementation tasks for MVP 4, a full build and test cycle was performed to ensure the stability and correctness of the codebase. This included a clean build process to verify the Conan dependency management setup.

### Clean Build Process with Conan

Due to the project's reliance on Conan for dependency management, a specific procedure is required for a clean build:

1.  **Install Dependencies**: From the `ecs-realm-server` directory, run `conan install .`. This command reads the `conanfile.txt`, downloads the required dependencies, and generates the crucial `conan_toolchain.cmake` file in the same directory.

2.  **Configure CMake**: Create a `build` directory and navigate into it. From within the `build` directory, run the following command to configure the project:
    ```bash
    cmake .. -DCMAKE_TOOLCHAIN_FILE=../conan_toolchain.cmake
    ```
    This command explicitly tells CMake to use the toolchain file generated by Conan, which allows it to locate the necessary libraries like Boost.

3.  **Build and Test**: Once CMake has been configured, the project can be built and tested using the standard commands:
    ```bash
    make
    ./unit_tests
    ```

Following this procedure resulted in a successful build and a full pass of all 27 unit tests, confirming that the combat system refactoring and all related changes for MVP 4 were implemented correctly and without regressions.

---

## In-depth Analysis for Technical Interviews

#### 1. Architectural Decision: Why Abandon the Singleton-based Design?
*   **Problem:** The legacy singleton-based design found in `src/game/combat/` allowed for calls like `CombatManager::GetInstance()->ProcessDamage(...)` from anywhere. This created severe problems:
    *   **Tight Coupling:** All code became directly dependent on the concrete `CombatManager` class. This makes unit testing nearly impossible (you can't mock the CombatManager) and makes swapping to a different combat system extremely difficult.
    *   **Hidden Dependencies:** It was hard to understand how data changed just by reading the code. A function like `ProcessDamage` could internally touch other singletons (e.g., `PlayerManager`), making the system's behavior unpredictable and hard to debug.
    *   **Global State:** Singletons are effectively global variables. Global state exponentially increases program complexity and is a primary cause of data races in a multithreaded environment.
*   **Solution (ECS):** The ECS architecture solves this by clearly separating data (Components) from logic (Systems). Changes to data are only ever made by the Systems that process that data. The data flow is explicit (`Component -> System -> Component`), making the code much easier to understand and test. Re-implementing the combat system based on ECS, which aligns with the project's direction, was the clear choice.

#### 2. Technology Choices and Trade-offs: Component-Centric Design
*   **Design Principle:** All state related to combat was stored in pure-data Components, and all logic was handled in Systems, strictly separating their roles. For example, state like `is_attacking` and damage calculation logic from the old `CombatComponent` were separated: the state was moved to `SkillComponent`, and the damage calculation logic was moved to `TargetedCombatSystem`.
*   **Advantages (Pros):**
    *   **Flexibility:** It's very easy to add new combat mechanics. For instance, to add a 'bleed' effect, one would simply create a `BleedingComponent` (with remaining time, damage per tick, etc.) and a `BleedingSystem` to process it. Almost no existing code needs to be modified.
    *   **Reusability:** `CombatStatsComponent` can be attached to players, monsters, and even destructible objects to be reused.
*   **Disadvantages (Cons):**
    *   **Initial Complexity:** Code related to a specific feature is spread across multiple files (components, systems), which can make it difficult to grasp the overall structure at first.
    *   **Inter-System Communication:** Complexity can increase when the action of one system needs to affect another (e.g., when the combat system reduces health, the death system must detect it).

*   **Implementation Challenges:**
    *   **The Lure of the 'God Component':** Initially, there was a temptation to put all combat-related data into one giant `CombatComponent`. Consciously avoiding this and breaking it down into smaller pieces like `TargetComponent`, `SkillComponent`, and `CombatStatsComponent` according to the Single Responsibility Principle was a crucial design process.
    *   **Implementing Action Combat:** While targeted combat is a sequence of state changes, non-targeting action combat requires creating new dynamic entities ('projectiles') and handling their interaction with the physical world (collision). This means the combat system must communicate with the physics/collision system, making inter-system interaction more complex.

*   **If I Were to Rebuild It:**
    *   **Introduce an Event System:** To further reduce direct dependencies between systems, I would introduce a lightweight event bus. For example, instead of the `TargetedCombatSystem` directly modifying the `HealthComponent` after calculating damage, it would raise a `DamageDealtEvent`. The `HealthSystem` would then subscribe to this event to reduce the entity's health, a `LoggingSystem` could subscribe to record a combat log, and a `SoundSystem` could play a hit sound. This would allow each system to operate in a fully decoupled manner, leading to a much more flexible and scalable architecture.
