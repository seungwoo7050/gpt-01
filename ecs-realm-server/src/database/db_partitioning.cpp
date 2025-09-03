#include "db_partitioning.h"
#include "../core/logger.h"
#include <fmt/format.h>
#include <spdlog/spdlog.h>
#include <iomanip>
#include <sstream>
#include <algorithm>

namespace mmorpg::database {

// [SEQUENCE: 2960] Partition manager singleton
static PartitionManager* g_partition_manager = nullptr;

// [SEQUENCE: 2961] Initialize global partition manager
void InitializePartitionManager() {
    if (!g_partition_manager) {
        g_partition_manager = new PartitionManager();
        
        // Register common partitioned tables
        RegisterCommonPartitions();
        
        spdlog::info("[DB_PARTITION] Initialized partition manager");
    }
}

// [SEQUENCE: 2962] Register common partitioned tables
void RegisterCommonPartitions() {
    auto* manager = GetPartitionManager();
    if (!manager) return;
    
    // Player inventory (플레이어 인벤토리)
    manager->RegisterTable("player_inventory", 
        CommonPartitionSchemes::CreatePlayerBasedScheme("player_inventory", 32));
    
    // Combat logs (전투 로그 - 시간 기반)
    manager->RegisterTable("combat_logs", 
        CommonPartitionSchemes::CreateTimeBasedScheme("combat_logs", 7));
    
    // Transaction history (거래 내역)
    manager->RegisterTable("transaction_history", 
        CommonPartitionSchemes::CreateTimeBasedScheme("transaction_history", 30));
    
    // Player statistics (플레이어 통계)
    manager->RegisterTable("player_stats", 
        CommonPartitionSchemes::CreatePlayerBasedScheme("player_stats", 16));
    
    // Guild data by region (길드 데이터 - 지역 기반)
    manager->RegisterTable("guild_data", 
        CommonPartitionSchemes::CreateRegionBasedScheme("guild_data"));
    
    // Event logs (이벤트 로그)
    manager->RegisterTable("event_logs", 
        CommonPartitionSchemes::CreateTimeBasedScheme("event_logs", 1));
    
    // Chat history (채팅 기록)
    manager->RegisterTable("chat_history", 
        CommonPartitionSchemes::CreateTimeBasedScheme("chat_history", 7));
    
    // Auction house (경매장)
    manager->RegisterTable("auction_listings", 
        CommonPartitionSchemes::CreateHashBasedScheme("auction_listings", 8));
    
    spdlog::info("[DB_PARTITION] Registered {} common partitioned tables", 8);
}

// [SEQUENCE: 2963] Get partition manager instance
PartitionManager* GetPartitionManager() {
    return g_partition_manager;
}

// [SEQUENCE: 2964] Cleanup partition manager
void CleanupPartitionManager() {
    if (g_partition_manager) {
        delete g_partition_manager;
        g_partition_manager = nullptr;
        spdlog::info("[DB_PARTITION] Cleaned up partition manager");
    }
}

// [SEQUENCE: 2965] Hash-based partition scheme helper
PartitionScheme CommonPartitionSchemes::CreateHashBasedScheme(
    const std::string& table_name, 
    uint32_t partition_count) {
    
    PartitionScheme scheme;
    scheme.scheme_name = table_name + "_hash_based";
    scheme.strategy = PartitionStrategy::HASH;
    scheme.key_type = PartitionKeyType::CUSTOM;
    scheme.partition_columns = {"id"};  // Default to ID column
    scheme.hash_partition_count = partition_count;
    scheme.hash_function = [](const std::string& key) {
        return std::hash<std::string>{}(key);
    };
    scheme.max_rows_per_partition = 10000000;  // 10M per partition
    scheme.auto_create_partitions = false;  // Hash partitions are pre-created
    
    return scheme;
}

// [SEQUENCE: 2966] Maintenance worker thread
class PartitionMaintenanceWorker {
public:
    PartitionMaintenanceWorker() : running_(false) {}
    
    void Start() {
        running_ = true;
        thread_ = std::thread([this] {
            MaintenanceLoop();
        });
        spdlog::info("[DB_PARTITION] Started maintenance worker");
    }
    
    void Stop() {
        running_ = false;
        if (thread_.joinable()) {
            thread_.join();
        }
        spdlog::info("[DB_PARTITION] Stopped maintenance worker");
    }
    
private:
    void MaintenanceLoop() {
        while (running_) {
            try {
                // Run maintenance every hour
                std::this_thread::sleep_for(std::chrono::hours(1));
                
                if (!running_) break;
                
                auto* manager = GetPartitionManager();
                if (manager) {
                    manager->RunGlobalMaintenance();
                    
                    // Log statistics
                    auto stats = manager->GetGlobalStatistics();
                    spdlog::info("[DB_PARTITION] Global stats: {} tables, {} partitions, {} GB total",
                        stats.total_tables,
                        stats.total_partitions,
                        stats.total_data_size / (1024.0 * 1024.0 * 1024.0));
                }
            } catch (const std::exception& e) {
                spdlog::error("[DB_PARTITION] Maintenance error: {}", e.what());
            }
        }
    }
    
