#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <functional>
#include <mutex>
#include <atomic>
#include "../core/types.h"

namespace mmorpg::database {

// [SEQUENCE: MVP14-267] Database partitioning strategies
enum class PartitionStrategy {
    RANGE,          // 범위 기반 (날짜, ID 범위)
    HASH,           // 해시 기반 (균등 분산)
    LIST,           // 리스트 기반 (특정 값)
    COMPOSITE,      // 복합 파티셔닝
    ROUND_ROBIN     // 라운드 로빈
};

// [SEQUENCE: MVP14-268] Partition key types
enum class PartitionKeyType {
    PLAYER_ID,      // 플레이어 ID 기반
    TIMESTAMP,      // 시간 기반
    GUILD_ID,       // 길드 ID 기반
    SERVER_ID,      // 서버 ID 기반
    REGION,         // 지역 기반
    CUSTOM          // 커스텀 키
};

// [SEQUENCE: MVP14-269] Partition metadata
struct PartitionInfo {
    std::string partition_name;
    uint32_t partition_id;
    
    // Partition boundaries
    std::string min_value;
    std::string max_value;
    
    // Physical location
    std::string database_name;
    std::string table_name;
    std::string server_host;
    uint16_t server_port;
    
    // Statistics
    uint64_t row_count{0};
    uint64_t data_size_bytes{0};
    uint64_t index_size_bytes{0};
    
    // Status
    bool is_active{true};
    bool is_read_only{false};
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point last_accessed;
};

// [SEQUENCE: MVP14-270] Partition scheme definition
struct PartitionScheme {
    std::string scheme_name;
    PartitionStrategy strategy;
    PartitionKeyType key_type;
    
    // Partition columns
    std::vector<std::string> partition_columns;
    
    // Range partitioning
    struct RangePartition {
        std::string start_value;
        std::string end_value;
        uint32_t partition_id;
    };
    std::vector<RangePartition> range_partitions;
    
    // Hash partitioning
    uint32_t hash_partition_count{16};
    std::function<uint32_t(const std::string&)> hash_function;
    
    // List partitioning
    std::unordered_map<std::string, uint32_t> list_values;
    
    // Maintenance settings
    bool auto_create_partitions{true};
    bool auto_drop_old_partitions{false};
    uint32_t retention_days{365};
    
    // Performance settings
    uint32_t max_rows_per_partition{10000000};  // 10M rows
    uint64_t max_size_per_partition{10737418240}; // 10GB
};

// [SEQUENCE: MVP14-271] Partitioned table interface
class PartitionedTable {
public:
    // [SEQUENCE: MVP14-272] Initialize partitioned table
    PartitionedTable(const std::string& table_name, 
                    const PartitionScheme& scheme)
        : table_name_(table_name)
        , scheme_(scheme) {
        
        InitializePartitions();
    }
    
    // [SEQUENCE: MVP14-273] Get partition for key
    PartitionInfo* GetPartition(const std::string& partition_key) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        uint32_t partition_id = CalculatePartitionId(partition_key);
        
        auto it = partitions_.find(partition_id);
        if (it != partitions_.end()) {
            it->second.last_accessed = std::chrono::system_clock::now();
            return &it->second;
        }
        
        // Auto-create partition if enabled
        if (scheme_.auto_create_partitions) {
            return CreatePartition(partition_id, partition_key);
        }
        
        return nullptr;
    }
    
    // [SEQUENCE: MVP14-274] Get all partitions
    std::vector<PartitionInfo> GetAllPartitions() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        std::vector<PartitionInfo> result;
        for (const auto& [id, info] : partitions_) {
            result.push_back(info);
        }
        
