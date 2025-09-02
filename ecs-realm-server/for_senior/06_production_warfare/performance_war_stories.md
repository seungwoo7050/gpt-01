# 🔥 Performance War Stories: 실제 성능 장애 사례

## 개요

실제 프로덕션 환경에서 겪은 **성능 장애와 해결 과정**을 상세히 기록한 실전 가이드입니다.

### 🎯 학습 목표

- **실제 장애 패턴** 인식과 대응
- **긴급 상황 디버깅** 기법
- **근본 원인 분석** (RCA) 방법론
- **예방과 모니터링** 전략

## 1. Case #1: 메모리 누수로 인한 서버 멜트다운

### 1.1 장애 발생 상황

```cpp
// [SEQUENCE: 1] 문제가 된 코드
class PlayerManager {
private:
    // 문제의 시작: 스마트 포인터 순환 참조
    struct Player {
        uint64_t id;
        std::string name;
        std::shared_ptr<Guild> guild;
        std::vector<std::shared_ptr<Player>> friends;  // 순환 참조 위험!
        std::shared_ptr<Inventory> inventory;
        
        // 이벤트 핸들러가 람다로 자기 자신 캡처
        std::function<void()> onLevelUp;
    };
    
    std::unordered_map<uint64_t, std::shared_ptr<Player>> players_;
    
public:
    void addPlayer(uint64_t id, const std::string& name) {
        auto player = std::make_shared<Player>();
        player->id = id;
        player->name = name;
        
        // 문제 1: 람다가 shared_ptr 캡처로 순환 참조 생성
        player->onLevelUp = [player]() {  // shared_ptr 캡처!
            std::cout << player->name << " leveled up!\n";
            // 통계 업데이트 등...
        };
        
        players_[id] = player;
    }
    
    void makeFriends(uint64_t player1_id, uint64_t player2_id) {
        auto p1 = players_[player1_id];
        auto p2 = players_[player2_id];
        
        // 문제 2: 상호 참조로 메모리 해제 불가
        p1->friends.push_back(p2);
        p2->friends.push_back(p1);
    }
    
    void removePlayer(uint64_t id) {
        players_.erase(id);
        // 친구 목록에서는 제거하지 않음 - 메모리 누수!
    }
};
```

### 1.2 장애 증상 및 타임라인

```
=== Production Incident Timeline ===

Day 1, 14:00 - 새 친구 시스템 배포
Day 1, 18:00 - 메모리 사용량 정상 (8GB/64GB)

Day 2, 10:00 - 메모리 사용량 증가 시작 (12GB/64GB)
Day 2, 14:00 - 첫 알람: 메모리 20GB 초과
Day 2, 16:00 - 메모리 32GB, 응답 시간 증가

Day 2, 18:00 - 긴급 상황:
  - 메모리: 48GB/64GB
  - 평균 레이턴시: 50ms → 800ms
  - 동시 접속자: 5,000명
  - CPU: 95% (GC 스레드)

Day 2, 18:30 - 첫 번째 서버 OOM Kill
Day 2, 19:00 - 연쇄 장애 시작
```

### 1.3 긴급 대응 과정

