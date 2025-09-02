#include <iostream>
#include <csignal>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include "core/network/tcp_server.h"
#include "core/monitoring/server_monitor.h"
// [SEQUENCE: 407] Infrastructure integration for MVP7
#include "core/network/packet_handler.h"
#include "core/database/database_manager.h"
#include "core/cache/session_manager.h"
#include "core/auth/auth_service.h"
#include "game/handlers/auth_handler.h"

// [SEQUENCE: 1] Global server instance for signal handling
std::shared_ptr<mmorpg::core::network::TcpServer> g_server;

// [SEQUENCE: 2] Signal handler
void SignalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        spdlog::info("Received shutdown signal, stopping server...");
        if (g_server) {
            g_server->Stop();
        }
    }
}

// [SEQUENCE: 3] Main entry point
int main(int argc, char* argv[]) {
    try {
        // Setup logging
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "logs/game_server.log", 1024 * 1024 * 10, 5);
        
        std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
        auto logger = std::make_shared<spdlog::logger>("game_server", sinks.begin(), sinks.end());
        spdlog::set_default_logger(logger);
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
        
        spdlog::info("MMORPG Game Server starting...");
        
        // Register signal handlers
        std::signal(SIGINT, SignalHandler);
        std::signal(SIGTERM, SignalHandler);
        
        // Configure server
        mmorpg::core::network::ServerConfig config;
        config.address = "0.0.0.0";
        config.port = 8081;  // Different port from login server
        config.worker_threads = 8;  // More threads for game server
        config.io_context_pool_size = 4;
        config.max_connections = 5000;
        
        // [SEQUENCE: 408] Initialize infrastructure (MVP7)
        // MySQL connection pool
        mmorpg::database::MySQLConnectionPool::Config db_config;
        db_config.host = "localhost";
        db_config.user = "mmorpg_user";
        db_config.password = "password";
        db_config.database = "mmorpg";
        db_config.pool_size = 20;
        
        auto mysql_pool = std::make_shared<mmorpg::database::MySQLConnectionPool>(db_config);
        auto db_manager = std::make_shared<mmorpg::database::DatabaseManager>(mysql_pool);
        
        // Redis connection pool
        mmorpg::cache::RedisConnectionPool::Config redis_config;
        redis_config.host = "localhost";
        redis_config.port = 6379;
        redis_config.pool_size = 10;
        
        auto redis_pool = std::make_shared<mmorpg::cache::RedisConnectionPool>(redis_config);
        auto session_manager = std::make_shared<mmorpg::cache::SessionManager>(redis_pool);
        
        // [SEQUENCE: 409] Create auth service
        const std::string jwt_secret = "your-super-secret-key-change-this-in-production";
        auto auth_service = std::make_shared<mmorpg::auth::AuthService>(
            db_manager, session_manager, jwt_secret
        );
        
        // Create server
        g_server = std::make_shared<mmorpg::core::network::TcpServer>(config);
        
        // [SEQUENCE: 410] Create and register handlers
        auto packet_handler = std::make_shared<mmorpg::core::network::PacketHandler>();
        auto auth_handler = std::make_shared<mmorpg::game::handlers::AuthHandler>(auth_service);
        
        // Register auth handlers
        packet_handler->RegisterHandler(
            mmorpg::proto::PACKET_LOGIN_REQUEST,
            [auth_handler](auto session, const auto& packet) {
                auth_handler->HandleLoginRequest(session, packet);
            }
        );
        
        packet_handler->RegisterHandler(
            mmorpg::proto::PACKET_LOGOUT_REQUEST,
            [auth_handler](auto session, const auto& packet) {
                auth_handler->HandleLogoutRequest(session, packet);
            }
        );
        
        packet_handler->RegisterHandler(
            mmorpg::proto::PACKET_HEARTBEAT_REQUEST,
            [auth_handler](auto session, const auto& packet) {
                auth_handler->HandleHeartbeatRequest(session, packet);
            }
        );
        
        // [SEQUENCE: 411] Set packet handler in server
        g_server->SetPacketHandler(packet_handler);
        
        // Create server monitor
        auto monitor = std::make_shared<mmorpg::core::monitoring::ServerMonitor>();
        monitor->Start(std::chrono::seconds(1));
        
        // Start server
        g_server->Start();
        
        spdlog::info("Game server started on port {}", config.port);
        spdlog::info("Press Ctrl+C to stop the server");
        
        // Keep main thread alive
        while (g_server->IsRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        // Cleanup
        monitor->Stop();
        g_server.reset();
        
        spdlog::info("Game server shutdown complete");
        
    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        return 1;
    }
    
    return 0;
}