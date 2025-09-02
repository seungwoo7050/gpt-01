# 게임 서버 미니 프로젝트 가이드

## 🎯 이 문서의 목적

각 학습 단계별로 실습할 수 있는 미니 프로젝트를 제공하여 이론을 실제 코드로 구현하는 능력을 기릅니다.

## 🚀 프로젝트 난이도
- ⭐ **쉬움**: 1-2일 소요
- ⭐⭐ **보통**: 3-5일 소요
- ⭐⭐⭐ **어려움**: 1주일 이상 소요

---

## Phase 1: 기초 프로젝트

### 프로젝트 1: TCP 에코 서버 ⭐
**목표**: 네트워크 프로그래밍 기초 이해

**요구사항**:
```cpp
// 1. 포트 9999에서 리스닝
// 2. 클라이언트 메시지를 그대로 반환
// 3. 여러 클라이언트 동시 처리 (select 사용)
// 4. 연결/해제 로그 출력
```

**구현 힌트**:
```cpp
class EchoServer {
    int listen_fd;
    fd_set master_set;
    
public:
    void Start(int port) {
        // 1. 소켓 생성 및 바인드
        // 2. select()로 이벤트 대기
        // 3. 새 연결은 accept()
        // 4. 데이터는 recv() → send()
    }
};
```

**평가 기준**:
- [ ] 동시에 10개 클라이언트 처리
- [ ] 메모리 누수 없음
- [ ] 정상적인 종료 처리

### 프로젝트 2: 멀티스레드 채팅 서버 ⭐⭐
**목표**: 스레드와 동기화 이해

**요구사항**:
```cpp
// 1. 각 클라이언트별 스레드 생성
// 2. 닉네임 설정 기능
// 3. 브로드캐스트 메시지
// 4. /list, /whisper 명령어
// 5. 스레드 안전한 구현
```

**구조 설계**:
```cpp
class ChatServer {
    std::map<int, ClientInfo> clients;
    std::mutex clients_mutex;
    
    struct ClientInfo {
        int socket;
        std::string nickname;
        std::thread handler_thread;
    };
    
    void HandleClient(int client_socket) {
        // 클라이언트별 처리 로직
    }
    
    void Broadcast(const std::string& message) {
        std::lock_guard<std::mutex> lock(clients_mutex);
        // 모든 클라이언트에게 전송
    }
};
```

**평가 기준**:
- [ ] 경쟁 상태 없음
- [ ] 데드락 없음
- [ ] 50명 동시 채팅 가능

### 프로젝트 3: 게임 틱 시뮬레이터 ⭐⭐
**목표**: 게임 루프와 틱 시스템 이해

**요구사항**:
```cpp
// 1. 30 TPS (Tick Per Second) 구현
// 2. 플레이어 이동 시뮬레이션
// 3. 틱 지연 시 따라잡기
// 4. 성능 통계 출력
```

**핵심 구현**:
```cpp
class GameLoop {
    static constexpr int TARGET_TPS = 30;
    static constexpr auto TICK_DURATION = std::chrono::milliseconds(1000 / TARGET_TPS);
    
    void Run() {
        auto next_tick = std::chrono::steady_clock::now();
        
        while (running) {
            auto now = std::chrono::steady_clock::now();
            
            // 틱 처리
            ProcessTick();
            
            // 다음 틱까지 대기
            next_tick += TICK_DURATION;
            std::this_thread::sleep_until(next_tick);
            
            // 지연된 경우 따라잡기
            if (now > next_tick) {
                // 스킵된 틱 처리
            }
        }
    }
};
```

---

## Phase 2: 아키텍처 프로젝트

### 프로젝트 4: 미니 ECS 엔진 ⭐⭐⭐
**목표**: Entity-Component-System 구현

**요구사항**:
```cpp
// 1. Entity = ID
// 2. Component = 순수 데이터
// 3. System = 로직 처리
// 4. 1000개 엔티티 처리
```

**구현 예제**:
```cpp
// Components
struct PositionComponent {
    float x, y, z;
};

struct VelocityComponent {
    float vx, vy, vz;
};

// System
class MovementSystem {
    void Update(float deltaTime) {
        // PositionComponent + VelocityComponent를 가진 엔티티 처리
        for (auto entity : GetEntitiesWithComponents<PositionComponent, VelocityComponent>()) {
            auto& pos = GetComponent<PositionComponent>(entity);
            auto& vel = GetComponent<VelocityComponent>(entity);
            
            pos.x += vel.vx * deltaTime;
            pos.y += vel.vy * deltaTime;
            pos.z += vel.vz * deltaTime;
        }
    }
};
```

**평가 기준**:
- [ ] 컴포넌트 동적 추가/제거
- [ ] 시스템 실행 순서 제어
- [ ] 메모리 효율적 저장

### 프로젝트 5: 로그인 서버 분리 ⭐⭐
**목표**: 서버 아키텍처 분리

**요구사항**:
```cpp
// 1. 로그인 서버 (포트 8080)
// 2. 게임 서버 (포트 8081)
// 3. Redis를 통한 세션 공유
// 4. JWT 토큰 인증
```

**아키텍처**:
```
[클라이언트] → [로그인 서버] → [Redis]
     ↓                             ↑
[게임 서버] ←─────────────────────┘
```

