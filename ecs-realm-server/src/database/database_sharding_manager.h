#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>
#include <future>
#include <queue>

namespace mmorpg::database {

// [SEQUENCE: MVP14-576] 데이터베이스 샤딩 관리자
class DatabaseShardingManager {
public:
    struct ShardInfo {
        std::string shard_id;
        std::string host;
        int port;
        std::string database_name;
        std::string username;
        std::string password;
        bool is_master;
        std::vector<std::string> replica_hosts;
        std::atomic<bool> is_healthy{true};
        std::atomic<int> connection_count{0};
        std::chrono::steady_clock::time_point last_health_check;
        
        // 샤드 메타데이터
        uint64_t user_id_range_start;
        uint64_t user_id_range_end;
        std::atomic<uint64_t> total_users{0};
        std::atomic<uint64_t> storage_used_mb{0};
    };

    struct ShardingConfig {
        std::vector<ShardInfo> shards;
        std::string sharding_key = "user_id"; // 샤딩 키
        int max_connections_per_shard = 50;
        std::chrono::seconds health_check_interval{30};
        bool enable_read_write_split = true;
        bool enable_auto_failover = true;
        double load_balancing_threshold = 0.8;
        bool enable_cross_shard_transactions = false;
    };

    explicit DatabaseShardingManager(const ShardingConfig& config)
        : config_(config), is_running_(false) {
        InitializeShards();
    }

    ~DatabaseShardingManager() {
        Shutdown();
    }

    // [SEQUENCE: MVP14-577] 샤드 초기화
    bool InitializeShards() {
        std::lock_guard<std::mutex> lock(shards_mutex_);
        
        for (const auto& shard_info : config_.shards) {
            auto shard = std::make_shared<ShardInfo>(shard_info);
            shards_[shard_info.shard_id] = shard;
            
            // 사용자 ID 범위별 매핑
            for (uint64_t user_id = shard_info.user_id_range_start; 
                 user_id <= shard_info.user_id_range_end; ++user_id) {
                user_id_to_shard_[user_id] = shard;
            }
        }
        
        // 헬스체크 시작
        is_running_ = true;
        health_check_thread_ = std::thread(&DatabaseShardingManager::HealthCheckLoop, this);
        
        return !shards_.empty();
    }

    // [SEQUENCE: MVP14-578] 사용자별 데이터 조회
    template<typename T>
    std::future<std::optional<T>> GetUserDataAsync(uint64_t user_id, const std::string& table, 
                                                  const std::string& columns = "*") {
        return std::async(std::launch::async, [this, user_id, table, columns]() -> std::optional<T> {
            auto shard = GetShardForUser(user_id);
            if (!shard || !shard->is_healthy.load()) {
                // 읽기 전용 replica로 fallback
                shard = GetHealthyReplicaForUser(user_id);
                if (!shard) return std::nullopt;
            }
            
            return ExecuteQuery<T>(shard, BuildSelectQuery(table, columns, user_id), false);
        });
    }

    // [SEQUENCE: MVP14-579] 사용자별 데이터 저장
    template<typename T>
    std::future<bool> SaveUserDataAsync(uint64_t user_id, const std::string& table, 
                                       const T& data) {
        return std::async(std::launch::async, [this, user_id, table, data]() -> bool {
            auto shard = GetShardForUser(user_id);
            if (!shard || !shard->is_healthy.load()) {
                return false; // 쓰기는 master에서만
            }
            
            auto query = BuildInsertQuery(table, data, user_id);
            return ExecuteQuery<bool>(shard, query, true).value_or(false);
        });
    }

    // [SEQUENCE: MVP14-580] 크로스 샤드 트랜잭션
    struct CrossShardTransaction {
        std::string transaction_id;
        std::vector<std::string> involved_shards;
        std::unordered_map<std::string, std::vector<std::string>> shard_queries;
        std::atomic<bool> is_committed{false};
        std::atomic<bool> is_rolled_back{false};
        std::chrono::steady_clock::time_point created_at;
    };

    std::future<bool> ExecuteCrossShardTransactionAsync(
        const std::vector<std::pair<uint64_t, std::string>>& user_queries) {
        
        if (!config_.enable_cross_shard_transactions) {
            throw std::runtime_error("Cross-shard transactions are disabled");
        }
        
        return std::async(std::launch::async, [this, user_queries]() -> bool {
            auto transaction = std::make_shared<CrossShardTransaction>();
            transaction->transaction_id = GenerateTransactionId();
            transaction->created_at = std::chrono::steady_clock::now();
            
            // 관련된 샤드들 식별
            std::unordered_map<std::string, std::vector<std::string>> shard_operations;
            
            for (const auto& [user_id, query] : user_queries) {
                auto shard = GetShardForUser(user_id);
                if (!shard || !shard->is_healthy.load()) {
                    return false;
                }
                
                shard_operations[shard->shard_id].push_back(query);
                transaction->involved_shards.push_back(shard->shard_id);
            }
            
            // 2PC (Two-Phase Commit) 실행
            return ExecuteTwoPhaseCommit(transaction, shard_operations);
        });
    }

