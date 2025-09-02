# 게임 서버 개발 트러블슈팅 가이드

## 🎯 이 문서의 목적

게임 서버 개발과 운영 중 자주 발생하는 문제들과 실전 해결 방법을 정리했습니다. 실제 경험에서 나온 노하우를 공유합니다.

## 🔥 네트워킹 관련 문제

### 문제 1: "Connection refused" 에러

**증상**:
```
connect() failed: Connection refused
```

**원인들**:
1. 서버가 실행되지 않음
2. 방화벽이 포트를 차단
3. 잘못된 IP/포트
4. 서버가 다른 인터페이스에 바인드됨

**해결 방법**:
```bash
# 1. 서버 프로세스 확인
ps aux | grep game_server

# 2. 포트 리스닝 확인
netstat -tlnp | grep 8080
lsof -i :8080

# 3. 방화벽 확인
sudo iptables -L -n
sudo ufw status

# 4. 모든 인터페이스에 바인드
server.bind("0.0.0.0", 8080);  // 127.0.0.1이 아닌 0.0.0.0
```

### 문제 2: 대량 접속 시 "Too many open files"

**증상**:
```
accept() failed: Too many open files
```

**원인**: 파일 디스크립터 한계 도달

**해결 방법**:
```bash
# 현재 한계 확인
ulimit -n

# 임시 증가
ulimit -n 65535

# 영구 설정 (/etc/security/limits.conf)
* soft nofile 65535
* hard nofile 65535

# 시스템 전체 설정 (/etc/sysctl.conf)
fs.file-max = 2097152
```

**코드 레벨 해결**:
```cpp
// 사용하지 않는 소켓 즉시 닫기
class Session {
    ~Session() {
        if (socket_.is_open()) {
            socket_.close();
        }
    }
};

// 타임아웃된 연결 정리
void CleanupTimeoutConnections() {
    auto now = std::chrono::steady_clock::now();
    for (auto it = sessions_.begin(); it != sessions_.end(); ) {
        if (now - it->last_activity > timeout_duration) {
            it = sessions_.erase(it);
        } else {
            ++it;
        }
    }
}
```

### 문제 3: 네트워크 지연 급증

**증상**:
- 평소 50ms → 갑자기 500ms+
- 간헐적 끊김 현상

**진단 도구**:
```cpp
class NetworkDiagnostics {
    struct PingStats {
        uint64_t sent = 0;
        uint64_t received = 0;
        double min_rtt = DBL_MAX;
        double max_rtt = 0;
        double avg_rtt = 0;
        double jitter = 0;
    };
    
    void MeasureLatency(Session& session) {
        auto start = std::chrono::high_resolution_clock::now();
        
        session.SendPing();
        session.OnPong = [start, this](auto pong_time) {
            auto rtt = std::chrono::duration<double, std::milli>(
                pong_time - start).count();
            
            UpdateStats(rtt);
            
            if (rtt > 200.0) {  // 200ms 이상이면 경고
                LOG_WARN("High latency detected: {} ms", rtt);
                // Nagle 알고리즘 비활성화 시도
                session.SetNoDelay(true);
            }
        };
    }
};
```

**해결 방법**:
1. **Nagle 알고리즘 비활성화**:
```cpp
socket_.set_option(tcp::no_delay(true));
```

2. **패킷 배칭 구현**:
```cpp
class PacketBatcher {
    std::vector<Packet> pending_packets_;
    std::chrono::milliseconds batch_interval_{5};
    
    void AddPacket(Packet packet) {
        pending_packets_.push_back(std::move(packet));
        
        if (pending_packets_.size() >= 10 || 
            ShouldFlush()) {
            Flush();
        }
    }
};
```

## 💥 동시성 관련 문제

### 문제 4: 데드락 발생

**증상**:
- 서버가 멈춤 (응답 없음)
- CPU 사용률 0%
- 스레드가 대기 상태

