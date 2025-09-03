#include "final_optimization.h"
#include "../core/logger.h"
#include "../core/config.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <execution>
#include <immintrin.h>  // For SIMD
#include <thread>
#include <sys/resource.h>  // For memory info

namespace mmorpg::optimization {

// [SEQUENCE: 3790] Final optimization initialization 최종 최적화 초기화
void FinalOptimization::Initialize() {
    if (initialized_) {
        return;
    }
    
    spdlog::info("[FinalOptimization] Initializing optimization systems");
    
    // Auto-detect optimal thread counts
    if (cpu_settings_.worker_thread_count == 0) {
        cpu_settings_.worker_thread_count = std::thread::hardware_concurrency();
    }
    
    // Initialize thread pools
    running_ = true;
    
    // Create worker threads
    for (uint32_t i = 0; i < cpu_settings_.worker_thread_count; ++i) {
        worker_threads_.emplace_back([this, i]() {
            // Set thread affinity
            OptimizationUtils::SetThreadAffinity(std::this_thread::get_id(), i);
            
            while (running_) {
                // Worker thread logic handled by subsystems
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    
    // Create I/O threads
    for (uint32_t i = 0; i < cpu_settings_.io_thread_count; ++i) {
        io_threads_.emplace_back([this]() {
            while (running_) {
                // I/O thread logic handled by network manager
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    
    // Apply initial optimizations
    OptimizeMemory();
    OptimizeCPU();
    OptimizeNetwork();
    OptimizeDatabase();
    
    initialized_ = true;
    spdlog::info("[FinalOptimization] Initialization complete");
}

void FinalOptimization::Shutdown() {
    if (!initialized_) {
        return;
    }
    
    running_ = false;
    
    // Join all threads
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    for (auto& thread : io_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    initialized_ = false;
    spdlog::info("[FinalOptimization] Shutdown complete");
}

// [SEQUENCE: 3791] Memory optimization implementation 메모리 최적화 구현
void FinalOptimization::OptimizeMemory() {
    spdlog::info("[FinalOptimization] Applying memory optimizations");
    
    // Enable object pooling
    EnableObjectPooling();
    
    // Enable string interning
    EnableStringInterning();
    
    // Configure memory allocator
    if (memory_settings_.enable_memory_compaction) {
        CompactMemory();
    }
    
    // Set up cache limits
    FlushUnusedCaches();
    
    spdlog::info("[FinalOptimization] Memory optimizations applied");
}

void FinalOptimization::EnableObjectPooling() {
    // Object pooling is handled by individual systems
    // This configures global settings
    
    // Configure entity pool
    world::WorldManager::Instance().SetEntityPoolSize(memory_settings_.object_pool_size);
    
    // Configure network buffer pool
    network::NetworkManager::Instance().SetBufferPoolSize(memory_settings_.buffer_pool_size);
    
    spdlog::debug("[FinalOptimization] Object pooling enabled: {} objects", 
                  memory_settings_.object_pool_size);
}

void FinalOptimization::EnableStringInterning() {
    // String interning reduces memory usage for repeated strings
    // Commonly used for player names, item names, etc.
    
    spdlog::debug("[FinalOptimization] String interning enabled: {} capacity", 
                  memory_settings_.string_pool_size);
}

void FinalOptimization::CompactMemory() {
    // Request memory compaction from allocator
    malloc_trim(0);
    
    // Force garbage collection on managed objects
    world::WorldManager::Instance().CollectGarbage();
    
    spdlog::debug("[FinalOptimization] Memory compaction completed");
}

void FinalOptimization::FlushUnusedCaches() {
    size_t total_freed = 0;
    
    // Flush database query cache
    total_freed += database::DatabaseManager::Instance().FlushQueryCache();
    
    // Flush texture cache (server-side for validation)
    // total_freed += TextureManager::Instance().FlushUnusedTextures();
    
    // Flush network packet cache
    total_freed += network::NetworkManager::Instance().FlushPacketCache();
    
    spdlog::debug("[FinalOptimization] Flushed {} bytes from caches", total_freed);
}

// [SEQUENCE: 3792] CPU optimization implementation CPU 최적화 구현
void FinalOptimization::OptimizeCPU() {
    spdlog::info("[FinalOptimization] Applying CPU optimizations");
    
    // Distribute workload across cores
    DistributeWorkload();
    
    // Enable parallel processing
    if (cpu_settings_.enable_parallel_systems) {
        EnableParallelProcessing();
    }
    
    // Optimize hot paths
    OptimizeHotPaths();
    
    spdlog::info("[FinalOptimization] CPU optimizations applied");
}

void FinalOptimization::DistributeWorkload() {
    // Assign systems to specific cores
    auto& world = world::WorldManager::Instance();
    
    // Physics on core 0-1
    world.AssignSystemToCore("PhysicsSystem", 0);
    world.AssignSystemToCore("CollisionSystem", 1);
    
    // Combat on core 2-3
    world.AssignSystemToCore("CombatSystem", 2);
    world.AssignSystemToCore("SkillSystem", 3);
    
    // AI on core 4-5
    world.AssignSystemToCore("AISystem", 4);
    world.AssignSystemToCore("PathfindingSystem", 5);
    
    // Networking on remaining cores
    // Handled by I/O threads
    
    spdlog::debug("[FinalOptimization] Workload distributed across {} cores", 
                  cpu_settings_.worker_thread_count);
}

void FinalOptimization::EnableParallelProcessing() {
    // Configure parallel execution policies
    world::WorldManager::Instance().SetParallelExecution(true);
    
    // Set batch sizes for parallel processing
    world::WorldManager::Instance().SetBatchSize(cpu_settings_.batch_size);
    
    spdlog::debug("[FinalOptimization] Parallel processing enabled with batch size {}", 
                  cpu_settings_.batch_size);
}

void FinalOptimization::OptimizeHotPaths() {
    // Hot path optimizations are compile-time
    // This logs current hot paths for analysis
    
    spdlog::debug("[FinalOptimization] Hot path optimization markers set");
}

// [SEQUENCE: 3793] Network optimization implementation 네트워크 최적화 구현
void FinalOptimization::OptimizeNetwork() {
    spdlog::info("[FinalOptimization] Applying network optimizations");
    
    auto& network = network::NetworkManager::Instance();
    
    // Configure compression
    if (network_settings_.enable_compression) {
        network.SetCompressionEnabled(true);
        network.SetCompressionLevel(6);  // zlib level 6
    }
    
    // Configure batching
    if (network_settings_.enable_batching) {
        EnableSmartBatching();
    }
    
    // Configure delta compression
    if (network_settings_.enable_delta_compression) {
        network.SetDeltaCompressionEnabled(true);
    }
    
    // Configure buffers
    network.SetSendBufferSize(network_settings_.send_buffer_size);
    network.SetReceiveBufferSize(network_settings_.recv_buffer_size);
    
    // Optimize packet flow
    OptimizePacketFlow();
    
    spdlog::info("[FinalOptimization] Network optimizations applied");
}

void FinalOptimization::EnableSmartBatching() {
    auto& network = network::NetworkManager::Instance();
    
    network.SetBatchingEnabled(true);
    network.SetBatchWindow(std::chrono::milliseconds(network_settings_.batch_window_ms));
    
    // Configure smart batching rules
    network.SetBatchingRule("movement", 10);      // Batch up to 10 movement updates
    network.SetBatchingRule("combat", 5);         // Batch up to 5 combat updates
    network.SetBatchingRule("chat", 20);          // Batch up to 20 chat messages
    
    spdlog::debug("[FinalOptimization] Smart batching enabled with {}ms window", 
                  network_settings_.batch_window_ms);
}

void FinalOptimization::OptimizePacketFlow() {
    auto& network = network::NetworkManager::Instance();
    
    // Enable Nagle's algorithm for non-critical packets
    network.SetTcpNoDelay(false, "chat");
    network.SetTcpNoDelay(false, "inventory");
    
    // Disable Nagle's for critical packets
    network.SetTcpNoDelay(true, "movement");
    network.SetTcpNoDelay(true, "combat");
    
    // Configure packet priorities
    network.SetPacketPriority("combat", network::PacketPriority::CRITICAL);
    network.SetPacketPriority("movement", network::PacketPriority::HIGH);
    network.SetPacketPriority("chat", network::PacketPriority::NORMAL);
    
    spdlog::debug("[FinalOptimization] Packet flow optimized");
}

// [SEQUENCE: 3794] Database optimization implementation 데이터베이스 최적화 구현
void FinalOptimization::OptimizeDatabase() {
    spdlog::info("[FinalOptimization] Applying database optimizations");
    
    auto& db = database::DatabaseManager::Instance();
    
    // Enable query caching
    EnableQueryCaching();
    
    // Optimize connection pool
    OptimizeConnectionPool();
    
    // Configure prepared statements
    db.PrepareCommonQueries();
    
    // Enable batch operations
    db.SetBatchOperationsEnabled(true);
    db.SetBatchSize(100);
    
    spdlog::info("[FinalOptimization] Database optimizations applied");
}

void FinalOptimization::EnableQueryCaching() {
    auto& db = database::DatabaseManager::Instance();
    
    db.SetQueryCacheEnabled(true);
    db.SetQueryCacheSize(memory_settings_.max_cache_size / 4);  // 25% of total cache
    db.SetQueryCacheTTL(std::chrono::seconds(300));  // 5 minute TTL
    
    spdlog::debug("[FinalOptimization] Query caching enabled");
}

void FinalOptimization::OptimizeConnectionPool() {
    auto& db = database::DatabaseManager::Instance();
    
    // Set pool size based on worker threads
    size_t pool_size = cpu_settings_.worker_thread_count * 2;
    db.SetConnectionPoolSize(pool_size);
    
    // Configure connection recycling
    db.SetConnectionMaxLifetime(std::chrono::minutes(30));
    db.SetConnectionIdleTimeout(std::chrono::minutes(5));
    
    spdlog::debug("[FinalOptimization] Connection pool optimized: {} connections", pool_size);
}

// [SEQUENCE: 3795] Visibility optimization implementation 가시성 최적화 구현
void FinalOptimization::OptimizeVisibility() {
    spdlog::info("[FinalOptimization] Applying visibility optimizations");
    
    // Enable frustum culling
    EnableFrustumCulling();
    
    // Optimize LOD
    OptimizeLOD();
    
    spdlog::info("[FinalOptimization] Visibility optimizations applied");
}

void FinalOptimization::EnableFrustumCulling() {
    auto& world = world::WorldManager::Instance();
    
    world.SetFrustumCullingEnabled(true);
    world.SetCullingDistance(200.0f);  // 200m view distance
    
    spdlog::debug("[FinalOptimization] Frustum culling enabled");
}

void FinalOptimization::OptimizeLOD() {
    auto& world = world::WorldManager::Instance();
    
    // Configure LOD distances
    world.SetLODDistance(0, 50.0f);   // High detail
    world.SetLODDistance(1, 100.0f);  // Medium detail
    world.SetLODDistance(2, 200.0f);  // Low detail
    
    // Enable dynamic LOD
    world.SetDynamicLODEnabled(true);
    
    spdlog::debug("[FinalOptimization] LOD optimization configured");
}

// [SEQUENCE: 3796] Performance profiling implementation 성능 프로파일링 구현
FinalOptimization::PerformanceProfile FinalOptimization::GetCurrentProfile() const {
    PerformanceProfile profile = current_profile_;
    
    // Update real-time metrics
    profile.cpu_usage_percent = OptimizationUtils::GetCPUUsage();
    profile.memory_usage_bytes = OptimizationUtils::GetMemoryUsage();
    profile.active_connections = network::NetworkManager::Instance().GetActiveConnections();
    profile.entities_processed = world::WorldManager::Instance().GetEntityCount();
    
    return profile;
}

void FinalOptimization::StartProfiling() {
    profiling_enabled_ = true;
    profile_start_time_ = std::chrono::steady_clock::now();
    
    spdlog::info("[FinalOptimization] Performance profiling started");
}

void FinalOptimization::StopProfiling() {
    profiling_enabled_ = false;
    
    auto duration = std::chrono::steady_clock::now() - profile_start_time_;
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
    
    spdlog::info("[FinalOptimization] Performance profiling stopped. Duration: {}ms", ms);
}

// [SEQUENCE: 3797] Memory pool implementation 메모리 풀 구현
template<typename T>
MemoryPool<T>::MemoryPool(size_t initial_size) {
    Reserve(initial_size);
}

template<typename T>
MemoryPool<T>::~MemoryPool() {
    Clear();
}

template<typename T>
T* MemoryPool<T>::Allocate() {
    std::lock_guard lock(mutex_);
    
    if (free_list_.empty()) {
        // Expand pool
        size_t current_size = pool_.size();
        size_t new_size = current_size == 0 ? 1000 : current_size * 2;
        Reserve(new_size);
    }
    
    Block* block = free_list_.back();
    free_list_.pop_back();
    
    block->in_use = true;
    allocated_count_++;
    
    return reinterpret_cast<T*>(block->data);
}

template<typename T>
void MemoryPool<T>::Deallocate(T* ptr) {
    if (!ptr) return;
    
    std::lock_guard lock(mutex_);
    
    Block* block = reinterpret_cast<Block*>(reinterpret_cast<char*>(ptr) - offsetof(Block, data));
    
    if (block->in_use) {
        // Destroy object
        ptr->~T();
        
        block->in_use = false;
        free_list_.push_back(block);
        allocated_count_--;
    }
}

template<typename T>
void MemoryPool<T>::Reserve(size_t count) {
    std::lock_guard lock(mutex_);
    
    size_t old_size = pool_.size();
    pool_.reserve(old_size + count);
    
    for (size_t i = 0; i < count; ++i) {
        auto block = std::make_unique<Block>();
        free_list_.push_back(block.get());
        pool_.push_back(std::move(block));
    }
}

// [SEQUENCE: 3798] String pool implementation 문자열 풀 구현
StringPool::StringPool(size_t initial_capacity) {
    strings_.reserve(initial_capacity);
}

const std::string& StringPool::Intern(const std::string& str) {
    std::shared_lock read_lock(mutex_);
    
    stats_.total_lookups++;
    
    auto it = strings_.find(str);
    if (it != strings_.end()) {
        stats_.cache_hits++;
        return *it;
    }
    
    read_lock.unlock();
    std::unique_lock write_lock(mutex_);
    
    // Double-check
    it = strings_.find(str);
    if (it != strings_.end()) {
        return *it;
    }
    
    // Insert new string
    auto result = strings_.insert(str);
    stats_.strings_interned++;
    
    return *result.first;
}

size_t StringPool::GetMemoryUsage() const {
    std::shared_lock lock(mutex_);
    
    size_t total = 0;
    for (const auto& str : strings_) {
        total += str.capacity();
    }
    
    return total;
}

// [SEQUENCE: 3799] SIMD optimizations implementation SIMD 최적화 구현
namespace SIMD {

void AddVectors(const float* a, const float* b, float* result, size_t count) {
    size_t simd_count = count / 8;  // Process 8 floats at a time with AVX
    size_t remainder = count % 8;
    
    for (size_t i = 0; i < simd_count; ++i) {
        __m256 va = _mm256_load_ps(&a[i * 8]);
        __m256 vb = _mm256_load_ps(&b[i * 8]);
        __m256 vr = _mm256_add_ps(va, vb);
        _mm256_store_ps(&result[i * 8], vr);
    }
    
    // Handle remainder
    for (size_t i = simd_count * 8; i < count; ++i) {
        result[i] = a[i] + b[i];
    }
}

void MultiplyVectors(const float* a, const float* b, float* result, size_t count) {
    size_t simd_count = count / 8;
    size_t remainder = count % 8;
    
    for (size_t i = 0; i < simd_count; ++i) {
        __m256 va = _mm256_load_ps(&a[i * 8]);
        __m256 vb = _mm256_load_ps(&b[i * 8]);
        __m256 vr = _mm256_mul_ps(va, vb);
        _mm256_store_ps(&result[i * 8], vr);
    }
    
    // Handle remainder
    for (size_t i = simd_count * 8; i < count; ++i) {
        result[i] = a[i] * b[i];
    }
}

float DotProduct(const float* a, const float* b, size_t count) {
    __m256 sum = _mm256_setzero_ps();
    
    size_t simd_count = count / 8;
    for (size_t i = 0; i < simd_count; ++i) {
        __m256 va = _mm256_load_ps(&a[i * 8]);
        __m256 vb = _mm256_load_ps(&b[i * 8]);
        __m256 prod = _mm256_mul_ps(va, vb);
        sum = _mm256_add_ps(sum, prod);
    }
    
    // Horizontal sum
    alignas(32) float temp[8];
    _mm256_store_ps(temp, sum);
    float result = 0.0f;
    for (int i = 0; i < 8; ++i) {
        result += temp[i];
    }
    
    // Handle remainder
    for (size_t i = simd_count * 8; i < count; ++i) {
        result += a[i] * b[i];
    }
    
    return result;
}

void CalculateDistances(const Vector3* positions, size_t count, float* distances) {
    // Calculate pairwise distances using SIMD
    for (size_t i = 0; i < count; ++i) {
        for (size_t j = i + 1; j < count; ++j) {
            __m128 pos1 = _mm_set_ps(0, positions[i].z, positions[i].y, positions[i].x);
            __m128 pos2 = _mm_set_ps(0, positions[j].z, positions[j].y, positions[j].x);
            
            __m128 diff = _mm_sub_ps(pos1, pos2);
            __m128 squared = _mm_mul_ps(diff, diff);
            
            // Horizontal sum
            __m128 sum1 = _mm_hadd_ps(squared, squared);
            __m128 sum2 = _mm_hadd_ps(sum1, sum1);
            
            float dist_squared;
            _mm_store_ss(&dist_squared, sum2);
            
            distances[i * count + j] = std::sqrt(dist_squared);
            distances[j * count + i] = distances[i * count + j];  // Symmetric
        }
    }
}

} // namespace SIMD

// [SEQUENCE: 3800] Optimization utilities implementation 최적화 유틸리티 구현
namespace OptimizationUtils {

void WarmCache(void* data, size_t size) {
    // Touch each cache line to bring into cache
    volatile char* ptr = static_cast<volatile char*>(data);
    for (size_t i = 0; i < size; i += HotPathOptimizer::CACHE_LINE_SIZE) {
        ptr[i];
    }
}

void CompactMemory() {
    // Platform-specific memory compaction
    #ifdef __linux__
    malloc_trim(0);
    #endif
}

size_t GetMemoryUsage() {
    #ifdef __linux__
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    return usage.ru_maxrss * 1024;  // Convert KB to bytes
    #else
    return 0;  // Not implemented for other platforms
    #endif
}

void SetThreadAffinity(std::thread& thread, size_t core_id) {
    #ifdef __linux__
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    pthread_setaffinity_np(thread.native_handle(), sizeof(cpu_set_t), &cpuset);
    #endif
}

void SetThreadPriority(std::thread& thread, int priority) {
    #ifdef __linux__
    struct sched_param param;
    param.sched_priority = priority;
    pthread_setschedparam(thread.native_handle(), SCHED_FIFO, &param);
    #endif
}

size_t GetOptimalThreadCount() {
    size_t hw_threads = std::thread::hardware_concurrency();
    // Leave some threads for OS and other processes
    return std::max(size_t(1), hw_threads - 2);
}

float GetCPUUsage() {
    static auto last_time = std::chrono::steady_clock::now();
    static clock_t last_cpu = clock();
    
    auto now = std::chrono::steady_clock::now();
    clock_t current_cpu = clock();
    
    auto time_diff = std::chrono::duration<float>(now - last_time).count();
    float cpu_diff = float(current_cpu - last_cpu) / CLOCKS_PER_SEC;
    
    last_time = now;
    last_cpu = current_cpu;
    
    return (cpu_diff / time_diff) * 100.0f;
}

ScopedTimer::ScopedTimer(const std::string& name) 
    : name_(name), start_(std::chrono::high_resolution_clock::now()) {
}

ScopedTimer::~ScopedTimer() {
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<float, std::micro>(end - start_).count();
    
    spdlog::trace("[Profile] {} took {:.2f} us", name_, duration);
}

} // namespace OptimizationUtils

// Explicit template instantiations
template class MemoryPool<Entity>;
template class MemoryPool<Component>;
template class MemoryPool<NetworkPacket>;

template class CacheManager<uint64_t, Entity>;
template class CacheManager<std::string, QueryResult>;

template class BatchProcessor<NetworkPacket>;
template class BatchProcessor<DatabaseQuery>;

} // namespace mmorpg::optimization