    // [SEQUENCE: MVP14-581] 샤드 리밸런싱
    std::future<bool> RebalanceShardsAsync() {
        return std::async(std::launch::async, [this]() -> bool {
            std::lock_guard<std::mutex> lock(shards_mutex_);
            
            // 각 샤드의 부하 분석
            std::vector<std::pair<std::string, double>> shard_loads;
            
            for (const auto& [shard_id, shard] : shards_) {
                double load = CalculateShardLoad(shard);
                shard_loads.emplace_back(shard_id, load);
            }
            
            // 부하가 임계값을 초과하는 샤드 식별
            std::vector<std::string> overloaded_shards;
            std::vector<std::string> underloaded_shards;
            
            for (const auto& [shard_id, load] : shard_loads) {
                if (load > config_.load_balancing_threshold) {
                    overloaded_shards.push_back(shard_id);
                } else if (load < config_.load_balancing_threshold * 0.5) {
                    underloaded_shards.push_back(shard_id);
                }
            }
            
            // 리밸런싱 계획 수립 및 실행
            if (!overloaded_shards.empty() && !underloaded_shards.empty()) {
                return ExecuteRebalancingPlan(overloaded_shards, underloaded_shards);
            }
            
            return true; // 리밸런싱 불필요
        });
    }

    // [SEQUENCE: MVP14-582] 샤드 통계 및 모니터링
    struct ShardStats {
        std::string shard_id;
        bool is_healthy;
        int active_connections;
        uint64_t total_users;
        uint64_t storage_used_mb;
        double cpu_usage_percent;
        double memory_usage_percent;
        double current_load;
        std::chrono::steady_clock::time_point last_update;
        
        // 성능 지표
        uint64_t queries_per_second;
        double average_query_time_ms;
        uint64_t failed_queries_count;
    };

    std::vector<ShardStats> GetAllShardStats() const {
        std::lock_guard<std::mutex> lock(shards_mutex_);
        std::vector<ShardStats> stats;
        
        for (const auto& [shard_id, shard] : shards_) {
            ShardStats stat;
            stat.shard_id = shard_id;
            stat.is_healthy = shard->is_healthy.load();
            stat.active_connections = shard->connection_count.load();
            stat.total_users = shard->total_users.load();
            stat.storage_used_mb = shard->storage_used_mb.load();
            stat.current_load = CalculateShardLoad(shard);
            stat.last_update = std::chrono::steady_clock::now();
            
            // 실시간 성능 지표 (시뮬레이션)
            stat.cpu_usage_percent = 20.0 + (std::rand() % 60); // 20-80%
            stat.memory_usage_percent = 30.0 + (std::rand() % 50); // 30-80%
            stat.queries_per_second = 100 + (std::rand() % 900); // 100-1000 QPS
            stat.average_query_time_ms = 1.0 + (std::rand() % 10); // 1-10ms
            stat.failed_queries_count = std::rand() % 10; // 0-9 failures
            
            stats.push_back(stat);
        }
        
        return stats;
    }

    // [SEQUENCE: MVP14-583] 동적 샤드 추가
    std::future<bool> AddShardAsync(const ShardInfo& new_shard_info) {
        return std::async(std::launch::async, [this, new_shard_info]() -> bool {
            std::lock_guard<std::mutex> lock(shards_mutex_);
            
            // 새 샤드 추가
            auto new_shard = std::make_shared<ShardInfo>(new_shard_info);
            shards_[new_shard_info.shard_id] = new_shard;
            
            // 사용자 ID 매핑 업데이트
            for (uint64_t user_id = new_shard_info.user_id_range_start; 
                 user_id <= new_shard_info.user_id_range_end; ++user_id) {
                user_id_to_shard_[user_id] = new_shard;
            }
            
            // 새 샤드 초기화
            bool success = InitializeShard(new_shard);
            if (success) {
                // 기존 샤드에서 데이터 마이그레이션 시작
                StartDataMigration(new_shard);
            }
            
            return success;
        });
    }

