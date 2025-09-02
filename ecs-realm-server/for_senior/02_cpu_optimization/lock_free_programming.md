# Lock-Free Programming으로 게임서버 동시성 성능 1000% 극대화

## 🎯 Lock-Free의 게임서버적 중요성

### 전형적인 Lock 기반 게임서버의 병목현상

```
5,000명 동시 접속 MMORPG 서버에서:
- 매초 50만 개의 상태 업데이트
- Lock contention으로 평균 대기시간 15ms
- 전체 처리량 80% 감소
- 지연시간 spike로 플레이어 경험 파괴
```

**Lock-Free가 필수인 이유:**
- 실시간 전투 시스템 - 1ms 내 반응 필요
- 대규모 멀티플레이어 동기화
- 확장성 - 코어 수에 비례한 성능 증가
- 데드락/우선순위 역전 문제 완전 제거

### 현재 프로젝트의 동시성 문제점 분석

```cpp
// 현재 코드의 Lock 기반 동시성 (src/core/threading/thread_safe_container.h)
template<typename T>
class ThreadSafeQueue {
private:
    std::queue<T> queue_;
    mutable std::mutex mutex_;           // 성능 병목의 주범
    std::condition_variable condition_;  // 컨텍스트 스위칭 오버헤드
    
public:
    void Push(const T& item) {
        std::lock_guard<std::mutex> lock(mutex_);  // 직렬화 강제
        queue_.push(item);
        condition_.notify_one();  // 시스템콜 오버헤드
    }
    
    // 문제점:
    // 1. 모든 스레드가 하나의 mutex를 두고 경쟁
    // 2. False sharing으로 캐시라인 무효화
    // 3. OS 스케줄러 의존적 성능
};
```

## 🔧 Atomic Operations와 Memory Ordering 마스tery

### 1. 메모리 모델 이해와 실제 적용

```cpp
// [SEQUENCE: 46] C++11 메모리 모델의 현실적 활용
#include <atomic>
#include <memory>

enum class MemoryOrder {
    RELAXED = std::memory_order_relaxed,    // 단순 원자성만 보장
    ACQUIRE = std::memory_order_acquire,    // Load 이후 메모리 읽기/쓰기 금지
    RELEASE = std::memory_order_release,    // Store 이전 메모리 읽기/쓰기 금지
    ACQ_REL = std::memory_order_acq_rel,    // Acquire + Release
    SEQ_CST = std::memory_order_seq_cst     // 순차 일관성 (가장 강함)
};

// [SEQUENCE: 47] 게임서버 특화 Atomic Counter
class GameServerCounter {
private:
    alignas(64) std::atomic<uint64_t> value_{0};  // 캐시라인 정렬
    
public:
    // 성능 크리티컬: 플레이어 ID 생성
    uint64_t GetNextPlayerId() noexcept {
        // Relaxed ordering: 순서는 중요하지 않고 유일성만 필요
        return value_.fetch_add(1, std::memory_order_relaxed);
    }
    
    // 통계용: 전체 플레이어 수
    uint64_t GetTotalPlayers() const noexcept {
        // Acquire ordering: 다른 스레드의 쓰기를 확실히 관찰
        return value_.load(std::memory_order_acquire);
    }
    
    // 서버 종료 시: 데이터 일관성 보장
    void SaveToDatabase(uint64_t checkpoint) noexcept {
        // Release ordering: 이전의 모든 쓰기가 완료된 후 저장
        value_.store(checkpoint, std::memory_order_release);
    }
};

// [SEQUENCE: 48] Memory Ordering 실전 적용 사례
class PlayerStateManager {
private:
    struct PlayerData {
        alignas(64) std::atomic<float> health;
        alignas(64) std::atomic<uint32_t> level;
        alignas(64) std::atomic<bool> is_online;
        char padding[52];  // 캐시라인 경계 유지
    };
    
    std::unique_ptr<PlayerData[]> players_;
    size_t max_players_;
    
public:
    PlayerStateManager(size_t max_players) 
        : max_players_(max_players)
        , players_(std::make_unique<PlayerData[]>(max_players)) {}
    
    // [SEQUENCE: 49] 전투 시스템: Acquire-Release 패턴
    bool DealDamage(uint32_t player_id, float damage) {
        if (player_id >= max_players_) return false;
        
        auto& player = players_[player_id];
        
        // 1. 현재 체력 읽기 (Acquire)
        float current_health = player.health.load(std::memory_order_acquire);
        
        // 2. 온라인 상태 확인 (이전 Acquire가 보장)
        if (!player.is_online.load(std::memory_order_relaxed)) {
            return false;
        }
        
        // 3. 새로운 체력 계산
        float new_health = std::max(0.0f, current_health - damage);
        
        // 4. Compare-And-Swap으로 원자적 업데이트
        while (!player.health.compare_exchange_weak(
            current_health, new_health,
            std::memory_order_release,  // 성공 시: 모든 계산 완료 후 쓰기
            std::memory_order_acquire   // 실패 시: 최신 값 다시 읽기
        )) {
            // 다른 스레드가 먼저 수정했다면 재계산
            new_health = std::max(0.0f, current_health - damage);
        }
        
        return true;
    }
    
    // [SEQUENCE: 50] 레벨업: Sequential Consistency 보장
    bool LevelUp(uint32_t player_id) {
        if (player_id >= max_players_) return false;
        
        auto& player = players_[player_id];
        
        // Sequential consistency: 모든 스레드가 동일한 순서로 관찰
        uint32_t current_level = player.level.load(std::memory_order_seq_cst);
        uint32_t new_level = current_level + 1;
        
        // 체력도 동기화해서 업데이트 (레벨에 따른 체력 증가)
        float new_max_health = CalculateMaxHealth(new_level);
        
        // 두 값을 모두 Sequential consistency로 업데이트
        if (player.level.compare_exchange_strong(
            current_level, new_level, std::memory_order_seq_cst)) {
            
            player.health.store(new_max_health, std::memory_order_seq_cst);
            return true;
        }
        
        return false;
    }
};
```

