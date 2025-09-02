# 📈 Scaling Disasters: 스케일링 실패 사례와 해결

## 개요

게임서버 **스케일링 과정에서 겪은 실패와 극복 과정**을 상세히 분석한 실전 가이드입니다.

### 🎯 학습 목표

- **스케일링 함정** 인식과 회피
- **분산 시스템 장애** 패턴
- **점진적 스케일링** 전략
- **장애 복구** 아키텍처

## 1. Case #1: 섣부른 샤딩으로 인한 데이터 불일치

### 1.1 초기 상황과 잘못된 결정

```cpp
// [SEQUENCE: 1] 문제가 된 샤딩 구현
class NaiveShardingSystem {
private:
    // 문제 1: 단순한 모듈로 샤딩
    int getShardId(uint64_t player_id) {
        return player_id % NUM_SHARDS;  // 재샤딩 시 문제!
    }
    
    // 문제 2: 크로스 샤드 트랜잭션 미지원
    bool transferItem(uint64_t from_player, uint64_t to_player, 
                     uint32_t item_id, uint32_t count) {
        int from_shard = getShardId(from_player);
        int to_shard = getShardId(to_player);
        
        if (from_shard != to_shard) {
            // 문제: 분산 트랜잭션 없이 처리!
            auto& from_db = shard_connections_[from_shard];
            auto& to_db = shard_connections_[to_shard];
            
            // 1단계: from에서 차감
            from_db.execute("UPDATE inventory SET count = count - ? "
                          "WHERE player_id = ? AND item_id = ?",
                          count, from_player, item_id);
            
            // 네트워크 장애 시 데이터 손실!
            
            // 2단계: to에 추가
            to_db.execute("UPDATE inventory SET count = count + ? "
                          "WHERE player_id = ? AND item_id = ?",
                          count, to_player, item_id);
        }
        return true;
    }
    
    // 문제 3: 핫스팟 샤드
    void createGuild(uint64_t leader_id, const std::string& guild_name) {
        // 모든 길드가 리더의 샤드에 생성 → 불균형!
        int shard_id = getShardId(leader_id);
        shard_connections_[shard_id].execute(
            "INSERT INTO guilds (leader_id, name) VALUES (?, ?)",
            leader_id, guild_name
        );
    }
};
```

### 1.2 장애 발생 과정

```
=== Sharding Disaster Timeline ===

Week 1: 샤딩 시스템 배포
- 4개 샤드로 시작
- 플레이어 ID 기반 모듈로 샤딩
- 초기 성능 개선 확인

Week 2: 첫 번째 문제 발생
- 크로스 샤드 아이템 거래 중 네트워크 장애
- 100여 건의 아이템 복사 버그 발생
- 긴급 롤백 시도 → 실패 (분산 데이터)

Week 3: 핫스팟 문제 심화
- 인기 길드가 Shard 2에 집중
- Shard 2 부하: 85%, 다른 샤드: 20%
- 리밸런싱 시도 → 대규모 다운타임

Week 4: 스케일 아웃 시도
- 4 → 8 샤드로 확장
- 모든 데이터 재배치 필요
- 12시간 점검 → 데이터 불일치 발견

결과:
- 5,000명 플레이어 데이터 손실
- 보상 비용: $50,000
- 신뢰도 하락으로 DAU 30% 감소
```

### 1.3 문제 분석과 해결

