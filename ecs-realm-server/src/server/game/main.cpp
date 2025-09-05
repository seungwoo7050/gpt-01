#include "core/logger.h"
#include "network/tcp_server.h"
#include "network/udp_server.h"
#include "network/packet_handler.h"
#include "network/udp_packet_handler.h"
#include "network/session.h"
#include "network/session_manager.h"
#include "proto/auth.pb.h"
#include "game/systems/pvp_manager.h"
#include "database/connection_pool.h"
#include "database/distributed_lock_manager.h"
#include "database/cache_manager.h"
#include "core/scripting/script_manager.h"

#include <boost/asio.hpp>
#include <atomic>
#include <csignal>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <filesystem>
#include <chrono>

// Global pointers for signal handling
std::shared_ptr<mmorpg::network::TcpServer> g_tcp_server;
std::shared_ptr<mmorpg::network::UdpServer> g_udp_server;
boost::asio::io_context* g_io_context = nullptr;

void SignalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        mmorpg::core::Logger::GetLogger()->info("Received shutdown signal, stopping servers...");

        // Shutdown managers
        mmorpg::database::ConnectionPoolManager::Instance().ShutdownAll();
        mmorpg::database::CacheManager::Instance().Shutdown();

        if (g_tcp_server) {
            g_tcp_server->stop();
        }
        if (g_udp_server) {
            g_udp_server->Stop();
        }
        if (g_io_context && !g_io_context->stopped()) {
            g_io_context->stop();
        }
    }
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    mmorpg::core::Logger::Initialize();
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    try {
        // [SEQUENCE: MVP7-47] Initialize all database and distributed systems managers.
        {
            mmorpg::core::Logger::GetLogger()->info("Initializing backend services...");

            // DB Connection Pool
            mmorpg::database::ConnectionPoolConfig db_config;
            db_config.host = "127.0.0.1";
            db_config.port = 3306;
            db_config.user = "user";
            db_config.password = "password";
            db_config.schema = "mmorpg";
            db_config.initial_size = 5;
            db_config.max_size = 20;
            mmorpg::database::ConnectionPoolManager::Instance().CreatePool("primary", db_config);

            // Distributed Lock Manager
            const std::string redis_uri = "tcp://127.0.0.1:6379";
            mmorpg::database::DistributedLockManager::Instance().Initialize(redis_uri);

            // Cache Manager (starts its own eviction thread on first Instance() call)
            mmorpg::database::CacheManager::Instance().GetOrCreateCache("player_data");

            // [SEQUENCE: MVP8-8] Initialize the script manager and run a test script.
            mmorpg::scripting::ScriptManager::Instance().Initialize();
            mmorpg::scripting::ScriptManager::Instance().RunScriptFile("ecs-realm-server/scripts/test.lua");

            mmorpg::core::Logger::GetLogger()->info("Backend services initialized.");
        }

        const uint16_t tcp_port = 8080;
        const uint16_t udp_port = 8081;
        const size_t thread_pool_size = 4;
        const std::string cert_file = "server.crt";
        const std::string key_file = "server.key";

        if (!std::filesystem::exists(cert_file) || !std::filesystem::exists(key_file)) {
             mmorpg::core::Logger::GetLogger()->error("SSL certificate or key file not found!");
             mmorpg::core::Logger::GetLogger()->error("Please generate them using: openssl req -x509 -newkey rsa:2048 -keyout server.key -out server.crt -days 365 -nodes");
             return 1;
        }

        boost::asio::io_context io_context(thread_pool_size);
        g_io_context = &io_context;

        auto session_manager = std::make_shared<mmorpg::network::SessionManager>();
        auto tcp_packet_handler = std::make_shared<mmorpg::network::PacketHandler>();
        auto pvp_manager = std::make_shared<mmorpg::game::systems::PvpManager>();

        static std::atomic<uint64_t> g_next_player_id{1};
        tcp_packet_handler->RegisterHandler(mmorpg::proto::LoginRequest::descriptor(),
            [session_manager](std::shared_ptr<mmorpg::network::Session> session, const google::protobuf::Message& message) {
                const auto& req = static_cast<const mmorpg::proto::LoginRequest&>(message);
                mmorpg::proto::LoginResponse resp;
                resp.set_success(true);
                uint64_t player_id = g_next_player_id++;
                resp.set_player_id(player_id);
                resp.set_session_token("dummy-token-for-load-test");
                session->SetPlayerId(player_id);
                session_manager->SetPlayerIdForSession(session->GetSessionId(), player_id);
                session->Send(resp);
                mmorpg::core::Logger::GetLogger()->info("Processed login for user '{}', assigned player_id {}", req.username(), player_id);
            });

        g_tcp_server = std::make_shared<mmorpg::network::TcpServer>(io_context, session_manager, tcp_packet_handler, tcp_port, cert_file, key_file);
        g_tcp_server->run();

        auto udp_packet_handler = std::make_shared<mmorpg::network::UdpPacketHandler>(*session_manager);
        g_udp_server = std::make_shared<mmorpg::network::UdpServer>(udp_port, *session_manager);
        g_udp_server->SetPacketHandler(udp_packet_handler);
        g_udp_server->Start();

        mmorpg::core::Logger::GetLogger()->info("Press Ctrl+C to stop the servers");

        std::vector<std::thread> threads;
        for (size_t i = 0; i < thread_pool_size; ++i) {
            threads.emplace_back([&io_context]() {
                io_context.run();
            });
        }

        auto last_time = std::chrono::high_resolution_clock::now();
        while (!io_context.stopped()) {
            auto current_time = std::chrono::high_resolution_clock::now();
            float delta_time = std::chrono::duration<float>(current_time - last_time).count();
            last_time = current_time;

            pvp_manager->Update(delta_time);

            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
        }

        for (auto& t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }

        g_tcp_server.reset();
        g_udp_server.reset();

        mmorpg::core::Logger::GetLogger()->info("Game server shutdown complete");

    } catch (const std::exception& e) {
        mmorpg::core::Logger::GetLogger()->critical("Fatal error: {}", e.what());
        return 1;
    }

    return 0;
}