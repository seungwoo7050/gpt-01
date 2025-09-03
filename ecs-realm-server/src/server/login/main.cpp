#include <iostream>
#include <csignal>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include "core/network/tcp_server.h"
#include "core/monitoring/server_monitor.h"
#include "server/login/auth_handler.h"

// [SEQUENCE: 1] Global server instance for signal handling
std::shared_ptr<mmorpg::core::network::TcpServer> g_server;

// [SEQUENCE: 2] Signal handler for graceful shutdown
void SignalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        spdlog::info("Received shutdown signal, stopping server...");
        if (g_server) {
            g_server->Stop();
        }
    }
}

// [SEQUENCE: 3] Setup logging system
void SetupLogging() {
    // Console sink with colors
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(spdlog::level::info);
    
    // File sink with rotation (10MB x 5 files)
    auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        "logs/login_server.log", 1024 * 1024 * 10, 5);
    file_sink->set_level(spdlog::level::debug);
    
    // Create logger with multiple sinks
    std::vector<spdlog::sink_ptr> sinks{console_sink, file_sink};
    auto logger = std::make_shared<spdlog::logger>("login_server", sinks.begin(), sinks.end());
    
    // Set as default logger
    spdlog::set_default_logger(logger);
    spdlog::set_level(spdlog::level::debug);
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");
    
    spdlog::info("Logging system initialized");
}

// [SEQUENCE: 4] Main entry point
int main(int argc, char* argv[]) {
    try {
        // Setup logging
        SetupLogging();
        
        spdlog::info("MMORPG Login Server starting...");
        
        // Register signal handlers
        std::signal(SIGINT, SignalHandler);
        std::signal(SIGTERM, SignalHandler);
        
        // Configure server
        mmorpg::core::network::ServerConfig config;
        config.address = "0.0.0.0";
        config.port = 8080;
        config.worker_threads = 4;
        config.io_context_pool_size = 2;
        config.max_connections = 1000;
        
        // Parse command line arguments
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--port" && i + 1 < argc) {
                config.port = static_cast<uint16_t>(std::stoi(argv[++i]));
            } else if (arg == "--threads" && i + 1 < argc) {
                config.worker_threads = std::stoul(argv[++i]);
            } else if (arg == "--help") {
                std::cout << "Usage: " << argv[0] << " [options]\n"
                         << "Options:\n"
                         << "  --port <port>       Server port (default: 8080)\n"
                         << "  --threads <count>   Worker thread count (default: 4)\n"
                         << "  --help              Show this help message\n";
                return 0;
            }
        }
        
        // Create server
        g_server = std::make_shared<mmorpg::core::network::TcpServer>(config);
        
        // Create and register auth handler
        auto auth_handler = std::make_shared<mmorpg::server::login::AuthHandler>();
        auth_handler->RegisterHandlers(g_server->GetPacketHandler());
        
        // Create server monitor
        auto monitor = std::make_shared<mmorpg::core::monitoring::ServerMonitor>();
        
        // Set up monitoring callback
        monitor->SetMetricsCallback([&](const mmorpg::core::monitoring::ServerMetrics& metrics) {
            // Update connection count
            monitor->SetActiveConnections(g_server->GetConnectionCount());
            
            // Log metrics every 10 seconds
            static auto last_log_time = std::chrono::steady_clock::now();
            auto now = std::chrono::steady_clock::now();
            
            if (std::chrono::duration_cast<std::chrono::seconds>(now - last_log_time).count() >= 10) {
                spdlog::info("Server Metrics - CPU: {:.1f}%, Memory: {:.1f}% ({:.2f}MB), "
                           "Connections: {}, Packets: {} sent / {} received",
                           metrics.cpu_usage_percent,
                           metrics.memory_usage_percent,
                           metrics.memory_usage_bytes / (1024.0 * 1024.0),
                           metrics.active_connections,
                           metrics.packets_sent,
                           metrics.packets_received);
                last_log_time = now;
            }
        });
        
        // Start monitoring
        monitor->Start(std::chrono::seconds(1));
        
        // Start server
        g_server->Start();
        
        spdlog::info("Login server started on port {}", config.port);
        spdlog::info("Press Ctrl+C to stop the server");
        
        // Keep main thread alive
        while (g_server->IsRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        // Cleanup
        monitor->Stop();
        g_server.reset();
        
        spdlog::info("Login server shutdown complete");
        
    } catch (const std::exception& e) {
        spdlog::error("Fatal error: {}", e.what());
        return 1;
    }
    
    return 0;
}