```cpp
// [SEQUENCE: 2] 긴급 진단 도구
class EmergencyDiagnostics {
private:
    // 실시간 메모리 덤프 분석
    void analyzeMemoryUsage() {
        std::cout << "=== Emergency Memory Analysis ===\n";
        
        // /proc/self/status 읽기
        std::ifstream status("/proc/self/status");
        std::string line;
        while (std::getline(status, line)) {
            if (line.find("VmRSS:") == 0 || 
                line.find("VmSize:") == 0 ||
                line.find("VmPeak:") == 0) {
                std::cout << line << "\n";
            }
        }
        
        // 힙 프로파일링 (jemalloc)
        if (mallctl) {
            size_t allocated, active, metadata, resident, mapped;
            size_t sz = sizeof(size_t);
            
            mallctl("stats.allocated", &allocated, &sz, NULL, 0);
            mallctl("stats.active", &active, &sz, NULL, 0);
            mallctl("stats.metadata", &metadata, &sz, NULL, 0);
            mallctl("stats.resident", &resident, &sz, NULL, 0);
            mallctl("stats.mapped", &mapped, &sz, NULL, 0);
            
            std::cout << "Heap Allocated: " << allocated / (1024*1024) << " MB\n";
            std::cout << "Heap Active: " << active / (1024*1024) << " MB\n";
            std::cout << "Heap Metadata: " << metadata / (1024*1024) << " MB\n";
        }
    }
    
    // 객체 카운트 추적
    template<typename T>
    class ObjectCounter {
    private:
        inline static std::atomic<size_t> created_{0};
        inline static std::atomic<size_t> destroyed_{0};
        
    public:
        ObjectCounter() { created_++; }
        ~ObjectCounter() { destroyed_++; }
        
        static void printStats(const std::string& type_name) {
            size_t alive = created_ - destroyed_;
            std::cout << type_name << " - Created: " << created_ 
                     << ", Destroyed: " << destroyed_ 
                     << ", Alive: " << alive << "\n";
        }
    };
    
    // 수정된 Player 구조체 (디버깅용)
    struct DiagnosticPlayer : ObjectCounter<DiagnosticPlayer> {
        uint64_t id;
        std::weak_ptr<Guild> guild;  // weak_ptr로 변경
        std::vector<std::weak_ptr<DiagnosticPlayer>> friends;  // weak_ptr로 변경
        
        ~DiagnosticPlayer() {
            // 소멸자에서 로깅
            std::cout << "Player " << id << " destroyed\n";
        }
    };
    
public:
    // 긴급 패치 배포
    void deployEmergencyFix() {
        std::cout << "Deploying emergency fix...\n";
        
        // 1. 기존 순환 참조 끊기
        breakCircularReferences();
        
        // 2. 강제 GC 트리거
        forceGarbageCollection();
        
        // 3. 메모리 제한 설정
        setMemoryLimits();
    }
    
    void breakCircularReferences() {
        // 모든 플레이어의 친구 목록을 weak_ptr로 변환
        for (auto& [id, player] : players_) {
            // 람다 캡처를 weak_ptr로 변경
            std::weak_ptr<Player> weak_player = player;
            player->onLevelUp = [weak_player]() {
                if (auto p = weak_player.lock()) {
                    std::cout << p->name << " leveled up!\n";
                }
            };
            
            // 친구 목록 정리
            player->friends.clear();
        }
    }
    
    void forceGarbageCollection() {
        // C++는 GC가 없지만, 스마트 포인터 정리 강제
        std::cout << "Forcing cleanup...\n";
        
        // 모든 expired weak_ptr 제거
        for (auto& [id, player] : players_) {
            player->friends.erase(
                std::remove_if(player->friends.begin(), player->friends.end(),
                    [](const std::weak_ptr<Player>& wp) { return wp.expired(); }),
                player->friends.end()
            );
        }
        
        // 메모리 압축 힌트
        malloc_trim(0);
    }
};
```

### 1.4 근본 원인 분석 (RCA)

