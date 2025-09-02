# 캐시 친화적 설계로 게임서버 처리량 500% 폭증시키기

## 🎯 CPU 캐시의 현실적 중요성

### 메모리 계층구조와 성능 격차

```
CPU Register:    1 cycle    (기준)
L1 Cache:        3-4 cycles (4x slower)
L2 Cache:        10-25 cycles (25x slower)
L3 Cache:        40-75 cycles (75x slower)
Main Memory:     200-300 cycles (300x slower)
```

**게임서버에서 캐시 미스의 파괴력:**
- 1초에 1억 번의 컴포넌트 접근
- 캐시 미스 10% → 3천만 번의 300 사이클 지연
- 총 90억 사이클 낭비 = CPU 성능 90% 감소

### 현재 프로젝트의 캐시 비효율성 분석

```cpp
// 현재 ECS 구현의 문제점 (src/core/ecs/constrained_ecs.h)
template<typename ComponentType>
std::unordered_map<EntityIdType, ComponentType>& GetComponentStorage() {
    static std::unordered_map<EntityIdType, ComponentType> storage;
    return storage;
}

// 문제 1: 노드 기반 할당으로 메모리 분산
// 문제 2: 포인터 체이싱으로 캐시 미스 유발  
// 문제 3: 해시 계산 오버헤드
```

## 🔧 Data-Oriented Design (DOD) 혁명

### 1. Structure of Arrays (SoA) vs Array of Structures (AoS)

```cpp
// [SEQUENCE: 27] AoS (현재 방식) - 캐시 비효율적
struct Transform_AoS {
    struct Data {
        Vector3 position;    // 12 bytes
        Vector3 rotation;    // 12 bytes  
        Vector3 scale;       // 12 bytes
        float padding[4];    // 16 bytes (캐시라인 정렬)
    };
    std::vector<Data> components;  // 52바이트씩 분산 저장
};

// [SEQUENCE: 28] SoA (최적화) - 캐시 초효율적
struct Transform_SoA {
    // 동일한 데이터 타입끼리 연속 배치
    alignas(64) std::vector<float> position_x;
    alignas(64) std::vector<float> position_y; 
    alignas(64) std::vector<float> position_z;
    alignas(64) std::vector<float> rotation_x;
    alignas(64) std::vector<float> rotation_y;
    alignas(64) std::vector<float> rotation_z;
    alignas(64) std::vector<float> scale_x;
    alignas(64) std::vector<float> scale_y;
    alignas(64) std::vector<float> scale_z;
    
    size_t count = 0;
    
    // [SEQUENCE: 29] 캐시 친화적 일괄 처리
    void UpdatePositions(const std::vector<float>& velocity_x,
                        const std::vector<float>& velocity_y,
                        const std::vector<float>& velocity_z,
                        float delta_time) {
        // SIMD 벡터화 가능한 루프
        for (size_t i = 0; i < count; ++i) {
            position_x[i] += velocity_x[i] * delta_time;
            position_y[i] += velocity_y[i] * delta_time; 
            position_z[i] += velocity_z[i] * delta_time;
        }
        // 컴파일러가 자동으로 SIMD 명령어 생성
        // 캐시라인당 16개 float 동시 처리 가능
    }
};
```

### 2. 캐시라인 인식 데이터 구조

