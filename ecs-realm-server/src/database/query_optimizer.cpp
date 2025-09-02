#include "query_optimizer.h"
#include "../core/logger.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <regex>
#include <sstream>
#include <numeric>

namespace mmorpg::database {

// [SEQUENCE: MVP14-451] Query pattern analysis
QueryPatternAnalyzer::QueryPattern QueryPatternAnalyzer::AnalyzeQuery(
    const std::string& query) {
    
    QueryPattern pattern;
    
    // Convert to uppercase for analysis
    std::string upper_query = query;
    std::transform(upper_query.begin(), upper_query.end(), 
                  upper_query.begin(), ::toupper);
    
    // Determine query type
    if (upper_query.find("SELECT") == 0) {
        pattern.type = QueryPattern::SIMPLE_SELECT;
        
        // Check for joins
        if (upper_query.find("JOIN") != std::string::npos) {
            pattern.type = QueryPattern::JOIN_QUERY;
            pattern.has_join = true;
        }
        
        // Check for aggregations
        if (upper_query.find("COUNT(") != std::string::npos ||
            upper_query.find("SUM(") != std::string::npos ||
            upper_query.find("AVG(") != std::string::npos ||
            upper_query.find("MAX(") != std::string::npos ||
            upper_query.find("MIN(") != std::string::npos) {
            pattern.type = QueryPattern::AGGREGATE;
            pattern.has_aggregation = true;
        }
        
        // Check for subqueries
        if (std::count(upper_query.begin(), upper_query.end(), '(') > 
            (pattern.has_aggregation ? 1 : 0)) {
            pattern.type = QueryPattern::SUBQUERY;
            pattern.has_subquery = true;
        }
    } else if (upper_query.find("UPDATE") == 0) {
        pattern.type = QueryPattern::UPDATE_QUERY;
    } else if (upper_query.find("INSERT") == 0) {
        pattern.type = QueryPattern::INSERT_QUERY;
    } else if (upper_query.find("DELETE") == 0) {
        pattern.type = QueryPattern::DELETE_QUERY;
    }
    
    // Extract tables (simplified)
    std::regex table_regex(R"(FROM\s+(\w+))", std::regex::icase);
    std::smatch match;
    if (std::regex_search(query, match, table_regex)) {
        pattern.tables.push_back(match[1]);
    }
    
    // Check for ORDER BY
    pattern.has_order_by = (upper_query.find("ORDER BY") != std::string::npos);
    
    // Check for GROUP BY
    pattern.has_group_by = (upper_query.find("GROUP BY") != std::string::npos);
    
    // Extract LIMIT
    std::regex limit_regex(R"(LIMIT\s+(\d+))", std::regex::icase);
    if (std::regex_search(query, match, limit_regex)) {
        pattern.limit = std::stoi(match[1]);
    }
    
    return pattern;
}

// [SEQUENCE: MVP14-452] Suggest query optimizations
std::vector<std::string> QueryPatternAnalyzer::SuggestOptimizations(
    const QueryPattern& pattern) {
    
    std::vector<std::string> suggestions;
    
    // JOIN optimization
    if (pattern.has_join) {
        suggestions.push_back("Consider using STRAIGHT_JOIN if join order is important");
        suggestions.push_back("Ensure join columns are indexed");
    }
    
    // Subquery optimization
    if (pattern.has_subquery) {
        suggestions.push_back("Consider rewriting subquery as JOIN");
        suggestions.push_back("Use EXISTS instead of IN for better performance");
    }
    
    // ORDER BY optimization
    if (pattern.has_order_by && !pattern.limit) {
        suggestions.push_back("Add LIMIT to ORDER BY queries when possible");
    }
    
    // Aggregation optimization
    if (pattern.has_aggregation) {
        suggestions.push_back("Ensure GROUP BY columns are indexed");
        suggestions.push_back("Consider using covering indexes");
    }
    
    // General suggestions
    if (pattern.type == QueryPattern::SIMPLE_SELECT) {
        suggestions.push_back("Select only required columns instead of SELECT *");
    }
    
    return suggestions;
}

// [SEQUENCE: MVP14-453] Record query execution for index analysis
void IndexAdvisor::RecordQueryExecution(const std::string& query,
                                      const QueryPlan& plan,
                                      double execution_time_ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    for (const auto& table : plan.tables_accessed) {
        auto& pattern = table_patterns_[table];
        
        // Update access patterns
        if (plan.indexes_used.empty()) {
            pattern.full_scan_count++;
        }
        
        pattern.avg_rows_examined = 
            (pattern.avg_rows_examined * pattern.full_scan_count + plan.actual_rows) /
            (pattern.full_scan_count + 1);
        
        pattern.avg_execution_time_ms = 
            (pattern.avg_execution_time_ms * pattern.full_scan_count + execution_time_ms) /
            (pattern.full_scan_count + 1);
    }
    
    // Update index usage
    for (const auto& index : plan.indexes_used) {
        index_last_used_[index] = std::chrono::system_clock::now();
    }
}

// [SEQUENCE: MVP14-454] Get index recommendations
std::vector<IndexAdvisor::IndexRecommendation> IndexAdvisor::GetRecommendations(
    const std::string& table_name) const {
    
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<IndexRecommendation> recommendations;
    
    // Analyze each table
    for (const auto& [table, pattern] : table_patterns_) {
        if (!table_name.empty() && table != table_name) continue;
        
        // Check for full table scans
        if (pattern.full_scan_count > 10 && 
            pattern.avg_rows_examined > 1000) {
            
            // Find most filtered columns
            std::vector<std::pair<std::string, uint64_t>> column_usage;
            for (const auto& [col, count] : pattern.column_filter_count) {
                column_usage.push_back({col, count});
            }
            
            std::sort(column_usage.begin(), column_usage.end(),
                [](const auto& a, const auto& b) { return a.second > b.second; });
            
            // Recommend index on top columns
            if (!column_usage.empty()) {
                IndexRecommendation rec;
                rec.table_name = table;
                
                // Use top 1-3 columns
                for (size_t i = 0; i < std::min(size_t(3), column_usage.size()); ++i) {
                    rec.columns.push_back(column_usage[i].first);
                }
                
                rec.index_type = "BTREE";
                rec.estimated_improvement = 80.0;  // Estimated 80% improvement
                rec.reasoning = "Frequent full table scans with filters on these columns";
                
                recommendations.push_back(rec);
            }
        }
        
        // Check for join columns
        for (const auto& [col, count] : pattern.column_join_count) {
            if (count > 100) {  // Frequent joins
                IndexRecommendation rec;
                rec.table_name = table;
                rec.columns = {col};
                rec.index_type = "BTREE";
                rec.estimated_improvement = 60.0;
                rec.reasoning = "Frequent join operations on this column";
                
                recommendations.push_back(rec);
            }
        }
    }
    
    return recommendations;
}

// [SEQUENCE: MVP14-455] Generate CREATE INDEX SQL
std::string IndexAdvisor::IndexRecommendation::GetCreateIndexSQL() const {
    std::stringstream sql;
    
    // Generate index name
    std::string index_name = "idx_" + table_name;
    for (const auto& col : columns) {
        index_name += "_" + col;
    }
    
    sql << "CREATE INDEX " << index_name << " ON " << table_name << " (";
    
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0) sql << ", ";
        sql << columns[i];
    }
    
    sql << ")";
    
    if (index_type != "BTREE") {
        sql << " USING " << index_type;
    }
    
    return sql.str();
}

