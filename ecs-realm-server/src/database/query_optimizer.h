#pragma once

#include <string>
#include <unordered_map>
#include <memory>

namespace mmorpg::database {

// [SEQUENCE: MVP7-34] A singleton manager to store and retrieve pre-written, optimized SQL queries.
// This avoids dynamic SQL string generation and allows for easier management of queries.
class QueryOptimizer {
public:
    static QueryOptimizer& Instance();

    void RegisterQuery(const std::string& name, const std::string& sql);
    std::string GetQuery(const std::string& name) const;

private:
    QueryOptimizer() = default;
    ~QueryOptimizer() = default;
    QueryOptimizer(const QueryOptimizer&) = delete;
    QueryOptimizer& operator=(const QueryOptimizer&) = delete;

    std::unordered_map<std::string, std::string> m_queries;
};

}