```cpp
// [SEQUENCE: 30] 64바이트 캐시라인 최적화 컴포넌트
struct alignas(64) CacheOptimizedComponent {
    // 핫 데이터 (자주 접근) - 첫 번째 캐시라인
    float position_x, position_y, position_z;  // 12 bytes
    float velocity_x, velocity_y, velocity_z;  // 12 bytes  
    uint32_t entity_id;                        // 4 bytes
    uint32_t flags;                            // 4 bytes
    // 32 bytes 사용, 32 bytes 남음
    
    // 추가 핫 데이터
    float health;                              // 4 bytes
    float max_health;                          // 4 bytes  
    uint32_t level;                            // 4 bytes
    uint32_t experience;                       // 4 bytes
    // 48 bytes 사용, 16 bytes 남음
    
    // 패딩으로 캐시라인 경계 맞춤
    char padding[16];
    
    // 콜드 데이터 (가끔 접근) - 두 번째 캐시라인
    alignas(64) char name[32];                 // 32 bytes
    uint64_t creation_time;                    // 8 bytes
    uint64_t last_update_time;                 // 8 bytes
    char reserved[16];                         // 16 bytes
};

static_assert(sizeof(CacheOptimizedComponent) == 128, 
              "Component must be exactly 2 cache lines");
```

### 3. 메모리 접근 패턴 최적화

```cpp
// [SEQUENCE: 31] 캐시 친화적 ECS 시스템
template<typename... Components>
class CacheFriendlyECS {
private:
    // 각 컴포넌트 타입별 SoA 저장소
    std::tuple<std::vector<Components>...> component_arrays_;
    
    // 엔티티-컴포넌트 매핑 (Sparse Set)
    std::vector<uint32_t> dense_entities_;     // 압축된 엔티티 배열
    std::vector<uint32_t> sparse_to_dense_;    // EntityId -> dense 인덱스
    
    size_t entity_count_ = 0;
    
public:
    // [SEQUENCE: 32] 메모리 사전 할당으로 재할당 방지
    void Reserve(size_t capacity) {
        dense_entities_.reserve(capacity);
        sparse_to_dense_.reserve(capacity * 2);  // 여유분 확보
        
        // 모든 컴포넌트 배열 사전 할당
        std::apply([capacity](auto&... arrays) {
            (arrays.reserve(capacity), ...);
        }, component_arrays_);
    }
    
    // [SEQUENCE: 33] 캐시 친화적 컴포넌트 추가
    template<typename Component>
    void AddComponent(uint32_t entity_id, const Component& component) {
        // Sparse 배열 확장
        if (entity_id >= sparse_to_dense_.size()) {
            sparse_to_dense_.resize(entity_id + 1, UINT32_MAX);
        }
        
        // 이미 존재하는지 확인
        if (sparse_to_dense_[entity_id] != UINT32_MAX) {
            return;
        }
        
        // Dense 배열에 추가
        uint32_t dense_index = entity_count_++;
        dense_entities_.push_back(entity_id);
        sparse_to_dense_[entity_id] = dense_index;
        
        // 해당 컴포넌트 배열에 추가
        auto& array = std::get<std::vector<Component>>(component_arrays_);
        array.push_back(component);
    }
    
    // [SEQUENCE: 34] 배치 처리 최적화 시스템 업데이트
    template<typename System>
    void RunSystem(System&& system) {
        constexpr size_t BATCH_SIZE = 64;  // 캐시라인 경계 기준
        
        for (size_t start = 0; start < entity_count_; start += BATCH_SIZE) {
            size_t end = std::min(start + BATCH_SIZE, entity_count_);
            
            // 배치 단위로 연속된 메모리 접근
            system.ProcessBatch(start, end, component_arrays_);
        }
    }
    
    // [SEQUENCE: 35] 프리패치를 활용한 예측적 로딩
    template<typename Component>
    Component* GetComponentWithPrefetch(uint32_t entity_id) {
        if (entity_id >= sparse_to_dense_.size()) {
            return nullptr;
        }
        
        uint32_t dense_index = sparse_to_dense_[entity_id];
        if (dense_index == UINT32_MAX) {
            return nullptr;
        }
        
        auto& array = std::get<std::vector<Component>>(component_arrays_);
        
        // 다음 캐시라인 프리패치 (하드웨어 힌트)
        if (dense_index + 16 < array.size()) {
            __builtin_prefetch(&array[dense_index + 16], 0, 1);
        }
        
        return &array[dense_index];
    }
};
```

## 🎮 게임 특화 캐시 최적화 패턴

