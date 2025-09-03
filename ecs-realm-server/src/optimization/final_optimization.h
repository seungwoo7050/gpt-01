#pragma once

#include <memory>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <thread>
#include "../core/types.h"
#include "../core/singleton.h"
#include "../network/network_manager.h"
#include "../world/world_manager.h"
#include "../database/database_manager.h"

namespace mmorpg::optimization {

// [SEQUENCE: 3775] Memory optimization settings 메모리 최적화 설정
struct MemoryOptimizationSettings {
    size_t object_pool_size{10000};          // Object pool capacity
    size_t string_pool_size{50000};          // String intern pool
    size_t buffer_pool_size{100};            // Network buffer pool
    size_t max_cache_size{1024 * 1024 * 512}; // 512MB cache
    bool enable_memory_compaction{true};      // Memory defragmentation
    bool enable_lazy_loading{true};           // Lazy resource loading
    uint32_t gc_interval_ms{5000};           // Garbage collection interval
};

// [SEQUENCE: 3776] CPU optimization settings CPU 최적화 설정
struct CPUOptimizationSettings {
    uint32_t worker_thread_count{0};          // 0 = auto (CPU cores)
    uint32_t io_thread_count{2};              // I/O thread pool size
    bool enable_simd{true};                   // SIMD optimizations
    bool enable_vectorization{true};          // Auto-vectorization
    bool enable_parallel_systems{true};       // Parallel ECS systems
    uint32_t batch_size{1000};                // Processing batch size
    float load_balancing_threshold{0.8f};     // CPU load threshold
};

// [SEQUENCE: 3777] Network optimization settings 네트워크 최적화 설정
struct NetworkOptimizationSettings {
    bool enable_compression{true};            // Packet compression
    bool enable_batching{true};               // Packet batching
    uint32_t batch_window_ms{16};             // Batching window
    bool enable_delta_compression{true};      // Delta state updates
    bool enable_predictive_sending{true};     // Predictive packet sending
    uint32_t send_buffer_size{65536};         // 64KB send buffer
    uint32_t recv_buffer_size{65536};         // 64KB receive buffer
};

// [SEQUENCE: 3778] Final optimization manager 최종 최적화 관리자
class FinalOptimization : public Singleton<FinalOptimization> {
public:
    // [SEQUENCE: 3779] Optimization initialization 최적화 초기화
    void Initialize();
    void Shutdown();
    
    // Memory optimizations
    void OptimizeMemory();
    void EnableObjectPooling();
    void EnableStringInterning();
    void CompactMemory();
    void FlushUnusedCaches();
    
    // CPU optimizations
    void OptimizeCPU();
    void DistributeWorkload();
    void EnableParallelProcessing();
    void OptimizeHotPaths();
    
    // Network optimizations
    void OptimizeNetwork();
    void EnableSmartBatching();
    void OptimizePacketFlow();
    
    // Database optimizations
    void OptimizeDatabase();
    void EnableQueryCaching();
    void OptimizeConnectionPool();
    
    // Rendering optimizations (server-side culling)
    void OptimizeVisibility();
    void EnableFrustumCulling();
    void OptimizeLOD();
    
    // [SEQUENCE: 3780] Performance profiling 성능 프로파일링
    struct PerformanceProfile {
        float cpu_usage_percent;
        size_t memory_usage_bytes;
        uint32_t network_bandwidth_kbps;
        float average_frame_time_ms;
        uint32_t active_connections;
        uint32_t entities_processed;
        float db_query_time_ms;
    };
    
    PerformanceProfile GetCurrentProfile() const;
    void StartProfiling();
    void StopProfiling();
    
    // Settings
    void UpdateMemorySettings(const MemoryOptimizationSettings& settings);
    void UpdateCPUSettings(const CPUOptimizationSettings& settings);
    void UpdateNetworkSettings(const NetworkOptimizationSettings& settings);
    
private:
    friend class Singleton<FinalOptimization>;
    FinalOptimization() = default;
    
    // Settings
    MemoryOptimizationSettings memory_settings_;
    CPUOptimizationSettings cpu_settings_;
    NetworkOptimizationSettings network_settings_;
    
    // Profiling
    std::atomic<bool> profiling_enabled_{false};
    PerformanceProfile current_profile_;
    std::chrono::steady_clock::time_point profile_start_time_;
    
