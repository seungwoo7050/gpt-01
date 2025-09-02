# 6단계: NoSQL 데이터베이스 - 유연한 게임 데이터 관리하기
*SQL로는 어려운 복잡한 게임 데이터를 NoSQL로 쉽게 다루는 방법*

> **🎯 목표**: 아이템, 퀘스트, 이벤트 같은 복잡하고 자주 변하는 게임 데이터를 NoSQL 데이터베이스로 효율적으로 관리하기

---

## 📋 문서 정보

- **난이도**: 🟡 중급 (SQL 기초 이해 후 학습)
- **예상 학습 시간**: 5-6시간 (SQL과 비교하며 학습)
- **필요 선수 지식**: 
  - ✅ [5단계: 데이터베이스 기초](./04_database_design_optimization_guide.md) 완료
  - ✅ JSON 형식을 알고 있음 (기본적인 수준)
  - ✅ "배열"과 "객체" 개념 이해
- **실습 환경**: MongoDB 6.0+, AWS SDK for C++, C++17
- **최종 결과물**: SQL + NoSQL 하이브리드 게임 서버!

---

## 🤔 왜 SQL만으로는 부족할까?

### **게임에서 만나는 복잡한 데이터들**

```cpp
// 😰 SQL로 저장하기 어려운 게임 데이터들

// 1️⃣ 플레이어 인벤토리 (아이템이 계속 추가/삭제됨)
struct PlayerInventory {
    // 문제: 아이템 종류가 수백 개, 각각 다른 속성
    std::vector<Item> items;  // 어떻게 테이블로 만들지? 😱
};

struct Item {
    int id;
    std::string name;
    int quantity;
    
    // 😰 아이템마다 다른 속성들...
    // 무기: damage, durability, enchantments
    // 갑옷: defense, magic_resistance
    // 포션: effect_type, duration, power
    // 어떻게 하나의 테이블에? 😭
    std::map<std::string, std::any> custom_properties;
};

// 2️⃣ 퀘스트 데이터 (매우 복잡한 구조)
struct Quest {
    int id;
    std::string title;
    std::string description;
    
    // 😰 복잡한 중첩 구조
    struct QuestStep {
        std::string type;  // "kill", "collect", "talk_to"
        std::map<std::string, int> targets;
        std::vector<QuestStep> sub_steps;  // 하위 단계들
    };
    std::vector<QuestStep> steps;
    
    // 😰 조건도 복잡함
    struct Requirement {
        std::string type;
        std::vector<std::variant<int, std::string>> values;
    };
    std::vector<Requirement> requirements;
};

// 3️⃣ 이벤트 로그 (종류가 무한정)
struct GameEvent {
    std::string event_type;  // "player_login", "item_purchase", "pvp_kill"...
    uint64_t timestamp;
    uint64_t player_id;
    
    // 😰 이벤트마다 완전히 다른 데이터
    nlohmann::json event_data;  // 자유 형식... SQL 테이블로는? 😱
};
```

### **SQL로 억지로 만들면...**

```sql
-- 😭 SQL로 억지로 아이템 시스템을 만든다면...

-- 기본 아이템 테이블
CREATE TABLE items (
    id BIGINT PRIMARY KEY,
    player_id BIGINT,
    item_type VARCHAR(50),
    name VARCHAR(100),
    quantity INT
);

-- 😰 무기 속성 테이블
CREATE TABLE weapon_properties (
    item_id BIGINT,
    damage INT,
    durability INT,
    enchantment_level INT
);

-- 😰 갑옷 속성 테이블  
CREATE TABLE armor_properties (
    item_id BIGINT,
    defense INT,
    magic_resistance INT
);

-- 😰 포션 속성 테이블
CREATE TABLE potion_properties (
    item_id BIGINT,
    effect_type VARCHAR(50),
    duration INT,
    power INT
);

-- 😰 새로운 아이템 타입이 추가될 때마다 테이블 추가...
-- 인첸트, 보석, 룬, 펫, 탈것... 끝이 없음! 😱

-- 😭 아이템 하나 조회하려면 조인이 복잡해짐
SELECT i.*, w.damage, a.defense, p.effect_type
FROM items i
LEFT JOIN weapon_properties w ON i.id = w.item_id  
LEFT JOIN armor_properties a ON i.id = a.item_id
LEFT JOIN potion_properties p ON i.id = p.item_id
WHERE i.player_id = ?;

-- 새 아이템 타입 추가될 때마다 이 쿼리도 수정... 😰
```

### **NoSQL의 마법적 해결책 ✨**