### 1. Spatial Locality 활용

```cpp
// [SEQUENCE: 36] 공간 지역성 기반 엔티티 정렬
class SpatiallyOptimizedWorld {
private:
    struct EntityCluster {
        alignas(64) struct {
            float center_x, center_y, center_z;
            float radius;
            uint32_t entity_count;
            uint32_t start_index;
            char padding[40];  // 64바이트 정렬
        } header;
        
        // 클러스터 내 엔티티들은 연속된 메모리에 배치
        std::vector<uint32_t> entities;
    };
    
    std::vector<EntityCluster> clusters_;
    constexpr static float CLUSTER_SIZE = 100.0f;
    
public:
    // [SEQUENCE: 37] 위치 기반 엔티티 클러스터링
    void OptimizeEntityLayout() {
        // 1. 모든 엔티티를 위치별로 정렬
        auto& transforms = GetComponentArray<TransformComponent>();
        std::vector<std::pair<uint32_t, Vector3>> entity_positions;
        
        for (size_t i = 0; i < transforms.size(); ++i) {
            entity_positions.emplace_back(
                dense_entities_[i], 
                Vector3{transforms[i].position_x, transforms[i].position_y, transforms[i].position_z}
            );
        }
        
        // 2. Z-order curve (Morton code) 기반 정렬
        std::sort(entity_positions.begin(), entity_positions.end(),
                 [](const auto& a, const auto& b) {
                     return CalculateMortonCode(a.second) < CalculateMortonCode(b.second);
                 });
        
        // 3. 클러스터 단위로 재배치
        ReorganizeByCluster(entity_positions);
    }
    
private:
    // [SEQUENCE: 38] Morton code를 통한 3D 공간 인덱싱
    uint64_t CalculateMortonCode(const Vector3& position) {
        // 3D 좌표를 1D Morton code로 변환 (Z-order curve)
        uint32_t x = static_cast<uint32_t>(position.x / CLUSTER_SIZE);
        uint32_t y = static_cast<uint32_t>(position.y / CLUSTER_SIZE);  
        uint32_t z = static_cast<uint32_t>(position.z / CLUSTER_SIZE);
        
        return EncodeMorton3D(x, y, z);
    }
    
    uint64_t EncodeMorton3D(uint32_t x, uint32_t y, uint32_t z) {
        uint64_t result = 0;
        for (int i = 0; i < 21; ++i) {  // 21비트씩 처리
            result |= ((uint64_t)(x & (1u << i)) << (2 * i));
            result |= ((uint64_t)(y & (1u << i)) << (2 * i + 1));
            result |= ((uint64_t)(z & (1u << i)) << (2 * i + 2));
        }
        return result;
    }
};
```

### 2. 핫/콜드 데이터 분리

