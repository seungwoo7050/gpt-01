# 22단계: 실시간 분석 & 빅데이터 파이프라인 - 게임 데이터로 미래를 예측하기
*1초마다 100만 개의 이벤트를 처리하여 플레이어 행동을 실시간으로 분석하는 마법*

> **🎯 목표**: 게임에서 발생하는 대량의 실시간 데이터를 수집, 처리, 분석하여 비즈니스 의사결정에 활용할 수 있는 완전한 데이터 파이프라인 구축

---

## 📋 문서 정보

- **난이도**: 🟡 중급→🔴 고급 (데이터 사이언스의 절정!)
- **예상 학습 시간**: 12-15시간 (빅데이터는 시간이 필요!)
- **필요 선수 지식**: 
  - ✅ [1-21단계](./01_advanced_cpp_features.md) 모든 내용 완료
  - ✅ 기본적인 통계학 지식
  - ✅ 빅데이터 개념 (Hadoop, Spark 등)
  - ✅ 게임 운영 경험 또는 관심
- **실습 환경**: Apache Spark, Kafka, ClickHouse, Python
- **최종 결과물**: 넷플릭스급 실시간 추천 시스템!

---

## 🤔 왜 게임에서 실시간 분석이 중요할까?

### **현대 게임 비즈니스의 현실**

```cpp
// 😰 데이터 없이 운영하는 게임의 결말
void TraditionalGameOperation() {
    // 감으로 운영
    if (user_complaints > "많다고 느낌") {
        panic_patch();  // 급하게 패치
    }
    
    // 3개월 후
    if (revenue < expected) {
        std::cout << "왜 매출이 떨어졌을까?" << std::endl;
        // 이미 늦음 - 유저들 이미 이탈
    }
}

// ✅ 데이터 기반으로 운영하는 게임
class DataDrivenGameOperation {
public:
    void MonitorRealtime() {
        // 실시간 지표 모니터링
        if (concurrent_users < threshold) {
            TriggerEvent("사용자 감소 이벤트");
        }
        
        if (churn_prediction_model.predict(user) > 0.7f) {
            SendRetentionCampaign(user);  // 이탈 예상 유저에게 즉시 대응
        }
        
        if (revenue_anomaly_detected()) {
            AlertBusinessTeam();  // 매출 이상 즉시 감지
        }
    }
};
```

**데이터 기반 운영의 효과:**
- **유저 이탈률**: 30% → 15% (50% 감소)
- **매출 예측 정확도**: 60% → 85% (의사결정 품질 향상)
- **문제 대응 시간**: 3일 → 30분 (실시간 대응)
- **A/B 테스트 속도**: 1개월 → 1주일 (빠른 실험)

---

## 🏗️ 1. 게임 데이터 파이프라인 아키텍처

### **1.1 전체 아키텍처 개요**

```
📱 Game Clients (5,000+ users)
    ↓ (실시간 이벤트 전송)
🌐 API Gateway + Load Balancer
    ↓
📨 Message Queue (Apache Kafka)
    ↓ (스트림 처리)
⚡ Stream Processing (Apache Spark)
    ↓ (실시간 집계)
🗄️ Real-time Database (ClickHouse)
    ↓ (배치 처리)
📊 Data Warehouse (Apache Spark + Parquet)
    ↓ (머신러닝)
🤖 ML Pipeline (Python + MLflow)
    ↓ (시각화)
📈 Dashboard (Grafana + Custom WebApp)
```

### **1.2 데이터 레이어 구조**

```cpp
#include <nlohmann/json.hpp>
#include <chrono>
#include <string>

// 🎮 게임에서 발생하는 이벤트 타입들
enum class GameEventType {
    USER_LOGIN,           // 로그인
    USER_LOGOUT,          // 로그아웃
    LEVEL_UP,            // 레벨업
    ITEM_PURCHASE,       // 아이템 구매
    SKILL_USE,           // 스킬 사용
    PLAYER_DEATH,        // 플레이어 사망
    QUEST_COMPLETE,      // 퀘스트 완료
    CHAT_MESSAGE,        // 채팅
    GUILD_ACTION,        // 길드 관련 행동
    PVP_BATTLE,          // PvP 전투
    PAYMENT,             // 결제
    ERROR_OCCURRED       // 오류 발생
};

// 📊 표준화된 게임 이벤트 구조
struct GameEvent {
    std::string event_id;         // 고유 ID
    GameEventType event_type;     // 이벤트 타입
    uint21_t user_id;            // 사용자 ID
    std::string session_id;       // 세션 ID
    uint21_t timestamp_ms;        // 타임스탬프 (밀리초)
    std::string server_id;        // 서버 ID
    nlohmann::json properties;    // 이벤트별 추가 데이터
    std::string client_version;   // 클라이언트 버전
    std::string platform;         // 플랫폼 (iOS, Android, PC)
    
    // 스키마 검증
    bool IsValid() const {
        return !event_id.empty() && 
               user_id > 0 && 
               timestamp_ms > 0 &&
               !session_id.empty();
    }
    
    // JSON 직렬화
    nlohmann::json ToJson() const {
        return {
            {"event_id", event_id},
            {"event_type", static_cast<int>(event_type)},
            {"user_id", user_id},
            {"session_id", session_id},
            {"timestamp_ms", timestamp_ms},
            {"server_id", server_id},
            {"properties", properties},
            {"client_version", client_version},
            {"platform", platform}
        };
    }
    
    // JSON 역직렬화
    static GameEvent FromJson(const nlohmann::json& j) {
        GameEvent event;
        event.event_id = j["event_id"];
        event.event_type = static_cast<GameEventType>(j["event_type"]);
        event.user_id = j["user_id"];
        event.session_id = j["session_id"];
        event.timestamp_ms = j["timestamp_ms"];
        event.server_id = j["server_id"];
        event.properties = j["properties"];
        event.client_version = j["client_version"];
        event.platform = j["platform"];
        return event;
    }
};

// 🎯 특화된 이벤트 생성기들
class GameEventFactory {
public:
    static GameEvent CreateLoginEvent(uint21_t user_id, const std::string& session_id) {
        GameEvent event;
        event.event_id = GenerateUUID();
        event.event_type = GameEventType::USER_LOGIN;
        event.user_id = user_id;
        event.session_id = session_id;
        event.timestamp_ms = GetCurrentTimestampMs();
        event.properties = {
            {"login_method", "password"},
            {"device_type", "mobile"},
            {"ip_country", "KR"}
        };
        return event;
    }
    
    static GameEvent CreatePurchaseEvent(uint21_t user_id, const std::string& session_id,
                                       const std::string& item_id, int price, const std::string& currency) {
        GameEvent event;
        event.event_id = GenerateUUID();
        event.event_type = GameEventType::ITEM_PURCHASE;
        event.user_id = user_id;
        event.session_id = session_id;
        event.timestamp_ms = GetCurrentTimestampMs();
        event.properties = {
            {"item_id", item_id},
            {"price", price},
            {"currency", currency},
            {"payment_method", "credit_card"},
            {"promotion_id", ""},
            {"is_first_purchase", false}
        };
        return event;
    }
    
    static GameEvent CreateBattleEvent(uint21_t attacker_id, uint21_t defender_id,
                                     const std::string& session_id, const std::string& result) {
        GameEvent event;
        event.event_id = GenerateUUID();
        event.event_type = GameEventType::PVP_BATTLE;
        event.user_id = attacker_id;
        event.session_id = session_id;
        event.timestamp_ms = GetCurrentTimestampMs();
        event.properties = {
            {"defender_id", defender_id},
            {"battle_result", result},  // "win", "lose", "draw"
            {"battle_duration_sec", 45},
            {"damage_dealt", 1200},
            {"damage_received", 800},
            {"location", "arena_1"}
        };
        return event;
    }

private:
    static std::string GenerateUUID() {
        // UUID 생성 로직 (실제로는 uuid 라이브러리 사용)
        return "evt_" + std::to_string(GetCurrentTimestampMs()) + "_" + 
               std::to_string(rand() % 10000);
    }
    
    static uint21_t GetCurrentTimestampMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
};
```

---

## ⚡ 2. Apache Spark 스트리밍 구현

### **2.1 실시간 스트림 처리 엔진**

