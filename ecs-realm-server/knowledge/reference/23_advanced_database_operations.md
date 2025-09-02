# 18단계: 고급 데이터베이스 운영 - 페타바이트 게임 데이터를 완벽하게 관리하기
*작은 게임에서 글로벌 대작까지, 어떤 규모든 감당하는 무적의 데이터베이스 아키텍처*

> **🎯 목표**: 1억 명 플레이어의 모든 게임 데이터를 안전하고 빠르게 관리하는 페타바이트급 데이터베이스 시스템 구축하기

---

## 📋 문서 정보

- **난이도**: ⚫ 전문가 (데이터베이스 운영의 최고봉!)
- **예상 학습 시간**: 10-12시간 (아키텍처 설계 + 최적화)
- **필요 선수 지식**: 
  - ✅ [1-17단계](./01_advanced_cpp_features.md) 모든 내용 완료
  - ✅ SQL 기본기 완벽 숙지
  - ✅ "샤딩", "복제" 같은 용어 이해
  - ✅ 대용량 서비스 운영 경험 (또는 관심)
- **실습 환경**: MySQL/PostgreSQL, Redis Cluster, 모니터링 도구
- **최종 결과물**: 포트나이트급 대용량 게임 데이터베이스 시스템!

---

## 🤔 왜 작은 게임도 대용량 데이터베이스를 준비해야 할까?

### **게임 서비스 성장에 따른 데이터 폭발**

```sql
-- 😰 1년차: 소규모 서비스
SELECT COUNT(*) FROM players;        -- 10,000명
SELECT COUNT(*) FROM game_logs;      -- 1백만 건
SELECT SUM(data_length) FROM tables; -- 10GB

-- 😅 3년차: 중간 규모 서비스  
SELECT COUNT(*) FROM players;        -- 100만명
SELECT COUNT(*) FROM game_logs;      -- 10억 건
SELECT SUM(data_length) FROM tables; -- 1TB

-- 😱 5년차: 대규모 서비스
SELECT COUNT(*) FROM players;        -- 1,000만명  
SELECT COUNT(*) FROM game_logs;      -- 1조 건
SELECT SUM(data_length) FROM tables; -- 100TB+

-- 💀 글로벌 서비스
SELECT COUNT(*) FROM players;        -- 1억명+
SELECT COUNT(*) FROM game_logs;      -- 100조 건
SELECT SUM(data_length) FROM tables; -- 페타바이트급
```

**실제 게임 사례:**
- **월드 오브 워크래프트**: 수십 페타바이트 데이터
- **리그 오브 레전드**: 일일 10억+ 게임 이벤트
- **포트나이트**: 3억+ 유저 데이터 관리

### **단일 DB의 한계점**

```cpp
// 🐌 단일 MySQL 서버의 현실적 한계
struct SingleDBLimits {
    int max_connections = 10000;           // 동시 연결 한계
    long max_table_size_gb = 256;          // 단일 테이블 크기 한계  
    int max_qps = 50000;                   // 초당 쿼리 한계
    float backup_time_hours = 24;          // 100GB 백업에 24시간
    float recovery_time_hours = 48;        // 복구에 48시간
    
    void PrintBottlenecks() {
        std::cout << "❌ 5,000명 동시 접속 시 한계점:" << std::endl;
        std::cout << "- 커넥션 부족: " << max_connections << " < 5,000" << std::endl;
        std::cout << "- 스토리지 한계: 로그 테이블 256GB 초과" << std::endl;
        std::cout << "- 성능 저하: QPS 50,000 초과 시 응답 지연" << std::endl;
        std::cout << "- 백업 시간: 일일 백업에 하루 종일 소요" << std::endl;
        std::cout << "- 장애 복구: 48시간 서비스 중단" << std::endl;
    }
};

// ✅ 분산 DB 아키텍처의 장점
struct DistributedDBBenefits {
    int max_connections = 1000000;        // 거의 무제한 확장
    long max_table_size_tb = 100;         // 테라바이트급 테이블
    int max_qps = 10000000;               // 천만 QPS
    float backup_time_minutes = 5;        // 5분 내 스냅샷 백업
    float recovery_time_seconds = 30;     // 30초 내 자동 복구
    
    void PrintAdvantages() {
        std::cout << "✅ 분산 DB 아키텍처의 이점:" << std::endl;
        std::cout << "- 수평적 확장: 서버 추가로 성능 선형 증가" << std::endl;
        std::cout << "- 고가용성: 단일 장애점 제거" << std::endl;
        std::cout << "- 지역별 최적화: 글로벌 서비스 대응" << std::endl;
        std::cout << "- 비용 효율성: 필요한 만큼만 확장" << std::endl;
    }
};
```

---

## 🔄 1. 샤딩 전략 구현 (수평적 확장)

### **1.1 샤딩 방식 비교**

```cpp
#include <iostream>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cmath>

// 🎯 다양한 샤딩 전략 구현
class ShardingStrategy {
public:
    virtual ~ShardingStrategy() = default;
    virtual int GetShardIndex(uint21_t key, int shard_count) const = 0;
    virtual std::string GetStrategyName() const = 0;
    virtual void AnalyzeDistribution(const std::vector<uint21_t>& keys, int shard_count) const = 0;
};

// 1️⃣ 해시 기반 샤딩 (가장 일반적)
class HashSharding : public ShardingStrategy {
public:
    int GetShardIndex(uint21_t key, int shard_count) const override {
        return std::hash<uint21_t>{}(key) % shard_count;
    }
    
    std::string GetStrategyName() const override {
        return "Hash Sharding";
    }
    
    void AnalyzeDistribution(const std::vector<uint21_t>& keys, int shard_count) const override {
        std::vector<int> distribution(shard_count, 0);
        
        for (uint21_t key : keys) {
            int shard = GetShardIndex(key, shard_count);
            distribution[shard]++;
        }
        
        std::cout << "\n📊 " << GetStrategyName() << " 분포 분석:" << std::endl;
        for (int i = 0; i < shard_count; ++i) {
            float percentage = (float)distribution[i] / keys.size() * 100;
            std::cout << "샤드 " << i << ": " << distribution[i] 
                      << " 키 (" << percentage << "%)" << std::endl;
        }
        
        // 편차 계산
        float avg = (float)keys.size() / shard_count;
        float variance = 0;
        for (int count : distribution) {
            variance += std::pow(count - avg, 2);
        }
        variance /= shard_count;
        float std_dev = std::sqrt(variance);
        
        std::cout << "표준편차: " << std_dev << " (낮을수록 균등 분산)" << std::endl;
    }
};

// 2️⃣ 범위 기반 샤딩 (순차적 데이터에 적합)
class RangeSharding : public ShardingStrategy {
private:
    std::vector<uint21_t> range_boundaries;
    
public:
    RangeSharding(const std::vector<uint21_t>& boundaries) : range_boundaries(boundaries) {}
    
    int GetShardIndex(uint21_t key, int shard_count) const override {
        for (size_t i = 0; i < range_boundaries.size(); ++i) {
            if (key <= range_boundaries[i]) {
                return static_cast<int>(i);
            }
        }
        return shard_count - 1;
    }
    
    std::string GetStrategyName() const override {
        return "Range Sharding";
    }
    
    void AnalyzeDistribution(const std::vector<uint21_t>& keys, int shard_count) const override {
        std::vector<int> distribution(shard_count, 0);
        
        for (uint21_t key : keys) {
            int shard = GetShardIndex(key, shard_count);
            distribution[shard]++;
        }
        
        std::cout << "\n📊 " << GetStrategyName() << " 분포 분석:" << std::endl;
        for (int i = 0; i < shard_count; ++i) {
            float percentage = (float)distribution[i] / keys.size() * 100;
            uint21_t range_start = (i == 0) ? 0 : range_boundaries[i-1] + 1;
            uint21_t range_end = range_boundaries[i];
            
            std::cout << "샤드 " << i << " (" << range_start << "-" << range_end << "): " 
                      << distribution[i] << " 키 (" << percentage << "%)" << std::endl;
        }
    }
};

// 3️⃣ 컨시스턴트 해싱 (동적 확장에 최적)
class ConsistentHashing : public ShardingStrategy {
private:
    struct VirtualNode {
        uint21_t hash_value;
        int shard_index;
    };
    
    std::vector<VirtualNode> ring;
    int virtual_nodes_per_shard;
    
public:
    ConsistentHashing(int shard_count, int virtual_nodes = 150) 
        : virtual_nodes_per_shard(virtual_nodes) {
        
        ring.reserve(shard_count * virtual_nodes_per_shard);
        
        for (int shard = 0; shard < shard_count; ++shard) {
            for (int vnode = 0; vnode < virtual_nodes_per_shard; ++vnode) {
                std::string vnode_key = "shard_" + std::to_string(shard) + "_vnode_" + std::to_string(vnode);
                uint21_t hash_val = std::hash<std::string>{}(vnode_key);
                ring.push_back({hash_val, shard});
            }
        }
        
        // 해시값 순으로 정렬
        std::sort(ring.begin(), ring.end(), 
                  [](const VirtualNode& a, const VirtualNode& b) {
                      return a.hash_value < b.hash_value;
                  });
    }
    
    int GetShardIndex(uint21_t key, int shard_count) const override {
        uint21_t key_hash = std::hash<uint21_t>{}(key);
        
        // 링에서 key_hash보다 큰 첫 번째 가상 노드 찾기
        auto it = std::lower_bound(ring.begin(), ring.end(), key_hash,
                                   [](const VirtualNode& node, uint21_t hash) {
                                       return node.hash_value < hash;
                                   });
        
        if (it == ring.end()) {
            // 링의 끝에 도달하면 첫 번째 노드로 돌아감
            return ring[0].shard_index;
        }
        
        return it->shard_index;
    }
    
    std::string GetStrategyName() const override {
        return "Consistent Hashing";
    }
    
    void AnalyzeDistribution(const std::vector<uint21_t>& keys, int shard_count) const override {
        std::vector<int> distribution(shard_count, 0);
        
        for (uint21_t key : keys) {
            int shard = GetShardIndex(key, shard_count);
            distribution[shard]++;
        }
        
        std::cout << "\n📊 " << GetStrategyName() << " 분포 분석:" << std::endl;
        for (int i = 0; i < shard_count; ++i) {
            float percentage = (float)distribution[i] / keys.size() * 100;
            std::cout << "샤드 " << i << ": " << distribution[i] 
                      << " 키 (" << percentage << "%)" << std::endl;
        }
        
        // 가상 노드 효과 분석
        std::cout << "가상 노드 수: " << virtual_nodes_per_shard << "개/샤드" << std::endl;
        std::cout << "총 가상 노드: " << ring.size() << "개" << std::endl;
    }
};

// 🎮 게임별 최적 샤딩 전략
class GameShardingAnalyzer {
public:
    void AnalyzeShardingStrategies() {
        std::cout << "🎮 게임 데이터 샤딩 전략 비교 분석" << std::endl;
        std::cout << "===========================================" << std::endl;
        
        // 게임 사용자 ID 패턴 시뮬레이션
        std::vector<uint21_t> user_ids;
        
        // 1) 순차적 가입 (초기 유저)
        for (uint21_t i = 1; i <= 50000; ++i) {
            user_ids.push_back(i);
        }
        
        // 2) 랜덤 가입 (중후기 유저)
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint21_t> dis(100000, 999999);
        for (int i = 0; i < 30000; ++i) {
            user_ids.push_back(dis(gen));
        }
        
        // 3) 클러스터된 가입 (이벤트 기간)
        for (uint21_t base = 1000000; base <= 1010000; base += 100) {
            for (int i = 0; i < 20; ++i) {
                user_ids.push_back(base + i);
            }
        }
        
        int shard_count = 8;
        
        // 전략별 분석
        std::unique_ptr<ShardingStrategy> hash_strategy = std::make_unique<HashSharding>();
        hash_strategy->AnalyzeDistribution(user_ids, shard_count);
        
        std::vector<uint21_t> range_boundaries = {125000, 250000, 375000, 500000, 625000, 750000, 875000, UINT21_MAX};
        std::unique_ptr<ShardingStrategy> range_strategy = std::make_unique<RangeSharding>(range_boundaries);
        range_strategy->AnalyzeDistribution(user_ids, shard_count);
        
        std::unique_ptr<ShardingStrategy> consistent_strategy = std::make_unique<ConsistentHashing>(shard_count);
        consistent_strategy->AnalyzeDistribution(user_ids, shard_count);
        
        PrintRecommendations();
    }
    
private:
    void PrintRecommendations() {
        std::cout << "\n🎯 게임 서버 샤딩 전략 추천:" << std::endl;
        std::cout << "\n1️⃣ Hash Sharding - 추천 ⭐⭐⭐⭐⭐" << std::endl;
        std::cout << "   ✅ 장점: 균등 분산, 단순 구현, 빠른 성능" << std::endl;
        std::cout << "   ❌ 단점: 샤드 추가 시 리샤딩 필요" << std::endl;
        std::cout << "   🎮 적합한 경우: 안정적인 유저 수, 단순한 쿼리" << std::endl;
        
        std::cout << "\n2️⃣ Range Sharding - 부분 추천 ⭐⭐⭐" << std::endl;
        std::cout << "   ✅ 장점: 범위 쿼리 최적화, 샤드 추가 용이" << std::endl;
        std::cout << "   ❌ 단점: 핫스팟 발생 가능, 불균등 분산" << std::endl;
        std::cout << "   🎮 적합한 경우: 시간순 데이터, 지역별 서비스" << std::endl;
        
        std::cout << "\n3️⃣ Consistent Hashing - 고급 추천 ⭐⭐⭐⭐" << std::endl;
        std::cout << "   ✅ 장점: 동적 확장, 최소한의 데이터 이동" << std::endl;
        std::cout << "   ❌ 단점: 복잡한 구현, 약간의 성능 오버헤드" << std::endl;
        std::cout << "   🎮 적합한 경우: 급성장 서비스, 글로벌 확장" << std::endl;
    }
};
```