    // Thread pools
    std::vector<std::thread> worker_threads_;
    std::vector<std::thread> io_threads_;
    
    // Optimization state
    bool initialized_{false};
    std::atomic<bool> running_{false};
};

// [SEQUENCE: 3781] Memory pool manager 메모리 풀 관리자
template<typename T>
class MemoryPool {
public:
    explicit MemoryPool(size_t initial_size = 1000);
    ~MemoryPool();
    
    // Allocation
    T* Allocate();
    void Deallocate(T* ptr);
    
    // Pool management
    void Reserve(size_t count);
    void Shrink();
    void Clear();
    
    // Statistics
    size_t GetAllocatedCount() const { return allocated_count_; }
    size_t GetPoolSize() const { return pool_.size(); }
    float GetUsageRatio() const;
    
private:
    struct Block {
        alignas(T) char data[sizeof(T)];
        bool in_use{false};
    };
    
    std::vector<std::unique_ptr<Block>> pool_;
    std::vector<Block*> free_list_;
    size_t allocated_count_{0};
    mutable std::mutex mutex_;
};

// [SEQUENCE: 3782] String interning pool 문자열 인터닝 풀
class StringPool {
public:
    StringPool(size_t initial_capacity = 10000);
    
    // Intern string
    const std::string& Intern(const std::string& str);
    const std::string& Intern(std::string&& str);
    
    // Pool management
    void Clear();
    size_t GetSize() const { return strings_.size(); }
    size_t GetMemoryUsage() const;
    
    // Statistics
    struct Stats {
        uint64_t total_lookups{0};
        uint64_t cache_hits{0};
        uint64_t strings_interned{0};
        size_t memory_saved{0};
    };
    
    Stats GetStats() const { return stats_; }
    
private:
    std::unordered_set<std::string> strings_;
    mutable Stats stats_;
    mutable std::shared_mutex mutex_;
};

// [SEQUENCE: 3783] Parallel task executor 병렬 작업 실행기
class ParallelExecutor {
public:
    ParallelExecutor(size_t thread_count = 0);  // 0 = hardware concurrency
    ~ParallelExecutor();
    
    // Task execution
    template<typename Func>
    void Execute(Func&& func, size_t count);
    
    template<typename Func>
    void ExecuteBatch(Func&& func, size_t start, size_t end, size_t batch_size);
    
    // Parallel for_each
    template<typename Container, typename Func>
    void ForEach(Container& container, Func&& func);
    
    // Wait for completion
    void Wait();
    
private:
    std::vector<std::thread> threads_;
    std::queue<std::function<void()>> tasks_;
    std::mutex queue_mutex_;
    std::condition_variable cv_;
    std::atomic<bool> stop_{false};
    std::atomic<size_t> active_tasks_{0};
    
    void WorkerThread();
};

// [SEQUENCE: 3784] Cache manager 캐시 관리자
template<typename Key, typename Value>
class CacheManager {
public:
    CacheManager(size_t max_size = 10000);
    
    // Cache operations
    void Put(const Key& key, const Value& value);
    std::optional<Value> Get(const Key& key);
    void Remove(const Key& key);
    void Clear();
    
    // Eviction policies
    enum class EvictionPolicy {
        LRU,    // Least Recently Used
        LFU,    // Least Frequently Used
        FIFO,   // First In First Out
        RANDOM  // Random eviction
    };
    
    void SetEvictionPolicy(EvictionPolicy policy);
    
    // Statistics
    struct CacheStats {
        uint64_t hits{0};
        uint64_t misses{0};
        uint64_t evictions{0};
        float hit_rate{0.0f};
    };
    
    CacheStats GetStats() const;
    
private:
    struct CacheEntry {
        Value value;
        uint64_t access_count{0};
        std::chrono::steady_clock::time_point last_access;
        std::chrono::steady_clock::time_point insert_time;
    };
    
    std::unordered_map<Key, CacheEntry> cache_;
    std::deque<Key> access_order_;  // For LRU
    size_t max_size_;
    EvictionPolicy eviction_policy_{EvictionPolicy::LRU};
    mutable CacheStats stats_;
    mutable std::shared_mutex mutex_;
    
