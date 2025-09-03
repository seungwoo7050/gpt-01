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

## [SEQUENCE: MVP4-16] Unit Tests (`tests/unit/`)
Unit tests for the combat systems to ensure correctness of the core logic.

*   `[SEQUENCE: MVP4-16]` `test_combat_system.cpp`: Test fixture for combat system tests.
*   `[SEQUENCE: MVP4-17]` `test_combat_system.cpp`: Basic damage calculation tests.
*   `[SEQUENCE: MVP4-18]` `test_combat_system.cpp`: Critical hit tests.
*   `[SEQUENCE: MVP4-19]` `test_combat_system.cpp`: Combat execution tests.
*   `[SEQUENCE: MVP4-20]` `test_combat_system.cpp`: Death handling tests.
*   `[SEQUENCE: MVP4-21]` `test_combat_system.cpp`: Skill cooldown tests.
*   `[SEQUENCE: MVP4-22]` `test_combat_system.cpp`: Combat state management tests.
*   `[SEQUENCE: MVP4-23]` `test_combat_system.cpp`: Damage mitigation tests.
*   `[SEQUENCE: MVP4-24]` `test_combat_system.cpp`: Area of effect damage tests.
*   `[SEQUENCE: MVP4-25]` `test_combat_system.cpp`: Healing tests.
*   `[SEQUENCE: MVP4-26]` `test_combat_system.cpp`: Combat immunity tests.
*   `[SEQUENCE: MVP4-27]` `test_combat_system.cpp`: Combo system tests.

## [SEQUENCE: MVP4-28] Build Verification

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

### [SEQUENCE: MVP4-29] 기술 면접 대비 심층 분석 (In-depth Analysis for Technical Interviews)

#### 1. 아키텍처 결정: 왜 싱글톤 기반 설계를 버렸는가? (Why Abandon the Singleton-based Design?)
*   **문제점:** 기존에 발견된 `src/game/combat/`의 싱글톤 기반 설계는 `CombatManager::GetInstance()->ProcessDamage(...)`와 같은 형태로 어디서든 호출될 수 있는 구조였습니다. 이는 다음과 같은 심각한 문제를 야기합니다.
    *   **강한 결합 (Tight Coupling):** 모든 코드가 `CombatManager`라는 구체적인 클래스에 직접 의존하게 됩니다. 이는 유닛 테스트를 거의 불가능하게 만들며(CombatManager를 Mocking 할 수 없음), 다른 전투 시스템으로 교체하는 것을 매우 어렵게 만듭니다.
    *   **숨겨진 의존성 (Hidden Dependencies):** 코드를 읽는 것만으로는 데이터가 어디서 어떻게 변하는지 파악하기 어렵습니다. `ProcessDamage` 함수가 내부적으로 어떤 다른 싱글톤(e.g., `PlayerManager`)을 건드리는지 알 수 없으므로, 시스템의 동작을 예측하고 디버깅하기가 매우 힘들어집니다.
    *   **전역 상태 (Global State):** 싱글톤은 사실상의 전역 변수입니다. 전역 상태는 프로그램의 복잡도를 기하급수적으로 높이며, 특히 멀티스레드 환경에서 데이터 경쟁(Data Race)의 주된 원인이 됩니다.
*   **해결책 (ECS):** ECS 아키텍처는 데이터(Components)와 로직(Systems)을 명확히 분리함으로써 이 문제를 해결합니다. 데이터의 변경은 오직 해당 데이터를 처리하는 'System'에 의해서만 발생합니다. 데이터 흐름이 `Component -> System -> Component`로 명확해지므로, 코드를 이해하고 테스트하기가 훨씬 쉬워집니다. 이 프로젝트의 방향성과 일치하는 ECS 기반으로 전투 시스템을 재구현하는 것은 당연한 결정이었습니다.

#### 2. 기술 선택과 트레이드오프: 컴포넌트 중심 설계
*   **설계 원칙:** 전투와 관련된 모든 상태(State)는 순수 데이터인 컴포넌트에 저장하고, 모든 로직(Logic)은 시스템에서 처리하도록 역할을 엄격하게 분리했습니다. 예를 들어, 기존 `CombatComponent`에 있던 `is_attacking` 같은 상태와 데미지 계산 로직을 분리하여, 상태는 `SkillComponent`로 옮기고 데미지 계산 로직은 `TargetedCombatSystem`으로 옮겼습니다.
*   **장점 (Pros):**
    *   **유연성:** 새로운 전투 메커니즘을 추가하기가 매우 쉽습니다. 예를 들어, '출혈' 효과를 추가하고 싶다면, 단순히 `BleedingComponent`(남은 시간, 초당 데미지 등)를 만들고, 이를 처리하는 `BleedingSystem`을 추가하기만 하면 됩니다. 기존 코드를 수정할 필요가 거의 없습니다.
    *   **재사용성:** `CombatStatsComponent`는 플레이어, 몬스터, 심지어 파괴 가능한 오브젝트에도 부착하여 재사용할 수 있습니다.
*   **단점 (Cons):**
    *   **초기 복잡성:** 특정 기능과 관련된 코드가 여러 파일(컴포넌트, 시스템)에 분산되어 있어, 처음에는 전체 구조를 파악하기 어려울 수 있습니다.
    *   **시스템 간 통신:** 한 시스템의 동작이 다른 시스템에 영향을 줘야 할 때(e.g., 전투 시스템이 체력을 깎으면, 사망 시스템이 이를 감지해야 함) 복잡성이 증가할 수 있습니다.

*   **구현 시 어려웠던 점:**
    *   **'갓 컴포넌트'의 유혹:** 처음에는 모든 전투 관련 데이터를 하나의 거대한 `CombatComponent`에 넣고 싶은 유혹이 있었습니다. 이를 의식적으로 피하고, 단일 책임 원칙(Single Responsibility Principle)에 따라 `TargetComponent`, `SkillComponent`, `CombatStatsComponent` 등으로 잘게 나누는 설계 과정이 중요했습니다.
    *   **액션 전투 구현:** 타겟팅 전투는 상태 변화의 연속이지만, 논타겟팅 액션 전투는 '발사체'라는 새로운 동적 엔티티를 생성하고, 이 엔티티가 물리 월드와 상호작용(충돌)하는 것을 처리해야 합니다. 이는 전투 시스템이 물리/충돌 시스템과 소통해야 함을 의미하며, 시스템 간의 상호작용을 더 복잡하게 만듭니다.

*   **만약 다시 만든다면:**
    *   **이벤트 시스템 도입:** 시스템 간의 직접적인 의존성을 더욱 줄이기 위해, 경량 이벤트 버스(Event Bus)를 도입할 것입니다. 예를 들어, `TargetedCombatSystem`이 데미지를 계산한 후 `HealthComponent`를 직접 수정하는 대신, `DamageDealtEvent`라는 이벤트를 발생시킵니다. 그러면 `HealthSystem`이 이 이벤트를 구독(subscribe)하여 해당 엔티티의 체력을 깎고, `LoggingSystem`은 이 이벤트를 받아 전투 로그를 기록하며, `SoundSystem`은 피격 사운드를 재생하는 등, 각 시스템이 완전히 분리(decoupled)되어 작동하게 만들 수 있습니다. 이는 훨씬 더 유연하고 확장성 높은 아키텍처가 될 것입니다.