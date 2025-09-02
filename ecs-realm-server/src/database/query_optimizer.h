#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <optional>
#include <variant>
#include "../core/types.h"

namespace mmorpg::database {

// [SEQUENCE: MVP14-409] Query optimization hints
enum class OptimizationHint {
    USE_INDEX,          // 특정 인덱스 사용
    FORCE_INDEX,        // 인덱스 강제 사용
    IGNORE_INDEX,       // 인덱스 무시
    STRAIGHT_JOIN,      // 조인 순서 고정
    NO_CACHE,           // 쿼리 캐시 사용 안함
    PARALLEL,           // 병렬 실행
    BATCH_SIZE          // 배치 크기 지정
};

// [SEQUENCE: MVP14-410] Query execution plan
struct QueryPlan {
    std::string original_query;
    std::string optimized_query;
    
    // Execution details
    std::vector<std::string> tables_accessed;
    std::vector<std::string> indexes_used;
    std::string join_type;
    
    // Cost estimates
    uint64_t estimated_rows{0};
    double estimated_cost{0};
    double estimated_time_ms{0};
    
    // Optimization applied
    std::vector<std::string> optimizations_applied;
    
    // Statistics
    uint64_t actual_rows{0};
    double actual_time_ms{0};
    bool cache_hit{false};
};

// [SEQUENCE: MVP14-411] Query pattern analyzer
class QueryPatternAnalyzer {
public:
    // [SEQUENCE: MVP14-412] Analyze query pattern
    struct QueryPattern {
        enum Type {
            SIMPLE_SELECT,      // 단순 SELECT
            JOIN_QUERY,         // JOIN 쿼리
            AGGREGATE,          // 집계 쿼리
            SUBQUERY,          // 서브쿼리
            UNION_QUERY,       // UNION 쿼리
            UPDATE_QUERY,      // UPDATE
            INSERT_QUERY,      // INSERT
            DELETE_QUERY       // DELETE
        } type;
        
        std::vector<std::string> tables;
        std::vector<std::string> columns;
        std::vector<std::string> conditions;
        std::vector<std::string> order_by;
        std::optional<uint32_t> limit;
        
        bool has_join{false};
        bool has_subquery{false};
        bool has_aggregation{false};
        bool has_group_by{false};
        bool has_order_by{false};
    };
    
    static QueryPattern AnalyzeQuery(const std::string& query);
    
    // [SEQUENCE: MVP14-413] Suggest optimizations
    static std::vector<std::string> SuggestOptimizations(const QueryPattern& pattern);
};

// [SEQUENCE: MVP14-414] Index advisor
class IndexAdvisor {
public:
    // [SEQUENCE: MVP14-415] Index recommendation
    struct IndexRecommendation {
        std::string table_name;
        std::vector<std::string> columns;
        std::string index_type;  // BTREE, HASH, FULLTEXT
        
        double estimated_improvement{0};  // Percentage
        std::string reasoning;
        
        // SQL to create index
        std::string GetCreateIndexSQL() const;
    };
    
    // [SEQUENCE: MVP14-416] Analyze table access patterns
    void RecordQueryExecution(const std::string& query, 
                            const QueryPlan& plan,
                            double execution_time_ms);
    
    // [SEQUENCE: MVP14-417] Get index recommendations
    std::vector<IndexRecommendation> GetRecommendations(
        const std::string& table_name = "") const;
    
    // [SEQUENCE: MVP14-418] Check for unused indexes
    std::vector<std::string> GetUnusedIndexes(
        std::chrono::hours threshold = std::chrono::hours(24*7)) const;
    
private:
    struct TableAccessPattern {
        std::unordered_map<std::string, uint64_t> column_access_count;
        std::unordered_map<std::string, uint64_t> column_filter_count;
        std::unordered_map<std::string, uint64_t> column_join_count;
        std::unordered_map<std::string, uint64_t> column_order_count;
        
        uint64_t full_scan_count{0};
        double avg_rows_examined{0};
        double avg_execution_time_ms{0};
    };
    
    std::unordered_map<std::string, TableAccessPattern> table_patterns_;
    std::unordered_map<std::string, std::chrono::system_clock::time_point> 
        index_last_used_;
    
