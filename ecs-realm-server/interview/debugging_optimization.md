# 디버깅과 최적화

## 1. 디버깅 기법

### 로깅 전략

**구조화된 로깅 시스템:**
```cpp
enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARNING = 3,
    ERROR = 4,
    CRITICAL = 5
};

class Logger {
    struct LogContext {
        string playerId;
        string sessionId;
        string serverId;
        string requestId;
    };
    
    template<typename... Args>
    void Log(LogLevel level, const string& format, Args... args) {
        if (level < currentLevel) return;
        
        json logEntry = {
            {"timestamp", GetISOTimestamp()},
            {"level", ToString(level)},
            {"message", fmt::format(format, args...)},
            {"context", {
                {"player_id", context.playerId},
                {"session_id", context.sessionId},
                {"server_id", context.serverId},
                {"request_id", context.requestId}
            }},
            {"thread_id", this_thread::get_id()},
            {"file", __FILE__},
            {"line", __LINE__}
        };
        
        logQueue.Push(logEntry);
    }
};

// 사용 예시
LOG_INFO("Player {} entered zone {} at position ({}, {}, {})", 
         playerId, zoneId, pos.x, pos.y, pos.z);

// 성능 영향 최소화
#ifdef DEBUG_BUILD
    #define LOG_TRACE(...) logger.Log(LogLevel::TRACE, __VA_ARGS__)
#else
    #define LOG_TRACE(...) ((void)0)
#endif
```

### 메모리 디버깅

**커스텀 메모리 할당자:**
```cpp
class DebugAllocator {
    static constexpr uint32_t GUARD_VALUE = 0xDEADBEEF;
    
    struct AllocationHeader {
        size_t size;
        uint32_t guardFront;
        const char* file;
        int line;
        chrono::time_point<chrono::steady_clock> timestamp;
    };
    
    void* Allocate(size_t size, const char* file, int line) {
        size_t totalSize = sizeof(AllocationHeader) + size + sizeof(uint32_t);
        void* raw = malloc(totalSize);
        
        AllocationHeader* header = (AllocationHeader*)raw;
        header->size = size;
        header->guardFront = GUARD_VALUE;
        header->file = file;
        header->line = line;
        header->timestamp = chrono::steady_clock::now();
        
        // 뒤쪽 가드
        uint32_t* guardBack = (uint32_t*)((char*)raw + sizeof(AllocationHeader) + size);
        *guardBack = GUARD_VALUE;
        
        // 메모리 패턴으로 초기화 (초기화되지 않은 읽기 감지)
        memset((char*)raw + sizeof(AllocationHeader), 0xCD, size);
        
        trackAllocation(header);
        
        return (char*)raw + sizeof(AllocationHeader);
    }
    
    void Deallocate(void* ptr) {
        if (!ptr) return;
        
        AllocationHeader* header = (AllocationHeader*)((char*)ptr - sizeof(AllocationHeader));
        
        // 메모리 손상 체크
        if (header->guardFront != GUARD_VALUE) {
            LOG_ERROR("Memory corruption detected (front guard) at {}", ptr);
        }
        
        uint32_t* guardBack = (uint32_t*)((char*)ptr + header->size);
        if (*guardBack != GUARD_VALUE) {
            LOG_ERROR("Memory corruption detected (back guard) at {}", ptr);
        }
        
        // 해제된 메모리 패턴으로 채우기 (use-after-free 감지)
        memset(ptr, 0xDD, header->size);
        
        untrackAllocation(header);
        free(header);
    }
    
    void ReportLeaks() {
        for (const auto& [ptr, info] : allocations) {
            auto duration = chrono::steady_clock::now() - info.timestamp;
            LOG_WARNING("Memory leak: {} bytes allocated at {}:{}, age: {}s",
                       info.size, info.file, info.line,
                       chrono::duration_cast<chrono::seconds>(duration).count());
        }
    }
};

#ifdef DEBUG_BUILD
    #define NEW(type) new (DebugAllocator::Allocate(sizeof(type), __FILE__, __LINE__)) type
    #define DELETE(ptr) DebugAllocator::Deallocate(ptr); ptr = nullptr
#endif
```

