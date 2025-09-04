# [SEQUENCE: MVP7-1] MVP 7: Advanced Database & Distributed Systems

## [SEQUENCE: MVP7-2] Objective
This MVP focuses on building out the backend data systems to support large-scale concurrency and a distributed server architecture.

## [SEQUENCE: MVP7-3] Key Features
1.  **Advanced Database Systems:**
    *   Connection Pool & Manager
    *   Global Cache Manager
    *   Database Health Monitor
    *   Table Partitioning & Shard Managers
    *   Query Optimizer & Read Replica Manager
2.  **Distributed Lock Manager:** Implement a distributed lock system using Redis to ensure data consistency in a multi-server environment.

---

## [SEQUENCE: MVP7-16] 1. 데이터베이스 시스템 기반 (Foundational Database Systems)
대규모 데이터베이스 시스템을 구축하기 위한 첫 단계로, 핵심 기반 모듈인 커넥션 풀과 데이터베이스 모니터를 구현하고 안정화했습니다. 기존에 존재하던 소스 코드를 분석하고, 불완전했던 헤더 파일을 재구성하여 전체 모듈을 성공적으로 컴파일했습니다.

*   **`[SEQUENCE: MVP7-17] MySQL C++ Connector 의존성 추가` (`conanfile.txt`, `CMakeLists.txt`):**
    *   DB와 통신하기 위해 `mysql-connector-cpp/9.2.0` 의존성을 `conanfile.txt`에 추가했습니다.
    *   `CMakeLists.txt`를 수정하여 `find_package(mysql-concpp)`를 호출하고, `mmorpg_core` 라이브러리에 `mysql::concpp`를 링크했습니다.
    *   빌드 과정에서 `conan_toolchain.cmake` 파일이 올바르게 적용되도록 `CMakeLists.txt`에 `include(conan_toolchain.cmake)`를 추가하여 빌드 환경 문제를 해결했습니다.

*   **`[SEQUENCE: MVP7-6] 커넥션 풀 (Connection Pool)` (`connection_pool.h`, `connection_pool.cpp`):**
    *   **`PooledConnection`:** 실제 DB 커넥션(`mysqlx::Session`)을 래핑하고, 사용 상태(`ConnectionState`), 생명주기, 마지막 사용 시간 등의 메타데이터를 관리하는 클래스를 구현했습니다.
    *   **`ConnectionPool`:** `PooledConnection` 객체들을 관리하는 풀입니다. 설정에 따라 초기 커넥션을 생성하고, `Acquire`/`Release`를 통해 커넥션을 대여하고 반납하는 핵심 로직을 구현했습니다.
    *   **`ConnectionPoolManager`:** 여러 개의 커넥션 풀을 이름으로 관리할 수 있는 싱글톤 매니저를 구현하여, 향후 읽기 전용 DB와 쓰기 전용 DB에 대한 커넥션 풀을 분리하는 등의 확장을 용이하게 했습니다.

*   **`[SEQUENCE: MVP7-9] 데이터베이스 모니터 (Database Monitor)` (`db_monitor.h`, `db_monitor.cpp`):**
    *   **`DatabaseMonitor`:** `ConnectionPool`의 상태를 주기적으로 확인하는 모니터 클래스를 구현했습니다.
    *   별도의 모니터링 스레드를 생성하여, 주기적으로 커넥션 풀로부터 통계 정보를 가져오고 DB에 핑을 보내는 등 헬스 체크를 수행하는 `MonitoringLoop`를 구현했습니다.
    *   쿼리 이름과 실행 시간을 받아 성능 지표를 기록하는 `RecordQuery` 메서드를 구현했습니다.

### [SEQUENCE: MVP7-18] 기술 면접 대비 심층 분석 (In-depth Analysis)

*   **기술 선택 이유 (Why a Connection Pool?):**
    *   데이터베이스 커넥션을 생성하는 작업은 TCP/IP 핸드셰이크, 인증 등 많은 리소스를 소모하는 비싼 작업입니다. 매 쿼리마다 커넥션을 새로 생성하고 파괴하는 것은 서버에 큰 부하를 줍니다.
    *   커넥션 풀은 미리 일정량의 커넥션을 만들어두고, 필요할 때마다 빌려주고 반납받는 방식으로 재사용하여 이 오버헤드를 크게 줄입니다. 이는 대규모 요청을 처리해야 하는 서버의 응답 속도와 처리량을 극적으로 향상시키는 필수적인 기법입니다.
