# 심화 기술 질문

## 1. C++ 심화

### 메모리 모델과 원자성

**Q: C++11 메모리 모델에서 memory_order를 설명하고 게임 서버에서의 활용 예시를 보여주세요.**

```cpp
class LockFreeCounter {
    atomic<uint64_t> counter{0};
    
    // Relaxed ordering - 순서 보장 X, 가장 빠름
    void IncrementRelaxed() {
        counter.fetch_add(1, memory_order_relaxed);
    }
    
    // Acquire-Release - 동기화 보장
    class SpinLock {
        atomic<bool> locked{false};
        
        void lock() {
            while (locked.exchange(true, memory_order_acquire)) {
                // spin
            }
        }
        
        void unlock() {
            locked.store(false, memory_order_release);
        }
    };
    
    // Sequential Consistency - 완전한 순서 보장
    class GameState {
        atomic<State> currentState{State::INIT};
        
        void TransitionState(State newState) {
            State expected = currentState.load();
            
            // CAS 연산
            while (!currentState.compare_exchange_weak(
                expected, newState, 
                memory_order_seq_cst,  // 성공 시
                memory_order_seq_cst   // 실패 시
            )) {
                // retry
            }
        }
    };
};

// 실제 활용: Lock-free 플레이어 리스트
class LockFreePlayerList {
    struct Node {
        atomic<Node*> next;
        Player* player;
        
        Node(Player* p) : next(nullptr), player(p) {}
    };
    
    atomic<Node*> head{nullptr};
    
    void AddPlayer(Player* player) {
        Node* newNode = new Node(player);
        Node* prevHead = head.load(memory_order_relaxed);
        
        do {
            newNode->next.store(prevHead, memory_order_relaxed);
        } while (!head.compare_exchange_weak(
            prevHead, newNode,
            memory_order_release,
            memory_order_relaxed
        ));
    }
    
    void TraversePlayersSafe(function<void(Player*)> callback) {
        Node* current = head.load(memory_order_acquire);
        
        while (current) {
            callback(current->player);
            current = current->next.load(memory_order_acquire);
        }
    }
};
```

### 템플릿 메타프로그래밍

**Q: SFINAE와 Concepts를 활용한 타입 안전 직렬화 시스템을 구현하세요.**

```cpp
// C++17 SFINAE
template<typename T, typename = void>
struct is_serializable : false_type {};

template<typename T>
struct is_serializable<T, void_t<
    decltype(declval<T>().serialize(declval<ByteBuffer&>())),
    decltype(T::deserialize(declval<ByteBuffer&>()))
>> : true_type {};

// C++20 Concepts
template<typename T>
concept Serializable = requires(T t, ByteBuffer& buffer) {
    { t.serialize(buffer) } -> same_as<void>;
    { T::deserialize(buffer) } -> same_as<T>;
};

// 타입별 최적화된 직렬화
class TypeSafeSerializer {
    ByteBuffer buffer;
    
    // POD 타입 최적화
    template<typename T>
    enable_if_t<is_trivially_copyable_v<T>> Write(const T& value) {
        buffer.WriteBytes(&value, sizeof(T));
    }
    
    // 컨테이너 특수화
    template<typename T>
    void Write(const vector<T>& vec) {
        Write(static_cast<uint32_t>(vec.size()));
        
        if constexpr (is_trivially_copyable_v<T>) {
            // 벌크 복사
            buffer.WriteBytes(vec.data(), vec.size() * sizeof(T));
        } else {
            // 개별 직렬화
            for (const auto& item : vec) {
                Write(item);
            }
        }
    }
    
    // Concepts 활용 (C++20)
    template<Serializable T>
    void Write(const T& obj) {
        obj.serialize(buffer);
    }
    
    // 가변 템플릿으로 여러 타입 한번에
    template<typename... Args>
    void WriteMultiple(const Args&... args) {
        (Write(args), ...);  // Fold expression
    }
};

// 컴파일 타임 리플렉션 흉내
template<typename T>
class Reflectable {
    // 멤버 변수 자동 등록
    template<auto MemberPtr>
    struct Member {
        using Type = decltype(((T*)nullptr)->*MemberPtr);
        static constexpr auto ptr = MemberPtr;
    };
    
    template<typename... Members>
    static void SerializeMembers(const T& obj, ByteBuffer& buffer) {
        ((buffer.Write(obj.*Members::ptr)), ...);
    }
};

// 사용 예시
struct PlayerData : Reflectable<PlayerData> {
    int id;
    string name;
    Vector3 position;
    
    // 멤버 등록
    using Members = tuple<
        Member<&PlayerData::id>,
        Member<&PlayerData::name>,
        Member<&PlayerData::position>
    >;
};
```