```cpp
// [SEQUENCE: 2] 개선된 샤딩 시스템
class ImprovedShardingSystem {
private:
    // 해결책 1: Consistent Hashing
    class ConsistentHashRing {
    private:
        static constexpr int VIRTUAL_NODES = 150;
        std::map<uint64_t, int> ring_;
        std::hash<std::string> hasher_;
        
    public:
        void addShard(int shard_id) {
            for (int i = 0; i < VIRTUAL_NODES; ++i) {
                std::string key = std::to_string(shard_id) + ":" + std::to_string(i);
                uint64_t hash = hasher_(key);
                ring_[hash] = shard_id;
            }
        }
        
        void removeShard(int shard_id) {
            auto it = ring_.begin();
            while (it != ring_.end()) {
                if (it->second == shard_id) {
                    it = ring_.erase(it);
                } else {
                    ++it;
                }
            }
        }
        
        int getShardForKey(uint64_t key) {
            if (ring_.empty()) return -1;
            
            auto it = ring_.lower_bound(key);
            if (it == ring_.end()) {
                it = ring_.begin();
            }
            return it->second;
        }
    };
    
    // 해결책 2: 2PC (Two-Phase Commit) 구현
    class TwoPhaseCommit {
    private:
        struct Transaction {
            std::string id;
            std::vector<int> participating_shards;
            std::unordered_map<int, bool> votes;
            enum State { PREPARING, PREPARED, COMMITTING, COMMITTED, ABORTING, ABORTED };
            State state = PREPARING;
        };
        
        std::unordered_map<std::string, Transaction> transactions_;
        
    public:
        bool executeDistributedTransaction(
            const std::vector<int>& shards,
            const std::vector<std::function<bool(int)>>& operations) {
            
            std::string tx_id = generateTransactionId();
            Transaction& tx = transactions_[tx_id];
            tx.id = tx_id;
            tx.participating_shards = shards;
            
            // Phase 1: Prepare
            tx.state = Transaction::PREPARING;
            for (int shard : shards) {
                bool vote = prepareOnShard(shard, tx_id, operations[shard]);
                tx.votes[shard] = vote;
                
                if (!vote) {
                    // 하나라도 실패하면 abort
                    abortTransaction(tx);
                    return false;
                }
            }
            
            tx.state = Transaction::PREPARED;
            
            // Phase 2: Commit
            tx.state = Transaction::COMMITTING;
            for (int shard : shards) {
                commitOnShard(shard, tx_id);
            }
            
            tx.state = Transaction::COMMITTED;
            transactions_.erase(tx_id);
            return true;
        }
        
    private:
        void abortTransaction(Transaction& tx) {
            tx.state = Transaction::ABORTING;
            for (int shard : tx.participating_shards) {
                abortOnShard(shard, tx.id);
            }
            tx.state = Transaction::ABORTED;
        }
    };
    
    // 해결책 3: 샤드키 전략 개선
    class SmartShardKeyStrategy {
    private:
        // 복합 샤드키로 핫스팟 방지
        struct ShardKey {
            enum Type { PLAYER, GUILD, REGION, RANDOM };
            Type type;
            uint64_t id;
            
            uint64_t hash() const {
                // 타입별로 다른 해시 전략
                switch (type) {
                    case PLAYER:
                        return std::hash<uint64_t>{}(id);
                    case GUILD:
                        // 길드는 추가 랜덤성 부여
                        return std::hash<uint64_t>{}(id) ^ 
                               std::hash<uint64_t>{}(id >> 32);
                    case REGION:
                        // 지역은 지리적 근접성 고려
                        return (id & 0xFF) << 56;  // 상위 바이트 사용
                    case RANDOM:
                        return std::random_device{}();
                }
            }
        };
        
    public:
        int getShardForEntity(ShardKey::Type type, uint64_t id) {
            ShardKey key{type, id};
            return consistent_hash_ring_.getShardForKey(key.hash());
        }
    };
    
    // 해결책 4: 온라인 리샤딩
    class OnlineResharding {
    private:
        enum MigrationState { NONE, COPYING, SWITCHING, CLEANUP };
        
        struct MigrationTask {
            int source_shard;
            int target_shard;
            uint64_t key_range_start;
            uint64_t key_range_end;
            MigrationState state;
            std::atomic<uint64_t> progress{0};
        };
        
        std::vector<MigrationTask> active_migrations_;
        
    public:
        void startResharding(int from_shard, int to_shard, 
                           uint64_t range_start, uint64_t range_end) {
            MigrationTask task{
                from_shard, to_shard, range_start, range_end, 
                MigrationState::COPYING, {0}
            };
            
            active_migrations_.push_back(task);
            
            // 백그라운드 복사 시작
            std::thread([this, task]() mutable {
                copyDataInBackground(task);
            }).detach();
        }
        
    private:
        void copyDataInBackground(MigrationTask& task) {
            // 1. 스냅샷 생성
            uint64_t snapshot_version = createSnapshot(task.source_shard);
            
            // 2. 청크 단위로 복사
            const size_t CHUNK_SIZE = 1000;
            uint64_t current = task.key_range_start;
            
            while (current < task.key_range_end) {
                // 배치 읽기
                auto data = readChunk(task.source_shard, current, 
                                    std::min(current + CHUNK_SIZE, task.key_range_end));
                
                // 대상 샤드에 쓰기
                writeChunk(task.target_shard, data);
                
                current += CHUNK_SIZE;
                task.progress = current - task.key_range_start;
                
                // 부하 제어
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
            // 3. 델타 적용 (스냅샷 이후 변경사항)
            task.state = MigrationState::SWITCHING;
            applyDelta(task, snapshot_version);
            
            // 4. 라우팅 전환
            switchRouting(task);
            
            // 5. 정리
            task.state = MigrationState::CLEANUP;
            cleanupOldData(task);
        }
    };
};
```

