#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace mmorpg::database {

// [SEQUENCE: MVP7-38] Defines the partitioning strategy for a table (e.g., by range or hash).
// This helps in managing large tables by splitting them into smaller, more manageable pieces.
enum class PartitionStrategy {
    NONE,
    BY_RANGE,
    BY_HASH,
    BY_LIST
};

// [SEQUENCE: MVP7-39] Holds metadata about a partitioned table, such as its name and key column.
class PartitionedTable {
public:
    PartitionedTable(std::string name, PartitionStrategy strategy, std::string key_column)
        : m_table_name(std::move(name)), m_strategy(strategy), m_key_column(std::move(key_column)) {}

    const std::string& GetTableName() const { return m_table_name; }
    std::string GetPartitionForValue(int64_t value);

private:
    std::string m_table_name;
    PartitionStrategy m_strategy;
    std::string m_key_column;
};

// [SEQUENCE: MVP7-40] A singleton manager for all partitioned tables in the database.
class PartitionManager {
public:
    static PartitionManager& Instance();

    void RegisterTable(const std::string& name, PartitionStrategy strategy, const std::string& key_column);
    std::shared_ptr<PartitionedTable> GetTable(const std::string& name);

private:
    PartitionManager() = default;
    ~PartitionManager() = default;
    PartitionManager(const PartitionManager&) = delete;
    PartitionManager& operator=(const PartitionManager&) = delete;

    std::unordered_map<std::string, std::shared_ptr<PartitionedTable>> m_tables;
};

}