```cpp
// 🔥 고성능 이벤트 프로세서 (C++ → Python Spark 연동)
#include <kafka/kafka.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

class GameEventStreamProcessor {
private:
    kafka::Consumer consumer_;
    std::atomic<bool> running_{true};
    std::thread processing_thread_;
    
    // 실시간 집계 데이터
    struct RealTimeMetrics {
        std::atomic<uint21_t> concurrent_users{0};
        std::atomic<uint21_t> events_per_second{0};
        std::atomic<double> average_session_length{0};
        std::atomic<uint21_t> total_revenue_today{0};
        
        std::unordered_map<std::string, std::atomic<uint21_t>> events_by_type;
        std::unordered_map<std::string, std::atomic<uint21_t>> users_by_platform;
        
        mutable std::mutex metrics_mutex;
    };
    
    RealTimeMetrics metrics_;
    
public:
    GameEventStreamProcessor(const std::string& kafka_brokers, const std::string& topic) 
        : consumer_(kafka::Consumer::create({
            {"bootstrap.servers", kafka_brokers},
            {"group.id", "game_analytics_processor"},
            {"auto.offset.reset", "latest"},
            {"enable.auto.commit", "true"}
        })) {
        
        consumer_.subscribe({topic});
        
        // 처리 스레드 시작
        processing_thread_ = std::thread(&GameEventStreamProcessor::ProcessEvents, this);
    }
    
    ~GameEventStreamProcessor() {
        Stop();
    }
    
    void Stop() {
        running_ = false;
        if (processing_thread_.joinable()) {
            processing_thread_.join();
        }
    }
    
    // 📊 실시간 메트릭 수집
    void ProcessEvents() {
        const std::chrono::seconds timeout(1);
        
        while (running_) {
            auto messages = consumer_.poll(timeout);
            
            for (auto& message : messages) {
                if (message.get_error()) {
                    std::cerr << "Kafka error: " << message.get_error() << std::endl;
                    continue;
                }
                
                try {
                    // JSON 파싱
                    auto json_data = nlohmann::json::parse(message.get_payload());
                    auto event = GameEvent::FromJson(json_data);
                    
                    if (!event.IsValid()) {
                        std::cerr << "Invalid event: " << event.event_id << std::endl;
                        continue;
                    }
                    
                    // 실시간 집계 업데이트
                    UpdateRealTimeMetrics(event);
                    
                    // 특별한 처리가 필요한 이벤트들
                    ProcessSpecialEvents(event);
                    
                } catch (const std::exception& e) {
                    std::cerr << "Event processing error: " << e.what() << std::endl;
                }
            }
            
            // 매 초마다 메트릭을 ClickHouse에 전송
            static auto last_flush = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            if (now - last_flush >= std::chrono::seconds(1)) {
                FlushMetricsToClickHouse();
                last_flush = now;
            }
        }
    }
    
private:
    void UpdateRealTimeMetrics(const GameEvent& event) {
        metrics_.events_per_second++;
        
        // 이벤트 타입별 카운팅
        std::string event_type_str = std::to_string(static_cast<int>(event.event_type));
        metrics_.events_by_type[event_type_str]++;
        
        // 플랫폼별 사용자 카운팅
        metrics_.users_by_platform[event.platform]++;
        
        // 특별한 이벤트 처리
        switch (event.event_type) {
            case GameEventType::USER_LOGIN:
                metrics_.concurrent_users++;
                break;
                
            case GameEventType::USER_LOGOUT:
                if (metrics_.concurrent_users > 0) {
                    metrics_.concurrent_users--;
                }
                break;
                
            case GameEventType::ITEM_PURCHASE:
                if (event.properties.contains("price")) {
                    int price = event.properties["price"];
                    metrics_.total_revenue_today += price;
                }
                break;
        }
    }
    
    void ProcessSpecialEvents(const GameEvent& event) {
        // 🚨 이상 징후 탐지
        if (event.event_type == GameEventType::ERROR_OCCURRED) {
            std::string error_type = event.properties.value("error_type", "unknown");
            
            // 크리티컬 에러는 즉시 알림
            if (error_type == "crash" || error_type == "memory_leak") {
                SendAlertToDevTeam(event);
            }
        }
        
        // 💰 고액 결제 모니터링
        if (event.event_type == GameEventType::PAYMENT) {
            int amount = event.properties.value("amount", 0);
            if (amount > 10000) {  // 100달러 이상
                LogHighValueTransaction(event);
            }
        }
        
        // 🎯 특이한 행동 패턴 감지
        if (event.event_type == GameEventType::SKILL_USE) {
            std::string skill_id = event.properties.value("skill_id", "");
            DetectSuspiciousActivity(event.user_id, skill_id, event.timestamp_ms);
        }
    }
    
    void FlushMetricsToClickHouse() {
        // ClickHouse에 실시간 메트릭 전송
        std::lock_guard<std::mutex> lock(metrics_.metrics_mutex);
        
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            now.time_since_epoch()).count();
        
        std::string query = fmt::format(
            "INSERT INTO realtime_metrics "
            "(timestamp, concurrent_users, events_per_second, total_revenue_today) "
            "VALUES ({}, {}, {}, {})",
            timestamp,
            metrics_.concurrent_users.load(),
            metrics_.events_per_second.exchange(0),  // 리셋하면서 읽기
            metrics_.total_revenue_today.load()
        );
        
        // ClickHouse 클라이언트로 전송
        ExecuteClickHouseQuery(query);
    }
    
    void SendAlertToDevTeam(const GameEvent& event) {
        // Slack, PagerDuty 등으로 즉시 알림
        std::cout << "🚨 CRITICAL ALERT: " << event.ToJson().dump() << std::endl;
    }
    
    void LogHighValueTransaction(const GameEvent& event) {
        // 고액 결제 로깅 (사기 방지)
        std::cout << "💰 High Value Transaction: " << event.ToJson().dump() << std::endl;
    }
    
    void DetectSuspiciousActivity(uint21_t user_id, const std::string& skill_id, uint21_t timestamp) {
        // 매크로 사용 의심 행동 감지
        static std::unordered_map<uint21_t, std::vector<uint21_t>> user_skill_timings;
        
        auto& timings = user_skill_timings[user_id];
        timings.push_back(timestamp);
        
        // 최근 10초간의 스킬 사용만 유지
        auto cutoff = timestamp - 10000;  // 10초 전
        timings.erase(
            std::remove_if(timings.begin(), timings.end(),
                [cutoff](uint21_t t) { return t < cutoff; }),
            timings.end()
        );
        
        // 10초에 50번 이상 스킬 사용 시 의심
        if (timings.size() >= 50) {
            std::cout << "🤖 Suspicious Activity Detected: User " << user_id 
                      << " used skills " << timings.size() << " times in 10 seconds" << std::endl;
        }
    }
    
    void ExecuteClickHouseQuery(const std::string& query) {
        // 실제 ClickHouse 클라이언트 구현
        // clickhouse-cpp 라이브러리 사용
        std::cout << "ClickHouse Query: " << query << std::endl;
    }
};
```

### **2.2 Spark Streaming Python 코드**

```python
# 🐍 Apache Spark로 대규모 스트림 처리
from pyspark.sql import SparkSession
from pyspark.sql.functions import *
from pyspark.sql.types import *
import json

class GameAnalyticsSparkProcessor:
    def __init__(self):
        self.spark = SparkSession.builder \
            .appName("GameAnalyticsProcessor") \
            .config("spark.sql.streaming.checkpointLocation", "/tmp/checkpoints") \
            .config("spark.sql.adaptive.enabled", "true") \
            .config("spark.sql.adaptive.coalescePartitions.enabled", "true") \
            .getOrCreate()
        
        self.spark.sparkContext.setLogLevel("WARN")
    
    def process_game_events(self):
        """게임 이벤트 실시간 처리"""
        
        # Kafka에서 스트림 읽기
        raw_stream = self.spark \
            .readStream \
            .format("kafka") \
            .option("kafka.bootstrap.servers", "localhost:9092") \
            .option("subscribe", "game_events") \
            .option("startingOffsets", "latest") \
            .load()
        
        # 이벤트 스키마 정의
        event_schema = StructType([
            StructField("event_id", StringType(), True),
            StructField("event_type", IntegerType(), True),
            StructField("user_id", LongType(), True),
            StructField("session_id", StringType(), True),
            StructField("timestamp_ms", LongType(), True),
            StructField("server_id", StringType(), True),
            StructField("properties", MapType(StringType(), StringType()), True),
            StructField("client_version", StringType(), True),
            StructField("platform", StringType(), True)
        ])
        
        # JSON 파싱 및 변환
        parsed_events = raw_stream \
            .select(from_json(col("value").cast("string"), event_schema).alias("event")) \
            .select("event.*") \
            .withColumn("timestamp", to_timestamp(col("timestamp_ms") / 1000)) \
            .withColumn("hour", hour("timestamp")) \
            .withColumn("date", to_date("timestamp"))
        
        # 🎯 실시간 집계 1: 시간당 활성 사용자 수 (DAU/MAU 계산)
        hourly_active_users = parsed_events \
            .withWatermark("timestamp", "1 minute") \
            .groupBy(
                window("timestamp", "1 hour"),
                "platform"
            ) \
            .agg(
                countDistinct("user_id").alias("active_users"),
                count("*").alias("total_events")
            ) \
            .writeStream \
            .outputMode("update") \
            .format("console") \
            .trigger(processingTime="30 seconds") \
            .start()
        
        # 🎯 실시간 집계 2: 매출 모니터링
        revenue_stream = parsed_events \
            .filter(col("event_type") == 4)  # ITEM_PURCHASE \
            .select(
                "timestamp",
                "user_id",
                get_json_object("properties", "$.price").cast("double").alias("price"),
                get_json_object("properties", "$.currency").alias("currency"),
                get_json_object("properties", "$.item_id").alias("item_id")
            ) \
            .withWatermark("timestamp", "1 minute") \
            .groupBy(
                window("timestamp", "5 minutes"),
                "currency"
            ) \
            .agg(
                sum("price").alias("total_revenue"),
                count("*").alias("transaction_count"),
                countDistinct("user_id").alias("paying_users")
            ) \
            .writeStream \
            .outputMode("update") \
            .format("console") \
            .trigger(processingTime="10 seconds") \
            .start()
        
        # 🎯 실시간 집계 3: 게임플레이 패턴 분석
        gameplay_analysis = parsed_events \
            .filter(col("event_type").isin([2, 3, 5, 6]))  # LEVEL_UP, SKILL_USE, PLAYER_DEATH, QUEST_COMPLETE \
            .withWatermark("timestamp", "2 minutes") \
            .groupBy(
                window("timestamp", "10 minutes"),
                "event_type",
                get_json_object("properties", "$.level").cast("int").alias("player_level")
            ) \
            .agg(
                count("*").alias("event_count"),
                countDistinct("user_id").alias("unique_players")
            ) \
            .writeStream \
            .outputMode("update") \
            .format("console") \
            .trigger(processingTime="30 seconds") \
            .start()
        
        # 🚨 실시간 이상 탐지: 급격한 이탈률 증가
        churn_detection = parsed_events \
            .filter(col("event_type") == 1)  # USER_LOGOUT \
            .withWatermark("timestamp", "1 minute") \
            .groupBy(window("timestamp", "5 minutes")) \
            .agg(
                count("*").alias("logout_count"),
                countDistinct("user_id").alias("users_logged_out")
            ) \
            .withColumn("logout_rate", col("logout_count") / col("users_logged_out")) \
            .filter(col("logout_rate") > 2.0)  # 평균보다 2배 높은 이탈률 \
            .writeStream \
            .outputMode("update") \
            .foreachBatch(self.send_churn_alert) \
            .trigger(processingTime="1 minute") \
            .start()
        
        return [hourly_active_users, revenue_stream, gameplay_analysis, churn_detection]
    
    def send_churn_alert(self, batch_df, batch_id):
        """이탈률 급증 알림"""
        if batch_df.count() > 0:
            print(f"🚨 CHURN ALERT - Batch {batch_id}:")
            batch_df.show(truncate=False)
            
            # 실제로는 Slack, PagerDuty 등으로 알림 발송
            # self.send_slack_notification(batch_df)
    
    def create_real_time_dashboard_data(self):
        """실시간 대시보드용 데이터 생성"""
        
        # Kafka에서 읽기
        stream = self.spark \
            .readStream \
            .format("kafka") \
            .option("kafka.bootstrap.servers", "localhost:9092") \
            .option("subscribe", "game_events") \
            .load()
        
        # 대시보드용 실시간 메트릭 계산
        dashboard_metrics = stream \
            .select(from_json(col("value").cast("string"), self.get_event_schema()).alias("event")) \
            .select("event.*") \
            .withColumn("timestamp", to_timestamp(col("timestamp_ms") / 1000)) \
            .withWatermark("timestamp", "30 seconds") \
            .groupBy(window("timestamp", "1 minute")) \
            .agg(
                count("*").alias("events_per_minute"),
                countDistinct("user_id").alias("concurrent_users"),
                sum(when(col("event_type") == 4, 
                         get_json_object("properties", "$.price").cast("double"))
                    .otherwise(0)).alias("revenue_per_minute"),
                countDistinct(when(col("event_type") == 0, col("user_id"))).alias("new_logins"),
                countDistinct(when(col("event_type") == 1, col("user_id"))).alias("logouts")
            ) \
            .select(
                col("window.start").alias("timestamp"),
                "*"
            ) \
            .drop("window")
        
        # ClickHouse에 실시간 저장
        query = dashboard_metrics \
            .writeStream \
            .outputMode("update") \
            .format("jdbc") \
            .option("url", "jdbc:clickhouse://localhost:8123/analytics") \
            .option("dbtable", "real_time_dashboard") \
            .option("user", "default") \
            .option("driver", "ru.yandex.clickhouse.ClickHouseDriver") \
            .trigger(processingTime="10 seconds") \
            .start()
        
        return query
    
    def get_event_schema(self):
        """이벤트 스키마 반환"""
        return StructType([
            StructField("event_id", StringType(), True),
            StructField("event_type", IntegerType(), True),
            StructField("user_id", LongType(), True),
            StructField("session_id", StringType(), True),
            StructField("timestamp_ms", LongType(), True),
            StructField("server_id", StringType(), True),
            StructField("properties", MapType(StringType(), StringType()), True),
            StructField("client_version", StringType(), True),
            StructField("platform", StringType(), True)
        ])

# 🚀 실행 코드
if __name__ == "__main__":
    processor = GameAnalyticsSparkProcessor()
    
    # 모든 스트림 시작
    streams = processor.process_game_events()
    dashboard_stream = processor.create_real_time_dashboard_data()
    
    # 모든 스트림이 완료될 때까지 대기
    for stream in streams + [dashboard_stream]:
        stream.awaitTermination()
```