        return result;
    }
    
    // [SEQUENCE: MVP14-275] Split partition
    bool SplitPartition(uint32_t partition_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = partitions_.find(partition_id);
        if (it == partitions_.end()) {
            return false;
        }
        
        const auto& original = it->second;
        
        // Check if split is needed
        if (original.row_count < scheme_.max_rows_per_partition &&
            original.data_size_bytes < scheme_.max_size_per_partition) {
            return false;
        }
        
        // Create two new partitions
        auto [new1, new2] = CreateSplitPartitions(original);
        
        // Migrate data
        MigrateDataForSplit(original, new1, new2);
        
        // Update partition map
        partitions_[new1.partition_id] = new1;
        partitions_[new2.partition_id] = new2;
        
        // Mark original as inactive
        it->second.is_active = false;
        
        spdlog::info("Split partition {} into {} and {}", 
                    partition_id, new1.partition_id, new2.partition_id);
        
        return true;
    }
    
    // [SEQUENCE: MVP14-276] Merge partitions
    bool MergePartitions(uint32_t partition1_id, uint32_t partition2_id) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it1 = partitions_.find(partition1_id);
        auto it2 = partitions_.find(partition2_id);
        
        if (it1 == partitions_.end() || it2 == partitions_.end()) {
            return false;
        }
        
        // Check if merge is beneficial
        uint64_t combined_size = it1->second.data_size_bytes + 
                               it2->second.data_size_bytes;
        uint64_t combined_rows = it1->second.row_count + 
                               it2->second.row_count;
        
        if (combined_size > scheme_.max_size_per_partition ||
            combined_rows > scheme_.max_rows_per_partition) {
            return false;
        }
        
        // Create merged partition
        auto merged = CreateMergedPartition(it1->second, it2->second);
        
        // Migrate data
        MigrateDataForMerge(it1->second, it2->second, merged);
        
        // Update partition map
        partitions_[merged.partition_id] = merged;
        
        // Mark originals as inactive
        it1->second.is_active = false;
        it2->second.is_active = false;
        
        spdlog::info("Merged partitions {} and {} into {}", 
                    partition1_id, partition2_id, merged.partition_id);
        
        return true;
    }
    
    // [SEQUENCE: MVP14-277] Maintenance operations
    void RunMaintenance() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::system_clock::now();
        
        // Check for old partitions to drop
        if (scheme_.auto_drop_old_partitions) {
            DropOldPartitions(now);
        }
        
        // Check for partitions needing split
        for (auto& [id, partition] : partitions_) {
            if (partition.is_active && NeedsSplit(partition)) {
                split_queue_.push_back(id);
            }
        }
        
        // Check for partitions that can be merged
        CheckMergeCandidates();
        
        // Update statistics
        UpdatePartitionStatistics();
    }
    
    // [SEQUENCE: MVP14-278] Get partition statistics
    struct PartitionStatistics {
        uint32_t total_partitions{0};
        uint32_t active_partitions{0};
        uint64_t total_rows{0};
        uint64_t total_data_size{0};
        uint64_t total_index_size{0};
        
        // Distribution metrics
        double avg_rows_per_partition{0};
        double std_dev_rows{0};
        uint32_t empty_partitions{0};
        uint32_t hot_partitions{0};  // >80% of max size
        
        // Performance metrics
        double avg_query_time_ms{0};
        uint64_t cache_hit_rate{0};
    };
    
    PartitionStatistics GetStatistics() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        PartitionStatistics stats;
        
        for (const auto& [id, partition] : partitions_) {
            stats.total_partitions++;
            
            if (partition.is_active) {
                stats.active_partitions++;
                stats.total_rows += partition.row_count;
                stats.total_data_size += partition.data_size_bytes;
                stats.total_index_size += partition.index_size_bytes;
                
                if (partition.row_count == 0) {
                    stats.empty_partitions++;
                }
                
                if (partition.data_size_bytes > 
                    scheme_.max_size_per_partition * 0.8) {
                    stats.hot_partitions++;
                }
            }
        }
        
        if (stats.active_partitions > 0) {
            stats.avg_rows_per_partition = 
                static_cast<double>(stats.total_rows) / stats.active_partitions;
        }
        
        return stats;
    }
    