### 크래시 덤프 분석

**미니덤프 생성:**
```cpp
#ifdef _WIN32
class CrashHandler {
    static LONG WINAPI UnhandledExceptionFilter(EXCEPTION_POINTERS* exceptionInfo) {
        // 미니덤프 생성
        HANDLE hFile = CreateFile(
            L"crashdump.dmp",
            GENERIC_WRITE,
            FILE_SHARE_WRITE,
            NULL,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        
        MINIDUMP_EXCEPTION_INFORMATION mdei;
        mdei.ThreadId = GetCurrentThreadId();
        mdei.ExceptionPointers = exceptionInfo;
        mdei.ClientPointers = FALSE;
        
        MiniDumpWriteDump(
            GetCurrentProcess(),
            GetCurrentProcessId(),
            hFile,
            MiniDumpWithFullMemory,
            &mdei,
            NULL,
            NULL
        );
        
        CloseHandle(hFile);
        
        // 스택 트레이스 로깅
        LogStackTrace(exceptionInfo);
        
        return EXCEPTION_EXECUTE_HANDLER;
    }
};
#endif

// Linux 코어 덤프
class CoreDumpHandler {
    static void SignalHandler(int sig, siginfo_t* info, void* context) {
        // 백트레이스 획득
        void* buffer[100];
        int nptrs = backtrace(buffer, 100);
        
        // 심볼 정보 획득
        char** strings = backtrace_symbols(buffer, nptrs);
        
        // 로그 파일에 기록
        ofstream crashLog("crash.log", ios::app);
        crashLog << "Signal " << sig << " received" << endl;
        crashLog << "Backtrace:" << endl;
        
        for (int i = 0; i < nptrs; i++) {
            crashLog << strings[i] << endl;
        }
        
        free(strings);
        
        // 기본 핸들러 호출 (코어 덤프 생성)
        signal(sig, SIG_DFL);
        raise(sig);
    }
    
    void Install() {
        struct sigaction sa;
        sa.sa_sigaction = SignalHandler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_SIGINFO;
        
        sigaction(SIGSEGV, &sa, NULL);
        sigaction(SIGABRT, &sa, NULL);
    }
};
```

## 2. 성능 프로파일링

### CPU 프로파일링

**샘플링 프로파일러:**
```cpp
class SamplingProfiler {
    struct Sample {
        void* instructionPointer;
        chrono::time_point<chrono::high_resolution_clock> timestamp;
        thread::id threadId;
    };
    
    vector<Sample> samples;
    atomic<bool> profiling{false};
    
    void StartProfiling() {
        profiling = true;
        
        thread samplerThread([this]() {
            while (profiling) {
                // 모든 스레드 일시 정지
                SuspendAllThreads();
                
                // 각 스레드의 스택 샘플링
                for (auto& threadInfo : GetAllThreads()) {
                    Sample sample;
                    sample.instructionPointer = GetInstructionPointer(threadInfo);
                    sample.timestamp = chrono::high_resolution_clock::now();
                    sample.threadId = threadInfo.id;
                    
                    samples.push_back(sample);
                }
                
                ResumeAllThreads();
                
                // 샘플링 주기
                this_thread::sleep_for(1ms);
            }
        });
    }
    
    void GenerateReport() {
        // 함수별 히트 카운트
        map<string, int> functionHits;
        
        for (const auto& sample : samples) {
            string functionName = ResolveSymbol(sample.instructionPointer);
            functionHits[functionName]++;
        }
        
        // 핫스팟 출력
        vector<pair<string, int>> hotspots(functionHits.begin(), functionHits.end());
        sort(hotspots.begin(), hotspots.end(), 
             [](const auto& a, const auto& b) { return a.second > b.second; });
        
        cout << "Top 10 CPU Hotspots:" << endl;
        for (int i = 0; i < min(10, (int)hotspots.size()); i++) {
            float percentage = (float)hotspots[i].second / samples.size() * 100;
            cout << fmt::format("{:>5.1f}% {}", percentage, hotspots[i].first) << endl;
        }
    }
};
```

