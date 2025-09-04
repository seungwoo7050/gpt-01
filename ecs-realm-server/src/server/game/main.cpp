#include "core/logger.h"
#include "network/tcp_server.h"
#include "network/udp_server.h"
#include "network/packet_handler.h"
#include "network/udp_packet_handler.h"
#include "core/scripting/script_manager.h"
#include "network/session.h"
#include "network/session_manager.h"
#include "proto/auth.pb.h"

#include <boost/asio.hpp>
#include <atomic>
#include <csignal>
#include <iostream>
#include <memory>
#include <thread>
#include <vector>
#include <filesystem>

// [SEQUENCE: MVP9-7] Global server instances for signal handling
std::shared_ptr<mmorpg::network::TcpServer> g_tcp_server;
std::shared_ptr<mmorpg::network::UdpServer> g_udp_server;
boost::asio::io_context* g_io_context = nullptr;

static std::atomic<uint64_t> g_next_player_id{1};

void SignalHandler(int signal) {
    mmorpg::core::Logger::GetLogger()->info("Signal {} received.", signal);
    if (signal == SIGINT || signal == SIGTERM) {
        mmorpg::core::Logger::GetLogger()->info("Received shutdown signal, stopping servers...");
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
        const uint16_t tcp_port = 8080;
        // const uint16_t udp_port = 8081;
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

        // [SEQUENCE: MVP9-6] Register a handler for LoginRequest
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

        // [SEQUENCE: MVP9-8] Instantiate the new TcpServer
        g_tcp_server = std::make_shared<mmorpg::network::TcpServer>(io_context, session_manager, tcp_packet_handler, tcp_port, cert_file, key_file);
        g_tcp_server->run();

        // UDP Server setup (assuming it needs a session manager)
        // g_udp_server = std::make_shared<mmorpg::network::UdpServer>(io_context, udp_port, session_manager);
        // g_udp_server->run();

        mmorpg::core::Logger::GetLogger()->info("Press Ctrl+C to stop the servers");

        // Run the io_context on a pool of threads
        // std::vector<std::thread> threads;
        // for (size_t i = 0; i < thread_pool_size; ++i) {
        //     threads.emplace_back([&io_context]() {
        //         io_context.run();
        //     });
        // }

        mmorpg::core::Logger::GetLogger()->info("Calling io_context.run()...");
        io_context.run();
        mmorpg::core::Logger::GetLogger()->info("io_context.run() returned.");

        // for (auto& t : threads) {
        //     if (t.joinable()) {
        //         t.join();
        //     }
        // }

        g_tcp_server.reset();
        g_udp_server.reset();
        mmorpg::core::Logger::GetLogger()->info("Game server shutdown complete");

    } catch (const std::exception& e) {
        mmorpg::core::Logger::GetLogger()->critical("Fatal error: {}", e.what());
        return 1;
    }

    return 0;
}