---

## 🗄️ 3. ClickHouse 실시간 분석 데이터베이스

### **3.1 ClickHouse 스키마 설계**

```sql
-- 📊 게임 이벤트 원본 데이터 테이블 (MergeTree 엔진)
CREATE TABLE game_events (
    event_id String,
    event_type UInt8,
    user_id UInt64,
    session_id String,
    timestamp DateTime64(3),
    server_id String,
    properties String,  -- JSON 형태로 저장
    client_version String,
    platform LowCardinality(String),  -- 카디널리티 최적화
    date Date MATERIALIZED toDate(timestamp)  -- 파티셔닝용
) ENGINE = MergeTree()
PARTITION BY toYYYYMM(timestamp)  -- 월별 파티셔닝
ORDER BY (event_type, user_id, timestamp)
TTL timestamp + INTERVAL 1 YEAR DELETE;  -- 1년 후 자동 삭제

-- 📈 실시간 메트릭 집계 테이블
CREATE TABLE realtime_metrics (
    timestamp DateTime,
    concurrent_users UInt32,
    events_per_second UInt32,
    total_revenue_today UInt64,
    platform LowCardinality(String),
    server_id String
) ENGINE = MergeTree()
ORDER BY (timestamp, platform, server_id)
TTL timestamp + INTERVAL 30 DAY DELETE;

-- 💰 매출 분석용 집계 테이블
CREATE TABLE revenue_analytics (
    date Date,
    hour UInt8,
    platform LowCardinality(String),
    currency LowCardinality(String),
    total_revenue Decimal64(2),
    transaction_count UInt32,
    paying_users UInt32,
    arpu Decimal64(2) MATERIALIZED total_revenue / paying_users  -- 계산된 컬럼
) ENGINE = SummingMergeTree()  -- 자동 집계
ORDER BY (date, hour, platform, currency);

-- 🎮 사용자 행동 분석 테이블
CREATE TABLE user_behavior_summary (
    user_id UInt64,
    date Date,
    session_count UInt16,
    total_playtime_minutes UInt32,
    events_count UInt32,
    revenue_spent Decimal64(2),
    level_start UInt16,
    level_end UInt16,
    last_login DateTime
) ENGINE = ReplacingMergeTree(last_login)  -- 중복 제거
ORDER BY (user_id, date);

-- 🏆 실시간 랭킹 테이블
CREATE TABLE player_rankings (
    ranking_type LowCardinality(String),  -- 'level', 'pvp_rating', 'guild_contribution'
    user_id UInt64,
    player_name String,
    score UInt64,
    rank UInt32,
    updated_at DateTime DEFAULT now()
) ENGINE = ReplacingMergeTree(updated_at)
ORDER BY (ranking_type, rank);

-- 📊 실시간 대시보드용 Materialized View
CREATE MATERIALIZED VIEW realtime_dashboard_mv
TO realtime_dashboard_table
AS SELECT
    toStartOfMinute(timestamp) as minute,
    count() as events_count,
    uniq(user_id) as unique_users,
    sum(if(event_type = 4, extractFloat64(JSONExtract(properties, 'price')), 0)) as revenue,
    platform
FROM game_events
GROUP BY minute, platform;

-- 🎯 사용자 리텐션 분석 쿼리
CREATE VIEW user_retention_view AS
SELECT
    toDate(first_login) as cohort_date,
    dateDiff('day', first_login, activity_date) as day_number,
    count(DISTINCT user_id) as active_users,
    count(DISTINCT user_id) / any(cohort_size) as retention_rate
FROM (
    SELECT 
        user_id,
        min(timestamp) as first_login,
        toDate(timestamp) as activity_date
    FROM game_events 
    WHERE event_type = 0  -- LOGIN
    GROUP BY user_id, activity_date
) a
LEFT JOIN (
    SELECT 
        toDate(min(timestamp)) as cohort_date,
        count(DISTINCT user_id) as cohort_size
    FROM game_events 
    WHERE event_type = 0
    GROUP BY cohort_date
) b ON toDate(first_login) = b.cohort_date
GROUP BY cohort_date, day_number
ORDER BY cohort_date, day_number;
```

### **3.2 고성능 분석 쿼리들**

```sql
-- 🔥 실시간 대시보드 KPI 쿼리 (1초 이내 응답)

-- 1️⃣ 현재 동접자 수 및 트렌드
SELECT 
    toStartOfMinute(now() - INTERVAL 10 MINUTE) as time_window,
    count(DISTINCT user_id) as concurrent_users,
    avg(concurrent_users) OVER (ORDER BY time_window ROWS 4 PRECEDING) as trend_avg
FROM game_events 
WHERE timestamp >= now() - INTERVAL 10 MINUTE
  AND event_type IN (0, 1)  -- LOGIN, LOGOUT
GROUP BY time_window
ORDER BY time_window;

-- 2️⃣ 실시간 매출 모니터링 (분당 업데이트)
SELECT 
    toStartOfHour(timestamp) as hour,
    sum(toFloat64(JSONExtract(properties, 'price'))) as hourly_revenue,
    count() as transaction_count,
    uniq(user_id) as paying_users,
    hourly_revenue / paying_users as arpu
FROM game_events 
WHERE event_type = 4  -- ITEM_PURCHASE
  AND timestamp >= today()
GROUP BY hour
ORDER BY hour DESC
LIMIT 24;

-- 3️⃣ 서버별 성능 모니터링
SELECT 
    server_id,
    count() as events_count,
    uniq(user_id) as unique_users,
    avg(toFloat64(JSONExtract(properties, 'response_time_ms'))) as avg_response_time,
    quantile(0.95)(toFloat64(JSONExtract(properties, 'response_time_ms'))) as p95_response_time
FROM game_events 
WHERE timestamp >= now() - INTERVAL 1 HOUR
GROUP BY server_id
HAVING unique_users > 10  -- 최소 사용자 수 필터
ORDER BY avg_response_time DESC;

-- 4️⃣ 치트 의심 활동 탐지
WITH suspicious_activities AS (
    SELECT 
        user_id,
        count() as actions_per_minute,
        uniq(JSONExtract(properties, 'skill_id')) as unique_skills,
        max(toFloat64(JSONExtract(properties, 'damage'))) as max_damage
    FROM game_events 
    WHERE event_type = 3  -- SKILL_USE
      AND timestamp >= now() - INTERVAL 1 MINUTE
    GROUP BY user_id
)
SELECT 
    user_id,
    actions_per_minute,
    unique_skills,
    max_damage,
    'possible_macro' as alert_type
FROM suspicious_activities
WHERE actions_per_minute > 60  -- 분당 60회 이상 액션
   OR (unique_skills = 1 AND actions_per_minute > 30)  -- 같은 스킬 반복
   OR max_damage > 50000;  -- 비정상적 고데미지

-- 5️⃣ A/B 테스트 실시간 결과
SELECT 
    JSONExtract(properties, 'ab_test_group') as test_group,
    count(DISTINCT user_id) as participants,
    count(DISTINCT if(event_type = 4, user_id, NULL)) as converters,
    converters / participants as conversion_rate,
    sum(if(event_type = 4, toFloat64(JSONExtract(properties, 'price')), 0)) as revenue,
    revenue / participants as revenue_per_user
FROM game_events 
WHERE timestamp >= today()
  AND JSONHas(properties, 'ab_test_group')
GROUP BY test_group
ORDER BY conversion_rate DESC;

-- 6️⃣ 게임 밸런스 분석 (레벨별 진행 속도)
SELECT 
    toInt32(JSONExtract(properties, 'from_level')) as from_level,
    toInt32(JSONExtract(properties, 'to_level')) as to_level,
    count() as levelups_count,
    avg(toFloat64(JSONExtract(properties, 'time_spent_minutes'))) as avg_time_to_levelup,
    quantile(0.5)(toFloat64(JSONExtract(properties, 'time_spent_minutes'))) as median_time
FROM game_events 
WHERE event_type = 2  -- LEVEL_UP
  AND timestamp >= today() - INTERVAL 7 DAY
GROUP BY from_level, to_level
HAVING levelups_count >= 100  -- 통계적 유의성
ORDER BY from_level, to_level;

-- 7️⃣ 실시간 이탈 위험 사용자 식별
WITH user_sessions AS (
    SELECT 
        user_id,
        min(timestamp) as session_start,
        max(timestamp) as session_end,
        dateDiff('minute', session_start, session_end) as session_length,
        count() as events_in_session
    FROM game_events 
    WHERE timestamp >= now() - INTERVAL 2 HOUR
    GROUP BY user_id, session_id
),
recent_behavior AS (
    SELECT 
        user_id,
        avg(session_length) as avg_session_length,
        avg(events_in_session) as avg_events_per_session,
        count() as sessions_count
    FROM user_sessions
    GROUP BY user_id
)
SELECT 
    user_id,
    avg_session_length,
    avg_events_per_session,
    sessions_count,
    'at_risk' as status
FROM recent_behavior
WHERE avg_session_length < 5  -- 평균 5분 미만 세션
   OR avg_events_per_session < 10  -- 세션당 이벤트 10개 미만
   OR sessions_count = 1  -- 재접속 없음
ORDER BY avg_session_length ASC;
```