### Move Semantics 최적화

**Q: Perfect Forwarding과 Move Semantics를 활용한 효율적인 이벤트 시스템을 구현하세요.**

```cpp
class OptimizedEventSystem {
    template<typename Event>
    class EventHandler {
        using HandlerFunc = function<void(Event&&)>;
        vector<HandlerFunc> handlers;
        
        // Perfect forwarding으로 등록
        template<typename F>
        void Subscribe(F&& handler) {
            handlers.emplace_back(forward<F>(handler));
        }
        
        // Universal reference로 이벤트 전달
        template<typename E>
        void Emit(E&& event) {
            for (auto& handler : handlers) {
                // 마지막 핸들러에게는 move, 나머지는 copy
                if (&handler == &handlers.back()) {
                    handler(forward<E>(event));
                } else {
                    handler(Event(event));  // copy
                }
            }
        }
    };
    
    // Move-only 이벤트 타입
    class LargeGameEvent {
        unique_ptr<uint8_t[]> data;
        size_t size;
        
    public:
        LargeGameEvent(size_t s) : size(s), data(make_unique<uint8_t[]>(s)) {}
        
        // Move constructor
        LargeGameEvent(LargeGameEvent&& other) noexcept
            : data(move(other.data)), size(other.size) {
            other.size = 0;
        }
        
        // Move assignment
        LargeGameEvent& operator=(LargeGameEvent&& other) noexcept {
            if (this != &other) {
                data = move(other.data);
                size = other.size;
                other.size = 0;
            }
            return *this;
        }
        
        // Copy 금지
        LargeGameEvent(const LargeGameEvent&) = delete;
        LargeGameEvent& operator=(const LargeGameEvent&) = delete;
    };
};
```

## 2. 네트워크 프로그래밍 심화

### Zero-Copy 네트워킹

**Q: Zero-Copy 기법을 활용한 고성능 네트워크 스택을 구현하세요.**

```cpp
class ZeroCopyNetworkStack {
    // sendfile 활용 (Linux)
    class FileTransfer {
        void SendFile(int socket, const string& filename) {
            int fd = open(filename.c_str(), O_RDONLY);
            struct stat stat_buf;
            fstat(fd, &stat_buf);
            
            // Zero-copy 전송
            off_t offset = 0;
            sendfile(socket, fd, &offset, stat_buf.st_size);
            
            close(fd);
        }
    };
    
    // Memory-mapped I/O
    class MMapBuffer {
        void* mapped_region;
        size_t size;
        
        void MapFile(const string& filename) {
            int fd = open(filename.c_str(), O_RDONLY);
            struct stat sb;
            fstat(fd, &sb);
            
            mapped_region = mmap(nullptr, sb.st_size, PROT_READ, 
                               MAP_PRIVATE, fd, 0);
            size = sb.st_size;
            
            // 힌트: 순차 읽기
            madvise(mapped_region, size, MADV_SEQUENTIAL);
            
            close(fd);
        }
        
        void SendMapped(int socket) {
            // MSG_ZEROCOPY 플래그 (Linux 4.14+)
            send(socket, mapped_region, size, MSG_ZEROCOPY);
        }
    };
    
    // Ring Buffer 기반 Zero-Copy
    class RingBufferIO {
        static constexpr size_t BUFFER_SIZE = 1 << 20;  // 1MB
        alignas(64) uint8_t buffer[BUFFER_SIZE];
        
        atomic<size_t> write_pos{0};
        atomic<size_t> read_pos{0};
        
        // Lock-free write
        bool Write(const void* data, size_t len) {
            size_t current_write = write_pos.load(memory_order_relaxed);
            size_t current_read = read_pos.load(memory_order_acquire);
            
            size_t available = (current_read + BUFFER_SIZE - current_write - 1) 
                              % BUFFER_SIZE;
            
            if (len > available) return false;
            
            // 순환 버퍼에 복사
            size_t first_part = min(len, BUFFER_SIZE - current_write);
            memcpy(buffer + current_write, data, first_part);
            
            if (first_part < len) {
                memcpy(buffer, (uint8_t*)data + first_part, len - first_part);
            }
            
            write_pos.store((current_write + len) % BUFFER_SIZE, 
                           memory_order_release);
            return true;
        }
        
        // scatter-gather I/O
        void SendToSocket(int socket) {
            size_t current_write = write_pos.load(memory_order_acquire);
            size_t current_read = read_pos.load(memory_order_relaxed);
            
            if (current_read == current_write) return;
            
            iovec iov[2];
            int iovcnt = 0;
            
            if (current_write > current_read) {
                iov[0].iov_base = buffer + current_read;
                iov[0].iov_len = current_write - current_read;
                iovcnt = 1;
            } else {
                iov[0].iov_base = buffer + current_read;
                iov[0].iov_len = BUFFER_SIZE - current_read;
                iov[1].iov_base = buffer;
                iov[1].iov_len = current_write;
                iovcnt = 2;
            }
            
            ssize_t sent = writev(socket, iov, iovcnt);
            read_pos.store((current_read + sent) % BUFFER_SIZE, 
                          memory_order_release);
        }
    };
};
```