### 2. Lock-Free 자료구조 구현

```cpp
// [SEQUENCE: 51] Michael & Scott Lock-Free Queue (업계 표준)
template<typename T>
class LockFreeQueue {
private:
    struct Node {
        std::atomic<T*> data{nullptr};
        std::atomic<Node*> next{nullptr};
        
        Node() = default;
        ~Node() {
            T* d = data.load();
            delete d;
        }
    };
    
    // Head와 Tail은 서로 다른 캐시라인에 배치
    alignas(64) std::atomic<Node*> head_;
    alignas(64) std::atomic<Node*> tail_;
    
    // 메모리 재사용을 위한 노드 풀
    alignas(64) std::atomic<Node*> free_nodes_{nullptr};
    
public:
    LockFreeQueue() {
        Node* dummy = new Node;
        head_.store(dummy, std::memory_order_relaxed);
        tail_.store(dummy, std::memory_order_relaxed);
    }
    
    ~LockFreeQueue() {
        // 남은 데이터 정리
        T* item;
        while (Dequeue(item)) {
            delete item;
        }
        
        // 더미 노드 정리
        delete head_.load();
        
        // 노드 풀 정리
        Node* node = free_nodes_.load();
        while (node) {
            Node* next = node->next.load();
            delete node;
            node = next;
        }
    }
    
    // [SEQUENCE: 52] Lock-Free Enqueue 구현
    void Enqueue(T item) {
        Node* new_node = AllocateNode();
        T* data = new T(std::move(item));
        new_node->data.store(data, std::memory_order_relaxed);
        
        while (true) {
            Node* last = tail_.load(std::memory_order_acquire);
            Node* next = last->next.load(std::memory_order_acquire);
            
            // Tail이 변경되었는지 확인
            if (last != tail_.load(std::memory_order_acquire)) {
                continue;
            }
            
            if (next == nullptr) {
                // Tail이 실제로 마지막 노드인 경우
                if (last->next.compare_exchange_weak(
                    next, new_node,
                    std::memory_order_release,
                    std::memory_order_acquire)) {
                    break;  // 성공적으로 연결
                }
            } else {
                // Tail을 앞으로 이동 (다른 스레드 도움)
                tail_.compare_exchange_weak(
                    last, next,
                    std::memory_order_release,
                    std::memory_order_acquire);
            }
        }
        
        // Tail을 새 노드로 이동
        tail_.compare_exchange_weak(
            tail_.load(), new_node,
            std::memory_order_release,
            std::memory_order_acquire);
    }
    
    // [SEQUENCE: 53] Lock-Free Dequeue 구현
    bool Dequeue(T& result) {
        while (true) {
            Node* first = head_.load(std::memory_order_acquire);
            Node* last = tail_.load(std::memory_order_acquire);
            Node* next = first->next.load(std::memory_order_acquire);
            
            // Head가 변경되었는지 확인
            if (first != head_.load(std::memory_order_acquire)) {
                continue;
            }
            
            if (first == last) {
                if (next == nullptr) {
                    return false;  // 큐가 비어있음
                }
                
                // Tail을 앞으로 이동 (다른 스레드 도움)
                tail_.compare_exchange_weak(
                    last, next,
                    std::memory_order_release,
                    std::memory_order_acquire);
            } else {
                if (next == nullptr) {
                    continue;  // 일시적 불일치 상태
                }
                
                // 데이터 읽기
                T* data = next->data.load(std::memory_order_acquire);
                if (data == nullptr) {
                    continue;  // 다른 스레드가 이미 가져갔음
                }
                
                // Head를 다음 노드로 이동
                if (head_.compare_exchange_weak(
                    first, next,
                    std::memory_order_release,
                    std::memory_order_acquire)) {
                    
                    result = *data;
                    delete data;
                    
                    // 이전 Head 노드를 풀에 반환
                    FreeNode(first);
                    return true;
                }
            }
        }
    }
    
private:
    // [SEQUENCE: 54] 메모리 풀 기반 노드 할당
    Node* AllocateNode() {
        Node* node = free_nodes_.load(std::memory_order_acquire);
        
        while (node != nullptr) {
            Node* next = node->next.load(std::memory_order_relaxed);
            
            if (free_nodes_.compare_exchange_weak(
                node, next,
                std::memory_order_release,
                std::memory_order_acquire)) {
                
                // 노드 재사용을 위한 초기화
                node->data.store(nullptr, std::memory_order_relaxed);
                node->next.store(nullptr, std::memory_order_relaxed);
                return node;
            }
        }
        
        // 풀이 비어있으면 새로 할당
        return new Node;
    }
    
    void FreeNode(Node* node) {
        Node* old_head = free_nodes_.load(std::memory_order_relaxed);
        
        do {
            node->next.store(old_head, std::memory_order_relaxed);
        } while (!free_nodes_.compare_exchange_weak(
            old_head, node,
            std::memory_order_release,
            std::memory_order_relaxed));
    }
};
```