## 2. Case #2: 마이크로서비스 분할의 함정

### 2.1 과도한 서비스 분할

```cpp
// [SEQUENCE: 3] 잘못된 마이크로서비스 설계
class OverEngineeredMicroservices {
private:
    // 문제: 너무 작은 단위로 분할
    struct ServiceEndpoints {
        std::string player_service = "http://player-service:8001";
        std::string inventory_service = "http://inventory-service:8002";
        std::string stats_service = "http://stats-service:8003";
        std::string level_service = "http://level-service:8004";
        std::string achievement_service = "http://achievement-service:8005";
        std::string skill_service = "http://skill-service:8006";
        std::string equipment_service = "http://equipment-service:8007";
        // ... 총 47개 마이크로서비스!
    };
    
    // 문제: 단순 조회에도 수십 개 API 호출
    PlayerInfo getPlayerInfo(uint64_t player_id) {
        PlayerInfo info;
        
        // 1. 기본 정보 (player-service)
        auto player_future = std::async(std::launch::async, [=] {
            return httpGet(endpoints_.player_service + "/player/" + 
                         std::to_string(player_id));
        });
        
        // 2. 인벤토리 (inventory-service)
        auto inventory_future = std::async(std::launch::async, [=] {
            return httpGet(endpoints_.inventory_service + "/inventory/" + 
                         std::to_string(player_id));
        });
        
        // 3. 스탯 (stats-service)
        auto stats_future = std::async(std::launch::async, [=] {
            return httpGet(endpoints_.stats_service + "/stats/" + 
                         std::to_string(player_id));
        });
        
        // ... 15개 더 많은 API 호출
        
        // 문제: 네트워크 레이턴시 누적
        info.basic = parseJson(player_future.get());      // 10ms
        info.inventory = parseJson(inventory_future.get()); // 15ms
        info.stats = parseJson(stats_future.get());        // 12ms
        // ... 총 200ms+ 소요
        
        return info;
    }
    
    // 문제: 분산 트랜잭션 복잡도
    bool purchaseItem(uint64_t player_id, uint32_t item_id, uint32_t price) {
        // 1. 재화 확인 (currency-service)
        auto currency = httpGet(endpoints_.currency_service + 
                              "/balance/" + std::to_string(player_id));
        
        if (currency.gold < price) return false;
        
        // 2. 재화 차감 (currency-service) - API 호출
        auto deduct_result = httpPost(endpoints_.currency_service + "/deduct",
            {{"player_id", player_id}, {"amount", price}});
        
        if (!deduct_result.success) return false;
        
        // 3. 아이템 지급 (inventory-service) - API 호출
        auto grant_result = httpPost(endpoints_.inventory_service + "/grant",
            {{"player_id", player_id}, {"item_id", item_id}});
        
        if (!grant_result.success) {
            // 롤백 필요! - 또 다른 API 호출
            httpPost(endpoints_.currency_service + "/refund",
                {{"player_id", player_id}, {"amount", price}});
            return false;
        }
        
        // 4. 통계 업데이트 (analytics-service) - API 호출
        httpPost(endpoints_.analytics_service + "/event",
            {{"type", "purchase"}, {"player_id", player_id}, 
             {"item_id", item_id}, {"price", price}});
        
        return true;
    }
};
```

### 2.2 장애 현상

```
=== Microservice Cascade Failure ===

초기 상태:
- 47개 마이크로서비스
- 평균 응답시간: 200ms
- 서비스 간 호출: 평균 8홉

장애 시작:
1. stats-service 10% 성능 저하
2. 의존 서비스들 타임아웃 증가
3. 리트라이 폭증 → 부하 가중
4. 연쇄적 서비스 장애

15분 후:
- 23개 서비스 응답 불가
- 평균 응답시간: 5,000ms
- 에러율: 65%

복구 시도:
- 개별 서비스 재시작 → 실패
- 순차적 재시작 → 실패
- 전체 시스템 재시작 → 2시간 소요
```

### 2.3 아키텍처 재설계

