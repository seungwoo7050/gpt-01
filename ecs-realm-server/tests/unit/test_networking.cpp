#include <gtest/gtest.h>
#include <thread>
#include <future>
#include "network/tcp_server.h"
#include "network/session.h"
#include "network/packet_handler.h"
#include "proto/packet.pb.h"

using namespace mmorpg::network;
using namespace std::chrono_literals;

// [SEQUENCE: MVP1-65] Basic networking tests
class NetworkingTest : public ::testing::Test {
protected:
    std::unique_ptr<TcpServer> server;
    std::unique_ptr<boost::asio::io_context> client_io;
    uint16_t test_port = 9999;
    
    void SetUp() override {
        // Start test server on random port
        server = std::make_unique<TcpServer>(test_port);
        
        // Set up client io context
        client_io = std::make_unique<boost::asio::io_context>();
    }
    
    void TearDown() override {
        if (server) {
            server->Stop();
        }
        if (client_io) {
            client_io->stop();
        }
    }
};

// [SEQUENCE: MVP1-66] Test server startup and shutdown
TEST_F(NetworkingTest, ServerStartupShutdown) {
    // Start server
    std::thread server_thread([this]() {
        server->Start();
    });
    
    // Give server time to start
    std::this_thread::sleep_for(100ms);
    
    // Server should be running
    EXPECT_TRUE(server->IsRunning());
    
    // Stop server
    server->Stop();
    server_thread.join();
    
    // Server should be stopped
    EXPECT_FALSE(server->IsRunning());
}

// [SEQUENCE: MVP1-67] Test client connection
TEST_F(NetworkingTest, ClientConnection) {
    // Start server
    std::atomic<bool> client_connected{false};
    
    server->SetOnConnect([&client_connected](std::shared_ptr<Session> session) {
        client_connected = true;
    });
    
    std::thread server_thread([this]() {
        server->Start();
    });
    
    std::this_thread::sleep_for(100ms);
    
    // Connect client
    boost::asio::ip::tcp::socket client_socket(*client_io);
    boost::asio::ip::tcp::endpoint endpoint(
        boost::asio::ip::address::from_string("127.0.0.1"), 
        test_port
    );
    
    client_socket.connect(endpoint);
    
    // Wait for connection callback
    std::this_thread::sleep_for(100ms);
    
    EXPECT_TRUE(client_connected);
    EXPECT_EQ(server->GetConnectionCount(), 1);
    
    // Disconnect
    client_socket.close();
    std::this_thread::sleep_for(100ms);
    
    EXPECT_EQ(server->GetConnectionCount(), 0);
    
    server->Stop();
    server_thread.join();
}

// [SEQUENCE: MVP1-68] Test packet sending and receiving
TEST_F(NetworkingTest, PacketTransmission) {
    std::promise<bool> packet_received;
    auto packet_future = packet_received.get_future();
    
    // Set up packet handler
    auto handler = std::make_shared<PacketHandler>();
    handler->RegisterHandler(PacketType::PING, 
        [&packet_received](std::shared_ptr<Session> session, const Packet& packet) {
            // Verify packet content
            PingPacket ping;
            if (packet.body().UnpackTo(&ping)) {
                if (ping.timestamp() == 12345) {
                    packet_received.set_value(true);
                    
                    // Send pong response
                    PongPacket pong;
                    pong.set_timestamp(ping.timestamp());
                    
                    Packet response;
                    response.set_type(PacketType::PONG);
                    response.mutable_body()->PackFrom(pong);
                    
                    session->Send(response);
                }
            }
        }
    );
    
    server->SetPacketHandler(handler);
    
    // Start server
    std::thread server_thread([this]() {
        server->Start();
    });
    
    std::this_thread::sleep_for(100ms);
    
    // Connect and send packet
    boost::asio::ip::tcp::socket client_socket(*client_io);
    boost::asio::ip::tcp::endpoint endpoint(
        boost::asio::ip::address::from_string("127.0.0.1"), 
        test_port
    );
    
    client_socket.connect(endpoint);
    
    // Create and send ping packet
    PingPacket ping;
    ping.set_timestamp(12345);
    
    Packet packet;
    packet.set_type(PacketType::PING);
    packet.set_sequence(1);
    packet.mutable_body()->PackFrom(ping);
    
    // Serialize and send
    std::string serialized = packet.SerializeAsString();
    uint32_t size = htonl(serialized.size());
    
    boost::asio::write(client_socket, boost::asio::buffer(&size, sizeof(size)));
    boost::asio::write(client_socket, boost::asio::buffer(serialized));
    
    // Wait for packet to be received
    EXPECT_TRUE(packet_future.wait_for(1s) == std::future_status::ready);
    EXPECT_TRUE(packet_future.get());
    
    // Read pong response
    uint32_t response_size;
    boost::asio::read(client_socket, boost::asio::buffer(&response_size, sizeof(response_size)));
    response_size = ntohl(response_size);
    
    std::vector<char> response_buffer(response_size);
    boost::asio::read(client_socket, boost::asio::buffer(response_buffer));
    
    Packet response_packet;
    EXPECT_TRUE(response_packet.ParseFromArray(response_buffer.data(), response_size));
    EXPECT_EQ(response_packet.type(), PacketType::PONG);
    
    PongPacket pong;
    EXPECT_TRUE(response_packet.body().UnpackTo(&pong));
    EXPECT_EQ(pong.timestamp(), 12345);
    
    client_socket.close();
    server->Stop();
    server_thread.join();
}