```cpp
// [SEQUENCE: 39] 접근 빈도 기반 데이터 분할
struct HotColdSeparatedComponent {
    // HOT DATA (매 프레임 접근) - 첫 번째 캐시라인
    struct alignas(64) Hot {
        Vector3 position;           // 12 bytes - 매 프레임 업데이트
        Vector3 velocity;           // 12 bytes - 매 프레임 계산
        float health;               // 4 bytes - 전투 시 빈번한 접근
        uint32_t entity_id;         // 4 bytes - 식별용
        uint32_t system_flags;      // 4 bytes - 시스템 처리 플래그
        uint32_t frame_counter;     // 4 bytes - 프레임 동기화
        char padding[20];           // 패딩으로 64바이트 맞춤
    } hot;
    
    // WARM DATA (초당 몇 번 접근) - 두 번째 캐시라인
    struct alignas(64) Warm {
        Vector3 rotation;           // 12 bytes - 회전 업데이트
        Vector3 scale;              // 12 bytes - 크기 변경
        float max_health;           // 4 bytes - 체력 계산시 참조
        float attack_power;         // 4 bytes - 전투 계산
        float defense;              // 4 bytes - 피해 계산
        uint32_t level;             // 4 bytes - 레벨 관련 계산
        uint32_t experience;        // 4 bytes - 경험치 업데이트
        char padding[16];           // 패딩
    } warm;
    
    // COLD DATA (가끔 접근) - 별도 메모리 영역
    struct Cold {
        std::string name;           // 캐릭터 이름 (UI 표시시만)
        uint64_t creation_time;     // 생성 시간 (통계용)
        uint64_t last_login;        // 마지막 로그인 (세션 관리)
        std::vector<uint32_t> inventory;  // 인벤토리 (열 때만 접근)
        std::unordered_map<std::string, float> stats;  // 상세 스탯 (계산시만)
    };
    
    // Cold data는 별도 저장소에서 관리
    static std::unordered_map<uint32_t, std::unique_ptr<Cold>> cold_storage_;
};

// [SEQUENCE: 40] Hot/Warm/Cold 전용 시스템
class TemperatureAwareSystem {
public:
    // 매 프레임 실행 - Hot data만 접근
    void UpdateMovement(float delta_time) {
        auto& components = GetHotComponents();
        
        // 캐시 친화적 순차 접근
        for (size_t i = 0; i < components.size(); ++i) {
            auto& hot = components[i].hot;
            
            hot.position.x += hot.velocity.x * delta_time;
            hot.position.y += hot.velocity.y * delta_time;
            hot.position.z += hot.velocity.z * delta_time;
            
            hot.frame_counter++;
        }
    }
    
    // 전투 시에만 실행 - Hot + Warm data 접근
    void UpdateCombat() {
        auto& components = GetHotComponents();
        auto& warm_components = GetWarmComponents();
        
        for (size_t i = 0; i < components.size(); ++i) {
            auto& hot = components[i].hot;
            auto& warm = warm_components[i].warm;
            
            if (hot.system_flags & COMBAT_FLAG) {
                // 전투 로직 - warm data 활용
                float damage = CalculateDamage(warm.attack_power, warm.level);
                hot.health = std::max(0.0f, hot.health - damage);
            }
        }
    }
    
    // 가끔 실행 - Cold data 접근
    void UpdatePlayerStats(uint32_t entity_id) {
        auto* cold_data = HotColdSeparatedComponent::cold_storage_[entity_id].get();
        if (cold_data) {
            // Cold data 업데이트
            cold_data->last_login = GetCurrentTime();
            RecalculateStats(cold_data->stats);
        }
    }
};
```

## 📊 캐시 성능 측정 및 최적화

### 캐시 미스 측정 도구

