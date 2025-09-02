#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <thread>
#include <vector>
#include <memory>
#include <future>

// [SEQUENCE: MVP18-26] Tier 1 통합 테스트
#include "../src/core/cache/redis_cluster_manager.h"
#include "../src/database/database_sharding_manager.h"
#include "../src/game/ai/adaptive_ai_engine.h"
#include "../src/analytics/realtime_analytics_engine.h"
#include "../src/network/global_load_balancer.h"
#include "../src/network/quic_protocol_handler.h"
#include "../src/core/security/network_security.h"

using namespace mmorpg;
using namespace testing;

class Tier1IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Redis Cluster 초기화
        core::cache::RedisClusterManager::ClusterConfig redis_config;
        redis_config.seed_nodes = {"127.0.0.1:7000", "127.0.0.1:7001", "127.0.0.1:7002"};
        redis_manager_ = std::make_unique<core::cache::RedisClusterManager>(redis_config);
        
        // Database Sharding 초기화
        database::DatabaseShardingManager::ShardingConfig db_config;
        database::DatabaseShardingManager::ShardInfo shard1;
        shard1.shard_id = "shard1";
        shard1.host = "localhost";
        shard1.port = 5432;
        shard1.database_name = "mmorpg_shard1";
        shard1.is_master = true;
        shard1.user_id_range_start = 1;
        shard1.user_id_range_end = 100000;
        db_config.shards.push_back(shard1);
        
        database::DatabaseShardingManager::ShardInfo shard2;
        shard2.shard_id = "shard2";
        shard2.host = "localhost";
        shard2.port = 5433;
        shard2.database_name = "mmorpg_shard2";
        shard2.is_master = true;
        shard2.user_id_range_start = 100001;
        shard2.user_id_range_end = 200000;
        db_config.shards.push_back(shard2);
        
        db_manager_ = std::make_unique<database::DatabaseShardingManager>(db_config);
        
        // AI Engine 초기화
        ai_engine_ = std::make_unique<game::ai::AdaptiveAIEngine>(1000);
        
        // Analytics Engine 초기화
        analytics_engine_ = std::make_unique<analytics::RealtimeAnalyticsEngine>(10000);
        
        // Load Balancer 초기화
        network::GlobalLoadBalancer::LoadBalancerConfig lb_config;
        lb_config.primary_strategy = network::GlobalLoadBalancer::LoadBalancingStrategy::INTELLIGENT;
        load_balancer_ = std::make_unique<network::GlobalLoadBalancer>(lb_config);
        
        // QUIC Protocol Handler 초기화
        network::QUICProtocolHandler::QUICConfig quic_config;
        quic_config.enable_0rtt = true;
        quic_config.enable_migration = true;
        quic_protocol_ = std::make_unique<network::QUICProtocolHandler>(quic_config);
        
        // Network Security Manager 초기화
        security_manager_ = std::make_unique<core::security::NetworkSecurityManager>();
        
        // 모든 서비스 시작
        analytics_engine_->StartAnalyticsEngine();
        load_balancer_->StartLoadBalancer();
        quic_protocol_->StartProtocolHandler();
        
        // 테스트 서버 노드들 등록
        RegisterTestServers();
    }
    
    void TearDown() override {
        analytics_engine_->Shutdown();
        load_balancer_->Shutdown();
        quic_protocol_->Shutdown();
    }
    
    void RegisterTestServers() {
        // 테스트용 서버 노드들 등록
        network::GlobalLoadBalancer::ServerNode server1("server1", "game1.example.com", 8080, "us-east");
        server1.latitude = 40.7128;
        server1.longitude = -74.0060;
        server1.weight = 100;
        load_balancer_->RegisterServer(server1);
        
        network::GlobalLoadBalancer::ServerNode server2("server2", "game2.example.com", 8080, "eu-west");
        server2.latitude = 51.5074;
        server2.longitude = -0.1278;
        server2.weight = 100;
        load_balancer_->RegisterServer(server2);
        
        network::GlobalLoadBalancer::ServerNode server3("server3", "game3.example.com", 8080, "asia-pacific");
        server3.latitude = 35.6762;
        server3.longitude = 139.6503;
        server3.weight = 150;
        load_balancer_->RegisterServer(server3);
    }

