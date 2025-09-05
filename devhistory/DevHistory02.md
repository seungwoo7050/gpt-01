# [SEQUENCE: MVP2-1] MVP 2: Entity Component System (Data-Oriented)

## [SEQUENCE: MVP2-2] Introduction
*This document has been rewritten to detail the successful, from-scratch implementation of the data-oriented ECS framework, replacing the previous non-functional placeholder code.*

MVP 2 introduces the core architectural pattern for managing all game state: the Entity Component System (ECS). This implementation uses a **Data-Oriented** design to achieve maximum performance, which is critical for a large-scale MMORPG server. By separating data (Components) into tightly packed, contiguous arrays from logic (Systems), we enable cache-friendly batch processing of entities, leading to significant performance gains.

## Core ECS Framework (`src/core/ecs/`)

*   **[SEQUENCE: MVP2-3] `health_component.h`:** Defines the `HealthComponent` for any entity that can take damage.
*   **[SEQUENCE: MVP2-4] `transform_component.h`:** Defines `TransformComponent` and `VelocityComponent` for entity spatial state.
*   **[SEQUENCE: MVP2-5] `types.h`:** Defines the basic types for the ECS (`EntityId`, `ComponentTypeId`, `ComponentMask`).
*   **[SEQUENCE: MVP2-6] `system.h`:** Defines the abstract `System` base class.
*   **[SEQUENCE: MVP2-7] `component_manager.h`:** Implements the `ComponentManager`, which uses `ComponentArray`s to store all component data in a Structure-of-Arrays (SoA) layout. It is responsible for mapping entities to their components.
*   **[SEQUENCE: MVP2-8] `system_manager.h`:** Implements the `SystemManager`, which manages the lifecycle of all systems and updates their entity lists based on component signature changes.
*   **[SEQUENCE: MVP2-9] `dense_entity_manager.h`:** Defines the `DenseEntityManager` class.
*   **[SEQUENCE: MVP2-10] `dense_entity_manager.cpp`:** Implements the `Create` method, which efficiently recycles old entity IDs from a free list.
*   **[SEQUENCE: MVP2-11] `dense_entity_manager.cpp`:** Implements the `Destroy` method, which returns an entity ID to the free list.
*   **[SEQUENCE: MVP2-12] `optimized_world.h`:** Defines the `OptimizedWorld`, the central facade for all ECS operations.
*   **[SEQUENCE: MVP2-13] `optimized_world.cpp`:** Implements the constructor, wiring up the entity, component, and system managers.
*   **[SEQUENCE: MVP2-14] `optimized_world.cpp`:** Implements entity lifecycle methods (`CreateEntity`, `DestroyEntity`).
*   **[SEQUENCE: MVP2-15] `optimized_world.cpp`:** Implements the main `Update` loop, which drives all registered systems.

## Build & Test (`CMakeLists.txt`, `tests/unit/`)

*   **[SEQUENCE: MVP2-16] `CMakeLists.txt`:** The build system is configured to compile the new ECS framework files as part of the `mmorpg_core` library.
*   **[SEQUENCE: MVP2-17] `test_ecs_system.cpp`:** Unit test for entity creation and destruction.
*   **[SEQUENCE: MVP2-18] `test_ecs_system.cpp`:** Unit test for entity ID recycling.
*   **[SEQUENCE: MVP2-19] `test_ecs_system.cpp`:** Unit test for component management (add/get/remove).
*   **[SEQUENCE: MVP2-20] `test_ecs_system.cpp`:** Unit test for basic system updates.
*   **[SEQUENCE: MVP2-21] `test_ecs_system.cpp`:** Unit test verifying that systems correctly process entities based on their component signatures.

## [SEQUENCE: MVP2-22] Build Verification

### TDD and Debugging Journey
The initial code for MVP 2 was non-existent and non-functional. The entire ECS framework was implemented from scratch following Test-Driven Development (TDD) principles. The journey involved:
1.  **Build System Hell:** A significant effort was spent debugging the `conan` and `cmake` configuration. This involved fixing dependency version conflicts (`zlib`), removing unneeded future dependencies (`mysql`, `redis`, `sol2`), and correcting the `CMakeLists.txt` to properly locate the conan toolchain file and disable unused build targets (`mmorpg_game`, `load_test_client`).
2.  **Framework Implementation:** The core classes (`ComponentManager`, `SystemManager`, `DenseEntityManager`, `OptimizedWorld`) were created from scratch to fulfill the data-oriented design specified in the documentation.
3.  **Unit Test Implementation:** The `test_ecs_system.cpp` file was completely rewritten to test the new framework.
4.  **Iterative Debugging:** The initial build after implementation revealed several compilation errors (`unused-parameter` warnings treated as errors) and a critical logic bug. The `SystemEntityProcessing` test failed because the `GetEntitySignature` method was a placeholder. This was fixed by implementing proper signature tracking in the `ComponentManager`.

### Final Build and Test Result
*   **Execution Command:** `conan install ... && cmake ... && make ... && ctest ...`
*   **Result:** **100% SUCCESS.** All builds completed, and all 5 unit tests in the `ECSSystemTest` suite passed.

### Conclusion
This successful outcome validates that the newly implemented data-oriented ECS is robust, functional, and correctly configured. MVP 2 is now a stable and verified foundation upon which all future game mechanics will be built.

## [SEQUENCE: MVP2-23] In-depth Analysis for Technical Interviews

*   **Why Data-Oriented ECS?:** For an MMORPG that needs to simulate thousands of entities, CPU cache performance is paramount. A traditional Object-Oriented approach (`Array of Structures`) leads to frequent cache misses when updating game state. Our data-oriented `Structure of Arrays` approach ensures that when a system (e.g., `MovementSystem`) runs, all the data it needs (all `Position` and `Velocity` components) are contiguous in memory. This maximizes cache hits and provides a massive performance boost.
*   **Implementation Deep Dive - ComponentManager:** The core of our design is the `ComponentManager`. It uses a `std::unordered_map<const char*, std::shared_ptr<IComponentArray>>` to map a component's `typeid().name()` to a generic component array interface. The actual components are stored in a templated `ComponentArray<T>`, which is a packed array. When a component is removed, we use the swap-and-pop idiom to maintain a dense array, which is crucial for cache-friendly iteration.
