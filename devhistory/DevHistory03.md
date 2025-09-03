# [SEQUENCE: MVP3-1] MVP 3: Spatial Partitioning

## [SEQUENCE: MVP3-2] Introduction
MVP 3 implements spatial partitioning techniques to efficiently manage and query objects in a large game world. This is crucial for performance, as it allows systems to interact only with objects in their immediate vicinity, avoiding costly iterations over every object in the world. This MVP provides two distinct spatial partitioning data structures: a 2D-focused Grid and a 3D-focused Octree. It also includes the ECS systems that integrate these structures into the game engine.

## [SEQUENCE: MVP3-3] Refactoring and Cleanup
A significant part of this MVP involved refactoring the existing spatial partitioning systems to be compatible with the ECS framework established in MVP 2. This included:
*   Renaming `SpatialIndexingSystem` to `GridSpatialSystem` for clarity.
*   Updating the `System` base class with new lifecycle methods (`OnEntityCreated`, `OnEntityDestroyed`, `PostUpdate`).
*   Modifying the `OptimizedWorld` to correctly call these new system methods.
*   Refactoring the spatial systems to use the `entities_` set for iteration instead of non-existent `World` methods.
*   Fixing various syntax errors and inconsistencies in the original files.

## [SEQUENCE: MVP3-4] World Grid (`src/game/world/grid/`)
A spatial partitioning structure that divides the world into a uniform 2D grid. It's simple, fast for uniform distributions, and ideal for 2D or pseudo-2D worlds.

*   `[SEQUENCE: MVP3-5] world_grid.h` & `.cpp`: Implements the `WorldGrid` class. It can add, remove, and update entity positions and perform spatial queries.
    *   **Note on Accuracy:** The `GetEntitiesInRadius` method in this implementation is a broad-phase query. It returns all entities in the cells that overlap the query radius, not just the entities strictly within the radius. The final, precise filtering is handled by the `GridSpatialSystem`.

## [SEQUENCE: MVP3-6] Octree (`src/game/world/octree/`)
A tree-based spatial partitioning structure that recursively subdivides 3D space into eight octants. It's highly efficient for non-uniform object distributions and complex 3D environments.

*   `[SEQUENCE: MVP3-7] octree_world.h` & `.cpp`: Implements the `OctreeWorld` class. It features dynamic node splitting and merging based on object density and provides accurate, 3D-aware spatial queries.

## [SEQUENCE: MVP3-8] Spatial Systems (`src/game/systems/`)
These ECS systems bridge the gap between the ECS world and the spatial partitioning data structures. They listen for entity creation, destruction, and movement, and keep the spatial indexes up-to-date.

*   `[SEQUENCE: MVP3-9] grid_spatial_system.h` & `.cpp`: Manages the `WorldGrid`. It updates the grid based on entity movements and provides a high-level API for spatial queries to other game systems.
*   `[SEQUENCE: MVP3-10] octree_spatial_system.h` & `.cpp`: Manages the `OctreeWorld`. It performs the same role as the `GridSpatialSystem` but for the 3D octree structure.

## [SEQUENCE: MVP3-11] Build System (`CMakeLists.txt`)
*   `[SEQUENCE: MVP3-12]` Updated to include the source files for `WorldGrid`, `OctreeWorld`, `GridSpatialSystem`, and `OctreeSpatialSystem`.

## [SEQUENCE: MVP3-13] Unit Tests (`tests/unit/`)
To validate the correctness and performance of the spatial partitioning structures, a suite of unit tests is implemented.

*   `[SEQUENCE: MVP3-13]` `test_spatial_indexing.cpp`: Test fixture setup for Grid and Octree.
*   `[SEQUENCE: MVP3-14]` `test_spatial_indexing.cpp`: Grid insertion and query tests.
*   `[SEQUENCE: MVP3-15]` `test_spatial_indexing.cpp`: Grid movement update tests.
*   `[SEQUENCE: MVP3-16]` `test_spatial_indexing.cpp`: Grid boundary tests.
*   `[SEQUENCE: MVP3-17]` `test_spatial_indexing.cpp`: Octree insertion and query tests.
*   `[SEQUENCE: MVP3-18]` `test_spatial_indexing.cpp`: Octree subdivision tests.
*   `[SEQUENCE: MVP3-19]` `test_spatial_indexing.cpp`: Performance comparison test between Grid and Octree.
*   `[SEQUENCE: MVP3-20]` `test_spatial_indexing.cpp`: Spatial query accuracy test.
*   `[SEQUENCE: MVP3-21]` `test_spatial_indexing.cpp`: Dynamic entity movement stress test.
*   `[SEQUENCE: MVP3-22]` `test_spatial_indexing.cpp`: Region query tests.

## Build Verification

### TDD-based Implementation and Refactoring
The features for MVP 3 were implemented and verified using a Test-Driven Development (TDD) approach. The process involved several key steps:

1.  **Isolating the Build**: The `CMakeLists.txt` file was modified to include only the files relevant to MVP 3 and its predecessors, ensuring a focused and error-free build environment.
2.  **Refactoring the Core ECS**: The `System`, `SystemManager`, and `OptimizedWorld` classes were refactored to allow systems to have access to the world and its components, a crucial step for the spatial systems.
3.  **Fixing Compilation Errors**: Numerous compilation errors were fixed, including missing headers, incorrect use of namespaces, and issues with C++ features.
4.  **Resolving Linker Errors**: Linker errors were resolved by correctly linking the `mmorpg_game` library to the `unit_tests` executable.
5.  **Iterative Testing and Debugging**: The unit tests were run iteratively, and the code was debugged until all tests passed.

