#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <random>
#include "server/game_server.h"
#include "client/test_client.h"
#include "monitoring/performance_monitor.h"

using namespace mmorpg;
using namespace std::chrono_literals;

// [SEQUENCE: MVP17-50] Load capacity and performance tests
class LoadCapacityTest : public ::testing::Test {
protected:
    std::unique_ptr<GameServer> server;
    std::unique_ptr<PerformanceMonitor> monitor;
    std::thread server_thread;
    
    void SetUp() override {
        // Start server with performance monitoring
        server = std::make_unique<GameServer>(8888);
        monitor = std::make_unique<PerformanceMonitor>();
        
        server->SetPerformanceMonitor(monitor.get());
        
        server_thread = std::thread([this]() {
            server->Start();
        });
        
        // Wait for server startup
        std::this_thread::sleep_for(1s);
    }
    
    void TearDown() override {
        if (server) {
            server->Stop();
        }
        if (server_thread.joinable()) {
            server_thread.join();
        }
    }
    
    std::unique_ptr<TestClient> CreateClient(int id) {
        auto client = std::make_unique<TestClient>(id);
        client->Connect("127.0.0.1", 8888);
        return client;
    }
};

// [SEQUENCE: MVP17-51] Test maximum concurrent connections
TEST_F(LoadCapacityTest, MaxConcurrentConnections) {
    const int TARGET_CONNECTIONS = 5000;
    std::vector<std::unique_ptr<TestClient>> clients;
    std::atomic<int> connected_count{0};
    std::atomic<int> failed_count{0};
    
    // Record start metrics
    auto start_metrics = monitor->GetCurrentMetrics();
    auto start_time = std::chrono::steady_clock::now();
    
    // Connect clients in batches
    const int BATCH_SIZE = 100;
    for (int batch = 0; batch < TARGET_CONNECTIONS / BATCH_SIZE; ++batch) {
        std::vector<std::thread> connect_threads;
        
        for (int i = 0; i < BATCH_SIZE; ++i) {
            int client_id = batch * BATCH_SIZE + i;
            
            connect_threads.emplace_back([this, client_id, &connected_count, &failed_count, &clients]() {
                try {
                    auto client = CreateClient(client_id);
                    if (client->IsConnected()) {
                        connected_count++;
                        
                        std::lock_guard<std::mutex> lock(clients_mutex);
                        clients.push_back(std::move(client));
                    } else {
                        failed_count++;
                    }
                } catch (...) {
                    failed_count++;
                }
            });
        }
        
        // Wait for batch to complete
        for (auto& t : connect_threads) {
            t.join();
        }
        
        // Small delay between batches
        std::this_thread::sleep_for(100ms);
        
        // Check server health
        if (monitor->GetCPUUsage() > 90.0f) {
            std::cout << "CPU limit reached at " << connected_count << " connections\n";
            break;
        }
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
    
    // Verify results
    EXPECT_GE(connected_count, 4000); // Should handle at least 4000
    EXPECT_LE(failed_count, TARGET_CONNECTIONS * 0.05); // Less than 5% failure
    
    // Check performance metrics
    auto end_metrics = monitor->GetCurrentMetrics();
    
    std::cout << "=== Connection Test Results ===\n";
    std::cout << "Connected: " << connected_count << "/" << TARGET_CONNECTIONS << "\n";
    std::cout << "Failed: " << failed_count << "\n";
    std::cout << "Time: " << duration.count() << " seconds\n";
    std::cout << "CPU Usage: " << end_metrics.cpu_usage << "%\n";
    std::cout << "Memory Usage: " << end_metrics.memory_usage_mb << " MB\n";
    std::cout << "===============================\n";
}

// [SEQUENCE: MVP17-52] Test message throughput
TEST_F(LoadCapacityTest, MessageThroughput) {
    const int CLIENT_COUNT = 1000;
    const int MESSAGES_PER_CLIENT = 100;
    
    std::vector<std::unique_ptr<TestClient>> clients;
    std::atomic<int> total_messages_sent{0};
    std::atomic<int> total_messages_received{0};
    
    // Connect clients
    for (int i = 0; i < CLIENT_COUNT; ++i) {
        clients.push_back(CreateClient(i));
    }
    
    // Start sending messages
    auto start_time = std::chrono::high_resolution_clock::now();
    
    std::vector<std::thread> sender_threads;
    for (int i = 0; i < CLIENT_COUNT; ++i) {
        sender_threads.emplace_back([&clients, i, &total_messages_sent, &total_messages_received]() {
            auto& client = clients[i];
            
            for (int j = 0; j < MESSAGES_PER_CLIENT; ++j) {
                // Send movement update
                MovementPacket movement;
                movement.position = {float(j), 0, float(j)};
                movement.velocity = {1, 0, 0};
                
                if (client->SendMovement(movement)) {
                    total_messages_sent++;
                }
                
                // Small delay to simulate real behavior
                std::this_thread::sleep_for(10ms);
            }
            
            // Count received messages
            total_messages_received += client->GetReceivedMessageCount();
        });
    }
    
    // Wait for all senders
    for (auto& t : sender_threads) {
        t.join();
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
    
    // Calculate throughput
    float messages_per_second = float(total_messages_sent) / duration.count();
    float broadcast_multiplier = float(total_messages_received) / float(total_messages_sent);
    
    std::cout << "=== Throughput Test Results ===\n";
    std::cout << "Messages sent: " << total_messages_sent << "\n";
    std::cout << "Messages received: " << total_messages_received << "\n";
    std::cout << "Duration: " << duration.count() << " seconds\n";
    std::cout << "Throughput: " << messages_per_second << " msg/sec\n";
    std::cout << "Broadcast multiplier: " << broadcast_multiplier << "x\n";
    std::cout << "==============================\n";
    
    // Verify performance
    EXPECT_GT(messages_per_second, 1000); // At least 1000 msg/sec
    EXPECT_GT(broadcast_multiplier, 0.8f); // Most broadcasts received
}

// [SEQUENCE: MVP17-53] Test server tick rate stability
TEST_F(LoadCapacityTest, TickRateStability) {
    const int CLIENT_COUNT = 2000;
    const int TEST_DURATION_SEC = 30;
    
    std::vector<std::unique_ptr<TestClient>> clients;
    
    // Connect clients
    for (int i = 0; i < CLIENT_COUNT; ++i) {
        clients.push_back(CreateClient(i));
    }
    
    // Monitor tick rate
    std::vector<float> tick_rates;
    std::atomic<bool> monitoring{true};
    
    std::thread monitor_thread([this, &tick_rates, &monitoring]() {
        while (monitoring) {
            auto tick_rate = server->GetCurrentTickRate();
            tick_rates.push_back(tick_rate);
            std::this_thread::sleep_for(100ms);
        }
    });
    
    // Simulate game activity
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> action_dist(0, 3);
    
    auto start_time = std::chrono::steady_clock::now();
    
    while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(TEST_DURATION_SEC)) {
        // Random client performs random action
        int client_idx = gen() % CLIENT_COUNT;
        int action = action_dist(gen);
        
        switch (action) {
            case 0: // Movement
                clients[client_idx]->SendMovement({float(gen() % 1000), 0, float(gen() % 1000)});
                break;
            case 1: // Combat
                clients[client_idx]->SendAttack(gen() % CLIENT_COUNT);
                break;
            case 2: // Chat
                clients[client_idx]->SendChat("Test message");
                break;
            case 3: // Skill
                clients[client_idx]->UseSkill(1, gen() % CLIENT_COUNT);
                break;
        }
        
        std::this_thread::sleep_for(1ms);
    }
    
    monitoring = false;
    monitor_thread.join();
    
    // Analyze tick rate stability
    float avg_tick_rate = 0;
    float min_tick_rate = 1000;
    float max_tick_rate = 0;
    
    for (float rate : tick_rates) {
        avg_tick_rate += rate;
        min_tick_rate = std::min(min_tick_rate, rate);
        max_tick_rate = std::max(max_tick_rate, rate);
    }
    avg_tick_rate /= tick_rates.size();
    
    // Calculate standard deviation
    float variance = 0;
    for (float rate : tick_rates) {
        variance += (rate - avg_tick_rate) * (rate - avg_tick_rate);
    }
    variance /= tick_rates.size();
    float std_dev = std::sqrt(variance);
    
    std::cout << "=== Tick Rate Stability ===\n";
    std::cout << "Average: " << avg_tick_rate << " FPS\n";
    std::cout << "Min: " << min_tick_rate << " FPS\n";
    std::cout << "Max: " << max_tick_rate << " FPS\n";
    std::cout << "Std Dev: " << std_dev << "\n";
    std::cout << "==========================\n";
    
    // Verify stability
    EXPECT_GE(avg_tick_rate, 25.0f); // At least 25 FPS average
    EXPECT_GE(min_tick_rate, 20.0f); // Never below 20 FPS
    EXPECT_LE(std_dev, 5.0f); // Low variance
}

// [SEQUENCE: MVP17-54] Test memory usage under load
TEST_F(LoadCapacityTest, MemoryUsageUnderLoad) {
    const int WAVES = 5;
    const int CLIENTS_PER_WAVE = 1000;
    
    std::vector<size_t> memory_readings;
    
    // Initial memory
    memory_readings.push_back(monitor->GetMemoryUsageMB());
    
    for (int wave = 0; wave < WAVES; ++wave) {
        std::vector<std::unique_ptr<TestClient>> wave_clients;
        
        // Connect wave
        for (int i = 0; i < CLIENTS_PER_WAVE; ++i) {
            wave_clients.push_back(CreateClient(wave * CLIENTS_PER_WAVE + i));
        }
        
        // Let them play for a bit
        std::this_thread::sleep_for(5s);
        
        // Record memory
        memory_readings.push_back(monitor->GetMemoryUsageMB());
        
        // Disconnect wave
        wave_clients.clear();
        
        // Wait for cleanup
        std::this_thread::sleep_for(2s);
        
        // Check for memory leaks
        size_t after_cleanup = monitor->GetMemoryUsageMB();
        
        std::cout << "Wave " << wave + 1 << " - Memory: " 
                  << memory_readings.back() << " MB, "
                  << "After cleanup: " << after_cleanup << " MB\n";
    }
    
    // Analyze memory growth
    size_t initial_memory = memory_readings[0];
    size_t final_memory = monitor->GetMemoryUsageMB();
    
    float memory_per_client = float(memory_readings[1] - memory_readings[0]) / CLIENTS_PER_WAVE;
    
    std::cout << "=== Memory Usage Analysis ===\n";
    std::cout << "Initial: " << initial_memory << " MB\n";
    std::cout << "Final: " << final_memory << " MB\n";
    std::cout << "Per client: " << memory_per_client << " MB\n";
    std::cout << "============================\n";
    
    // Verify no major leaks
    EXPECT_LE(final_memory, initial_memory * 1.1f); // Max 10% growth
    EXPECT_LE(memory_per_client, 0.1f); // Max 100KB per client
}

// [SEQUENCE: MVP17-55] Test database query performance
TEST_F(LoadCapacityTest, DatabaseQueryPerformance) {
    const int CLIENT_COUNT = 500;
    const int QUERIES_PER_CLIENT = 10;
    
    std::vector<std::unique_ptr<TestClient>> clients;
    
    // Connect and login clients
    for (int i = 0; i < CLIENT_COUNT; ++i) {
        auto client = CreateClient(i);
        client->Login("testuser" + std::to_string(i), "password");
        clients.push_back(std::move(client));
    }
    
    // Measure query performance
    std::atomic<int> total_queries{0};
    std::atomic<int64_t> total_latency_ms{0};
    
    auto start_time = std::chrono::steady_clock::now();
    
    std::vector<std::thread> query_threads;
    for (int i = 0; i < CLIENT_COUNT; ++i) {
        query_threads.emplace_back([&clients, i, &total_queries, &total_latency_ms]() {
            auto& client = clients[i];
            
            for (int j = 0; j < QUERIES_PER_CLIENT; ++j) {
                auto query_start = std::chrono::steady_clock::now();
                
                // Perform database-heavy operation
                bool success = false;
                switch (j % 4) {
                    case 0:
                        success = client->GetInventory();
                        break;
                    case 1:
                        success = client->GetCharacterStats();
                        break;
                    case 2:
                        success = client->GetFriendsList();
                        break;
                    case 3:
                        success = client->GetGuildInfo();
                        break;
                }
                
                if (success) {
                    auto query_end = std::chrono::steady_clock::now();
                    auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
                        query_end - query_start).count();
                    
                    total_queries++;
                    total_latency_ms += latency;
                }
                
                std::this_thread::sleep_for(50ms); // Spacing
            }
        });
    }
    
    for (auto& t : query_threads) {
        t.join();
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
    
    // Calculate metrics
    float avg_latency = float(total_latency_ms) / total_queries;
    float queries_per_second = float(total_queries) / duration.count();
    
    std::cout << "=== Database Performance ===\n";
    std::cout << "Total queries: " << total_queries << "\n";
    std::cout << "Average latency: " << avg_latency << " ms\n";
    std::cout << "Queries/second: " << queries_per_second << "\n";
    std::cout << "===========================\n";
    
    // Verify performance
    EXPECT_LE(avg_latency, 100.0f); // Max 100ms average
    EXPECT_GE(queries_per_second, 50.0f); // At least 50 QPS
}

// [SEQUENCE: MVP17-56] Test combat system under load
TEST_F(LoadCapacityTest, CombatSystemLoad) {
    const int PLAYER_COUNT = 1000;
    const int COMBAT_DURATION_SEC = 20;
    
    std::vector<std::unique_ptr<TestClient>> clients;
    
    // Create players and enter world
    for (int i = 0; i < PLAYER_COUNT; ++i) {
        auto client = CreateClient(i);
        client->Login("fighter" + std::to_string(i), "password");
        client->EnterWorld();
        clients.push_back(std::move(client));
    }
    
    // Start mass combat
    std::atomic<int> total_attacks{0};
    std::atomic<int> total_skills{0};
    std::atomic<bool> combat_active{true};
    
    std::vector<std::thread> combat_threads;
    std::random_device rd;
    
    for (int i = 0; i < PLAYER_COUNT; ++i) {
        combat_threads.emplace_back([&clients, i, &total_attacks, &total_skills, &combat_active, &rd]() {
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> target_dist(0, PLAYER_COUNT - 1);
            std::uniform_int_distribution<> skill_dist(1, 5);
            
            auto& client = clients[i];
            
            while (combat_active) {
                int target = target_dist(gen);
                if (target != i) { // Don't attack self
                    
                    if (gen() % 3 == 0) {
                        // Use skill
                        if (client->UseSkill(skill_dist(gen), target)) {
                            total_skills++;
                        }
                    } else {
                        // Basic attack
                        if (client->SendAttack(target)) {
                            total_attacks++;
                        }
                    }
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(100 + (gen() % 200)));
            }
        });
    }
    
    // Monitor server performance during combat
    std::vector<float> fps_readings;
    std::vector<float> cpu_readings;
    
    auto start_time = std::chrono::steady_clock::now();
    
    while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(COMBAT_DURATION_SEC)) {
        fps_readings.push_back(server->GetCurrentTickRate());
        cpu_readings.push_back(monitor->GetCPUUsage());
        std::this_thread::sleep_for(500ms);
    }
    
    combat_active = false;
    
    for (auto& t : combat_threads) {
        t.join();
    }
    
    // Analyze results
    float avg_fps = std::accumulate(fps_readings.begin(), fps_readings.end(), 0.0f) / fps_readings.size();
    float avg_cpu = std::accumulate(cpu_readings.begin(), cpu_readings.end(), 0.0f) / cpu_readings.size();
    
    std::cout << "=== Combat Load Test ===\n";
    std::cout << "Total attacks: " << total_attacks << "\n";
    std::cout << "Total skills: " << total_skills << "\n";
    std::cout << "Average FPS: " << avg_fps << "\n";
    std::cout << "Average CPU: " << avg_cpu << "%\n";
    std::cout << "Actions/second: " << (total_attacks + total_skills) / COMBAT_DURATION_SEC << "\n";
    std::cout << "=======================\n";
    
    // Verify performance
    EXPECT_GE(avg_fps, 20.0f); // Maintain 20+ FPS
    EXPECT_LE(avg_cpu, 85.0f); // CPU under 85%
}

private:
    mutable std::mutex clients_mutex;
};