```javascript
// ✨ MongoDB에서는 이런 식으로 자유롭게!

// 플레이어 인벤토리 - 하나의 도큐메트로!
{
  "_id": ObjectId("..."),
  "player_id": 12345,
  "last_updated": ISODate("2024-01-15T10:30:00Z"),
  "items": [
    {
      "id": 1001,
      "name": "드래곤 슬레이어 소드",
      "type": "weapon",
      "quantity": 1,
      "properties": {
        "damage": 250,
        "durability": 100,
        "enchantments": [
          {"type": "fire_damage", "power": 50},
          {"type": "critical_hit", "chance": 15}
        ],
        "rarity": "legendary",
        "required_level": 45
      }
    },
    {
      "id": 2001, 
      "name": "체력 회복 포션",
      "type": "consumable",
      "quantity": 15,
      "properties": {
        "effect_type": "heal",
        "heal_amount": 500,
        "duration": 0,
        "cooldown": 30
      }
    },
    {
      "id": 3001,
      "name": "미스릴 갑옷",
      "type": "armor",
      "quantity": 1, 
      "properties": {
        "defense": 180,
        "magic_resistance": 120,
        "durability": 85,
        "set_bonus": "mithril_guardian",
        "gem_slots": [
          {"type": "ruby", "effect": "+30 strength"},
          {"type": "empty", "effect": null}
        ]
      }
    }
  ],
  "total_value": 15750,
  "bag_slots": 50,
  "used_slots": 17
}

// 🚀 새로운 아이템 타입? 그냥 추가하면 됨!
// 테이블 수정, 스키마 변경 필요 없음!
```

**💡 NoSQL의 핵심 장점:**
- **스키마 자유도**: 구조를 미리 정하지 않아도 됨
- **복잡한 데이터**: 중첩된 객체, 배열을 자연스럽게 저장
- **빠른 개발**: 새로운 데이터 타입 추가가 즉시 가능
- **JSON 친화적**: 웹/모바일과 데이터 호환성 완벽
- **수평 확장**: 서버를 늘려서 성능 향상 용이

---

## 📚 Integration with MMORPG Server Engine

**Current Database Architecture:**
- **Primary**: MySQL 8.0 for ACID-compliant player data (accounts, characters, guilds)
- **Cache**: Redis 7.0 for sessions, real-time data, and pub/sub messaging
- **Proposed NoSQL**: MongoDB for flexible game content, DynamoDB for analytics

```
🗄️ Hybrid Database Architecture
├── MySQL: Critical ACID transactions (accounts, billing, character stats)
├── Redis: Real-time caching and messaging (sessions, leaderboards)
├── MongoDB: Flexible game content (quests, world events, logs)
└── DynamoDB: Analytics and time-series data (player behavior, metrics)
```

---

## 🍃 MongoDB Integration for Game Content

### Player Inventory and Dynamic Content

**`src/core/database/mongodb_manager.h` - Implementation:**
```cpp
// [SEQUENCE: 1] MongoDB C++ driver integration
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/json.hpp>

class MongoDBManager {
private:
    mongocxx::instance instance_{};
    mongocxx::client client_;
    mongocxx::database game_db_;
    
    // Connection pool for concurrent access
    std::unique_ptr<mongocxx::pool> connection_pool_;
    
public:
    MongoDBManager(const std::string& uri, const std::string& database_name) 
        : client_{mongocxx::uri{uri}}, game_db_{client_[database_name]} {
        
        // [SEQUENCE: 2] Initialize connection pool
        mongocxx::pool::config pool_config;
        pool_config.max_size(20);  // Max 20 concurrent connections
        pool_config.min_size(5);   // Keep 5 connections warm
        
        connection_pool_ = std::make_unique<mongocxx::pool>(mongocxx::uri{uri}, pool_config);
        
        // [SEQUENCE: 3] Create indexes for performance
        CreateGameIndexes();
    }
    
    // [SEQUENCE: 4] Player inventory management (flexible schema)
    bool SavePlayerInventory(uint64_t player_id, const nlohmann::json& inventory) {
        try {
            auto conn = connection_pool_->acquire();
            auto collection = (*conn)["game"]["player_inventories"];
            
            // [SEQUENCE: 5] BSON document creation from JSON
            auto builder = bsoncxx::builder::stream::document{};
            auto doc = builder 
                << "player_id" << static_cast<int64_t>(player_id)
                << "inventory" << bsoncxx::from_json(inventory.dump())
                << "updated_at" << bsoncxx::types::b_date{std::chrono::system_clock::now()}
                << bsoncxx::builder::stream::finalize;
            
            // [SEQUENCE: 6] Upsert operation (update or insert)
            mongocxx::options::replace options;
            options.upsert(true);
            
            auto filter = bsoncxx::builder::stream::document{} 
                << "player_id" << static_cast<int64_t>(player_id) 
                << bsoncxx::builder::stream::finalize;
            
            auto result = collection.replace_one(filter.view(), doc.view(), options);
            return result && (result->modified_count() > 0 || result->upserted_id());
            
        } catch (const mongocxx::exception& e) {
            spdlog::error("MongoDB save inventory error: {}", e.what());
            return false;
        }
    }
    
    // [SEQUENCE: 7] Complex aggregation for guild statistics
    std::vector<GuildStats> GetGuildStatistics(uint32_t guild_id) {
        try {
            auto conn = connection_pool_->acquire();
            auto collection = (*conn)["game"]["guild_activities"];
            
            // [SEQUENCE: 8] MongoDB aggregation pipeline
            auto pipeline = mongocxx::pipeline{};
            pipeline.match(bsoncxx::builder::stream::document{} 
                << "guild_id" << static_cast<int32_t>(guild_id)
                << "timestamp" << bsoncxx::builder::stream::open_document
                    << "$gte" << bsoncxx::types::b_date{
                        std::chrono::system_clock::now() - std::chrono::hours{24}
                    }
                << bsoncxx::builder::stream::close_document
                << bsoncxx::builder::stream::finalize);
            
            pipeline.group(bsoncxx::builder::stream::document{}
                << "_id" << "$activity_type"
                << "count" << bsoncxx::builder::stream::open_document
                    << "$sum" << 1
                << bsoncxx::builder::stream::close_document
                << "avg_value" << bsoncxx::builder::stream::open_document
                    << "$avg" << "$activity_value"
                << bsoncxx::builder::stream::close_document
                << bsoncxx::builder::stream::finalize);
            
            auto cursor = collection.aggregate(pipeline);
            
            std::vector<GuildStats> stats;
            for (auto&& doc : cursor) {
                GuildStats stat;
                stat.activity_type = doc["_id"].get_string().value.to_string();
                stat.count = doc["count"].get_int32().value;
                stat.average_value = doc["avg_value"].get_double().value;
                stats.push_back(stat);
            }
            
            return stats;
            
        } catch (const mongocxx::exception& e) {
            spdlog::error("MongoDB aggregation error: {}", e.what());
            return {};
        }
    }
    
private:
    // [SEQUENCE: 9] Index creation for optimal performance
    void CreateGameIndexes() {
        try {
            auto conn = connection_pool_->acquire();
            
            // Player inventory indexes
            auto inventory_collection = (*conn)["game"]["player_inventories"];
            inventory_collection.create_index(
                bsoncxx::builder::stream::document{} 
                << "player_id" << 1 
                << bsoncxx::builder::stream::finalize
            );
            
            // Guild activity indexes (compound index)
            auto guild_collection = (*conn)["game"]["guild_activities"];
            guild_collection.create_index(
                bsoncxx::builder::stream::document{}
                << "guild_id" << 1
                << "timestamp" << -1  // Descending for recent queries
                << bsoncxx::builder::stream::finalize
            );
            
            // Text search index for chat logs
            auto chat_collection = (*conn)["game"]["chat_logs"];
            chat_collection.create_index(
                bsoncxx::builder::stream::document{}
                << "message" << "text"
                << bsoncxx::builder::stream::finalize
            );
            
        } catch (const mongocxx::exception& e) {
            spdlog::warn("Index creation warning: {}", e.what());
        }
    }
};
```

