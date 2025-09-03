#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <atomic>
#include <mutex>
#include <list>
#include <functional>
#include <vector>
#include <thread>
#include "core/ecs/types.h"

namespace mmorpg::database {

// [SEQUENCE: 3043] Cache entry status
enum class CacheStatus {
    VALID,          // 캐시 데이터 유효
    STALE,          // 만료되었지만 사용 가능
    REFRESHING,     // 갱신 중
    INVALID         // 무효화됨
};

// [SEQUENCE: 3044] Cache eviction policy
enum class EvictionPolicy {
    LRU,            // Least Recently Used
    LFU,            // Least Frequently Used
    FIFO,           // First In First Out
    TTL,            // Time To Live based
    SIZE_BASED,     // Size-based eviction
    ADAPTIVE        // Adaptive policy
};

// [SEQUENCE: 3045] Cache consistency model
enum class ConsistencyModel {
    WRITE_THROUGH,      // 쓰기 시 즉시 DB 업데이트
    WRITE_BEHIND,       // 비동기 DB 업데이트
    WRITE_AROUND,       // 캐시 우회, DB만 업데이트
    REFRESH_AHEAD,      // 만료 전 미리 갱신
    EVENTUAL            // 최종 일관성
};

// [SEQUENCE: 3046] Cache statistics
struct CacheStats {
    // Hit/Miss statistics
    std::atomic<uint64_t> hits{0};
    std::atomic<uint64_t> misses{0};
    std::atomic<uint64_t> evictions{0};
    std::atomic<uint64_t> expirations{0};
    
    // Performance metrics
    std::atomic<uint64_t> total_get_time_us{0};
    std::atomic<uint64_t> total_set_time_us{0};
    std::atomic<uint32_t> get_count{0};
    std::atomic<uint32_t> set_count{0};
    
    // Memory metrics
    std::atomic<size_t> memory_usage{0};
    std::atomic<size_t> entry_count{0};
    
    // Error metrics
    std::atomic<uint64_t> serialization_errors{0};
    std::atomic<uint64_t> deserialization_errors{0};
    
    double GetHitRate() const {
        uint64_t total = hits + misses;
        return total > 0 ? static_cast<double>(hits) / total : 0.0;
    }
    
    double GetAvgGetTimeUs() const {
        return get_count > 0 ? static_cast<double>(total_get_time_us) / get_count : 0.0;
    }
    
    double GetAvgSetTimeUs() const {
        return set_count > 0 ? static_cast<double>(total_set_time_us) / set_count : 0.0;
    }
};

// [SEQUENCE: 3047] Cache entry metadata
template<typename T>
struct CacheEntry {
    T data;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_accessed;
    std::chrono::system_clock::time_point expires_at;
    
    uint32_t access_count{0};
    size_t size_bytes{0};
    CacheStatus status{CacheStatus::VALID};
    
    // For write-behind
    bool dirty{false};
    std::chrono::system_clock::time_point last_modified;
    
    bool IsExpired() const {
        return std::chrono::system_clock::now() > expires_at;
    }
    
    bool IsStale(std::chrono::seconds staleness_threshold) const {
        auto age = std::chrono::system_clock::now() - created_at;
        return age > staleness_threshold;
    }
};

// [SEQUENCE: 3048] Cache layer base class
template<typename KeyType, typename ValueType>
class CacheLayer {
public:
    using Key = KeyType;
    using Value = ValueType;
    using Entry = CacheEntry<Value>;
    
    CacheLayer(size_t max_size, EvictionPolicy policy)
        : max_size_(max_size), eviction_policy_(policy) {}
    
    virtual ~CacheLayer() = default;
    
    // [SEQUENCE: 3049] Core cache operations
    virtual bool Get(const Key& key, Value& value) = 0;
    virtual void Set(const Key& key, const Value& value, 
                    std::chrono::seconds ttl = std::chrono::seconds(300)) = 0;
    virtual bool Delete(const Key& key) = 0;
    virtual void Clear() = 0;
    
    // [SEQUENCE: 3050] Batch operations
    virtual std::unordered_map<Key, Value> MultiGet(const std::vector<Key>& keys) {
        std::unordered_map<Key, Value> results;
        for (const auto& key : keys) {
            Value value;
            if (Get(key, value)) {
                results[key] = value;
            }
        }
        return results;
    }
    