// [SEQUENCE: MVP14-456] Rewrite query for optimization
std::string QueryRewriter::RewriteQuery(const std::string& query,
                                      const std::vector<RewriteRule>& rules) {
    std::string optimized = query;
    
    // Apply all rules if none specified
    std::vector<RewriteRule> apply_rules = rules;
    if (apply_rules.empty()) {
        apply_rules = {
            RewriteRule::SUBQUERY_TO_JOIN,
            RewriteRule::IN_TO_EXISTS,
            RewriteRule::ELIMINATE_DISTINCT
        };
    }
    
    for (auto rule : apply_rules) {
        switch (rule) {
            case RewriteRule::SUBQUERY_TO_JOIN:
                optimized = ApplySubqueryToJoin(optimized);
                break;
            case RewriteRule::IN_TO_EXISTS:
                optimized = ApplyInToExists(optimized);
                break;
            case RewriteRule::OR_TO_UNION:
                optimized = ApplyOrToUnion(optimized);
                break;
            default:
                break;
        }
    }
    
    return optimized;
}

// [SEQUENCE: MVP14-457] Optimize SELECT N+1 queries
std::string QueryRewriter::OptimizeSelectN1(const std::string& query) {
    // Detect N+1 pattern and suggest batch query
    std::regex n1_pattern(R"(SELECT .* FROM (\w+) WHERE \w+_id = \?)", 
                         std::regex::icase);
    
    std::smatch match;
    if (std::regex_search(query, match, n1_pattern)) {
        std::string table = match[1];
        
        // Return batch query suggestion
        return "-- N+1 detected. Use batch query instead:\n"
               "-- SELECT * FROM " + table + " WHERE id IN (?, ?, ?, ...)";
    }
    
    return query;
}