### Dynamic Quest System with MongoDB

**Flexible Quest Schema Implementation:**
```cpp
// [SEQUENCE: 10] Dynamic quest system using MongoDB's flexible schema
class DynamicQuestSystem {
private:
    std::shared_ptr<MongoDBManager> mongo_manager_;
    
public:
    // [SEQUENCE: 11] Create dynamic quest with variable objectives
    bool CreateDynamicQuest(const QuestTemplate& quest_template) {
        try {
            auto conn = mongo_manager_->GetConnection();
            auto collection = (*conn)["game"]["dynamic_quests"];
            
            // [SEQUENCE: 12] Flexible BSON document for quest data
            auto builder = bsoncxx::builder::stream::document{};
            auto doc = builder
                << "quest_id" << quest_template.id
                << "name" << quest_template.name
                << "description" << quest_template.description
                << "level_requirement" << quest_template.min_level
                << "objectives" << bsoncxx::builder::stream::open_array;
            
            // [SEQUENCE: 13] Dynamic objectives array
            for (const auto& objective : quest_template.objectives) {
                doc << bsoncxx::builder::stream::open_document
                    << "type" << objective.type
                    << "target" << objective.target
                    << "count_required" << objective.count
                    << "reward_exp" << objective.exp_reward;
                
                // [SEQUENCE: 14] Conditional fields based on objective type
                if (objective.type == "kill_monster") {
                    doc << "monster_id" << objective.monster_id
                        << "location_hint" << objective.location;
                } else if (objective.type == "collect_item") {
                    doc << "item_id" << objective.item_id
                        << "drop_rate" << objective.drop_rate;
                } else if (objective.type == "reach_location") {
                    doc << "coordinates" << bsoncxx::builder::stream::open_document
                        << "x" << objective.target_x
                        << "y" << objective.target_y
                        << "z" << objective.target_z
                        << "radius" << objective.completion_radius
                        << bsoncxx::builder::stream::close_document;
                }
                
                doc << bsoncxx::builder::stream::close_document;
            }
            
            doc << bsoncxx::builder::stream::close_array
                << "rewards" << bsoncxx::builder::stream::open_document
                    << "experience" << quest_template.exp_reward
                    << "gold" << quest_template.gold_reward
                    << "items" << bsoncxx::builder::stream::open_array;
            
            // [SEQUENCE: 15] Dynamic reward items
            for (const auto& reward_item : quest_template.reward_items) {
                doc << bsoncxx::builder::stream::open_document
                    << "item_id" << reward_item.id
                    << "quantity" << reward_item.quantity
                    << "rarity" << reward_item.rarity
                    << bsoncxx::builder::stream::close_document;
            }
            
            doc << bsoncxx::builder::stream::close_array
                << bsoncxx::builder::stream::close_document
                << "created_at" << bsoncxx::types::b_date{std::chrono::system_clock::now()}
                << "expires_at" << bsoncxx::types::b_date{
                    std::chrono::system_clock::now() + std::chrono::hours{quest_template.duration_hours}
                }
                << bsoncxx::builder::stream::finalize;
            
            auto result = collection.insert_one(doc.view());
            return result && result->inserted_id();
            
        } catch (const mongocxx::exception& e) {
            spdlog::error("Dynamic quest creation error: {}", e.what());
            return false;
        }
    }
    
    // [SEQUENCE: 16] Complex quest progress tracking
    bool UpdateQuestProgress(uint64_t player_id, uint32_t quest_id, 
                           const std::string& objective_type, int progress_delta) {
        try {
            auto conn = mongo_manager_->GetConnection();
            auto collection = (*conn)["game"]["player_quest_progress"];
            
            // [SEQUENCE: 17] MongoDB array update operations
            auto filter = bsoncxx::builder::stream::document{}
                << "player_id" << static_cast<int64_t>(player_id)
                << "quest_id" << static_cast<int32_t>(quest_id)
                << "objectives.type" << objective_type
                << bsoncxx::builder::stream::finalize;
            
            auto update = bsoncxx::builder::stream::document{}
                << "$inc" << bsoncxx::builder::stream::open_document
                    << "objectives.$.current_count" << progress_delta
                << bsoncxx::builder::stream::close_document
                << "$set" << bsoncxx::builder::stream::open_document
                    << "last_updated" << bsoncxx::types::b_date{std::chrono::system_clock::now()}
                << bsoncxx::builder::stream::close_document
                << bsoncxx::builder::stream::finalize;
            
            auto result = collection.update_one(filter.view(), update.view());
            return result && result->modified_count() > 0;
            
        } catch (const mongocxx::exception& e) {
            spdlog::error("Quest progress update error: {}", e.what());
            return false;
        }
    }
};
```