```cpp
// [SEQUENCE: 3] 사후 분석 도구
class PostMortemAnalyzer {
private:
    // 메모리 할당 추적
    class AllocationTracker {
    private:
        struct AllocationInfo {
            void* ptr;
            size_t size;
            std::string stack_trace;
            uint64_t timestamp;
        };
        
        std::unordered_map<void*, AllocationInfo> allocations_;
        std::mutex tracker_mutex_;
        std::atomic<size_t> total_allocated_{0};
        
        std::string captureStackTrace() {
            void* buffer[20];
            int nptrs = backtrace(buffer, 20);
            char** strings = backtrace_symbols(buffer, nptrs);
            
            std::stringstream ss;
            for (int i = 0; i < nptrs; ++i) {
                ss << strings[i] << "\n";
            }
            
            free(strings);
            return ss.str();
        }
        
    public:
        void trackAllocation(void* ptr, size_t size) {
            std::lock_guard<std::mutex> lock(tracker_mutex_);
            
            AllocationInfo info;
            info.ptr = ptr;
            info.size = size;
            info.stack_trace = captureStackTrace();
            info.timestamp = getCurrentTimestamp();
            
            allocations_[ptr] = info;
            total_allocated_ += size;
        }
        
        void trackDeallocation(void* ptr) {
            std::lock_guard<std::mutex> lock(tracker_mutex_);
            
            auto it = allocations_.find(ptr);
            if (it != allocations_.end()) {
                total_allocated_ -= it->second.size;
                allocations_.erase(it);
            }
        }
        
        void generateLeakReport() {
            std::cout << "=== Memory Leak Report ===\n";
            std::cout << "Total leaked: " << total_allocated_ / (1024*1024) << " MB\n";
            
            // 크기별로 정렬
            std::vector<AllocationInfo> leaks;
            for (const auto& [ptr, info] : allocations_) {
                leaks.push_back(info);
            }
            
            std::sort(leaks.begin(), leaks.end(),
                [](const auto& a, const auto& b) { return a.size > b.size; });
            
            // 상위 10개 누수 출력
            std::cout << "\nTop 10 Leaks:\n";
            for (size_t i = 0; i < std::min(size_t(10), leaks.size()); ++i) {
                std::cout << i+1 << ". Size: " << leaks[i].size << " bytes\n";
                std::cout << "   Stack trace:\n" << leaks[i].stack_trace << "\n";
            }
        }
    };
    
    // 근본 원인 찾기
    void findRootCause() {
        std::cout << "\n=== Root Cause Analysis ===\n";
        
        // 1. 순환 참조 패턴 찾기
        detectCircularReferences();
        
        // 2. 메모리 할당 패턴 분석
        analyzeAllocationPatterns();
        
        // 3. 타임라인 재구성
        reconstructTimeline();
    }
    
    void detectCircularReferences() {
        // shared_ptr 참조 카운트 분석
        std::cout << "Analyzing reference counts...\n";
        
        size_t circular_refs = 0;
        for (const auto& [id, player] : players_) {
            if (player.use_count() > 1) {
                // DFS로 순환 참조 탐지
                std::unordered_set<uint64_t> visited;
                if (hasCircularReference(player, visited)) {
                    circular_refs++;
                }
            }
        }
        
        std::cout << "Found " << circular_refs << " circular references\n";
    }
    
public:
    // 최종 해결책
    void implementPermanentFix() {
        std::cout << "\n=== Permanent Solution ===\n";
        
        // 1. 아키텍처 개선
        std::cout << "1. Replacing shared_ptr with weak_ptr for cross-references\n";
        std::cout << "2. Implementing proper cleanup in destructors\n";
        std::cout << "3. Adding memory leak detection in CI/CD\n";
        
        // 2. 모니터링 강화
        std::cout << "4. Real-time memory tracking dashboard\n";
        std::cout << "5. Automated leak detection alerts\n";
        std::cout << "6. Memory growth trend analysis\n";
    }
};
```

## 2. Case #2: 캐시 스탬피드로 인한 DB 다운

### 2.1 문제 상황

```cpp
// [SEQUENCE: 4] 문제가 된 캐시 코드
class GameCache {
private:
    struct CacheEntry {
        std::string data;
        std::chrono::steady_clock::time_point expiry;
    };
    
    std::unordered_map<std::string, CacheEntry> cache_;
    std::shared_mutex cache_mutex_;
    
    // 문제: 동시에 많은 요청이 캐시 미스 시 DB 폭주
    std::string getData(const std::string& key) {
        {
            std::shared_lock<std::shared_mutex> lock(cache_mutex_);
            auto it = cache_.find(key);
            if (it != cache_.end() && it->second.expiry > std::chrono::steady_clock::now()) {
                return it->second.data;  // 캐시 히트
            }
        }
        
        // 문제 지점: 여러 스레드가 동시에 여기 도달!
        std::string data = fetchFromDatabase(key);  // DB 쿼리 폭주
        
        {
            std::unique_lock<std::shared_mutex> lock(cache_mutex_);
            cache_[key] = {data, std::chrono::steady_clock::now() + std::chrono::seconds(60)};
        }
        
        return data;
    }
};
```

### 2.2 장애 발생 과정

```
=== Cache Stampede Timeline ===

17:00:00 - 인기 아이템 "전설의 검" 이벤트 시작
17:00:01 - 캐시 TTL 만료 (60초 주기)
17:00:01.001 - 1,000개 동시 요청 발생
17:00:01.002 - 모든 요청이 캐시 미스
17:00:01.003 - 1,000개 DB 쿼리 동시 실행
17:00:01.500 - DB 커넥션 풀 고갈 (max: 100)
17:00:02.000 - DB 응답 시간 10ms → 5,000ms
17:00:05.000 - 애플리케이션 타임아웃 시작
17:00:10.000 - 연쇄 장애: 다른 서비스도 영향
17:00:30.000 - DB 서버 CPU 100%, 메모리 부족
17:01:00.000 - DB 응답 불가, 전체 서비스 다운
```

### 2.3 해결 과정