// [SEQUENCE: MVP14-458] Optimize pagination queries
std::string QueryRewriter::OptimizePagination(const std::string& query) {
    // Check for OFFSET pagination
    std::regex offset_pattern(R"(LIMIT\s+(\d+)\s+OFFSET\s+(\d+))", 
                            std::regex::icase);
    
    std::smatch match;
    if (std::regex_search(query, match, offset_pattern)) {
        int limit = std::stoi(match[1]);
        int offset = std::stoi(match[2]);
        
        if (offset > 1000) {
            // Suggest cursor-based pagination
            return "-- High OFFSET detected. Consider cursor-based pagination:\n"
                   "-- SELECT * FROM table WHERE id > last_id ORDER BY id LIMIT " + 
                   std::to_string(limit);
        }
    }
    
    return query;
}

// [SEQUENCE: MVP14-459] Check if query is cacheable
bool QueryCacheOptimizer::IsCacheable(const std::string& query) {
    std::string upper_query = query;
    std::transform(upper_query.begin(), upper_query.end(), 
                  upper_query.begin(), ::toupper);
    
    // Don't cache writes
    if (upper_query.find("INSERT") == 0 ||
        upper_query.find("UPDATE") == 0 ||
        upper_query.find("DELETE") == 0) {
        return false;
    }
    
    // Don't cache queries with NOW() or RAND()
    if (upper_query.find("NOW()") != std::string::npos ||
        upper_query.find("RAND()") != std::string::npos ||
        upper_query.find("UUID()") != std::string::npos) {
        return false;
    }
    
    return true;
}

// [SEQUENCE: MVP14-460] Calculate optimal cache TTL
std::chrono::seconds QueryCacheOptimizer::CalculateTTL(
    const std::string& query,
    const QueryPatternAnalyzer::QueryPattern& pattern) {
    
    // Static data - longer TTL
    if (query.find("item_data") != std::string::npos ||
        query.find("skill_data") != std::string::npos) {
        return std::chrono::seconds(3600);  // 1 hour
    }
    
    // Player data - medium TTL
    if (query.find("players") != std::string::npos) {
        return std::chrono::seconds(300);   // 5 minutes
    }
    
    // Real-time data - short TTL
    if (query.find("online_players") != std::string::npos ||
        query.find("combat_log") != std::string::npos) {
        return std::chrono::seconds(30);    // 30 seconds
    }
    
    // Default TTL
    return std::chrono::seconds(300);       // 5 minutes
}