    mutable std::mutex mutex_;
};

// [SEQUENCE: MVP14-419] Query rewriter
class QueryRewriter {
public:
    // [SEQUENCE: MVP14-420] Rewrite rules
    enum class RewriteRule {
        SUBQUERY_TO_JOIN,       // 서브쿼리를 JOIN으로
        IN_TO_EXISTS,           // IN을 EXISTS로
        OR_TO_UNION,            // OR을 UNION으로
        ELIMINATE_DISTINCT,     // 불필요한 DISTINCT 제거
        PUSH_DOWN_PREDICATE,    // 조건 하향 이동
        MERGE_VIEW,             // 뷰 병합
        MATERIALIZE_CTE         // CTE 구체화
    };
    
    // [SEQUENCE: MVP14-421] Apply rewrite rules
    static std::string RewriteQuery(const std::string& query,
                                   const std::vector<RewriteRule>& rules = {});
    
    // [SEQUENCE: MVP14-422] Optimize specific patterns
    static std::string OptimizeSelectN1(const std::string& query);
    static std::string OptimizePagination(const std::string& query);
    static std::string OptimizeBulkInsert(const std::string& query);
    
private:
    static std::string ApplySubqueryToJoin(const std::string& query);
    static std::string ApplyInToExists(const std::string& query);
    static std::string ApplyOrToUnion(const std::string& query);
};

// [SEQUENCE: MVP14-423] Query cache optimizer
class QueryCacheOptimizer {
public:
    // [SEQUENCE: MVP14-424] Cache key generation
    struct CacheKey {
        std::string query_hash;
        std::vector<std::string> parameter_values;
        std::string database_name;
        
        std::string ToString() const;
    };
    
    // [SEQUENCE: MVP14-425] Cache entry
    struct CacheEntry {
        std::string result_data;
        size_t result_size{0};
        uint32_t row_count{0};
        
        std::chrono::system_clock::time_point created_at;
        std::chrono::system_clock::time_point expires_at;
        uint64_t access_count{0};
        
        bool IsExpired() const {
            return std::chrono::system_clock::now() > expires_at;
        }
    };
    
    // [SEQUENCE: MVP14-426] Check if query is cacheable
    static bool IsCacheable(const std::string& query);
    
    // [SEQUENCE: MVP14-427] Calculate optimal TTL
    static std::chrono::seconds CalculateTTL(const std::string& query,
                                            const QueryPattern& pattern);
    
    // [SEQUENCE: MVP14-428] Invalidation rules
    static std::vector<CacheKey> GetInvalidationKeys(
        const std::string& table_name,
        const std::string& operation);
};

// [SEQUENCE: MVP14-429] Batch query optimizer
class BatchQueryOptimizer {
public:
    // [SEQUENCE: MVP14-430] Batch insert optimization
    static std::string OptimizeBatchInsert(
        const std::string& table,
        const std::vector<std::unordered_map<std::string, std::string>>& rows);
    
    // [SEQUENCE: MVP14-431] Batch update optimization
    static std::string OptimizeBatchUpdate(
        const std::string& table,
        const std::vector<std::pair<std::string, std::unordered_map<std::string, std::string>>>& updates);
    
    // [SEQUENCE: MVP14-432] Multi-query optimization
    static std::vector<std::string> OptimizeMultiQuery(
        const std::vector<std::string>& queries);
    
    // [SEQUENCE: MVP14-433] Prepared statement batching
    struct PreparedBatch {
        std::string statement;
        std::vector<std::vector<std::string>> parameter_sets;
        uint32_t batch_size{1000};
    };
    
    static PreparedBatch CreatePreparedBatch(
        const std::string& query_template,
        const std::vector<std::vector<std::string>>& parameters);
};

// [SEQUENCE: MVP14-434] Query execution optimizer
class QueryExecutionOptimizer {
public:
    // [SEQUENCE: MVP14-435] Execution strategies
    enum class ExecutionStrategy {
        SINGLE_THREAD,      // 단일 스레드 실행
        PARALLEL,           // 병렬 실행
        ASYNC,              // 비동기 실행
        DISTRIBUTED         // 분산 실행
    };
    
    // [SEQUENCE: MVP14-436] Determine optimal strategy
    static ExecutionStrategy DetermineStrategy(
        const QueryPattern& pattern,
        uint64_t estimated_rows);
    
