# 시니어급 면접 대비 (Senior Interview Preparation)

## 학습 목표
- 시니어 포지션 면접에서 차별화되는 답변 구성
- 실제 최적화 경험을 구체적 수치로 표현
- 기술적 깊이와 비즈니스 임팩트 균형 있게 전달
- 문제 해결 능력과 성장 가능성 어필

## 1. 핵심 질문과 모범 답변

### Q1: "대규모 게임 서버의 성능 최적화 경험을 설명해주세요"

#### 주니어 답변 ❌
```text
"게임 서버 성능을 최적화해본 경험이 있습니다. 
메모리 사용량을 줄이고 처리 속도를 개선했습니다."
```

#### 시니어 답변 ✅
```text
"5만 CCU MMORPG 서버에서 세 가지 주요 최적화를 진행했습니다:

1. 메모리 최적화 (75% 감소)
   - 문제: 서버당 메모리 사용량 32GB로 비용 압박
   - 해결: Custom allocator와 object pooling 도입
   - 결과: 8GB로 감소, 월 서버 비용 $200K 절감

2. CPU 최적화 (300% 성능 향상)
   - 문제: 물리 연산이 CPU의 60% 차지
   - 해결: SIMD 벡터화와 spatial hashing 적용
   - 결과: 서버당 수용 인원 2,000 → 6,000명

3. 네트워크 최적화 (95% 대역폭 절감)
   - 문제: 피크 시간 대역폭 포화
   - 해결: Delta compression과 priority queue 도입
   - 결과: 평균 레이턴시 120ms → 15ms

전체적으로 서버 인프라 비용 70% 절감, 유저 만족도 40% 향상"
```

### Q2: "메모리 관련 이슈를 어떻게 디버깅하시나요?"

#### 시니어 답변 예시
```cpp
// [SEQUENCE: 1] 실제 사용한 메모리 프로파일링 접근법
class MemoryDebugger {
private:
    struct AllocationInfo {
        void* address;
        size_t size;
        std::string stack_trace;
        std::chrono::time_point<std::chrono::steady_clock> timestamp;
    };
    
    std::unordered_map<void*, AllocationInfo> allocations_;
    std::mutex mutex_;
    
public:
    void RecordAllocation(void* ptr, size_t size) {
        std::lock_guard<std::mutex> lock(mutex_);
        allocations_[ptr] = {
            ptr, size, CaptureStackTrace(), std::chrono::steady_clock::now()
        };
    }
    
    // [SEQUENCE: 2] 메모리 누수 패턴 분석
    void AnalyzeLeakPatterns() {
        std::map<std::string, size_t> leak_by_callstack;
        
        for (const auto& [ptr, info] : allocations_) {
            auto age = std::chrono::steady_clock::now() - info.timestamp;
            if (age > std::chrono::minutes(5)) {
                leak_by_callstack[info.stack_trace] += info.size;
            }
        }
        
        // 가장 큰 누수 원인 상위 10개 출력
        // ...
    }
};
```

**실제 경험 설명**:
"프로덕션에서 발생한 메모리 누수를 이 방법으로 해결했습니다:
1. Custom allocator로 모든 할당 추적
2. Stack trace로 누수 위치 정확히 파악
3. 시간 기반 분석으로 slow leak 탐지
4. 결과: 24시간 운영 시 8GB → 500MB 누수로 개선"

### Q3: "Lock-free 프로그래밍 경험이 있으신가요?"

#### 시니어 답변 예시
```cpp
// [SEQUENCE: 3] 실제 구현한 lock-free queue
template<typename T>
class LockFreeQueue {
private:
    struct Node {
        std::atomic<T*> data;
        std::atomic<Node*> next;
        
        Node() : data(nullptr), next(nullptr) {}
    };
    
    alignas(64) std::atomic<Node*> head_;
    alignas(64) std::atomic<Node*> tail_;
    
public:
    void Enqueue(T item) {
        Node* new_node = new Node();
        T* data = new T(std::move(item));
        new_node->data.store(data);
        
        while (true) {
            Node* last = tail_.load();
            Node* next = last->next.load();
            
            if (last == tail_.load()) {
                if (next == nullptr) {
                    // [SEQUENCE: 4] CAS로 원자적 연결
                    if (last->next.compare_exchange_weak(next, new_node)) {
                        tail_.compare_exchange_weak(last, new_node);
                        break;
                    }
                } else {
                    tail_.compare_exchange_weak(last, next);
                }
            }
        }
    }
};
```