    std::atomic<bool> running_;
    std::thread thread_;
};

static std::unique_ptr<PartitionMaintenanceWorker> g_maintenance_worker;

// [SEQUENCE: 2967] Start partition maintenance
void StartPartitionMaintenance() {
    if (!g_maintenance_worker) {
        g_maintenance_worker = std::make_unique<PartitionMaintenanceWorker>();
        g_maintenance_worker->Start();
    }
}

// [SEQUENCE: 2968] Stop partition maintenance
void StopPartitionMaintenance() {
    if (g_maintenance_worker) {
        g_maintenance_worker->Stop();
        g_maintenance_worker.reset();
    }
}

// [SEQUENCE: 2969] Execute partition split operation
bool ExecutePartitionSplit(const std::string& table_name, 
                          uint32_t partition_id) {
    auto* manager = GetPartitionManager();
    if (!manager) return false;
    
    auto* table = manager->GetTable(table_name);
    if (!table) {
        spdlog::error("[DB_PARTITION] Table {} not found", table_name);
        return false;
    }
    
    spdlog::info("[DB_PARTITION] Executing split for {}, partition {}", 
                table_name, partition_id);
    
    return table->SplitPartition(partition_id);
}

// [SEQUENCE: 2970] Execute partition merge operation
bool ExecutePartitionMerge(const std::string& table_name,
                         uint32_t partition1_id,
                         uint32_t partition2_id) {
    auto* manager = GetPartitionManager();
    if (!manager) return false;
    
    auto* table = manager->GetTable(table_name);
    if (!table) {
        spdlog::error("[DB_PARTITION] Table {} not found", table_name);
        return false;
    }
    
    spdlog::info("[DB_PARTITION] Executing merge for {}, partitions {} and {}", 
                table_name, partition1_id, partition2_id);
    
    return table->MergePartitions(partition1_id, partition2_id);
}

// [SEQUENCE: 2971] Get partition info for query
PartitionQueryInfo GetPartitionForQuery(const std::string& table_name,
                                      const std::string& partition_key) {
    PartitionQueryInfo info;
    info.table_name = table_name;
    info.partition_key = partition_key;
    
    auto* manager = GetPartitionManager();
    if (!manager) {
        info.success = false;
        info.error_message = "Partition manager not initialized";
        return info;
    }
    
    auto* table = manager->GetTable(table_name);
    if (!table) {
        info.success = false;
        info.error_message = fmt::format("Table {} not partitioned", table_name);
        return info;
    }
    
    auto* partition = table->GetPartition(partition_key);
    if (!partition) {
        info.success = false;
        info.error_message = fmt::format("No partition found for key: {}", partition_key);
        return info;
    }
    
    // Build connection info
    info.success = true;
    info.database_name = partition->database_name;
    info.actual_table_name = partition->table_name;
    info.server_endpoint = fmt::format("{}:{}", 
                                     partition->server_host, 
                                     partition->server_port);
    info.partition_id = partition->partition_id;
    info.is_read_only = partition->is_read_only;
    
    return info;
}

// [SEQUENCE: 2972] Monitor partition health
PartitionHealthReport MonitorPartitionHealth() {
    PartitionHealthReport report;
    report.timestamp = std::chrono::system_clock::now();
    
    auto* manager = GetPartitionManager();
    if (!manager) {
        report.healthy = false;
        report.issues.push_back("Partition manager not initialized");
        return report;
    }
    
    auto stats = manager->GetGlobalStatistics();
    report.total_tables = stats.total_tables;
    report.total_partitions = stats.total_partitions;
    
    // Check each table's health
    for (const auto& [table_name, table_stats] : stats.table_stats) {
        // Check for hot partitions
        if (table_stats.hot_partitions > 0) {
            report.issues.push_back(
                fmt::format("Table {} has {} hot partitions (>80% capacity)",
                          table_name, table_stats.hot_partitions));
            report.tables_needing_attention.push_back(table_name);
        }
        
        // Check for empty partitions
        if (table_stats.empty_partitions > table_stats.total_partitions * 0.5) {
            report.issues.push_back(
                fmt::format("Table {} has {}% empty partitions",
                          table_name, 
                          (table_stats.empty_partitions * 100) / table_stats.total_partitions));
        }
        
        // Check for uneven distribution
        if (table_stats.std_dev_rows > table_stats.avg_rows_per_partition * 0.5) {
            report.issues.push_back(
                fmt::format("Table {} has uneven data distribution (std dev: {:.0f})",
                          table_name, table_stats.std_dev_rows));
            report.tables_needing_rebalance.push_back(table_name);
        }
    }
    
    report.healthy = report.issues.empty();
    return report;
}

// [SEQUENCE: 2973] Rebalance partitions
bool RebalancePartitions(const std::string& table_name) {
    auto* manager = GetPartitionManager();
    if (!manager) return false;
    
    auto* table = manager->GetTable(table_name);
    if (!table) return false;
    
    spdlog::info("[DB_PARTITION] Starting rebalance for table: {}", table_name);
    
    auto partitions = table->GetAllPartitions();
    auto stats = table->GetStatistics();
    
    // Calculate target size per partition
    uint64_t target_rows = stats.total_rows / stats.active_partitions;
    uint64_t tolerance = target_rows * 0.2;  // 20% tolerance
    
    std::vector<uint32_t> oversized_partitions;
    std::vector<uint32_t> undersized_partitions;
    
    for (const auto& partition : partitions) {
        if (!partition.is_active) continue;
        
        if (partition.row_count > target_rows + tolerance) {
            oversized_partitions.push_back(partition.partition_id);
        } else if (partition.row_count < target_rows - tolerance) {
            undersized_partitions.push_back(partition.partition_id);
        }
    }
    
    // Execute splits for oversized partitions
    for (uint32_t partition_id : oversized_partitions) {
        if (!table->SplitPartition(partition_id)) {
            spdlog::error("[DB_PARTITION] Failed to split partition {}", partition_id);
            return false;
        }
    }
    
    // Execute merges for undersized partitions
    for (size_t i = 0; i + 1 < undersized_partitions.size(); i += 2) {
        if (!table->MergePartitions(undersized_partitions[i], 
                                  undersized_partitions[i + 1])) {
            spdlog::error("[DB_PARTITION] Failed to merge partitions {} and {}",
                        undersized_partitions[i], undersized_partitions[i + 1]);
        }
    }
    
    spdlog::info("[DB_PARTITION] Rebalance complete. Splits: {}, Merges: {}",
                oversized_partitions.size(), 
                undersized_partitions.size() / 2);
    
    return true;
}

// [SEQUENCE: 2974] Generate partition report
std::string GeneratePartitionReport() {
    std::stringstream report;
    
    report << "=== Database Partition Report ===\n";
    report << "Generated: " << std::put_time(
        std::localtime(&std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now())), "%Y-%m-%d %H:%M:%S") << "\n\n";
    