### io_uring 활용

**Q: io_uring을 활용한 비동기 I/O 서버를 구현하세요.**

```cpp
class IoUringGameServer {
    struct io_uring ring;
    
    struct Connection {
        int fd;
        enum State { READING, WRITING } state;
        vector<uint8_t> read_buffer;
        vector<uint8_t> write_buffer;
    };
    
    unordered_map<int, unique_ptr<Connection>> connections;
    
    void InitializeRing() {
        struct io_uring_params params = {};
        params.flags = IORING_SETUP_SQPOLL;  // 커널 스레드 폴링
        params.sq_thread_idle = 10000;       // 10초 idle
        
        io_uring_queue_init_params(4096, &ring, &params);
    }
    
    void SubmitRead(Connection* conn) {
        struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
        
        conn->read_buffer.resize(4096);
        io_uring_prep_recv(sqe, conn->fd, 
                          conn->read_buffer.data(), 
                          conn->read_buffer.size(), 0);
        
        io_uring_sqe_set_data(sqe, conn);
        conn->state = Connection::READING;
    }
    
    void SubmitWrite(Connection* conn) {
        struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
        
        io_uring_prep_send(sqe, conn->fd,
                          conn->write_buffer.data(),
                          conn->write_buffer.size(), 0);
        
        io_uring_sqe_set_data(sqe, conn);
        conn->state = Connection::WRITING;
    }
    
    void ProcessCompletions() {
        struct io_uring_cqe* cqe;
        unsigned head;
        
        io_uring_for_each_cqe(&ring, head, cqe) {
            Connection* conn = (Connection*)io_uring_cqe_get_data(cqe);
            
            if (cqe->res < 0) {
                // 에러 처리
                connections.erase(conn->fd);
            } else {
                switch (conn->state) {
                    case Connection::READING:
                        ProcessGameData(conn, cqe->res);
                        SubmitWrite(conn);
                        break;
                    case Connection::WRITING:
                        SubmitRead(conn);
                        break;
                }
            }
        }
        
        io_uring_cq_advance(&ring, head);
    }
    
    // Batch 작업 제출
    void SubmitBatch() {
        vector<io_uring_sqe*> sqes;
        
        // 여러 작업 준비
        for (auto& [fd, conn] : connections) {
            if (conn->HasPendingData()) {
                auto* sqe = io_uring_get_sqe(&ring);
                PrepareOperation(sqe, conn.get());
                sqes.push_back(sqe);
            }
        }
        
        // 한번에 제출
        io_uring_submit(&ring);
    }
};
```

## 3. 동시성 프로그래밍 심화

### Lock-Free 자료구조

**Q: Lock-Free Queue를 구현하고 ABA 문제를 해결하세요.**