```cpp
// [SEQUENCE: 41] 하드웨어 성능 카운터 활용
class CachePerformanceProfiler {
private:
    struct CacheStats {
        uint64_t l1_cache_misses = 0;
        uint64_t l2_cache_misses = 0;
        uint64_t l3_cache_misses = 0;
        uint64_t memory_accesses = 0;
        uint64_t cycles = 0;
        double miss_rate = 0.0;
    };
    
    CacheStats start_stats_, end_stats_;
    bool profiling_active_ = false;
    
public:
    // [SEQUENCE: 42] Linux perf를 활용한 캐시 측정
    void StartProfiling() {
        if (profiling_active_) return;
        
        #ifdef __linux__
        // perf_event를 통한 하드웨어 카운터 접근
        start_stats_ = ReadHardwareCounters();
        #else
        // Windows/macOS에서는 대안 측정 방법
        start_stats_ = EstimateCachePerformance();
        #endif
        
        profiling_active_ = true;
    }
    
    CacheStats StopProfiling() {
        if (!profiling_active_) return {};
        
        #ifdef __linux__
        end_stats_ = ReadHardwareCounters();
        #else
        end_stats_ = EstimateCachePerformance();
        #endif
        
        profiling_active_ = false;
        
        return CalculateDelta();
    }
    
private:
    #ifdef __linux__
    CacheStats ReadHardwareCounters() {
        CacheStats stats;
        
        // perf_event_open을 통한 하드웨어 카운터 읽기
        // 실제 구현에서는 libpfm4 라이브러리 사용 권장
        int fd_l1_miss = perf_event_open_wrapper(PERF_COUNT_HW_CACHE_L1D, 
                                                PERF_COUNT_HW_CACHE_OP_READ,
                                                PERF_COUNT_HW_CACHE_RESULT_MISS);
        if (fd_l1_miss >= 0) {
            read(fd_l1_miss, &stats.l1_cache_misses, sizeof(uint64_t));
            close(fd_l1_miss);
        }
        
        // L2, L3 캐시 미스도 동일한 방식으로 측정
        
        return stats;
    }
    #endif
    
    // [SEQUENCE: 43] 소프트웨어 기반 캐시 성능 추정
    CacheStats EstimateCachePerformance() {
        CacheStats stats;
        
        // 시간 기반 추정
        auto start = std::chrono::high_resolution_clock::now();
        
        // 캐시 플러시 (측정을 위한 초기화)
        FlushCache();
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
        
        // 경험적 공식으로 캐시 미스 추정
        stats.cycles = duration.count() / GetCpuCycleTime();
        stats.miss_rate = EstimateMissRateFromTiming(duration);
        
        return stats;
    }
    
    void FlushCache() {
        // 캐시 플러시를 위한 대용량 메모리 접근
        constexpr size_t CACHE_SIZE = 32 * 1024 * 1024;  // 32MB
        static char dummy_data[CACHE_SIZE];
        
        volatile char sum = 0;
        for (size_t i = 0; i < CACHE_SIZE; i += 64) {  // 캐시라인 단위 접근
            sum += dummy_data[i];
        }
    }
};

// [SEQUENCE: 44] 캐시 성능 벤치마크
class CacheBenchmark {
public:
    static void CompareDataLayouts() {
        constexpr size_t NUM_ENTITIES = 100000;
        constexpr size_t NUM_ITERATIONS = 1000;
        
        std::cout << "=== Cache Performance Comparison ===" << std::endl;
        
        // AoS (Array of Structures) 테스트
        BenchmarkAoS(NUM_ENTITIES, NUM_ITERATIONS);
        
        // SoA (Structure of Arrays) 테스트  
        BenchmarkSoA(NUM_ENTITIES, NUM_ITERATIONS);
        
        // Hot/Cold 분리 테스트
        BenchmarkHotColdSeparation(NUM_ENTITIES, NUM_ITERATIONS);
    }
    
private:
    static void BenchmarkAoS(size_t num_entities, size_t iterations) {
        struct AoS_Component {
            Vector3 position;
            Vector3 velocity;
            float health;
            float max_health;
            uint32_t level;
            char padding[28];  // 64바이트로 패딩
        };
        
        std::vector<AoS_Component> components(num_entities);
        CachePerformanceProfiler profiler;
        
        profiler.StartProfiling();
        
        for (size_t iter = 0; iter < iterations; ++iter) {
            // 전체 순회 (캐시 미스 많이 발생)
            for (size_t i = 0; i < num_entities; ++i) {
                components[i].position.x += components[i].velocity.x * 0.016f;
                components[i].position.y += components[i].velocity.y * 0.016f;
                components[i].position.z += components[i].velocity.z * 0.016f;
            }
        }
        
        auto stats = profiler.StopProfiling();
        std::cout << "AoS Layout:" << std::endl;
        std::cout << "  Cache Miss Rate: " << stats.miss_rate << "%" << std::endl;
        std::cout << "  Total Cycles: " << stats.cycles << std::endl;
    }
    
    static void BenchmarkSoA(size_t num_entities, size_t iterations) {
        // SoA 구조로 분리
        std::vector<float> position_x(num_entities);
        std::vector<float> position_y(num_entities);
        std::vector<float> position_z(num_entities);
        std::vector<float> velocity_x(num_entities);
        std::vector<float> velocity_y(num_entities);
        std::vector<float> velocity_z(num_entities);
        
        CachePerformanceProfiler profiler;
        profiler.StartProfiling();
        
        for (size_t iter = 0; iter < iterations; ++iter) {
            // 캐시 친화적 순차 접근
            for (size_t i = 0; i < num_entities; ++i) {
                position_x[i] += velocity_x[i] * 0.016f;
            }
            for (size_t i = 0; i < num_entities; ++i) {
                position_y[i] += velocity_y[i] * 0.016f;
            }
            for (size_t i = 0; i < num_entities; ++i) {
                position_z[i] += velocity_z[i] * 0.016f;
            }
        }
        
        auto stats = profiler.StopProfiling();
        std::cout << "SoA Layout:" << std::endl;
        std::cout << "  Cache Miss Rate: " << stats.miss_rate << "%" << std::endl;
        std::cout << "  Total Cycles: " << stats.cycles << std::endl;
        std::cout << "  Performance Improvement: " << 
                     CalculateImprovement(stats) << "x" << std::endl;
    }
};
```

