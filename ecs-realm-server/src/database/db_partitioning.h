#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace mmorpg::database {

// [SEQUENCE: MVP7-24] Defines the partitioning strategy for a table.
enum class PartitionStrategy {
    NONE,
    BY_RANGE,
    BY_HASH,
    BY_LIST
};

// [SEQUENCE: MVP7-25] Holds metadata about a partitioned table.
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
    // In a real implementation, this would hold range boundaries, hash info, etc.
};

// [SEQUENCE: MVP7-26] Manages all partitioned tables.
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