```cpp
template<typename T>
class LockFreeQueue {
    struct Node {
        atomic<T*> data;
        atomic<Node*> next;
        
        Node() : data(nullptr), next(nullptr) {}
    };
    
    // ABA 방지를 위한 포인터 + 카운터
    struct TaggedPointer {
        Node* ptr;
        uint32_t tag;
    };
    
    atomic<TaggedPointer> head;
    atomic<TaggedPointer> tail;
    
    // Hazard Pointer로 메모리 안전성 보장
    class HazardPointerManager {
        static constexpr int MAX_THREADS = 128;
        static constexpr int HAZARDS_PER_THREAD = 2;
        
        atomic<Node*> hazardPointers[MAX_THREADS * HAZARDS_PER_THREAD];
        thread_local int threadId = -1;
        
        Node* Protect(int index, atomic<TaggedPointer>& src) {
            TaggedPointer ptr;
            do {
                ptr = src.load();
                hazardPointers[threadId * HAZARDS_PER_THREAD + index] = ptr.ptr;
                // 재확인
            } while (src.load().ptr != ptr.ptr);
            
            return ptr.ptr;
        }
        
        void Release(int index) {
            hazardPointers[threadId * HAZARDS_PER_THREAD + index] = nullptr;
        }
        
        bool IsHazardous(Node* node) {
            for (int i = 0; i < MAX_THREADS * HAZARDS_PER_THREAD; i++) {
                if (hazardPointers[i].load() == node) {
                    return true;
                }
            }
            return false;
        }
    };
    
public:
    void Enqueue(T item) {
        Node* newNode = new Node;
        T* data = new T(move(item));
        newNode->data.store(data);
        
        while (true) {
            TaggedPointer last = tail.load();
            Node* tailNode = hazards.Protect(0, tail);
            
            if (last.ptr == tailNode) {
                Node* next = last.ptr->next.load();
                
                if (next == nullptr) {
                    // 꼬리에 추가 시도
                    if (last.ptr->next.compare_exchange_weak(next, newNode)) {
                        // 꼬리 포인터 이동
                        tail.compare_exchange_weak(last, {newNode, last.tag + 1});
                        hazards.Release(0);
                        break;
                    }
                } else {
                    // 꼬리 포인터 도움
                    tail.compare_exchange_weak(last, {next, last.tag + 1});
                }
            }
            
            hazards.Release(0);
        }
    }
    
    optional<T> Dequeue() {
        while (true) {
            TaggedPointer first = head.load();
            Node* headNode = hazards.Protect(0, head);
            TaggedPointer last = tail.load();
            Node* next = hazards.Protect(1, headNode->next);
            
            if (first.ptr == head.load().ptr) {
                if (first.ptr == last.ptr) {
                    if (next == nullptr) {
                        hazards.Release(0);
                        hazards.Release(1);
                        return nullopt;  // 큐가 비어있음
                    }
                    // 꼬리 포인터 도움
                    tail.compare_exchange_weak(last, {next, last.tag + 1});
                } else {
                    // 데이터 읽기
                    T* data = next->data.load();
                    if (data == nullptr) continue;
                    
                    // 헤드 이동
                    if (head.compare_exchange_weak(first, {next, first.tag + 1})) {
                        hazards.Release(0);
                        hazards.Release(1);
                        
                        T result = *data;
                        delete data;
                        
                        // 나중에 안전하게 삭제
                        RetireNode(first.ptr);
                        
                        return result;
                    }
                }
            }
            
            hazards.Release(0);
            hazards.Release(1);
        }
    }
};
```

### Coroutine 기반 동시성

**Q: C++20 Coroutine을 활용한 게임 로직을 구현하세요.**