---

## ⚡ DynamoDB Integration for Analytics

### Real-time Player Analytics

**`src/core/database/dynamodb_manager.h` - Implementation:**
```cpp
// [SEQUENCE: 18] AWS SDK for C++ integration
#include <aws/core/Aws.h>
#include <aws/dynamodb/DynamoDBClient.h>
#include <aws/dynamodb/model/PutItemRequest.h>
#include <aws/dynamodb/model/QueryRequest.h>
#include <aws/dynamodb/model/AttributeValue.h>

class DynamoDBAnalytics {
private:
    std::unique_ptr<Aws::DynamoDB::DynamoDBClient> client_;
    std::string table_prefix_;
    
    // Batch writing for performance
    std::queue<Aws::DynamoDB::Model::PutItemRequest> write_queue_;
    std::mutex queue_mutex_;
    std::thread batch_writer_thread_;
    std::atomic<bool> should_stop_{false};
    
public:
    DynamoDBAnalytics(const std::string& region, const std::string& table_prefix) 
        : table_prefix_(table_prefix) {
        
        // [SEQUENCE: 19] Initialize AWS SDK
        Aws::SDKOptions options;
        Aws::InitAPI(options);
        
        Aws::Client::ClientConfiguration config;
        config.region = region;
        client_ = std::make_unique<Aws::DynamoDB::DynamoDBClient>(config);
        
        // [SEQUENCE: 20] Start batch writer thread
        batch_writer_thread_ = std::thread(&DynamoDBAnalytics::BatchWriterLoop, this);
    }
    
    ~DynamoDBAnalytics() {
        should_stop_ = true;
        if (batch_writer_thread_.joinable()) {
            batch_writer_thread_.join();
        }
        Aws::ShutdownAPI({});
    }
    
    // [SEQUENCE: 21] Track player action with time-series data
    void TrackPlayerAction(uint64_t player_id, const std::string& action, 
                          const nlohmann::json& metadata) {
        
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        
        // [SEQUENCE: 22] Create DynamoDB item
        Aws::DynamoDB::Model::PutItemRequest request;
        request.SetTableName(table_prefix_ + "_player_actions");
        
        // [SEQUENCE: 23] Partition key: player_id, Sort key: timestamp
        Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue> item;
        item["player_id"] = Aws::DynamoDB::Model::AttributeValue()
            .SetN(std::to_string(player_id));
        item["timestamp"] = Aws::DynamoDB::Model::AttributeValue()
            .SetN(std::to_string(timestamp));
        item["action"] = Aws::DynamoDB::Model::AttributeValue()
            .SetS(action);
        item["metadata"] = Aws::DynamoDB::Model::AttributeValue()
            .SetS(metadata.dump());
        
        // [SEQUENCE: 24] Add to batch queue for performance
        request.SetItem(item);
        
        std::lock_guard<std::mutex> lock(queue_mutex_);
        write_queue_.push(request);
    }
    
    // [SEQUENCE: 25] Get player behavior analytics
    PlayerBehaviorAnalytics GetPlayerBehavior(uint64_t player_id, 
                                            std::chrono::hours time_window) {
        
        auto now = std::chrono::system_clock::now();
        auto start_time = now - time_window;
        auto start_timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            start_time.time_since_epoch()).count();
        
        // [SEQUENCE: 26] DynamoDB Query with time range
        Aws::DynamoDB::Model::QueryRequest request;
        request.SetTableName(table_prefix_ + "_player_actions");
        
        // [SEQUENCE: 27] Key condition expression
        request.SetKeyConditionExpression("player_id = :pid AND #ts >= :start_time");
        
        Aws::Map<Aws::String, Aws::DynamoDB::Model::AttributeValue> expression_values;
        expression_values[":pid"] = Aws::DynamoDB::Model::AttributeValue()
            .SetN(std::to_string(player_id));
        expression_values[":start_time"] = Aws::DynamoDB::Model::AttributeValue()
            .SetN(std::to_string(start_timestamp));
        request.SetExpressionAttributeValues(expression_values);
        
        // [SEQUENCE: 28] Handle reserved keywords
        Aws::Map<Aws::String, Aws::String> expression_names;
        expression_names["#ts"] = "timestamp";
        request.SetExpressionAttributeNames(expression_names);
        
        PlayerBehaviorAnalytics analytics;
        
        try {
            auto result = client_->Query(request);
            if (result.IsSuccess()) {
                // [SEQUENCE: 29] Process query results
                for (const auto& item : result.GetResult().GetItems()) {
                    std::string action = item.at("action").GetS();
                    std::string metadata_str = item.at("metadata").GetS();
                    
                    // Parse metadata JSON
                    nlohmann::json metadata = nlohmann::json::parse(metadata_str);
                    
                    // [SEQUENCE: 30] Aggregate behavior statistics
                    analytics.action_counts[action]++;
                    analytics.total_actions++;
                    
                    if (action == "combat") {
                        analytics.combat_actions++;
                        if (metadata.contains("damage")) {
                            analytics.total_damage += metadata["damage"].get<int>();
                        }
                    } else if (action == "movement") {
                        analytics.movement_actions++;
                        if (metadata.contains("distance")) {
                            analytics.total_distance += metadata["distance"].get<float>();
                        }
                    } else if (action == "item_use") {
                        analytics.item_usage_count++;
                    }
                }
            }
        } catch (const Aws::DynamoDB::DynamoDBError& e) {
            spdlog::error("DynamoDB query error: {}", e.GetMessage());
        }
        
        return analytics;
    }
    
private:
    // [SEQUENCE: 31] Batch writer for optimal performance
    void BatchWriterLoop() {
        const size_t BATCH_SIZE = 25;  // DynamoDB batch limit
        
        while (!should_stop_) {
            std::vector<Aws::DynamoDB::Model::PutItemRequest> batch;
            
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                while (!write_queue_.empty() && batch.size() < BATCH_SIZE) {
                    batch.push_back(write_queue_.front());
                    write_queue_.pop();
                }
            }
            
            if (!batch.empty()) {
                // [SEQUENCE: 32] Execute batch write
                for (const auto& request : batch) {
                    try {
                        auto result = client_->PutItem(request);
                        if (!result.IsSuccess()) {
                            spdlog::warn("DynamoDB write failed: {}", 
                                       result.GetError().GetMessage());
                        }
                    } catch (const std::exception& e) {
                        spdlog::error("DynamoDB batch write error: {}", e.what());
                    }
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
};
```