// [SEQUENCE: MVP1-69] Test multiple concurrent connections
TEST_F(NetworkingTest, ConcurrentConnections) {
    const int NUM_CLIENTS = 100;
    
    std::atomic<int> connected_count{0};
    server->SetOnConnect([&connected_count](std::shared_ptr<Session> session) {
        connected_count++;
    });
    
    // Start server
    std::thread server_thread([this]() {
        server->Start();
    });
    
    std::this_thread::sleep_for(100ms);
    
    // Connect multiple clients
    std::vector<std::unique_ptr<boost::asio::ip::tcp::socket>> clients;
    std::vector<std::thread> client_threads;
    
    for (int i = 0; i < NUM_CLIENTS; ++i) {
        client_threads.emplace_back([this, i, &clients]() {
            auto socket = std::make_unique<boost::asio::ip::tcp::socket>(*client_io);
            boost::asio::ip::tcp::endpoint endpoint(
                boost::asio::ip::address::from_string("127.0.0.1"), 
                test_port
            );
            
            try {
                socket->connect(endpoint);
                
                std::lock_guard<std::mutex> lock(clients_mutex);
                clients.push_back(std::move(socket));
            } catch (...) {
                // Connection failed
            }
        });
    }
    
    // Wait for all connections
    for (auto& t : client_threads) {
        t.join();
    }
    
    std::this_thread::sleep_for(200ms);
    
    // Verify connection count
    EXPECT_EQ(connected_count, clients.size());
    EXPECT_EQ(server->GetConnectionCount(), clients.size());
    
    // Disconnect all
    clients.clear();
    std::this_thread::sleep_for(200ms);
    
    EXPECT_EQ(server->GetConnectionCount(), 0);
    
    server->Stop();
    server_thread.join();
}

// [SEQUENCE: MVP1-70] Test packet fragmentation handling
TEST_F(NetworkingTest, LargePacketHandling) {
    std::promise<bool> large_packet_received;
    auto future = large_packet_received.get_future();
    
    // Create large payload
    const size_t LARGE_SIZE = 1024 * 1024; // 1MB
    std::string large_data(LARGE_SIZE, 'X');
    
    auto handler = std::make_shared<PacketHandler>();
    handler->RegisterHandler(PacketType::CUSTOM, 
        [&large_packet_received, &large_data](std::shared_ptr<Session> session, const Packet& packet) {
            CustomPacket custom;
            if (packet.body().UnpackTo(&custom)) {
                if (custom.data() == large_data) {
                    large_packet_received.set_value(true);
                }
            }
        }
    );
    
    server->SetPacketHandler(handler);
    
    // Start server
    std::thread server_thread([this]() {
        server->Start();
    });
    
    std::this_thread::sleep_for(100ms);
    
    // Connect and send large packet
    boost::asio::ip::tcp::socket client_socket(*client_io);
    boost::asio::ip::tcp::endpoint endpoint(
        boost::asio::ip::address::from_string("127.0.0.1"), 
        test_port
    );
    
    client_socket.connect(endpoint);
    
    // Create large packet
    CustomPacket custom;
    custom.set_data(large_data);
    
    Packet packet;
    packet.set_type(PacketType::CUSTOM);
    packet.mutable_body()->PackFrom(custom);
    
    // Send
    std::string serialized = packet.SerializeAsString();
    uint32_t size = htonl(serialized.size());
    
    boost::asio::write(client_socket, boost::asio::buffer(&size, sizeof(size)));
    boost::asio::write(client_socket, boost::asio::buffer(serialized));
    
    // Verify reception
    EXPECT_TRUE(future.wait_for(5s) == std::future_status::ready);
    EXPECT_TRUE(future.get());
    
    client_socket.close();
    server->Stop();
    server_thread.join();
}