---

## 🤖 4. 머신러닝 파이프라인

### **4.1 사용자 이탈 예측 모델**

```python
# 🧠 MLflow 기반 모델 관리 시스템
import mlflow
import mlflow.sklearn
import pandas as pd
import numpy as np
from sklearn.ensemble import RandomForestClassifier, GradientBoostingClassifier
from sklearn.linear_model import LogisticRegression
from sklearn.model_selection import train_test_split, cross_val_score
from sklearn.metrics import classification_report, roc_auc_score, confusion_matrix
from sklearn.preprocessing import StandardScaler, LabelEncoder
import clickhouse_connect
import logging
from datetime import datetime, timedelta

class ChurnPredictionPipeline:
    def __init__(self, clickhouse_client):
        self.client = clickhouse_client
        self.models = {}
        self.scalers = {}
        self.feature_importance = {}
        
        # MLflow 설정
        mlflow.set_tracking_uri("http://localhost:5000")
        mlflow.set_experiment("churn_prediction")
    
    def extract_features(self, days_back=30):
        """ClickHouse에서 사용자 특성 추출"""
        
        # 🎯 고급 피처 엔지니어링 쿼리
        feature_query = f"""
        WITH user_stats AS (
            SELECT 
                user_id,
                -- 기본 통계
                count() as total_events,
                uniq(toDate(timestamp)) as active_days,
                dateDiff('day', min(timestamp), max(timestamp)) + 1 as user_lifetime_days,
                
                -- 세션 분석
                uniq(session_id) as total_sessions,
                avg(session_length_minutes) as avg_session_length,
                max(session_length_minutes) as max_session_length,
                
                -- 게임플레이 패턴
                countIf(event_type = 2) as level_ups,  -- LEVEL_UP
                countIf(event_type = 3) as skill_uses,  -- SKILL_USE
                countIf(event_type = 6) as quest_completes,  -- QUEST_COMPLETE
                countIf(event_type = 9) as pvp_battles,  -- PVP_BATTLE
                
                -- 소셜 활동
                countIf(event_type = 7) as chat_messages,  -- CHAT_MESSAGE
                countIf(event_type = 8) as guild_activities,  -- GUILD_ACTION
                
                -- 경제 활동
                countIf(event_type = 4) as purchases,  -- ITEM_PURCHASE
                sum(if(event_type = 4, toFloat64(JSONExtract(properties, 'price')), 0)) as total_spent,
                
                -- 최근 활동 (지난 7일)
                countIf(timestamp >= now() - INTERVAL 7 DAY) as recent_events,
                countIf(timestamp >= now() - INTERVAL 7 DAY AND event_type = 0) as recent_logins,
                
                -- 플랫폼 및 기기 정보
                uniq(platform) as platforms_used,
                uniq(JSONExtract(properties, 'device_type')) as devices_used,
                
                -- 시간 패턴
                uniq(toHour(timestamp)) as unique_hours_played,
                countIf(toHour(timestamp) BETWEEN 22 AND 6) as late_night_events,
                
                -- 마지막 활동
                max(timestamp) as last_activity,
                dateDiff('day', max(timestamp), now()) as days_since_last_activity
                
            FROM (
                SELECT 
                    *,
                    -- 세션 길이 계산
                    dateDiff('minute', 
                        lag(timestamp) OVER (PARTITION BY user_id, session_id ORDER BY timestamp),
                        timestamp
                    ) as session_length_minutes
                FROM game_events 
                WHERE timestamp >= now() - INTERVAL {days_back} DAY
            )
            GROUP BY user_id
            HAVING total_events >= 10  -- 최소 활동량 필터
        ),
        churn_labels AS (
            SELECT 
                user_id,
                if(days_since_last_activity >= 7, 1, 0) as is_churned  -- 7일 이상 미접속 = 이탈
            FROM user_stats
        )
        SELECT 
            s.*,
            c.is_churned,
            -- 파생 피처들
            total_events / user_lifetime_days as events_per_day,
            total_spent / greatest(purchases, 1) as avg_purchase_amount,
            recent_events / greatest(total_events, 1) as recent_activity_ratio,
            level_ups / greatest(user_lifetime_days, 1) as levelup_velocity,
            chat_messages / greatest(total_events, 1) as social_engagement_ratio
        FROM user_stats s
        JOIN churn_labels c ON s.user_id = c.user_id
        """
        
        return self.client.query_df(feature_query)
    
    def preprocess_features(self, df):
        """특성 전처리 및 엔지니어링"""
        
        # 🧹 데이터 정제
        df = df.fillna(0)
        
        # 📊 추가 파생 변수 생성
        df['spending_category'] = pd.cut(df['total_spent'], 
                                       bins=[0, 10, 100, 1000, float('inf')],
                                       labels=['free', 'light_spender', 'moderate_spender', 'whale'])
        
        df['engagement_score'] = (
            df['active_days'] * 0.3 +
            df['avg_session_length'] * 0.2 +
            df['social_engagement_ratio'] * 100 * 0.3 +
            np.log1p(df['total_spent']) * 0.2
        )
        
        df['gameplay_diversity'] = (
            (df['level_ups'] > 0).astype(int) +
            (df['skill_uses'] > 0).astype(int) +
            (df['quest_completes'] > 0).astype(int) +
            (df['pvp_battles'] > 0).astype(int) +
            (df['purchases'] > 0).astype(int)
        )
        
        # 🏷️ 범주형 변수 인코딩
        le = LabelEncoder()
        if 'spending_category' in df.columns:
            df['spending_category_encoded'] = le.fit_transform(df['spending_category'].astype(str))
        
        # 📈 정규화가 필요한 수치형 변수들
        numeric_features = [
            'total_events', 'avg_session_length', 'total_spent', 'engagement_score',
            'events_per_day', 'levelup_velocity', 'social_engagement_ratio'
        ]
        
        scaler = StandardScaler()
        df[numeric_features] = scaler.fit_transform(df[numeric_features])
        self.scalers['features'] = scaler
        
        return df
    
    def train_models(self, df):
        """다중 모델 훈련 및 비교"""
        
        # 피처와 타겟 분리
        feature_columns = [col for col in df.columns 
                          if col not in ['user_id', 'is_churned', 'spending_category']]
        X = df[feature_columns]
        y = df['is_churned']
        
        # 훈련/테스트 분할
        X_train, X_test, y_train, y_test = train_test_split(
            X, y, test_size=0.2, random_state=42, stratify=y
        )
        
        # 📊 모델들 정의
        models_to_train = {
            'random_forest': RandomForestClassifier(
                n_estimators=100, max_depth=10, random_state=42
            ),
            'gradient_boosting': GradientBoostingClassifier(
                n_estimators=100, max_depth=6, random_state=42
            ),
            'logistic_regression': LogisticRegression(
                random_state=42, max_iter=1000
            )
        }
        
        best_model = None
        best_score = 0
        
        for model_name, model in models_to_train.items():
            with mlflow.start_run(run_name=f"churn_{model_name}_{datetime.now().strftime('%Y%m%d_%H%M')}"):
                
                # 모델 훈련
                model.fit(X_train, y_train)
                
                # 예측 및 평가
                y_pred = model.predict(X_test)
                y_pred_proba = model.predict_proba(X_test)[:, 1]
                
                # 메트릭 계산
                auc_score = roc_auc_score(y_test, y_pred_proba)
                cv_scores = cross_val_score(model, X_train, y_train, cv=5, scoring='roc_auc')
                
                # MLflow 로깅
                mlflow.log_param("model_type", model_name)
                mlflow.log_param("train_size", len(X_train))
                mlflow.log_param("test_size", len(X_test))
                mlflow.log_metric("auc_score", auc_score)
                mlflow.log_metric("cv_mean", cv_scores.mean())
                mlflow.log_metric("cv_std", cv_scores.std())
                
                # 특성 중요도 (Random Forest, Gradient Boosting만)
                if hasattr(model, 'feature_importances_'):
                    feature_importance = dict(zip(feature_columns, model.feature_importances_))
                    self.feature_importance[model_name] = feature_importance
                    
                    # 상위 10개 특성 로깅
                    top_features = sorted(feature_importance.items(), 
                                        key=lambda x: x[1], reverse=True)[:10]
                    for i, (feature, importance) in enumerate(top_features):
                        mlflow.log_metric(f"feature_importance_{i+1}", importance)
                        mlflow.log_param(f"top_feature_{i+1}", feature)
                
                # 모델 저장
                mlflow.sklearn.log_model(model, f"churn_model_{model_name}")
                
                # 최고 성능 모델 추적
                if auc_score > best_score:
                    best_score = auc_score
                    best_model = model_name
                    self.models['best_churn_model'] = model
                
                print(f"{model_name} - AUC: {auc_score:.4f}, CV: {cv_scores.mean():.4f}±{cv_scores.std():.4f}")
        
        print(f"\n🏆 Best Model: {best_model} with AUC: {best_score:.4f}")
        return best_model, best_score
    
    def predict_churn_risk(self, user_ids):
        """실시간 이탈 위험도 예측"""
        
        if 'best_churn_model' not in self.models:
            raise ValueError("모델이 훈련되지 않았습니다.")
        
        # 사용자별 최신 특성 추출
        user_features_query = f"""
        SELECT 
            user_id,
            count() as total_events,
            dateDiff('day', min(timestamp), max(timestamp)) + 1 as user_lifetime_days,
            uniq(session_id) as total_sessions,
            avg(session_length_minutes) as avg_session_length,
            countIf(event_type = 4) as purchases,
            sum(if(event_type = 4, toFloat64(JSONExtract(properties, 'price')), 0)) as total_spent,
            countIf(timestamp >= now() - INTERVAL 7 DAY) as recent_events,
            dateDiff('day', max(timestamp), now()) as days_since_last_activity
        FROM game_events 
        WHERE user_id IN ({','.join(map(str, user_ids))})
        GROUP BY user_id
        """
        
        df = self.client.query_df(user_features_query)
        df = self.preprocess_features(df)
        
        # 예측
        model = self.models['best_churn_model']
        feature_columns = [col for col in df.columns 
                          if col not in ['user_id', 'is_churned', 'spending_category']]
        
        predictions = model.predict_proba(df[feature_columns])[:, 1]
        
        # 결과 구성
        results = []
        for i, user_id in enumerate(df['user_id']):
            risk_score = predictions[i]
            risk_level = 'high' if risk_score > 0.7 else 'medium' if risk_score > 0.3 else 'low'
            
            results.append({
                'user_id': user_id,
                'churn_probability': risk_score,
                'risk_level': risk_level,
                'predicted_at': datetime.now()
            })
        
        return results
    
    def schedule_daily_predictions(self):
        """일일 배치 예측 작업"""
        
        # 활성 사용자 목록 조회
        active_users_query = """
        SELECT DISTINCT user_id 
        FROM game_events 
        WHERE timestamp >= now() - INTERVAL 30 DAY
        LIMIT 10000  -- 배치 크기 제한
        """
        
        user_ids = self.client.query_df(active_users_query)['user_id'].tolist()
        
        if not user_ids:
            print("예측할 활성 사용자가 없습니다.")
            return
        
        # 배치 예측 실행
        predictions = self.predict_churn_risk(user_ids)
        
        # 결과를 ClickHouse에 저장
        self.save_predictions_to_db(predictions)
        
        # 고위험 사용자 알림
        high_risk_users = [p for p in predictions if p['risk_level'] == 'high']
        if high_risk_users:
            self.send_churn_alert(high_risk_users)
    
    def save_predictions_to_db(self, predictions):
        """예측 결과를 데이터베이스에 저장"""
        
        # ClickHouse 테이블 생성 (최초 1회)
        create_table_query = """
        CREATE TABLE IF NOT EXISTS churn_predictions (
            user_id UInt64,
            churn_probability Float64,
            risk_level LowCardinality(String),
            predicted_at DateTime,
            model_version String
        ) ENGINE = ReplacingMergeTree(predicted_at)
        ORDER BY (user_id, predicted_at)
        """
        
        self.client.command(create_table_query)
        
        # 예측 결과 삽입
        for pred in predictions:
            insert_query = f"""
            INSERT INTO churn_predictions VALUES 
            ({pred['user_id']}, {pred['churn_probability']}, 
             '{pred['risk_level']}', '{pred['predicted_at']}', 'v1.0')
            """
            self.client.command(insert_query)
    
    def send_churn_alert(self, high_risk_users):
        """이탈 위험 사용자 알림"""
        print(f"🚨 {len(high_risk_users)}명의 고위험 이탈 사용자 감지!")
        for user in high_risk_users[:5]:  # 상위 5명만 출력
            print(f"User {user['user_id']}: {user['churn_probability']:.2%} 이탈 확률")

# 🚀 사용 예제
def run_churn_prediction_pipeline():
    # ClickHouse 연결
    client = clickhouse_connect.get_client(
        host='localhost',
        port=8123,
        database='analytics',
        username='default'
    )
    
    # 파이프라인 실행
    pipeline = ChurnPredictionPipeline(client)
    
    # 데이터 추출 및 전처리
    print("📊 사용자 특성 데이터 추출 중...")
    df = pipeline.extract_features(days_back=30)
    df = pipeline.preprocess_features(df)
    
    print(f"총 {len(df)}명의 사용자 데이터 추출 완료")
    print(f"이탈 사용자 비율: {df['is_churned'].mean():.2%}")
    
    # 모델 훈련
    print("\n🤖 머신러닝 모델 훈련 중...")
    best_model, best_score = pipeline.train_models(df)
    
    # 일일 예측 실행
    print("\n🔮 일일 이탈 위험 예측 실행...")
    pipeline.schedule_daily_predictions()

if __name__ == "__main__":
    run_churn_prediction_pipeline()
```

