#pragma once

#include "cache_manager.h"
#include "../player/player_data.h"
#include "../items/item_data.h"
#include "../guild/guild_data.h"
#include <string>
#include <memory>

namespace mmorpg::database {

// [SEQUENCE: MVP14-576] Player cache configuration
struct PlayerCacheConfig {
    size_t l1_size = 10000;         // Hot cache: 10k active players
    size_t l2_size = 100000;        // Warm cache: 100k recent players
    
    std::chrono::seconds active_ttl{300};      // 5 minutes for active
    std::chrono::seconds inactive_ttl{3600};   // 1 hour for inactive
    std::chrono::seconds offline_ttl{86400};   // 24 hours for offline
    
    bool enable_write_behind = true;
    std::chrono::seconds write_delay{30};      // Write to DB after 30s
};

// [SEQUENCE: MVP14-577] Player data cache
class PlayerDataCache {
public:
    PlayerDataCache(const PlayerCacheConfig& config = {})
        : config_(config)
        , cache_(config.l1_size, config.l2_size) {
        
        InitializeWriteBehind();
    }
    
    // [SEQUENCE: MVP14-578] Get player data with status check
    bool GetPlayer(uint64_t player_id, PlayerData& data) {
        // Check online status for TTL adjustment
        auto ttl = IsPlayerOnline(player_id) ? config_.active_ttl : config_.inactive_ttl;
        
        if (cache_.Get(player_id, data)) {
            UpdateAccessPattern(player_id);
            return true;
        }
        
        // Load from database
        if (LoadFromDatabase(player_id, data)) {
            cache_.Set(player_id, data, ttl);
            return true;
        }
        
        return false;
    }
    
    // [SEQUENCE: MVP14-579] Update player data
    void UpdatePlayer(uint64_t player_id, const PlayerData& data) {
        auto ttl = IsPlayerOnline(player_id) ? config_.active_ttl : config_.inactive_ttl;
        cache_.Set(player_id, data, ttl);
        
        if (config_.enable_write_behind) {
            ScheduleWriteBehind(player_id, data);
        } else {
            WriteToDatabase(player_id, data);
        }
    }
    
    // [SEQUENCE: MVP14-580] Batch operations for efficiency
    std::unordered_map<uint64_t, PlayerData> GetMultiplePlayers(
        const std::vector<uint64_t>& player_ids) {
        
        std::unordered_map<uint64_t, PlayerData> results;
        std::vector<uint64_t> missing_ids;
        
        // Check cache first
        for (uint64_t id : player_ids) {
            PlayerData data;
            if (cache_.Get(id, data)) {
                results[id] = data;
            } else {
                missing_ids.push_back(id);
            }
        }
        
        // Batch load missing from database
        if (!missing_ids.empty()) {
            auto db_results = BatchLoadFromDatabase(missing_ids);
            for (const auto& [id, data] : db_results) {
                results[id] = data;
                cache_.Set(id, data);
            }
        }
        
        return results;
    }
    
    // [SEQUENCE: MVP14-581] Invalidate player cache
    void InvalidatePlayer(uint64_t player_id) {
        // Flush any pending writes
        if (config_.enable_write_behind) {
            FlushPendingWrite(player_id);
        }
        
        // Remove from both cache levels
        PlayerData dummy;
        cache_.Get(player_id, dummy);  // This removes from cache
    }
    
    // [SEQUENCE: MVP14-582] Preload frequently accessed players
    void PreloadFrequentPlayers() {
        auto frequent_players = GetFrequentlyAccessedPlayers(1000);
        
        for (uint64_t player_id : frequent_players) {
            PlayerData data;
            if (LoadFromDatabase(player_id, data)) {
                cache_.Set(player_id, data, config_.active_ttl);
            }
        }
        
        spdlog::info("[PLAYER_CACHE] Preloaded {} frequent players", 
                    frequent_players.size());
    }
    
    // [SEQUENCE: MVP14-583] Get cache statistics
    struct PlayerCacheStats {
        TwoLevelCache<uint64_t, PlayerData>::TwoLevelStats cache_stats;
        uint64_t write_behind_pending{0};
        uint64_t write_behind_completed{0};
        uint64_t write_behind_failed{0};
    };
    