```cpp
// [SEQUENCE: 5] 캐시 스탬피드 해결책
class ImprovedGameCache {
private:
    struct CacheEntry {
        std::string data;
        std::chrono::steady_clock::time_point expiry;
        std::chrono::steady_clock::time_point stale_time;  // 소프트 만료
        std::atomic<bool> refreshing{false};  // 갱신 중 플래그
    };
    
    std::unordered_map<std::string, CacheEntry> cache_;
    std::shared_mutex cache_mutex_;
    
    // 해결책 1: 확률적 조기 만료 (Probabilistic Early Expiration)
    bool shouldRefreshEarly(const CacheEntry& entry) {
        auto now = std::chrono::steady_clock::now();
        auto ttl = entry.expiry - entry.stale_time;
        auto elapsed = now - entry.stale_time;
        
        // XFetch 알고리즘
        double probability = 1.0 - std::exp(-elapsed.count() / static_cast<double>(ttl.count()));
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0.0, 1.0);
        
        return dis(gen) < probability;
    }
    
    // 해결책 2: 락 기반 단일 갱신
    std::string getDataWithSingleFlight(const std::string& key) {
        while (true) {
            {
                std::shared_lock<std::shared_mutex> lock(cache_mutex_);
                auto it = cache_.find(key);
                
                if (it != cache_.end()) {
                    auto& entry = it->second;
                    
                    // 아직 유효한 데이터
                    if (std::chrono::steady_clock::now() < entry.expiry) {
                        // 조기 갱신 체크
                        if (shouldRefreshEarly(entry)) {
                            bool expected = false;
                            if (entry.refreshing.compare_exchange_strong(expected, true)) {
                                // 백그라운드 갱신 시작
                                std::thread([this, key]() {
                                    refreshInBackground(key);
                                }).detach();
                            }
                        }
                        return entry.data;
                    }
                    
                    // 만료됐지만 갱신 중
                    if (entry.refreshing.load()) {
                        // 스테일 데이터 반환 (가용성 우선)
                        return entry.data;
                    }
                }
            }
            
            // 단일 스레드만 갱신
            static std::unordered_map<std::string, std::mutex> key_mutexes;
            static std::mutex map_mutex;
            
            std::unique_lock<std::mutex> map_lock(map_mutex);
            auto& key_mutex = key_mutexes[key];
            map_lock.unlock();
            
            std::unique_lock<std::mutex> key_lock(key_mutex, std::try_to_lock);
            if (key_lock.owns_lock()) {
                // 갱신 수행
                return refreshAndGet(key);
            } else {
                // 다른 스레드가 갱신 중, 잠시 대기
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    }
    
    // 해결책 3: 분산 락 (Redis 기반)
    class DistributedLock {
    private:
        std::string redis_key_;
        std::string lock_value_;
        int ttl_seconds_;
        
    public:
        bool acquire(const std::string& key, int ttl = 5) {
            redis_key_ = "lock:" + key;
            lock_value_ = generateUUID();
            ttl_seconds_ = ttl;
            
            // SET key value NX EX ttl
            return redisSetNX(redis_key_, lock_value_, ttl_seconds_);
        }
        
        void release() {
            // Lua 스크립트로 원자적 해제
            const char* lua_script = 
                "if redis.call('get', KEYS[1]) == ARGV[1] then "
                "    return redis.call('del', KEYS[1]) "
                "else "
                "    return 0 "
                "end";
            
            redisEval(lua_script, redis_key_, lock_value_);
        }
    };
    
    // 해결책 4: 적응형 TTL
    class AdaptiveTTL {
    private:
        struct AccessStats {
            std::atomic<uint64_t> hit_count{0};
            std::atomic<uint64_t> miss_count{0};
            std::chrono::steady_clock::time_point last_access;
        };
        
        std::unordered_map<std::string, AccessStats> stats_;
        
    public:
        std::chrono::seconds calculateTTL(const std::string& key) {
            auto& stat = stats_[key];
            
            double hit_rate = static_cast<double>(stat.hit_count) / 
                            (stat.hit_count + stat.miss_count + 1);
            
            // 히트율이 높으면 TTL 증가
            int base_ttl = 60;
            int adaptive_ttl = base_ttl * (1.0 + hit_rate);
            
            // 액세스 빈도 고려
            auto now = std::chrono::steady_clock::now();
            auto time_since_last = now - stat.last_access;
            
            if (time_since_last < std::chrono::seconds(10)) {
                adaptive_ttl *= 2;  // 자주 액세스되면 TTL 2배
            }
            
            return std::chrono::seconds(std::min(adaptive_ttl, 3600));
        }
    };
    
private:
    void refreshInBackground(const std::string& key) {
        try {
            std::string new_data = fetchFromDatabase(key);
            
            std::unique_lock<std::shared_mutex> lock(cache_mutex_);
            auto& entry = cache_[key];
            entry.data = new_data;
            entry.stale_time = std::chrono::steady_clock::now();
            entry.expiry = entry.stale_time + calculateAdaptiveTTL(key);
            entry.refreshing = false;
        } catch (...) {
            // 실패 시에도 플래그 해제
            cache_[key].refreshing = false;
        }
    }
};
```