// [SEQUENCE: MVP14-461] Optimize batch insert
std::string BatchQueryOptimizer::OptimizeBatchInsert(
    const std::string& table,
    const std::vector<std::unordered_map<std::string, std::string>>& rows) {
    
    if (rows.empty()) return "";
    
    std::stringstream sql;
    sql << "INSERT INTO " << table << " (";
    
    // Get column names from first row
    std::vector<std::string> columns;
    for (const auto& [col, _] : rows[0]) {
        columns.push_back(col);
    }
    
    // Write column list
    for (size_t i = 0; i < columns.size(); ++i) {
        if (i > 0) sql << ", ";
        sql << columns[i];
    }
    
    sql << ") VALUES ";
    
    // Write values
    for (size_t i = 0; i < rows.size(); ++i) {
        if (i > 0) sql << ", ";
        sql << "(";
        
        for (size_t j = 0; j < columns.size(); ++j) {
            if (j > 0) sql << ", ";
            
            auto it = rows[i].find(columns[j]);
            if (it != rows[i].end()) {
                sql << "'" << it->second << "'";
            } else {
                sql << "NULL";
            }
        }
        
        sql << ")";
    }
    
    return sql.str();
}

// [SEQUENCE: MVP14-462] Record query execution statistics
void QueryStatsCollector::RecordExecution(const std::string& query,
                                        double execution_time_ms,
                                        uint64_t rows_examined,
                                        uint64_t rows_returned,
                                        bool success) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string normalized = NormalizeQuery(query);
    auto& stats = query_stats_[normalized];
    
    stats.query_template = normalized;
    stats.execution_count++;
    
    // Update time statistics
    stats.min_time_ms = std::min(stats.min_time_ms, execution_time_ms);
    stats.max_time_ms = std::max(stats.max_time_ms, execution_time_ms);
    
    double old_avg = stats.avg_time_ms;
    stats.avg_time_ms = (old_avg * (stats.execution_count - 1) + execution_time_ms) / 
                       stats.execution_count;
    
    // Update resource usage
    stats.total_rows_examined += rows_examined;
    stats.total_rows_returned += rows_returned;
    
    // Update error count
    if (!success) {
        stats.error_count++;
        if (execution_time_ms > 30000) {  // 30 second timeout
            stats.timeout_count++;
        }
    }
    
    // Update percentiles periodically
    if (stats.execution_count % 100 == 0) {
        UpdatePercentiles(stats);
    }
}

// [SEQUENCE: MVP14-463] Get slow queries
std::vector<QueryStatsCollector::QueryStats> QueryStatsCollector::GetSlowQueries(
    double threshold_ms, uint32_t limit) const {
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<QueryStats> slow_queries;
    
    for (const auto& [_, stats] : query_stats_) {
        if (stats.avg_time_ms > threshold_ms) {
            slow_queries.push_back(stats);
        }
    }
    
    // Sort by average time descending
    std::sort(slow_queries.begin(), slow_queries.end(),
        [](const auto& a, const auto& b) {
            return a.avg_time_ms > b.avg_time_ms;
        });
    
    // Limit results
    if (slow_queries.size() > limit) {
        slow_queries.resize(limit);
    }
    
    return slow_queries;
}

// [SEQUENCE: MVP14-464] Normalize query for statistics
std::string QueryStatsCollector::NormalizeQuery(const std::string& query) const {
    std::string normalized = query;
    
    // Replace literals with placeholders
    std::regex number_regex(R"(\b\d+\b)");
    normalized = std::regex_replace(normalized, number_regex, "?");
    
    std::regex string_regex(R"('([^']*)')");
    normalized = std::regex_replace(normalized, string_regex, "?");
    
    // Remove extra whitespace
    std::regex whitespace_regex(R"(\s+)");
    normalized = std::regex_replace(normalized, whitespace_regex, " ");
    
    // Trim
    normalized.erase(0, normalized.find_first_not_of(" \t\n\r"));
    normalized.erase(normalized.find_last_not_of(" \t\n\r") + 1);
    
    return normalized;
}