```cpp
// Coroutine 기반 게임 로직
class CoroutineGameLogic {
    // Awaitable 타입
    template<typename T>
    struct Task {
        struct promise_type {
            T value;
            exception_ptr exception;
            
            Task get_return_object() {
                return {coroutine_handle<promise_type>::from_promise(*this)};
            }
            
            suspend_never initial_suspend() { return {}; }
            suspend_always final_suspend() noexcept { return {}; }
            
            void return_value(T val) { value = move(val); }
            void unhandled_exception() { exception = current_exception(); }
        };
        
        coroutine_handle<promise_type> handle;
        
        bool await_ready() { return handle.done(); }
        void await_suspend(coroutine_handle<> h) {
            // 스케줄러에 등록
            GameScheduler::Schedule(h);
        }
        T await_resume() {
            if (handle.promise().exception) {
                rethrow_exception(handle.promise().exception);
            }
            return handle.promise().value;
        }
    };
    
    // 게임 로직 예시
    Task<void> PlayerCombatSequence(Player& player, Enemy& enemy) {
        // 스킬 캐스팅
        co_await CastSkill(player, player.selectedSkill);
        
        // 애니메이션 대기
        co_await Delay(player.selectedSkill.animationTime);
        
        // 데미지 계산 (비동기 DB 조회 포함)
        float damage = co_await CalculateDamage(player, enemy);
        
        // 데미지 적용
        enemy.TakeDamage(damage);
        
        // 이펙트 재생
        co_await PlayEffect(enemy.position, "hit_effect");
        
        // 반격 체크
        if (enemy.CanCounter()) {
            co_await enemy.CounterAttack(player);
        }
    }
    
    // 시간 기반 awaiter
    struct DelayAwaiter {
        chrono::milliseconds duration;
        chrono::time_point<chrono::steady_clock> resumeTime;
        
        bool await_ready() { return duration.count() <= 0; }
        
        void await_suspend(coroutine_handle<> h) {
            resumeTime = chrono::steady_clock::now() + duration;
            TimerManager::Schedule(resumeTime, h);
        }
        
        void await_resume() {}
    };
    
    DelayAwaiter Delay(chrono::milliseconds ms) {
        return {ms};
    }
    
    // Generator 패턴
    template<typename T>
    struct Generator {
        struct promise_type {
            T current_value;
            
            Generator get_return_object() {
                return {coroutine_handle<promise_type>::from_promise(*this)};
            }
            
            suspend_always initial_suspend() { return {}; }
            suspend_always final_suspend() noexcept { return {}; }
            
            suspend_always yield_value(T value) {
                current_value = value;
                return {};
            }
            
            void return_void() {}
            void unhandled_exception() { terminate(); }
        };
        
        coroutine_handle<promise_type> handle;
        
        struct iterator {
            coroutine_handle<promise_type> handle;
            
            iterator& operator++() {
                handle.resume();
                return *this;
            }
            
            T operator*() const {
                return handle.promise().current_value;
            }
            
            bool operator!=(const iterator& other) const {
                return !handle.done();
            }
        };
        
        iterator begin() {
            handle.resume();
            return {handle};
        }
        
        iterator end() { return {nullptr}; }
    };
    
    // 무한 이벤트 스트림
    Generator<GameEvent> EventStream() {
        while (true) {
            if (auto event = PollEvent()) {
                co_yield *event;
            } else {
                co_await Delay(16ms);  // 60 FPS
            }
        }
    }
};
```

## 4. 데이터베이스 심화

### 분산 트랜잭션

**Q: 2PC(Two-Phase Commit)를 구현하고 장애 상황을 처리하세요.**

```cpp
class DistributedTransactionManager {
    enum class TransactionState {
        INIT,
        PREPARING,
        PREPARED,
        COMMITTING,
        COMMITTED,
        ABORTING,
        ABORTED
    };
    
    struct Transaction {
        string txId;
        TransactionState state;
        vector<string> participants;
        chrono::time_point<chrono::steady_clock> startTime;
    };
    
    class Coordinator {
        map<string, Transaction> transactions;
        
        bool ExecuteTransaction(const string& txId, 
                               const vector<DBOperation>& operations) {
            Transaction tx;
            tx.txId = txId;
            tx.state = TransactionState::INIT;
            tx.startTime = chrono::steady_clock::now();
            
            // Phase 1: Prepare
            tx.state = TransactionState::PREPARING;
            vector<future<bool>> prepareResults;
            
            for (const auto& op : operations) {
                string participantId = GetParticipant(op);
                tx.participants.push_back(participantId);
                
                prepareResults.push_back(
                    async(launch::async, [=]() {
                        return SendPrepare(participantId, txId, op);
                    })
                );
            }
            
            // 모든 참가자의 응답 대기
            bool allPrepared = true;
            for (auto& future : prepareResults) {
                try {
                    if (!future.get()) {
                        allPrepared = false;
                        break;
                    }
                } catch (...) {
                    allPrepared = false;
                    break;
                }
            }
            
            if (allPrepared) {
                tx.state = TransactionState::PREPARED;
                
                // Phase 2: Commit
                tx.state = TransactionState::COMMITTING;
                
                // 결정 로그 기록 (장애 복구용)
                LogDecision(txId, "COMMIT");
                
                vector<future<void>> commitResults;
                for (const auto& participant : tx.participants) {
                    commitResults.push_back(
                        async(launch::async, [=]() {
                            SendCommit(participant, txId);
                        })
                    );
                }
                
                // 커밋 완료 대기
                for (auto& future : commitResults) {
                    future.wait();
                }
                
                tx.state = TransactionState::COMMITTED;
                return true;
            } else {
                // Abort
                tx.state = TransactionState::ABORTING;
                LogDecision(txId, "ABORT");
                
                for (const auto& participant : tx.participants) {
                    SendAbort(participant, txId);
                }
                
                tx.state = TransactionState::ABORTED;
                return false;
            }
        }
        
        // 장애 복구
        void RecoverFromCrash() {
            auto pendingTxs = LoadPendingTransactions();
            
            for (const auto& tx : pendingTxs) {
                switch (tx.state) {
                    case TransactionState::PREPARING:
                    case TransactionState::PREPARED:
                        // 결정 로그 확인
                        if (HasDecisionLog(tx.txId)) {
                            if (GetDecision(tx.txId) == "COMMIT") {
                                ResumeCommit(tx);
                            } else {
                                ResumeAbort(tx);
                            }
                        } else {
                            // 타임아웃 체크
                            auto elapsed = chrono::steady_clock::now() - tx.startTime;
                            if (elapsed > TRANSACTION_TIMEOUT) {
                                AbortTransaction(tx);
                            } else {
                                // 참가자에게 상태 질의
                                QueryParticipants(tx);
                            }
                        }
                        break;
                        
                    case TransactionState::COMMITTING:
                        ResumeCommit(tx);
                        break;
                        
                    case TransactionState::ABORTING:
                        ResumeAbort(tx);
                        break;
                }
            }
        }
    };
};
```