## 3. Case #3: 락 경합으로 인한 성능 저하

### 3.1 문제 코드

```cpp
// [SEQUENCE: 6] 과도한 락 경합 문제
class InventorySystem {
private:
    struct Inventory {
        std::unordered_map<uint32_t, uint32_t> items;  // item_id -> count
        mutable std::mutex mutex;  // 인벤토리별 뮤텍스
    };
    
    std::unordered_map<uint64_t, Inventory> player_inventories_;
    mutable std::mutex global_mutex_;  // 문제: 전역 뮤텍스!
    
    // 문제: 모든 작업이 전역 락 필요
    bool transferItem(uint64_t from_player, uint64_t to_player, 
                     uint32_t item_id, uint32_t count) {
        std::lock_guard<std::mutex> global_lock(global_mutex_);
        
        auto from_it = player_inventories_.find(from_player);
        auto to_it = player_inventories_.find(to_player);
        
        if (from_it == player_inventories_.end() || 
            to_it == player_inventories_.end()) {
            return false;
        }
        
        // 중첩 락 - 데드락 위험!
        std::lock_guard<std::mutex> from_lock(from_it->second.mutex);
        std::lock_guard<std::mutex> to_lock(to_it->second.mutex);
        
        // 아이템 전송 로직...
        return true;
    }
};
```

### 3.2 성능 프로파일링 결과

```
=== Lock Contention Analysis ===

Total CPU Time: 100%
- User Space: 15%
- Kernel Space: 85% (대부분 futex_wait)

Thread States:
- Running: 8 (8 cores)
- Blocked on Mutex: 792
- Total Threads: 800

Lock Statistics (from perf):
- global_mutex_:
  - Contention Rate: 98.5%
  - Average Wait Time: 125ms
  - Max Wait Time: 2,450ms

Hot Path Analysis:
1. transferItem() - 45% of lock time
2. addItem() - 30% of lock time
3. removeItem() - 20% of lock time
4. Others - 5%

Performance Impact:
- Throughput: 50K ops/sec → 2K ops/sec
- Latency P99: 5ms → 850ms
- CPU Efficiency: 5% (95% waiting)
```

### 3.3 단계별 해결 과정

