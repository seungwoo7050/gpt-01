#include <iostream>
#include <boost/asio.hpp>
#include <thread>
#include <chrono>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include "proto/auth.pb.h"
#include "proto/packet.pb.h"
#include "core/network/packet_serializer.h"

using boost::asio::ip::tcp;

// [SEQUENCE: 1] Simple test client class
class SimpleClient {
public:
    SimpleClient(boost::asio::io_context& io_context, 
                 const std::string& host, 
                 uint16_t port)
        : io_context_(io_context)
        , socket_(io_context)
        , host_(host)
        , port_(port) {}
    
    // [SEQUENCE: 2] Connect to server
    bool Connect() {
        try {
            tcp::resolver resolver(io_context_);
            auto endpoints = resolver.resolve(host_, std::to_string(port_));
            
            boost::asio::connect(socket_, endpoints);
            
            spdlog::info("Connected to {}:{}", host_, port_);
            is_connected_ = true;
            
            // Start async read
            DoReadHeader();
            
            return true;
        } catch (const std::exception& e) {
            spdlog::error("Connection failed: {}", e.what());
            return false;
        }
    }
    
    // [SEQUENCE: 3] Send login request
    void SendLoginRequest(const std::string& username, const std::string& password) {
        mmorpg::proto::LoginRequest request;
        request.set_username(username);
        request.set_password_hash(password); // In real app, this would be hashed
        request.set_client_version("1.0.0");
        request.set_device_id("test-client-001");
        
        auto packet = mmorpg::core::network::PacketSerializer::CreatePacket(
            mmorpg::proto::PACKET_LOGIN_REQUEST, request, sequence_++);
        
        SendPacket(packet);
        spdlog::info("Sent login request for user '{}'", username);
    }
    
    // [SEQUENCE: 4] Send heartbeat
    void SendHeartbeat() {
        mmorpg::proto::HeartbeatRequest request;
        request.set_player_id(player_id_);
        request.set_timestamp(std::chrono::system_clock::now().time_since_epoch().count());
        
        auto packet = mmorpg::core::network::PacketSerializer::CreatePacket(
            mmorpg::proto::PACKET_HEARTBEAT_REQUEST, request, sequence_++);
        
        SendPacket(packet);
    }
    
    // [SEQUENCE: 5] Send logout request
    void SendLogoutRequest() {
        if (!is_authenticated_) {
            spdlog::warn("Not authenticated, cannot logout");
            return;
        }
        
        mmorpg::proto::LogoutRequest request;
        request.set_player_id(player_id_);
        request.set_session_token(session_token_);
        
        auto packet = mmorpg::core::network::PacketSerializer::CreatePacket(
            mmorpg::proto::PACKET_LOGOUT_REQUEST, request, sequence_++);
        
        SendPacket(packet);
        spdlog::info("Sent logout request");
    }
    
    // [SEQUENCE: 6] Disconnect
    void Disconnect() {
        if (is_connected_) {
            boost::system::error_code ec;
            socket_.shutdown(tcp::socket::shutdown_both, ec);
            socket_.close(ec);
            is_connected_ = false;
            spdlog::info("Disconnected from server");
        }
    }
    
    bool IsConnected() const { return is_connected_; }
    bool IsAuthenticated() const { return is_authenticated_; }
    
private:
    // [SEQUENCE: 7] Send packet
    void SendPacket(const mmorpg::proto::Packet& packet) {
        if (!is_connected_) {
            return;
        }
        
        auto buffer = mmorpg::core::network::PacketSerializer::SerializeWithHeader(packet);
        
        boost::asio::async_write(socket_,
            boost::asio::buffer(buffer),
            [this](boost::system::error_code ec, std::size_t length) {
                if (ec) {
                    spdlog::error("Write error: {}", ec.message());
                    Disconnect();
                }
            });
    }
    