*   **구현 시 어려웠던 점:**
    *   **스레드 안정성:** `ConnectionPool`은 여러 스레드에서 동시에 접근하므로, `std::mutex`와 `std::condition_variable`을 사용한 동기화 처리가 매우 중요했습니다. 특히, 풀이 비어있을 때 `Acquire`를 호출한 스레드가 무작정 대기하지 않고, 다른 스레드가 `Release`를 호출하여 커넥션을 반납하면 즉시 깨어나도록(`notify_one`) 하는 로직을 정확히 구현하는 것이 핵심이었습니다.
    *   **"죽은" 커넥션 관리:** 오랫동안 사용되지 않거나(Idle), 네트워크 문제로 끊어진 커넥션을 풀에 그대로 두면 애플리케이션 전체의 오류로 이어질 수 있습니다. `Validate` 메서드에서 간단한 쿼리(`SELECT 1`)를 실행하여 커넥션이 여전히 유효한지 검사하고, 문제가 있는 커넥션은 풀에서 제거하는 로직을 추가하여 안정성을 높였습니다.
*   **만약 다시 만든다면:**
    *   현재 `ConnectionPool`의 `Acquire`는 커넥션이 없을 경우 정해진 시간만큼 대기(timeout)합니다. 더 나아가, 서킷 브레이커(Circuit Breaker) 패턴을 도입하는 것을 고려할 수 있습니다. 짧은 시간 동안 커넥션 획득 실패가 일정 횟수 이상 발생하면, 풀이 과부하 상태라고 판단하고 일정 시간 동안 모든 `Acquire` 요청을 즉시 실패 처리하여 DB 서버를 보호하고, 시스템이 스스로 회복할 시간을 주는 더 발전된 장애 대응 전략을 구현할 수 있습니다.

---

## [SEQUENCE: MVP7-22] 2. 글로벌 캐시 매니저 (Global Cache Manager)
데이터베이스의 부하를 줄이고 자주 사용되는 데이터에 대한 접근 속도를 높이기 위해, 인-메모리 캐시 시스템을 구현했습니다.

*   **`[SEQUENCE: MVP7-21] CacheManager` (`cache_manager.h`, `cache_manager.cpp`):**
    *   **`Cache`:** `std::unordered_map`을 사용하여 키-값 형태로 데이터를 저장하는 스레드 안전(thread-safe) 캐시 클래스를 구현했습니다. 각 캐시 항목(`CacheEntry`)은 값과 함께 마지막 접근 시간, TTL(Time-To-Live)을 메타데이터로 가집니다.
    *   **`CacheManager`:** 여러 개의 `Cache` 인스턴스를 이름으로 관리하는 싱글톤 매니저를 구현했습니다. `GetOrCreateCache("player_cache")`와 같은 형태로 특정 목적을 가진 캐시를 동적으로 생성하고 가져올 수 있습니다.
    *   **백그라운드 정리 스레드 (Eviction Thread):** `CacheManager`는 자체적인 백그라운드 스레드를 실행합니다. 이 스레드는 주기적으로 모든 캐시를 순회하며, 설정된 TTL이 지난 오래된 데이터를 자동으로 삭제하는 `EvictExpired` 로직을 수행합니다.

### [SEQUENCE: MVP7-23] 기술 면접 대비 심층 분석 (In-depth Analysis)

*   **기술 선택 이유 (Why a Cache?):**
    *   **성능 향상:** 데이터베이스 I/O는 메모리 접근에 비해 수천, 수만 배 느립니다. 플레이어 정보, 아이템 정보, 몬스터 스펙 등 자주 읽히지만 자주 바뀌지 않는 데이터를 메모리에 캐싱해두면, DB까지 요청이 도달하는 횟수를 크게 줄여 전체적인 서버 응답 속도를 비약적으로 향상시킬 수 있습니다.
    *   **DB 부하 감소:** 캐시가 많은 읽기 요청을 대신 처리해주므로, 데이터베이스는 더 적은 수의 요청, 특히 중요한 쓰기 작업에 리소스를 집중할 수 있습니다. 이는 DB 서버의 부하를 줄여 전체 시스템의 안정성과 확장성을 높입니다.