```cpp
// [SEQUENCE: 4] 개선된 서비스 설계
class OptimizedServiceArchitecture {
private:
    // 해결책 1: 도메인 기반 서비스 통합
    class DomainBoundedServices {
        // Before: 15개 서비스 → After: 3개 서비스
        struct Services {
            std::string player_core_service;     // 플레이어 + 스탯 + 레벨
            std::string game_economy_service;    // 재화 + 인벤토리 + 상점
            std::string social_service;          // 친구 + 길드 + 채팅
        };
        
        // GraphQL로 필요한 데이터만 조회
        std::string getPlayerInfoQuery = R"(
            query GetPlayerInfo($playerId: ID!) {
                player(id: $playerId) {
                    basic { id name level class }
                    stats { hp mp strength agility }
                    inventory { items { id count } }
                    currency { gold gem }
                }
            }
        )";
    };
    
    // 해결책 2: Event Sourcing으로 일관성 보장
    class EventSourcingSystem {
    private:
        struct GameEvent {
            std::string event_id;
            std::string type;
            uint64_t player_id;
            std::string payload;
            uint64_t timestamp;
            uint32_t version;
        };
        
        // 이벤트 스토어
        class EventStore {
        private:
            std::vector<GameEvent> events_;
            std::mutex mutex_;
            
        public:
            void append(const GameEvent& event) {
                std::lock_guard<std::mutex> lock(mutex_);
                events_.push_back(event);
                
                // 비동기 이벤트 발행
                publishToKafka(event);
            }
            
            // 스냅샷 + 이벤트 리플레이
            PlayerState rebuildState(uint64_t player_id) {
                PlayerState state;
                
                // 최근 스냅샷 로드
                auto snapshot = loadSnapshot(player_id);
                state = snapshot.state;
                
                // 스냅샷 이후 이벤트 적용
                auto events = getEventsSince(player_id, snapshot.version);
                for (const auto& event : events) {
                    applyEvent(state, event);
                }
                
                return state;
            }
        };
        
    public:
        // 구매 트랜잭션을 이벤트로 처리
        bool purchaseItem(uint64_t player_id, uint32_t item_id, uint32_t price) {
            // 1. 단일 이벤트 생성
            GameEvent event{
                generateEventId(),
                "ITEM_PURCHASED",
                player_id,
                json::object({
                    {"item_id", item_id},
                    {"price", price},
                    {"currency", "gold"}
                }).dump(),
                getCurrentTimestamp(),
                getNextVersion(player_id)
            };
            
            // 2. 이벤트 저장 (원자적)
            event_store_.append(event);
            
            // 3. 이벤트 핸들러가 비동기 처리
            // - 재화 차감
            // - 아이템 지급
            // - 통계 업데이트
            
            return true;  // 즉시 반환
        }
    };
    
    // 해결책 3: 서킷 브레이커 패턴
    class CircuitBreaker {
    private:
        enum State { CLOSED, OPEN, HALF_OPEN };
        
        struct CircuitState {
            State state = CLOSED;
            std::atomic<uint32_t> failure_count{0};
            std::atomic<uint32_t> success_count{0};
            std::chrono::steady_clock::time_point last_failure_time;
            std::mutex state_mutex;
        };
        
        std::unordered_map<std::string, CircuitState> circuits_;
        
        // 설정
        static constexpr uint32_t FAILURE_THRESHOLD = 5;
        static constexpr auto TIMEOUT = std::chrono::seconds(30);
        static constexpr uint32_t SUCCESS_THRESHOLD = 3;
        
    public:
        template<typename Func>
        auto callWithCircuitBreaker(const std::string& service_name, Func&& func) 
            -> decltype(func()) {
            
            auto& circuit = circuits_[service_name];
            
            // 상태 확인
            if (circuit.state == OPEN) {
                auto now = std::chrono::steady_clock::now();
                if (now - circuit.last_failure_time > TIMEOUT) {
                    // Half-open 상태로 전환
                    std::lock_guard<std::mutex> lock(circuit.state_mutex);
                    circuit.state = HALF_OPEN;
                    circuit.success_count = 0;
                } else {
                    throw std::runtime_error("Circuit breaker is OPEN");
                }
            }
            
            try {
                auto result = func();
                
                // 성공 처리
                if (circuit.state == HALF_OPEN) {
                    circuit.success_count++;
                    if (circuit.success_count >= SUCCESS_THRESHOLD) {
                        std::lock_guard<std::mutex> lock(circuit.state_mutex);
                        circuit.state = CLOSED;
                        circuit.failure_count = 0;
                    }
                }
                
                return result;
                
            } catch (...) {
                // 실패 처리
                circuit.failure_count++;
                circuit.last_failure_time = std::chrono::steady_clock::now();
                
                if (circuit.failure_count >= FAILURE_THRESHOLD) {
                    std::lock_guard<std::mutex> lock(circuit.state_mutex);
                    circuit.state = OPEN;
                }
                
                throw;
            }
        }
    };
    
    // 해결책 4: Bulkhead 패턴 (격리)
    class BulkheadIsolation {
    private:
        struct ThreadPoolBulkhead {
            std::unique_ptr<ThreadPool> pool;
            std::atomic<uint32_t> active_tasks{0};
            uint32_t max_concurrent;
        };
        
        std::unordered_map<std::string, ThreadPoolBulkhead> bulkheads_;
        
    public:
        void initializeBulkheads() {
            // 서비스별 격리된 스레드 풀
            bulkheads_["critical"].pool = std::make_unique<ThreadPool>(20);
            bulkheads_["critical"].max_concurrent = 20;
            
            bulkheads_["normal"].pool = std::make_unique<ThreadPool>(10);
            bulkheads_["normal"].max_concurrent = 10;
            
            bulkheads_["background"].pool = std::make_unique<ThreadPool>(5);
            bulkheads_["background"].max_concurrent = 5;
        }
        
        template<typename Func>
        auto executeInBulkhead(const std::string& bulkhead_name, Func&& func) {
            auto& bulkhead = bulkheads_[bulkhead_name];
            
            if (bulkhead.active_tasks >= bulkhead.max_concurrent) {
                throw std::runtime_error("Bulkhead capacity exceeded");
            }
            
            bulkhead.active_tasks++;
            
            auto future = bulkhead.pool->enqueue([&bulkhead, func]() {
                auto result = func();
                bulkhead.active_tasks--;
                return result;
            });
            
            return future;
        }
    };
};
```

