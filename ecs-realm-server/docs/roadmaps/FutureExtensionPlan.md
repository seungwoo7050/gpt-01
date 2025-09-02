# Future Extension Plan - MMORPG Server Engine

## 현재 상태 분석 (2025-07-29)

### 프로젝트 개요
- **현재 수준**: 미드레벨 3-5년차 수준의 포트폴리오 프로젝트
- **목표 수준**: 시니어 7-10년차 수준의 프로덕션급 서버
- **완료 Phase**: 1-126 (기본 구조 및 기능 구현)
- **필요 Phase**: 127-150 (고급 최적화 및 프로덕션 강화)

### 주요 문제점

#### 1. 과대포장된 README vs 실제 구현
```
README 주장                    실제 상태
- 5,000+ 동시접속      →      테스트 없음
- 50만 PPS 처리        →      기본 Boost.Asio
- Anti-Cheat 시스템    →      기본 검증만
- 고급 AI 시스템       →      단순 FSM
- 분산 시스템          →      단일 서버
```

#### 2. 파일 구조 문제
- **현재**: 225개 파일 (과도한 구조)
- **실제 필요**: 약 80개 파일
- **문제**: 빈 구현, 중복 코드, 불필요한 클라이언트 요소

#### 3. C++20 미활용
- CMakeLists.txt에 C++20 설정만 있음
- 실제 코드는 C++11/14 스타일
- Modern C++ 기능 미사용

---

## 확장 계획: MVP19-23 (Phase 127-150)

### MVP19: 코드베이스 정리 및 C++20 현대화 (Phase 126-130)

#### Phase 126: 파일 정리 및 구조 단순화
**작업 내용:**
```bash
# 삭제 대상
- versions/ 폴더 (중복 코드)
- src/ui/ (서버에 불필요)
- src/housing/ (미구현)
- 빈 구현 파일들 (TODO만 있는 것)

# 통합 대상
- 유사 기능 헤더 파일들
- 중복된 유틸리티 함수들
```

**예상 결과:** 225개 → 80개 파일

#### Phase 127: Coroutines 전환
**현재 코드:**
```cpp
void Session::AsyncRead() {
    boost::asio::async_read(socket_, buffer_,
        [this](error_code ec, size_t bytes) {
            if (!ec) {
                ProcessData(bytes);
                AsyncRead();  // 재귀 호출
            }
        });
}
```

**개선 코드:**
```cpp
asio::awaitable<void> Session::HandleClient() {
    try {
        while (true) {
            size_t n = co_await async_read(socket_, buffer_, use_awaitable);
            co_await ProcessData(n);
        }
    } catch (std::exception& e) {
        // 에러 처리
    }
}
```

#### Phase 128: Concepts & Ranges 적용
**현재 코드:**
```cpp
template<typename T>
void BroadcastToPlayers(const T& packet) {
    // 타입 안전성 없음
}
```

**개선 코드:**
```cpp
template<typename T>
concept NetworkPacket = requires(T t) {
    { t.SerializeToString() } -> std::convertible_to<std::string>;
    { T::packet_type } -> std::convertible_to<PacketType>;
};

template<NetworkPacket T>
void BroadcastToPlayers(const T& packet) {
    auto active_players = players_
        | std::views::filter([](const auto& p) { return p.second->IsActive(); })
        | std::views::transform([&packet](const auto& p) { 
            return std::pair{p.first, packet}; 
        });
    
    for (const auto& [id, pkt] : active_players) {
        SendPacket(id, pkt);
    }
}
```

#### Phase 129: Module 시스템 (experimental)
```cpp
// network.ixx
export module mmorpg.network;

export namespace mmorpg::network {
    class TcpServer { ... };
    class Session { ... };
}

// main.cpp
import mmorpg.network;
```

#### Phase 130: 현대적 STL 활용
- std::format 대신 fmt 라이브러리
- std::jthread 사용
- std::stop_token으로 graceful shutdown
- std::latch/std::barrier 활용

### MVP20: 진짜 고성능 네트워킹 (Phase 131-135)

#### Phase 131: io_uring 기본 통합
**작업 내용:**
```cpp
class IoUringContext : public boost::asio::execution_context {
    struct io_uring ring_;
    
public:
    void submit_read(int fd, buffer_t& buf, handler_t handler);
    void submit_write(int fd, const buffer_t& buf, handler_t handler);
    void run();
};
```

#### Phase 132: Zero-copy buffer 구현
```cpp
class ZeroCopyBuffer {
    int fd_;
    void* mapped_region_;
    
public:
    // splice/sendfile 활용
    void send_to_socket(int socket_fd);
};
```

#### Phase 133: Kernel bypass (DPDK)
- DPDK 라이브러리 통합
- NIC에서 직접 패킷 처리
- 커널 스택 우회

#### Phase 134: TCP/UDP 하이브리드
```cpp
class HybridTransport {
    TcpConnection tcp_;  // 중요 데이터
    UdpSocket udp_;      // 위치 업데이트
    
    void SendReliable(packet_t packet) { tcp_.send(packet); }
    void SendUnreliable(packet_t packet) { udp_.send(packet); }
};
```

#### Phase 135: 50만 PPS 벤치마크
- 실제 부하 테스트 구현
- 성능 프로파일링
- 병목 지점 최적화

### MVP21: Lock-free 아키텍처 (Phase 136-140)

#### Phase 136: Custom memory pool
```cpp
template<typename T>
class ObjectPool {
    struct Node {
        std::atomic<Node*> next;
        alignas(T) char storage[sizeof(T)];
    };
    
    std::atomic<Node*> free_list_;
    
public:
    T* allocate();
    void deallocate(T* ptr);
};
```