### 예상 벤치마크 결과

```
=== Cache Performance Comparison ===
AoS Layout:
  Cache Miss Rate: 34.2%
  Total Cycles: 2,840,000,000
  
SoA Layout:
  Cache Miss Rate: 8.7% (75% reduction)
  Total Cycles: 620,000,000 (78% reduction)
  Performance Improvement: 4.6x
  
Hot/Cold Separation:
  Cache Miss Rate: 5.2% (85% reduction)
  Total Cycles: 480,000,000 (83% reduction)
  Performance Improvement: 5.9x
```

## 🎯 실제 프로젝트 적용 로드맵

### 1단계: 현재 ECS 캐시 분석

```cpp
// [SEQUENCE: 45] 기존 코드 캐시 효율성 측정
void AnalyzeCurrentCacheEfficiency() {
    CachePerformanceProfiler profiler;
    
    // 현재 ECS 시스템 테스트
    profiler.StartProfiling();
    
    // 기존 컴포넌트 업데이트 로직 실행
    for (int i = 0; i < 10000; ++i) {
        RunExistingECSUpdate();
    }
    
    auto current_stats = profiler.StopProfiling();
    
    std::cout << "=== Current ECS Cache Performance ===" << std::endl;
    std::cout << "Cache Miss Rate: " << current_stats.miss_rate << "%" << std::endl;
    std::cout << "Baseline for improvement measurement" << std::endl;
}
```

### 2단계: 점진적 최적화 적용

1. **컴포넌트 레이아웃 변경**: AoS → SoA
2. **Hot/Cold 데이터 분리**: 접근 빈도별 분할
3. **배치 처리**: 캐시라인 경계 기준 처리
4. **메모리 정렬**: alignas(64) 활용

### 3단계: 성능 검증

- 캐시 미스율 70% 이상 감소 목표
- 전체 처리량 400% 이상 향상 목표
- 메모리 사용량 최적화 (불필요한 패딩 최소화)

## 🚀 다음 단계

다음 문서 **NUMA 인식 최적화(numa_awareness.md)**에서는:

1. **NUMA 토폴로지 인식** - 메모리 노드별 최적화
2. **CPU 친화성 설정** - 스레드-코어 바인딩
3. **크로스 노드 트래픽 최소화** - 메모리 지역성 극대화
4. **실제 서버 환경 최적화** - 대형 서버에서의 성능 극대화

### 실습 과제

1. 현재 ECS를 SoA 구조로 변환
2. Hot/Cold 데이터 분리 적용
3. 캐시 성능 측정 및 개선 확인
4. 전체 처리량 400% 향상 달성

---

**"캐시 친화적 설계는 현대 게임서버의 필수 요소입니다"**

이제 NUMA 아키텍처 최적화로 대규모 서버 환경에서의 성능을 극대화해보겠습니다!