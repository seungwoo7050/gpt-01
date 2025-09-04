#include "core/logger.h"
#include "network/tcp_server.h"
#include "network/udp_server.h"
#include "network/packet_handler.h"
#include "network/udp_packet_handler.h"
#include "core/scripting/script_manager.h"
#include <csignal>
#include <iostream>
#include <memory>
#include <thread>
#include <filesystem>

std::shared_ptr<mmorpg::network::TcpServer> g_tcp_server;
std::shared_ptr<mmorpg::network::UdpServer> g_udp_server;

void SignalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        mmorpg::core::Logger::GetLogger()->info("Received shutdown signal, stopping servers...");
        if (g_tcp_server) {
            g_tcp_server->Stop();
        }
        if (g_udp_server) {
            g_udp_server->Stop();
        }
    }
}

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    mmorpg::core::Logger::Initialize();
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    try {
        const std::string address = "0.0.0.0";
        const uint16_t tcp_port = 8080;
        const uint16_t udp_port = 8081;
        const size_t thread_pool_size = 4;
        const std::string cert_file = "server.crt";
        const std::string key_file = "server.key";

        if (!std::filesystem::exists(cert_file) || !std::filesystem::exists(key_file)) {
             mmorpg::core::Logger::GetLogger()->warn("SSL certificate or key file not found. The server will likely fail if a client connects.");
             mmorpg::core::Logger::GetLogger()->warn("This is expected for the MVP1 build verification. Please create dummy files if needed: ");
             mmorpg::core::Logger::GetLogger()->warn("openssl req -x509 -newkey rsa:2048 -keyout server.key -out server.crt -days 365 -nodes");
        }

        g_tcp_server = std::make_shared<mmorpg::network::TcpServer>(address, tcp_port, thread_pool_size, cert_file, key_file);
        auto session_manager = g_tcp_server->GetSessionManager();
        g_udp_server = std::make_shared<mmorpg::network::UdpServer>(udp_port, *session_manager);

        // [SEQUENCE: MVP6-37] Create and set packet handlers
        auto tcp_packet_handler = std::make_shared<mmorpg::network::PacketHandler>();
        // TODO: Register actual TCP handlers for game logic here
        g_tcp_server->SetPacketHandler(tcp_packet_handler);

        auto udp_packet_handler = std::make_shared<mmorpg::network::UdpPacketHandler>(*session_manager);
        g_udp_server->SetPacketHandler(udp_packet_handler);

        // [SEQUENCE: MVP8-17] Initialize and run a test script
        auto& script_manager = mmorpg::scripting::ScriptManager::Instance();
        script_manager.Initialize();
        script_manager.RunScriptFile("ecs-realm-server/scripts/test.lua");

        g_tcp_server->Start();
        g_udp_server->Start();
        
        mmorpg::core::Logger::GetLogger()->info("TCP server started on port {}", tcp_port);
        mmorpg::core::Logger::GetLogger()->info("UDP server started on port {}", udp_port);
        mmorpg::core::Logger::GetLogger()->info("Press Ctrl+C to stop the servers");

        while (g_tcp_server->IsRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
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