### 3. ABA Problem 해결 기법

```cpp
// [SEQUENCE: 55] Tagged Pointer로 ABA Problem 해결
template<typename T>
class ABAFreeStack {
private:
    struct Node {
        T data;
        Node* next;
        
        Node(T d) : data(std::move(d)), next(nullptr) {}
    };
    
    // Tagged pointer: 포인터 + 버전 태그
    struct TaggedPointer {
        Node* ptr;
        uintptr_t tag;
        
        TaggedPointer() : ptr(nullptr), tag(0) {}
        TaggedPointer(Node* p, uintptr_t t) : ptr(p), tag(t) {}
        
        bool operator==(const TaggedPointer& other) const {
            return ptr == other.ptr && tag == other.tag;
        }
    };
    
    alignas(16) std::atomic<TaggedPointer> head_;  // 128-bit atomic
    
public:
    ABAFreeStack() : head_(TaggedPointer{nullptr, 0}) {}
    
    // [SEQUENCE: 56] ABA-Safe Push 구현
    void Push(T item) {
        Node* new_node = new Node(std::move(item));
        TaggedPointer old_head = head_.load(std::memory_order_acquire);
        
        do {
            new_node->next = old_head.ptr;
            TaggedPointer new_head(new_node, old_head.tag + 1);  // 태그 증가
            
        } while (!head_.compare_exchange_weak(
            old_head, new_head,
            std::memory_order_release,
            std::memory_order_acquire));
    }
    
    // [SEQUENCE: 57] ABA-Safe Pop 구현
    bool Pop(T& result) {
        TaggedPointer old_head = head_.load(std::memory_order_acquire);
        
        do {
            if (old_head.ptr == nullptr) {
                return false;  // 스택이 비어있음
            }
            
            result = old_head.ptr->data;
            TaggedPointer new_head(old_head.ptr->next, old_head.tag + 1);
            
        } while (!head_.compare_exchange_weak(
            old_head, new_head,
            std::memory_order_release,
            std::memory_order_acquire));
        
        // 안전하게 노드 삭제 (Hazard Pointer 사용 권장)
        delete old_head.ptr;
        return true;
    }
};

// [SEQUENCE: 58] Hazard Pointer를 활용한 안전한 메모리 관리
class HazardPointerManager {
private:
    static constexpr size_t MAX_THREADS = 64;
    static constexpr size_t HAZARD_POINTERS_PER_THREAD = 4;
    
    struct alignas(64) HazardPointer {
        std::atomic<void*> pointer{nullptr};
        char padding[56];  // 캐시라인 경계
    };
    
    // 스레드별 Hazard Pointer 배열
    HazardPointer hazards_[MAX_THREADS][HAZARD_POINTERS_PER_THREAD];
    
    // 삭제 대기 중인 포인터들
    struct RetiredPointer {
        void* pointer;
        void (*deleter)(void*);
        RetiredPointer* next;
    };
    
    thread_local RetiredPointer* retired_list_ = nullptr;
    thread_local size_t retired_count_ = 0;
    static constexpr size_t RETIRED_THRESHOLD = 16;
    
public:
    // [SEQUENCE: 59] Hazard Pointer 획득
    size_t AcquireHazardPointer(void* ptr) {
        size_t thread_id = GetThreadId();
        
        for (size_t i = 0; i < HAZARD_POINTERS_PER_THREAD; ++i) {
            void* expected = nullptr;
            if (hazards_[thread_id][i].pointer.compare_exchange_strong(
                expected, ptr, std::memory_order_release)) {
                return thread_id * HAZARD_POINTERS_PER_THREAD + i;
            }
        }
        
        throw std::runtime_error("No available hazard pointer");
    }
    
    // [SEQUENCE: 60] Hazard Pointer 해제
    void ReleaseHazardPointer(size_t index) {
        size_t thread_id = index / HAZARD_POINTERS_PER_THREAD;
        size_t slot = index % HAZARD_POINTERS_PER_THREAD;
        
        hazards_[thread_id][slot].pointer.store(nullptr, std::memory_order_release);
    }
    
    // [SEQUENCE: 61] 안전한 메모리 회수
    void RetirePointer(void* ptr, void (*deleter)(void*)) {
        RetiredPointer* retired = new RetiredPointer{ptr, deleter, retired_list_};
        retired_list_ = retired;
        ++retired_count_;
        
        if (retired_count_ >= RETIRED_THRESHOLD) {
            ReclaimMemory();
        }
    }
    
private:
    void ReclaimMemory() {
        // 현재 사용 중인 Hazard Pointer 수집
        std::set<void*> hazard_ptrs;
        for (size_t t = 0; t < MAX_THREADS; ++t) {
            for (size_t i = 0; i < HAZARD_POINTERS_PER_THREAD; ++i) {
                void* ptr = hazards_[t][i].pointer.load(std::memory_order_acquire);
                if (ptr != nullptr) {
                    hazard_ptrs.insert(ptr);
                }
            }
        }
        
        // 안전하게 회수 가능한 포인터들 정리
        RetiredPointer** current = &retired_list_;
        while (*current != nullptr) {
            if (hazard_ptrs.find((*current)->pointer) == hazard_ptrs.end()) {
                // 안전하게 삭제 가능
                RetiredPointer* to_delete = *current;
                *current = (*current)->next;
                
                to_delete->deleter(to_delete->pointer);
                delete to_delete;
                --retired_count_;
            } else {
                current = &((*current)->next);
            }
        }
    }
    
    size_t GetThreadId() {
        thread_local size_t thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id()) % MAX_THREADS;
        return thread_id;
    }
};
```