### **4.2 게임 밸런스 최적화 모델**

```python
# ⚖️ 게임 밸런스 자동 조정 시스템
import numpy as np
import pandas as pd
from scipy import optimize
from sklearn.cluster import KMeans
from sklearn.ensemble import IsolationForest
import matplotlib.pyplot as plt
import seaborn as sns

class GameBalanceOptimizer:
    def __init__(self, clickhouse_client):
        self.client = clickhouse_client
        self.balance_history = []
        self.current_config = {}
    
    def analyze_weapon_balance(self):
        """무기 밸런스 분석"""
        
        weapon_stats_query = """
        WITH weapon_usage AS (
            SELECT 
                JSONExtract(properties, 'weapon_id') as weapon_id,
                JSONExtract(properties, 'damage_dealt') as damage,
                JSONExtract(properties, 'battle_result') as result,
                user_id,
                timestamp
            FROM game_events 
            WHERE event_type = 9  -- PVP_BATTLE
              AND timestamp >= now() - INTERVAL 7 DAY
              AND JSONHas(properties, 'weapon_id')
        )
        SELECT 
            weapon_id,
            count() as usage_count,
            avg(toFloat64(damage)) as avg_damage,
            countIf(result = 'win') / count() as win_rate,
            uniq(user_id) as unique_users,
            
            -- 추가 밸런스 지표
            quantile(0.95)(toFloat64(damage)) as p95_damage,
            stddevPop(toFloat64(damage)) as damage_variance,
            
            -- 시간대별 사용 패턴
            countIf(toHour(timestamp) BETWEEN 20 AND 23) / count() as evening_usage_ratio
        FROM weapon_usage
        GROUP BY weapon_id
        HAVING usage_count >= 100  -- 통계적 유의성 확보
        ORDER BY usage_count DESC
        """
        
        weapon_data = self.client.query_df(weapon_stats_query)
        
        # 🎯 밸런스 문제 감지
        balance_issues = []
        
        # 1. 승률 불균형 (50% ± 10% 벗어나는 무기들)
        unbalanced_weapons = weapon_data[
            (weapon_data['win_rate'] < 0.4) | (weapon_data['win_rate'] > 0.6)
        ]
        
        for _, weapon in unbalanced_weapons.iterrows():
            severity = abs(weapon['win_rate'] - 0.5) * 2  # 0~1 스케일
            balance_issues.append({
                'type': 'win_rate_imbalance',
                'weapon_id': weapon['weapon_id'],
                'current_value': weapon['win_rate'],
                'target_value': 0.5,
                'severity': severity,
                'recommendation': 'damage_adjustment'
            })
        
        # 2. 사용률 불균형 (특정 무기만 과도하게 사용)
        total_usage = weapon_data['usage_count'].sum()
        expected_usage_rate = 1.0 / len(weapon_data)  # 균등 분배 기준
        
        for _, weapon in weapon_data.iterrows():
            actual_usage_rate = weapon['usage_count'] / total_usage
            if actual_usage_rate > expected_usage_rate * 2:  # 2배 이상 사용
                balance_issues.append({
                    'type': 'overused_weapon',
                    'weapon_id': weapon['weapon_id'],
                    'current_value': actual_usage_rate,
                    'target_value': expected_usage_rate,
                    'severity': actual_usage_rate / expected_usage_rate - 1,
                    'recommendation': 'nerf_damage_or_cooldown'
                })
        
        return weapon_data, balance_issues
    
    def optimize_skill_cooldowns(self):
        """스킬 쿨다운 최적화"""
        
        skill_usage_query = """
        WITH skill_sequences AS (
            SELECT 
                user_id,
                JSONExtract(properties, 'skill_id') as skill_id,
                timestamp,
                lag(timestamp) OVER (PARTITION BY user_id ORDER BY timestamp) as prev_usage,
                dateDiff('second', prev_usage, timestamp) as cooldown_gap
            FROM game_events 
            WHERE event_type = 3  -- SKILL_USE
              AND timestamp >= now() - INTERVAL 3 DAY
        )
        SELECT 
            skill_id,
            count() as total_uses,
            avg(cooldown_gap) as avg_gap_seconds,
            quantile(0.5)(cooldown_gap) as median_gap,
            quantile(0.1)(cooldown_gap) as p10_gap,  -- 빠른 연속 사용
            
            -- 전투 상황별 분석
            countIf(cooldown_gap <= 1) as instant_reuses,
            countIf(cooldown_gap BETWEEN 2 AND 5) as quick_reuses,
            
            -- 사용자 행동 패턴
            uniq(user_id) as unique_users,
            total_uses / unique_users as uses_per_user
        FROM skill_sequences
        WHERE cooldown_gap IS NOT NULL
        GROUP BY skill_id
        HAVING total_uses >= 1000
        ORDER BY instant_reuses DESC
        """
        
        skill_data = self.client.query_df(skill_usage_query)
        
        # 🎮 쿨다운 최적화 알고리즘
        optimized_cooldowns = {}
        
        for _, skill in skill_data.iterrows():
            skill_id = skill['skill_id']
            current_usage_pattern = {
                'instant_reuses': skill['instant_reuses'],
                'avg_gap': skill['avg_gap_seconds'],
                'uses_per_user': skill['uses_per_user']
            }
            
            # 목표: 즉시 재사용 < 5%, 평균 간격 > 10초
            if skill['instant_reuses'] > skill['total_uses'] * 0.05:
                # 너무 자주 사용됨 - 쿨다운 증가 필요
                current_cooldown = skill['p10_gap']  # 현재 실제 최소 쿨다운 추정
                recommended_cooldown = current_cooldown * 1.5
                
                optimized_cooldowns[skill_id] = {
                    'current_estimated': current_cooldown,
                    'recommended': recommended_cooldown,
                    'reason': 'reduce_spam_usage',
                    'impact_estimate': 'reduce_usage_by_30%'
                }
            
            elif skill['avg_gap'] > 30 and skill['uses_per_user'] < 5:
                # 너무 적게 사용됨 - 쿨다운 감소 고려
                current_cooldown = skill['median_gap']
                recommended_cooldown = max(5, current_cooldown * 0.8)
                
                optimized_cooldowns[skill_id] = {
                    'current_estimated': current_cooldown,
                    'recommended': recommended_cooldown,
                    'reason': 'increase_viability',
                    'impact_estimate': 'increase_usage_by_20%'
                }
        
        return skill_data, optimized_cooldowns
    
    def detect_economy_inflation(self):
        """게임 내 경제 인플레이션 감지"""
        
        economy_query = """
        WITH daily_economy AS (
            SELECT 
                toDate(timestamp) as date,
                sum(if(event_type = 4, toFloat64(JSONExtract(properties, 'price')), 0)) as daily_spent,
                count(DISTINCT if(event_type = 4, user_id, NULL)) as paying_users,
                
                -- 골드 생성 vs 소모
                sum(if(JSONExtract(properties, 'gold_change') > 0, 
                       toFloat64(JSONExtract(properties, 'gold_change')), 0)) as gold_generated,
                sum(if(JSONExtract(properties, 'gold_change') < 0, 
                       abs(toFloat64(JSONExtract(properties, 'gold_change'))), 0)) as gold_spent,
                
                -- 아이템 가격 트렌드
                avg(if(JSONHas(properties, 'item_market_price'), 
                       toFloat64(JSONExtract(properties, 'item_market_price')), NULL)) as avg_market_price
            FROM game_events 
            WHERE timestamp >= now() - INTERVAL 30 DAY
            GROUP BY date
            ORDER BY date
        )
        SELECT 
            *,
            gold_generated / gold_spent as gold_generation_ratio,
            daily_spent / paying_users as arpu_daily,
            
            -- 트렌드 계산 (7일 이동평균)
            avg(avg_market_price) OVER (ORDER BY date ROWS 6 PRECEDING) as price_trend_7d,
            avg(gold_generation_ratio) OVER (ORDER BY date ROWS 6 PRECEDING) as gold_ratio_trend_7d
        FROM daily_economy
        """
        
        economy_data = self.client.query_df(economy_query)
        
        # 📈 인플레이션 지표 계산
        inflation_indicators = {}
        
        if len(economy_data) >= 14:  # 최소 2주 데이터
            # 1. 시장 가격 상승률
            recent_prices = economy_data.tail(7)['avg_market_price'].mean()
            older_prices = economy_data.head(7)['avg_market_price'].mean()
            price_inflation = (recent_prices - older_prices) / older_prices if older_prices > 0 else 0
            
            # 2. 골드 생성/소모 비율 변화
            recent_gold_ratio = economy_data.tail(7)['gold_generation_ratio'].mean()
            older_gold_ratio = economy_data.head(7)['gold_generation_ratio'].mean()
            
            inflation_indicators = {
                'price_inflation_rate': price_inflation,
                'current_gold_ratio': recent_gold_ratio,
                'gold_ratio_change': recent_gold_ratio - older_gold_ratio,
                'risk_level': self._assess_inflation_risk(price_inflation, recent_gold_ratio),
                'recommendations': self._generate_economy_recommendations(price_inflation, recent_gold_ratio)
            }
        
        return economy_data, inflation_indicators
    
    def _assess_inflation_risk(self, price_inflation, gold_ratio):
        """인플레이션 위험도 평가"""
        if price_inflation > 0.1 or gold_ratio > 1.5:  # 10% 가격 상승 또는 골드 과다 생성
            return 'high'
        elif price_inflation > 0.05 or gold_ratio > 1.2:
            return 'medium'
        else:
            return 'low'
    
    def _generate_economy_recommendations(self, price_inflation, gold_ratio):
        """경제 정책 추천"""
        recommendations = []
        
        if price_inflation > 0.1:
            recommendations.append({
                'action': 'reduce_gold_rewards',
                'target': 'quest_rewards',
                'adjustment': '-20%',
                'reason': 'combat_price_inflation'
            })
        
        if gold_ratio > 1.5:
            recommendations.append({
                'action': 'add_gold_sinks',
                'target': 'repair_costs_and_taxes',
                'adjustment': '+30%',
                'reason': 'reduce_gold_surplus'
            })
        
        if not recommendations:
            recommendations.append({
                'action': 'monitor_only',
                'reason': 'economy_stable'
            })
        
        return recommendations
    
    def generate_balance_report(self):
        """종합 밸런스 리포트 생성"""
        
        print("🎮 게임 밸런스 분석 리포트")
        print("=" * 50)
        
        # 무기 밸런스 분석
        weapon_data, weapon_issues = self.analyze_weapon_balance()
        print(f"\n⚔️  무기 분석: {len(weapon_data)}개 무기, {len(weapon_issues)}개 이슈 발견")
        
        for issue in weapon_issues[:3]:  # 상위 3개 이슈만 출력
            print(f"  - {issue['weapon_id']}: {issue['type']} (심각도: {issue['severity']:.2f})")
        
        # 스킬 쿨다운 최적화
        skill_data, cooldown_opts = self.optimize_skill_cooldowns()
        print(f"\n🎯 스킬 분석: {len(skill_data)}개 스킬, {len(cooldown_opts)}개 조정 제안")
        
        for skill_id, opt in list(cooldown_opts.items())[:3]:
            print(f"  - {skill_id}: {opt['current_estimated']:.1f}초 → {opt['recommended']:.1f}초 ({opt['reason']})")
        
        # 경제 시스템 분석
        economy_data, inflation_info = self.detect_economy_inflation()
        if inflation_info:
            print(f"\n💰 경제 분석: 인플레이션 위험도 {inflation_info['risk_level']}")
            print(f"  - 가격 상승률: {inflation_info['price_inflation_rate']:.1%}")
            print(f"  - 골드 생성비: {inflation_info['current_gold_ratio']:.2f}")
        
        # 종합 추천사항
        print(f"\n📋 추천사항:")
        print(f"  1. 무기 밸런스: {len([i for i in weapon_issues if i['severity'] > 0.3])}개 긴급 조치 필요")
        print(f"  2. 스킬 조정: {len(cooldown_opts)}개 스킬 쿨다운 재조정")
        print(f"  3. 경제 정책: {len(inflation_info.get('recommendations', []))}개 정책 제안")
        
        return {
            'weapon_balance': weapon_data,
            'weapon_issues': weapon_issues,
            'skill_optimization': cooldown_opts,
            'economy_status': inflation_info
        }

# 🚀 실행 예제
def run_balance_analysis():
    client = clickhouse_connect.get_client(
        host='localhost', port=8123, database='analytics'
    )
    
    optimizer = GameBalanceOptimizer(client)
    balance_report = optimizer.generate_balance_report()
    
    return balance_report

if __name__ == "__main__":
    run_balance_analysis()
```

