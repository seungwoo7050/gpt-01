#include "core/logger.h"
#include "network/tcp_server.h"
#include <csignal>
#include <iostream>
#include <memory>
#include <thread>
#include <filesystem> // For checking file existence

// [SEQUENCE: MVP1-49] Main Server Application (`src/server/game/main.cpp`)
// [SEQUENCE: MVP1-51] Signal Handling
// Global server instance for signal handling
std::shared_ptr<mmorpg::network::TcpServer> g_server;

void SignalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        mmorpg::core::Logger::GetLogger()->info("Received shutdown signal, stopping server...");
        if (g_server) {
            g_server->Stop();
        }
    }
}

// [SEQUENCE: MVP1-50] `main()`: 프로그램의 시작점. 로거와 서버를 초기화하고 실행합니다.
int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    // Initialize logger
    mmorpg::core::Logger::Initialize();

    // Register signal handlers
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    try {
        // Server configuration
        const std::string address = "0.0.0.0";
        const uint16_t port = 8080;
        const size_t thread_pool_size = 4;
        const std::string cert_file = "server.crt";
        const std::string key_file = "server.key";

        // Dummy check for cert files. In a real scenario, the constructor would be simpler in MVP1.
        if (!std::filesystem::exists(cert_file) || !std::filesystem::exists(key_file)) {
             mmorpg::core::Logger::GetLogger()->warn("SSL certificate or key file not found. The server will likely fail if a client connects.");
             mmorpg::core::Logger::GetLogger()->warn("This is expected for the MVP1 build verification. Please create dummy files if needed: ");
             mmorpg::core::Logger::GetLogger()->warn("openssl req -x509 -newkey rsa:2048 -keyout server.key -out server.crt -days 365 -nodes");
        }

        // Create and start the server
        g_server = std::make_shared<mmorpg::network::TcpServer>(address, port, thread_pool_size, cert_file, key_file);
        g_server->Start();

        mmorpg::core::Logger::GetLogger()->info("Game server started on port {}", port);
        mmorpg::core::Logger::GetLogger()->info("Press Ctrl+C to stop the server");

        // Keep main thread alive
        while (g_server->IsRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        g_server.reset();
        mmorpg::core::Logger::GetLogger()->info("Game server shutdown complete");

    } catch (const std::exception& e) {
        mmorpg::core::Logger::GetLogger()->critical("Fatal error: {}", e.what());
        return 1;
    }

    return 0;
}