### **1.2 실제 MySQL 샤딩 구현**

```cpp
#include <mysql++/mysql++.h>
#include <vector>
#include <memory>
#include <thread>
#include <future>

// 🎯 MySQL 샤드 클러스터 관리자
class MySQLShardCluster {
private:
    struct ShardInfo {
        std::string host;
        int port;
        std::string database;
        std::string username;
        std::string password;
        std::unique_ptr<mysqlpp::Connection> read_conn;
        std::unique_ptr<mysqlpp::Connection> write_conn;
        bool is_healthy;
    };
    
    std::vector<ShardInfo> shards_;
    std::unique_ptr<ShardingStrategy> sharding_strategy_;
    
public:
    MySQLShardCluster() {
        // 해시 기반 샤딩을 기본값으로 사용
        sharding_strategy_ = std::make_unique<HashSharding>();
    }
    
    bool AddShard(const std::string& host, int port, const std::string& database,
                  const std::string& username, const std::string& password) {
        ShardInfo shard;
        shard.host = host;
        shard.port = port;
        shard.database = database;
        shard.username = username;
        shard.password = password;
        
        // 읽기 전용 연결 (슬레이브)
        shard.read_conn = std::make_unique<mysqlpp::Connection>();
        if (!shard.read_conn->connect(database.c_str(), host.c_str(), 
                                      username.c_str(), password.c_str(), port)) {
            std::cerr << "❌ 읽기 연결 실패: " << host << ":" << port << std::endl;
            return false;
        }
        
        // 쓰기 전용 연결 (마스터)
        shard.write_conn = std::make_unique<mysqlpp::Connection>();
        if (!shard.write_conn->connect(database.c_str(), host.c_str(), 
                                       username.c_str(), password.c_str(), port)) {
            std::cerr << "❌ 쓰기 연결 실패: " << host << ":" << port << std::endl;
            return false;
        }
        
        shard.is_healthy = true;
        shards_.push_back(std::move(shard));
        
        std::cout << "✅ 샤드 추가 완료: " << host << ":" << port << std::endl;
        return true;
    }
    
    // 🎮 플레이어 데이터 샤드별 저장
    bool SavePlayerData(uint21_t player_id, const std::string& player_name, 
                       int level, int21_t experience, const std::string& location_data) {
        int shard_index = sharding_strategy_->GetShardIndex(player_id, shards_.size());
        auto& shard = shards_[shard_index];
        
        if (!shard.is_healthy) {
            std::cerr << "❌ 샤드 " << shard_index << " 비정상 상태" << std::endl;
            return false;
        }
        
        try {
            mysqlpp::Query query = shard.write_conn->query();
            query << "INSERT INTO players (player_id, player_name, level, experience, location_data, last_updated) "
                  << "VALUES (" << player_id << ", " << mysqlpp::quote << player_name 
                  << ", " << level << ", " << experience << ", " << mysqlpp::quote << location_data 
                  << ", NOW()) "
                  << "ON DUPLICATE KEY UPDATE "
                  << "player_name = VALUES(player_name), "
                  << "level = VALUES(level), "
                  << "experience = VALUES(experience), "
                  << "location_data = VALUES(location_data), "
                  << "last_updated = NOW()";
            
            query.execute();
            
            std::cout << "✅ 플레이어 " << player_id << " 데이터 저장 완료 (샤드 " << shard_index << ")" << std::endl;
            return true;
            
        } catch (const mysqlpp::Exception& e) {
            std::cerr << "❌ DB 오류: " << e.what() << std::endl;
            return false;
        }
    }
    
    // 🎮 플레이어 데이터 샤드에서 로드
    std::optional<PlayerData> LoadPlayerData(uint21_t player_id) {
        int shard_index = sharding_strategy_->GetShardIndex(player_id, shards_.size());
        auto& shard = shards_[shard_index];
        
        if (!shard.is_healthy) {
            std::cerr << "❌ 샤드 " << shard_index << " 비정상 상태" << std::endl;
            return std::nullopt;
        }
        
        try {
            mysqlpp::Query query = shard.read_conn->query();
            query << "SELECT player_id, player_name, level, experience, location_data, last_updated "
                  << "FROM players WHERE player_id = " << player_id;
            
            mysqlpp::StoreQueryResult result = query.store();
            
            if (result.empty()) {
                std::cout << "⚠️ 플레이어 " << player_id << " 데이터 없음" << std::endl;
                return std::nullopt;
            }
            
            const mysqlpp::Row& row = result[0];
            PlayerData data;
            data.player_id = row["player_id"];
            data.player_name = std::string(row["player_name"]);
            data.level = row["level"];
            data.experience = row["experience"];
            data.location_data = std::string(row["location_data"]);
            
            std::cout << "✅ 플레이어 " << player_id << " 데이터 로드 완료 (샤드 " << shard_index << ")" << std::endl;
            return data;
            
        } catch (const mysqlpp::Exception& e) {
            std::cerr << "❌ DB 오류: " << e.what() << std::endl;
            return std::nullopt;
        }
    }
    
    // 🔍 전체 샤드에서 데이터 검색 (비효율적이지만 필요한 경우)
    std::vector<PlayerData> SearchPlayersByName(const std::string& name_pattern, int limit = 100) {
        std::vector<std::future<std::vector<PlayerData>>> futures;
        
        // 모든 샤드에서 병렬 검색
        for (size_t i = 0; i < shards_.size(); ++i) {
            futures.push_back(std::async(std::launch::async, [this, i, &name_pattern, limit]() {
                return SearchInShard(i, name_pattern, limit);
            }));
        }
        
        std::vector<PlayerData> all_results;
        for (auto& future : futures) {
            auto shard_results = future.get();
            all_results.insert(all_results.end(), shard_results.begin(), shard_results.end());
        }
        
        // 결과 제한
        if (all_results.size() > limit) {
            all_results.resize(limit);
        }
        
        std::cout << "🔍 전체 검색 완료: " << all_results.size() << "건 발견" << std::endl;
        return all_results;
    }
    
    // 📊 샤드 상태 모니터링
    void PrintShardStatus() {
        std::cout << "\n📊 샤드 클러스터 상태:" << std::endl;
        std::cout << "================================" << std::endl;
        
        for (size_t i = 0; i < shards_.size(); ++i) {
            const auto& shard = shards_[i];
            std::cout << "샤드 " << i << ": " << shard.host << ":" << shard.port 
                      << " [" << (shard.is_healthy ? "정상" : "비정상") << "]" << std::endl;
            
            // 간단한 헬스체크
            try {
                mysqlpp::Query query = shard.read_conn->query();
                query << "SELECT COUNT(*) as player_count FROM players";
                mysqlpp::StoreQueryResult result = query.store();
                
                if (!result.empty()) {
                    int player_count = result[0]["player_count"];
                    std::cout << "  플레이어 수: " << player_count << "명" << std::endl;
                }
            } catch (const mysqlpp::Exception& e) {
                std::cout << "  ❌ 헬스체크 실패: " << e.what() << std::endl;
                shards_[i].is_healthy = false;
            }
        }
    }

private:
    struct PlayerData {
        uint21_t player_id;
        std::string player_name;
        int level;
        int21_t experience;
        std::string location_data;
    };
    
    std::vector<PlayerData> SearchInShard(size_t shard_index, const std::string& name_pattern, int limit) {
        std::vector<PlayerData> results;
        
        if (!shards_[shard_index].is_healthy) {
            return results;
        }
        
        try {
            mysqlpp::Query query = shards_[shard_index].read_conn->query();
            query << "SELECT player_id, player_name, level, experience, location_data "
                  << "FROM players WHERE player_name LIKE " << mysqlpp::quote << ("%" + name_pattern + "%")
                  << " LIMIT " << limit;
            
            mysqlpp::StoreQueryResult result = query.store();
            
            for (const auto& row : result) {
                PlayerData data;
                data.player_id = row["player_id"];
                data.player_name = std::string(row["player_name"]);
                data.level = row["level"];
                data.experience = row["experience"];
                data.location_data = std::string(row["location_data"]);
                results.push_back(data);
            }
            
        } catch (const mysqlpp::Exception& e) {
            std::cerr << "❌ 샤드 " << shard_index << " 검색 오류: " << e.what() << std::endl;
        }
        
        return results;
    }
};

// 🎮 게임 서버에서 사용 예제
void DemoShardedGameServer() {
    std::cout << "🎮 샤드된 게임 서버 데모" << std::endl;
    
    MySQLShardCluster cluster;
    
    // 샤드 추가 (실제로는 설정 파일에서 로드)
    cluster.AddShard("shard1.game.com", 3306, "gamedb_shard1", "game_user", "password123");
    cluster.AddShard("shard2.game.com", 3306, "gamedb_shard2", "game_user", "password123");
    cluster.AddShard("shard3.game.com", 3306, "gamedb_shard3", "game_user", "password123");
    cluster.AddShard("shard4.game.com", 3306, "gamedb_shard4", "game_user", "password123");
    
    // 플레이어 데이터 저장 (각기 다른 샤드에 분산됨)
    cluster.SavePlayerData(12345, "DragonSlayer", 50, 2500000, "{\"map\":1, \"x\":100, \"y\":200}");
    cluster.SavePlayerData(67890, "MagicMaster", 35, 1200000, "{\"map\":2, \"x\":300, \"y\":400}");
    cluster.SavePlayerData(11111, "SwordKing", 60, 5000000, "{\"map\":1, \"x\":150, \"y\":250}");
    
    // 플레이어 데이터 로드
    auto player_data = cluster.LoadPlayerData(12345);
    if (player_data.has_value()) {
        std::cout << "로드된 플레이어: " << player_data->player_name 
                  << " (레벨 " << player_data->level << ")" << std::endl;
    }
    
    // 이름으로 검색 (모든 샤드 검색)
    auto search_results = cluster.SearchPlayersByName("Dragon", 10);
    std::cout << "검색 결과: " << search_results.size() << "건" << std::endl;
    
    // 클러스터 상태 확인
    cluster.PrintShardStatus();
}
```

---

## 🏗️ 2. 분산 데이터베이스 관리 (고가용성)

### **2.1 MySQL InnoDB 클러스터 구축**