private:
    std::string table_name_;
    PartitionScheme scheme_;
    std::unordered_map<uint32_t, PartitionInfo> partitions_;
    
    // Maintenance queues
    std::vector<uint32_t> split_queue_;
    std::vector<std::pair<uint32_t, uint32_t>> merge_queue_;
    
    mutable std::mutex mutex_;
    std::atomic<uint32_t> next_partition_id_{1000};
    
    // [SEQUENCE: MVP14-279] Calculate partition ID for key
    uint32_t CalculatePartitionId(const std::string& partition_key) {
        switch (scheme_.strategy) {
            case PartitionStrategy::HASH:
                return scheme_.hash_function(partition_key) % 
                       scheme_.hash_partition_count;
                
            case PartitionStrategy::RANGE:
                return GetRangePartitionId(partition_key);
                
            case PartitionStrategy::LIST:
                return GetListPartitionId(partition_key);
                
            case PartitionStrategy::ROUND_ROBIN:
                return GetNextRoundRobinPartition();
                
            default:
                return 0;
        }
    }
    
    // [SEQUENCE: MVP14-280] Initialize partitions
    void InitializePartitions() {
        switch (scheme_.strategy) {
            case PartitionStrategy::HASH:
                InitializeHashPartitions();
                break;
                
            case PartitionStrategy::RANGE:
                InitializeRangePartitions();
                break;
                
            case PartitionStrategy::LIST:
                InitializeListPartitions();
                break;
        }
    }
    
    void InitializeHashPartitions() {
        for (uint32_t i = 0; i < scheme_.hash_partition_count; ++i) {
            PartitionInfo partition;
            partition.partition_name = table_name_ + "_p" + std::to_string(i);
            partition.partition_id = i;
            partition.database_name = "shard_" + std::to_string(i % 4);
            partition.table_name = partition.partition_name;
            partition.created_at = std::chrono::system_clock::now();
            
            partitions_[i] = partition;
        }
    }
    
    void InitializeRangePartitions() {
        for (const auto& range : scheme_.range_partitions) {
            PartitionInfo partition;
            partition.partition_name = table_name_ + "_p" + 
                                     std::to_string(range.partition_id);
            partition.partition_id = range.partition_id;
            partition.min_value = range.start_value;
            partition.max_value = range.end_value;
            partition.created_at = std::chrono::system_clock::now();
            
            partitions_[range.partition_id] = partition;
        }
    }
    
    void InitializeListPartitions() {
        std::unordered_map<uint32_t, std::vector<std::string>> partition_values;
        
        for (const auto& [value, partition_id] : scheme_.list_values) {
            partition_values[partition_id].push_back(value);
        }
        
        for (const auto& [partition_id, values] : partition_values) {
            PartitionInfo partition;
            partition.partition_name = table_name_ + "_p" + 
                                     std::to_string(partition_id);
            partition.partition_id = partition_id;
            partition.created_at = std::chrono::system_clock::now();
            
            partitions_[partition_id] = partition;
        }
    }
    
    // Helper methods
    uint32_t GetRangePartitionId(const std::string& key) {
        for (const auto& range : scheme_.range_partitions) {
            if (key >= range.start_value && key < range.end_value) {
                return range.partition_id;
            }
        }
        return 0;
    }
    
    uint32_t GetListPartitionId(const std::string& key) {
        auto it = scheme_.list_values.find(key);
        if (it != scheme_.list_values.end()) {
            return it->second;
        }
        return 0;
    }
    
    uint32_t GetNextRoundRobinPartition() {
        static std::atomic<uint32_t> counter{0};
        return counter++ % partitions_.size();
    }
    
    PartitionInfo* CreatePartition(uint32_t partition_id, 
                                 const std::string& partition_key) {
        PartitionInfo partition;
        partition.partition_name = table_name_ + "_p" + 
                                 std::to_string(partition_id);
        partition.partition_id = partition_id;
        partition.created_at = std::chrono::system_clock::now();
        
        partitions_[partition_id] = partition;
        return &partitions_[partition_id];
    }
    
    bool NeedsSplit(const PartitionInfo& partition) {
        return partition.row_count > scheme_.max_rows_per_partition ||
               partition.data_size_bytes > scheme_.max_size_per_partition;
    }
    
    std::pair<PartitionInfo, PartitionInfo> CreateSplitPartitions(
        const PartitionInfo& original) {
        PartitionInfo new1 = original;
        new1.partition_id = next_partition_id_++;
        new1.partition_name = table_name_ + "_p" + 
                            std::to_string(new1.partition_id);
        new1.row_count = original.row_count / 2;
        new1.data_size_bytes = original.data_size_bytes / 2;
        
        PartitionInfo new2 = original;
        new2.partition_id = next_partition_id_++;
        new2.partition_name = table_name_ + "_p" + 
                            std::to_string(new2.partition_id);
        new2.row_count = original.row_count / 2;
        new2.data_size_bytes = original.data_size_bytes / 2;
        
        return {new1, new2};
    }
    
    PartitionInfo CreateMergedPartition(const PartitionInfo& p1, 
                                      const PartitionInfo& p2) {
        PartitionInfo merged;
        merged.partition_id = next_partition_id_++;
        merged.partition_name = table_name_ + "_p" + 
                              std::to_string(merged.partition_id);
        merged.row_count = p1.row_count + p2.row_count;
        merged.data_size_bytes = p1.data_size_bytes + p2.data_size_bytes;
        merged.index_size_bytes = p1.index_size_bytes + p2.index_size_bytes;
        merged.created_at = std::chrono::system_clock::now();
        
        return merged;
    }
    
    void MigrateDataForSplit(const PartitionInfo& original,
                           const PartitionInfo& new1,
                           const PartitionInfo& new2) {
        // Implementation would execute actual data migration
        spdlog::info("Migrating data from {} to {} and {}", 
                    original.partition_name, 
                    new1.partition_name, 
                    new2.partition_name);
    }
    
    void MigrateDataForMerge(const PartitionInfo& p1,
                           const PartitionInfo& p2,
                           const PartitionInfo& merged) {
        // Implementation would execute actual data migration
        spdlog::info("Migrating data from {} and {} to {}", 
                    p1.partition_name, 
                    p2.partition_name,
                    merged.partition_name);
    }
    
    void DropOldPartitions(std::chrono::system_clock::time_point now) {
        auto cutoff = now - std::chrono::hours(24 * scheme_.retention_days);
        
        for (auto& [id, partition] : partitions_) {
            if (!partition.is_active && partition.created_at < cutoff) {
                // Drop partition
                spdlog::info("Dropping old partition: {}", 
                            partition.partition_name);
                partitions_.erase(id);
            }
        }
    }
    
    void CheckMergeCandidates() {
        // Find small partitions that can be merged
        std::vector<uint32_t> small_partitions;
        
        for (const auto& [id, partition] : partitions_) {
            if (partition.is_active && 
                partition.data_size_bytes < scheme_.max_size_per_partition * 0.2) {
                small_partitions.push_back(id);
            }
        }
        
        // Pair up small partitions for merging
        for (size_t i = 0; i + 1 < small_partitions.size(); i += 2) {
            merge_queue_.push_back({small_partitions[i], small_partitions[i + 1]});
        }
    }
    
    void UpdatePartitionStatistics() {
        // In real implementation, query actual statistics from database
        for (auto& [id, partition] : partitions_) {
            if (partition.is_active) {
                // Update row count, data size, etc.
            }
        }
    }
};