*   **구현 시 어려웠던 점:**
    *   **캐시 무효화 (Cache Invalidation):** 캐싱에서 가장 어려운 문제입니다. "언제 캐시를 지워야 하는가?"에 대한 문제입니다. 만약 DB의 원본 데이터가 변경되었는데 캐시의 복사본이 제때 삭제되거나 갱신되지 않으면, 유저는 낡은(stale) 데이터를 보게 됩니다. 이 구현에서는 간단한 TTL 기반의 삭제 정책을 사용했지만, 실제 프로덕션 환경에서는 DB 데이터가 변경될 때마다 관련 캐시를 명시적으로 삭제하는 'Write-through' 또는 'Write-around' 같은 더 정교한 전략이 필요합니다.
    *   **동시성 제어:** `CacheManager`와 각 `Cache` 인스턴스는 여러 스레드에서 동시에 접근될 수 있습니다. `std::mutex`를 사용하여 각 캐시의 데이터 접근을 보호하고, `CacheManager`의 캐시 목록 접근을 보호하여 스레드 안정성을 확보했습니다.
*   **만약 다시 만든다면:**
    *   **정교한 삭제 정책 구현:** 현재의 TTL 기반 정책은 "언젠가는 삭제된다"를 보장하지만, 실시간성은 떨어집니다. 더 나아가, LRU(Least Recently Used)나 LFU(Least Frequently Used) 같은 더 정교한 캐시 삭제 알고리즘을 구현하여, 제한된 메모리를 더 효율적으로 사용하도록 개선할 것입니다.
    *   **분산 캐시 고려:** 현재 캐시는 각 서버 인스턴스의 메모리 안에서만 유효합니다. 서버가 여러 대로 확장될 경우, Redis나 Memcached 같은 외부 분산 캐시 솔루션을 도입하는 것을 고려할 것입니다. 이렇게 하면 모든 서버 인스턴스가 동일한 캐시 데이터를 공유할 수 있어, 데이터 정합성을 유지하고 캐시 효율을 극대화할 수 있습니다.

---

## [SEQUENCE: MVP7-28] 3. 데이터베이스 스케일링: 파티셔닝과 샤딩 (DB Scaling: Partitioning & Sharding)
대용량 테이블의 성능 저하를 막고, 데이터베이스 부하를 여러 서버로 분산하는 스케일 아웃(Scale-out) 아키텍처의 기반을 마련했습니다.

*   **`[SEQUENCE: MVP7-26] 파티션 매니저 (Partition Manager)` (`db_partitioning.h`, `db_partitioning.cpp`):**
    *   애플리케이션 레벨에서 테이블 파티셔닝 전략을 관리하는 `PartitionManager`를 구현했습니다.
    *   테이블 이름과 파티셔닝 키(e.g., 사용자 ID)를 기반으로, 데이터가 실제로 저장되거나 조회되어야 할 파티션의 이름을 결정하는 로직(`GetPartitionForValue`)을 포함합니다. 이를 통해 `INSERT INTO logs_p202309 (...)` 와 같이 동적으로 쿼리를 생성할 수 있습니다.

*   **`[SEQUENCE: MVP7-27] 샤드 매니저 (Shard Manager)` (`shard_manager.h`, `shard_manager.cpp`):**
    *   데이터베이스 샤딩을 관리하는 `ShardManager`를 구현했습니다.
    *   샤딩 키(e.g., `player_id`)를 입력받아, 어떤 데이터베이스 샤드(서버)에 해당 데이터가 저장되어 있는지 결정하는 해시 기반의 라우팅 로직(`GetShardIndexForKey`)을 포함합니다.
    *   `ConnectionPoolManager`와 연동하여, 결정된 샤드 인덱스에 해당하는 커넥션 풀(`shard_0`, `shard_1` 등)을 가져오는 `GetPoolForKey` 메서드를 구현했습니다.

### [SEQUENCE: MVP7-29] 기술 면접 대비 심층 분석 (In-depth Analysis)