    // [SEQUENCE: MVP14-437] Parallel execution plan
    struct ParallelPlan {
        uint32_t thread_count{1};
        std::vector<std::string> partition_queries;
        std::string merge_strategy;
    };
    
    static ParallelPlan CreateParallelPlan(
        const std::string& query,
        uint32_t available_threads = 4);
};

// [SEQUENCE: MVP14-438] Query statistics collector
class QueryStatsCollector {
public:
    // [SEQUENCE: MVP14-439] Query statistics
    struct QueryStats {
        std::string query_template;
        uint64_t execution_count{0};
        
        // Performance metrics
        double min_time_ms{std::numeric_limits<double>::max()};
        double max_time_ms{0};
        double avg_time_ms{0};
        double p95_time_ms{0};
        double p99_time_ms{0};
        
        // Resource usage
        uint64_t total_rows_examined{0};
        uint64_t total_rows_returned{0};
        uint64_t total_bytes_sent{0};
        
        // Errors
        uint64_t error_count{0};
        uint64_t timeout_count{0};
    };
    
    // [SEQUENCE: MVP14-440] Record query execution
    void RecordExecution(const std::string& query,
                        double execution_time_ms,
                        uint64_t rows_examined,
                        uint64_t rows_returned,
                        bool success = true);
    
    // [SEQUENCE: MVP14-441] Get slow queries
    std::vector<QueryStats> GetSlowQueries(
        double threshold_ms = 100.0,
        uint32_t limit = 100) const;
    
    // [SEQUENCE: MVP14-442] Get most frequent queries
    std::vector<QueryStats> GetFrequentQueries(
        uint32_t limit = 100) const;
    
    // [SEQUENCE: MVP14-443] Export statistics
    std::string ExportStatistics(const std::string& format = "json") const;
    
private:
    std::unordered_map<std::string, QueryStats> query_stats_;
    mutable std::mutex mutex_;
    
    std::string NormalizeQuery(const std::string& query) const;
    void UpdatePercentiles(QueryStats& stats);
};

// [SEQUENCE: MVP14-444] Main query optimizer
class QueryOptimizer {
public:
    static QueryOptimizer& Instance() {
        static QueryOptimizer instance;
        return instance;
    }
    
    // [SEQUENCE: MVP14-445] Optimize query
    QueryPlan OptimizeQuery(const std::string& query,
                           const std::vector<OptimizationHint>& hints = {});
    
    // [SEQUENCE: MVP14-446] Execute with optimization
    QueryResult ExecuteOptimized(const std::string& query,
                                const std::vector<std::string>& params = {});
    
    // [SEQUENCE: MVP14-447] Get optimization suggestions
    std::vector<std::string> GetOptimizationSuggestions(
        const std::string& query) const;
    
    // [SEQUENCE: MVP14-448] Components
    IndexAdvisor& GetIndexAdvisor() { return index_advisor_; }
    QueryStatsCollector& GetStatsCollector() { return stats_collector_; }
    
    // [SEQUENCE: MVP14-449] Configuration
    struct OptimizerConfig {
        bool enable_query_rewrite{true};
        bool enable_parallel_execution{true};
        bool enable_query_cache{true};
        bool enable_statistics{true};
        
        uint32_t max_parallel_threads{4};
        size_t query_cache_size{10000};
        std::chrono::seconds cache_ttl{300};
    };
    
    void Configure(const OptimizerConfig& config) { config_ = config; }
    
private:
    QueryOptimizer() = default;
    
    IndexAdvisor index_advisor_;
    QueryStatsCollector stats_collector_;
    OptimizerConfig config_;
    
    // Query plan cache
    std::unordered_map<std::string, QueryPlan> plan_cache_;
    mutable std::mutex cache_mutex_;
};

// [SEQUENCE: MVP14-450] Query optimization utilities
namespace QueryOptimizationUtils {
    // Generate EXPLAIN output
    std::string ExplainQuery(const std::string& query);
    
    // Validate query syntax
    bool ValidateQuery(const std::string& query);
    
    // Estimate query cost
    double EstimateQueryCost(const std::string& query);
    
    // Format query for readability
    std::string FormatQuery(const std::string& query);
}

} // namespace mmorpg::database