```bash
#!/bin/bash
# mysql_cluster_setup.sh - MySQL InnoDB 클러스터 자동 구축 스크립트

set -e

echo "🏗️ MySQL InnoDB 클러스터 구축 시작"

# 클러스터 구성 정보
CLUSTER_NODES=(
    "mysql-node1:3306"
    "mysql-node2:3306" 
    "mysql-node3:3306"
)
CLUSTER_NAME="GameServerCluster"
MYSQL_ROOT_PASSWORD="cluster_password_123"

# 1단계: 각 노드에 MySQL 설치 및 구성
setup_mysql_node() {
    local node_host=$1
    local node_id=$2
    
    echo "📦 MySQL 노드 $node_id 설정: $node_host"
    
    # MySQL 8.0 설치 (Ubuntu 기준)
    ssh root@$node_host << EOF
        apt update
        apt install -y mysql-server mysql-shell
        
        # MySQL 설정 파일 생성
        cat > /etc/mysql/mysql.conf.d/cluster.cnf << EOL
[mysqld]
# 클러스터 기본 설정
server-id = $node_id
gtid_mode = ON
enforce_gtid_consistency = ON
binlog_checksum = NONE
log_bin = binlog
log_slave_updates = ON
binlog_format = ROW
master_info_repository = TABLE
relay_log_info_repository = TABLE
transaction_write_set_extraction = XXHASH64

# 성능 최적화
innodb_buffer_pool_size = 8G
innodb_log_file_size = 1G
innodb_flush_log_at_trx_commit = 1
innodb_doublewrite = 1

# 네트워킹
bind-address = 0.0.0.0
max_connections = 10000
max_connect_errors = 999999

# 레플리케이션 최적화
slave_parallel_workers = 8
slave_parallel_type = LOGICAL_CLOCK
slave_preserve_commit_order = 1
EOL

        # MySQL 재시작
        systemctl restart mysql
        
        # 루트 패스워드 설정
        mysql -e "ALTER USER 'root'@'localhost' IDENTIFIED BY '$MYSQL_ROOT_PASSWORD';"
        
        # 클러스터 사용자 생성
        mysql -uroot -p$MYSQL_ROOT_PASSWORD -e "
            CREATE USER 'cluster_admin'@'%' IDENTIFIED BY '$MYSQL_ROOT_PASSWORD';
            GRANT ALL PRIVILEGES ON *.* TO 'cluster_admin'@'%' WITH GRANT OPTION;
            FLUSH PRIVILEGES;
        "
EOF
    
    echo "✅ 노드 $node_id 설정 완료"
}

# 2단계: 클러스터 초기화
initialize_cluster() {
    local primary_node=${CLUSTER_NODES[0]%:*}
    
    echo "🎯 클러스터 초기화: $primary_node"
    
    # 첫 번째 노드에서 클러스터 초기화
    ssh root@$primary_node << EOF
        mysqlsh cluster_admin@localhost:3306 --password=$MYSQL_ROOT_PASSWORD << MYSQLSH
            dba.configureInstance();
            var cluster = dba.createCluster('$CLUSTER_NAME', {gtidSetIsComplete: true});
            cluster.status();
MYSQLSH
EOF
}

# 3단계: 나머지 노드들을 클러스터에 추가
add_cluster_nodes() {
    local primary_node=${CLUSTER_NODES[0]%:*}
    
    echo "🔗 클러스터 노드 추가"
    
    for i in "${!CLUSTER_NODES[@]}"; do
        if [ $i -eq 0 ]; then
            continue  # 첫 번째 노드는 이미 초기화됨
        fi
        
        local node_host=${CLUSTER_NODES[$i]%:*}
        echo "추가 중: $node_host"
        
        # 노드 준비
        ssh root@$node_host << EOF
            mysqlsh cluster_admin@localhost:3306 --password=$MYSQL_ROOT_PASSWORD << MYSQLSH
                dba.configureInstance();
MYSQLSH
EOF
        
        # 클러스터에 추가
        ssh root@$primary_node << EOF
            mysqlsh cluster_admin@localhost:3306 --password=$MYSQL_ROOT_PASSWORD << MYSQLSH
                var cluster = dba.getCluster('$CLUSTER_NAME');
                cluster.addInstance('cluster_admin@$node_host:3306', {password: '$MYSQL_ROOT_PASSWORD'});
                cluster.status();
MYSQLSH
EOF
    done
}

# 4단계: MySQL Router 설정 (로드 밸런싱)
setup_mysql_router() {
    local router_host="mysql-router"
    local primary_node=${CLUSTER_NODES[0]%:*}
    
    echo "🔄 MySQL Router 설정: $router_host"
    
    ssh root@$router_host << EOF
        apt update
        apt install -y mysql-router
        
        # Router 부트스트랩
        mysqlrouter --bootstrap cluster_admin@$primary_node:3306 \
                   --user=mysqlrouter \
                   --password=$MYSQL_ROOT_PASSWORD \
                   --directory=/etc/mysqlrouter \
                   --conf-use-sockets
        
        # Router 서비스 시작
        systemctl enable mysqlrouter
        systemctl start mysqlrouter
        
        echo "✅ MySQL Router 설정 완료"
        echo "읽기/쓰기 포트: 6446"
        echo "읽기 전용 포트: 6447"
EOF
}

# 5단계: 클러스터 상태 확인
verify_cluster() {
    local primary_node=${CLUSTER_NODES[0]%:*}
    
    echo "🔍 클러스터 상태 확인"
    
    ssh root@$primary_node << EOF
        mysqlsh cluster_admin@localhost:3306 --password=$MYSQL_ROOT_PASSWORD << MYSQLSH
            var cluster = dba.getCluster('$CLUSTER_NAME');
            cluster.status();
            cluster.describe();
MYSQLSH
EOF
}

# 메인 실행 흐름
main() {
    echo "🚀 MySQL InnoDB 클러스터 구축 시작"
    echo "클러스터 이름: $CLUSTER_NAME"
    echo "노드 수: ${#CLUSTER_NODES[@]}"
    
    # 각 노드 설정
    for i in "${!CLUSTER_NODES[@]}"; do
        local node_host=${CLUSTER_NODES[$i]%:*}
        local node_id=$((i + 1))
        setup_mysql_node $node_host $node_id
    done
    
    # 클러스터 구성
    initialize_cluster
    add_cluster_nodes
    setup_mysql_router
    verify_cluster
    
    echo "🎉 MySQL InnoDB 클러스터 구축 완료!"
    echo "💡 연결 정보:"
    echo "  - 읽기/쓰기: mysql-router:6446"
    echo "  - 읽기 전용: mysql-router:6447"
    echo "  - 사용자: cluster_admin"
    echo "  - 패스워드: $MYSQL_ROOT_PASSWORD"
}

# 스크립트 실행
main "$@"
```

### **2.2 고가용성 모니터링 시스템**

```cpp
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <future>

// 🏥 MySQL 클러스터 헬스 모니터링 시스템
class MySQLClusterMonitor {
private:
    struct NodeHealth {
        std::string host;
        int port;
        std::string status;  // "ONLINE", "OFFLINE", "RECOVERING"
        int21_t last_check_time;
        int consecutive_failures;
        float cpu_usage;
        float memory_usage;
        int active_connections;
        float replication_lag_seconds;
        bool is_primary;
    };
    
    std::vector<NodeHealth> nodes_;
    std::atomic<bool> monitoring_active_{true};
    std::thread monitoring_thread_;
    
    // 알람 임계값
    const int MAX_CONSECUTIVE_FAILURES = 3;
    const float CPU_WARNING_THRESHOLD = 80.0f;
    const float MEMORY_WARNING_THRESHOLD = 85.0f;
    const float REPLICATION_LAG_WARNING = 5.0f;  // 5초
    
public:
    MySQLClusterMonitor() {
        // 모니터링 스레드 시작
        monitoring_thread_ = std::thread(&MySQLClusterMonitor::MonitoringLoop, this);
    }
    
    ~MySQLClusterMonitor() {
        monitoring_active_ = false;
        if (monitoring_thread_.joinable()) {
            monitoring_thread_.join();
        }
    }
    
    void AddNode(const std::string& host, int port, bool is_primary = false) {
        NodeHealth node;
        node.host = host;
        node.port = port;
        node.status = "UNKNOWN";
        node.last_check_time = 0;
        node.consecutive_failures = 0;
        node.cpu_usage = 0.0f;
        node.memory_usage = 0.0f;
        node.active_connections = 0;
        node.replication_lag_seconds = 0.0f;
        node.is_primary = is_primary;
        
        nodes_.push_back(node);
        std::cout << "📡 모니터링 노드 추가: " << host << ":" << port 
                  << (is_primary ? " [PRIMARY]" : " [SECONDARY]") << std::endl;
    }
    
private:
    void MonitoringLoop() {
        while (monitoring_active_) {
            auto start_time = std::chrono::steady_clock::now();
            
            // 모든 노드 병렬 헬스체크
            std::vector<std::future<void>> check_futures;
            for (size_t i = 0; i < nodes_.size(); ++i) {
                check_futures.push_back(
                    std::async(std::launch::async, &MySQLClusterMonitor::CheckNodeHealth, this, i)
                );
            }
            
            // 모든 체크 완료 대기
            for (auto& future : check_futures) {
                future.wait();
            }
            
            // 클러스터 전체 상태 분석
            AnalyzeClusterHealth();
            
            // 30초마다 체크
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            auto sleep_duration = std::chrono::seconds(30) - elapsed;
            if (sleep_duration > std::chrono::seconds(0)) {
                std::this_thread::sleep_for(sleep_duration);
            }
        }
    }
    
    void CheckNodeHealth(size_t node_index) {
        auto& node = nodes_[node_index];
        auto current_time = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        
        try {
            // MySQL 연결 테스트
            auto connection = CreateConnection(node.host, node.port);
            if (!connection) {
                throw std::runtime_error("연결 실패");
            }
            
            // 기본 상태 체크
            node.status = QueryNodeStatus(connection.get());
            node.active_connections = QueryActiveConnections(connection.get());
            node.cpu_usage = QueryCpuUsage(connection.get());
            node.memory_usage = QueryMemoryUsage(connection.get());
            
            // 레플리케이션 지연 체크 (Primary가 아닌 경우)
            if (!node.is_primary) {
                node.replication_lag_seconds = QueryReplicationLag(connection.get());
            }
            
            node.consecutive_failures = 0;  // 성공 시 실패 카운터 리셋
            node.last_check_time = current_time;
            
        } catch (const std::exception& e) {
            node.status = "OFFLINE";
            node.consecutive_failures++;
            
            std::cerr << "❌ 노드 헬스체크 실패: " << node.host << ":" << node.port 
                      << " (" << e.what() << ")" << std::endl;
            
            // 연속 실패 시 알람
            if (node.consecutive_failures >= MAX_CONSECUTIVE_FAILURES) {
                TriggerNodeFailureAlarm(node);
            }
        }
    }
    
    std::unique_ptr<mysqlpp::Connection> CreateConnection(const std::string& host, int port) {
        auto conn = std::make_unique<mysqlpp::Connection>();
        
        // 타임아웃 설정 (5초)
        if (!conn->connect("information_schema", host.c_str(), "monitor_user", "monitor_pass", port)) {
            return nullptr;
        }
        
        return conn;
    }
    
    std::string QueryNodeStatus(mysqlpp::Connection* conn) {
        mysqlpp::Query query = conn->query();
        query << "SELECT MEMBER_STATE FROM performance_schema.replication_group_members "
              << "WHERE MEMBER_HOST = @@hostname";
        
        mysqlpp::StoreQueryResult result = query.store();
        if (!result.empty()) {
            return std::string(result[0]["MEMBER_STATE"]);
        }
        
        return "STANDALONE";  // 클러스터에 속하지 않은 단독 서버
    }
    
    int QueryActiveConnections(mysqlpp::Connection* conn) {
        mysqlpp::Query query = conn->query();
        query << "SELECT COUNT(*) as conn_count FROM information_schema.processlist "
              << "WHERE COMMAND != 'Sleep'";
        
        mysqlpp::StoreQueryResult result = query.store();
        if (!result.empty()) {
            return result[0]["conn_count"];
        }
        
        return 0;
    }
    
    float QueryCpuUsage(mysqlpp::Connection* conn) {
        // MySQL에서 직접 CPU 사용률을 얻기는 어려우므로
        // 시스템 모니터링 도구 연동이나 MySQL의 성능 스키마 활용
        mysqlpp::Query query = conn->query();
        query << "SELECT VARIABLE_VALUE FROM performance_schema.global_status "
              << "WHERE VARIABLE_NAME = 'Threads_running'";
        
        mysqlpp::StoreQueryResult result = query.store();
        if (!result.empty()) {
            int running_threads = result[0]["VARIABLE_VALUE"];
            // 러닝 스레드 수를 CPU 사용률의 대략적 지표로 사용
            return std::min(100.0f, running_threads * 5.0f);
        }
        
        return 0.0f;
    }
    
    float QueryMemoryUsage(mysqlpp::Connection* conn) {
        mysqlpp::Query query = conn->query();
        query << "SELECT "
              << "(SELECT VARIABLE_VALUE FROM performance_schema.global_status WHERE VARIABLE_NAME = 'Innodb_buffer_pool_bytes_data') / "
              << "(SELECT VARIABLE_VALUE FROM performance_schema.global_variables WHERE VARIABLE_NAME = 'innodb_buffer_pool_size') * 100 "
              << "as buffer_pool_usage";
        
        mysqlpp::StoreQueryResult result = query.store();
        if (!result.empty()) {
            return result[0]["buffer_pool_usage"];
        }
        
        return 0.0f;
    }
    
    float QueryReplicationLag(mysqlpp::Connection* conn) {
        mysqlpp::Query query = conn->query();
        query << "SELECT COUNT_TRANSACTIONS_IN_QUEUE as lag_count "
              << "FROM performance_schema.replication_group_member_stats "
              << "WHERE MEMBER_ID = @@server_uuid";
        
        mysqlpp::StoreQueryResult result = query.store();
        if (!result.empty()) {
            int lag_count = result[0]["lag_count"];
            // 큐에 있는 트랜잭션 수를 초 단위 지연으로 근사화
            return lag_count * 0.1f;
        }
        
        return 0.0f;
    }
    
    void AnalyzeClusterHealth() {
        int online_nodes = 0;
        int primary_nodes = 0;
        float total_cpu = 0.0f;
        float total_memory = 0.0f;
        float max_replication_lag = 0.0f;
        
        std::cout << "\n📊 클러스터 헬스 리포트 " 
                  << "(" << GetCurrentTimeString() << ")" << std::endl;
        std::cout << "==========================================" << std::endl;
        
        for (const auto& node : nodes_) {
            // 노드별 상태 출력
            std::string status_indicator = "❌";
            if (node.status == "ONLINE") {
                status_indicator = "✅";
                online_nodes++;
            } else if (node.status == "RECOVERING") {
                status_indicator = "⚠️";
            }
            
            std::cout << status_indicator << " " << node.host << ":" << node.port;
            if (node.is_primary) {
                std::cout << " [PRIMARY]";
                if (node.status == "ONLINE") primary_nodes++;
            }
            std::cout << " - " << node.status << std::endl;
            
            std::cout << "   CPU: " << node.cpu_usage << "% | "
                      << "메모리: " << node.memory_usage << "% | "
                      << "연결: " << node.active_connections;
            
            if (!node.is_primary && node.replication_lag_seconds > 0) {
                std::cout << " | 복제지연: " << node.replication_lag_seconds << "초";
                max_replication_lag = std::max(max_replication_lag, node.replication_lag_seconds);
            }
            std::cout << std::endl;
            
            // 경고 체크
            if (node.cpu_usage > CPU_WARNING_THRESHOLD) {
                std::cout << "   ⚠️ CPU 사용률 높음 (" << node.cpu_usage << "%)" << std::endl;
            }
            if (node.memory_usage > MEMORY_WARNING_THRESHOLD) {
                std::cout << "   ⚠️ 메모리 사용률 높음 (" << node.memory_usage << "%)" << std::endl;
            }
            if (!node.is_primary && node.replication_lag_seconds > REPLICATION_LAG_WARNING) {
                std::cout << "   ⚠️ 복제 지연 심각 (" << node.replication_lag_seconds << "초)" << std::endl;
            }
            
            total_cpu += node.cpu_usage;
            total_memory += node.memory_usage;
        }
        
        // 클러스터 전체 상태 요약
        std::cout << "\n📈 클러스터 요약:" << std::endl;
        std::cout << "온라인 노드: " << online_nodes << "/" << nodes_.size() << std::endl;
        std::cout << "Primary 노드: " << primary_nodes << std::endl;
        std::cout << "평균 CPU: " << (total_cpu / nodes_.size()) << "%" << std::endl;
        std::cout << "평균 메모리: " << (total_memory / nodes_.size()) << "%" << std::endl;
        std::cout << "최대 복제지연: " << max_replication_lag << "초" << std::endl;
        
        // 클러스터 상태 판정
        if (primary_nodes == 0) {
            std::cout << "🚨 심각: Primary 노드 없음! 쓰기 불가!" << std::endl;
            TriggerClusterFailureAlarm();
        } else if (online_nodes < nodes_.size() / 2 + 1) {
            std::cout << "⚠️ 경고: 쿼럼 부족! 클러스터 불안정!" << std::endl;
        } else if (online_nodes == nodes_.size()) {
            std::cout << "✅ 정상: 모든 노드 온라인" << std::endl;
        }
    }
    
    void TriggerNodeFailureAlarm(const NodeHealth& node) {
        std::cout << "🚨 노드 장애 알람: " << node.host << ":" << node.port << std::endl;
        std::cout << "연속 실패 횟수: " << node.consecutive_failures << std::endl;
        
        // 실제 운영에서는 여기서 다음과 같은 작업 수행:
        // 1. Slack/이메일 알림 발송
        // 2. 장애 노드 자동 격리
        // 3. 백업 노드 자동 승격
        // 4. 로드밸런서에서 제외
    }
    
    void TriggerClusterFailureAlarm() {
        std::cout << "🚨🚨 클러스터 전체 장애 알람 🚨🚨" << std::endl;
        
        // 실제 운영에서는 긴급 대응 프로세스 실행:
        // 1. 온콜 엔지니어에게 즉시 연락
        // 2. 읽기 전용 모드로 전환
        // 3. 백업 클러스터로 트래픽 전환
        // 4. 장애 복구 절차 자동 시작
    }
    
    std::string GetCurrentTimeString() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }
    
public:
    void PrintClusterStatus() {
        AnalyzeClusterHealth();
    }
};

// 🎮 게임 서버용 모니터링 설정
void SetupGameServerMonitoring() {
    std::cout << "🎮 게임 서버 MySQL 클러스터 모니터링 시작" << std::endl;
    
    MySQLClusterMonitor monitor;
    
    // 클러스터 노드 등록
    monitor.AddNode("mysql-primary.game.com", 3306, true);   // Primary
    monitor.AddNode("mysql-secondary1.game.com", 3306);     // Secondary
    monitor.AddNode("mysql-secondary2.game.com", 3306);     // Secondary
    
    // 모니터링 실행 (실제로는 서비스로 백그라운드에서 실행)
    std::cout << "모니터링 시작됨. 30초마다 헬스체크 수행..." << std::endl;
    
    // 데모를 위해 10분간 실행
    std::this_thread::sleep_for(std::chrono::minutes(10));
}
```