*   **기술 선택 이유 (Why Partitioning and Sharding?):**
    *   **파티셔닝 (수직적 확장):** 단일 DB 서버 내에서, 수억 건이 넘는 거대한 테이블(e.g., 전투 로그, 재화 로그)은 인덱스가 있더라도 검색 속도가 저하됩니다. 파티셔닝은 이 테이블을 월별, 또는 유저 ID 범위별 등 작은 단위로 물리적으로 분리하여, 쿼리가 전체 테이블이 아닌 특정 파티션만 스캔하도록 만듭니다. 이는 쿼리 성능을 크게 향상시킵니다.
    *   **샤딩 (수평적 확장):** 단일 데이터베이스 서버가 감당할 수 있는 부하(CPU, I/O, 커넥션 수)에는 물리적 한계가 있습니다. 샤딩은 유저 데이터 자체를 여러 개의 독립된 DB 서버(샤드)에 분산 저장하는 기술입니다. 예를 들어, 1~100만번 유저는 1번 DB, 101~200만번 유저는 2번 DB에 저장하는 방식입니다. 이를 통해 전체 시스템의 쓰기/읽기 부하를 여러 서버로 분산시켜, 이론상 무한한 확장이 가능한 아키텍처를 만들 수 있습니다.
*   **구현 시 어려웠던 점:**
    *   **샤딩 키(Sharding Key) 선정:** 어떤 키를 기준으로 데이터를 분산할지 결정하는 것이 가장 중요하고 어려운 문제입니다. 유저 ID처럼 데이터가 모든 샤드에 고르게 분포될 수 있는 키를 선택해야 합니다. 만약 국가나 서버 이름처럼 분포가 불균일한 키를 선택하면, 특정 샤드에만 데이터가 몰리는 '핫스팟(Hotspot)' 문제가 발생하여 샤딩의 효과가 사라집니다.
    *   **샤드 간 데이터 조인 불가:** 샤딩의 가장 큰 제약은 여러 샤드에 걸친 데이터를 `JOIN`하는 쿼리를 직접 실행할 수 없다는 것입니다. 예를 들어, 1번 샤드의 A 유저와 2번 샤드의 B 유저가 친구를 맺는 로직은 DB 레벨에서 처리할 수 없습니다. 이런 경우, 각 샤드에서 필요한 데이터를 애플리케이션 레벨에서 각각 조회한 뒤, 코드에서 조합(JOIN)해야 하는 복잡성이 추가됩니다.
*   **만약 다시 만든다면:**
    *   **리샤딩(Resharding) 전략 고려:** 현재의 모듈러(Modulo) 기반 샤딩은 간단하지만, 나중에 샤드 서버를 추가(e.g., 4대 -> 8대)할 때 대부분의 데이터가 재배치되어야 하는 큰 단점이 있습니다. 실제 프로덕션 환경에서는, 서버를 추가하더라도 최소한의 데이터만 재배치되도록 하는 '컨시스턴트 해싱(Consistent Hashing)' 기법을 샤딩 알고리즘으로 채택하여, 보다 유연한 확장 및 유지보수 전략을 구현할 것입니다.

---

## [SEQUENCE: MVP7-32] 4. 쿼리 최적화 및 읽기/쓰기 분리 (Query Optimization & Read/Write Splitting)
데이터베이스 성능을 향상시키기 위한 고급 기법으로, 쿼리 최적화와 읽기/쓰기 부하 분산 아키텍처의 기반을 구현했습니다.

*   **`[SEQUENCE: MVP7-30] 쿼리 옵티마이저 (Query Optimizer)` (`query_optimizer.h`, `query_optimizer.cpp`):**
    *   자주 사용되고 이미 최적화된 SQL 쿼리문을 이름으로 등록하고 가져올 수 있는 `QueryOptimizer` 싱글톤을 구현했습니다.
    *   이는 ORM(Object-Relational Mapping)등에서 동적으로 생성되는 비효율적인 쿼리 대신, DBA가 직접 튜닝한 고성능 쿼리를 사용하도록 강제하는 간단하면서도 효과적인 최적화 방법입니다.

*   **`[SEQUENCE: MVP7-31] 읽기 전용 복제본 매니저 (Read Replica Manager)` (`read_replica_manager.h`, `read_replica_manager.cpp`):**
    *   데이터베이스의 읽기 부하를 분산시키기 위한 `ReadReplicaManager` 싱글톤을 구현했습니다.
    *   `Initialize` 메서드를 통해 주(Primary) DB 커넥션 풀과 여러 개의 읽기 전용 복제본(Read Replica) DB 커넥션 풀들의 이름을 등록합니다.
    *   `GetWritePool()`은 항상 주 DB 풀을 반환하고, `GetReadPool()`은 등록된 복제본 풀들을 라운드 로빈(Round-Robin) 방식으로 순회하며 반환하여, 읽기 요청의 부하를 여러 서버로 분산시킵니다.