### 메모리 프로파일링

**힙 프로파일러:**
```cpp
class HeapProfiler {
    struct HeapSnapshot {
        size_t totalAllocated;
        size_t totalFreed;
        size_t currentUsage;
        map<string, size_t> allocationsByType;
        map<string, size_t> allocationsByCallstack;
    };
    
    HeapSnapshot TakeSnapshot() {
        HeapSnapshot snapshot;
        
        // 전역 할당 추적
        for (const auto& [ptr, info] : allocations) {
            snapshot.currentUsage += info.size;
            
            // 타입별 집계
            string typeName = DemangleTypeName(info.typeInfo);
            snapshot.allocationsByType[typeName] += info.size;
            
            // 콜스택별 집계
            string callstack = FormatCallstack(info.callstack);
            snapshot.allocationsByCallstack[callstack] += info.size;
        }
        
        return snapshot;
    }
    
    void CompareSnapshots(const HeapSnapshot& before, const HeapSnapshot& after) {
        cout << "Memory Growth Analysis:" << endl;
        cout << "Total growth: " << FormatBytes(after.currentUsage - before.currentUsage) << endl;
        
        // 증가한 타입 찾기
        cout << "\nTop growing types:" << endl;
        map<string, int64_t> growth;
        
        for (const auto& [type, size] : after.allocationsByType) {
            int64_t beforeSize = before.allocationsByType.count(type) ? 
                                before.allocationsByType.at(type) : 0;
            growth[type] = size - beforeSize;
        }
        
        // 상위 10개 출력
        vector<pair<string, int64_t>> sorted(growth.begin(), growth.end());
        sort(sorted.begin(), sorted.end(), 
             [](const auto& a, const auto& b) { return a.second > b.second; });
        
        for (int i = 0; i < min(10, (int)sorted.size()); i++) {
            if (sorted[i].second > 0) {
                cout << fmt::format("  {} : +{}", sorted[i].first, 
                                   FormatBytes(sorted[i].second)) << endl;
            }
        }
    }
};
```

## 3. 최적화 기법

### 캐시 최적화

**데이터 지역성 개선:**
```cpp
// Before: Cache-unfriendly
struct GameObject {
    Vector3 position;      // 12 bytes
    Quaternion rotation;   // 16 bytes
    Vector3 scale;        // 12 bytes
    
    string name;          // 32 bytes
    Texture* texture;     // 8 bytes
    Mesh* mesh;          // 8 bytes
    
    // ... 더 많은 데이터
};  // 총 200+ bytes, 캐시 라인 여러 개

// After: Data-Oriented Design
struct TransformComponent {
    Vector3 position;
    Quaternion rotation;
    Vector3 scale;
};  // 40 bytes, 1 캐시 라인

struct RenderComponent {
    Texture* texture;
    Mesh* mesh;
};

// SOA (Structure of Arrays)
class TransformSystem {
    vector<Vector3> positions;
    vector<Quaternion> rotations;
    vector<Vector3> scales;
    
    void UpdatePositions(float deltaTime) {
        // 벡터화 가능, 캐시 효율적
        #pragma omp simd
        for (size_t i = 0; i < positions.size(); i++) {
            positions[i] += velocities[i] * deltaTime;
        }
    }
};
```

### 알고리즘 최적화