---

## 💾 3. 백업 및 복구 자동화 (무중단 서비스)

### **3.1 실시간 백업 시스템**

```bash
#!/bin/bash
# mysql_backup_system.sh - 무중단 MySQL 백업 자동화

set -e

# 설정 변수
BACKUP_CONFIG_FILE="/etc/mysql-backup/config.conf"
BACKUP_BASE_DIR="/backup/mysql"
RETENTION_DAYS=30
COMPRESSION_LEVEL=6
PARALLEL_THREADS=8
ENCRYPTION_KEY_FILE="/etc/mysql-backup/encryption.key"

# 로깅 함수
log() {
    echo "[$(date +'%Y-%m-%d %H:%M:%S')] $1" | tee -a "$BACKUP_BASE_DIR/backup.log"
}

# 설정 파일 로드
load_config() {
    if [[ -f "$BACKUP_CONFIG_FILE" ]]; then
        source "$BACKUP_CONFIG_FILE"
        log "✅ 설정 파일 로드 완료: $BACKUP_CONFIG_FILE"
    else
        log "❌ 설정 파일 없음: $BACKUP_CONFIG_FILE"
        exit 1
    fi
}

# 🎯 핫 백업 (무중단 백업)
perform_hot_backup() {
    local backup_type=$1  # "full" 또는 "incremental"
    local timestamp=$(date +%Y%m%d_%H%M%S)
    local backup_dir="$BACKUP_BASE_DIR/$backup_type/$timestamp"
    
    log "🔥 핫 백업 시작: $backup_type"
    mkdir -p "$backup_dir"
    
    # XtraBackup을 사용한 무중단 백업
    if [[ "$backup_type" == "full" ]]; then
        log "전체 백업 수행 중..."
        
        xtrabackup \
            --backup \
            --target-dir="$backup_dir/data" \
            --host="$MYSQL_HOST" \
            --port="$MYSQL_PORT" \
            --user="$BACKUP_USER" \
            --password="$BACKUP_PASSWORD" \
            --parallel="$PARALLEL_THREADS" \
            --compress \
            --compress-threads="$PARALLEL_THREADS" \
            --encrypt=AES256 \
            --encrypt-key-file="$ENCRYPTION_KEY_FILE" \
            --stream=xbstream | \
        gzip -"$COMPRESSION_LEVEL" > "$backup_dir/backup.xbstream.gz"
        
        # 백업 메타데이터 저장
        echo "BACKUP_TYPE=full" > "$backup_dir/backup.info"
        echo "TIMESTAMP=$timestamp" >> "$backup_dir/backup.info"
        echo "MYSQL_HOST=$MYSQL_HOST" >> "$backup_dir/backup.info"
        echo "MYSQL_PORT=$MYSQL_PORT" >> "$backup_dir/backup.info"
        
    else
        # 증분 백업
        local last_full_backup=$(find "$BACKUP_BASE_DIR/full" -name "backup.info" -exec dirname {} \; | sort | tail -1)
        
        if [[ -z "$last_full_backup" ]]; then
            log "❌ 증분 백업을 위한 전체 백업을 찾을 수 없음"
            return 1
        fi
        
        log "증분 백업 수행 중 (기준: $last_full_backup)"
        
        xtrabackup \
            --backup \
            --target-dir="$backup_dir/data" \
            --incremental-basedir="$last_full_backup/data" \
            --host="$MYSQL_HOST" \
            --port="$MYSQL_PORT" \
            --user="$BACKUP_USER" \
            --password="$BACKUP_PASSWORD" \
            --parallel="$PARALLEL_THREADS" \
            --compress \
            --compress-threads="$PARALLEL_THREADS" \
            --encrypt=AES256 \
            --encrypt-key-file="$ENCRYPTION_KEY_FILE" \
            --stream=xbstream | \
        gzip -"$COMPRESSION_LEVEL" > "$backup_dir/backup.xbstream.gz"
        
        echo "BACKUP_TYPE=incremental" > "$backup_dir/backup.info"
        echo "TIMESTAMP=$timestamp" >> "$backup_dir/backup.info"
        echo "BASE_BACKUP=$last_full_backup" >> "$backup_dir/backup.info"
    fi
    
    # 백업 검증
    if verify_backup "$backup_dir"; then
        log "✅ 백업 성공: $backup_dir"
        
        # 원격 저장소에 업로드
        upload_to_remote_storage "$backup_dir"
        
        # 슬랙 알림
        send_backup_notification "SUCCESS" "$backup_type" "$backup_dir"
        
        return 0
    else
        log "❌ 백업 실패: $backup_dir"
        send_backup_notification "FAILED" "$backup_type" "$backup_dir"
        return 1
    fi
}

# 백업 검증
verify_backup() {
    local backup_dir=$1
    
    log "🔍 백업 검증 중: $backup_dir"
    
    # 파일 존재 확인
    if [[ ! -f "$backup_dir/backup.xbstream.gz" ]]; then
        log "❌ 백업 파일 없음"
        return 1
    fi
    
    # 파일 크기 확인 (최소 크기)
    local file_size=$(stat -c%s "$backup_dir/backup.xbstream.gz")
    local min_size=1048576  # 1MB (너무 작으면 백업 실패)
    
    if [[ $file_size -lt $min_size ]]; then
        log "❌ 백업 파일이 너무 작음: $file_size bytes"
        return 1
    fi
    
    # 압축 파일 무결성 확인
    if ! gzip -t "$backup_dir/backup.xbstream.gz"; then
        log "❌ 백업 파일 압축 손상"
        return 1
    fi
    
    # 테스트 복원 (샘플링)
    if [[ $(date +%u) == "7" ]]; then  # 일요일에만 테스트 복원
        log "🧪 테스트 복원 수행 중..."
        test_restore "$backup_dir"
    fi
    
    log "✅ 백업 검증 성공"
    return 0
}

# 테스트 복원
test_restore() {
    local backup_dir=$1
    local test_restore_dir="/tmp/mysql_test_restore_$$"
    
    mkdir -p "$test_restore_dir"
    
    # 백업 파일 압축 해제
    gunzip -c "$backup_dir/backup.xbstream.gz" | \
    xbstream -x -C "$test_restore_dir"
    
    # 백업 준비 (apply logs)
    xtrabackup \
        --prepare \
        --target-dir="$test_restore_dir" \
        --decrypt=AES256 \
        --decrypt-key-file="$ENCRYPTION_KEY_FILE"
    
    # 테스트 MySQL 인스턴스로 복원
    docker run -d \
        --name mysql_restore_test_$$ \
        -v "$test_restore_dir:/var/lib/mysql" \
        -e MYSQL_ROOT_PASSWORD=test_password \
        -p 13306:3306 \
        mysql:8.0
    
    # 테스트 연결 대기
    sleep 30
    
    # 데이터 무결성 확인
    if mysql -h 127.0.0.1 -P 13306 -uroot -ptest_password -e "SHOW DATABASES;" > /dev/null 2>&1; then
        log "✅ 테스트 복원 성공"
        
        # 샘플 테이블 데이터 확인
        local table_count=$(mysql -h 127.0.0.1 -P 13306 -uroot -ptest_password -e "
            SELECT COUNT(*) as count 
            FROM information_schema.tables 
            WHERE table_schema NOT IN ('information_schema', 'mysql', 'performance_schema', 'sys')
        " --batch --skip-column-names)
        
        log "복원된 사용자 테이블 수: $table_count"
        
        restore_success=true
    else
        log "❌ 테스트 복원 실패"
        restore_success=false
    fi
    
    # 정리
    docker stop mysql_restore_test_$$ && docker rm mysql_restore_test_$$
    rm -rf "$test_restore_dir"
    
    return $restore_success
}

# 원격 저장소 업로드 (AWS S3)
upload_to_remote_storage() {
    local backup_dir=$1
    local s3_bucket="game-mysql-backups"
    local s3_path="$(basename $backup_dir)"
    
    log "☁️ 원격 저장소 업로드 시작: $s3_path"
    
    # AWS S3에 업로드 (멀티파트 업로드로 대용량 파일 지원)
    aws s3 cp "$backup_dir/backup.xbstream.gz" \
        "s3://$s3_bucket/$s3_path/backup.xbstream.gz" \
        --storage-class STANDARD_IA \
        --server-side-encryption AES256
    
    aws s3 cp "$backup_dir/backup.info" \
        "s3://$s3_bucket/$s3_path/backup.info"
    
    # 업로드 검증
    if aws s3 ls "s3://$s3_bucket/$s3_path/backup.xbstream.gz" > /dev/null; then
        log "✅ 원격 저장소 업로드 성공"
        
        # 로컬 백업 파일 정리 (디스크 공간 절약)
        if [[ "$KEEP_LOCAL_BACKUP" != "true" ]]; then
            rm -f "$backup_dir/backup.xbstream.gz"
            log "🗑️ 로컬 백업 파일 삭제 (원격 저장소에 보관)"
        fi
    else
        log "❌ 원격 저장소 업로드 실패"
        return 1
    fi
}

# 슬랙 알림
send_backup_notification() {
    local status=$1
    local backup_type=$2
    local backup_dir=$3
    
    local color="good"
    local icon="✅"
    
    if [[ "$status" == "FAILED" ]]; then
        color="danger"
        icon="❌"
    fi
    
    local message="$icon MySQL 백업 $status
    타입: $backup_type
    경로: $backup_dir
    시간: $(date +'%Y-%m-%d %H:%M:%S')
    호스트: $(hostname)"
    
    curl -X POST -H 'Content-type: application/json' \
        --data "{
            \"attachments\": [{
                \"color\": \"$color\",
                \"text\": \"$message\"
            }]
        }" \
        "$SLACK_WEBHOOK_URL"
}

# 오래된 백업 정리
cleanup_old_backups() {
    log "🧹 오래된 백업 정리 시작 (보존 기간: $RETENTION_DAYS일)"
    
    # 로컬 백업 정리
    find "$BACKUP_BASE_DIR" -type d -name "*" -mtime +$RETENTION_DAYS -exec rm -rf {} \;
    
    # S3 백업 정리 (Lifecycle Policy로도 가능)
    aws s3api list-objects-v2 \
        --bucket "game-mysql-backups" \
        --query "Contents[?LastModified<='$(date -d "$RETENTION_DAYS days ago" --iso-8601)'].Key" \
        --output text | \
    while read -r key; do
        if [[ -n "$key" ]]; then
            aws s3 rm "s3://game-mysql-backups/$key"
            log "🗑️ S3 백업 삭제: $key"
        fi
    done
    
    log "✅ 백업 정리 완료"
}

# Point-in-Time 복구 준비
prepare_point_in_time_recovery() {
    local target_time=$1  # "2024-01-15 14:30:00"
    local recovery_dir="/tmp/mysql_pitr_$(date +%s)"
    
    log "⏰ Point-in-Time 복구 준비: $target_time"
    
    # 1. 대상 시간 이전의 최신 전체 백업 찾기
    local base_backup=$(find "$BACKUP_BASE_DIR/full" -name "backup.info" | \
        xargs grep -l "TIMESTAMP" | \
        while read info_file; do
            local backup_time=$(grep "TIMESTAMP" "$info_file" | cut -d'=' -f2)
            if [[ "$backup_time" < "$(date -d "$target_time" +%Y%m%d_%H%M%S)" ]]; then
                echo "$(dirname "$info_file"):$backup_time"
            fi
        done | sort -t: -k2 | tail -1 | cut -d: -f1)
    
    if [[ -z "$base_backup" ]]; then
        log "❌ 적절한 기본 백업을 찾을 수 없음"
        return 1
    fi
    
    log "기본 백업 사용: $base_backup"
    
    # 2. 필요한 증분 백업들 찾기
    local incremental_backups=$(find "$BACKUP_BASE_DIR/incremental" -name "backup.info" | \
        xargs grep -l "BASE_BACKUP.*$base_backup" | \
        while read info_file; do
            local backup_time=$(grep "TIMESTAMP" "$info_file" | cut -d'=' -f2)
            if [[ "$backup_time" < "$(date -d "$target_time" +%Y%m%d_%H%M%S)" ]]; then
                echo "$(dirname "$info_file"):$backup_time"
            fi
        done | sort -t: -k2)
    
    # 3. 복구 디렉토리 준비
    mkdir -p "$recovery_dir"
    
    # 4. 복구 스크립트 생성
    cat > "$recovery_dir/restore.sh" << EOF
#!/bin/bash
set -e

echo "🔄 Point-in-Time 복구 시작"
echo "대상 시간: $target_time"
echo "복구 디렉토리: $recovery_dir"

# 기본 백업 복원
echo "1. 기본 백업 복원: $base_backup"
gunzip -c "$base_backup/backup.xbstream.gz" | xbstream -x -C "$recovery_dir/data"

# 증분 백업들 순차 적용
EOF

    local backup_num=2
    for inc_backup in $incremental_backups; do
        local inc_dir=$(echo "$inc_backup" | cut -d: -f1)
        echo "echo \"$backup_num. 증분 백업 적용: $inc_dir\"" >> "$recovery_dir/restore.sh"
        echo "gunzip -c \"$inc_dir/backup.xbstream.gz\" | xbstream -x -C \"$recovery_dir/incremental_$backup_num\"" >> "$recovery_dir/restore.sh"
        echo "xtrabackup --prepare --target-dir=\"$recovery_dir/data\" --incremental-dir=\"$recovery_dir/incremental_$backup_num\"" >> "$recovery_dir/restore.sh"
        ((backup_num++))
    done
    
    cat >> "$recovery_dir/restore.sh" << EOF

# 최종 준비
echo "3. 최종 로그 적용"
xtrabackup --prepare --target-dir="$recovery_dir/data"

echo "4. Point-in-Time 복구 완료"
echo "복구된 데이터 위치: $recovery_dir/data"
echo "MySQL 시작 명령:"
echo "mysqld --datadir=$recovery_dir/data --port=13307 --socket=/tmp/mysql_pitr.sock"
EOF

    chmod +x "$recovery_dir/restore.sh"
    
    log "✅ Point-in-Time 복구 스크립트 준비 완료: $recovery_dir/restore.sh"
    log "복구 실행: bash $recovery_dir/restore.sh"
}

# 메인 백업 스케줄러
main() {
    log "🚀 MySQL 백업 시스템 시작"
    
    load_config
    
    case "${1:-auto}" in
        "full")
            perform_hot_backup "full"
            ;;
        "incremental")
            perform_hot_backup "incremental"
            ;;
        "cleanup")
            cleanup_old_backups
            ;;
        "pitr")
            if [[ -z "$2" ]]; then
                echo "사용법: $0 pitr '2024-01-15 14:30:00'"
                exit 1
            fi
            prepare_point_in_time_recovery "$2"
            ;;
        "auto")
            # 자동 스케줄 (cron 설정에 따라)
            local hour=$(date +%H)
            local day=$(date +%u)
            
            if [[ "$hour" == "02" && "$day" == "7" ]]; then
                # 일요일 새벽 2시 - 전체 백업
                perform_hot_backup "full"
            elif [[ "$hour" == "02" ]]; then
                # 매일 새벽 2시 - 증분 백업
                perform_hot_backup "incremental"
            elif [[ "$hour" == "06" ]]; then
                # 매일 오전 6시 - 정리 작업
                cleanup_old_backups
            fi
            ;;
        *)
            echo "사용법: $0 {full|incremental|cleanup|pitr|auto}"
            exit 1
            ;;
    esac
}

# 스크립트 실행
main "$@"
```