### [SEQUENCE: MVP7-33] 기술 면접 대비 심층 분석 (In-depth Analysis)

*   **기술 선택 이유 (Why Read/Write Splitting?):**
    *   대부분의 온라인 서비스, 특히 게임은 쓰기(Write)보다 읽기(Read) 요청이 훨씬 더 많습니다 (通常 80:20 또는 90:10 비율).
    *   데이터베이스 복제(Replication)를 통해 주 DB의 데이터를 거의 실시간으로 복사하는 여러 개의 복제본 DB를 만들고, 모든 쓰기 요청은 주 DB로, 모든 읽기 요청은 복제본 DB들로 분산시키면, 전체 시스템이 감당할 수 있는 읽기 처리량을 복제본의 수만큼 선형적으로 늘릴 수 있습니다. 이는 읽기 작업이 많은 워크로드에 대한 가장 표준적이고 효과적인 확장 전략입니다.
*   **구현 시 어려웠던 점:**
    *   **복제 지연 (Replication Lag):** 주 DB에 데이터가 쓰여진 후, 복제본 DB에 반영되기까지는 약간의 시간(수 밀리초 ~ 수 초)이 걸릴 수 있습니다. 만약 유저가 글을 쓴 직후(주 DB에 쓰기), 바로 자신의 글을 목록에서 확인(복제본 DB에서 읽기)하려 할 때, 이 지연 시간 때문에 방금 쓴 글이 보이지 않는 문제가 발생할 수 있습니다.
    *   이 문제를 해결하기 위해, "내 글 보기"와 같이 데이터 정합성이 매우 중요한 특정 읽기 요청은 예외적으로 주 DB에서 직접 읽어오도록 하는 로직이 필요합니다. 어떤 요청을 복제본으로 보낼지, 어떤 요청을 주 DB로 보낼지 결정하는 것이 이 아키텍처의 핵심적인 과제입니다.
*   **만약 다시 만든다면:**
    *   현재의 라운드 로빈 방식은 가장 간단한 부하 분산 방식입니다. 더 나아가, 각 복제본 DB의 상태를 주기적으로 헬스 체크하여, 응답 시간이 느리거나 부하가 높은 복제본은 일시적으로 로드 밸런싱에서 제외하는 동적인 라우팅(Dynamic Routing) 기능을 추가할 것입니다. 또한, 각 쿼리의 특성(e.g., `SELECT * FROM users WHERE user_id = ?` vs `SELECT * FROM logs ...`)을 분석하여, 가벼운 쿼리는 하나의 복제본으로, 무거운 분석용 쿼리는 별도의 분석 전용 복제본으로 보내는 등, 쿼리 기반 라우팅(Query-based Routing)을 구현하여 시스템을 더욱 지능적으로 만들 수 있습니다.

---

## [SEQUENCE: MVP7-34] 5. 분산 락 매니저 (Distributed Lock Manager)
여러 서버 인스턴스 환경에서 공유된 자원에 대한 동시 접근을 제어하고 데이터 정합성을 보장하기 위해, Redis를 이용한 분산 락 매니저를 구현했습니다.

*   **`[SEQUENCE: MVP7-35] Redis 의존성 추가` (`conanfile.txt`, `CMakeLists.txt`):**
    *   Redis와 통신하기 위해 C++ 클라이언트 라이브러리인 `redis-plus-plus/1.3.13` 의존성을 `conanfile.txt`에 추가했습니다.
    *   `CMakeLists.txt`를 수정하여 `find_package(redis++)`를 호출하고, `mmorpg_core` 라이브러리에 `redis++::redis++_static`를 링크했습니다.
*   **`[SEQUENCE: MVP7-34] DistributedLockManager` (`distributed_lock_manager.h`, `distributed_lock_manager.cpp`):**
    *   Redis 서버에 연결하고 락 명령을 수행하는 `DistributedLockManager` 싱글톤을 구현했습니다.
    *   `Lock` 메서드는 Redis의 `SET key value NX PX ttl` 명령을 사용하여 원자적(atomic)으로 락을 획득합니다. `NX` 옵션은 키가 존재하지 않을 때만 `SET`을 수행하여, 오직 하나의 클라이언트만 락을 잡을 수 있도록 보장합니다. `PX` 옵션은 락에 TTL(Time-To-Live)을 설정하여, 락을 가진 서버가 비정상 종료되더라도 락이 영원히 해제되지 않는 데드락 상태를 방지합니다.
    *   `Unlock` 메서드는 간단히 Redis의 `DEL key` 명령을 사용하여 락을 해제합니다.