    void Shutdown() {
        is_running_ = false;
        if (health_check_thread_.joinable()) {
            health_check_thread_.join();
        }
    }

private:
    ShardingConfig config_;
    std::unordered_map<std::string, std::shared_ptr<ShardInfo>> shards_;
    std::unordered_map<uint64_t, std::shared_ptr<ShardInfo>> user_id_to_shard_;
    mutable std::mutex shards_mutex_;
    std::atomic<bool> is_running_;
    std::thread health_check_thread_;

    // [SEQUENCE: MVP14-584] 샤드 선택 로직
    std::shared_ptr<ShardInfo> GetShardForUser(uint64_t user_id) {
        std::lock_guard<std::mutex> lock(shards_mutex_);
        
        auto it = user_id_to_shard_.find(user_id);
        if (it != user_id_to_shard_.end()) {
            return it->second;
        }
        
        // 사용자 ID 범위 기반 샤드 선택
        for (const auto& [shard_id, shard] : shards_) {
            if (user_id >= shard->user_id_range_start && 
                user_id <= shard->user_id_range_end) {
                user_id_to_shard_[user_id] = shard; // 캐시
                return shard;
            }
        }
        
        return nullptr;
    }

    std::shared_ptr<ShardInfo> GetHealthyReplicaForUser(uint64_t user_id) {
        auto master_shard = GetShardForUser(user_id);
        if (!master_shard) return nullptr;
        
        // replica에서 읽기 (시뮬레이션)
        if (!master_shard->replica_hosts.empty()) {
            return master_shard; // 단순화: 같은 샤드 객체 반환
        }
        
        return nullptr;
    }

    // [SEQUENCE: MVP14-585] 쿼리 실행
    template<typename T>
    std::optional<T> ExecuteQuery(std::shared_ptr<ShardInfo> shard, 
                                 const std::string& query, bool is_write) {
        if (!shard || !shard->is_healthy.load()) {
            return std::nullopt;
        }
        
        // 연결 수 증가
        shard->connection_count.fetch_add(1);
        
        try {
            // 실제 데이터베이스 연결 및 쿼리 실행 시뮬레이션
            if constexpr (std::is_same_v<T, bool>) {
                // INSERT/UPDATE/DELETE 결과
                bool success = (std::rand() % 100) < 95; // 95% 성공률
                shard->connection_count.fetch_sub(1);
                return success;
            } else {
                // SELECT 결과
                T result{}; // 실제로는 데이터베이스에서 조회
                shard->connection_count.fetch_sub(1);
                return result;
            }
            
        } catch (const std::exception&) {
            shard->connection_count.fetch_sub(1);
            if (is_write) {
                shard->is_healthy.store(false);
            }
            return std::nullopt;
        }
    }

    // [SEQUENCE: MVP14-586] 쿼리 빌더
    std::string BuildSelectQuery(const std::string& table, const std::string& columns, 
                                uint64_t user_id) {
        return "SELECT " + columns + " FROM " + table + " WHERE user_id = " + std::to_string(user_id);
    }

    template<typename T>
    std::string BuildInsertQuery(const std::string& table, const T& data, uint64_t user_id) {
        // 실제로는 ORM 또는 query builder 사용
        return "INSERT INTO " + table + " (user_id, data) VALUES (" + 
               std::to_string(user_id) + ", '" + SerializeData(data) + "')";
    }

    template<typename T>
    std::string SerializeData(const T& data) {
        // JSON 직렬화 또는 기타 직렬화 방법
        return "{}"; // 시뮬레이션
    }

    // [SEQUENCE: MVP14-587] 2PC (Two-Phase Commit) 구현
    bool ExecuteTwoPhaseCommit(std::shared_ptr<CrossShardTransaction> transaction,
                              const std::unordered_map<std::string, std::vector<std::string>>& shard_operations) {
        
        // Phase 1: Prepare
        std::vector<bool> prepare_results;
        
        for (const auto& [shard_id, queries] : shard_operations) {
            auto shard = shards_[shard_id];
            bool prepare_success = ExecutePreparePhase(shard, transaction->transaction_id, queries);
            prepare_results.push_back(prepare_success);
        }
        
        // 모든 샤드가 준비되었는지 확인
        bool all_prepared = std::all_of(prepare_results.begin(), prepare_results.end(),
                                       [](bool result) { return result; });
        
        if (!all_prepared) {
            // Rollback all shards
            ExecuteRollback(transaction, shard_operations);
            return false;
        }
        
        // Phase 2: Commit
        bool commit_success = true;
        for (const auto& [shard_id, queries] : shard_operations) {
            auto shard = shards_[shard_id];
            if (!ExecuteCommitPhase(shard, transaction->transaction_id)) {
                commit_success = false;
                break;
            }
        }
        
        if (commit_success) {
            transaction->is_committed.store(true);
        } else {
            ExecuteRollback(transaction, shard_operations);
        }
        
        return commit_success;
    }