    PlayerCacheStats GetStats() const {
        PlayerCacheStats stats;
        stats.cache_stats = cache_.GetStats();
        stats.write_behind_pending = write_behind_queue_.size();
        stats.write_behind_completed = write_behind_completed_;
        stats.write_behind_failed = write_behind_failed_;
        return stats;
    }
    
private:
    PlayerCacheConfig config_;
    TwoLevelCache<uint64_t, PlayerData> cache_;
    
    // Write-behind queue
    struct WriteBehindEntry {
        uint64_t player_id;
        PlayerData data;
        std::chrono::system_clock::time_point scheduled_time;
    };
    
    std::unordered_map<uint64_t, WriteBehindEntry> write_behind_queue_;
    std::mutex write_behind_mutex_;
    std::thread write_behind_thread_;
    std::atomic<bool> write_behind_running_{false};
    
    std::atomic<uint64_t> write_behind_completed_{0};
    std::atomic<uint64_t> write_behind_failed_{0};
    
    // Access pattern tracking
    std::unordered_map<uint64_t, uint32_t> access_frequency_;
    std::mutex access_mutex_;
    
    void InitializeWriteBehind() {
        if (!config_.enable_write_behind) return;
        
        write_behind_running_ = true;
        write_behind_thread_ = std::thread([this] {
            WriteBehindLoop();
        });
    }
    