---

## 📊 5. 실시간 대시보드 구축

### **5.1 React 기반 실시간 대시보드**

```typescript
// 🚀 실시간 게임 분석 대시보드
import React, { useState, useEffect, useRef } from 'react';
import {
  Chart as ChartJS,
  CategoryScale,
  LinearScale,
  PointElement,
  LineElement,
  BarElement,
  Title,
  Tooltip,
  Legend,
  TimeScale,
  ArcElement
} from 'chart.js';
import { Line, Bar, Doughnut } from 'react-chartjs-2';
import 'chartjs-adapter-date-fns';

ChartJS.register(
  CategoryScale, LinearScale, PointElement, LineElement, BarElement,
  Title, Tooltip, Legend, TimeScale, ArcElement
);

// 📡 실시간 데이터 타입 정의
interface RealTimeMetrics {
  timestamp: string;
  concurrent_users: number;
  events_per_second: number;
  revenue_per_minute: number;
  server_response_time: number;
  error_rate: number;
}

interface ChurnPrediction {
  user_id: number;
  churn_probability: number;
  risk_level: 'low' | 'medium' | 'high';
  predicted_at: string;
}

interface ServerHealth {
  server_id: string;
  cpu_usage: number;
  memory_usage: number;
  active_connections: number;
  status: 'healthy' | 'warning' | 'critical';
}

// 🎮 메인 대시보드 컴포넌트
const GameAnalyticsDashboard: React.FC = () => {
  const [metrics, setMetrics] = useState<RealTimeMetrics[]>([]);
  const [churnData, setChurnData] = useState<ChurnPrediction[]>([]);
  const [serverHealth, setServerHealth] = useState<ServerHealth[]>([]);
  const [isConnected, setIsConnected] = useState(false);
  const wsRef = useRef<WebSocket | null>(null);

  // 🔌 WebSocket 연결 설정
  useEffect(() => {
    connectWebSocket();
    return () => {
      if (wsRef.current) {
        wsRef.current.close();
      }
    };
  }, []);

  const connectWebSocket = () => {
    try {
      wsRef.current = new WebSocket('ws://localhost:8080/analytics');
      
      wsRef.current.onopen = () => {
        setIsConnected(true);
        console.log('📡 실시간 데이터 연결 성공');
      };

      wsRef.current.onmessage = (event) => {
        const data = JSON.parse(event.data);
        handleRealTimeData(data);
      };

      wsRef.current.onclose = () => {
        setIsConnected(false);
        console.log('🔌 연결 끊김, 5초 후 재연결 시도');
        setTimeout(connectWebSocket, 5000);
      };

      wsRef.current.onerror = (error) => {
        console.error('WebSocket 오류:', error);
      };
    } catch (error) {
      console.error('WebSocket 연결 실패:', error);
    }
  };

  // 📊 실시간 데이터 처리
  const handleRealTimeData = (data: any) => {
    switch (data.type) {
      case 'metrics':
        setMetrics(prev => {
          const newMetrics = [...prev, data.payload];
          // 최근 100개 데이터포인트만 유지
          return newMetrics.slice(-100);
        });
        break;

      case 'churn_predictions':
        setChurnData(data.payload);
        break;

      case 'server_health':
        setServerHealth(data.payload);
        break;

      default:
        console.log('알 수 없는 데이터 타입:', data.type);
    }
  };

  // 📈 차트 데이터 준비
  const prepareConcurrentUsersChart = () => {
    const last24Hours = metrics.slice(-144); // 10분 간격으로 24시간

    return {
      labels: last24Hours.map(m => new Date(m.timestamp)),
      datasets: [
        {
          label: '동접자 수',
          data: last24Hours.map(m => m.concurrent_users),
          borderColor: 'rgb(75, 192, 192)',
          backgroundColor: 'rgba(75, 192, 192, 0.2)',
          tension: 0.4,
          fill: true
        }
      ]
    };
  };

  const prepareRevenueChart = () => {
    const hourlyRevenue = metrics.reduce((acc, metric) => {
      const hour = new Date(metric.timestamp).getHours();
      if (!acc[hour]) acc[hour] = 0;
      acc[hour] += metric.revenue_per_minute;
      return acc;
    }, {} as Record<number, number>);

    return {
      labels: Array.from({length: 24}, (_, i) => `${i}:00`),
      datasets: [
        {
          label: '시간당 매출',
          data: Array.from({length: 24}, (_, i) => hourlyRevenue[i] || 0),
          backgroundColor: 'rgba(54, 162, 235, 0.6)',
          borderColor: 'rgba(54, 162, 235, 1)',
          borderWidth: 2
        }
      ]
    };
  };

  const prepareChurnRiskChart = () => {
    const riskCounts = churnData.reduce((acc, user) => {
      acc[user.risk_level] = (acc[user.risk_level] || 0) + 1;
      return acc;
    }, {} as Record<string, number>);

    return {
      labels: ['저위험', '중위험', '고위험'],
      datasets: [
        {
          data: [riskCounts.low || 0, riskCounts.medium || 0, riskCounts.high || 0],
          backgroundColor: ['#4CAF50', '#FF9800', '#F44336'],
          borderWidth: 2
        }
      ]
    };
  };

  // 🎨 차트 옵션
  const chartOptions = {
    responsive: true,
    maintainAspectRatio: false,
    plugins: {
      legend: {
        position: 'top' as const,
      },
      title: {
        display: true,
        text: '실시간 게임 메트릭'
      }
    },
    scales: {
      x: {
        type: 'time' as const,
        time: {
          displayFormats: {
            minute: 'HH:mm',
            hour: 'HH:mm'
          }
        }
      },
      y: {
        beginAtZero: true
      }
    }
  };

  // 🚨 알림 상태 계산
  const getAlertStatus = () => {
    const latestMetrics = metrics[metrics.length - 1];
    if (!latestMetrics) return 'unknown';

    const alerts = [];
    
    if (latestMetrics.concurrent_users < 1000) alerts.push('low_users');
    if (latestMetrics.server_response_time > 200) alerts.push('high_latency');
    if (latestMetrics.error_rate > 0.05) alerts.push('high_errors');

    const highRiskUsers = churnData.filter(u => u.risk_level === 'high').length;
    if (highRiskUsers > 50) alerts.push('high_churn_risk');

    return alerts.length === 0 ? 'healthy' : alerts.length < 2 ? 'warning' : 'critical';
  };

  return (
    <div className="min-h-screen bg-gray-100 p-6">
      {/* 📱 헤더 */}
      <div className="mb-8">
        <div className="flex items-center justify-between">
          <h1 className="text-3xl font-bold text-gray-900">🎮 게임 분석 대시보드</h1>
          <div className="flex items-center space-x-4">
            <div className={`flex items-center space-x-2 px-3 py-1 rounded-full text-sm ${
              isConnected ? 'bg-green-100 text-green-800' : 'bg-red-100 text-red-800'
            }`}>
              <div className={`w-2 h-2 rounded-full ${
                isConnected ? 'bg-green-500' : 'bg-red-500'
              }`} />
              <span>{isConnected ? '실시간 연결' : '연결 끊김'}</span>
            </div>
            <div className={`px-3 py-1 rounded-full text-sm ${
              getAlertStatus() === 'healthy' ? 'bg-green-100 text-green-800' :
              getAlertStatus() === 'warning' ? 'bg-yellow-100 text-yellow-800' :
              'bg-red-100 text-red-800'
            }`}>
              시스템 상태: {getAlertStatus() === 'healthy' ? '정상' : 
                           getAlertStatus() === 'warning' ? '주의' : '위험'}
            </div>
          </div>
        </div>
      </div>

      {/* 📊 KPI 카드들 */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6 mb-8">
        <KPICard
          title="현재 동접자"
          value={metrics[metrics.length - 1]?.concurrent_users || 0}
          change="+12.3%"
          trend="up"
          icon="👥"
        />
        <KPICard
          title="분당 이벤트"
          value={metrics[metrics.length - 1]?.events_per_second * 60 || 0}
          change="+5.7%"
          trend="up"
          icon="⚡"
        />
        <KPICard
          title="분당 매출"
          value={`$${(metrics[metrics.length - 1]?.revenue_per_minute || 0).toFixed(2)}`}
          change="-2.1%"
          trend="down"
          icon="💰"
        />
        <KPICard
          title="고위험 사용자"
          value={churnData.filter(u => u.risk_level === 'high').length}
          change="+8.4%"
          trend="up"
          icon="🚨"
        />
      </div>

      {/* 📈 메인 차트들 */}
      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6 mb-8">
        <ChartCard title="실시간 동접자 추이">
          <Line data={prepareConcurrentUsersChart()} options={chartOptions} />
        </ChartCard>

        <ChartCard title="시간대별 매출">
          <Bar data={prepareRevenueChart()} options={{
            ...chartOptions,
            scales: { y: { beginAtZero: true } }
          }} />
        </ChartCard>

        <ChartCard title="이탈 위험도 분포">
          <Doughnut data={prepareChurnRiskChart()} options={{
            responsive: true,
            maintainAspectRatio: false,
            plugins: {
              legend: { position: 'bottom' }
            }
          }} />
        </ChartCard>

        <ChartCard title="서버 상태">
          <ServerHealthDisplay servers={serverHealth} />
        </ChartCard>
      </div>

      {/* 📋 상세 데이터 테이블 */}
      <div className="bg-white rounded-lg shadow p-6">
        <h3 className="text-lg font-semibold mb-4">🎯 고위험 이탈 사용자 목록</h3>
        <ChurnRiskTable data={churnData.filter(u => u.risk_level === 'high').slice(0, 10)} />
      </div>
    </div>
  );
};

// 🎯 KPI 카드 컴포넌트
interface KPICardProps {
  title: string;
  value: number | string;
  change: string;
  trend: 'up' | 'down';
  icon: string;
}

const KPICard: React.FC<KPICardProps> = ({ title, value, change, trend, icon }) => (
  <div className="bg-white rounded-lg shadow p-6">
    <div className="flex items-center justify-between">
      <div>
        <p className="text-sm text-gray-600">{title}</p>
        <p className="text-2xl font-bold text-gray-900">{value}</p>
        <p className={`text-sm ${trend === 'up' ? 'text-green-600' : 'text-red-600'}`}>
          {change} from last hour
        </p>
      </div>
      <div className="text-3xl">{icon}</div>
    </div>
  </div>
);

// 📊 차트 카드 컴포넌트
interface ChartCardProps {
  title: string;
  children: React.ReactNode;
}

const ChartCard: React.FC<ChartCardProps> = ({ title, children }) => (
  <div className="bg-white rounded-lg shadow p-6">
    <h3 className="text-lg font-semibold mb-4">{title}</h3>
    <div className="h-64">{children}</div>
  </div>
);

// 🖥️ 서버 상태 표시 컴포넌트
const ServerHealthDisplay: React.FC<{ servers: ServerHealth[] }> = ({ servers }) => (
  <div className="space-y-4">
    {servers.map(server => (
      <div key={server.server_id} className="flex items-center justify-between p-3 bg-gray-50 rounded">
        <div>
          <p className="font-medium">{server.server_id}</p>
          <p className="text-sm text-gray-600">
            CPU: {server.cpu_usage}% | Memory: {server.memory_usage}%
          </p>
        </div>
        <div className={`px-2 py-1 rounded text-xs ${
          server.status === 'healthy' ? 'bg-green-100 text-green-800' :
          server.status === 'warning' ? 'bg-yellow-100 text-yellow-800' :
          'bg-red-100 text-red-800'
        }`}>
          {server.status}
        </div>
      </div>
    ))}
  </div>
);

// 📋 이탈 위험 테이블 컴포넌트
const ChurnRiskTable: React.FC<{ data: ChurnPrediction[] }> = ({ data }) => (
  <div className="overflow-x-auto">
    <table className="min-w-full divide-y divide-gray-200">
      <thead className="bg-gray-50">
        <tr>
          <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">사용자 ID</th>
          <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">이탈 확률</th>
          <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">위험도</th>
          <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">예측 시간</th>
          <th className="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">액션</th>
        </tr>
      </thead>
      <tbody className="bg-white divide-y divide-gray-200">
        {data.map(user => (
          <tr key={user.user_id}>
            <td className="px-6 py-4 whitespace-nowrap text-sm font-medium text-gray-900">
              {user.user_id}
            </td>
            <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-500">
              {(user.churn_probability * 100).toFixed(1)}%
            </td>
            <td className="px-6 py-4 whitespace-nowrap">
              <span className={`px-2 py-1 text-xs rounded-full ${
                user.risk_level === 'high' ? 'bg-red-100 text-red-800' :
                user.risk_level === 'medium' ? 'bg-yellow-100 text-yellow-800' :
                'bg-green-100 text-green-800'
              }`}>
                {user.risk_level}
              </span>
            </td>
            <td className="px-6 py-4 whitespace-nowrap text-sm text-gray-500">
              {new Date(user.predicted_at).toLocaleString()}
            </td>
            <td className="px-6 py-4 whitespace-nowrap text-sm text-blue-600">
              <button className="hover:text-blue-800">리텐션 캠페인 발송</button>
            </td>
          </tr>
        ))}
      </tbody>
    </table>
  </div>
);

export default GameAnalyticsDashboard;
```