### **3.2 자동 복구 시스템**

```cpp
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <future>
#include <fstream>
#include <json/json.h>

// 🚨 MySQL 자동 복구 시스템
class MySQLAutoRecovery {
private:
    struct RecoveryPlan {
        std::string failure_type;
        std::vector<std::string> recovery_steps;
        int estimated_time_minutes;
        bool requires_manual_intervention;
    };
    
    std::vector<RecoveryPlan> recovery_plans_;
    std::atomic<bool> recovery_in_progress_{false};
    
public:
    MySQLAutoRecovery() {
        InitializeRecoveryPlans();
    }
    
    void InitializeRecoveryPlans() {
        // 1. 단일 노드 장애
        recovery_plans_.push_back({
            "SINGLE_NODE_FAILURE",
            {
                "장애 노드 격리",
                "로드밸런서에서 제거",
                "복제 재구성",
                "장애 노드 재시작 시도",
                "데이터 동기화 확인"
            },
            15,  // 15분 예상
            false
        });
        
        // 2. Primary 노드 장애
        recovery_plans_.push_back({
            "PRIMARY_NODE_FAILURE",
            {
                "Secondary 중 최신 노드 선택",
                "새 Primary로 승격",
                "모든 Secondary를 새 Primary로 재구성",
                "애플리케이션 연결 정보 업데이트",
                "장애 노드 복구 시 Secondary로 재추가"
            },
            5,   // 5분 예상 (자동 페일오버)
            false
        });
        
        // 3. 다중 노드 장애 (쿼럼 손실)
        recovery_plans_.push_back({
            "QUORUM_LOSS",
            {
                "클러스터 강제 쿼럼 설정",
                "생존 노드에서 읽기 전용 모드 활성화",
                "장애 노드들 순차 복구",
                "데이터 일관성 검증",
                "정상 클러스터 모드 복원"
            },
            60,  // 1시간 예상
            true  // 수동 개입 필요
        });
        
        // 4. 데이터 손상
        recovery_plans_.push_back({
            "DATA_CORRUPTION",
            {
                "손상된 테이블 격리",
                "Point-in-Time 복구 수행",
                "데이터 무결성 검증",
                "트랜잭션 로그 재적용",
                "애플리케이션 데이터 검증"
            },
            120, // 2시간 예상
            true
        });
        
        std::cout << "✅ 복구 플랜 초기화 완료: " << recovery_plans_.size() << "개 시나리오" << std::endl;
    }
    
    // 🚨 장애 감지 및 자동 복구 시작
    void HandleFailure(const std::string& failure_type, const Json::Value& context) {
        if (recovery_in_progress_) {
            std::cout << "⚠️ 복구 작업이 이미 진행 중입니다." << std::endl;
            return;
        }
        
        std::cout << "🚨 장애 감지: " << failure_type << std::endl;
        
        // 해당하는 복구 플랜 찾기
        auto plan_it = std::find_if(recovery_plans_.begin(), recovery_plans_.end(),
                                    [&failure_type](const RecoveryPlan& plan) {
                                        return plan.failure_type == failure_type;
                                    });
        
        if (plan_it == recovery_plans_.end()) {
            std::cout << "❌ 알 수 없는 장애 유형: " << failure_type << std::endl;
            NotifyOperators("UNKNOWN_FAILURE", "복구 플랜이 없는 장애 발생", context);
            return;
        }
        
        const RecoveryPlan& plan = *plan_it;
        
        if (plan.requires_manual_intervention) {
            std::cout << "⚠️ 수동 개입이 필요한 장애입니다." << std::endl;
            NotifyOperators("MANUAL_INTERVENTION_REQUIRED", 
                           "수동 복구 필요: " + failure_type, context);
            return;
        }
        
        // 자동 복구 시작
        recovery_in_progress_ = true;
        std::thread recovery_thread(&MySQLAutoRecovery::ExecuteRecoveryPlan, this, plan, context);
        recovery_thread.detach();
    }
    
private:
    void ExecuteRecoveryPlan(const RecoveryPlan& plan, const Json::Value& context) {
        auto start_time = std::chrono::steady_clock::now();
        
        std::cout << "🔧 자동 복구 시작: " << plan.failure_type << std::endl;
        std::cout << "예상 복구 시간: " << plan.estimated_time_minutes << "분" << std::endl;
        
        NotifyOperators("RECOVERY_STARTED", 
                       "자동 복구 시작: " + plan.failure_type, context);
        
        bool recovery_success = true;
        
        for (size_t step = 0; step < plan.recovery_steps.size(); ++step) {
            const std::string& step_description = plan.recovery_steps[step];
            
            std::cout << "단계 " << (step + 1) << "/" << plan.recovery_steps.size() 
                      << ": " << step_description << std::endl;
            
            if (!ExecuteRecoveryStep(plan.failure_type, step, step_description, context)) {
                std::cout << "❌ 복구 단계 실패: " << step_description << std::endl;
                recovery_success = false;
                break;
            }
            
            std::cout << "✅ 단계 완료: " << step_description << std::endl;
            
            // 단계 간 안전 대기 시간
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto elapsed_minutes = std::chrono::duration_cast<std::chrono::minutes>(end_time - start_time).count();
        
        if (recovery_success) {
            std::cout << "🎉 자동 복구 성공! 소요 시간: " << elapsed_minutes << "분" << std::endl;
            
            // 복구 후 검증
            if (VerifyRecovery(plan.failure_type, context)) {
                NotifyOperators("RECOVERY_COMPLETED", 
                               "자동 복구 성공: " + plan.failure_type, context);
            } else {
                std::cout << "⚠️ 복구 후 검증 실패" << std::endl;
                NotifyOperators("RECOVERY_VERIFICATION_FAILED", 
                               "복구 완료되었으나 검증 실패", context);
            }
        } else {
            std::cout << "❌ 자동 복구 실패" << std::endl;
            NotifyOperators("RECOVERY_FAILED", 
                           "자동 복구 실패: " + plan.failure_type, context);
        }
        
        recovery_in_progress_ = false;
    }
    
    bool ExecuteRecoveryStep(const std::string& failure_type, size_t step_index, 
                           const std::string& step_description, const Json::Value& context) {
        
        // 장애 유형과 단계에 따른 구체적인 복구 작업 수행
        if (failure_type == "SINGLE_NODE_FAILURE") {
            return ExecuteSingleNodeRecoveryStep(step_index, step_description, context);
        } else if (failure_type == "PRIMARY_NODE_FAILURE") {
            return ExecutePrimaryFailoverStep(step_index, step_description, context);
        } else if (failure_type == "DATA_CORRUPTION") {
            return ExecuteDataRecoveryStep(step_index, step_description, context);
        }
        
        return false;
    }
    
    bool ExecuteSingleNodeRecoveryStep(size_t step_index, const std::string& step_description, 
                                     const Json::Value& context) {
        std::string failed_node = context["failed_node"].asString();
        
        switch (step_index) {
            case 0: // "장애 노드 격리"
                return IsolateFailedNode(failed_node);
                
            case 1: // "로드밸런서에서 제거"
                return RemoveFromLoadBalancer(failed_node);
                
            case 2: // "복제 재구성"
                return ReconfigureReplication(failed_node);
                
            case 3: // "장애 노드 재시작 시도"
                return AttemptNodeRestart(failed_node);
                
            case 4: // "데이터 동기화 확인"
                return VerifyDataSync(failed_node);
                
            default:
                return false;
        }
    }
    
    bool ExecutePrimaryFailoverStep(size_t step_index, const std::string& step_description, 
                                  const Json::Value& context) {
        std::string failed_primary = context["failed_primary"].asString();
        
        switch (step_index) {
            case 0: // "Secondary 중 최신 노드 선택"
                return SelectNewPrimary(failed_primary);
                
            case 1: // "새 Primary로 승격"
                return PromoteNewPrimary();
                
            case 2: // "모든 Secondary를 새 Primary로 재구성"
                return ReconfigureSecondaries();
                
            case 3: // "애플리케이션 연결 정보 업데이트"
                return UpdateApplicationConnections();
                
            case 4: // "장애 노드 복구 시 Secondary로 재추가"
                return ScheduleFailedNodeRecovery(failed_primary);
                
            default:
                return false;
        }
    }
    
    bool ExecuteDataRecoveryStep(size_t step_index, const std::string& step_description, 
                               const Json::Value& context) {
        std::string corrupted_table = context["corrupted_table"].asString();
        std::string target_time = context.get("recovery_time", "").asString();
        
        switch (step_index) {
            case 0: // "손상된 테이블 격리"
                return IsolateCorruptedTable(corrupted_table);
                
            case 1: // "Point-in-Time 복구 수행"
                return PerformPointInTimeRecovery(target_time);
                
            case 2: // "데이터 무결성 검증"
                return VerifyDataIntegrity(corrupted_table);
                
            case 3: // "트랜잭션 로그 재적용"
                return ReplayTransactionLogs(target_time);
                
            case 4: // "애플리케이션 데이터 검증"
                return VerifyApplicationData(corrupted_table);
                
            default:
                return false;
        }
    }
    
    // 구체적인 복구 작업 함수들
    bool IsolateFailedNode(const std::string& node) {
        std::cout << "🔒 노드 격리 중: " << node << std::endl;
        
        // MySQL Router에서 노드 제거
        std::string command = "mysqlrouter_remove_node.sh " + node;
        int result = system(command.c_str());
        
        if (result == 0) {
            std::cout << "✅ 노드 격리 완료: " << node << std::endl;
            return true;
        } else {
            std::cout << "❌ 노드 격리 실패: " << node << std::endl;
            return false;
        }
    }
    
    bool RemoveFromLoadBalancer(const std::string& node) {
        std::cout << "⚖️ 로드밸런서에서 제거 중: " << node << std::endl;
        
        // HAProxy/Nginx 설정에서 노드 제거
        std::string command = "haproxy_remove_server.sh " + node;
        int result = system(command.c_str());
        
        return result == 0;
    }
    
    bool SelectNewPrimary(const std::string& failed_primary) {
        std::cout << "👑 새로운 Primary 선택 중..." << std::endl;
        
        // 가장 최신 데이터를 가진 Secondary 선택
        std::string command = "mysql_select_best_secondary.sh " + failed_primary;
        int result = system(command.c_str());
        
        return result == 0;
    }
    
    bool PromoteNewPrimary() {
        std::cout << "👑 Primary 승격 중..." << std::endl;
        
        std::string command = "mysql_promote_primary.sh";
        int result = system(command.c_str());
        
        return result == 0;
    }
    
    bool PerformPointInTimeRecovery(const std::string& target_time) {
        std::cout << "⏰ Point-in-Time 복구 수행 중: " << target_time << std::endl;
        
        std::string command = "mysql_backup_system.sh pitr '" + target_time + "'";
        int result = system(command.c_str());
        
        return result == 0;
    }
    
    bool VerifyRecovery(const std::string& failure_type, const Json::Value& context) {
        std::cout << "🔍 복구 검증 중..." << std::endl;
        
        // 기본 연결 테스트
        if (!TestDatabaseConnection()) {
            return false;
        }
        
        // 복제 상태 확인
        if (!VerifyReplicationStatus()) {
            return false;
        }
        
        // 데이터 일관성 검증
        if (!VerifyDataConsistency()) {
            return false;
        }
        
        std::cout << "✅ 복구 검증 완료" << std::endl;
        return true;
    }
    
    bool TestDatabaseConnection() {
        std::string command = "mysql -h mysql-router -P 6446 -utest -ptest -e 'SELECT 1' > /dev/null 2>&1";
        return system(command.c_str()) == 0;
    }
    
    bool VerifyReplicationStatus() {
        std::string command = "mysql_check_replication.sh";
        return system(command.c_str()) == 0;
    }
    
    bool VerifyDataConsistency() {
        std::string command = "mysql_checksum_tables.sh";
        return system(command.c_str()) == 0;
    }
    
    void NotifyOperators(const std::string& event_type, const std::string& message, 
                        const Json::Value& context) {
        
        std::cout << "📢 운영자 알림: " << event_type << " - " << message << std::endl;
        
        // Slack 알림
        std::string slack_command = "send_slack_alert.sh '" + event_type + "' '" + message + "'";
        system(slack_command.c_str());
        
        // 이메일 알림 (중요한 경우)
        if (event_type == "RECOVERY_FAILED" || event_type == "MANUAL_INTERVENTION_REQUIRED") {
            std::string email_command = "send_email_alert.sh '" + event_type + "' '" + message + "'";
            system(email_command.c_str());
        }
        
        // 로그 기록
        LogRecoveryEvent(event_type, message, context);
    }
    
    void LogRecoveryEvent(const std::string& event_type, const std::string& message, 
                         const Json::Value& context) {
        std::ofstream log_file("/var/log/mysql-recovery/recovery.log", std::ios::app);
        
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        
        log_file << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
                 << " [" << event_type << "] " << message << std::endl;
        
        if (!context.empty()) {
            log_file << "Context: " << context << std::endl;
        }
        
        log_file.close();
    }
    
    // 추가 복구 함수들 (간략화)
    bool ReconfigureReplication(const std::string& node) { return true; }
    bool AttemptNodeRestart(const std::string& node) { return true; }
    bool VerifyDataSync(const std::string& node) { return true; }
    bool ReconfigureSecondaries() { return true; }
    bool UpdateApplicationConnections() { return true; }
    bool ScheduleFailedNodeRecovery(const std::string& node) { return true; }
    bool IsolateCorruptedTable(const std::string& table) { return true; }
    bool VerifyDataIntegrity(const std::string& table) { return true; }
    bool ReplayTransactionLogs(const std::string& target_time) { return true; }
    bool VerifyApplicationData(const std::string& table) { return true; }
};

// 🎮 게임 서버와 연동된 복구 시스템 데모
void DemoAutoRecoverySystem() {
    std::cout << "🎮 MySQL 자동 복구 시스템 데모" << std::endl;
    
    MySQLAutoRecovery recovery_system;
    
    // 시나리오 1: 단일 노드 장애
    {
        Json::Value context;
        context["failed_node"] = "mysql-node2";
        context["failure_reason"] = "하드웨어 장애";
        
        recovery_system.HandleFailure("SINGLE_NODE_FAILURE", context);
        std::this_thread::sleep_for(std::chrono::seconds(10));  // 복구 시뮬레이션
    }
    
    // 시나리오 2: Primary 장애 (자동 페일오버)
    {
        Json::Value context;
        context["failed_primary"] = "mysql-primary";
        context["failure_reason"] = "네트워크 분리";
        
        recovery_system.HandleFailure("PRIMARY_NODE_FAILURE", context);
        std::this_thread::sleep_for(std::chrono::seconds(30));  // 페일오버 시뮬레이션
    }
    
    // 시나리오 3: 데이터 손상 (수동 개입 필요)
    {
        Json::Value context;
        context["corrupted_table"] = "players";
        context["recovery_time"] = "2024-01-15 14:30:00";
        context["corruption_type"] = "인덱스 손상";
        
        recovery_system.HandleFailure("DATA_CORRUPTION", context);
    }
    
    std::cout << "데모 완료. 로그 확인: /var/log/mysql-recovery/recovery.log" << std::endl;
}
```

