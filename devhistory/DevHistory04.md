# [SEQUENCE: MVP4-1] MVP 4: Combat Systems

## [SEQUENCE: MVP4-2] Introduction
MVP 4 implements the foundational combat systems, a core element of any MMORPG. This MVP focuses on refactoring the existing combat systems to align with the ECS architecture established in the previous MVPs. The goal is to create a flexible and extensible combat framework that can support both targeted and action-based combat styles.

## [SEQUENCE: MVP4-3] Architectural Revision: Abandoning `src/game/combat/`
During the discovery phase, a collection of files was found in `src/game/combat/` that represented a non-ECS, singleton-based combat architecture. This design was incompatible with the project's established ECS-based direction. A decision was made to abandon these files in favor of refactoring the existing ECS-native combat systems in `src/game/systems/combat/`. This ensures a consistent and maintainable architecture.

## [SEQUENCE: MVP4-4] Combat Components (`src/game/components/`)
The data for combat-related entities was refactored and consolidated into a set of pure-data components.

*   `[SEQUENCE: MVP4-5] combat_stats_component.h`: A comprehensive, data-only component for storing all combat-related statistics, such as attack power, defense, critical hit chance, etc.
*   `[SEQUENCE: MVP4-6] skill_component.h`: Manages an entity's skills, cooldowns, and casting state. The `is_attacking` and `last_attack_time` states were moved here from the deprecated `CombatComponent`.
*   `[SEQUENCE: MVP4-7] target_component.h`: Holds all data related to an entity's target, such as the current target's ID and other targeting-related states.
*   `[SEQUENCE: MVP4-8] projectile_component.h`: A new component to represent a projectile in the world. It stores information like the owner, velocity, range, and damage.
*   `[SEQUENCE: MVP4-9] dodge_component.h`: A new component to manage the state of an entity's dodge roll, including its duration and invulnerability frames.
*   `[SEQUENCE: MVP4-10] combat_component.h`: This component was deprecated. Its logic was removed, and its state variables were moved to more appropriate components (`SkillComponent`, `TargetComponent`).

## [SEQUENCE: MVP4-11] Combat Systems (`src/game/systems/combat/`)
The combat systems were heavily refactored to be compatible with the ECS framework.

*   `[SEQUENCE: MVP4-12] targeted_combat_system.h` & `.cpp`: This system handles traditional tab-target combat. It was refactored to:
    *   Work with the established ECS `World` and `SystemManager`.
    *   Iterate over its `entities_` set in the `Update` method.
    *   Remove dependencies on the old, non-existent ECS methods.
    *   Contain the damage calculation logic previously found in `CombatComponent`.
*   `[SEQUENCE: MVP4-13] action_combat_system.h` & `.cpp`: This system handles non-targeting, action-based combat. It was refactored to:
    *   Also work with the new ECS framework.
    *   Use the new `ProjectileComponent` and `DodgeComponent` to manage state, instead of internal maps.
    *   Create projectiles as separate entities.
    *   Iterate over its `entities_` set for updates.

## [SEQUENCE: MVP4-14] Build System (`CMakeLists.txt`)
*   `[SEQUENCE: MVP4-15]` Updated to include the new combat system source files in the `mmorpg_game` library.