**시간 복잡도 개선:**
```cpp
// O(n²) → O(n log n) 최적화 예시
class CollisionDetection {
    // Before: Brute force
    void CheckCollisionsBruteForce(vector<GameObject>& objects) {
        for (size_t i = 0; i < objects.size(); i++) {
            for (size_t j = i + 1; j < objects.size(); j++) {
                if (IsColliding(objects[i], objects[j])) {
                    HandleCollision(objects[i], objects[j]);
                }
            }
        }
    }
    
    // After: Spatial partitioning
    void CheckCollisionsOptimized(vector<GameObject>& objects) {
        // 공간 분할 구조 구축
        SpatialHashGrid grid(worldBounds, cellSize);
        
        for (auto& obj : objects) {
            grid.Insert(obj);
        }
        
        // 같은 셀 내의 객체만 검사
        for (auto& cell : grid.GetOccupiedCells()) {
            auto& objectsInCell = cell.objects;
            
            for (size_t i = 0; i < objectsInCell.size(); i++) {
                for (size_t j = i + 1; j < objectsInCell.size(); j++) {
                    if (IsColliding(objectsInCell[i], objectsInCell[j])) {
                        HandleCollision(objectsInCell[i], objectsInCell[j]);
                    }
                }
            }
        }
    }
};
```

### 병렬화 최적화

**SIMD 활용:**
```cpp
class VectorMath {
    // 스칼라 버전
    void AddVectorsScalar(const float* a, const float* b, float* result, size_t count) {
        for (size_t i = 0; i < count; i++) {
            result[i] = a[i] + b[i];
        }
    }
    
    // SIMD 버전 (AVX2)
    void AddVectorsSIMD(const float* a, const float* b, float* result, size_t count) {
        size_t simdCount = count / 8;  // AVX2는 8개 float 동시 처리
        
        for (size_t i = 0; i < simdCount; i++) {
            __m256 va = _mm256_load_ps(&a[i * 8]);
            __m256 vb = _mm256_load_ps(&b[i * 8]);
            __m256 vr = _mm256_add_ps(va, vb);
            _mm256_store_ps(&result[i * 8], vr);
        }
        
        // 나머지 처리
        for (size_t i = simdCount * 8; i < count; i++) {
            result[i] = a[i] + b[i];
        }
    }
};
```

## 4. 실시간 모니터링

### 성능 메트릭 수집

**실시간 통계:**
```cpp
class PerformanceMonitor {
    template<typename T>
    class RollingAverage {
        circular_buffer<T> samples;
        T sum = 0;
        
    public:
        RollingAverage(size_t size) : samples(size) {}
        
        void Add(T value) {
            if (samples.full()) {
                sum -= samples.front();
            }
            samples.push_back(value);
            sum += value;
        }
        
        T GetAverage() const {
            return samples.empty() ? 0 : sum / samples.size();
        }
        
        T GetPercentile(float percentile) const {
            vector<T> sorted(samples.begin(), samples.end());
            sort(sorted.begin(), sorted.end());
            
            size_t index = (size_t)(sorted.size() * percentile);
            return sorted[index];
        }
    };
    
    RollingAverage<float> frameTime{1000};     // 최근 1000 프레임
    RollingAverage<float> tickTime{1000};      // 게임 로직 시간
    RollingAverage<float> renderTime{1000};    // 렌더링 시간
    RollingAverage<float> networkTime{1000};   // 네트워크 처리 시간
    
    void UpdateMetrics() {
        json metrics = {
            {"fps", 1000.0f / frameTime.GetAverage()},
            {"frame_time_avg", frameTime.GetAverage()},
            {"frame_time_p95", frameTime.GetPercentile(0.95f)},
            {"frame_time_p99", frameTime.GetPercentile(0.99f)},
            {"tick_time", tickTime.GetAverage()},
            {"render_time", renderTime.GetAverage()},
            {"network_time", networkTime.GetAverage()},
            {"memory_usage", GetMemoryUsage()},
            {"thread_count", GetThreadCount()},
            {"cpu_usage", GetCPUUsage()}
        };
        
        // Prometheus로 내보내기
        for (const auto& [key, value] : metrics.items()) {
            prometheus::Gauge gauge{key, "Game server metric"};
            gauge.Set(value);
        }
    }
};
```

## 면접 대비 체크포인트

1. **디버깅 도구 경험**: GDB, Valgrind, PerfView 등
2. **프로파일링 방법론**: 병목 지점 찾기, 해석 능력
3. **최적화 우선순위**: 측정 → 분석 → 최적화
4. **트레이드오프 이해**: 가독성 vs 성능
5. **실제 최적화 사례**: 구체적인 개선 수치 제시