---

## 🔄 4. 데이터 마이그레이션 (제로 다운타임)

### **4.1 온라인 스키마 변경**

```sql
-- 🔄 제로 다운타임 스키마 변경 예제

-- 상황: players 테이블에 새로운 컬럼 추가 (5천만 행 테이블)
-- 기존 방식: ALTER TABLE (서비스 중단 1-2시간)
-- 개선 방식: Online DDL + pt-online-schema-change

-- 1️⃣ 기존 방식 (❌ 서비스 중단)
-- ALTER TABLE players ADD COLUMN guild_id INT DEFAULT NULL;
-- 문제: 테이블 락으로 인한 서비스 중단

-- 2️⃣ MySQL 8.0 Online DDL (✅ 부분적 해결)
ALTER TABLE players 
ADD COLUMN guild_id INT DEFAULT NULL,
ALGORITHM=INPLACE, 
LOCK=NONE;

-- 3️⃣ pt-online-schema-change (✅ 완전한 해결)
/*
pt-online-schema-change \
  --alter "ADD COLUMN guild_id INT DEFAULT NULL" \
  --host=mysql-primary \
  --user=schema_change_user \
  --password=password \
  --database=gamedb \
  --table=players \
  --chunk-size=1000 \
  --max-load="Threads_running=25" \
  --critical-load="Threads_running=50" \
  --progress=time,30 \
  --print \
  --execute
*/

-- 4️⃣ 복잡한 스키마 변경 - 테이블 분할
-- 큰 테이블을 연도별로 파티션 분할

-- 기존 game_logs 테이블 (1억 행)
CREATE TABLE game_logs_backup AS SELECT * FROM game_logs WHERE 1=0;

-- 새로운 파티션 테이블 생성
CREATE TABLE game_logs_new (
    log_id BIGINT NOT NULL AUTO_INCREMENT,
    player_id BIGINT NOT NULL,
    action_type VARCHAR(50) NOT NULL,
    action_data JSON,
    created_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (log_id, created_at),
    INDEX idx_player_time (player_id, created_at),
    INDEX idx_action_type (action_type, created_at)
) PARTITION BY RANGE (YEAR(created_at)) (
    PARTITION p2022 VALUES LESS THAN (2023),
    PARTITION p2023 VALUES LESS THAN (2024),
    PARTITION p2024 VALUES LESS THAN (2025),
    PARTITION p2025 VALUES LESS THAN (2026),
    PARTITION p_future VALUES LESS THAN MAXVALUE
);

-- 5️⃣ 게임별 특수 마이그레이션 - 캐릭터 데이터 정규화
-- 기존: character_stats (JSON으로 모든 스탯 저장)
-- 변경: 개별 스탯 컬럼으로 분리 (검색 성능 향상)

-- 단계적 마이그레이션 프로시저
DELIMITER //

CREATE PROCEDURE MigrateCharacterStats()
BEGIN
    DECLARE done INT DEFAULT FALSE;
    DECLARE char_id BIGINT;
    DECLARE stats_json JSON;
    DECLARE cur CURSOR FOR 
        SELECT character_id, character_stats 
        FROM characters 
        WHERE migrated = 0 
        LIMIT 1000;
    DECLARE CONTINUE HANDLER FOR NOT FOUND SET done = TRUE;
    
    START TRANSACTION;
    
    OPEN cur;
    read_loop: LOOP
        FETCH cur INTO char_id, stats_json;
        IF done THEN
            LEAVE read_loop;
        END IF;
        
        -- JSON에서 개별 스탯 추출
        UPDATE characters SET
            strength = JSON_EXTRACT(stats_json, '$.strength'),
            agility = JSON_EXTRACT(stats_json, '$.agility'),
            intelligence = JSON_EXTRACT(stats_json, '$.intelligence'),
            vitality = JSON_EXTRACT(stats_json, '$.vitality'),
            migrated = 1
        WHERE character_id = char_id;
        
    END LOOP;
    CLOSE cur;
    
    COMMIT;
    
    -- 마이그레이션 진행 상황 출력
    SELECT 
        COUNT(*) as total_characters,
        SUM(migrated) as migrated_characters,
        ROUND(SUM(migrated) / COUNT(*) * 100, 2) as progress_percent
    FROM characters;
    
END //

DELIMITER ;

-- 마이그레이션 실행 (배치로 처리)
-- CALL MigrateCharacterStats();

-- 6️⃣ 인덱스 최적화 - 게임 쿼리 패턴에 맞춤
-- 게임에서 자주 사용하는 쿼리 패턴 분석 후 인덱스 추가

-- 플레이어 검색 쿼리 최적화
CREATE INDEX idx_player_search ON players (
    server_id, 
    level DESC, 
    last_login DESC
) WHERE status = 'active';

-- 길드 랭킹 쿼리 최적화  
CREATE INDEX idx_guild_ranking ON guilds (
    server_id,
    guild_level DESC,
    total_experience DESC
) WHERE is_active = 1;

-- 아이템 거래 내역 쿼리 최적화
CREATE INDEX idx_trade_history ON item_trades (
    server_id,
    traded_at DESC,
    item_id
) WHERE trade_status = 'completed';

-- 7️⃣ 데이터 아카이빙 - 오래된 데이터 분리
-- 6개월 이상 된 게임 로그를 아카이브 테이블로 이동

-- 아카이브 테이블 생성 (압축 스토리지 엔진 사용)
CREATE TABLE game_logs_archive (
    log_id BIGINT NOT NULL,
    player_id BIGINT NOT NULL,
    action_type VARCHAR(50) NOT NULL,
    action_data JSON,
    created_at TIMESTAMP NOT NULL,
    archived_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_player_archived (player_id, archived_at),
    INDEX idx_action_archived (action_type, archived_at)
) ENGINE=InnoDB 
  ROW_FORMAT=COMPRESSED 
  KEY_BLOCK_SIZE=8;

-- 아카이빙 프로시저
DELIMITER //

CREATE PROCEDURE ArchiveOldGameLogs()
BEGIN
    DECLARE archive_cutoff DATE;
    DECLARE archived_count INT DEFAULT 0;
    
    -- 6개월 전 날짜 계산
    SET archive_cutoff = DATE_SUB(CURDATE(), INTERVAL 6 MONTH);
    
    -- 배치 단위로 아카이빙 (1만 건씩)
    archive_loop: LOOP
        START TRANSACTION;
        
        -- 오래된 로그를 아카이브 테이블로 복사
        INSERT INTO game_logs_archive (log_id, player_id, action_type, action_data, created_at)
        SELECT log_id, player_id, action_type, action_data, created_at
        FROM game_logs 
        WHERE created_at < archive_cutoff
        ORDER BY created_at
        LIMIT 10000;
        
        SET archived_count = ROW_COUNT();
        
        IF archived_count = 0 THEN
            ROLLBACK;
            LEAVE archive_loop;
        END IF;
        
        -- 원본 테이블에서 삭제
        DELETE FROM game_logs 
        WHERE created_at < archive_cutoff
        ORDER BY created_at
        LIMIT 10000;
        
        COMMIT;
        
        -- 진행 상황 로깅
        INSERT INTO migration_log (operation, affected_rows, completed_at)
        VALUES ('archive_game_logs', archived_count, NOW());
        
        -- CPU 부하 방지를 위한 대기
        SELECT SLEEP(1);
        
    END LOOP;
    
    -- 최종 결과 출력
    SELECT 
        'Archiving completed' as status,
        COUNT(*) as remaining_logs,
        MIN(created_at) as oldest_log,
        MAX(created_at) as newest_log
    FROM game_logs;
    
END //

DELIMITER ;

-- 8️⃣ 마이그레이션 롤백 계획
-- 모든 스키마 변경에 대한 롤백 스크립트 준비

-- 롤백 스크립트 예제
-- ALTER TABLE players DROP COLUMN guild_id;

-- 마이그레이션 상태 추적 테이블
CREATE TABLE schema_migrations (
    migration_id VARCHAR(100) PRIMARY KEY,
    description TEXT,
    executed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    rollback_script TEXT,
    is_rolled_back BOOLEAN DEFAULT FALSE
);

-- 마이그레이션 실행 기록
INSERT INTO schema_migrations (migration_id, description, rollback_script) VALUES
('20240111_add_guild_id', 'Add guild_id column to players table', 
 'ALTER TABLE players DROP COLUMN guild_id;'),
('20240112_partition_game_logs', 'Partition game_logs table by year',
 'DROP TABLE game_logs_new; RENAME TABLE game_logs_backup TO game_logs;');
```