### Time-Series Data for Server Metrics

**Server Performance Analytics:**
```cpp
// [SEQUENCE: 33] Server metrics tracking with DynamoDB
class ServerMetricsCollector {
private:
    std::shared_ptr<DynamoDBAnalytics> analytics_;
    std::chrono::steady_clock::time_point last_collection_;
    
public:
    // [SEQUENCE: 34] Collect and store server performance metrics
    void CollectServerMetrics() {
        auto now = std::chrono::steady_clock::now();
        
        ServerMetrics metrics;
        metrics.timestamp = std::chrono::system_clock::now();
        metrics.active_connections = GetActiveConnectionCount();
        metrics.cpu_usage = GetCPUUsage();
        metrics.memory_usage = GetMemoryUsage();
        metrics.packets_per_second = GetPacketsPerSecond();
        metrics.average_latency = GetAverageLatency();
        
        // [SEQUENCE: 35] Convert to JSON for DynamoDB storage
        nlohmann::json metrics_json = {
            {"active_connections", metrics.active_connections},
            {"cpu_usage", metrics.cpu_usage},
            {"memory_usage", metrics.memory_usage},
            {"packets_per_second", metrics.packets_per_second},
            {"average_latency", metrics.average_latency},
            {"server_id", GetServerId()}
        };
        
        // [SEQUENCE: 36] Store in time-series table
        analytics_->TrackServerMetric("performance", metrics_json);
        
        last_collection_ = now;
    }
    
    // [SEQUENCE: 37] Aggregate metrics for monitoring dashboard
    std::vector<ServerMetricsSummary> GetMetricsSummary(std::chrono::hours window) {
        // Implementation to query and aggregate DynamoDB time-series data
        // Returns data suitable for Grafana/monitoring dashboards
    }
};
```