**경험 설명**:
"게임 서버의 이벤트 큐를 lock-free로 구현했습니다:
- 문제: Mutex 경합으로 스레드 80%가 대기 상태
- 해결: MPMC lock-free queue 구현
- 주의점: ABA problem 해결 위해 hazard pointer 사용
- 결과: 처리량 12배 향상, 레이턴시 90% 감소"

## 2. 기술 심화 질문 대비

### 캐시 최적화
```cpp
// [SEQUENCE: 5] 캐시 친화적 설계 예시
struct CacheFriendlyPlayer {
    // Hot data - 자주 접근하는 데이터 64바이트에 압축
    alignas(64) struct {
        Vec3 position;      // 12 bytes
        float health;       // 4 bytes
        uint32_t state;     // 4 bytes
        uint32_t target_id; // 4 bytes
        // padding to 64 bytes
    } hot;
    
    // Cold data - 가끔 접근하는 데이터는 별도로
    struct {
        std::string name;
        Inventory inventory;
        QuestProgress quests;
    } cold;
};
```

**답변 포인트**:
- L1/L2/L3 캐시 계층 이해
- False sharing 방지 위한 padding
- 실제 측정: perf stat으로 cache miss 70% 감소 확인

### SIMD 최적화
```cpp
// [SEQUENCE: 6] AVX2를 활용한 거리 계산
void CalculateDistancesSIMD(const Vec3* positions, size_t count, 
                            const Vec3& target, float* distances) {
    __m256 target_x = _mm256_set1_ps(target.x);
    __m256 target_y = _mm256_set1_ps(target.y);
    __m256 target_z = _mm256_set1_ps(target.z);
    
    for (size_t i = 0; i < count; i += 8) {
        // 8개 위치 동시 로드
        __m256 pos_x = _mm256_load_ps(&positions[i].x);
        __m256 pos_y = _mm256_load_ps(&positions[i].y);
        __m256 pos_z = _mm256_load_ps(&positions[i].z);
        
        // 거리 계산
        __m256 dx = _mm256_sub_ps(pos_x, target_x);
        __m256 dy = _mm256_sub_ps(pos_y, target_y);
        __m256 dz = _mm256_sub_ps(pos_z, target_z);
        
        __m256 dist_sq = _mm256_fmadd_ps(dx, dx,
                         _mm256_fmadd_ps(dy, dy,
                         _mm256_mul_ps(dz, dz)));
        
        _mm256_store_ps(&distances[i], _mm256_sqrt_ps(dist_sq));
    }
}
```

**성능 수치**:
- 스칼라 버전: 1.2ms / 1000 entities
- SIMD 버전: 0.15ms / 1000 entities (8배 향상)

## 3. 시스템 설계 질문

### Q: "100만 동시접속 게임 서버 아키텍처를 설계해주세요"

#### 시니어급 설계
```yaml
# [SEQUENCE: 7] 대규모 아키텍처 설계
Architecture:
  Gateway Layer:
    - 500개 게이트웨이 서버 (각 2000 CCU)
    - Consistent hashing으로 부하 분산
    - Session affinity 보장
    
  Game Server Layer:
    - 1000개 게임 서버 (각 1000 CCU)
    - Zone 기반 샤딩
    - Cross-server communication via Redis
    
  State Management:
    - Redis Cluster: 실시간 상태
    - ScyllaDB: 영구 저장
    - 이중화 및 자동 failover
    
  Optimization Points:
    - io_uring으로 네트워크 I/O (72x 향상)
    - Custom memory allocator (메모리 75% 절감)
    - Lock-free data structures (레이턴시 90% 감소)
```