## 3. Case #3: 상태 동기화 지옥

### 3.1 분산 상태 관리의 함정

```cpp
// [SEQUENCE: 5] 상태 동기화 문제
class DistributedStateNightmare {
private:
    // 문제: 여러 서버가 같은 상태를 다르게 보관
    struct PlayerState {
        // Game Server A의 상태
        struct ServerAState {
            float x, y, z;
            float health;
            uint32_t level;
            std::unordered_map<uint32_t, uint32_t> inventory;
        };
        
        // Game Server B의 캐시된 상태
        struct ServerBCache {
            float x, y, z;  // 5초 지연
            float health;   // 실시간
            // 레벨과 인벤토리는 없음
        };
        
        // Database의 영구 상태
        struct DatabaseState {
            float last_x, last_y, last_z;  // 30초마다 저장
            float health;
            uint32_t level;
            std::string inventory_json;  // 직렬화된 형태
        };
    };
    
    // 문제 발생 시나리오
    void stateInconsistencyScenario() {
        // T+0: 플레이어가 Server A에서 아이템 획득
        // - Server A: 인벤토리 업데이트
        // - DB: 아직 반영 안됨 (배치 저장 대기)
        
        // T+2: 플레이어가 Server B로 이동
        // - Server B: DB에서 로드 → 아이템 없음!
        
        // T+5: 플레이어가 Server A로 다시 이동
        // - Server A: 메모리에 있던 상태 → 아이템 있음
        
        // T+30: DB 저장 실행
        // - 충돌: Server A와 Server B의 상태가 다름
    }
};
```

### 3.2 실제 장애 사례

```
=== State Synchronization Disaster ===

배경:
- 10개 게임 서버
- 1개 마스터 DB + 5개 읽기 전용 복제본
- Redis 캐시 클러스터

Day 1: 새로운 크로스 서버 던전 출시
- 플레이어가 서버 간 자유 이동
- 초기에는 정상 작동

Day 3: 첫 번째 아이템 복사 버그 신고
- 서버 이동 시 아이템 2배로 증가
- 원인: 상태 동기화 레이스 컨디션

Day 5: 대규모 어뷰징 시작
- 복사 버그 악용 확산
- 경제 시스템 붕괴 시작

Day 7: 긴급 점검
- 모든 거래 중지
- 로그 분석: 50만 개 아이템 부정 생성

Day 10: 롤백 결정
- 7일치 데이터 롤백
- 정상 플레이어도 피해
- 대규모 이탈 발생
```

### 3.3 근본적 해결책