## 5. 보안 심화

### 암호화와 인증

**Q: TLS 1.3 핸드셰이크를 구현하고 0-RTT를 지원하세요.**

```cpp
class TLS13Implementation {
    struct SessionTicket {
        vector<uint8_t> psk;  // Pre-shared key
        uint32_t maxEarlyData;
        chrono::time_point<chrono::steady_clock> expiry;
    };
    
    class Server {
        EVP_PKEY* serverKey;
        X509* serverCert;
        map<string, SessionTicket> sessionCache;
        
        void HandleClientHello(Connection& conn, const ClientHello& hello) {
            ServerHello response;
            
            // 0-RTT 체크
            if (hello.hasEarlyData && hello.pskIdentity) {
                auto it = sessionCache.find(*hello.pskIdentity);
                if (it != sessionCache.end() && it->second.expiry > now()) {
                    // 0-RTT 수락
                    response.selectedPsk = 0;
                    conn.earlyDataAccepted = true;
                    
                    // Early data 복호화 키 설정
                    DeriveEarlyDataKeys(it->second.psk, conn);
                    
                    // Early data 처리 시작
                    ProcessEarlyData(conn);
                }
            }
            
            // 키 교환
            if (!conn.earlyDataAccepted) {
                // ECDHE 키 생성
                EVP_PKEY* ephemeralKey = GenerateECDHEKey();
                response.keyShare = GetPublicKey(ephemeralKey);
                
                // 공유 비밀 계산
                vector<uint8_t> sharedSecret = ComputeSharedSecret(
                    ephemeralKey, hello.keyShare);
                
                // 핸드셰이크 키 유도
                DeriveHandshakeKeys(sharedSecret, conn);
            }
            
            // 서버 인증서
            response.certificate = serverCert;
            response.certificateVerify = SignHandshakeData(serverKey, conn);
            
            // 새 세션 티켓 발급
            if (hello.supportsPSK) {
                SessionTicket newTicket;
                newTicket.psk = GenerateRandomBytes(32);
                newTicket.maxEarlyData = 16384;  // 16KB
                newTicket.expiry = now() + 7_days;
                
                string ticketId = GenerateTicketId();
                sessionCache[ticketId] = newTicket;
                
                response.newSessionTicket = EncryptTicket(ticketId, newTicket);
            }
            
            SendServerHello(conn, response);
        }
        
        // 암호화 키 유도 (HKDF)
        void DeriveKeys(const vector<uint8_t>& secret, Connection& conn) {
            // Extract
            vector<uint8_t> earlySecret = HKDF_Extract(zero_salt, secret);
            
            // Expand
            conn.clientHandshakeKey = HKDF_Expand(earlySecret, 
                "client handshake key", 32);
            conn.serverHandshakeKey = HKDF_Expand(earlySecret, 
                "server handshake key", 32);
            
            // Application keys
            vector<uint8_t> masterSecret = HKDF_Extract(
                DeriveSecret(earlySecret, "derived"), secret);
            
            conn.clientAppKey = HKDF_Expand(masterSecret, 
                "client application key", 32);
            conn.serverAppKey = HKDF_Expand(masterSecret, 
                "server application key", 32);
        }
    };
};
```

## 면접 대비 포인트

1. **깊이 있는 이해**: 단순 사용이 아닌 내부 구현 원리
2. **트레이드오프**: 각 기술의 장단점과 적용 상황
3. **실전 경험**: 실제 적용 시 겪은 문제와 해결
4. **최신 표준**: C++20/23, 최신 네트워크 프로토콜
5. **성능 분석**: 벤치마크, 프로파일링 결과 제시