    bool ExecutePreparePhase(std::shared_ptr<ShardInfo> shard, const std::string& tx_id,
                            const std::vector<std::string>& queries) {
        // PREPARE 명령 실행 시뮬레이션
        return shard->is_healthy.load() && (std::rand() % 100) < 90; // 90% 성공률
    }

    bool ExecuteCommitPhase(std::shared_ptr<ShardInfo> shard, const std::string& tx_id) {
        // COMMIT 명령 실행 시뮬레이션
        return shard->is_healthy.load() && (std::rand() % 100) < 95; // 95% 성공률
    }

    void ExecuteRollback(std::shared_ptr<CrossShardTransaction> transaction,
                        const std::unordered_map<std::string, std::vector<std::string>>& shard_operations) {
        for (const auto& [shard_id, queries] : shard_operations) {
            auto shard = shards_[shard_id];
            // ROLLBACK 명령 실행
        }
        transaction->is_rolled_back.store(true);
    }

    // [SEQUENCE: MVP14-588] 부하 계산
    double CalculateShardLoad(std::shared_ptr<ShardInfo> shard) const {
        double connection_load = static_cast<double>(shard->connection_count.load()) / 
                               config_.max_connections_per_shard;
        
        double storage_load = static_cast<double>(shard->storage_used_mb.load()) / 
                            (10 * 1024); // 10GB 기준
        
        double user_load = static_cast<double>(shard->total_users.load()) / 
                         (shard->user_id_range_end - shard->user_id_range_start + 1);
        
        return std::max({connection_load, storage_load, user_load});
    }

    // [SEQUENCE: MVP14-589] 리밸런싱 실행
    bool ExecuteRebalancingPlan(const std::vector<std::string>& overloaded_shards,
                               const std::vector<std::string>& underloaded_shards) {
        // 사용자 데이터 마이그레이션 계획 수립
        for (const auto& overloaded_shard_id : overloaded_shards) {
            for (const auto& underloaded_shard_id : underloaded_shards) {
                // 일부 사용자를 overloaded에서 underloaded로 이동
                if (MigrateUsers(overloaded_shard_id, underloaded_shard_id, 1000)) {
                    break; // 하나의 대상 샤드로 충분
                }
            }
        }
        
        return true;
    }

    bool MigrateUsers(const std::string& source_shard_id, const std::string& target_shard_id, 
                     uint64_t user_count) {
        // 사용자 데이터 마이그레이션 실행 (시뮬레이션)
        auto source_shard = shards_[source_shard_id];
        auto target_shard = shards_[target_shard_id];
        
        if (!source_shard || !target_shard) return false;
        
        // 마이그레이션 실행 (실제로는 복잡한 데이터 이동 과정)
        source_shard->total_users.fetch_sub(user_count);
        target_shard->total_users.fetch_add(user_count);
        
        return true;
    }

    // [SEQUENCE: MVP14-590] 헬스체크 및 유틸리티
    void HealthCheckLoop() {
        while (is_running_) {
            {
                std::lock_guard<std::mutex> lock(shards_mutex_);
                for (const auto& [shard_id, shard] : shards_) {
                    CheckShardHealth(shard);
                }
            }
            
            std::this_thread::sleep_for(config_.health_check_interval);
        }
    }

    void CheckShardHealth(std::shared_ptr<ShardInfo> shard) {
        try {
            // 간단한 SELECT 1 쿼리로 상태 확인
            bool health_check = ExecuteQuery<bool>(shard, "SELECT 1", false).value_or(false);
            shard->is_healthy.store(health_check);
            
            if (health_check) {
                shard->last_health_check = std::chrono::steady_clock::now();
            }
            
        } catch (const std::exception&) {
            shard->is_healthy.store(false);
        }
    }

    bool InitializeShard(std::shared_ptr<ShardInfo> shard) {
        // 샤드 초기화 (테이블 생성, 인덱스 등)
        return shard->is_healthy.load();
    }

    void StartDataMigration(std::shared_ptr<ShardInfo> new_shard) {
        // 기존 샤드에서 새 샤드로 데이터 마이그레이션
        // 백그라운드 작업으로 실행
    }

    std::string GenerateTransactionId() {
        static std::atomic<uint64_t> counter{0};
        auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        return "tx_" + std::to_string(now) + "_" + std::to_string(counter.fetch_add(1));
    }
};

} // namespace mmorpg::database