```cpp
// [SEQUENCE: 6] 상태 동기화 해결 방안
class ConsistentStateManagement {
private:
    // 해결책 1: Single Source of Truth
    class StateAuthority {
    private:
        // 각 엔티티마다 권한 서버 지정
        struct EntityAuthority {
            uint64_t entity_id;
            uint32_t authority_server_id;
            uint64_t version;
            std::chrono::steady_clock::time_point last_update;
        };
        
        std::unordered_map<uint64_t, EntityAuthority> authorities_;
        
    public:
        // 권한 서버만 상태 변경 가능
        bool updateEntityState(uint64_t entity_id, uint32_t server_id,
                             const std::string& state_data) {
            auto it = authorities_.find(entity_id);
            if (it == authorities_.end() || it->second.authority_server_id != server_id) {
                return false;  // 권한 없음
            }
            
            // 버전 증가
            it->second.version++;
            it->second.last_update = std::chrono::steady_clock::now();
            
            // 모든 서버에 브로드캐스트
            broadcastStateUpdate(entity_id, state_data, it->second.version);
            
            return true;
        }
        
        // 권한 이전 (서버 이동 시)
        void transferAuthority(uint64_t entity_id, uint32_t from_server, 
                             uint32_t to_server) {
            // 2단계 핸드셰이크
            // 1. 기존 서버에서 최종 상태 전송
            auto final_state = requestFinalState(from_server, entity_id);
            
            // 2. 새 서버로 권한 이전
            authorities_[entity_id].authority_server_id = to_server;
            authorities_[entity_id].version++;
            
            // 3. 확인 메시지
            confirmTransfer(to_server, entity_id, final_state);
        }
    };
    
    // 해결책 2: Event Ordering with Vector Clocks
    class VectorClockSync {
    private:
        struct VectorClock {
            std::unordered_map<uint32_t, uint64_t> clocks;
            
            void increment(uint32_t server_id) {
                clocks[server_id]++;
            }
            
            void update(const VectorClock& other) {
                for (const auto& [server, time] : other.clocks) {
                    clocks[server] = std::max(clocks[server], time);
                }
            }
            
            bool happensBefore(const VectorClock& other) const {
                bool all_less_equal = true;
                bool at_least_one_less = false;
                
                for (const auto& [server, time] : clocks) {
                    auto it = other.clocks.find(server);
                    if (it == other.clocks.end() || time > it->second) {
                        all_less_equal = false;
                        break;
                    }
                    if (time < it->second) {
                        at_least_one_less = true;
                    }
                }
                
                return all_less_equal && at_least_one_less;
            }
            
            bool concurrent(const VectorClock& other) const {
                return !happensBefore(other) && !other.happensBefore(*this);
            }
        };
        
        struct StateUpdate {
            uint64_t entity_id;
            std::string state_data;
            VectorClock clock;
            uint32_t server_id;
        };
        
        // 동시 업데이트 해결
        StateUpdate resolveConflict(const StateUpdate& update1, 
                                  const StateUpdate& update2) {
            if (update1.clock.happensBefore(update2.clock)) {
                return update2;
            } else if (update2.clock.happensBefore(update1.clock)) {
                return update1;
            } else {
                // 동시 업데이트 - 결정론적 해결
                // 1. 서버 ID가 높은 쪽 우선
                // 2. 또는 커스텀 충돌 해결 로직
                return (update1.server_id > update2.server_id) ? update1 : update2;
            }
        }
    };
    
    // 해결책 3: CRDT (Conflict-free Replicated Data Types)
    class CRDTInventory {
    private:
        // OR-Set CRDT for inventory
        struct ORSet {
            struct Element {
                uint32_t item_id;
                uint32_t count;
                std::string unique_id;  // UUID
                bool tombstone = false;
            };
            
            std::vector<Element> elements;
            
            void add(uint32_t item_id, uint32_t count) {
                elements.push_back({item_id, count, generateUUID(), false});
            }
            
            void remove(const std::string& unique_id) {
                for (auto& elem : elements) {
                    if (elem.unique_id == unique_id) {
                        elem.tombstone = true;
                    }
                }
            }
            
            // CRDT 병합 - 충돌 없음!
            void merge(const ORSet& other) {
                // 모든 요소를 합집합으로 병합
                std::unordered_set<std::string> seen;
                
                for (const auto& elem : elements) {
                    seen.insert(elem.unique_id);
                }
                
                for (const auto& elem : other.elements) {
                    if (seen.find(elem.unique_id) == seen.end()) {
                        elements.push_back(elem);
                    }
                }
            }
            
            std::unordered_map<uint32_t, uint32_t> getItems() const {
                std::unordered_map<uint32_t, uint32_t> result;
                
                for (const auto& elem : elements) {
                    if (!elem.tombstone) {
                        result[elem.item_id] += elem.count;
                    }
                }
                
                return result;
            }
        };
        
    public:
        // 분산 환경에서 안전한 인벤토리 업데이트
        void grantItem(uint64_t player_id, uint32_t item_id, uint32_t count) {
            auto& inventory = player_inventories_[player_id];
            inventory.add(item_id, count);
            
            // 다른 서버로 상태 전파
            propagateState(player_id, inventory);
        }
    };
    
    // 해결책 4: Saga 패턴으로 분산 트랜잭션
    class DistributedSaga {
    private:
        struct SagaStep {
            std::string name;
            std::function<bool()> execute;
            std::function<void()> compensate;
        };
        
        class ItemTransferSaga {
        private:
            std::vector<SagaStep> steps_;
            std::vector<size_t> executed_steps_;
            
        public:
            void setupSteps(uint64_t from_player, uint64_t to_player,
                          uint32_t item_id, uint32_t count) {
                // Step 1: 발신자 인벤토리 검증
                steps_.push_back({
                    "verify_sender",
                    [=]() { return verifySenderHasItem(from_player, item_id, count); },
                    []() { /* No compensation needed */ }
                });
                
                // Step 2: 발신자 아이템 차감
                steps_.push_back({
                    "deduct_from_sender",
                    [=]() { return deductItem(from_player, item_id, count); },
                    [=]() { grantItem(from_player, item_id, count); }  // 보상
                });
                
                // Step 3: 수신자 아이템 추가
                steps_.push_back({
                    "grant_to_receiver",
                    [=]() { return grantItem(to_player, item_id, count); },
                    [=]() { deductItem(to_player, item_id, count); }  // 보상
                });
                
                // Step 4: 트랜잭션 로그
                steps_.push_back({
                    "log_transaction",
                    [=]() { return logTransfer(from_player, to_player, item_id, count); },
                    [=]() { rollbackLog(from_player, to_player, item_id, count); }
                });
            }
            
            bool execute() {
                for (size_t i = 0; i < steps_.size(); ++i) {
                    if (!steps_[i].execute()) {
                        // 실패 시 보상 트랜잭션 실행
                        compensate();
                        return false;
                    }
                    executed_steps_.push_back(i);
                }
                return true;
            }
            
            void compensate() {
                // 역순으로 보상 실행
                for (auto it = executed_steps_.rbegin(); 
                     it != executed_steps_.rend(); ++it) {
                    steps_[*it].compensate();
                }
            }
        };
    };
};
```