    void WriteBehindLoop() {
        while (write_behind_running_) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            std::vector<WriteBehindEntry> entries_to_write;
            
            {
                std::lock_guard<std::mutex> lock(write_behind_mutex_);
                auto now = std::chrono::system_clock::now();
                
                for (auto it = write_behind_queue_.begin(); 
                     it != write_behind_queue_.end(); ) {
                    if (it->second.scheduled_time <= now) {
                        entries_to_write.push_back(it->second);
                        it = write_behind_queue_.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
            
            // Write entries to database
            for (const auto& entry : entries_to_write) {
                if (WriteToDatabase(entry.player_id, entry.data)) {
                    write_behind_completed_++;
                } else {
                    write_behind_failed_++;
                }
            }
        }
    }
    
    void ScheduleWriteBehind(uint64_t player_id, const PlayerData& data) {
        std::lock_guard<std::mutex> lock(write_behind_mutex_);
        
        WriteBehindEntry entry;
        entry.player_id = player_id;
        entry.data = data;
        entry.scheduled_time = std::chrono::system_clock::now() + config_.write_delay;
        
        write_behind_queue_[player_id] = entry;
    }
    
    void FlushPendingWrite(uint64_t player_id) {
        WriteBehindEntry entry;
        
        {
            std::lock_guard<std::mutex> lock(write_behind_mutex_);
            auto it = write_behind_queue_.find(player_id);
            if (it != write_behind_queue_.end()) {
                entry = it->second;
                write_behind_queue_.erase(it);
            } else {
                return;
            }
        }
        
        WriteToDatabase(entry.player_id, entry.data);
    }
    
    void UpdateAccessPattern(uint64_t player_id) {
        std::lock_guard<std::mutex> lock(access_mutex_);
        access_frequency_[player_id]++;
    }
    
    std::vector<uint64_t> GetFrequentlyAccessedPlayers(size_t count) {
        std::lock_guard<std::mutex> lock(access_mutex_);
        
        // Sort by frequency
        std::vector<std::pair<uint64_t, uint32_t>> freq_pairs(
            access_frequency_.begin(), access_frequency_.end());
        
        std::sort(freq_pairs.begin(), freq_pairs.end(),
            [](const auto& a, const auto& b) {
                return a.second > b.second;
            });
        
        std::vector<uint64_t> result;
        for (size_t i = 0; i < std::min(count, freq_pairs.size()); ++i) {
            result.push_back(freq_pairs[i].first);
        }
        
        return result;
    }
    
    // Database operations (would be implemented with actual DB)
    bool LoadFromDatabase(uint64_t player_id, PlayerData& data) {
        // Simulate DB load
        return false;
    }
    
    std::unordered_map<uint64_t, PlayerData> BatchLoadFromDatabase(
        const std::vector<uint64_t>& player_ids) {
        // Simulate batch DB load
        return {};
    }
    
    bool WriteToDatabase(uint64_t player_id, const PlayerData& data) {
        // Simulate DB write
        return true;
    }
    
    bool IsPlayerOnline(uint64_t player_id) {
        // Check online status
        return false;
    }
};

// [SEQUENCE: MVP14-584] Item cache for static data
class ItemDataCache {
public:
    ItemDataCache(size_t max_items = 50000)
        : cache_(max_items) {
        
        // Items are mostly static, so longer TTL
        default_ttl_ = std::chrono::seconds(3600);  // 1 hour
    }
    
    // [SEQUENCE: MVP14-585] Get item with read-through
    bool GetItem(uint32_t item_id, ItemData& data) {
        if (cache_.Get(item_id, data)) {
            return true;
        }
        
        // Load from static data or database
        if (LoadItemFromSource(item_id, data)) {
            cache_.Set(item_id, data, default_ttl_);
            return true;
        }
        
        return false;
    }
    
    // [SEQUENCE: MVP14-586] Preload all items on startup
    void PreloadAllItems() {
        auto all_items = LoadAllItemsFromSource();
        
        for (const auto& [id, data] : all_items) {
            cache_.Set(id, data, default_ttl_);
        }
        
        spdlog::info("[ITEM_CACHE] Preloaded {} items", all_items.size());
    }
    
    // [SEQUENCE: MVP14-587] Invalidate when items are updated
    void InvalidateItem(uint32_t item_id) {
        cache_.Delete(item_id);
    }
    
    void RefreshAllItems() {
        cache_.Clear();
        PreloadAllItems();
    }
    
private:
    LRUCache<uint32_t, ItemData> cache_;
    std::chrono::seconds default_ttl_;
    
    bool LoadItemFromSource(uint32_t item_id, ItemData& data) {
        // Load from static data files or database
        return false;
    }
    
    std::unordered_map<uint32_t, ItemData> LoadAllItemsFromSource() {
        // Load all items
        return {};
    }
};

// [SEQUENCE: MVP14-588] Guild cache with member tracking
class GuildDataCache {
public:
    GuildDataCache(size_t max_guilds = 5000)
        : cache_(max_guilds) {
        
        active_ttl_ = std::chrono::seconds(600);    // 10 minutes for active
        inactive_ttl_ = std::chrono::seconds(3600); // 1 hour for inactive
    }
    
    // [SEQUENCE: MVP14-589] Get guild with member preloading
    bool GetGuild(uint32_t guild_id, GuildData& data) {
        if (cache_.Get(guild_id, data)) {
            return true;
        }
        
        if (LoadGuildFromDatabase(guild_id, data)) {
            auto ttl = IsGuildActive(guild_id) ? active_ttl_ : inactive_ttl_;
            cache_.Set(guild_id, data, ttl);
            
            // Preload guild members into player cache
            PreloadGuildMembers(data);
            
            return true;
        }
        
        return false;
    }
    
    // [SEQUENCE: MVP14-590] Update guild data
    void UpdateGuild(uint32_t guild_id, const GuildData& data) {
        auto ttl = IsGuildActive(guild_id) ? active_ttl_ : inactive_ttl_;
        cache_.Set(guild_id, data, ttl);
        
        // Invalidate related caches
        InvalidateRelatedCaches(guild_id);
        
        // Write to database
        WriteGuildToDatabase(guild_id, data);
    }
    
    // [SEQUENCE: MVP14-591] Get guilds by member
    std::vector<uint32_t> GetGuildsByMember(uint64_t player_id) {
        // Use member index cache
        std::vector<uint32_t> guild_ids;
        
        auto it = member_index_.find(player_id);
        if (it != member_index_.end()) {
            guild_ids.push_back(it->second);
        }
        
        return guild_ids;
    }
    
    // [SEQUENCE: MVP14-592] Preload active guilds
    void PreloadActiveGuilds() {
        auto active_guilds = GetActiveGuildIds();
        
        for (uint32_t guild_id : active_guilds) {
            GuildData data;
            if (LoadGuildFromDatabase(guild_id, data)) {
                cache_.Set(guild_id, data, active_ttl_);
                UpdateMemberIndex(data);
            }
        }
        
        spdlog::info("[GUILD_CACHE] Preloaded {} active guilds", 
                    active_guilds.size());
    }
    
private:
    LRUCache<uint32_t, GuildData> cache_;
    std::chrono::seconds active_ttl_;
    std::chrono::seconds inactive_ttl_;
    
    // Member to guild index
    std::unordered_map<uint64_t, uint32_t> member_index_;
    std::mutex index_mutex_;
    
    void UpdateMemberIndex(const GuildData& data) {
        std::lock_guard<std::mutex> lock(index_mutex_);
        
        for (const auto& member : data.members) {
            member_index_[member.player_id] = data.guild_id;
        }
    }
    
    void PreloadGuildMembers(const GuildData& data) {
        // Request player cache to preload guild members
        std::vector<uint64_t> member_ids;
        for (const auto& member : data.members) {
            member_ids.push_back(member.player_id);
        }
        
        // PlayerDataCache::Instance().PreloadPlayers(member_ids);
    }
    
    void InvalidateRelatedCaches(uint32_t guild_id) {
        // Invalidate guild war cache, rankings, etc.
    }
    
    bool LoadGuildFromDatabase(uint32_t guild_id, GuildData& data) {
        return false;
    }
    
    bool WriteGuildToDatabase(uint32_t guild_id, const GuildData& data) {
        return true;
    }
    
    bool IsGuildActive(uint32_t guild_id) {
        // Check if guild has online members
        return false;
    }
    
    std::vector<uint32_t> GetActiveGuildIds() {
        // Get guilds with recent activity
        return {};
    }
};

// [SEQUENCE: MVP14-593] Query result cache for expensive operations
class QueryResultCache {
public:
    using QueryKey = std::string;
    using QueryResult = std::string;  // JSON or serialized result
    
    QueryResultCache(size_t max_entries = 10000)
        : cache_(max_entries) {
        
        default_ttl_ = std::chrono::seconds(300);  // 5 minutes default
    }
    
    // [SEQUENCE: MVP14-594] Cache query results
    bool GetQueryResult(const QueryKey& key, QueryResult& result) {
        return cache_.Get(key, result);
    }
    
    void SetQueryResult(const QueryKey& key, const QueryResult& result,
                       std::chrono::seconds ttl = std::chrono::seconds(300)) {
        cache_.Set(key, result, ttl);
    }
    
    // [SEQUENCE: MVP14-595] Generate cache key from query
    static QueryKey GenerateKey(const std::string& query, 
                               const std::vector<std::string>& params) {
        std::string key = query;
        for (const auto& param : params) {
            key += "|" + param;
        }
        return key;
    }
    
    // [SEQUENCE: MVP14-596] Invalidate by query pattern
    void InvalidatePattern(const std::string& pattern) {
        // Would iterate and invalidate matching keys
    }
    
private:
    LRUCache<QueryKey, QueryResult> cache_;
    std::chrono::seconds default_ttl_;
};

// [SEQUENCE: MVP14-597] Initialize all game caches
void InitializeGameCaches() {
    auto& manager = GlobalCacheManager::Instance();
    
    // Register player cache
    auto player_cache = std::make_unique<PlayerDataCache>();
    player_cache->PreloadFrequentPlayers();
    
    // Register item cache
    auto item_cache = std::make_unique<ItemDataCache>();
    item_cache->PreloadAllItems();
    
    // Register guild cache
    auto guild_cache = std::make_unique<GuildDataCache>();
    guild_cache->PreloadActiveGuilds();
    
    // Register query result cache
    auto query_cache = std::make_unique<QueryResultCache>();
    
    // Start maintenance
    manager.StartMaintenanceThread(std::chrono::seconds(60));
    
    spdlog::info("[GAME_CACHE] Initialized all game caches");
}

} // namespace mmorpg::database