---

## 🔄 Hybrid Database Strategy

### Data Distribution Strategy

**`src/core/database/hybrid_data_manager.h` - Implementation:**
```cpp
// [SEQUENCE: 38] Hybrid database manager for optimal data placement
class HybridDataManager {
private:
    std::shared_ptr<DatabaseManager> mysql_manager_;      // ACID transactions
    std::shared_ptr<RedisConnectionPool> redis_manager_;  // Real-time cache
    std::shared_ptr<MongoDBManager> mongo_manager_;       // Flexible content
    std::shared_ptr<DynamoDBAnalytics> dynamo_manager_;   // Analytics
    
public:
    // [SEQUENCE: 39] Data routing based on access patterns
    enum class DataType {
        CRITICAL_TRANSACTIONAL,  // MySQL: Account data, billing
        REAL_TIME_CACHE,         // Redis: Sessions, leaderboards
        FLEXIBLE_CONTENT,        // MongoDB: Quests, inventory
        ANALYTICS_TIMESERIES     // DynamoDB: Metrics, behavior
    };
    
    // [SEQUENCE: 40] Intelligent data placement
    template<typename T>
    bool SaveData(const std::string& key, const T& data, DataType type) {
        switch (type) {
            case DataType::CRITICAL_TRANSACTIONAL:
                return SaveToMySQL(key, data);
                
            case DataType::REAL_TIME_CACHE:
                return SaveToRedis(key, data);
                
            case DataType::FLEXIBLE_CONTENT:
                return SaveToMongoDB(key, data);
                
            case DataType::ANALYTICS_TIMESERIES:
                return SaveToDynamoDB(key, data);
        }
        return false;
    }
    
    // [SEQUENCE: 41] Cross-database consistency for critical operations
    bool TransferCharacterToGuild(uint64_t player_id, uint32_t guild_id) {
        // [SEQUENCE: 42] Start distributed transaction
        auto mysql_transaction = mysql_manager_->BeginTransaction();
        
        try {
            // [SEQUENCE: 43] Update critical data in MySQL
            if (!mysql_manager_->UpdatePlayerGuild(player_id, guild_id)) {
                mysql_transaction->Rollback();
                return false;
            }
            
            // [SEQUENCE: 44] Update flexible data in MongoDB
            nlohmann::json guild_data = {{"guild_id", guild_id}, {"join_date", std::time(nullptr)}};
            if (!mongo_manager_->UpdatePlayerDocument(player_id, "guild_info", guild_data)) {
                mysql_transaction->Rollback();
                return false;
            }
            
            // [SEQUENCE: 45] Update cache in Redis
            if (!redis_manager_->SetPlayerGuild(player_id, guild_id)) {
                mysql_transaction->Rollback();
                // Compensate MongoDB change
                mongo_manager_->RemovePlayerGuildInfo(player_id);
                return false;
            }
            
            // [SEQUENCE: 46] Log analytics event
            nlohmann::json analytics_data = {
                {"event", "guild_join"}, 
                {"guild_id", guild_id}, 
                {"timestamp", std::time(nullptr)}
            };
            dynamo_manager_->TrackPlayerAction(player_id, "guild_join", analytics_data);
            
            mysql_transaction->Commit();
            return true;
            
        } catch (const std::exception& e) {
            mysql_transaction->Rollback();
            spdlog::error("Hybrid transaction failed: {}", e.what());
            return false;
        }
    }
    
    // [SEQUENCE: 47] Data synchronization across databases
    void SynchronizePlayerData(uint64_t player_id) {
        // Get authoritative data from MySQL
        auto player_core = mysql_manager_->GetPlayerCore(player_id);
        if (!player_core) return;
        
        // Update Redis cache
        redis_manager_->CachePlayerData(player_id, *player_core);
        
        // Update MongoDB flexible data
        nlohmann::json mongo_update = {
            {"level", player_core->level},
            {"experience", player_core->experience},
            {"last_login", player_core->last_login}
        };
        mongo_manager_->UpdatePlayerDocument(player_id, "core_stats", mongo_update);
    }
};
```

---

## 📊 Performance Comparison

### Database Performance Benchmarks

**Operation Performance (5,000 concurrent users):**

| Operation | MySQL | Redis | MongoDB | DynamoDB |
|-----------|-------|-------|---------|----------|
| Simple Read | 2ms | 0.1ms | 3ms | 5ms |
| Complex Query | 15ms | N/A | 8ms | 12ms |
| Write (ACID) | 5ms | 0.2ms | 4ms | 8ms |
| Batch Write | 20ms | 1ms | 12ms | 6ms |
| Analytics Query | 50ms | N/A | 25ms | 15ms |

**Use Case Optimization:**
- **User Login**: Redis (session) + MySQL (auth) = 2.1ms total
- **Inventory Update**: MongoDB flexible schema = 4ms
- **Player Analytics**: DynamoDB time-series = 15ms
- **Guild Operations**: Hybrid transaction = 25ms

---

## 🎯 Best Practices