## 4. 스케일링 교훈과 베스트 프랙티스

### 4.1 점진적 스케일링 전략

```cpp
// [SEQUENCE: 7] 스케일링 베스트 프랙티스
class ScalingBestPractices {
private:
    // 1. Vertical → Horizontal 전환 전략
    class ProgressiveScaling {
    public:
        void scaleSystemGradually() {
            std::cout << "=== Progressive Scaling Strategy ===\n";
            
            // Phase 1: Vertical Scaling (더 큰 서버)
            std::cout << "Phase 1: Scale Up\n";
            std::cout << "- Upgrade to larger instances\n";
            std::cout << "- Optimize single-server performance\n";
            std::cout << "- Identify bottlenecks\n\n";
            
            // Phase 2: Read Replica (읽기 분산)
            std::cout << "Phase 2: Read Scaling\n";
            std::cout << "- Add read replicas for DB\n";
            std::cout << "- Implement caching layer\n";
            std::cout << "- Separate read/write paths\n\n";
            
            // Phase 3: Functional Sharding (기능별 분리)
            std::cout << "Phase 3: Functional Decomposition\n";
            std::cout << "- Separate chat servers\n";
            std::cout << "- Dedicated match-making\n";
            std::cout << "- Independent leaderboards\n\n";
            
            // Phase 4: Data Sharding (데이터 분산)
            std::cout << "Phase 4: Data Sharding\n";
            std::cout << "- Shard by player ID range\n";
            std::cout << "- Implement consistent hashing\n";
            std::cout << "- Build resharding tools\n\n";
            
            // Phase 5: Geographic Distribution
            std::cout << "Phase 5: Multi-Region\n";
            std::cout << "- Regional game servers\n";
            std::cout << "- Cross-region replication\n";
            std::cout << "- Global load balancing\n";
        }
    };
    
    // 2. 모니터링 기반 스케일링
    class MetricsDrivenScaling {
    private:
        struct ScalingMetrics {
            double cpu_usage;
            double memory_usage;
            double network_throughput;
            uint32_t concurrent_players;
            double avg_response_time;
            double error_rate;
        };
        
        struct ScalingThresholds {
            double cpu_high = 70.0;
            double memory_high = 80.0;
            double response_time_high = 100.0;  // ms
            double error_rate_high = 0.01;      // 1%
        };
        
    public:
        void autoScale(const ScalingMetrics& metrics) {
            ScalingThresholds thresholds;
            
            // CPU 기반 스케일링
            if (metrics.cpu_usage > thresholds.cpu_high) {
                scaleOut("High CPU usage detected");
            }
            
            // 응답 시간 기반 스케일링
            if (metrics.avg_response_time > thresholds.response_time_high) {
                scaleOut("High latency detected");
            }
            
            // 에러율 기반 스케일링
            if (metrics.error_rate > thresholds.error_rate_high) {
                investigateAndScale("High error rate");
            }
            
            // 플레이어 수 기반 스케일링
            uint32_t players_per_server = 1000;
            uint32_t required_servers = (metrics.concurrent_players + 
                                       players_per_server - 1) / players_per_server;
            adjustServerCount(required_servers);
        }
    };
    
    // 3. 장애 격리 아키텍처
    class FaultIsolation {
    public:
        void implementBulkheadPattern() {
            std::cout << "=== Bulkhead Pattern Implementation ===\n";
            
            // 서비스별 리소스 격리
            std::cout << "1. Resource Isolation:\n";
            std::cout << "   - Separate thread pools per service\n";
            std::cout << "   - Independent connection pools\n";
            std::cout << "   - Isolated cache instances\n\n";
            
            // 지역별 격리
            std::cout << "2. Regional Isolation:\n";
            std::cout << "   - Independent regional clusters\n";
            std::cout << "   - No cross-region dependencies\n";
            std::cout << "   - Regional failover only\n\n";
            
            // 기능별 격리
            std::cout << "3. Feature Isolation:\n";
            std::cout << "   - Core gameplay always available\n";
            std::cout << "   - Optional features can fail\n";
            std::cout << "   - Graceful degradation\n";
        }
    };
    
public:
    void summarizeLessonsLearned() {
        std::cout << "\n=== Key Scaling Lessons ===\n\n";
        
        std::cout << "1. 🚫 DON'T:\n";
        std::cout << "   - Over-engineer early\n";
        std::cout << "   - Ignore consistency models\n";
        std::cout << "   - Assume infinite scalability\n";
        std::cout << "   - Neglect operational complexity\n\n";
        
        std::cout << "2. ✅ DO:\n";
        std::cout << "   - Start simple, evolve gradually\n";
        std::cout << "   - Measure before optimizing\n";
        std::cout << "   - Design for failure\n";
        std::cout << "   - Automate everything\n";
        std::cout << "   - Test at scale regularly\n\n";
        
        std::cout << "3. 🏗️ Architecture Principles:\n";
        std::cout << "   - Stateless where possible\n";
        std::cout << "   - Async communication\n";
        std::cout << "   - Event-driven design\n";
        std::cout << "   - Circuit breakers everywhere\n";
        std::cout << "   - Observability first\n";
    }
};
```