// [SEQUENCE: MVP14-281] Partition manager for multiple tables
class PartitionManager {
public:
    // [SEQUENCE: MVP14-282] Register partitioned table
    void RegisterTable(const std::string& table_name,
                      const PartitionScheme& scheme) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto table = std::make_unique<PartitionedTable>(table_name, scheme);
        tables_[table_name] = std::move(table);
        
        spdlog::info("Registered partitioned table: {} with {} strategy", 
                    table_name, GetStrategyName(scheme.strategy));
    }
    
    // [SEQUENCE: MVP14-283] Get partitioned table
    PartitionedTable* GetTable(const std::string& table_name) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        auto it = tables_.find(table_name);
        if (it != tables_.end()) {
            return it->second.get();
        }
        
        return nullptr;
    }
    
    // [SEQUENCE: MVP14-284] Run maintenance on all tables
    void RunGlobalMaintenance() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        spdlog::info("Running global partition maintenance");
        
        for (auto& [name, table] : tables_) {
            table->RunMaintenance();
        }
        
        // Process split queue
        ProcessSplitQueue();
        
        // Process merge queue
        ProcessMergeQueue();
        
        // Update global statistics
        UpdateGlobalStatistics();
    }
    
    // [SEQUENCE: MVP14-285] Get global statistics
    struct GlobalStatistics {
        uint32_t total_tables{0};
        uint32_t total_partitions{0};
        uint64_t total_data_size{0};
        
        std::unordered_map<std::string, PartitionedTable::PartitionStatistics> 
            table_stats;
    };
    
    GlobalStatistics GetGlobalStatistics() const {
        std::lock_guard<std::mutex> lock(mutex_);
        
        GlobalStatistics stats;
        stats.total_tables = tables_.size();
        
        for (const auto& [name, table] : tables_) {
            auto table_stats = table->GetStatistics();
            stats.table_stats[name] = table_stats;
            stats.total_partitions += table_stats.total_partitions;
            stats.total_data_size += table_stats.total_data_size;
        }
        
        return stats;
    }
    
private:
    std::unordered_map<std::string, std::unique_ptr<PartitionedTable>> tables_;
    mutable std::mutex mutex_;
    
    void ProcessSplitQueue() {
        // Process pending splits
    }
    
    void ProcessMergeQueue() {
        // Process pending merges
    }
    
    void UpdateGlobalStatistics() {
        // Update cross-table statistics
    }
    
    std::string GetStrategyName(PartitionStrategy strategy) {
        switch (strategy) {
            case PartitionStrategy::RANGE: return "RANGE";
            case PartitionStrategy::HASH: return "HASH";
            case PartitionStrategy::LIST: return "LIST";
            case PartitionStrategy::COMPOSITE: return "COMPOSITE";
            case PartitionStrategy::ROUND_ROBIN: return "ROUND_ROBIN";
            default: return "UNKNOWN";
        }
    }
};