**구현 포인트**:
```cpp
// 로그인 서버
class LoginServer {
    void HandleLogin(const LoginRequest& req) {
        // 1. DB에서 사용자 확인
        // 2. JWT 토큰 생성
        // 3. Redis에 세션 저장
        // 4. 토큰 반환
    }
};

// 게임 서버
class GameServer {
    void ValidateSession(const std::string& token) {
        // 1. JWT 검증
        // 2. Redis에서 세션 확인
        // 3. 게임 입장 허용
    }
};
```

### 프로젝트 6: 이벤트 시스템 ⭐⭐
**목표**: 이벤트 기반 아키텍처 구현

**요구사항**:
```cpp
// 1. 이벤트 발행/구독 패턴
// 2. 비동기 이벤트 처리
// 3. 이벤트 우선순위
// 4. 이벤트 로깅
```

**구현 예제**:
```cpp
template<typename EventType>
class EventBus {
    using Handler = std::function<void(const EventType&)>;
    std::vector<Handler> handlers;
    
public:
    void Subscribe(Handler handler) {
        handlers.push_back(handler);
    }
    
    void Publish(const EventType& event) {
        for (auto& handler : handlers) {
            handler(event);
        }
    }
};

// 사용 예
struct PlayerJoinEvent {
    uint64_t player_id;
    std::string nickname;
};

EventBus<PlayerJoinEvent> join_bus;
join_bus.Subscribe([](const PlayerJoinEvent& e) {
    std::cout << e.nickname << " joined!" << std::endl;
});
```

---

## Phase 3: 성능 프로젝트

### 프로젝트 7: Lock-Free 큐 ⭐⭐⭐
**목표**: 고성능 동시성 구현

**요구사항**:
```cpp
// 1. Compare-And-Swap 사용
// 2. ABA 문제 해결
// 3. 메모리 순서 보장
// 4. 벤치마크 구현
```

**기본 구조**:
```cpp
template<typename T>
class LockFreeQueue {
    struct Node {
        std::atomic<T*> data;
        std::atomic<Node*> next;
    };
    
    std::atomic<Node*> head;
    std::atomic<Node*> tail;
    
public:
    void Enqueue(T item) {
        Node* new_node = new Node{new T(std::move(item)), nullptr};
        Node* prev_tail = tail.exchange(new_node);
        prev_tail->next.store(new_node);
    }
    
    std::optional<T> Dequeue() {
        // CAS 루프로 구현
    }
};
```

### 프로젝트 8: 10K 동시접속 서버 ⭐⭐⭐
**목표**: 대규모 동시접속 처리

**요구사항**:
```cpp
// 1. epoll/IOCP 사용
// 2. 메모리 풀 구현
// 3. 제로 카피 최적화
// 4. 실시간 모니터링
```

**최적화 포인트**:
- 객체 풀링
- 버퍼 재사용
- 시스템 콜 최소화
- CPU 캐시 최적화

### 프로젝트 9: 분산 캐시 시스템 ⭐⭐⭐
**목표**: Redis 클러스터 활용

**요구사항**:
```cpp
// 1. 일관된 해싱
// 2. 캐시 무효화 전략
// 3. 핫 키 처리
// 4. 장애 복구
```

---

## Phase 4: 실전 프로젝트

### 프로젝트 10: 미니 MMORPG ⭐⭐⭐
**목표**: 완전한 게임 서버 구현

**핵심 기능**:
1. **캐릭터 시스템**
   - 이동, 전투, 레벨업
   - 인벤토리, 장비

2. **월드 시스템**
   - 맵 로딩, 인스턴스
   - NPC, 몬스터 AI

3. **소셜 시스템**
   - 파티, 길드
   - 채팅, 거래

4. **전투 시스템**
   - 스킬, 버프/디버프
   - PvP, PvE

**아키텍처**:
```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│Login Server │     │ Game Server │     │ Chat Server │
└─────────────┘     └─────────────┘     └─────────────┘
       │                    │                    │
       └────────────────────┴────────────────────┘
                            │
                     ┌─────────────┐
                     │    Redis    │
                     └─────────────┘
                            │
                     ┌─────────────┐
                     │    MySQL    │
                     └─────────────┘
```

## 📊 프로젝트 평가 기준

### 코드 품질
- [ ] 클린 코드 원칙 준수
- [ ] 적절한 주석과 문서화
- [ ] 단위 테스트 작성
- [ ] 에러 처리 완벽

### 성능
- [ ] 목표 동시접속 달성
- [ ] 메모리 사용량 최적화
- [ ] CPU 사용률 적정
- [ ] 네트워크 대역폭 효율

### 확장성
- [ ] 모듈화된 설계
- [ ] 수평적 확장 가능
- [ ] 설정 파일 분리
- [ ] 플러그인 시스템

## 🎓 학습 팁

### 프로젝트 진행 방법
1. **요구사항 분석** → 무엇을 만들 것인가?
2. **설계** → 어떻게 만들 것인가?
3. **구현** → 단계별로 개발
4. **테스트** → 부하 테스트 필수
5. **최적화** → 병목 지점 개선

### 디버깅 팁
```cpp
// 성능 측정 매크로
#define MEASURE_TIME(name, code) \
    do { \
        auto start = std::chrono::high_resolution_clock::now(); \
        code \
        auto end = std::chrono::high_resolution_clock::now(); \
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start); \
        std::cout << name << ": " << duration.count() << "μs" << std::endl; \
    } while(0)

// 사용 예
MEASURE_TIME("Database Query", {
    auto result = db.Query("SELECT * FROM users");
});
```

---

💡 **Remember**: 완벽한 코드보다 동작하는 코드를 먼저 만드세요. 그 다음 개선하세요!