## 핵심 교훈 정리

### 1. 샤딩 실패 교훈
- **문제**: 단순 모듈로 샤딩, 크로스 샤드 트랜잭션
- **결과**: 데이터 불일치, 핫스팟, 리샤딩 지옥
- **해결**: Consistent hashing, 2PC, 온라인 리샤딩
- **교훈**: 샤딩은 최후의 수단, 신중한 키 설계 필수

### 2. 마이크로서비스 실패 교훈
- **문제**: 과도한 서비스 분할 (47개)
- **결과**: 네트워크 지연 누적, 연쇄 장애
- **해결**: 도메인 경계 재정의, 서킷 브레이커
- **교훈**: 적절한 크기의 서비스, 장애 격리 필수

### 3. 상태 동기화 실패 교훈
- **문제**: 분산 상태 불일치
- **결과**: 아이템 복사, 롤백, 대규모 이탈
- **해결**: Single source of truth, CRDT, Saga 패턴
- **교훈**: CAP 이론 이해, 일관성 모델 선택

### 4. 스케일링 원칙
- **점진적 확장**: Vertical → Horizontal
- **측정 기반**: 추측 말고 측정
- **장애 가정**: Design for failure
- **자동화**: 수동 작업 최소화
- **격리**: Blast radius 제한

## 다음 단계

다음 문서에서는 **최적화 성공 사례**를 다룹니다:
- **[optimization_case_studies.md](optimization_case_studies.md)** - 최적화 성공 사례 분석

---

**"확장은 필요할 때 하되, 준비는 미리 하라 - 섣부른 최적화보다 위험한 것은 섣부른 분산화다"**