**디버깅 방법**:
```bash
# GDB로 데드락 확인
gdb -p $(pgrep game_server)
(gdb) thread apply all bt
```

**예방 코드**:
```cpp
// 1. Lock 순서 지정
class LockManager {
    enum LockOrder {
        PLAYER_LOCK = 1,
        INVENTORY_LOCK = 2,
        WORLD_LOCK = 3
    };
    
    template<typename... Locks>
    void AcquireInOrder(Locks&... locks) {
        // 항상 같은 순서로 획득
        (void)std::initializer_list<int>{
            (locks.lock(), 0)...
        };
    }
};

// 2. Lock-free 대안 사용
template<typename T>
class LockFreeStack {
    struct Node {
        T data;
        std::atomic<Node*> next;
    };
    std::atomic<Node*> head{nullptr};
    
public:
    void push(T value) {
        Node* new_node = new Node{std::move(value), head.load()};
        while (!head.compare_exchange_weak(new_node->next, new_node));
    }
};

// 3. Timeout 있는 lock
template<typename Mutex>
bool TryLockWithTimeout(Mutex& mutex, std::chrono::milliseconds timeout) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    
    while (std::chrono::steady_clock::now() < deadline) {
        if (mutex.try_lock()) {
            return true;
        }
        std::this_thread::yield();
    }
    
    LOG_ERROR("Lock timeout - possible deadlock!");
    return false;
}
```

### 문제 5: 레이스 컨디션

**증상**:
- 간헐적으로 이상한 값
- 재현이 어려움
- 부하가 높을 때 자주 발생

**탐지 도구**:
```bash
# ThreadSanitizer 사용
g++ -fsanitize=thread -g game_server.cpp
./game_server

# Helgrind 사용
valgrind --tool=helgrind ./game_server
```

**해결 패턴**:
```cpp
// 1. 원자적 연산 사용
class PlayerStats {
    std::atomic<int> kill_count{0};
    std::atomic<int> death_count{0};
    
    void AddKill() {
        kill_count.fetch_add(1, std::memory_order_relaxed);
    }
};

// 2. 불변 객체 패턴
class ImmutablePlayer {
    const std::string name_;
    const int level_;
    const Position position_;
    
public:
    // 변경 시 새 객체 생성
    ImmutablePlayer MoveTo(Position new_pos) const {
        return ImmutablePlayer(name_, level_, new_pos);
    }
};

// 3. 액터 모델
class PlayerActor {
    std::queue<std::function<void()>> mailbox_;
    std::mutex mailbox_mutex_;
    
public:
    void SendMessage(std::function<void()> msg) {
        {
            std::lock_guard<std::mutex> lock(mailbox_mutex_);
            mailbox_.push(std::move(msg));
        }
        cv_.notify_one();
    }
    
    void ProcessMessages() {
        // 단일 스레드에서 처리
    }
};
```

## 🎮 게임 로직 문제

### 문제 6: 위치 동기화 불일치

**증상**:
- 클라이언트와 서버의 위치가 다름
- 순간이동 현상
- 벽 통과 버그

**해결 방법**:

1. **서버 권위적 모델**:
```cpp
class PositionValidator {
    bool ValidateMovement(
        const Position& from, 
        const Position& to, 
        float delta_time,
        float max_speed) {
        
        float distance = Distance(from, to);
        float max_distance = max_speed * delta_time * 1.1f; // 10% 여유
        
        if (distance > max_distance) {
            LOG_WARN("Speed hack detected: {} > {}", 
                     distance, max_distance);
            return false;
        }
        
        // 충돌 검사
        if (HasCollision(from, to)) {
            return false;
        }
        
        return true;
    }
};
```