```cpp
// [SEQUENCE: 7] 락 경합 해결 방법들
class OptimizedInventorySystem {
private:
    // 해결책 1: 락 세분화 (Fine-grained Locking)
    struct ShardedInventories {
        static constexpr size_t SHARD_COUNT = 64;
        
        struct Shard {
            std::unordered_map<uint64_t, Inventory> inventories;
            mutable std::shared_mutex mutex;
        };
        
        std::array<Shard, SHARD_COUNT> shards;
        
        Shard& getShard(uint64_t player_id) {
            return shards[player_id % SHARD_COUNT];
        }
    };
    
    // 해결책 2: 락프리 업데이트 큐
    class LockFreeUpdateQueue {
    private:
        struct UpdateCommand {
            enum Type { ADD, REMOVE, TRANSFER };
            Type type;
            uint64_t player_id;
            uint64_t target_id;  // for transfer
            uint32_t item_id;
            uint32_t count;
            std::promise<bool> result;
        };
        
        // MPSC 락프리 큐
        moodycamel::ConcurrentQueue<std::unique_ptr<UpdateCommand>> queue_;
        std::thread worker_thread_;
        std::atomic<bool> running_{true};
        
        void workerLoop() {
            std::unique_ptr<UpdateCommand> cmd;
            
            while (running_) {
                if (queue_.try_dequeue(cmd)) {
                    bool result = processCommand(*cmd);
                    cmd->result.set_value(result);
                } else {
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
            }
        }
        
    public:
        std::future<bool> submitUpdate(UpdateCommand cmd) {
            auto future = cmd.result.get_future();
            queue_.enqueue(std::make_unique<UpdateCommand>(std::move(cmd)));
            return future;
        }
    };
    
    // 해결책 3: 읽기 쓰기 락 분리
    class RWLockInventory {
    private:
        struct VersionedInventory {
            std::atomic<uint64_t> version{0};
            std::unordered_map<uint32_t, uint32_t> items;
        };
        
        std::unordered_map<uint64_t, std::unique_ptr<VersionedInventory>> inventories_;
        mutable std::shared_mutex rw_mutex_;
        
    public:
        // 읽기는 락 없이
        std::optional<uint32_t> getItemCount(uint64_t player_id, uint32_t item_id) const {
            std::shared_lock<std::shared_mutex> lock(rw_mutex_);
            
            auto it = inventories_.find(player_id);
            if (it == inventories_.end()) return std::nullopt;
            
            uint64_t version_before = it->second->version.load();
            
            auto item_it = it->second->items.find(item_id);
            uint32_t count = (item_it != it->second->items.end()) ? item_it->second : 0;
            
            uint64_t version_after = it->second->version.load();
            
            // 읽는 동안 변경되었는지 확인
            if (version_before != version_after) {
                return getItemCount(player_id, item_id);  // 재시도
            }
            
            return count;
        }
    };
    
    // 해결책 4: 스레드 로컬 캐싱
    class ThreadLocalCache {
    private:
        struct CacheEntry {
            uint64_t player_id;
            std::unordered_map<uint32_t, uint32_t> items;
            std::chrono::steady_clock::time_point last_update;
        };
        
        static thread_local std::unordered_map<uint64_t, CacheEntry> tl_cache_;
        
    public:
        std::optional<uint32_t> getCached(uint64_t player_id, uint32_t item_id) {
            auto it = tl_cache_.find(player_id);
            if (it == tl_cache_.end()) return std::nullopt;
            
            auto now = std::chrono::steady_clock::now();
            if (now - it->second.last_update > std::chrono::milliseconds(100)) {
                tl_cache_.erase(it);  // 캐시 만료
                return std::nullopt;
            }
            
            auto item_it = it->second.items.find(item_id);
            return (item_it != it->second.items.end()) ? 
                   std::optional<uint32_t>(item_it->second) : std::nullopt;
        }
    };
    
    // 해결책 5: 배치 처리
    class BatchProcessor {
    private:
        struct BatchUpdate {
            std::vector<UpdateCommand> commands;
            std::chrono::steady_clock::time_point deadline;
        };
        
        std::queue<BatchUpdate> pending_batches_;
        std::mutex batch_mutex_;
        
    public:
        void processBatch() {
            std::unique_lock<std::mutex> lock(batch_mutex_);
            if (pending_batches_.empty()) return;
            
            BatchUpdate batch = std::move(pending_batches_.front());
            pending_batches_.pop();
            lock.unlock();
            
            // 플레이어별로 정렬하여 락 순서 보장
            std::sort(batch.commands.begin(), batch.commands.end(),
                [](const auto& a, const auto& b) { return a.player_id < b.player_id; });
            
            // 배치 실행
            for (const auto& cmd : batch.commands) {
                executeCommand(cmd);
            }
        }
    };
    
    // 최종 통합 솔루션
    void implementFinalSolution() {
        std::cout << "=== Lock Contention Solution ===\n";
        std::cout << "1. Sharding: 64 shards reduce contention by 64x\n";
        std::cout << "2. Lock-free queues: Async updates\n";
        std::cout << "3. RW locks: 90% reads don't block each other\n";
        std::cout << "4. Thread-local caching: 95% cache hit rate\n";
        std::cout << "5. Batch processing: 10x throughput improvement\n";
        
        // 결과
        std::cout << "\nPerformance After Fix:\n";
        std::cout << "- Throughput: 2K → 480K ops/sec\n";
        std::cout << "- Latency P99: 850ms → 8ms\n";
        std::cout << "- CPU Efficiency: 5% → 87%\n";
    }
};
```

## 4. 장애 대응 체크리스트

### 4.1 즉시 대응 프로토콜