---

## 🚀 6. 실전 프로젝트: 종합 분석 시스템

### **6.1 프로젝트 목표**
- **실시간 게임 메트릭 수집**: 초당 10,000+ 이벤트 처리
- **사용자 이탈 예측**: 85% 이상 정확도
- **게임 밸런스 최적화**: 자동 추천 시스템
- **매출 최적화**: A/B 테스트 자동화

### **6.2 시스템 아키텍처**
```
Game Clients → Kafka → Spark Streaming → ClickHouse
                ↓             ↓              ↓
            ML Pipeline → Model Serving → Dashboard
```

### **6.3 기대 성과**
- **운영 효율성**: 모니터링 시간 80% 단축
- **사용자 유지율**: 이탈률 30% 감소
- **매출 증가**: 데이터 기반 최적화로 20% 매출 향상
- **게임 품질**: 밸런스 이슈 조기 발견으로 플레이어 만족도 향상

---

## 🎯 마무리

**🎉 축하합니다!** 이제 여러분은 **게임 업계 최고 수준의 실시간 분석 시스템**을 구축할 수 있습니다!

### **습득한 핵심 역량:**
- ✅ **대규모 데이터 처리**: Apache Spark로 초당 만개 이벤트 처리
- ✅ **실시간 분석**: ClickHouse로 밀리초 단위 응답
- ✅ **머신러닝 파이프라인**: 이탈 예측, 밸런스 최적화 자동화
- ✅ **실시간 대시보드**: React로 임원진용 인사이트 제공
- ✅ **비즈니스 임팩트**: 데이터로 매출과 사용자 만족도 동시 향상

