#include "server/game_server.h"

// [SEQUENCE: MVP1-60] Game server main entry point.

std::unique_ptr<mmorpg::server::GameServer> g_game_server;

void SignalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        spdlog::info("Received shutdown signal, stopping server...");
        if (g_game_server) {
            g_game_server->Stop();
        }
    }
}

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
        
        std::signal(SIGINT, SignalHandler);
        std::signal(SIGTERM, SignalHandler);
        
        g_game_server = std::make_unique<mmorpg::server::GameServer>();
        g_game_server->Run();
        
        g_game_server.reset();
        
        spdlog::info("Game server shutdown complete");
        
    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        return 1;
    }
    
    return 0;
}