### 1. Data Placement Guidelines
```cpp
// ✅ Good: Use appropriate database for each data type
class PlayerDataStrategy {
    void SavePlayerLogin(uint64_t player_id) {
        // Critical: MySQL for account security
        mysql_->UpdateLastLogin(player_id);
        
        // Fast access: Redis for session
        redis_->SetPlayerSession(player_id, session_token);
        
        // Analytics: DynamoDB for behavior tracking
        dynamo_->TrackPlayerAction(player_id, "login", {});
    }
};

// ❌ Avoid: Using single database for all data types
class BadStrategy {
    void SaveEverythingInMySQL() {  // Poor performance and inflexibility
        mysql_->SavePlayerSession();   // Should be Redis
        mysql_->SavePlayerInventory(); // Should be MongoDB
        mysql_->SaveAnalytics();       // Should be DynamoDB
    }
};
```

### 2. Connection Management
```cpp
// ✅ Good: Connection pooling for each database
class OptimalConnections {
    std::shared_ptr<mysql::ConnectionPool> mysql_pool_;      // 10 connections
    std::shared_ptr<redis::ConnectionPool> redis_pool_;      // 20 connections  
    std::shared_ptr<mongocxx::pool> mongo_pool_;            // 15 connections
    std::unique_ptr<Aws::DynamoDB::DynamoDBClient> dynamo_; // SDK managed
};

// ❌ Avoid: Creating connections per operation
class BadConnections {
    void SaveData() {
        auto mysql_conn = mysql::connect(...);  // Expensive connection creation
        auto mongo_conn = mongocxx::client{...}; // No pooling
    }
};
```

### 3. Error Handling and Fallbacks
```cpp
// ✅ Good: Graceful degradation
class ResilientDataAccess {
    std::optional<PlayerData> GetPlayerData(uint64_t player_id) {
        // Try cache first
        if (auto cached = redis_->GetPlayerData(player_id)) {
            return cached;
        }
        
        // Fallback to primary database
        if (auto mysql_data = mysql_->GetPlayerData(player_id)) {
            // Repopulate cache
            redis_->CachePlayerData(player_id, *mysql_data);
            return mysql_data;
        }
        
        return std::nullopt;
    }
};
```

---

## 🔗 Integration Points

This guide complements:
- **database_design_optimization_guide.md**: Extends MySQL with NoSQL solutions
- **performance_optimization_basics.md**: NoSQL databases optimize read/write patterns
- **security_authentication_guide.md**: Distributed auth across multiple databases

**Next Steps:**
1. Review `src/core/database/` for implementation examples
2. Study hybrid transaction patterns in `hybrid_data_manager.h`
3. Examine DynamoDB time-series analytics in `dynamodb_manager.h`

---

*💡 Key Insight: NoSQL databases excel at specific use cases. Use MongoDB for flexible schemas, DynamoDB for analytics, but keep MySQL for ACID transactions. The hybrid approach gives you the best of all worlds while maintaining data consistency where it matters most.*

---

## 🔥 흔한 실수와 해결방법

### 1. 스키마리스 데이터베이스 오용

#### [SEQUENCE: 48] 무분별한 필드 추가
```cpp
// ❌ 잘못된 예: 일관성 없는 문서 구조
void SavePlayerData(const Player& player) {
    bson::document doc;
    doc << "id" << player.id;
    // 개발자마다 다른 필드명 사용
    doc << "playerName" << player.name;      // camelCase
    doc << "player_level" << player.level;   // snake_case
    doc << "XP" << player.experience;        // 대문자
}

// ✅ 올바른 예: 일관된 스키마 가이드라인
class PlayerSchema {
    static constexpr auto FIELD_ID = "_id";
    static constexpr auto FIELD_NAME = "name";
    static constexpr auto FIELD_LEVEL = "level";
    static constexpr auto FIELD_EXPERIENCE = "experience";
    
    static bson::document ToDocument(const Player& player) {
        return bson::document{}
            << FIELD_ID << player.id
            << FIELD_NAME << player.name
            << FIELD_LEVEL << player.level
            << FIELD_EXPERIENCE << player.experience;
    }
};
```

### 2. 트랜잭션 일관성 문제

#### [SEQUENCE: 49] 분산 트랜잭션 미처리
```cpp
// ❌ 잘못된 예: 부분 실패 시 데이터 불일치
void TransferItem(uint64_t from_player, uint64_t to_player, uint32_t item_id) {
    mysql_->RemoveItem(from_player, item_id);  // 성공
    mongo_->AddItem(to_player, item_id);       // 실패! 아이템 소멸
}

// ✅ 올바른 예: 보상 트랜잭션 패턴
void TransferItemSafely(uint64_t from_player, uint64_t to_player, uint32_t item_id) {
    // 2단계 커밋 패턴
    auto mysql_tx = mysql_->BeginTransaction();
    
    try {
        // 1. MySQL에서 아이템 제거 (잠금)
        if (!mysql_tx->LockAndRemoveItem(from_player, item_id)) {
            throw std::runtime_error("Failed to remove item");
        }
        
        // 2. MongoDB에 아이템 추가
        if (!mongo_->AddItem(to_player, item_id)) {
            mysql_tx->Rollback();
            throw std::runtime_error("Failed to add item");
        }
        
        // 3. 모두 성공하면 커밋
        mysql_tx->Commit();
        
    } catch (const std::exception& e) {
        // 보상 로직
        mysql_tx->Rollback();
        mongo_->RemoveItem(to_player, item_id);  // MongoDB 롤백
        throw;
    }
}
```