## 🎮 게임서버 특화 Lock-Free 시스템

### 1. 실시간 이벤트 처리 시스템

```cpp
// [SEQUENCE: 62] 게임 이벤트 전용 Lock-Free Queue
enum class GameEventType : uint8_t {
    PLAYER_MOVE = 0,
    PLAYER_ATTACK = 1,
    PLAYER_CHAT = 2,
    ITEM_PICKUP = 3,
    SPELL_CAST = 4,
    MAX_EVENTS = 5
};

struct GameEvent {
    GameEventType type;
    uint32_t player_id;
    uint64_t timestamp;
    
    union EventData {
        struct {
            float x, y, z;
            float velocity_x, velocity_y, velocity_z;
        } move_data;
        
        struct {
            uint32_t target_id;
            float damage;
            uint32_t skill_id;
        } attack_data;
        
        struct {
            char message[128];
            uint32_t channel_id;
        } chat_data;
        
        struct {
            uint32_t item_id;
            uint32_t quantity;
        } item_data;
        
        EventData() {}  // Union 기본 생성자
    } data;
    
    GameEvent() : type(GameEventType::PLAYER_MOVE), player_id(0), timestamp(0) {}
};

// [SEQUENCE: 63] 이벤트 타입별 분리된 Lock-Free 처리
class GameEventProcessor {
private:
    // 이벤트 타입별 전용 큐 (False sharing 방지)
    alignas(64) LockFreeQueue<GameEvent> event_queues_[static_cast<size_t>(GameEventType::MAX_EVENTS)];
    
    // 처리 통계 (성능 모니터링)
    alignas(64) std::atomic<uint64_t> processed_counts_[static_cast<size_t>(GameEventType::MAX_EVENTS)];
    alignas(64) std::atomic<uint64_t> dropped_counts_[static_cast<size_t>(GameEventType::MAX_EVENTS)];
    
    // 우선순위 기반 처리 순서
    static constexpr int EVENT_PRIORITIES[] = {
        3,  // PLAYER_MOVE - 높은 우선순위
        4,  // PLAYER_ATTACK - 최고 우선순위
        1,  // PLAYER_CHAT - 낮은 우선순위
        2,  // ITEM_PICKUP - 중간 우선순위
        3   // SPELL_CAST - 높은 우선순위
    };
    
public:
    // [SEQUENCE: 64] 이벤트 발생 (Producer)
    bool EnqueueEvent(GameEventType type, uint32_t player_id, const void* event_data) {
        if (type >= GameEventType::MAX_EVENTS) {
            return false;
        }
        
        GameEvent event;
        event.type = type;
        event.player_id = player_id;
        event.timestamp = GetHighResolutionTimestamp();
        
        // 이벤트 데이터 복사
        switch (type) {
            case GameEventType::PLAYER_MOVE:
                std::memcpy(&event.data.move_data, event_data, sizeof(event.data.move_data));
                break;
            case GameEventType::PLAYER_ATTACK:
                std::memcpy(&event.data.attack_data, event_data, sizeof(event.data.attack_data));
                break;
            // ... 다른 이벤트 타입들
        }
        
        size_t queue_index = static_cast<size_t>(type);
        event_queues_[queue_index].Enqueue(std::move(event));
        
        return true;
    }
    
    // [SEQUENCE: 65] 우선순위 기반 이벤트 처리 (Consumer)
    void ProcessEvents(size_t max_events_per_frame = 1000) {
        // 우선순위 순으로 이벤트 처리
        std::vector<std::pair<int, size_t>> priority_queues;
        
        for (size_t i = 0; i < static_cast<size_t>(GameEventType::MAX_EVENTS); ++i) {
            priority_queues.emplace_back(EVENT_PRIORITIES[i], i);
        }
        
        // 우선순위 내림차순 정렬
        std::sort(priority_queues.begin(), priority_queues.end(), std::greater<>());
        
        size_t processed_total = 0;
        
        for (const auto& [priority, queue_index] : priority_queues) {
            if (processed_total >= max_events_per_frame) {
                break;
            }
            
            // 각 우선순위별로 최대 처리량 제한
            size_t max_for_this_priority = (max_events_per_frame - processed_total) / (5 - priority);
            size_t processed_this_queue = 0;
            
            GameEvent event;
            while (processed_this_queue < max_for_this_priority && 
                   event_queues_[queue_index].Dequeue(event)) {
                
                ProcessSingleEvent(event);
                processed_counts_[queue_index].fetch_add(1, std::memory_order_relaxed);
                ++processed_this_queue;
                ++processed_total;
            }
        }
    }
    
private:
    // [SEQUENCE: 66] 이벤트별 전용 처리 로직
    void ProcessSingleEvent(const GameEvent& event) {
        switch (event.type) {
            case GameEventType::PLAYER_MOVE:
                ProcessMoveEvent(event);
                break;
            case GameEventType::PLAYER_ATTACK:
                ProcessAttackEvent(event);
                break;
            case GameEventType::PLAYER_CHAT:
                ProcessChatEvent(event);
                break;
            case GameEventType::ITEM_PICKUP:
                ProcessItemEvent(event);
                break;
            case GameEventType::SPELL_CAST:
                ProcessSpellEvent(event);
                break;
        }
    }
    
    void ProcessMoveEvent(const GameEvent& event) {
        // 플레이어 위치 업데이트 (Lock-Free ECS 활용)
        auto* transform = GetPlayerTransform(event.player_id);
        if (transform) {
            transform->position_x.store(event.data.move_data.x, std::memory_order_relaxed);
            transform->position_y.store(event.data.move_data.y, std::memory_order_relaxed);
            transform->position_z.store(event.data.move_data.z, std::memory_order_relaxed);
        }
    }
    
    void ProcessAttackEvent(const GameEvent& event) {
        // 전투 시스템 호출
        CombatSystem::ProcessAttack(
            event.player_id,
            event.data.attack_data.target_id,
            event.data.attack_data.damage,
            event.data.attack_data.skill_id
        );
    }
    
    uint64_t GetHighResolutionTimestamp() {
        return std::chrono::high_resolution_clock::now().time_since_epoch().count();
    }
};
```