```cpp
// [SEQUENCE: 8] 장애 대응 자동화 도구
class IncidentResponseSystem {
private:
    enum class Severity { LOW, MEDIUM, HIGH, CRITICAL };
    
    struct IncidentMetrics {
        double cpu_usage;
        double memory_usage;
        double error_rate;
        double response_time_p99;
        uint32_t active_connections;
    };
    
    // 자동 진단 및 대응
    class AutoResponder {
    private:
        std::vector<std::function<bool(const IncidentMetrics&)>> detectors_;
        std::vector<std::function<void()>> responses_;
        
    public:
        void initialize() {
            // CPU 과부하 감지 및 대응
            detectors_.push_back([](const IncidentMetrics& m) {
                return m.cpu_usage > 90.0;
            });
            responses_.push_back([]() {
                std::cout << "[AUTO] CPU throttling detected, enabling rate limiting\n";
                enableRateLimiting(0.8);  // 20% 트래픽 감소
            });
            
            // 메모리 부족 감지 및 대응
            detectors_.push_back([](const IncidentMetrics& m) {
                return m.memory_usage > 85.0;
            });
            responses_.push_back([]() {
                std::cout << "[AUTO] Memory pressure detected, clearing caches\n";
                clearNonEssentialCaches();
                forceGarbageCollection();
            });
            
            // 응답 시간 증가 감지
            detectors_.push_back([](const IncidentMetrics& m) {
                return m.response_time_p99 > 1000.0;  // 1초
            });
            responses_.push_back([]() {
                std::cout << "[AUTO] High latency detected, enabling circuit breaker\n";
                enableCircuitBreaker();
            });
        }
        
        void checkAndRespond(const IncidentMetrics& metrics) {
            for (size_t i = 0; i < detectors_.size(); ++i) {
                if (detectors_[i](metrics)) {
                    responses_[i]();
                }
            }
        }
    };
    
    // 실시간 진단 도구
    class LiveDiagnostics {
    public:
        void captureSystemState() {
            std::cout << "\n=== EMERGENCY DIAGNOSTICS ===\n";
            std::cout << "Timestamp: " << getCurrentTimestamp() << "\n\n";
            
            // 1. 프로세스 상태
            captureProcessState();
            
            // 2. 스레드 덤프
            captureThreadDump();
            
            // 3. 메모리 상태
            captureMemoryState();
            
            // 4. 네트워크 상태
            captureNetworkState();
            
            // 5. 최근 에러 로그
            captureRecentErrors();
        }
        
    private:
        void captureThreadDump() {
            std::cout << "=== Thread Dump ===\n";
            
            // 모든 스레드 정보 수집
            for (const auto& thread_id : getAllThreadIds()) {
                std::cout << "Thread " << thread_id << ":\n";
                
                // 스택 트레이스
                void* buffer[50];
                int nptrs = backtrace(buffer, 50);
                backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO);
                
                std::cout << "\n";
            }
        }
        
        void captureMemoryState() {
            std::cout << "=== Memory State ===\n";
            
            // 힙 정보
            malloc_info(0, stdout);
            
            // 가상 메모리 정보
            std::ifstream vmstat("/proc/self/status");
            std::string line;
            while (std::getline(vmstat, line)) {
                if (line.find("Vm") == 0) {
                    std::cout << line << "\n";
                }
            }
        }
    };
    
public:
    // 장애 대응 플레이북
    void executePlaybook(Severity severity) {
        switch (severity) {
            case Severity::CRITICAL:
                std::cout << "\n=== CRITICAL INCIDENT RESPONSE ===\n";
                // 1. 즉시 트래픽 차단
                blockIncomingTraffic();
                // 2. 시스템 상태 덤프
                createFullSystemDump();
                // 3. 롤백 준비
                prepareRollback();
                // 4. 알림 발송
                notifyOncallTeam();
                break;
                
            case Severity::HIGH:
                // 부분 격리 및 복구 시도
                isolateProblematicComponents();
                attemptAutoRecovery();
                break;
                
            case Severity::MEDIUM:
                // 모니터링 강화 및 예방 조치
                increaseMonitoringGranularity();
                enableDefensiveMode();
                break;
                
            case Severity::LOW:
                // 로깅 및 추적
                enableDetailedLogging();
                break;
        }
    }
    
private:
    static void enableRateLimiting(double factor) {
        // 레이트 리미팅 구현
    }
    
    static void clearNonEssentialCaches() {
        // 캐시 정리 구현
    }
    
    static void forceGarbageCollection() {
        malloc_trim(0);
    }
    
    static void enableCircuitBreaker() {
        // 서킷 브레이커 구현
    }
};
```

## 5. 교훈과 예방책

### 5.1 사후 개선 사항