// [SEQUENCE: MVP1-71] Test session timeout
TEST_F(NetworkingTest, SessionTimeout) {
    // Configure short timeout
    server->SetSessionTimeout(500ms);
    
    std::atomic<bool> session_closed{false};
    server->SetOnDisconnect([&session_closed](std::shared_ptr<Session> session) {
        session_closed = true;
    });
    
    // Start server
    std::thread server_thread([this]() {
        server->Start();
    });
    
    std::this_thread::sleep_for(100ms);
    
    // Connect client but don't send anything
    boost::asio::ip::tcp::socket client_socket(*client_io);
    boost::asio::ip::tcp::endpoint endpoint(
        boost::asio::ip::address::from_string("127.0.0.1"), 
        test_port
    );
    
    client_socket.connect(endpoint);
    EXPECT_EQ(server->GetConnectionCount(), 1);
    
    // Wait for timeout
    std::this_thread::sleep_for(1s);
    
    // Session should be closed due to timeout
    EXPECT_TRUE(session_closed);
    EXPECT_EQ(server->GetConnectionCount(), 0);
    
    server->Stop();
    server_thread.join();
}

// [SEQUENCE: MVP1-72] Test broadcast functionality
TEST_F(NetworkingTest, BroadcastMessage) {
    const int NUM_CLIENTS = 5;
    std::atomic<int> messages_received{0};
    
    auto handler = std::make_shared<PacketHandler>();
    server->SetPacketHandler(handler);
    
    // Start server
    std::thread server_thread([this]() {
        server->Start();
    });
    
    std::this_thread::sleep_for(100ms);
    
    // Connect multiple clients
    std::vector<boost::asio::ip::tcp::socket> clients;
    for (int i = 0; i < NUM_CLIENTS; ++i) {
        clients.emplace_back(*client_io);
        boost::asio::ip::tcp::endpoint endpoint(
            boost::asio::ip::address::from_string("127.0.0.1"), 
            test_port
        );
        clients.back().connect(endpoint);
    }
    
    std::this_thread::sleep_for(100ms);
    
    // Broadcast message
    BroadcastPacket broadcast;
    broadcast.set_message("Hello everyone!");
    
    Packet packet;
    packet.set_type(PacketType::BROADCAST);
    packet.mutable_body()->PackFrom(broadcast);
    
    server->Broadcast(packet);
    
    // All clients should receive the message
    for (auto& client : clients) {
        uint32_t size;
        boost::asio::read(client, boost::asio::buffer(&size, sizeof(size)));
        size = ntohl(size);
        
        std::vector<char> buffer(size);
        boost::asio::read(client, boost::asio::buffer(buffer));
        
        Packet received;
        EXPECT_TRUE(received.ParseFromArray(buffer.data(), size));
        EXPECT_EQ(received.type(), PacketType::BROADCAST);
        
        BroadcastPacket broadcast_msg;
        EXPECT_TRUE(received.body().UnpackTo(&broadcast_msg));
        EXPECT_EQ(broadcast_msg.message(), "Hello everyone!");
    }
    
    // Cleanup
    for (auto& client : clients) {
        client.close();
    }
    
    server->Stop();
    server_thread.join();
}

private:
    mutable std::mutex clients_mutex;
};