2. **클라이언트 예측 + 서버 조정**:
```cpp
class ClientPrediction {
    struct StateSnapshot {
        uint32_t tick;
        Position position;
        Velocity velocity;
    };
    
    std::deque<StateSnapshot> history_;
    
    void ReconcileWithServer(uint32_t server_tick, 
                           const Position& server_pos) {
        // 서버 틱 시점의 클라이언트 상태 찾기
        auto it = std::find_if(history_.begin(), history_.end(),
            [server_tick](const auto& snap) {
                return snap.tick == server_tick;
            });
        
        if (it != history_.end()) {
            // 차이가 크면 서버 위치로 스냅
            if (Distance(it->position, server_pos) > 1.0f) {
                current_position_ = server_pos;
                // 이후 입력 재적용
                ReapplyInputsAfter(server_tick);
            }
        }
    }
};
```

### 문제 7: 메모리 사용량 폭증

**증상**:
- 시간이 지날수록 메모리 증가
- OOM Killer에 의한 프로세스 종료

**메모리 프로파일링**:
```cpp
class MemoryProfiler {
    struct AllocationInfo {
        size_t size;
        std::string location;
        std::chrono::time_point<std::chrono::steady_clock> time;
    };
    
    std::unordered_map<void*, AllocationInfo> allocations_;
    std::mutex mutex_;
    
    void* TrackedAlloc(size_t size, const std::string& location) {
        void* ptr = malloc(size);
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            allocations_[ptr] = {size, location, 
                               std::chrono::steady_clock::now()};
        }
        
        return ptr;
    }
    
    void PrintLeaks() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        size_t total_leaked = 0;
        for (const auto& [ptr, info] : allocations_) {
            std::cout << "Leak: " << info.size << " bytes at " 
                     << info.location << std::endl;
            total_leaked += info.size;
        }
        
        std::cout << "Total leaked: " << total_leaked << " bytes" << std::endl;
    }
};

// 메모리 풀 사용
template<typename T>
class ObjectPool {
    std::stack<std::unique_ptr<T>> available_;
    std::mutex mutex_;
    
public:
    std::shared_ptr<T> Acquire() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (available_.empty()) {
            return std::make_shared<T>();
        }
        
        auto obj = std::move(available_.top());
        available_.pop();
        
        return std::shared_ptr<T>(obj.release(), 
            [this](T* ptr) {
                std::lock_guard<std::mutex> lock(mutex_);
                available_.push(std::unique_ptr<T>(ptr));
            });
    }
};
```

## 🚀 성능 문제

### 문제 8: 틱레이트 저하

**증상**:
- 목표 30 TPS → 실제 20 TPS
- 점점 느려지는 게임

**프로파일링**:
```cpp
class TickProfiler {
    struct SystemProfile {
        std::string name;
        double total_time = 0;
        uint64_t call_count = 0;
        double max_time = 0;
    };
    
    std::unordered_map<std::string, SystemProfile> profiles_;
    
    template<typename Func>
    void Profile(const std::string& name, Func&& func) {
        auto start = std::chrono::high_resolution_clock::now();
        
        func();
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration<double, std::milli>(
            end - start).count();
        
        auto& profile = profiles_[name];
        profile.name = name;
        profile.total_time += duration;
        profile.call_count++;
        profile.max_time = std::max(profile.max_time, duration);
        
        if (duration > 10.0) {  // 10ms 이상이면 경고
            LOG_WARN("Slow system: {} took {} ms", name, duration);
        }
    }
    
    void PrintReport() {
        std::cout << "\n=== Tick Profile Report ===" << std::endl;
        for (const auto& [name, profile] : profiles_) {
            double avg = profile.total_time / profile.call_count;
            std::cout << name << ": avg=" << avg 
                     << "ms, max=" << profile.max_time << "ms" << std::endl;
        }
    }
};
```

**최적화 전략**:
1. **시스템 병렬화**:
```cpp
void ParallelSystemUpdate() {
    std::vector<std::future<void>> futures;
    
    // 독립적인 시스템들은 병렬 실행
    futures.push_back(std::async(std::launch::async, 
        [this] { movement_system_.Update(); }));
    futures.push_back(std::async(std::launch::async, 
        [this] { ai_system_.Update(); }));
    
    // 모두 완료 대기
    for (auto& f : futures) {
        f.wait();
    }
    
    // 의존성 있는 시스템은 순차 실행
    combat_system_.Update();
    network_sync_system_.Update();
}
```