    void Evict();
};

// [SEQUENCE: 3785] SIMD optimizations SIMD 최적화
namespace SIMD {
    // Vector math optimizations
    void AddVectors(const float* a, const float* b, float* result, size_t count);
    void MultiplyVectors(const float* a, const float* b, float* result, size_t count);
    float DotProduct(const float* a, const float* b, size_t count);
    
    // Matrix operations
    void MatrixMultiply4x4(const float* a, const float* b, float* result);
    void TransformPoints(const float* matrix, const float* points, float* result, size_t count);
    
    // Distance calculations
    void CalculateDistances(const Vector3* positions, size_t count, float* distances);
    void FindNearestNeighbors(const Vector3* positions, size_t count, uint32_t* indices, float* distances);
}

// [SEQUENCE: 3786] Hot path optimizer 핫 패스 최적화기
class HotPathOptimizer {
public:
    // Profile hot paths
    void StartProfiling();
    void StopProfiling();
    
    // Optimization hints
    void MarkHotFunction(const std::string& function_name);
    void MarkColdFunction(const std::string& function_name);
    
    // Branch prediction hints
    #define LIKELY(x) __builtin_expect(!!(x), 1)
    #define UNLIKELY(x) __builtin_expect(!!(x), 0)
    
    // Prefetch hints
    void Prefetch(const void* addr, int rw = 0, int locality = 3);
    
    // Function inlining
    #define ALWAYS_INLINE __attribute__((always_inline)) inline
    #define NEVER_INLINE __attribute__((noinline))
    
    // Cache line optimization
    static constexpr size_t CACHE_LINE_SIZE = 64;
    #define CACHE_ALIGNED alignas(CACHE_LINE_SIZE)
    
private:
    std::unordered_map<std::string, uint64_t> function_calls_;
    std::unordered_map<std::string, uint64_t> function_times_;
    std::atomic<bool> profiling_enabled_{false};
};

// [SEQUENCE: 3787] Batch processor 배치 프로세서
template<typename T>
class BatchProcessor {
public:
    BatchProcessor(size_t batch_size = 1000);
    
    // Add items to batch
    void Add(T&& item);
    void Add(const T& item);
    
    // Process batch
    template<typename Func>
    void Process(Func&& processor);
    
    // Force process remaining items
    template<typename Func>
    void Flush(Func&& processor);
    
    // Auto-process when batch is full
    template<typename Func>
    void SetAutoProcessor(Func&& processor);
    
private:
    std::vector<T> batch_;
    size_t batch_size_;
    std::function<void(std::vector<T>&)> auto_processor_;
    std::mutex mutex_;
};

// [SEQUENCE: 3788] Load balancer 로드 밸런서
class LoadBalancer {
public:
    LoadBalancer(size_t worker_count);
    
    // Task distribution
    template<typename Task>
    void SubmitTask(Task&& task);
    
    // Load monitoring
    float GetWorkerLoad(size_t worker_id) const;
    size_t GetLeastLoadedWorker() const;
    
    // Dynamic balancing
    void RebalanceLoad();
    void SetBalancingStrategy(const std::string& strategy);
    
private:
    struct Worker {
        std::queue<std::function<void()>> tasks;
        std::atomic<size_t> task_count{0};
        std::atomic<uint64_t> total_execution_time{0};
        std::thread thread;
    };
    
    std::vector<std::unique_ptr<Worker>> workers_;
    std::atomic<size_t> next_worker_{0};
    std::string balancing_strategy_{"round_robin"};
    
    void WorkerLoop(Worker* worker);
};

// [SEQUENCE: 3789] Optimization utilities 최적화 유틸리티
namespace OptimizationUtils {
    // Memory utilities
    void WarmCache(void* data, size_t size);
    void CompactMemory();
    size_t GetMemoryUsage();
    
    // CPU utilities
    void SetThreadAffinity(std::thread& thread, size_t core_id);
    void SetThreadPriority(std::thread& thread, int priority);
    size_t GetOptimalThreadCount();
    
    // Profiling utilities
    class ScopedTimer {
    public:
        explicit ScopedTimer(const std::string& name);
        ~ScopedTimer();
        
    private:
        std::string name_;
        std::chrono::high_resolution_clock::time_point start_;
    };
    
    #define PROFILE_SCOPE(name) OptimizationUtils::ScopedTimer _timer(name)
}

} // namespace mmorpg::optimization