```cpp
// [SEQUENCE: 9] 장애 예방 시스템
class IncidentPreventionSystem {
private:
    // 카오스 엔지니어링
    class ChaosMonkey {
    public:
        void runRandomFailures() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(1, 100);
            
            int failure_type = dis(gen);
            
            if (failure_type <= 5) {
                // 5% 확률로 메모리 압박
                simulateMemoryPressure();
            } else if (failure_type <= 10) {
                // 5% 확률로 CPU 스파이크
                simulateCPUSpike();
            } else if (failure_type <= 15) {
                // 5% 확률로 네트워크 지연
                simulateNetworkLatency();
            }
        }
        
    private:
        void simulateMemoryPressure() {
            std::cout << "[CHAOS] Simulating memory pressure\n";
            std::vector<char> dummy(100 * 1024 * 1024);  // 100MB 할당
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    };
    
    // 성능 회귀 감지
    class PerformanceRegression {
    private:
        struct Baseline {
            double avg_latency;
            double p99_latency;
            double throughput;
            double cpu_usage;
            double memory_usage;
        };
        
        Baseline production_baseline_;
        
    public:
        bool detectRegression(const Baseline& current) {
            // 10% 이상 성능 저하 감지
            if (current.avg_latency > production_baseline_.avg_latency * 1.1 ||
                current.p99_latency > production_baseline_.p99_latency * 1.1 ||
                current.throughput < production_baseline_.throughput * 0.9) {
                
                std::cout << "[ALERT] Performance regression detected!\n";
                std::cout << "Avg latency: " << production_baseline_.avg_latency 
                         << " → " << current.avg_latency << "\n";
                return true;
            }
            return false;
        }
    };
    
    // 자동화된 부하 테스트
    class LoadTestAutomation {
    public:
        void runLoadTest() {
            std::cout << "=== Automated Load Test ===\n";
            
            // 점진적 부하 증가
            for (int concurrent = 100; concurrent <= 10000; concurrent *= 2) {
                std::cout << "Testing with " << concurrent << " concurrent users\n";
                
                auto results = simulateLoad(concurrent, 60);  // 60초 테스트
                
                if (results.error_rate > 0.01) {  // 1% 에러율 초과
                    std::cout << "[FAIL] Error rate too high: " 
                             << results.error_rate * 100 << "%\n";
                    break;
                }
                
                if (results.p99_latency > 1000) {  // 1초 초과
                    std::cout << "[FAIL] P99 latency too high: " 
                             << results.p99_latency << "ms\n";
                    break;
                }
            }
        }
    };
    
public:
    // 통합 예방 시스템
    void setupPreventionMeasures() {
        std::cout << "=== Setting Up Prevention Measures ===\n";
        
        // 1. 메모리 누수 감지
        std::cout << "✓ Memory leak detection in CI/CD\n";
        
        // 2. 성능 프로파일링 자동화
        std::cout << "✓ Automated performance profiling\n";
        
        // 3. 카나리 배포
        std::cout << "✓ Canary deployment with auto-rollback\n";
        
        // 4. 실시간 이상 감지
        std::cout << "✓ Real-time anomaly detection\n";
        
        // 5. 자동 스케일링
        std::cout << "✓ Auto-scaling based on metrics\n";
        
        // 6. 장애 주입 테스트
        std::cout << "✓ Chaos engineering in staging\n";
    }
};
```

## 핵심 교훈

### 1. 메모리 누수 사례
- **원인**: shared_ptr 순환 참조
- **증상**: 점진적 메모리 증가, GC 압박
- **해결**: weak_ptr 사용, RAII 패턴
- **예방**: 자동 누수 감지, 메모리 프로파일링

### 2. 캐시 스탬피드 사례  
- **원인**: 동시 캐시 미스
- **증상**: DB 과부하, 연쇄 장애
- **해결**: 확률적 조기 갱신, 분산 락
- **예방**: 적응형 TTL, 부하 테스트

### 3. 락 경합 사례
- **원인**: 굵은 입자 락
- **증상**: CPU 대기, 처리량 급감
- **해결**: 락 세분화, 락프리 구조
- **예방**: 락 프로파일링, 동시성 설계

### 4. 공통 대응 원칙
- **즉시 격리**: 문제 컴포넌트 분리
- **점진적 복구**: 단계별 트래픽 증가
- **근본 원인 분석**: 5 Whys 기법
- **지속적 개선**: 플레이북 업데이트

## 다음 단계

다음 문서에서는 **스케일링 실패 사례**를 다룹니다:
- **[scaling_disasters.md](scaling_disasters.md)** - 스케일링 실패와 해결

---

**"장애는 반드시 온다 - 중요한 것은 얼마나 빨리 감지하고 복구하는가"**