2. **공간 분할 최적화**:
```cpp
class SpatialGrid {
    static constexpr int CELL_SIZE = 100;
    std::unordered_map<int, std::vector<Entity*>> cells_;
    
    int GetCellIndex(float x, float y) {
        int cx = static_cast<int>(x / CELL_SIZE);
        int cy = static_cast<int>(y / CELL_SIZE);
        return cx + cy * 10000;  // 2D to 1D
    }
    
    std::vector<Entity*> GetNearbyEntities(float x, float y, float radius) {
        std::vector<Entity*> result;
        
        int min_cx = (x - radius) / CELL_SIZE;
        int max_cx = (x + radius) / CELL_SIZE;
        int min_cy = (y - radius) / CELL_SIZE;
        int max_cy = (y + radius) / CELL_SIZE;
        
        // 관련 셀만 검사
        for (int cy = min_cy; cy <= max_cy; cy++) {
            for (int cx = min_cx; cx <= max_cx; cx++) {
                int idx = cx + cy * 10000;
                if (cells_.count(idx)) {
                    for (auto* entity : cells_[idx]) {
                        if (Distance(x, y, entity->x, entity->y) <= radius) {
                            result.push_back(entity);
                        }
                    }
                }
            }
        }
        
        return result;
    }
};
```

## 📊 모니터링과 디버깅

### 실시간 모니터링 시스템

```cpp
class ServerMonitor {
    void StartHttpServer(int port) {
        httplib::Server svr;
        
        svr.Get("/metrics", [this](const auto& req, auto& res) {
            std::stringstream ss;
            
            ss << "# HELP player_count Current number of players\n";
            ss << "player_count " << GetPlayerCount() << "\n";
            
            ss << "# HELP tick_rate Current server tick rate\n";
            ss << "tick_rate " << GetTickRate() << "\n";
            
            ss << "# HELP memory_usage_bytes Memory usage in bytes\n";
            ss << "memory_usage_bytes " << GetMemoryUsage() << "\n";
            
            res.set_content(ss.str(), "text/plain");
        });
        
        svr.Get("/profile", [this](const auto& req, auto& res) {
            res.set_content(GetProfileReport(), "text/html");
        });
        
        svr.listen("0.0.0.0", port);
    }
};
```

### 크래시 덤프 분석

```cpp
void SetupCrashHandler() {
    std::signal(SIGSEGV, [](int sig) {
        void* array[50];
        size_t size = backtrace(array, 50);
        
        std::cerr << "Error: signal " << sig << ":\n";
        backtrace_symbols_fd(array, size, STDERR_FILENO);
        
        // 미니덤프 생성
        CreateMinidump();
        
        std::exit(1);
    });
}
```

## 🆘 긴급 대응 가이드

### 서버가 멈췄을 때
1. **프로세스 상태 확인**: `ps aux | grep game_server`
2. **CPU/메모리 확인**: `top -p $(pgrep game_server)`
3. **스택 트레이스**: `gdb -p $(pgrep game_server)` → `thread apply all bt`
4. **로그 확인**: `tail -f game_server.log`

### 성능이 급격히 저하될 때
1. **네트워크 상태**: `netstat -an | grep ESTABLISHED | wc -l`
2. **디스크 I/O**: `iotop -p $(pgrep game_server)`
3. **캐시 상태**: `redis-cli info stats`
4. **DB 슬로우 쿼리**: `SHOW PROCESSLIST`

---

💡 **Remember**: 문제가 발생했을 때 당황하지 마세요. 체계적으로 하나씩 확인하면 반드시 원인을 찾을 수 있습니다!