// [SEQUENCE: MVP14-286] Common partition schemes
class CommonPartitionSchemes {
public:
    // [SEQUENCE: MVP14-287] Time-based partitioning (logs, events)
    static PartitionScheme CreateTimeBasedScheme(const std::string& table_name,
                                                uint32_t days_per_partition = 30) {
        PartitionScheme scheme;
        scheme.scheme_name = table_name + "_time_based";
        scheme.strategy = PartitionStrategy::RANGE;
        scheme.key_type = PartitionKeyType::TIMESTAMP;
        scheme.partition_columns = {"created_at"};
        scheme.auto_create_partitions = true;
        scheme.auto_drop_old_partitions = true;
        scheme.retention_days = 365;
        
        // Create initial partitions for next 3 months
        auto now = std::chrono::system_clock::now();
        for (int i = 0; i < 90; i += days_per_partition) {
            auto start = now + std::chrono::hours(24 * i);
            auto end = start + std::chrono::hours(24 * days_per_partition);
            
            PartitionScheme::RangePartition range;
            range.start_value = FormatTimestamp(start);
            range.end_value = FormatTimestamp(end);
            range.partition_id = i / days_per_partition;
            
            scheme.range_partitions.push_back(range);
        }
        
        return scheme;
    }
    
    // [SEQUENCE: MVP14-288] Player-based partitioning
    static PartitionScheme CreatePlayerBasedScheme(const std::string& table_name,
                                                  uint32_t partition_count = 16) {
        PartitionScheme scheme;
        scheme.scheme_name = table_name + "_player_based";
        scheme.strategy = PartitionStrategy::HASH;
        scheme.key_type = PartitionKeyType::PLAYER_ID;
        scheme.partition_columns = {"player_id"};
        scheme.hash_partition_count = partition_count;
        scheme.hash_function = [](const std::string& key) {
            return std::hash<std::string>{}(key);
        };
        scheme.max_rows_per_partition = 5000000;  // 5M per partition
        
        return scheme;
    }
    
    // [SEQUENCE: MVP14-289] Region-based partitioning
    static PartitionScheme CreateRegionBasedScheme(const std::string& table_name) {
        PartitionScheme scheme;
        scheme.scheme_name = table_name + "_region_based";
        scheme.strategy = PartitionStrategy::LIST;
        scheme.key_type = PartitionKeyType::REGION;
        scheme.partition_columns = {"region"};
        
        // Define region mappings
        scheme.list_values = {
            {"NA_EAST", 0},
            {"NA_WEST", 1},
            {"EU_WEST", 2},
            {"EU_EAST", 3},
            {"ASIA_PACIFIC", 4},
            {"SOUTH_AMERICA", 5},
            {"OCEANIA", 6}
        };
        
        return scheme;
    }
    
private:
    static std::string FormatTimestamp(std::chrono::system_clock::time_point time) {
        auto time_t = std::chrono::system_clock::to_time_t(time);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d");
        return ss.str();
    }
};

// [SEQUENCE: MVP14-290] Partition query info for routing
struct PartitionQueryInfo {
    std::string table_name;
    std::string partition_key;
    
    bool success{false};
    std::string error_message;
    
    // Connection info
    std::string database_name;
    std::string actual_table_name;
    std::string server_endpoint;
    uint32_t partition_id{0};
    bool is_read_only{false};
};

// [SEQUENCE: MVP14-291] Partition health report
struct PartitionHealthReport {
    std::chrono::system_clock::time_point timestamp;
    bool healthy{true};
    
    uint32_t total_tables{0};
    uint32_t total_partitions{0};
    
    std::vector<std::string> issues;
    std::vector<std::string> tables_needing_attention;
    std::vector<std::string> tables_needing_rebalance;
};

// [SEQUENCE: MVP14-292] Public API functions
void InitializePartitionManager();
void CleanupPartitionManager();
PartitionManager* GetPartitionManager();

void RegisterCommonPartitions();
void StartPartitionMaintenance();
void StopPartitionMaintenance();

bool ExecutePartitionSplit(const std::string& table_name, uint32_t partition_id);
bool ExecutePartitionMerge(const std::string& table_name, 
                         uint32_t partition1_id, 
                         uint32_t partition2_id);

PartitionQueryInfo GetPartitionForQuery(const std::string& table_name,
                                      const std::string& partition_key);

PartitionHealthReport MonitorPartitionHealth();
bool RebalancePartitions(const std::string& table_name);
std::string GeneratePartitionReport();

} // namespace mmorpg::database
