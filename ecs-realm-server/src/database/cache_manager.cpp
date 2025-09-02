#include "cache_manager.h"
#include "../core/logger.h"
#include <spdlog/spdlog.h>

namespace mmorpg::database {

// [SEQUENCE: MVP14-405] Global cache manager implementation
void GlobalCacheManager::ClearAllCaches() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    spdlog::info("[CACHE_MANAGER] Clearing all {} caches", caches_.size());
    
    // Note: In real implementation, would need proper type handling
    caches_.clear();
}

// [SEQUENCE: MVP14-406] Print statistics for all caches
void GlobalCacheManager::PrintAllStats() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    spdlog::info("[CACHE_MANAGER] === Global Cache Statistics ===");
    spdlog::info("[CACHE_MANAGER] Total caches: {}", caches_.size());
    
    // In real implementation, would iterate and print each cache's stats
    // This requires storing type information with the caches
}

// [SEQUENCE: MVP14-407] Start maintenance thread
void GlobalCacheManager::StartMaintenanceThread(std::chrono::seconds interval) {
    if (maintenance_running_) {
        spdlog::warn("[CACHE_MANAGER] Maintenance thread already running");
        return;
    }
    
    maintenance_running_ = true;
    maintenance_thread_ = std::thread([this, interval] {
        while (maintenance_running_) {
            std::this_thread::sleep_for(interval);
            
            if (!maintenance_running_) break;
            
            // Perform maintenance tasks
            PerformMaintenance();
        }
    });
    
    spdlog::info("[CACHE_MANAGER] Started maintenance thread with {}s interval", 
                interval.count());
}

// [SEQUENCE: MVP14-408] Perform cache maintenance
void GlobalCacheManager::PerformMaintenance() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // In real implementation:
    // - Evict expired entries
    // - Collect statistics
    // - Perform memory pressure checks
    // - Trigger cache warming if needed
    
    spdlog::debug("[CACHE_MANAGER] Performed maintenance on {} caches", caches_.size());
}

} // namespace mmorpg::database
