# [SEQUENCE: MVP5-1] MVP 5: Guild & PvP Systems

## [SEQUENCE: MVP5-2] Introduction
MVP 5 introduces large-scale social and competitive gameplay systems. This includes the framework for guilds, guild wars, and various Player-vs-Player (PvP) modes. The focus of this MVP was to refactor the existing, complex systems to be compatible with the established ECS architecture, while acknowledging their manager-style design.

## [SEQUENCE: MVP5-3] Architectural Approach: Manager Systems
The guild and PvP systems were found to be architecturally different from the pure ECS systems developed in earlier MVPs. They were designed as high-level managers with significant internal state, rather than systems that operate on component data in a tight loop. A pragmatic decision was made to treat them as "manager systems." This involved making the minimal changes necessary for them to compile and run within the ECS framework, without undertaking a full architectural rewrite. This approach allows for their functionality to be preserved while maintaining overall system stability.

## [SEQUENCE: MVP5-4] Guild & PvP Components (`src/game/components/`)
The data for guild and PvP functionality was refactored into a set of pure-data components.

*   `[SEQUENCE: MVP5-5] guild_component.h`: Stores an entity's guild information, such as guild ID, rank, and war status. Logic was removed from the component.
*   `[SEQUENCE: MVP5-6] pvp_stats_component.h`: A data-only component for tracking a player's PvP statistics, including kills, deaths, rating, and seasonal data. A helper function was moved to a new utility file.
*   `[SEQUENCE: MVP5-7] match_component.h`: Defines the data for a PvP match instance. Duplicated enums were removed.
*   `[SEQUENCE: MVP5-8] pvp_zone_component.h`: A new component, extracted from `match_component.h`, to define a PvP-enabled zone in the world.
*   `[SEQUENCE: MVP5-9] pvp_state_component.h`: A new component, extracted from `openworld_pvp_system.h`, to track an individual player's PvP state, such as whether they are flagged for PvP.

## [SEQUENCE: MVP5-10] Guild Systems (`src/game/systems/guild/`)
These systems manage the logic for guild wars. They were refactored to be compatible with the ECS framework.

*   `[SEQUENCE: MVP5-11] guild_war_instanced_system.h` & `.cpp`: Manages instanced guild wars. Refactored to remove incompatible methods and to use the `world_` member variable instead of `GetWorld()`.
*   `[SEQUENCE: MVP5-12] guild_war_seamless_system.h` & `.cpp`: Manages guild wars that occur in the main game world. Also refactored for ECS compatibility.

## [SEQUENCE: MVP5-13] PvP Systems (`src/game/systems/pvp/`)
These systems manage the logic for various PvP modes.

*   `[SEQUENCE: MVP5-14] openworld_pvp_system.h` & `.cpp`: Manages open-world PvP, including flagging and factional warfare. It was refactored to use the new `PvPStateComponent` and to be compatible with the ECS framework.
*   `[SEQUENCE: MVP5-15] arena_system.h` & `.cpp`: Placeholder files were created for a future arena system, as it was referenced in the project but the files were missing.

## [SEQUENCE: MVP5-16] New Utilities (`src/game/utils/`)
*   `[SEQUENCE: MVP5-17] pvp_utils.h`: A new utility file created to house the `GetRankFromRating` function, separating it from component data.

## [SEQUENCE: MVP5-18] Build System (`CMakeLists.txt`)
*   `[SEQUENCE: MVP5-19]` Updated to include the source files for the guild and PvP systems in the `mmorpg_game` library.