    virtual void MultiSet(const std::unordered_map<Key, Value>& items,
                         std::chrono::seconds ttl = std::chrono::seconds(300)) {
        for (const auto& [key, value] : items) {
            Set(key, value, ttl);
        }
    }
    
    // [SEQUENCE: 3051] Cache management
    virtual size_t Size() const = 0;
    virtual CacheStats GetStats() const { return stats_; }
    virtual void ResetStats() { stats_ = CacheStats(); }
    
protected:
    size_t max_size_;
    EvictionPolicy eviction_policy_;
    mutable CacheStats stats_;
};

// [SEQUENCE: 3052] LRU cache implementation
template<typename KeyType, typename ValueType>
class LRUCache : public CacheLayer<KeyType, ValueType> {
public:
    using Key = KeyType;
    using Value = ValueType;
    using Entry = CacheEntry<Value>;
    using Base = CacheLayer<Key, Value>;
    
    LRUCache(size_t max_size) 
        : Base(max_size, EvictionPolicy::LRU) {}
    
    // [SEQUENCE: 3053] LRU Get implementation
    bool Get(const Key& key, Value& value) override {
        auto start = std::chrono::high_resolution_clock::now();
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_map_.find(key);
        if (it == cache_map_.end()) {
            this->stats_.misses++;
            UpdateGetStats(start);
            return false;
        }
        
        // Check expiration
        if (it->second->entry.IsExpired()) {
            RemoveEntry(it);
            this->stats_.expirations++;
            this->stats_.misses++;
            UpdateGetStats(start);
            return false;
        }
        
        // Move to front (most recently used)
        lru_list_.splice(lru_list_.begin(), lru_list_, it->second);
        
        // Update access info
        it->second->entry.last_accessed = std::chrono::system_clock::now();
        it->second->entry.access_count++;
        
        value = it->second->entry.data;
        this->stats_.hits++;
        UpdateGetStats(start);
        
        return true;
    }
    
    // [SEQUENCE: 3054] LRU Set implementation
    void Set(const Key& key, const Value& value, 
            std::chrono::seconds ttl = std::chrono::seconds(300)) override {
        auto start = std::chrono::high_resolution_clock::now();
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            // Update existing entry
            it->second->entry.data = value;
            it->second->entry.expires_at = std::chrono::system_clock::now() + ttl;
            it->second->entry.last_accessed = std::chrono::system_clock::now();
            
            // Move to front
            lru_list_.splice(lru_list_.begin(), lru_list_, it->second);
        } else {
            // Add new entry
            if (cache_map_.size() >= this->max_size_) {
                EvictLRU();
            }
            
            auto node = std::make_unique<LRUNode>();
            node->key = key;
            node->entry.data = value;
            node->entry.created_at = std::chrono::system_clock::now();
            node->entry.last_accessed = node->entry.created_at;
            node->entry.expires_at = node->entry.created_at + ttl;
            node->entry.size_bytes = EstimateSize(value);
            
            lru_list_.push_front(node.get());
            cache_map_[key] = lru_list_.begin();
            nodes_[key] = std::move(node);
            
            this->stats_.entry_count++;
            this->stats_.memory_usage += node->entry.size_bytes;
        }
        
        UpdateSetStats(start);
    }
    
    // [SEQUENCE: 3055] Delete entry
    bool Delete(const Key& key) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = cache_map_.find(key);
        if (it != cache_map_.end()) {
            RemoveEntry(it);
            return true;
        }
        return false;
    }
    
    // [SEQUENCE: 3056] Clear all entries
    void Clear() override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        cache_map_.clear();
        lru_list_.clear();
        nodes_.clear();
        
        this->stats_.entry_count = 0;
        this->stats_.memory_usage = 0;
    }
    
    size_t Size() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_map_.size();
    }
    