    // [SEQUENCE: 8] Read packet header
    void DoReadHeader() {
        boost::asio::async_read(socket_,
            boost::asio::buffer(header_buffer_, 4), // Size prefix
            [this](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    uint32_t packet_size;
                    std::memcpy(&packet_size, header_buffer_.data(), 4);
                    
                    if (packet_size > 1024 * 1024) { // 1MB max
                        spdlog::error("Packet too large: {} bytes", packet_size);
                        Disconnect();
                        return;
                    }
                    
                    body_buffer_.resize(packet_size);
                    DoReadBody();
                } else {
                    spdlog::error("Read header error: {}", ec.message());
                    Disconnect();
                }
            });
    }
    
    // [SEQUENCE: 9] Read packet body
    void DoReadBody() {
        boost::asio::async_read(socket_,
            boost::asio::buffer(body_buffer_),
            [this](boost::system::error_code ec, std::size_t length) {
                if (!ec) {
                    ProcessPacket();
                    DoReadHeader(); // Continue reading
                } else {
                    spdlog::error("Read body error: {}", ec.message());
                    Disconnect();
                }
            });
    }
    
    // [SEQUENCE: 10] Process received packet
    void ProcessPacket() {
        mmorpg::proto::Packet packet;
        if (!packet.ParseFromArray(body_buffer_.data(), body_buffer_.size())) {
            spdlog::error("Failed to parse packet");
            return;
        }
        
        switch (packet.header().type()) {
            case mmorpg::proto::PACKET_LOGIN_RESPONSE:
                HandleLoginResponse(packet);
                break;
                
            case mmorpg::proto::PACKET_HEARTBEAT_RESPONSE:
                HandleHeartbeatResponse(packet);
                break;
                
            case mmorpg::proto::PACKET_LOGOUT_RESPONSE:
                HandleLogoutResponse(packet);
                break;
                
            default:
                spdlog::warn("Unhandled packet type: {}", packet.header().type());
        }
    }
    
    // [SEQUENCE: 11] Handle login response
    void HandleLoginResponse(const mmorpg::proto::Packet& packet) {
        mmorpg::proto::LoginResponse response;
        if (!mmorpg::core::network::PacketSerializer::ExtractMessage(packet, response)) {
            spdlog::error("Failed to parse LoginResponse");
            return;
        }
        
        if (response.success()) {
            is_authenticated_ = true;
            player_id_ = response.player_id();
            session_token_ = response.session_token();
            
            spdlog::info("Login successful! Player ID: {}", player_id_);
            
            // Show available servers
            for (const auto& server : response.game_servers()) {
                spdlog::info("  Game Server: {} - {}:{} ({}/{} players, {:.1f}% load)",
                           server.server_name(),
                           server.ip_address(),
                           server.port(),
                           server.current_players(),
                           server.max_players(),
                           server.load_percentage());
            }
        } else {
            spdlog::error("Login failed: {} - {}", 
                         response.error_code(), 
                         response.error_message());
        }
    }
    
    // [SEQUENCE: 12] Handle heartbeat response
    void HandleHeartbeatResponse(const mmorpg::proto::Packet& packet) {
        mmorpg::proto::HeartbeatResponse response;
        if (!mmorpg::core::network::PacketSerializer::ExtractMessage(packet, response)) {
            return;
        }
        
        spdlog::debug("Heartbeat response - Latency: {}ms", response.latency_ms());
    }
    
    // [SEQUENCE: 13] Handle logout response
    void HandleLogoutResponse(const mmorpg::proto::Packet& packet) {
        mmorpg::proto::LogoutResponse response;
        if (!mmorpg::core::network::PacketSerializer::ExtractMessage(packet, response)) {
            return;
        }
        
        if (response.success()) {
            spdlog::info("Logout successful");
            is_authenticated_ = false;
        } else {
            spdlog::error("Logout failed: {}", response.error_code());
        }
    }
    
    // Network components
    boost::asio::io_context& io_context_;
    tcp::socket socket_;
    std::string host_;
    uint16_t port_;
    
    // Connection state
    bool is_connected_ = false;
    bool is_authenticated_ = false;
    uint64_t player_id_ = 0;
    std::string session_token_;
    
    // Packet sequence
    uint64_t sequence_ = 0;
    
    // Read buffers
    std::array<uint8_t, 4> header_buffer_;
    std::vector<uint8_t> body_buffer_;
};

// [SEQUENCE: 14] Main function
int main(int argc, char* argv[]) {
    // Setup logging
    auto console = spdlog::stdout_color_mt("console");
    spdlog::set_default_logger(console);
    spdlog::set_level(spdlog::level::info);
    spdlog::set_pattern("[%H:%M:%S.%e] [%^%l%$] %v");
    
    // Parse arguments
    std::string host = "127.0.0.1";
    uint16_t port = 8080;
    std::string username = "test1";
    std::string password = "password1";
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--user" && i + 1 < argc) {
            username = argv[++i];
        } else if (arg == "--pass" && i + 1 < argc) {
            password = argv[++i];
        } else if (arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n"
                     << "Options:\n"
                     << "  --host <ip>      Server IP (default: 127.0.0.1)\n"
                     << "  --port <port>    Server port (default: 8080)\n"
                     << "  --user <name>    Username (default: test1)\n"
                     << "  --pass <pass>    Password (default: password1)\n"
                     << "  --help           Show this help message\n";
            return 0;
        }
    }
    
    try {
        boost::asio::io_context io_context;
        
        // Create client
        SimpleClient client(io_context, host, port);
        
        // Run io_context in separate thread
        std::thread io_thread([&io_context]() {
            io_context.run();
        });
        
        // Connect to server
        if (!client.Connect()) {
            io_context.stop();
            io_thread.join();
            return 1;
        }
        
        // Wait a bit for connection to stabilize
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Send login request
        client.SendLoginRequest(username, password);
        
        // Wait for authentication
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        if (client.IsAuthenticated()) {
            // Send periodic heartbeats
            for (int i = 0; i < 5; ++i) {
                std::this_thread::sleep_for(std::chrono::seconds(2));
                client.SendHeartbeat();
            }
            
            // Logout
            client.SendLogoutRequest();
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
        
        // Disconnect
        client.Disconnect();
        
        // Stop io_context
        io_context.stop();
        io_thread.join();
        
        spdlog::info("Client test completed");
        
    } catch (const std::exception& e) {
        spdlog::error("Error: {}", e.what());
        return 1;
    }
    
    return 0;
}