### 3. 성능 최적화 실수

#### [SEQUENCE: 50] 비효율적인 쿼리 패턴
```cpp
// ❌ 잘못된 예: N+1 쿼리 문제
std::vector<GuildMember> GetGuildMembers(uint32_t guild_id) {
    auto guild = mongo_->FindGuild(guild_id);
    std::vector<GuildMember> members;
    
    for (const auto& member_id : guild.member_ids) {
        auto member = mongo_->FindPlayer(member_id);  // N개의 쿼리!
        members.push_back(member);
    }
    return members;
}

// ✅ 올바른 예: 배치 쿼리 사용
std::vector<GuildMember> GetGuildMembersEfficiently(uint32_t guild_id) {
    // MongoDB aggregation pipeline
    auto pipeline = bson::pipeline{}
        .match(bson::document{} << "guild_id" << guild_id)
        .lookup("players", "member_id", "_id", "member_info")
        .unwind("$member_info");
    
    return mongo_->Aggregate<GuildMember>("guilds", pipeline);
}
```

## 실습 프로젝트

### 프로젝트 1: MongoDB 기반 인벤토리 시스템 (기초)
**목표**: 유연한 아이템 시스템 구현

**구현 사항**:
- MongoDB C++ 드라이버 설정
- 다양한 아이템 타입 저장 (무기, 방어구, 소비품)
- 동적 속성 지원 (인챈트, 업그레이드)
- 검색 및 필터링 기능

**학습 포인트**:
- BSON 문서 다루기
- 동적 스키마 설계
- 인덱스 최적화

### 프로젝트 2: DynamoDB 실시간 분석 시스템 (중급)
**목표**: 플레이어 행동 분석 시스템 구축

**구현 사항**:
- AWS SDK 통합
- 시계열 데이터 저장
- 실시간 집계 쿼리
- 대시보드용 API

**학습 포인트**:
- 파티션 키 설계
- GSI (Global Secondary Index) 활용
- 배치 쓰기 최적화

### 프로젝트 3: 하이브리드 데이터베이스 아키텍처 (고급)
**목표**: 다중 데이터베이스 통합 시스템

**구현 사항**:
- MySQL + Redis + MongoDB + DynamoDB 통합
- 데이터 타입별 최적 저장소 라우팅
- 분산 트랜잭션 처리
- 데이터 동기화 시스템

**학습 포인트**:
- CAP 이론 실습
- 보상 트랜잭션
- 이벤트 소싱 패턴

## 학습 체크리스트

### 기초 레벨 ✅
- [ ] NoSQL vs SQL 차이점 이해
- [ ] MongoDB 기본 CRUD 작업
- [ ] JSON/BSON 데이터 형식
- [ ] 기본 인덱스 생성

### 중급 레벨 📚
- [ ] MongoDB Aggregation Pipeline
- [ ] DynamoDB 파티션 전략
- [ ] 복합 인덱스 설계
- [ ] 샤딩 개념 이해

### 고급 레벨 🚀
- [ ] 분산 트랜잭션 구현
- [ ] 하이브리드 아키텍처 설계
- [ ] 성능 튜닝 및 최적화
- [ ] 장애 복구 전략

### 전문가 레벨 🏆
- [ ] 커스텀 샤딩 전략
- [ ] 이벤트 드리븐 아키텍처
- [ ] 실시간 데이터 동기화
- [ ] 멀티 리전 복제

## 추가 학습 자료

### 추천 도서
- "MongoDB: The Definitive Guide" - Shannon Bradshaw
- "DynamoDB Book" - Alex DeBrie
- "NoSQL Distilled" - Martin Fowler
- "Seven Databases in Seven Weeks" - Eric Redmond

### 온라인 리소스
- [MongoDB University](https://university.mongodb.com/)
- [AWS DynamoDB Documentation](https://docs.aws.amazon.com/dynamodb/)
- [MongoDB C++ Driver Tutorial](https://mongocxx.org/mongocxx-v3/tutorial/)
- [AWS SDK for C++ Guide](https://docs.aws.amazon.com/sdk-for-cpp/)

### 실습 도구
- MongoDB Compass (GUI 클라이언트)
- AWS DynamoDB Local (로컬 테스트)
- NoSQLBooster (MongoDB IDE)
- AWS CLI (DynamoDB 관리)

### 벤치마킹 도구
- YCSB (Yahoo! Cloud Serving Benchmark)
- MongoDB Benchmark Tool
- DynamoDB Performance Tester
- 커스텀 부하 테스트 스크립트

---

*🎮 게임 서버에서 NoSQL은 유연성과 확장성의 열쇠입니다. 하지만 "NoSQL = No Problem"이 아님을 기억하세요. 각 데이터베이스의 강점을 이해하고 적절히 활용하는 것이 성공의 비결입니다.*