#### Phase 137: MPMC lock-free queue
```cpp
template<typename T>
class LockFreeQueue {
    struct Node {
        std::atomic<T*> data;
        std::atomic<Node*> next;
    };
    
    alignas(64) std::atomic<Node*> head_;
    alignas(64) std::atomic<Node*> tail_;
    
public:
    void enqueue(T item);
    std::optional<T> dequeue();
};
```

#### Phase 138: RCU 패턴 구현
```cpp
template<typename T>
class RCUProtected {
    struct Version {
        T data;
        std::atomic<uint64_t> epoch;
    };
    
    std::atomic<Version*> current_;
    
public:
    RCUReader<T> read();
    void update(T new_data);
};
```

#### Phase 139: Hazard pointer
- 메모리 안전한 포인터 관리
- ABA 문제 해결

#### Phase 140: False sharing 최적화
```cpp
struct alignas(std::hardware_destructive_interference_size) 
CacheLinePadded {
    std::atomic<uint64_t> counter;
};
```

### MVP22: 분산 시스템 (Phase 141-145)

#### Phase 141: Raft consensus 기초
```cpp
class RaftNode {
    enum State { FOLLOWER, CANDIDATE, LEADER };
    
    State state_;
    uint64_t current_term_;
    std::vector<LogEntry> log_;
    
    void RequestVote();
    void AppendEntries();
    void HandleTimeout();
};
```

#### Phase 142: Consistent hashing
```cpp
class ConsistentHash {
    std::map<uint32_t, ServerNode> ring_;
    
    ServerNode GetServer(const std::string& key);
    void AddNode(ServerNode node);
    void RemoveNode(ServerNode node);
};
```

#### Phase 143: Event sourcing
```cpp
class EventStore {
    void Append(Event event);
    std::vector<Event> GetEvents(EntityId id, uint64_t from_version);
    State Replay(EntityId id);
};
```

#### Phase 144: 분산 트랜잭션
- 2PC (Two-Phase Commit)
- Saga 패턴
- 보상 트랜잭션

#### Phase 145: 서버간 동기화
- gRPC 통신
- 상태 복제
- 장애 감지 및 복구

### MVP23: 프로덕션 완성 (Phase 146-150)

#### Phase 146: 고급 모니터링 (eBPF)
```cpp
class eBPFMonitor {
    void AttachToKernel();
    void TraceSystemCalls();
    void ProfileCPUUsage();
    void AnalyzeNetworkLatency();
};
```

#### Phase 147: 보안 강화 (TLS 1.3)
- OpenSSL 통합
- Certificate pinning
- Perfect forward secrecy

#### Phase 148: 5000명 부하 테스트
- 실제 게임 시나리오
- 다양한 플레이 패턴
- 스트레스 테스트

#### Phase 149: 장애 복구 시스템
- Circuit breaker
- Bulkhead 패턴
- Graceful degradation

#### Phase 150: 최종 최적화
- Profile-guided optimization
- Link-time optimization
- Binary packing

---

## 구현 우선순위

### 즉시 필요 (High Priority)
1. Phase 126: 파일 정리 - 코드베이스 단순화
2. Phase 127-128: C++20 현대화 - 코드 품질 향상
3. Phase 131-132: 네트워킹 최적화 - 실제 성능 향상

### 중요함 (Medium Priority)
4. Phase 136-137: Lock-free 구조 - 확장성 향상
5. Phase 141-142: 분산 시스템 기초 - 수평 확장

### 장기 목표 (Low Priority)
6. Phase 146-150: 프로덕션 완성 - 실제 서비스 준비

---

## 예상 일정

- **MVP19** (파일정리 + C++20): 2주
- **MVP20** (고성능 네트워킹): 3주
- **MVP21** (Lock-free): 3주
- **MVP22** (분산 시스템): 4주
- **MVP23** (프로덕션): 2주

**총 예상 기간**: 14주 (약 3.5개월)

---

## 성공 지표

### 기술적 지표
- **네트워킹**: 실제 50만 PPS 달성
- **동시접속**: 5,000명 안정적 처리
- **레이턴시**: P99 < 50ms
- **CPU 사용률**: 코어당 < 70%
- **메모리**: < 16GB for 5,000 players

### 코드 품질
- **C++20 활용도**: > 80%
- **테스트 커버리지**: > 85%
- **정적 분석**: 0 warnings
- **문서화**: 모든 public API

---

## 참고 자료

### 필수 학습
1. C++20 Coroutines: https://en.cppreference.com/w/cpp/language/coroutines
2. io_uring: https://kernel.dk/io_uring.pdf
3. Lock-free programming: "The Art of Multiprocessor Programming"
4. Raft: https://raft.github.io/

### 추천 도서
- "C++ Concurrency in Action" (2nd Edition)
- "Game Programming Patterns"
- "High Performance Browser Networking"
- "Designing Data-Intensive Applications"

### 벤치마크 도구
- wrk2: HTTP 부하 테스트
- tcpkali: TCP 부하 테스트
- perf: Linux 성능 분석
- Flamegraph: CPU 프로파일링

---

## 주의사항

1. **점진적 개선**: 한 번에 모든 것을 바꾸지 말 것
2. **테스트 우선**: 각 Phase마다 성능 테스트 필수
3. **문서화**: DEVELOPMENT_JOURNEY.md 지속 업데이트
4. **호환성**: 기존 클라이언트와의 호환성 유지

이 계획을 따르면 현재의 미드레벨 포트폴리오를 진정한 시니어 레벨 프로덕션 서버로 발전시킬 수 있습니다.