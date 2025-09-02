## MVP 9: Full Combat Systems

This MVP represents a major deepening of the game's core mechanics, focusing entirely on building out the rich, interconnected systems that create strategic and engaging combat. This goes far beyond simple damage exchange, introducing status effects, complex AI, skills, items, and character progression.

### Core Combat Logic

#### `src/game/systems/combat_system.h` & `.cpp`
The foundation of all combat interactions.
```cpp
// [SEQUENCE: MVP9-1] Combat system handles attack resolution and damage
class CombatSystem : public core::ecs::System {
public:
    // [SEQUENCE: MVP9-2] System lifecycle
    // [SEQUENCE: MVP9-3] Update combat state
    // [SEQUENCE: MVP9-4] System metadata
    // [SEQUENCE: MVP9-5] Combat actions
    // [SEQUENCE: MVP9-6] Process single entity combat
    // [SEQUENCE: MVP9-7] Execute attack
    // [SEQUENCE: MVP9-8] Check if target is in range
    // [SEQUENCE: MVP9-9] Apply damage to target
};

// --- Implementation ---
// [SEQUENCE: MVP9-10] System initialization
// [SEQUENCE: MVP9-11] System shutdown
// [SEQUENCE: MVP9-12] Update all entities with combat logic
// [SEQUENCE: MVP9-13] Process combat for a single entity
// [SEQUENCE: MVP9-14] Start an attack
// [SEQUENCE: MVP9-15] Stop an attack
// [SEQUENCE: MVP9-16] Execute a single attack
// [SEQUENCE: MVP9-17] Check attack range
// [SEQUENCE: MVP9-18] Apply damage to a target
```

### Skill & Status Effect Systems

#### `src/game/skills/skill_system.h` & `.cpp`
Manages character abilities, including casting, cooldowns, and resource costs.
```cpp
// [SEQUENCE: MVP9-19] Skill system for managing abilities
class SkillSystem : public core::ecs::System {
public:
    // [SEQUENCE: MVP9-20] Update active skill casts and cooldowns
    // [SEQUENCE: MVP9-21] Use a skill
    // [SEQUENCE: MVP9-22] Process a single skill cast
    // [SEQUENCE: MVP9-23] Apply the effects of a skill
};

// --- Implementation ---
// [SEQUENCE: MVP9-24] Update loop for the skill system
// [SEQUENCE: MVP9-25] Attempt to use a skill
// [SEQUENCE: MVP9-26] Process an ongoing cast
// [SEQUENCE: MVP9-27] Apply the skill's effect
```

#### `src/game/status/status_effect_system.h` & `.cpp`
Handles all buffs, debuffs, and damage-over-time effects.
```cpp
// [SEQUENCE: MVP9-28] Status effect system for buffs and debuffs
class StatusEffectSystem : public core::ecs::System {
public:
    // [SEQUENCE: MVP9-29] Update active effects, check durations
    // [SEQUENCE: MVP9-30] Apply a status effect to an entity
    // [SEQUENCE: MVP9-31] Remove a status effect from an entity
    // [SEQUENCE: MVP9-32] Process effects for a single entity
};

// --- Implementation ---
// [SEQUENCE: MVP9-33] Update loop for status effect system
// [SEQUENCE: MVP9-34] Process effects for a single entity
// [SEQUENCE: MVP9-35] Apply a new status effect
// [SEQUENCE: MVP9-36] Remove a status effect
```

### AI and Character Progression

#### `src/game/ai/ai_behavior_system.h` & `.cpp`
Updates all AI entities, allowing them to react to the world and engage in combat.
```cpp
// [SEQUENCE: MVP9-37] AI Behavior System to update all AI entities
class AiBehaviorSystem : public core::ecs::System {
public:
    // [SEQUENCE: MVP9-38] Update loop for AI logic
    // [SEQUENCE: MVP9-39] Process the behavior tree for a single AI entity
};

// --- Implementation ---
// [SEQUENCE: MVP9-40] Update loop for the AI behavior system
// [SEQUENCE: MVP9-41] Process AI for a single entity
```

#### `src/game/systems/character_stats_system.h` & `.cpp`
Manages character leveling, experience gain, and the calculation of derived stats.
```cpp
// [SEQUENCE: MVP9-54] Character stats and leveling system
class CharacterStatsSystem : public core::ecs::System {
public:
    // [SEQUENCE: MVP9-55] Add experience to an entity
    // [SEQUENCE: MVP9-56] Recalculate all derived stats for an entity
};

// --- Implementation ---
// [SEQUENCE: MVP9-57] Add experience to an entity
// [SEQUENCE: MVP9-58] Recalculate all derived stats
```

### Item and Inventory Systems

#### `src/game/systems/item_system.h` & `.cpp`
Handles the logic for using items and applying their effects.
```cpp
// [SEQUENCE: MVP9-42] Item system for managing item lifecycle and effects
class ItemSystem : public core::ecs::System {
public:
    // [SEQUENCE: MVP9-43] Update items, e.g., for cooldowns on consumables
    // [SEQUENCE: MVP9-44] Use an item from an inventory
};

// --- Implementation ---
// [SEQUENCE: MVP9-45] Update loop for the item system
// [SEQUENCE: MVP9-46] Use an item
```