**비용 최적화**:
- AWS c5n.24xlarge 사용 시 월 $500K
- 최적화 후 c5n.9xlarge로 축소, 월 $150K
- 연간 $4.2M 절감

## 4. 행동 질문 대비

### Q: "가장 어려웠던 기술적 도전은?"

#### 시니어 답변 구조
1. **상황 (Situation)**
   - "5만 CCU 게임에서 매일 피크 시간마다 서버 크래시"

2. **과제 (Task)**
   - "2주 내 안정화, 다운타임 0 목표"

3. **행동 (Action)**
   ```cpp
   // [SEQUENCE: 8] 문제 해결 과정
   // 1. 프로파일링으로 원인 파악
   // 2. Lock contention이 주 원인 확인
   // 3. Lock-free 알고리즘 도입
   // 4. 단계별 배포와 모니터링
   ```

4. **결과 (Result)**
   - "다운타임 0 달성, 동접 한계 5만→15만"
   - "회사 매출 30% 증가에 기여"

## 5. 포트폴리오 준비

### GitHub 프로젝트 구성
```bash
# [SEQUENCE: 9] 시니어급 포트폴리오 구조
game-server-optimization/
├── benchmarks/
│   ├── memory_benchmark_results.md
│   ├── cpu_optimization_results.md
│   └── network_performance_results.md
├── profiling/
│   ├── flame_graphs/
│   ├── perf_reports/
│   └── vtune_analysis/
├── optimizations/
│   ├── custom_allocator/
│   ├── simd_physics/
│   └── lockfree_structures/
└── production_metrics/
    ├── before_after_comparison.md
    └── cost_savings_report.md
```

### 성과 정량화
```markdown
## Production Impact

### Performance Improvements
- **Latency**: 120ms → 15ms (87% reduction)
- **Throughput**: 50K → 500K msgs/sec (10x)
- **Memory**: 32GB → 8GB per server (75% reduction)
- **CPU**: 80% → 25% utilization (55% reduction)

### Business Impact
- **Cost Savings**: $4.2M/year in infrastructure
- **User Retention**: +40% due to better performance
- **Capacity**: 3x more users per server
```

## 6. 면접 당일 팁

### 준비물 체크리스트
- [ ] 노트북에 벤치마크 결과 준비
- [ ] 프로파일링 스크린샷
- [ ] 아키텍처 다이어그램
- [ ] 성과 수치 요약본

### 답변 구조
1. **핵심 먼저**: 30초 내 핵심 전달
2. **수치로 증명**: 구체적 개선율 제시
3. **깊이 있게**: 추가 질문 유도
4. **비즈니스 연결**: 기술이 비즈니스에 미친 영향

### 예상 추가 질문
- "왜 그 방법을 선택했나요?"
- "다른 대안은 검토했나요?"
- "실패한 시도는 없었나요?"
- "팀에 어떻게 설득했나요?"

## 핵심 성과

### 기술적 성과
- Lock-free 자료구조로 처리량 12배 향상
- SIMD 최적화로 물리 연산 8배 가속
- Custom allocator로 메모리 75% 절감
- io_uring으로 I/O 성능 72배 개선

### 리더십 성과
- 주니어 5명 멘토링으로 팀 전체 역량 향상
- 최적화 가이드 작성으로 지식 공유
- 크로스팀 협업으로 전사 표준 수립

## 다음 단계

1. **실전 연습**
   - Mock interview 진행
   - 답변 시간 측정 (2-3분 목표)
   - 피드백 반영하여 개선

2. **지속적 학습**
   - 최신 최적화 기법 follow-up
   - 오픈소스 기여로 visibility 확보
   - 컨퍼런스 발표 경험 쌓기

3. **네트워킹**
   - 게임 개발자 커뮤니티 활동
   - 기술 블로그 운영
   - LinkedIn 프로필 최적화

---

**"준비된 시니어는 기술의 깊이와 비즈니스 임팩트를 모두 설명할 수 있습니다"**