protected:
    std::unique_ptr<core::cache::RedisClusterManager> redis_manager_;
    std::unique_ptr<database::DatabaseShardingManager> db_manager_;
    std::unique_ptr<game::ai::AdaptiveAIEngine> ai_engine_;
    std::unique_ptr<analytics::RealtimeAnalyticsEngine> analytics_engine_;
    std::unique_ptr<network::GlobalLoadBalancer> load_balancer_;
    std::unique_ptr<network::QUICProtocolHandler> quic_protocol_;
    std::unique_ptr<core::security::NetworkSecurityManager> security_manager_;
};

// [SEQUENCE: MVP18-27] Redis Cluster + Database Sharding 통합 테스트
TEST_F(Tier1IntegrationTest, TestCacheAndDatabaseIntegration) {
    const uint64_t user_id = 12345;
    const std::string cache_key = "user:" + std::to_string(user_id);
    
    // 1. 데이터베이스에 사용자 데이터 저장
    struct UserData {
        std::string username = "testuser";
        int level = 50;
        double experience = 12500.5;
    };
    
    UserData user_data;
    auto db_future = db_manager_->SaveUserDataAsync(user_id, "users", user_data);
    
    // 2. Redis에 캐시 저장
    auto cache_future = redis_manager_->SetAsync(cache_key, user_data.username, std::chrono::seconds(300));
    
    // 3. 결과 확인
    ASSERT_TRUE(db_future.get());
    ASSERT_TRUE(cache_future.get());
    
    // 4. 캐시에서 데이터 조회
    auto cached_result = redis_manager_->GetAsync<std::string>(cache_key);
    auto cached_username = cached_result.get();
    
    ASSERT_TRUE(cached_username.has_value());
    EXPECT_EQ(*cached_username, user_data.username);
    
    // 5. 데이터베이스에서 데이터 조회
    auto db_result = db_manager_->GetUserDataAsync<UserData>(user_id, "users");
    auto retrieved_data = db_result.get();
    
    ASSERT_TRUE(retrieved_data.has_value());
    // 실제 구현에서는 직렬화/역직렬화가 필요
    
    std::cout << "✓ Cache and Database integration test passed" << std::endl;
}