### [SEQUENCE: MVP7-36] 기술 면접 대비 심층 분석 (In-depth Analysis)

*   **기술 선택 이유 (Why Redis for Distributed Locks?):**
    *   **원자적 연산 보장:** Redis는 싱글 스레드 기반으로 동작하여, `SETNX`와 같은 명령어의 원자성을 보장합니다. 여러 클라이언트에서 동시에 `SETNX`를 요청해도, Redis 서버는 이를 순차적으로 처리하여 단 하나의 클라이언트만 성공시키는 것을 보장합니다. 이는 분산 락의 핵심 요구사항입니다.
    *   **TTL (Lease) 기능:** 락에 만료 시간을 쉽게 설정할 수 있어, 락을 소유한 프로세스가 다운되더라도 일정 시간 후에 락이 자동으로 해제됩니다. 이는 시스템 전체가 멈추는 것을 방지하는 중요한 안전장치입니다.
    *   **높은 성능:** In-memory 기반 데이터 저장소인 Redis는 매우 빠르므로, 락을 획득하고 해제하는 과정이 애플리케이션 성능에 미치는 영향을 최소화할 수 있습니다.
*   **구현 시 어려웠던 점:**
    *   **락의 소유권 문제:** `Unlock` 시, 현재 락을 잡고 있는 클라이언트가 정말 '나'인지 확인하는 절차가 없다면, 다른 클라이언트가 잡은 락을 실수로 해제할 수 있습니다. 이 구현에서는 `Lock`을 시도할 때 `value` 부분에 해당 서버나 스레드를 식별할 수 있는 고유한 난수 값을 저장하고, `Unlock`을 하기 전에 Redis에서 키의 값을 읽어와 내가 저장한 값이 맞는지 확인한 후 삭제하는 'Compare-And-Delete' 로직을 추가하면 더 안전해집니다. (이 로직은 Lua 스크립트를 사용하면 Redis에서 원자적으로 수행할 수 있습니다.)
    *   **TTL과 작업 시간:** 락의 TTL은 "이 시간 안에 작업을 반드시 끝내겠다"는 약속입니다. 만약 예상치 못한 문제로 작업이 TTL보다 길어지면, 락이 자동으로 해제되고 다른 클라이언트가 락을 획득하여 공유 자원에 대한 동시 접근이 발생할 수 있습니다. 이를 방지하기 위해, 락을 잡고 있는 스레드가 주기적으로 락의 TTL을 연장해주는 '락 연장(Lock Extension)' 또는 'Heartbeat' 메커니즘을 도입하는 것을 고려해야 합니다.
*   **만약 다시 만든다면:**
    *   **Redlock 알고리즘 고려:** 단일 Redis 인스턴스에 장애가 발생하면 전체 분산 락 시스템이 마비될 수 있습니다. 더 높은 가용성을 위해, 마틴 클레프만이 제안한 'Redlock' 알고리즘을 구현하는 것을 고려할 수 있습니다. 이는 서로 다른 여러 개의 Redis 마스터 노드에 대해 과반수(e.g., 5대 중 3대)의 노드로부터 락을 획득해야만 최종적으로 락이 성공한 것으로 간주하는 방식입니다. 이는 단일 노드의 장애에 훨씬 더 강건한 분산 락을 제공합니다.

---

## [SEQUENCE: MVP7-37] 빌드 검증 (Build Verification)
MVP 7의 모든 기능(Connection Pool, DB Monitor, Cache Manager, Partition/Shard Manager, Query Optimizer, Read Replica Manager, Distributed Lock Manager)을 통합하고, 관련 의존성(mysql-connector-cpp, redis-plus-plus)을 추가한 후 전체 프로젝트를 빌드했습니다.

*   **실행 명령어:** `make -C ecs-realm-server/build`
*   **빌드 결과:** **성공 (100%)**

### 결론
이번 빌드 성공을 통해, **MVP 7의 목표였던 '고급 데이터베이스 및 분산 시스템'의 기반이 성공적으로 완료되었음**을 확인했습니다. 이제 서버는 커넥션 풀, 캐시, 모니터링, 파티셔닝, 샤딩, 분산 락 등 대규모 서비스를 위한 핵심적인 데이터 시스템들을 갖추게 되었습니다.