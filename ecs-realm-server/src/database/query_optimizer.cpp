#include "database/query_optimizer.h"

namespace mmorpg::database {

// [SEQUENCE: MVP7-35] Implements the singleton and query registration/retrieval methods.
QueryOptimizer& QueryOptimizer::Instance() {
    static QueryOptimizer instance;
    return instance;
}

void QueryOptimizer::RegisterQuery(const std::string& name, const std::string& sql) {
    m_queries[name] = sql;
}

std::string QueryOptimizer::GetQuery(const std::string& name) const {
    auto it = m_queries.find(name);
    if (it != m_queries.end()) {
        return it->second;
    }
    return ""; // Or throw an exception
}

}