private:
    struct LRUNode {
        Key key;
        Entry entry;
    };
    
    using NodePtr = LRUNode*;
    using ListIterator = typename std::list<NodePtr>::iterator;
    
    std::unordered_map<Key, ListIterator> cache_map_;
    std::list<NodePtr> lru_list_;
    std::unordered_map<Key, std::unique_ptr<LRUNode>> nodes_;
    
    mutable std::mutex mutex_;
    
    // [SEQUENCE: 3057] Evict least recently used
    void EvictLRU() {
        if (lru_list_.empty()) return;
        
        auto lru_node = lru_list_.back();
        cache_map_.erase(lru_node->key);
        
        this->stats_.evictions++;
        this->stats_.entry_count--;
        this->stats_.memory_usage -= lru_node->entry.size_bytes;
        
        nodes_.erase(lru_node->key);
        lru_list_.pop_back();
    }
    
    void RemoveEntry(typename std::unordered_map<Key, ListIterator>::iterator it) {
        auto node = *(it->second);
        
        this->stats_.entry_count--;
        this->stats_.memory_usage -= node->entry.size_bytes;
        
        lru_list_.erase(it->second);
        nodes_.erase(node->key);
        cache_map_.erase(it);
    }
    
    void UpdateGetStats(std::chrono::high_resolution_clock::time_point start) {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start).count();
        this->stats_.total_get_time_us += duration;
        this->stats_.get_count++;
    }
    
    void UpdateSetStats(std::chrono::high_resolution_clock::time_point start) {
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start).count();
        this->stats_.total_set_time_us += duration;
        this->stats_.set_count++;
    }
    
    size_t EstimateSize(const Value& value) {
        // Simple size estimation - override for complex types
        return sizeof(Value);
    }
};

// [SEQUENCE: 3058] Two-level cache (L1 + L2)
template<typename KeyType, typename ValueType>
class TwoLevelCache {
public:
    using Key = KeyType;
    using Value = ValueType;
    
    TwoLevelCache(size_t l1_size, size_t l2_size)
        : l1_cache_(std::make_unique<LRUCache<Key, Value>>(l1_size))
        , l2_cache_(std::make_unique<LRUCache<Key, Value>>(l2_size)) {}
    
    // [SEQUENCE: 3059] Two-level get
    bool Get(const Key& key, Value& value) {
        // Check L1 first
        if (l1_cache_->Get(key, value)) {
            l1_hits_++;
            return true;
        }
        
        // Check L2
        if (l2_cache_->Get(key, value)) {
            l2_hits_++;
            // Promote to L1
            l1_cache_->Set(key, value);
            return true;
        }
        
        misses_++;
        return false;
    }
    
    // [SEQUENCE: 3060] Two-level set
    void Set(const Key& key, const Value& value, 
            std::chrono::seconds ttl = std::chrono::seconds(300)) {
        // Always set in L1
        l1_cache_->Set(key, value, ttl);
        
        // Optionally set in L2 for larger items
        if (ShouldSetInL2(value)) {
            l2_cache_->Set(key, value, ttl * 2);  // Longer TTL in L2
        }
    }
    
    struct TwoLevelStats {
        uint64_t l1_hits{0};
        uint64_t l2_hits{0};
        uint64_t misses{0};
        
        CacheStats l1_stats;
        CacheStats l2_stats;
    };
    
    TwoLevelStats GetStats() const {
        TwoLevelStats stats;
        stats.l1_hits = l1_hits_;
        stats.l2_hits = l2_hits_;
        stats.misses = misses_;
        stats.l1_stats = l1_cache_->GetStats();
        stats.l2_stats = l2_cache_->GetStats();
        return stats;
    }
    
private:
    std::unique_ptr<LRUCache<Key, Value>> l1_cache_;
    std::unique_ptr<LRUCache<Key, Value>> l2_cache_;
    
    std::atomic<uint64_t> l1_hits_{0};
    std::atomic<uint64_t> l2_hits_{0};
    std::atomic<uint64_t> misses_{0};
    
    bool ShouldSetInL2(const Value& value) {
        // Set in L2 if value is "important" (override for custom logic)
        return true;
    }
};

// [SEQUENCE: 3061] Write-through cache wrapper
template<typename KeyType, typename ValueType>
class WriteThroughCache {
public:
    using Key = KeyType;
    using Value = ValueType;
    using LoadFunction = std::function<bool(const Key&, Value&)>;
    using StoreFunction = std::function<bool(const Key&, const Value&)>;
    