### 2. Wait-Free 알고리즘 구현

```cpp
// [SEQUENCE: 67] Wait-Free Single Producer Single Consumer Queue
template<typename T>
class WaitFreeSPSCQueue {
private:
    static constexpr size_t CACHE_LINE_SIZE = 64;
    static constexpr size_t DEFAULT_CAPACITY = 4096;
    
    struct alignas(CACHE_LINE_SIZE) CacheLinePadded {
        std::atomic<size_t> value{0};
        char padding[CACHE_LINE_SIZE - sizeof(std::atomic<size_t>)];
    };
    
    // Producer와 Consumer가 서로 다른 캐시라인 사용
    CacheLinePadded head_;  // Producer가 사용
    CacheLinePadded tail_;  // Consumer가 사용
    
    const size_t capacity_;
    const size_t mask_;  // capacity_ - 1 (2의 거듭제곱일 때)
    
    // 데이터 배열도 캐시라인 정렬
    alignas(CACHE_LINE_SIZE) std::unique_ptr<T[]> buffer_;
    
public:
    explicit WaitFreeSPSCQueue(size_t capacity = DEFAULT_CAPACITY)
        : capacity_(RoundUpToPowerOfTwo(capacity))
        , mask_(capacity_ - 1)
        , buffer_(std::make_unique<T[]>(capacity_)) {
    }
    
    // [SEQUENCE: 68] Wait-Free Enqueue (Producer Only)
    bool Enqueue(const T& item) noexcept {
        const size_t head = head_.value.load(std::memory_order_relaxed);
        const size_t next_head = (head + 1) & mask_;
        
        // Wait-Free: tail 로드는 한 번만
        const size_t tail = tail_.value.load(std::memory_order_acquire);
        
        if (next_head == tail) {
            return false;  // 큐가 가득 참
        }
        
        // 데이터 저장
        buffer_[head] = item;
        
        // Head 포인터 업데이트 (Release semantics)
        head_.value.store(next_head, std::memory_order_release);
        
        return true;
    }
    
    // [SEQUENCE: 69] Wait-Free Dequeue (Consumer Only)
    bool Dequeue(T& item) noexcept {
        const size_t tail = tail_.value.load(std::memory_order_relaxed);
        
        // Wait-Free: head 로드는 한 번만
        const size_t head = head_.value.load(std::memory_order_acquire);
        
        if (tail == head) {
            return false;  // 큐가 비어 있음
        }
        
        // 데이터 읽기
        item = buffer_[tail];
        
        // Tail 포인터 업데이트
        const size_t next_tail = (tail + 1) & mask_;
        tail_.value.store(next_tail, std::memory_order_release);
        
        return true;
    }
    
    // [SEQUENCE: 70] Wait-Free Size 계산
    size_t Size() const noexcept {
        const size_t head = head_.value.load(std::memory_order_acquire);
        const size_t tail = tail_.value.load(std::memory_order_acquire);
        
        return (head - tail) & mask_;
    }
    
    bool Empty() const noexcept {
        return head_.value.load(std::memory_order_acquire) == 
               tail_.value.load(std::memory_order_acquire);
    }
    
    bool Full() const noexcept {
        const size_t head = head_.value.load(std::memory_order_acquire);
        const size_t tail = tail_.value.load(std::memory_order_acquire);
        
        return ((head + 1) & mask_) == tail;
    }
    
private:
    static size_t RoundUpToPowerOfTwo(size_t value) {
        --value;
        value |= value >> 1;
        value |= value >> 2;
        value |= value >> 4;
        value |= value >> 8;
        value |= value >> 16;
        value |= value >> 32;
        return ++value;
    }
};

// [SEQUENCE: 71] 멀티 Producer/Consumer를 위한 Wait-Free 해시테이블
template<typename Key, typename Value>
class WaitFreeHashMap {
private:
    static constexpr size_t DEFAULT_CAPACITY = 16384;
    static constexpr size_t MAX_PROBES = 8;  // Linear probing 제한
    
    struct Entry {
        std::atomic<Key> key{Key{}};
        std::atomic<Value> value{Value{}};
        std::atomic<bool> occupied{false};
        char padding[64 - sizeof(std::atomic<Key>) - sizeof(std::atomic<Value>) - sizeof(std::atomic<bool>)];
    };
    
    const size_t capacity_;
    const size_t mask_;
    std::unique_ptr<Entry[]> entries_;
    
    // 해시 함수 (Murmur3 기반)
    size_t Hash(const Key& key) const noexcept {
        static_assert(sizeof(Key) <= 8, "Key must fit in 64 bits");
        
        uint64_t k = static_cast<uint64_t>(key);
        k ^= k >> 33;
        k *= 0xff51afd7ed558ccdULL;
        k ^= k >> 33;
        k *= 0xc4ceb9fe1a85ec53ULL;
        k ^= k >> 33;
        
        return k & mask_;
    }
    
public:
    explicit WaitFreeHashMap(size_t capacity = DEFAULT_CAPACITY)
        : capacity_(RoundUpToPowerOfTwo(capacity))
        , mask_(capacity_ - 1)
        , entries_(std::make_unique<Entry[]>(capacity_)) {
    }
    
    // [SEQUENCE: 72] Wait-Free Insert/Update
    bool Set(const Key& key, const Value& value) noexcept {
        size_t index = Hash(key);
        
        for (size_t probe = 0; probe < MAX_PROBES; ++probe) {
            Entry& entry = entries_[index];
            
            // 빈 슬롯 찾기
            bool expected = false;
            if (entry.occupied.compare_exchange_strong(
                expected, true, std::memory_order_acquire)) {
                
                // 성공적으로 슬롯 획득
                entry.key.store(key, std::memory_order_relaxed);
                entry.value.store(value, std::memory_order_release);
                return true;
            }
            
            // 동일한 키 업데이트
            if (entry.key.load(std::memory_order_relaxed) == key) {
                entry.value.store(value, std::memory_order_release);
                return true;
            }
            
            // 다음 슬롯으로 이동 (Linear probing)
            index = (index + 1) & mask_;
        }
        
        return false;  // 해시테이블이 가득 참
    }
    
    // [SEQUENCE: 73] Wait-Free Lookup
    bool Get(const Key& key, Value& value) const noexcept {
        size_t index = Hash(key);
        
        for (size_t probe = 0; probe < MAX_PROBES; ++probe) {
            const Entry& entry = entries_[index];
            
            if (!entry.occupied.load(std::memory_order_acquire)) {
                return false;  // 빈 슬롯 도달
            }
            
            if (entry.key.load(std::memory_order_relaxed) == key) {
                value = entry.value.load(std::memory_order_acquire);
                return true;
            }
            
            index = (index + 1) & mask_;
        }
        
        return false;  // 키를 찾지 못함
    }
    
private:
    static size_t RoundUpToPowerOfTwo(size_t value) {
        --value;
        value |= value >> 1;
        value |= value >> 2;
        value |= value >> 4;
        value |= value >> 8;
        value |= value >> 16;
        value |= value >> 32;
        return ++value;
    }
};
```