### **4.2 실시간 데이터 동기화**

```cpp
#include <iostream>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <mysql++/mysql++.h>

// 🔄 실시간 데이터 동기화 시스템
class RealTimeDataSync {
private:
    struct SyncOperation {
        enum Type { INSERT, UPDATE, DELETE } type;
        std::string table_name;
        std::string primary_key;
        std::string data_json;
        std::chrono::steady_clock::time_point timestamp;
    };
    
    // 소스와 타겟 데이터베이스 연결
    std::unique_ptr<mysqlpp::Connection> source_conn_;
    std::unique_ptr<mysqlpp::Connection> target_conn_;
    
    // 동기화 큐
    std::queue<SyncOperation> sync_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
    // 동기화 상태
    std::atomic<bool> sync_running_{false};
    std::atomic<uint21_t> operations_processed_{0};
    std::atomic<uint21_t> operations_failed_{0};
    
    // 동기화 스레드들
    std::vector<std::thread> sync_workers_;
    
    // Binary Log 파싱을 위한 변수들
    std::string last_binlog_file_;
    uint21_t last_binlog_position_ = 0;
    
public:
    RealTimeDataSync(const std::string& source_host, const std::string& target_host,
                     const std::string& username, const std::string& password) {
        
        // 소스 DB 연결 (읽기 전용)
        source_conn_ = std::make_unique<mysqlpp::Connection>();
        if (!source_conn_->connect("gamedb", source_host.c_str(), username.c_str(), password.c_str())) {
            throw std::runtime_error("소스 DB 연결 실패: " + source_host);
        }
        
        // 타겟 DB 연결 (쓰기)
        target_conn_ = std::make_unique<mysqlpp::Connection>();
        if (!target_conn_->connect("gamedb_new", target_host.c_str(), username.c_str(), password.c_str())) {
            throw std::runtime_error("타겟 DB 연결 실패: " + target_host);
        }
        
        std::cout << "✅ 데이터 동기화 시스템 초기화 완료" << std::endl;
    }
    
    ~RealTimeDataSync() {
        Stop();
    }
    
    void Start(int worker_count = 4) {
        if (sync_running_) {
            std::cout << "⚠️ 동기화가 이미 실행 중입니다." << std::endl;
            return;
        }
        
        sync_running_ = true;
        
        // Binary Log 파싱 스레드 시작
        std::thread binlog_thread(&RealTimeDataSync::BinlogParsingLoop, this);
        binlog_thread.detach();
        
        // 동기화 워커 스레드들 시작
        for (int i = 0; i < worker_count; ++i) {
            sync_workers_.emplace_back(&RealTimeDataSync::SyncWorkerLoop, this, i);
        }
        
        std::cout << "🚀 실시간 데이터 동기화 시작 (워커: " << worker_count << "개)" << std::endl;
    }
    
    void Stop() {
        if (!sync_running_) return;
        
        sync_running_ = false;
        queue_cv_.notify_all();
        
        // 모든 워커 스레드 종료 대기
        for (auto& worker : sync_workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        sync_workers_.clear();
        
        std::cout << "🛑 데이터 동기화 중지" << std::endl;
        PrintSyncStats();
    }
    
private:
    // Binary Log 파싱 루프 (MySQL의 변경사항 실시간 감지)
    void BinlogParsingLoop() {
        std::cout << "📖 Binary Log 파싱 시작" << std::endl;
        
        // 마지막 동기화 위치 복원
        RestoreLastSyncPosition();
        
        while (sync_running_) {
            try {
                // Binary Log에서 새로운 이벤트 읽기
                ParseBinaryLogEvents();
                
                // 짧은 대기 후 다시 확인
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
            } catch (const std::exception& e) {
                std::cerr << "❌ Binary Log 파싱 오류: " << e.what() << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        }
    }
    
    void ParseBinaryLogEvents() {
        // 실제로는 MySQL의 Binary Log를 파싱하지만, 
        // 여기서는 간단한 시뮬레이션으로 대체
        
        // SHOW BINARY LOGS로 현재 로그 파일 확인
        mysqlpp::Query query = source_conn_->query();
        query << "SHOW MASTER STATUS";
        mysqlpp::StoreQueryResult result = query.store();
        
        if (result.empty()) return;
        
        std::string current_file = result[0]["File"];
        uint21_t current_position = result[0]["Position"];
        
        if (current_file != last_binlog_file_ || current_position > last_binlog_position_) {
            // 변경 사항이 있음 - 실제로는 mysqlbinlog 도구나 라이브러리 사용
            SimulateBinlogEvents();
            
            last_binlog_file_ = current_file;
            last_binlog_position_ = current_position;
            
            // 동기화 위치 저장
            SaveSyncPosition();
        }
    }
    
    void SimulateBinlogEvents() {
        // 실제 운영에서는 Binary Log 파싱 라이브러리 사용
        // 여기서는 테스트용 이벤트 생성
        
        static int event_counter = 0;
        event_counter++;
        
        if (event_counter % 3 == 0) {
            // 플레이어 레벨업 이벤트 시뮬레이션
            SyncOperation op;
            op.type = SyncOperation::UPDATE;
            op.table_name = "players";
            op.primary_key = "12345";
            op.data_json = R"({"level": 51, "experience": 2600000, "updated_at": "2024-01-15 15:30:00"})";
            op.timestamp = std::chrono::steady_clock::now();
            
            AddToSyncQueue(op);
        }
        
        if (event_counter % 5 == 0) {
            // 아이템 획득 이벤트 시뮬레이션  
            SyncOperation op;
            op.type = SyncOperation::INSERT;
            op.table_name = "inventory";
            op.primary_key = "inv_" + std::to_string(event_counter);
            op.data_json = R"({"player_id": 12345, "item_id": 1001, "quantity": 1, "obtained_at": "2024-01-15 15:30:00"})";
            op.timestamp = std::chrono::steady_clock::now();
            
            AddToSyncQueue(op);
        }
    }
    
    void AddToSyncQueue(const SyncOperation& op) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            sync_queue_.push(op);
        }
        queue_cv_.notify_one();
    }
    
    // 동기화 워커 루프
    void SyncWorkerLoop(int worker_id) {
        std::cout << "👷 동기화 워커 " << worker_id << " 시작" << std::endl;
        
        while (sync_running_) {
            SyncOperation op;
            
            // 큐에서 작업 가져오기
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                queue_cv_.wait(lock, [this] { return !sync_queue_.empty() || !sync_running_; });
                
                if (!sync_running_) break;
                
                if (sync_queue_.empty()) continue;
                
                op = sync_queue_.front();
                sync_queue_.pop();
            }
            
            // 동기화 작업 수행
            if (ProcessSyncOperation(op, worker_id)) {
                operations_processed_++;
            } else {
                operations_failed_++;
                
                // 실패한 작업은 나중에 재시도하도록 큐에 다시 추가
                std::this_thread::sleep_for(std::chrono::seconds(1));
                AddToSyncQueue(op);
            }
        }
        
        std::cout << "👷 동기화 워커 " << worker_id << " 종료" << std::endl;
    }
    
    bool ProcessSyncOperation(const SyncOperation& op, int worker_id) {
        try {
            switch (op.type) {
                case SyncOperation::INSERT:
                    return ProcessInsert(op);
                    
                case SyncOperation::UPDATE:
                    return ProcessUpdate(op);
                    
                case SyncOperation::DELETE:
                    return ProcessDelete(op);
                    
                default:
                    std::cerr << "❌ 알 수 없는 동기화 작업 타입" << std::endl;
                    return false;
            }
            
        } catch (const std::exception& e) {
            std::cerr << "❌ 워커 " << worker_id << " 동기화 오류: " << e.what() << std::endl;
            return false;
        }
    }
    
    bool ProcessInsert(const SyncOperation& op) {
        // JSON 데이터 파싱하여 INSERT 쿼리 생성
        // 실제로는 JSON 라이브러리 사용
        
        mysqlpp::Query query = target_conn_->query();
        
        if (op.table_name == "players") {
            // 플레이어 데이터 삽입 예제
            query << "INSERT INTO players (player_id, level, experience, updated_at) "
                  << "VALUES (12345, 51, 2600000, NOW()) "
                  << "ON DUPLICATE KEY UPDATE "
                  << "level = VALUES(level), "
                  << "experience = VALUES(experience), "
                  << "updated_at = VALUES(updated_at)";
                  
        } else if (op.table_name == "inventory") {
            // 인벤토리 데이터 삽입 예제
            query << "INSERT INTO inventory (player_id, item_id, quantity, obtained_at) "
                  << "VALUES (12345, 1001, 1, NOW())";
        }
        
        query.execute();
        
        std::cout << "✅ INSERT 동기화 완료: " << op.table_name 
                  << " [" << op.primary_key << "]" << std::endl;
        
        return true;
    }
    
    bool ProcessUpdate(const SyncOperation& op) {
        mysqlpp::Query query = target_conn_->query();
        
        if (op.table_name == "players") {
            query << "UPDATE players SET "
                  << "level = 51, "
                  << "experience = 2600000, "
                  << "updated_at = NOW() "
                  << "WHERE player_id = " << op.primary_key;
        }
        
        mysqlpp::SimpleResult result = query.execute();
        
        if (result.rows() > 0) {
            std::cout << "✅ UPDATE 동기화 완료: " << op.table_name 
                      << " [" << op.primary_key << "] (" << result.rows() << "행)" << std::endl;
            return true;
        } else {
            std::cout << "⚠️ UPDATE 대상 없음: " << op.table_name 
                      << " [" << op.primary_key << "]" << std::endl;
            return true;  // 대상이 없는 것은 오류가 아님
        }
    }
    
    bool ProcessDelete(const SyncOperation& op) {
        mysqlpp::Query query = target_conn_->query();
        
        if (op.table_name == "players") {
            query << "DELETE FROM players WHERE player_id = " << op.primary_key;
        } else if (op.table_name == "inventory") {
            query << "DELETE FROM inventory WHERE inventory_id = " << mysqlpp::quote << op.primary_key;
        }
        
        mysqlpp::SimpleResult result = query.execute();
        
        std::cout << "✅ DELETE 동기화 완료: " << op.table_name 
                  << " [" << op.primary_key << "] (" << result.rows() << "행)" << std::endl;
        
        return true;
    }
    
    void RestoreLastSyncPosition() {
        try {
            mysqlpp::Query query = target_conn_->query();
            query << "SELECT binlog_file, binlog_position FROM sync_checkpoint "
                  << "WHERE sync_name = 'realtime_migration' ORDER BY updated_at DESC LIMIT 1";
            
            mysqlpp::StoreQueryResult result = query.store();
            
            if (!result.empty()) {
                last_binlog_file_ = std::string(result[0]["binlog_file"]);
                last_binlog_position_ = result[0]["binlog_position"];
                
                std::cout << "📍 동기화 위치 복원: " << last_binlog_file_ 
                          << ":" << last_binlog_position_ << std::endl;
            }
            
        } catch (const std::exception& e) {
            std::cout << "⚠️ 동기화 위치 복원 실패: " << e.what() << std::endl;
            // 처음부터 시작
        }
    }
    
    void SaveSyncPosition() {
        try {
            mysqlpp::Query query = target_conn_->query();
            query << "INSERT INTO sync_checkpoint (sync_name, binlog_file, binlog_position, updated_at) "
                  << "VALUES ('realtime_migration', " << mysqlpp::quote << last_binlog_file_ 
                  << ", " << last_binlog_position_ << ", NOW()) "
                  << "ON DUPLICATE KEY UPDATE "
                  << "binlog_file = VALUES(binlog_file), "
                  << "binlog_position = VALUES(binlog_position),                  << "updated_at = NOW()";
            
            query.execute();
            
        } catch (const std::exception& e) {
            std::cerr << "❌ 동기화 위치 저장 실패: " << e.what() << std::endl;
        }
    }
};

---

## 🔥 흔한 실수와 해결방법 (Common Mistakes & Solutions)

### 1. 샤딩 후 JOIN 쿼리 실패
```cpp
// [SEQUENCE: 1] ❌ 잘못된 예: 샤딩된 테이블 간 JOIN
class BadShardingQuery {
    void GetPlayerWithInventory(uint21_t player_id) {
        // 플레이어와 인벤토리가 다른 샤드에 있으면 JOIN 불가\!
        std::string query = 
            "SELECT p.*, i.* FROM players p "
            "JOIN inventory i ON p.player_id = i.player_id "
            "WHERE p.player_id = " + std::to_string(player_id);
        // 에러: 크로스 샤드 JOIN 불가능
    }
};