### **실제 산업 활용도:**
- **게임 회사**: 넥슨, 엔씨소프트, 크래프톤의 데이터 팀 수준
- **테크 기업**: Netflix, Uber의 실시간 분석 시스템과 동급
- **스타트업**: 데이터 기반 의사결정으로 빠른 성장 실현

### **다음 성장 방향:**
1. **A/B 테스트 자동화** 시스템 구축
2. **개인화 추천 엔진** 개발
3. **사기 탐지 시스템** 고도화
4. **예측 분석** 모델 다양화

**💼 이 수준의 역량이면 게임 업계 데이터 사이언티스트/분석가로 충분히 경쟁력이 있습니다!**

---

## 🔥 흔한 실수와 해결방법 (Common Mistakes & Solutions)

### 1. 이벤트 스키마 관리 실패
```cpp
// [SEQUENCE: 1] ❌ 잘못된 예: 유연하지 못한 이벤트 구조
struct BadEventSchema {
    int user_id;
    int event_type;  // 하드코딩된 타입
    float value1;
    float value2;
    // 새 필드 추가 불가능!
};

// ✅ 올바른 예: 확장 가능한 JSON 스키마
struct GoodEventSchema {
    std::string event_id;
    std::string event_type;
    std::string user_id;
    uint21_t timestamp;
    nlohmann::json properties;  // 유연한 속성
    
    // 스키마 버전 관리
    std::string schema_version = "2.0";
    
    // 검증 함수
    bool Validate() const {
        return !event_id.empty() && 
               !event_type.empty() && 
               timestamp > 0;
    }
};
```

### 2. 데이터 처리 병목
```cpp
// [SEQUENCE: 2] ❌ 잘못된 예: 동기적 처리로 병목 발생
class BadDataProcessor {
    void ProcessEvent(const GameEvent& event) {
        // 매 이벤트마다 DB 쓰기 - 초당 100개만 처리 가능
        database.Insert(event);
        
        // 동기적 집계 - 느림!
        UpdateStatisticsSync(event);
        
        // 블로킹 ML 추론
        auto prediction = ml_model.Predict(event);
    }
};

// ✅ 올바른 예: 비동기 배치 처리
class GoodDataProcessor {
    ThreadSafeQueue<GameEvent> event_buffer;
    
    void ProcessEvent(const GameEvent& event) {
        // 버퍼에 추가 (논블로킹)
        event_buffer.Push(event);
    }
    
    void BatchProcessor() {
        while (running) {
            auto batch = event_buffer.PopBatch(1000, 100ms);
            
            // 배치 DB 삽입
            database.BulkInsert(batch);
            
            // 병렬 집계
            std::async(std::launch::async, [batch] {
                UpdateStatisticsAsync(batch);
            });
            
            // 비동기 ML 추론
            ml_pipeline.ProcessBatch(batch);
        }
    }
};
```

### 3. 잘못된 데이터 보존 정책
```cpp
// [SEQUENCE: 3] ❌ 잘못된 예: 모든 데이터 무제한 보관
class BadDataRetention {
    void StoreEvent(const GameEvent& event) {
        // 모든 이벤트를 영구 보관 - 스토리지 폭발!
        raw_storage.Store(event);
        
        // 집계 데이터도 전부 보관
        aggregated_storage.Store(AggregateAll(event));
    }
};

// ✅ 올바른 예: 계층적 데이터 보존
class GoodDataRetention {
    void ApplyRetentionPolicy() {
        // Hot: 7일 (실시간 분석)
        if (age > 7_days) {
            MoveToWarm(event);
        }
        
        // Warm: 30일 (집계된 데이터)
        if (age > 30_days) {
            auto aggregated = Aggregate(event);
            MoveToCold(aggregated);
            Delete(raw_event);
        }
        
        // Cold: 1년 (압축된 요약)
        if (age > 1_year) {
            auto summary = Summarize(event);
            ArchiveToS3(summary);
            Delete(aggregated);
        }
    }
};
```

---

## 🚀 실습 프로젝트 (Practice Projects)

### 📚 기초 프로젝트: 실시간 게임 메트릭 대시보드
**목표**: 기본적인 게임 메트릭 수집 및 시각화

```cpp
// 구현 요구사항:
// 1. 게임 이벤트 수집 API
// 2. Kafka로 이벤트 스트리밍
// 3. 실시간 DAU/MAU 계산
// 4. Grafana 대시보드
// 5. 알림 시스템
// 6. 초당 1,000 이벤트 처리
```

### 🎮 중급 프로젝트: 유저 이탈 예측 시스템
**목표**: ML 기반 이탈 예측 파이프라인

```python
# 핵심 기능:
# 1. 특성 엔지니어링 파이프라인
# 2. 실시간 모델 학습
# 3. A/B 테스트 프레임워크
# 4. 예측 결과 API
# 5. 리텐션 캠페인 자동화
# 6. 85% 이상 정확도
```

### 🏆 고급 프로젝트: 종합 게임 분석 플랫폼
**목표**: 엔터프라이즈급 분석 시스템

```yaml
구현 내용:
  - 멀티 게임 지원
  - 실시간 이상 탐지
  - 자동 밸런스 분석
  - 매출 최적화 엔진
  - 커스텀 쿼리 엔진
  - PB급 데이터 처리
  - 멀티 리전 지원
```

---

## 📊 학습 체크리스트 (Learning Checklist)

### 🥉 브론즈 레벨
- [ ] Kafka 기본 사용법
- [ ] 간단한 스트림 처리
- [ ] 기본 SQL 집계
- [ ] Grafana 대시보드

### 🥈 실버 레벨
- [ ] Spark Streaming
- [ ] ClickHouse 쿼리
- [ ] 파이썬 데이터 분석
- [ ] 기초 ML 모델

### 🥇 골드 레벨
- [ ] 복잡한 ETL 파이프라인
- [ ] 실시간 ML 서빙
- [ ] 분산 처리 최적화
- [ ] 커스텀 분석 도구

### 💎 플래티넘 레벨
- [ ] 대규모 시스템 설계
- [ ] 고급 ML 파이프라인
- [ ] 실시간 의사결정 시스템
- [ ] 비즈니스 임팩트 측정

---

## 📚 추가 학습 자료 (Additional Resources)

### 필독서
- 📖 "Streaming Systems" - Tyler Akidau
- 📖 "Designing Data-Intensive Applications" - Martin Kleppmann
- 📖 "The Data Warehouse Toolkit" - Ralph Kimball

### 온라인 강의
- 🎓 Coursera: Big Data Specialization
- 🎓 Udacity: Data Streaming Nanodegree
- 🎓 Fast.ai: Practical Deep Learning

### 오픈소스 프로젝트
- 🔧 Apache Kafka: 분산 스트리밍 플랫폼
- 🔧 Apache Spark: 대규모 데이터 처리
- 🔧 ClickHouse: 실시간 분석 DB
- 🔧 Metabase: 오픈소스 BI 도구

### 실무 도구
- 📊 Tableau: 비즈니스 인텔리전스
- 📊 Looker: 데이터 플랫폼
- 📊 Amplitude: 제품 분석
- 📊 Mixpanel: 사용자 행동 분석

**계속해서 다른 고급 문서들도 학습해보세요! 🚀**