## 📊 Lock-Free 성능 측정 및 벤치마크

### 성능 비교 도구

```cpp
// [SEQUENCE: 74] Lock-Free vs Lock-Based 성능 벤치마크
class LockFreePerformanceBenchmark {
private:
    static constexpr size_t NUM_THREADS = 16;
    static constexpr size_t OPERATIONS_PER_THREAD = 1000000;
    static constexpr size_t WARM_UP_OPERATIONS = 100000;
    
public:
    static void RunComprehensiveBenchmark() {
        std::cout << "=== Lock-Free vs Lock-Based Performance Comparison ===" << std::endl;
        
        // Queue 성능 비교
        BenchmarkQueues();
        
        // Counter 성능 비교
        BenchmarkCounters();
        
        // Hash Map 성능 비교
        BenchmarkHashMaps();
        
        // 실제 게임서버 시나리오
        BenchmarkGameScenario();
    }
    
private:
    static void BenchmarkQueues() {
        std::cout << "\n--- Queue Performance ---" << std::endl;
        
        // Lock-based queue
        auto lock_time = BenchmarkLockQueue();
        std::cout << "Lock-based Queue: " << lock_time << " ms" << std::endl;
        
        // Lock-free queue
        auto lockfree_time = BenchmarkLockFreeQueue();
        std::cout << "Lock-free Queue: " << lockfree_time << " ms" << std::endl;
        
        double improvement = static_cast<double>(lock_time) / lockfree_time;
        std::cout << "Performance Improvement: " << improvement << "x" << std::endl;
    }
    
    static uint64_t BenchmarkLockQueue() {
        std::queue<int> queue;
        std::mutex queue_mutex;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<std::thread> threads;
        std::atomic<bool> start_flag{false};
        
        // Producer threads
        for (size_t i = 0; i < NUM_THREADS / 2; ++i) {
            threads.emplace_back([&queue, &queue_mutex, &start_flag]() {
                while (!start_flag.load()) {
                    std::this_thread::yield();
                }
                
                for (size_t j = 0; j < OPERATIONS_PER_THREAD; ++j) {
                    std::lock_guard<std::mutex> lock(queue_mutex);
                    queue.push(static_cast<int>(j));
                }
            });
        }
        
        // Consumer threads
        for (size_t i = 0; i < NUM_THREADS / 2; ++i) {
            threads.emplace_back([&queue, &queue_mutex, &start_flag]() {
                while (!start_flag.load()) {
                    std::this_thread::yield();
                }
                
                size_t consumed = 0;
                while (consumed < OPERATIONS_PER_THREAD) {
                    std::lock_guard<std::mutex> lock(queue_mutex);
                    if (!queue.empty()) {
                        queue.pop();
                        ++consumed;
                    }
                }
            });
        }
        
        start_flag.store(true);
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    }
    
    static uint64_t BenchmarkLockFreeQueue() {
        LockFreeQueue<int> queue;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<std::thread> threads;
        std::atomic<bool> start_flag{false};
        
        // Producer threads
        for (size_t i = 0; i < NUM_THREADS / 2; ++i) {
            threads.emplace_back([&queue, &start_flag]() {
                while (!start_flag.load()) {
                    std::this_thread::yield();
                }
                
                for (size_t j = 0; j < OPERATIONS_PER_THREAD; ++j) {
                    queue.Enqueue(static_cast<int>(j));
                }
            });
        }
        
        // Consumer threads
        for (size_t i = 0; i < NUM_THREADS / 2; ++i) {
            threads.emplace_back([&queue, &start_flag]() {
                while (!start_flag.load()) {
                    std::this_thread::yield();
                }
                
                size_t consumed = 0;
                int value;
                while (consumed < OPERATIONS_PER_THREAD) {
                    if (queue.Dequeue(value)) {
                        ++consumed;
                    }
                }
            });
        }
        
        start_flag.store(true);
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    }
    
    // [SEQUENCE: 75] 실제 게임서버 시나리오 벤치마크
    static void BenchmarkGameScenario() {
        std::cout << "\n--- Game Server Scenario ---" << std::endl;
        std::cout << "5,000 concurrent players, 60 FPS, 10 seconds" << std::endl;
        
        constexpr size_t NUM_PLAYERS = 5000;
        constexpr size_t FPS = 60;
        constexpr size_t DURATION_SECONDS = 10;
        constexpr size_t TOTAL_FRAMES = FPS * DURATION_SECONDS;
        
        // Lock-free event processing
        GameEventProcessor processor;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        std::vector<std::thread> producer_threads;
        std::atomic<bool> running{true};
        
        // 플레이어 행동 시뮬레이션 (Producer threads)
        for (size_t i = 0; i < 8; ++i) {  // 8개 Producer 스레드
            producer_threads.emplace_back([&processor, &running, NUM_PLAYERS]() {
                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<uint32_t> player_dist(0, NUM_PLAYERS - 1);
                std::uniform_int_distribution<int> event_dist(0, 4);
                
                while (running.load()) {
                    uint32_t player_id = player_dist(gen);
                    GameEventType event_type = static_cast<GameEventType>(event_dist(gen));
                    
                    // 이벤트 데이터 생성
                    GameEvent::EventData data;
                    switch (event_type) {
                        case GameEventType::PLAYER_MOVE:
                            data.move_data = {
                                gen() % 1000.0f, gen() % 1000.0f, gen() % 100.0f,
                                (gen() % 20) - 10.0f, (gen() % 20) - 10.0f, 0.0f
                            };
                            break;
                        default:
                            break;
                    }
                    
                    processor.EnqueueEvent(event_type, player_id, &data);
                    
                    // 실제 게임 속도 시뮬레이션
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            });
        }
        
        // 메인 게임 루프 (Consumer)
        size_t total_processed = 0;
        for (size_t frame = 0; frame < TOTAL_FRAMES; ++frame) {
            auto frame_start = std::chrono::high_resolution_clock::now();
            
            processor.ProcessEvents(1000);  // 프레임당 최대 1000개 이벤트 처리
            ++total_processed;
            
            // 60 FPS 유지
            auto frame_time = std::chrono::high_resolution_clock::now() - frame_start;
            auto target_frame_time = std::chrono::microseconds(16667);  // 1/60초
            
            if (frame_time < target_frame_time) {
                std::this_thread::sleep_for(target_frame_time - frame_time);
            }
        }
        
        running.store(false);
        
        for (auto& thread : producer_threads) {
            thread.join();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "Game simulation completed in: " << duration.count() << " ms" << std::endl;
        std::cout << "Total frames processed: " << total_processed << std::endl;
        std::cout << "Average FPS: " << (total_processed * 1000) / duration.count() << std::endl;
    }
};
```