    WriteThroughCache(std::unique_ptr<CacheLayer<Key, Value>> cache,
                     LoadFunction loader,
                     StoreFunction storer)
        : cache_(std::move(cache))
        , loader_(loader)
        , storer_(storer) {}
    
    // [SEQUENCE: 3062] Read-through get
    bool Get(const Key& key, Value& value) {
        // Try cache first
        if (cache_->Get(key, value)) {
            return true;
        }
        
        // Load from storage
        if (loader_(key, value)) {
            // Cache the loaded value
            cache_->Set(key, value);
            return true;
        }
        
        return false;
    }
    
    // [SEQUENCE: 3063] Write-through set
    bool Set(const Key& key, const Value& value) {
        // Write to storage first
        if (!storer_(key, value)) {
            return false;
        }
        
        // Update cache
        cache_->Set(key, value);
        return true;
    }
    
private:
    std::unique_ptr<CacheLayer<Key, Value>> cache_;
    LoadFunction loader_;
    StoreFunction storer_;
};

// [SEQUENCE: 3064] Cache invalidation strategies
class CacheInvalidator {
public:
    // [SEQUENCE: 3065] Invalidation patterns
    enum class InvalidationPattern {
        SINGLE_KEY,         // 단일 키 무효화
        KEY_PATTERN,        // 패턴 매칭 무효화
        TAG_BASED,          // 태그 기반 무효화
        TIME_BASED,         // 시간 기반 무효화
        CASCADE             // 연쇄 무효화
    };
    
    // [SEQUENCE: 3066] Invalidate by pattern
    template<typename Cache>
    static void InvalidateByPattern(Cache& cache, const std::string& pattern) {
        // Implementation would iterate and invalidate matching keys
    }
    
    // [SEQUENCE: 3067] Invalidate by tags
    template<typename Cache>
    static void InvalidateByTags(Cache& cache, const std::vector<std::string>& tags) {
        // Implementation would track and invalidate tagged entries
    }
};

// [SEQUENCE: 3068] Cache warming strategies
class CacheWarmer {
public:
    // [SEQUENCE: 3069] Warming strategies
    enum class WarmingStrategy {
        LAZY,               // 요청 시 로드
        EAGER,              // 즉시 전체 로드
        PREDICTIVE,         // 예측 기반 로드
        SCHEDULED,          // 스케줄 기반 로드
        ADAPTIVE            // 적응형 로드
    };
    
    // [SEQUENCE: 3070] Warm cache with data
    template<typename Cache, typename DataSource>
    static void WarmCache(Cache& cache, DataSource& source, 
                         WarmingStrategy strategy = WarmingStrategy::LAZY) {
        switch (strategy) {
            case WarmingStrategy::EAGER:
                // Load all data immediately
                break;
            case WarmingStrategy::PREDICTIVE:
                // Load based on access patterns
                break;
            case WarmingStrategy::SCHEDULED:
                // Load on schedule
                break;
            default:
                // Lazy loading - do nothing
                break;
        }
    }
};

// [SEQUENCE: 3071] Global cache manager
class GlobalCacheManager {
public:
    static GlobalCacheManager& Instance() {
        static GlobalCacheManager instance;
        return instance;
    }
    
    // [SEQUENCE: 3072] Register named cache
    template<typename Key, typename Value>
    void RegisterCache(const std::string& name, 
                      std::unique_ptr<CacheLayer<Key, Value>> cache) {
        // Store cache with type erasure
    }
    
    // [SEQUENCE: 3073] Get named cache
    template<typename Key, typename Value>
    CacheLayer<Key, Value>* GetCache(const std::string& name) {
        // Retrieve cache with type checking
        return nullptr;
    }
    
    // [SEQUENCE: 3074] Global cache operations
    void ClearAllCaches();
    void PrintAllStats();
    void StartMaintenanceThread(std::chrono::seconds interval);
    
private:
    GlobalCacheManager() = default;
    ~GlobalCacheManager() = default;
    
    // Type-erased cache storage
    std::unordered_map<std::string, std::shared_ptr<void>> caches_;
    std::mutex mutex_;
    std::thread maintenance_thread_;
    std::atomic<bool> maintenance_running_{false};
    void PerformMaintenance();
};

} // namespace mmorpg::database