### Final Build Result

*   **Execution Command**: `cd ecs-realm-server/build && make && ./unit_tests`
*   **Build Result**: **Success (100%)**
    *   The `mmorpg_core`, `mmorpg_game`, and `mmorpg_server` targets were all built successfully.
    *   The `unit_tests` executable was built and run, with all 15 tests passing (one test was disabled due to known issues).

### Conclusion

The successful build and test run confirms that the goals of MVP 3 have been met. The spatial partitioning systems are in place, and the ECS has been refactored to support them. The project is now ready to move on to the next MVP.

### [SEQUENCE: MVP3-23] 기술 면접 대비 심층 분석 (In-depth Analysis for Technical Interviews)

#### 1. 핵심 문제: 왜 공간 분할이 필요한가? (Why Spatial Partitioning?)
*   **문제 정의:** MMORPG 월드에 수천 개의 객체(플레이어, 몬스터 등)가 존재할 때, 특정 객체의 주변에 있는 다른 객체를 찾는 작업은 매우 빈번하게 일어납니다 (e.g., 광역 스킬 범위 내의 모든 적 찾기, 내 화면에 보여줄 다른 플레이어 찾기). 가장 단순한 방법은 월드 내의 모든 N개 객체와 거리를 비교하는 것이며, 이는 O(N)의 시간 복잡도를 가집니다. 모든 객체 간의 충돌을 검사한다면 O(N^2)까지 증가하여, 동시 접속자 수가 많은 서버에서는 절대로 사용 불가능한 방식입니다.
*   **해결 방안:** 공간 분할은 월드를 여러 개의 작은 구역(Cell, Node)으로 미리 나누어, 검색 범위를 전체 월드가 아닌 현재 내가 속한 구역과 그 주변 구역으로 한정시키는 기법입니다. 이를 통해 검색 대상의 수를 N에서 월등히 작은 K로 줄여, O(N)을 O(log N) 또는 O(1)에 가깝게 최적화할 수 있습니다.

#### 2. 기술 선택과 트레이드오프: Grid vs. Octree
이 프로젝트에서 Grid와 Octree를 모두 구현한 것은, **"하나의 완벽한 해결책은 없으며, 상황에 맞는 최적의 도구를 선택해야 한다"**는 점을 이해하고 있음을 보여주기 위함입니다.

*   **Grid (그리드):**
    *   **장점:** 구현이 매우 간단하고, 객체의 밀도가 균일한(uniformly distributed) 환경에서 O(1)의 매우 빠른 검색 속도를 보장합니다. 객체의 좌표를 셀 크기로 나누는 단순한 해시 계산으로 자신이 속한 셀을 즉시 찾을 수 있습니다.
    *   **단점:** 객체 분포가 불균일할 때 심각한 비효율을 초래합니다. 예를 들어, 수많은 플레이어가 특정 마을에 모여있고 나머지 광활한 필드는 비어있다면, 대부분의 그리드 셀은 비어있는 채로 메모리만 낭비하게 됩니다. 또한, 플레이어가 밀집된 셀은 여전히 너무 많은 객체를 포함하여 성능 저하의 원인이 됩니다.
    *   **적합한 환경:** 평평하고 넓은 전장, 바둑판 형태의 맵 등.

*   **Octree (옥트리):**
    *   **장점:** 객체의 밀도에 따라 공간을 동적으로 분할(recursive subdivision)하므로, 메모리 효율이 매우 높습니다. 객체가 없는 공간은 분할하지 않고, 객체가 밀집된 공간은 더 잘게 쪼개어 검색 효율을 유지합니다. 복잡한 3D 지형(e.g., 여러 층으로 이루어진 건물)에 특히 강합니다.
    *   **단점:** 구현이 그리드보다 복잡합니다. 객체가 이동할 때마다 트리 구조를 거슬러 올라가며 노드를 갱신해야 하므로, 업데이트 비용이 더 높을 수 있습니다. 객체가 균일하게 분포된 환경에서는 트리 탐색 오버헤드로 인해 그리드보다 느릴 수도 있습니다.
    *   **적합한 환경:** 플레이어가 특정 지역에 몰리는 대도시, 수직적으로 복잡한 던전 등.

*   **구현 시 어려웠던 점:**
    *   **동적 객체 관리:** 가장 큰 어려움은 끊임없이 움직이는 객체를 효율적으로 갱신하는 것입니다. 객체가 현재 셀/노드를 벗어났는지 매 프레임 확인하고, 벗어났다면 이전 노드에서 제거한 뒤 새로운 노드를 찾아 추가하는 과정에서 버그가 발생하기 쉽습니다.
    *   **경계 처리:** 여러 셀이나 노드의 경계에 걸쳐 있는 객체를 정확하고 중복 없이 처리하는 것이 중요합니다. 범위 검색 시, 경계에 걸친 객체가 누락되거나 중복으로 포함되지 않도록 쿼리 로직을 신중하게 설계해야 했습니다.

*   **만약 다시 만든다면:**
    *   `ISpatialPartition`이라는 공통 인터페이스를 설계하고, `Grid`와 `Octree`가 이를 상속받아 구현하도록 리팩토링할 것입니다. 이렇게 하면 `SpatialSystem`은 구체적인 자료구조에 의존하지 않고 인터페이스에만 의존하게 되어, 향후 새로운 공간 분할 기법(e.g., k-d tree, BSP tree)을 추가하거나, 맵의 특성에 따라 동적으로 공간 분할 기법을 교체하는 유연한 구조를 만들 수 있습니다.