    auto* manager = GetPartitionManager();
    if (!manager) {
        report << "ERROR: Partition manager not initialized\n";
        return report.str();
    }
    
    auto stats = manager->GetGlobalStatistics();
    
    report << "Global Statistics:\n";
    report << fmt::format("  Total Tables: {}\n", stats.total_tables);
    report << fmt::format("  Total Partitions: {}\n", stats.total_partitions);
    report << fmt::format("  Total Data Size: {:.2f} GB\n", 
                        stats.total_data_size / (1024.0 * 1024.0 * 1024.0));
    report << "\n";
    
    // Per-table statistics
    report << "Table Statistics:\n";
    for (const auto& [table_name, table_stats] : stats.table_stats) {
        report << fmt::format("\n  Table: {}\n", table_name);
        report << fmt::format("    Partitions: {} (active: {})\n", 
                            table_stats.total_partitions, 
                            table_stats.active_partitions);
        report << fmt::format("    Total Rows: {}\n", table_stats.total_rows);
        report << fmt::format("    Avg Rows/Partition: {:.0f}\n", 
                            table_stats.avg_rows_per_partition);
        report << fmt::format("    Data Size: {:.2f} GB\n", 
                            table_stats.total_data_size / (1024.0 * 1024.0 * 1024.0));
        report << fmt::format("    Hot Partitions: {}\n", table_stats.hot_partitions);
        report << fmt::format("    Empty Partitions: {}\n", table_stats.empty_partitions);
        
        if (table_stats.avg_query_time_ms > 0) {
            report << fmt::format("    Avg Query Time: {:.2f} ms\n", 
                                table_stats.avg_query_time_ms);
        }
    }
    
    // Health check
    auto health = MonitorPartitionHealth();
    report << "\nHealth Status: " << (health.healthy ? "HEALTHY" : "ISSUES DETECTED") << "\n";
    
    if (!health.issues.empty()) {
        report << "\nIssues Detected:\n";
        for (const auto& issue : health.issues) {
            report << "  - " << issue << "\n";
        }
    }
    
    if (!health.tables_needing_attention.empty()) {
        report << "\nTables Needing Attention:\n";
        for (const auto& table : health.tables_needing_attention) {
            report << "  - " << table << "\n";
        }
    }
    
    if (!health.tables_needing_rebalance.empty()) {
        report << "\nTables Needing Rebalance:\n";
        for (const auto& table : health.tables_needing_rebalance) {
            report << "  - " << table << "\n";
        }
    }
    
    return report.str();
}

} // namespace mmorpg::database