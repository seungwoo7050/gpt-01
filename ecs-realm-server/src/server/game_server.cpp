#include "game_server.h"
#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>

// [SEQUENCE: MVP1-82] Implementation of the GameServer class.
namespace mmorpg::server {

GameServer::GameServer()
    : running_(false),
      world_(std::make_unique<core::ecs::World>()),
      packet_handler_(std::make_shared<core::network::PacketHandler>()),
      packet_queue_(1024) // Packet queue with a capacity of 1024
{
    // Load configuration
    core::config::EnvironmentConfig::Instance().LoadConfiguration();
    auto env_config = core::config::EnvironmentConfig::Instance();

    // Initialize security
    core::security::SecurityManager::Instance().Initialize();
    if (!core::security::SecurityManager::Instance().ValidateSecurityRequirements()) {
        throw std::runtime_error("Security requirements validation failed");
    }

    // Initialize database
    auto db_env_config = env_config.GetDatabaseConfig();
    database::MySQLConnectionPool::Config db_config;
    db_config.host = db_env_config.host;
    db_config.user = db_env_config.username;
    db_config.password = db_env_config.password;
    db_config.database = db_env_config.database;
    db_config.pool_size = db_env_config.pool_size;
    auto mysql_pool = std::make_shared<database::MySQLConnectionPool>(db_config);
    db_manager_ = std::make_shared<database::DatabaseManager>(mysql_pool);

    // Initialize cache
    cache::RedisConnectionPool::Config redis_config;
    redis_config.host = "localhost";
    redis_config.port = 6379;
    redis_config.pool_size = 10;
    auto redis_pool = std::make_shared<cache::RedisConnectionPool>(redis_config);
    session_manager_ = std::make_shared<cache::SessionManager>(redis_pool);

    // Initialize auth service
    const std::string jwt_secret = env_config.GetJWTSecret();
    auth_service_ = std::make_shared<auth::AuthService>(
        db_manager_, session_manager_, jwt_secret
    );

    // Initialize TCP Server
    auto net_config = env_config.GetNetworkConfig();
    core::network::ServerConfig config;
    config.address = net_config.game_server_host;
    config.port = net_config.game_server_port;
    config.worker_threads = net_config.worker_threads;
    config.max_connections = net_config.max_connections;
    tcp_server_ = std::make_unique<core::network::TcpServer>(config, packet_queue_);

    // Register packet handlers
    auto auth_handler = std::make_shared<game::handlers::AuthHandler>(auth_service_);
    packet_handler_->RegisterHandler(
        proto::PACKET_LOGIN_REQUEST,
        [auth_handler](auto session, const auto& packet) {
            auth_handler->HandleLoginRequest(session, packet);
        }
    );
    // ... register other handlers ...
    tcp_server_->SetPacketHandler(packet_handler_);

    // Initialize monitor
    monitor_ = std::make_shared<core::monitoring::ServerMonitor>();

    spdlog::info("GameServer created and initialized.");
}

GameServer::~GameServer() {
    spdlog::info("GameServer destroyed.");
}

void GameServer::Start() {
    if (running_.exchange(true)) {
        return; // Already running
    }
    monitor_->Start(std::chrono::seconds(1));
    tcp_server_->Start();
    spdlog::info("GameServer starting...");
}

void GameServer::Stop() {
    if (!running_.exchange(false)) {
        return; // Already stopped
    }
    tcp_server_->Stop();
    monitor_->Stop();
    spdlog::info("GameServer stopping...");
}

void GameServer::Run() {
    Start();
    auto last_tick_time = std::chrono::steady_clock::now();

    while (running_) {
        auto current_time = std::chrono::steady_clock::now();
        auto delta_time = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_tick_time);

        if (delta_time >= TICK_INTERVAL) {
            last_tick_time = current_time;

            // 1. Process incoming network packets
            ProcessIncomingPackets();

            // 2. Update all game systems
            world_->Update(delta_time.count() / 1000.0f);

            // 3. Other periodic tasks (e.g., save world state)
        }

        // Sleep to prevent busy-waiting and save CPU
        auto time_to_next_tick = std::chrono::steady_clock::now() - last_tick_time;
        if (time_to_next_tick < TICK_INTERVAL) {
            std::this_thread::sleep_for(TICK_INTERVAL - time_to_next_tick);
        }
    }
}

void GameServer::ProcessIncomingPackets() {
    std::pair<uint64_t, std::string> packet_data;
    while (packet_queue_.Dequeue(packet_data)) {
        auto session = tcp_server_->GetSessionManager()->GetSession(packet_data.first);
        if (session) {
            mmorpg::proto::Packet packet;
            if (packet.ParseFromString(packet_data.second)) {
                try {
                    packet_handler_->HandlePacket(session, packet);
                } catch (const std::exception& e) {
                    spdlog::error("Packet processing error: {}", e.what());
                    session->Disconnect();
                }
            } else {
                spdlog::warn("Failed to parse packet from session {}", session->GetSessionId());
            }
        }
    }
}

} // namespace mmorpg::server