// ✅ 올바른 예: 애플리케이션 레벨 JOIN
class GoodShardingQuery {
    void GetPlayerWithInventory(uint21_t player_id) {
        // 1. 플레이어 데이터 조회
        auto player = GetPlayerFromShard(player_id);
        
        // 2. 인벤토리 데이터 별도 조회
        auto inventory = GetInventoryFromShard(player_id);
        
        // 3. 애플리케이션에서 조합
        return CombinePlayerAndInventory(player, inventory);
    }
};
```

### 2. 무분별한 인덱스 생성
```cpp
// [SEQUENCE: 2] ❌ 잘못된 예: 모든 컬럼에 인덱스
CREATE INDEX idx_player_level ON players(level);
CREATE INDEX idx_player_exp ON players(experience);
CREATE INDEX idx_player_gold ON players(gold);
CREATE INDEX idx_player_login ON players(last_login);
// 문제: INSERT/UPDATE 성능 급격히 저하\!

// ✅ 올바른 예: 선택적 복합 인덱스
CREATE INDEX idx_player_search ON players(level, last_login);
CREATE INDEX idx_player_ranking ON players(experience DESC, player_id);
// WHERE 절과 ORDER BY에 실제로 사용되는 컬럼만 인덱싱
```

### 3. 캐시 무효화 전략 부재
```cpp
// [SEQUENCE: 3] ❌ 잘못된 예: 무작정 캐시만 사용
class BadCacheStrategy {
    void UpdatePlayerLevel(uint21_t player_id, int new_level) {
        // DB 업데이트
        db.Update("UPDATE players SET level = ? WHERE player_id = ?", 
                 new_level, player_id);
        
        // 캐시는? 그대로 남아있어 불일치 발생\!
    }
};

// ✅ 올바른 예: Cache-Aside 패턴
class GoodCacheStrategy {
    void UpdatePlayerLevel(uint21_t player_id, int new_level) {
        // 1. 캐시 무효화
        cache.Delete("player:" + std::to_string(player_id));
        
        // 2. DB 업데이트
        db.Update("UPDATE players SET level = ? WHERE player_id = ?", 
                 new_level, player_id);
        
        // 3. 선택적 캐시 프리로드 (핫 데이터인 경우)
        if (IsHotPlayer(player_id)) {
            auto player = db.Query("SELECT * FROM players WHERE player_id = ?", player_id);
            cache.Set("player:" + std::to_string(player_id), player, 3600);
        }
    }
};
```

---

## 🚀 실습 프로젝트 (Practice Projects)

### 📚 기초 프로젝트: 게임 로그 샤딩 시스템
**목표**: 날짜 기반 샤딩으로 대용량 로그 관리

```cpp
// 구현 요구사항:
// 1. 날짜별 자동 테이블 생성
// 2. 30일 이상 된 로그 자동 아카이빙
// 3. 병렬 쿼리 실행
// 4. 로그 분석 대시보드
// 5. 1일 10억 로그 처리
// 6. 쿼리 응답시간 100ms 이내
```

### 🎮 중급 프로젝트: 실시간 랭킹 시스템
**목표**: Redis와 MySQL을 활용한 하이브리드 랭킹

```cpp
// 핵심 기능:
// 1. Redis Sorted Set 실시간 랭킹
// 2. MySQL 영구 저장 및 백업
// 3. 주기적 동기화 시스템
// 4. 랭킹 변동 알림
// 5. 100만 유저 실시간 처리
// 6. 글로벌/서버별/길드별 랭킹
```

### 🏆 고급 프로젝트: 무중단 데이터 마이그레이션
**목표**: 라이브 서비스 중 페타바이트 데이터 이전

```cpp
// 구현 내용:
// 1. CDC 기반 실시간 동기화
// 2. 데이터 일관성 검증
// 3. 점진적 트래픽 전환
// 4. 롤백 계획 수립
// 5. 모니터링 대시보드
// 6. 0 다운타임 달성
// 7. 페타바이트 규모 처리
```

---

## 📊 학습 체크리스트 (Learning Checklist)

### 🥉 브론즈 레벨
- [ ] 인덱스 설계 원칙 이해
- [ ] 기본 샤딩 개념 숙지
- [ ] 읽기/쓰기 분리 구현
- [ ] 백업/복구 전략 수립

### 🥈 실버 레벨
- [ ] 복합 샤딩 전략 구현
- [ ] 캐시 전략 설계
- [ ] 쿼리 최적화 마스터
- [ ] 모니터링 시스템 구축

### 🥇 골드 레벨
- [ ] 크로스 샤드 트랜잭션
- [ ] 실시간 데이터 동기화
- [ ] 자동 장애 복구 시스템
- [ ] 성능 튜닝 자동화

### 💎 플래티넘 레벨
- [ ] 글로벌 분산 DB 설계
- [ ] 무중단 마이그레이션
- [ ] 데이터 분석 파이프라인
- [ ] 페타바이트급 운영 경험

---

## 📚 추가 학습 자료 (Additional Resources)

### 필독서
- 📖 "Database Internals" - Alex Petrov
- 📖 "Designing Data-Intensive Applications" - Martin Kleppmann
- 📖 "High Performance MySQL" - Baron Schwartz

### 온라인 강의
- 🎓 CMU Database Systems (YouTube)
- 🎓 Distributed Systems by Martin Kleppmann
- 🎓 MySQL Performance Tuning (Percona)

### 오픈소스 프로젝트
- 🔧 Vitess: YouTube의 MySQL 샤딩 솔루션
- 🔧 ProxySQL: 고성능 MySQL 프록시
- 🔧 ClickHouse: 대용량 분석 DB
- 🔧 TiDB: 분산 SQL 데이터베이스

### 실무 도구
- 🛠️ Percona Toolkit: MySQL 관리 도구
- 🛠️ pt-online-schema-change: 무중단 스키마 변경
- 🛠️ Orchestrator: MySQL 복제 관리
- 🛠️ PMM: Percona Monitoring and Management