#### `src/game/systems/inventory_manager.h` & `.cpp`
Provides an API for manipulating entity inventories.
```cpp
// [SEQUENCE: MVP9-47] Inventory Manager to handle inventory operations
class InventoryManager {
public:
    // [SEQUENCE: MVP9-48] Add an item to an entity's inventory
    // [SEQUENCE: MVP9-49] Remove an item from an entity's inventory
    // [SEQUENCE: MVP9-50] Check if an entity has a specific item
};

// --- Implementation ---
// [SEQUENCE: MVP9-51] Add an item to an inventory
// [SEQUENCE: MVP9-52] Remove an item from an inventory
// [SEQUENCE: MVP9-53] Check for an item in an inventory
```

### Advanced Combat Mechanics

#### `src/game/systems/combo_system.h` & `.cpp`
Manages input sequences for executing powerful combo moves.
```cpp
// [SEQUENCE: MVP9-59] Combo system for handling input sequences
class ComboSystem : public core::ecs::System {
public:
    // [SEQUENCE: MVP9-60] Process player input for combos
    // [SEQUENCE: MVP9-61] Update combo states, check for timeouts
};

// --- Implementation ---
// [SEQUENCE: MVP9-62] Process player input for the combo system
// [SEQUENCE: MVP9-63] Update loop for the combo system
```

#### `src/game/systems/pvp_manager.h` & `.cpp`
Handles direct player-versus-player interactions like duels.
```cpp
// [SEQUENCE: MVP9-62] PvP Manager for handling player versus player interactions
class PvpManager {
public:
    // [SEQUENCE: MVP9-63] Handle a duel request
    // [SEQUENCE: MVP9-64] Update PvP states
};

// --- Implementation ---
// [SEQUENCE: MVP9-74] Handle a duel request
// [SEQUENCE: MVP9-75] Update PvP states
```

### 프로덕션 고려사항 (Production Considerations)
*   **데이터 관리 (Data-Driven Balancing):** 전투와 관련된 모든 수치(데미지 공식, 스킬 계수, 아이템 능력치, AI 행동 패턴 등)는 게임 밸런싱의 핵심이므로, 코드에서 분리하여 Lua 스크립트, JSON, 또는 데이터베이스에서 로드하는 데이터 기반 설계가 필수적입니다. 이를 통해 기획자는 서버 재컴파일 없이 실시간으로 게임 밸런스를 수정하고 테스트할 수 있습니다.
    ```lua
    -- 예시: data/skills/fireball.lua
    return {
        id = 101,
        name = "Fireball",
        damage_type = "FIRE",
        damage_coefficient = 1.8, -- 주문력의 180%
        mana_cost = 75,
        cast_time = 2.5,
        cooldown = 5.0,
        effect = "BURN",
        effect_duration = 6.0
    }
    ```
*   **API 및 운영 도구:** 전투 상황을 테스트하고 지원하기 위한 GM 명령어는 매우 중요합니다. `/godmode` (무적), `/kill <target>`, `/give_item <item_id>`, `/set_level <level>`, `/apply_buff <buff_id>` 등 특정 상황을 즉시 연출하는 기능이 필요합니다. 또한, 특정 플레이어의 상세 스탯, 인벤토리, 현재 적용 중인 효과 등을 실시간으로 조회하는 운영 툴이 있어야 합니다.
*   **성능 및 확장성:**
    *   **핫 경로 최적화 (Hot Path Optimization):** 데미지 계산, 스탯 조회 등 전투 중에 초당 수천, 수만 번 호출될 수 있는 핵심 로직(핫 경로)은 극도로 최적화되어야 합니다. 가상 함수 호출을 피하고, 메모리 접근을 최소화하는 등의 기법이 필요합니다.
    *   **스탯 캐싱:** 매번 모든 버프와 장비 효과를 종합하여 최종 스탯을 계산하는 대신, 상태 변경이 있을 때만 최종 스탯을 미리 계산하여 캐싱해두는 '스탯 시트' 방식은 서버 부하를 크게 줄여줍니다.
*   **보안 (Server-Authoritative Logic):** 전투는 모든 치팅 시도의 집결지이므로, 100% 서버 권위적 구조를 가져야 합니다.
    *   **입력과 결과의 분리:** 클라이언트는 "Fireball 스킬을 Target A에게 사용하고 싶다"는 **입력/의도**만을 서버에 보냅니다. 그러면 서버는 사거리, 쿨다운, 마나, 장애물(LoS) 등 모든 조건을 **검증**한 후, 데미지를 **계산**하고, 그 **결과**를 클라이언트에게 통보합니다. 클라이언트가 보낸 "10000 데미지를 입혔다"와 같은 결과값은 즉시 무시하고 해당 유저를 감시 목록에 올려야 합니다.
    *   **시뮬레이션과 검증:** 액션 전투의 투사체(Projectile)의 경우, 클라이언트는 예측을 위해 자체적으로 시뮬레이션하여 즉시 보여주되, 실제 피격 판정은 서버가 투사체의 경로를 시뮬레이션하고 충돌을 검증한 결과만을 인정해야 합니다. 이는 판정 조작 핵을 방지합니다.


