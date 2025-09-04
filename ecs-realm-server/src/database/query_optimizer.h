#pragma once

#include <string>
#include <unordered_map>
#include <memory>

namespace mmorpg::database {

// [SEQUENCE: MVP7-30] A simple query optimizer that stores and retrieves pre-written queries.
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
