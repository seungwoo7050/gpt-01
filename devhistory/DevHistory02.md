# [SEQUENCE: MVP2-1] MVP 2: Entity Component System (ECS)

## [SEQUENCE: MVP2-2] Introduction
MVP 2 introduces the core architectural pattern for managing game state: the Entity Component System (ECS). This paradigm shift moves away from deep inheritance hierarchies towards a more flexible and data-oriented design. By separating data (Components) from logic (Systems), we can create more modular, reusable, and performant game objects. This MVP implements two versions of the ECS: a basic, easy-to-understand version and an optimized, data-oriented version designed for high-performance batch processing.

## [SEQUENCE: MVP2-3] Core ECS Framework (`src/core/ecs/`)
This directory contains the fundamental building blocks of the ECS.

### [SEQUENCE: MVP2-4] Basic ECS Implementation
A straightforward implementation focusing on clarity and correctness.

*   `[SEQUENCE: MVP2-5] types.h`: Defines fundamental ECS types like `EntityId`, `ComponentTypeId`, and `ComponentMask`.
*   `[SEQUENCE: MVP2-6] component.h`: Provides a mechanism for generating unique type IDs for each component type.
*   `[SEQUENCE: MVP2-7] entity.h` & `.cpp`: Defines `EntityManager` for creating, destroying, and managing entities and their component signatures.
*   `[SEQUENCE: MVP2-8] system.h`: Defines the base `System` class and `SystemManager` for registering systems and routing entities to them based on their component signatures.
*   `[SEQUENCE: MVP2-9] component_storage.h`: A template-based storage class using a sparse set for efficient component access.
*   `[SEQUENCE: MVP2-10] world.h` & `.cpp`: The central ECS coordinator, bringing together all the managers and providing a simple API for game logic.

### [SEQUENCE: MVP2-11] Optimized ECS Implementation (`src/core/ecs/optimized/`)
A data-oriented implementation designed for cache-friendly batch processing.

*   `[SEQUENCE: MVP2-12] component_array.h`: Similar to `ComponentStorage`, but designed for direct, contiguous memory access by systems.
*   `[SEQUENCE: MVP2-13] dense_entity_manager.h` & `.cpp`: Manages entities for the optimized world.
*   `[SEQUENCE: MVP2-14] optimized_world.h` & `.cpp`: The coordinator for the optimized ECS. It provides systems with direct access to component arrays, enabling high-performance, data-oriented processing.

## [SEQUENCE: MVP2-15] Game Components (`src/game/components/`)
These are the pure data building blocks that define our game entities.

*   `[SEQUENCE: MVP2-16] transform_component.h`: Stores an entity's position, rotation, and scale in 3D space.
*   `[SEQUENCE: MVP2-17] velocity_component.h`: Stores an entity's linear and angular velocity for movement calculations.
*   `[SEQUENCE: MVP2-18] health_component.h`: Stores an entity's current and maximum health, shield, and death state.
*   `[SEQUENCE: MVP2-19] tag_component.h`: Stores metadata about an entity, such as its name and type (e.g., Player, NPC, Monster).

## [SEQUENCE: MVP2-20] Game Systems (`src/game/systems/`)
These classes contain the logic that operates on entities with specific sets of components.

### [SEQUENCE: MVP2-21] Basic Systems
*   `[SEQUENCE: MVP2-22] movement_system.h` & `.cpp`: Updates entity positions based on their `TransformComponent` and `VelocityComponent`.
*   `[SEQUENCE: MVP2-23] health_regeneration_system.h` & `.cpp`: Implements passive health regeneration for entities with a `HealthComponent`.
*   `[SEQUENCE: MVP2-24] network_sync_system.h` & `.cpp`: Responsible for synchronizing entity state changes (like position and health) to clients.

### [SEQUENCE: MVP2-25] Optimized Systems (`src/game/systems/optimized/`)
*   `[SEQUENCE: MVP2-26] optimized_movement_system.h` & `.cpp`: A data-oriented version of the movement system that iterates directly over contiguous component arrays for maximum cache efficiency.

## [SEQUENCE: MVP2-27] Core Utilities (`src/core/utils/`)
*   `[SEQUENCE: MVP2-28] vector3.h`: A simple 3D vector class with basic arithmetic operations, used by many components and systems.

## [SEQUENCE: MVP2-29] Build System (`CMakeLists.txt`)
*   `[SEQUENCE: MVP2-30]` Updated to include all new source files for the basic and optimized ECS, components, and systems. Removed references to non-existent files.