// [SEQUENCE: MVP14-465] Main query optimization
QueryPlan QueryOptimizer::OptimizeQuery(const std::string& query,
                                      const std::vector<OptimizationHint>& hints) {
    // Check cache first
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        auto it = plan_cache_.find(query);
        if (it != plan_cache_.end()) {
            return it->second;
        }
    }
    
    QueryPlan plan;
    plan.original_query = query;
    
    // Analyze query pattern
    auto pattern = QueryPatternAnalyzer::AnalyzeQuery(query);
    
    // Apply query rewriting if enabled
    if (config_.enable_query_rewrite) {
        plan.optimized_query = QueryRewriter::RewriteQuery(query);
        if (plan.optimized_query != query) {
            plan.optimizations_applied.push_back("Query rewriting");
        }
    } else {
        plan.optimized_query = query;
    }
    
    // Get optimization suggestions
    auto suggestions = QueryPatternAnalyzer::SuggestOptimizations(pattern);
    for (const auto& suggestion : suggestions) {
        plan.optimizations_applied.push_back(suggestion);
    }
    
    // Apply hints
    for (auto hint : hints) {
        switch (hint) {
            case OptimizationHint::USE_INDEX:
                plan.optimizations_applied.push_back("Force index usage");
                break;
            case OptimizationHint::PARALLEL:
                if (config_.enable_parallel_execution) {
                    plan.optimizations_applied.push_back("Parallel execution");
                }
                break;
            default:
                break;
        }
    }
    
    // Estimate cost
    plan.estimated_cost = 100.0;  // Simplified estimation
    plan.estimated_rows = 1000;
    plan.estimated_time_ms = pattern.has_join ? 50.0 : 10.0;
    
    // Cache the plan
    {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        plan_cache_[query] = plan;
    }
    
    return plan;
}

// [SEQUENCE: MVP14-466] Execute query with optimization
QueryResult QueryOptimizer::ExecuteOptimized(const std::string& query,
                                           const std::vector<std::string>& params) {
    auto start = std::chrono::high_resolution_clock::now();
    
    // Get optimized plan
    auto plan = OptimizeQuery(query);
    
    // Record statistics if enabled
    if (config_.enable_statistics) {
        auto pattern = QueryPatternAnalyzer::AnalyzeQuery(query);
        
        // Check if cacheable
        if (config_.enable_query_cache && 
            QueryCacheOptimizer::IsCacheable(query)) {
            plan.cache_hit = true;
        }
    }
    
    // Execute query (simplified - would use actual database)
    QueryResult result;
    result.success = true;
    result.affected_rows = 0;
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(end - start).count();
    
    // Update plan with actual results
    plan.actual_time_ms = duration;
    plan.actual_rows = result.affected_rows;
    
    // Record execution statistics
    if (config_.enable_statistics) {
        stats_collector_.RecordExecution(query, duration, 
                                       plan.estimated_rows, 
                                       plan.actual_rows, 
                                       result.success);
        
        // Record for index advisor
        index_advisor_.RecordQueryExecution(query, plan, duration);
    }
    
    return result;
}

// [SEQUENCE: MVP14-467] Get optimization suggestions
std::vector<std::string> QueryOptimizer::GetOptimizationSuggestions(
    const std::string& query) const {
    
    std::vector<std::string> suggestions;
    
    // Analyze pattern
    auto pattern = QueryPatternAnalyzer::AnalyzeQuery(query);
    auto pattern_suggestions = QueryPatternAnalyzer::SuggestOptimizations(pattern);
    suggestions.insert(suggestions.end(), 
                      pattern_suggestions.begin(), 
                      pattern_suggestions.end());
    
    // Check for N+1
    auto n1_optimized = QueryRewriter::OptimizeSelectN1(query);
    if (n1_optimized != query) {
        suggestions.push_back("Potential N+1 query pattern detected");
    }
    
    // Check pagination
    auto pagination_optimized = QueryRewriter::OptimizePagination(query);
    if (pagination_optimized != query) {
        suggestions.push_back("Consider cursor-based pagination for high offsets");
    }
    
    // Get index recommendations
    if (!pattern.tables.empty()) {
        auto index_recs = index_advisor_.GetRecommendations(pattern.tables[0]);
        for (const auto& rec : index_recs) {
            suggestions.push_back("Consider index: " + rec.GetCreateIndexSQL());
        }
    }
    
    return suggestions;
}

} // namespace mmorpg::database