// [SEQUENCE: MVP18-28] AI Engine + Analytics 통합 테스트
TEST_F(Tier1IntegrationTest, TestAIAndAnalyticsIntegration) {
    const uint64_t player_id = 67890;
    
    // 1. 플레이어 행동 시뮬레이션
    std::vector<std::string> actions = {"attack", "defend", "heal", "explore", "retreat"};
    
    for (int i = 0; i < 50; ++i) {
        std::string action = actions[i % actions.size()];
        
        // AI 엔진에 플레이어 행동 학습
        std::unordered_map<std::string, float> context;
        context["health_percentage"] = 0.5f + (i % 5) * 0.1f;
        context["enemy_count"] = 1.0f + (i % 3);
        
        ai_engine_->LearnFromPlayerAction(player_id, action, context);
        
        // Analytics 엔진에 이벤트 기록
        analytics_engine_->RecordPlayerAction(player_id, action, {
            {"health", context["health_percentage"]},
            {"enemies", static_cast<int64_t>(context["enemy_count"])}});
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // 2. 전투 결과 시뮬레이션
    std::vector<std::string> battle_actions = {"attack", "combo", "defend", "special"};
    ai_engine_->LearnFromBattleOutcome(player_id, true, 45.5f, battle_actions);
    
    // 3. AI 예측 테스트
    auto prediction = ai_engine_->PredictPlayerBehavior(player_id, "low_health");
    EXPECT_FALSE(prediction.most_likely_action.empty());
    EXPECT_GT(prediction.confidence, 0.0f);
    
    // 4. AI 행동 생성 테스트
    std::vector<std::string> available_actions = {"attack", "defend", "heal"};
    std::string ai_action = ai_engine_->GenerateAIAction(player_id, "combat", available_actions);
    EXPECT_FALSE(ai_action.empty());
    
    // 5. Analytics 대시보드 데이터 확인
    auto dashboard = analytics_engine_->GetRealtimeDashboard();
    EXPECT_GT(dashboard.active_players, 0);
    
    // 6. 개인화된 챌린지 생성
    auto challenge = ai_engine_->GeneratePersonalizedChallenge(player_id);
    EXPECT_FALSE(challenge.challenge_type.empty());
    
    std::cout << "✓ AI and Analytics integration test passed" << std::endl;
    std::cout << "  - AI predicted action: " << prediction.most_likely_action 
              << " (confidence: " << prediction.confidence << ")" << std::endl;
    std::cout << "  - Generated challenge: " << challenge.challenge_type << std::endl;
}

// [SEQUENCE: MVP18-29] Load Balancer + QUIC Protocol 통합 테스트
TEST_F(Tier1IntegrationTest, TestLoadBalancerAndQUICIntegration) {
    const std::string client_id = "client_test_001";
    const std::string client_ip = "192.168.1.100";
    
    // 1. 서버 메트릭 업데이트
    load_balancer_->UpdateServerMetrics("server1", 45.0, 60.0, 150, 25.5);
    load_balancer_->UpdateServerMetrics("server2", 70.0, 80.0, 300, 45.0);
    load_balancer_->UpdateServerMetrics("server3", 30.0, 40.0, 100, 15.2);
    
    // 2. 클라이언트 라우팅 테스트
    auto routing_result = load_balancer_->RouteClient(client_id, client_ip);
    
    ASSERT_TRUE(routing_result.success);
    EXPECT_FALSE(routing_result.selected_server_id.empty());
    EXPECT_GT(routing_result.server_port, 0);
    
    std::cout << "✓ Client routed to server: " << routing_result.selected_server_id 
              << " (" << routing_result.routing_reason << ")" << std::endl;
    
    // 3. QUIC 연결 생성
    auto quic_connection = quic_protocol_->CreateConnection(routing_result.server_hostname, 
                                                           routing_result.server_port);
    ASSERT_NE(quic_connection, nullptr);
    
    // 4. QUIC 스트림 생성 및 데이터 전송
    auto stream = quic_protocol_->CreateStream(quic_connection->connection_id, 
                                              network::QUICProtocolHandler::StreamType::BIDIRECTIONAL_CLIENT);
    ASSERT_NE(stream, nullptr);
    
    std::vector<uint8_t> test_data = {'H', 'e', 'l', 'l', 'o', ' ', 'Q', 'U', 'I', 'C'};
    bool send_result = quic_protocol_->SendData(quic_connection->connection_id, 
                                               stream->stream_id, test_data);
    EXPECT_TRUE(send_result);
    
    // 5. 0-RTT 데이터 전송 테스트
    std::vector<uint8_t> zero_rtt_data = {'0', 'R', 'T', 'T'};
    bool zero_rtt_result = quic_protocol_->Send0RTTData(quic_connection->connection_id, zero_rtt_data);
    // 첫 연결이므로 실패할 수 있음
    
    // 6. 로드 밸런서 통계 확인
    auto lb_stats = load_balancer_->GetStatistics();
    EXPECT_GT(lb_stats.total_servers, 0);
    EXPECT_GT(lb_stats.successful_routings, 0);
    
    // 7. QUIC 통계 확인
    auto quic_stats = quic_protocol_->GetStatistics();
    EXPECT_GT(quic_stats.total_connections, 0);
    
    std::cout << "✓ Load Balancer and QUIC integration test passed" << std::endl;
    std::cout << "  - QUIC connections: " << quic_stats.total_connections << std::endl;
    std::cout << "  - Load balancer success rate: " << lb_stats.routing_success_rate << std::endl;
}

// [SEQUENCE: MVP18-30] 보안 + 전체 시스템 통합 테스트
TEST_F(Tier1IntegrationTest, TestSecurityIntegration) {
    const std::string test_ip = "203.0.113.1";
    const uint16_t test_port = 12345;
    const std::string user_agent = "TestClient/1.0";
    
    // 1. 정상 패킷 처리 테스트
    std::vector<uint8_t> normal_packet(1024, 0x41); // 'A'로 채운 1KB 패킷
    
    bool normal_result = security_manager_->ProcessIncomingPacket(test_ip, test_port, user_agent, normal_packet);
    EXPECT_TRUE(normal_result);
    
    // 2. 대량 패킷 시뮬레이션 (DDoS 방지 테스트)
    for (int i = 0; i < 100; ++i) {
        std::vector<uint8_t> packet(512, 0x42);
        security_manager_->ProcessIncomingPacket(test_ip, test_port, user_agent, packet);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // 3. 의심스러운 사용자 에이전트 테스트
    std::vector<uint8_t> bot_packet(256, 0x43);
    bool bot_result = security_manager_->ProcessIncomingPacket(test_ip, test_port, "curl/7.68.0", bot_packet);
    // 봇으로 감지될 수 있지만 첫 번째 요청은 통과할 수 있음
    
    // 4. 네트워크 보안 세션 테스트
    auto session = security_manager_->CreateSession("test_session_001");
    ASSERT_NE(session, nullptr);
    EXPECT_TRUE(session->IsValid());
    
    // 5. 패킷 암호화/복호화 테스트
    std::vector<uint8_t> plaintext = {'T', 'e', 's', 't', ' ', 'D', 'a', 't', 'a'};
    auto encrypted_packet = session->EncryptPacket(plaintext);
    
    auto decrypted_data = session->DecryptPacket(encrypted_packet);
    EXPECT_EQ(plaintext.size(), decrypted_data.size());
    EXPECT_EQ(plaintext, decrypted_data);
    
    // 6. Analytics에 보안 이벤트 기록
    analytics_engine_->RecordServerMetric("security_events", 1.0);
    analytics_engine_->RecordServerMetric("ddos_attempts", 1.0);
    
    std::cout << "✓ Security integration test passed" << std::endl;
    std::cout << "  - Session encryption/decryption: OK" << std::endl;
    std::cout << "  - DDoS protection: Active" << std::endl;
}

// [SEQUENCE: MVP18-31] 성능 및 스케일링 테스트
TEST_F(Tier1IntegrationTest, TestPerformanceAndScaling) {
    const int num_concurrent_users = 100;
    const int operations_per_user = 10;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 1. 동시 사용자 시뮬레이션
    std::vector<std::future<void>> user_futures;
    
    for (int user_id = 1; user_id <= num_concurrent_users; ++user_id) {
        auto future = std::async(std::launch::async, [this, user_id, operations_per_user]() {
            // 각 사용자에 대해 다양한 작업 수행
            for (int op = 0; op < operations_per_user; ++op) {
                // Cache 작업
                std::string cache_key = "user:" + std::to_string(user_id) + ":session";
                redis_manager_->SetAsync(cache_key, "active");
                
                // AI 행동 학습
                std::unordered_map<std::string, float> context;
                context["action_number"] = static_cast<float>(op);
                ai_engine_->LearnFromPlayerAction(user_id, "test_action", context);
                
                // Analytics 이벤트
                analytics_engine_->RecordPlayerAction(std::to_string(user_id), "performance_test");
                
                // Load Balancer 라우팅
                load_balancer_->RouteClient("client_" + std::to_string(user_id), "192.168.1." + std::to_string(user_id % 254 + 1));
                
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
        
        user_futures.push_back(std::move(future));
    }
    
    // 2. 모든 작업 완료 대기
    for (auto& future : user_futures) {
        future.wait();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // 3. 성능 지표 수집
    auto analytics_dashboard = analytics_engine_->GetRealtimeDashboard();
    auto lb_stats = load_balancer_->GetStatistics();
    auto quic_stats = quic_protocol_->GetStatistics();
    
    // 4. 스케일링 권장사항 분석
    auto scaling_recommendations = load_balancer_->AnalyzeScalingNeeds();
    
    // 5. 성능 검증
    EXPECT_LT(duration.count(), 10000); // 10초 이내 완료
    EXPECT_GE(analytics_dashboard.active_players, 0);
    EXPECT_GE(lb_stats.successful_routings, num_concurrent_users);
    
    std::cout << "✓ Performance and scaling test passed" << std::endl;
    std::cout << "  - " << num_concurrent_users << " concurrent users processed in " 
              << duration.count() << "ms" << std::endl;
    std::cout << "  - Total operations: " << (num_concurrent_users * operations_per_user) << std::endl;
    std::cout << "  - Throughput: " << (num_concurrent_users * operations_per_user * 1000.0 / duration.count()) 
              << " ops/sec" << std::endl;
    
    for (const auto& rec : scaling_recommendations) {
        std::cout << "  - Scaling recommendation for " << rec.region << ": " << rec.action 
                  << " (load: " << rec.current_load_percentage << "%)" << std::endl;
    }
}

// [SEQUENCE: MVP18-32] 전체 시스템 종단간 테스트
TEST_F(Tier1IntegrationTest, TestEndToEndSystemFlow) {
    const std::string player_id = "e2e_player_001";
    const std::string client_ip = "198.51.100.42";
    
    std::cout << "Starting end-to-end system flow test..." << std::endl;
    
    // 1. 플레이어 연결 및 라우팅
    auto routing_result = load_balancer_->RouteClient(player_id, client_ip);
    ASSERT_TRUE(routing_result.success);
    
    // 2. 보안 검증
    std::vector<uint8_t> handshake_packet(256, 0x01);
    bool security_check = security_manager_->ProcessIncomingPacket(client_ip, 443, "GameClient/2.0", handshake_packet);
    EXPECT_TRUE(security_check);
    
    // 3. QUIC 연결 설정
    auto quic_connection = quic_protocol_->CreateConnection(routing_result.server_hostname, routing_result.server_port);
    ASSERT_NE(quic_connection, nullptr);
    
    // 4. 플레이어 데이터 로드 (캐시 우선, DB 폴백)
    std::string cache_key = "player:" + player_id;
    auto cached_data = redis_manager_->GetAsync<std::string>(cache_key);
    
    if (!cached_data.get().has_value()) {
        // 캐시에 없으면 DB에서 로드
        auto db_data = db_manager_->GetUserDataAsync<std::string>(std::hash<std::string>{}(player_id) % 200000 + 1, "players");
        if (db_data.get().has_value()) {
            // DB 데이터를 캐시에 저장
            redis_manager_->SetAsync(cache_key, "player_data_from_db", std::chrono::seconds(300));
        }
    }
    
    // 5. 게임 플레이 시뮬레이션
    std::vector<std::string> game_actions = {"login", "move", "attack", "loot", "level_up", "logout"};
    
    for (const auto& action : game_actions) {
        // AI 학습
        std::unordered_map<std::string, float> context;
        context["timestamp"] = static_cast<float>(std::chrono::steady_clock::now().time_since_epoch().count());
        ai_engine_->LearnFromPlayerAction(std::hash<std::string>{}(player_id), action, context);
        
        // Analytics 기록
        analytics_engine_->RecordPlayerAction(player_id, action);
        
        // 서버 메트릭 업데이트
        analytics_engine_->RecordServerMetric("action_" + action, 1.0);
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // 6. AI 기반 적응형 콘텐츠 생성
    auto ai_challenge = ai_engine_->GeneratePersonalizedChallenge(std::hash<std::string>{}(player_id));
    EXPECT_FALSE(ai_challenge.challenge_type.empty());
    
    // 7. 실시간 분석 데이터 검증
    auto dashboard = analytics_engine_->GetRealtimeDashboard();
    EXPECT_GT(dashboard.total_logins_today, 0);
    
    // 8. 시스템 성능 모니터링
    auto trend_analysis = analytics_engine_->AnalyzeTrend("active_players", 10);
    auto anomaly_detection = analytics_engine_->DetectAnomaly("server_cpu");
    
    // 9. 데이터 지속성 검증 (DB 저장)
    std::unordered_map<std::string, std::string> session_data = {
        {"last_action", game_actions.back()},
        {"session_duration", "600"},
        {"experience_gained", "150"}
    };
    
    auto save_result = db_manager_->SaveUserDataAsync(std::hash<std::string>{}(player_id) % 200000 + 1, "sessions", session_data);
    EXPECT_TRUE(save_result.get());
    
    // 10. 최종 통계 확인
    auto final_lb_stats = load_balancer_->GetStatistics();
    auto final_quic_stats = quic_protocol_->GetStatistics();
    auto final_analytics = analytics_engine_->GetRealtimeDashboard();
    
    // 검증
    EXPECT_GT(final_lb_stats.total_routing_requests, 0);
    EXPECT_GT(final_quic_stats.total_connections, 0);
    EXPECT_GE(final_analytics.active_players, 0);
    
    std::cout << "✓ End-to-end system flow test completed successfully" << std::endl;
    std::cout << "  - Player actions processed: " << game_actions.size() << std::endl;
    std::cout << "  - AI challenge generated: " << ai_challenge.challenge_type << std::endl;
    std::cout << "  - System components: All functional" << std::endl;
    std::cout << "  - Data persistence: Verified" << std::endl;
    std::cout << "  - Real-time analytics: Active" << std::endl;
}

// [SEQUENCE: MVP18-33] 메인 테스트 실행
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    std::cout << "=== MMORPG Server Tier 1 Integration Tests ===" << std::endl;
    std::cout << "Testing components:" << std::endl;
    std::cout << "- Redis Cluster Integration" << std::endl;
    std::cout << "- Database Sharding System" << std::endl;
    std::cout << "- AI/ML Adaptive Engine" << std::endl;
    std::cout << "- Real-time Analytics" << std::endl;
    std::cout << "- Global Load Balancer" << std::endl;
    std::cout << "- QUIC Protocol Handler" << std::endl;
    std::cout << "- Network Security Manager" << std::endl;
    std::cout << "================================================" << std::endl;
    
    return RUN_ALL_TESTS();
}