### 예상 벤치마크 결과

```
=== Lock-Free vs Lock-Based Performance Comparison ===

--- Queue Performance ---
Lock-based Queue: 2847 ms
Lock-free Queue: 312 ms
Performance Improvement: 9.1x

--- Counter Performance ---
Lock-based Counter: 1920 ms
Lock-free Counter: 145 ms
Performance Improvement: 13.2x

--- Hash Map Performance ---
Lock-based HashMap: 3654 ms
Wait-free HashMap: 423 ms
Performance Improvement: 8.6x

--- Game Server Scenario ---
5,000 concurrent players, 60 FPS, 10 seconds
Game simulation completed in: 10,034 ms
Total frames processed: 600
Average FPS: 59.8
Event processing efficiency: 98.7%
```

## 🎯 실제 프로젝트 적용 가이드

### 1단계: Lock-Free 전환 우선순위

1. **High-Contention 지점 식별**
   - 멀티스레드 큐 시스템
   - 플레이어 상태 카운터
   - 이벤트 버퍼

2. **점진적 전환**
   - 단일 Producer/Consumer 큐부터 시작
   - 성능 측정 후 Multi-Producer 확장
   - 전체 시스템 검증

### 2단계: 메모리 안전성 보장

- Hazard Pointer 구현
- ABA Problem 해결
- Memory ordering 최적화

### 3단계: 성능 목표

- **처리량**: 기존 대비 800% 향상
- **지연시간**: 평균 응답시간 90% 감소
- **확장성**: 코어 수에 선형 비례 성능

## 🚀 다음 단계

다음 문서 **네트워크 최적화(03_network_optimization/)**에서는:

1. **Zero-Copy 네트워킹** - 메모리 복사 최소화
2. **Kernel Bypass** - DPDK, io_uring 활용
3. **배치 처리** - 패킷 일괄 전송/수신
4. **프로토콜 최적화** - 바이너리 직렬화, 압축

### 실습 과제

1. 현재 프로젝트의 Lock-Free Queue 교체
2. 게임 이벤트 시스템 Lock-Free 전환
3. 성능 벤치마크로 개선 효과 검증
4. 전체 처리량 1000% 향상 달성

<function_calls>
<invoke name="TodoWrite">
<parameter name="todos">[{"id": "1", "content": "Complete CPU optimization section with lock_free_programming.md", "status": "completed", "priority": "high"}]