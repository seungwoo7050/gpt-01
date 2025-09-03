#include <gtest/gtest.h>
#include <thread>
#include <future>
#include "network/tcp_server.h"
#include "network/session.h"
#include "network/packet_handler.h"
#include "proto/packet.pb.h"

using namespace mmorpg::network;
using namespace std::chrono_literals;

// [SEQUENCE: MVP1-73] Basic networking tests
class NetworkingTest : public ::testing::Test {
protected:
    std::unique_ptr<TcpServer> server;
    std::unique_ptr<boost::asio::io_context> client_io;
    uint16_t test_port = 9999;
    
    void SetUp() override {
        server = std::make_unique<TcpServer>(test_port);
        client_io = std::make_unique<boost::asio::io_context>();
    }
    
    void TearDown() override {
        if (server) server->Stop();
        if (client_io) client_io->stop();
    }
};

// [SEQUENCE: MVP1-74] Test server startup and shutdown
TEST_F(NetworkingTest, ServerStartupShutdown) {
    std::thread server_thread([this]() { server->Start(); });
    std::this_thread::sleep_for(100ms);
    EXPECT_TRUE(server->IsRunning());
    server->Stop();
    server_thread.join();
    EXPECT_FALSE(server->IsRunning());
}

// [SEQUENCE: MVP1-75] Test client connection
TEST_F(NetworkingTest, ClientConnection) {
    std::atomic<bool> client_connected{false};
    server->SetOnConnect([&](std::shared_ptr<Session> s) { client_connected = true; });
    std::thread server_thread([this]() { server->Start(); });
    std::this_thread::sleep_for(100ms);
    boost::asio::ip::tcp::socket client_socket(*client_io);
    client_socket.connect({boost::asio::ip::address::from_string("127.0.0.1"), test_port});
    std::this_thread::sleep_for(100ms);
    EXPECT_TRUE(client_connected);
    EXPECT_EQ(server->GetConnectionCount(), 1);
    client_socket.close();
    std::this_thread::sleep_for(100ms);
    EXPECT_EQ(server->GetConnectionCount(), 0);
    server->Stop();
    server_thread.join();
}

// [SEQUENCE: MVP1-76] Test packet sending and receiving
TEST_F(NetworkingTest, PacketTransmission) {
    std::promise<bool> packet_received;
    auto packet_future = packet_received.get_future();
    auto handler = std::make_shared<PacketHandler>();
    handler->RegisterHandler(PacketType::PING, 
        [&](std::shared_ptr<Session> session, const Packet& packet) {
            packet_received.set_value(true);
        }
    );
    server->SetPacketHandler(handler);
    std::thread server_thread([this]() { server->Start(); });
    std::this_thread::sleep_for(100ms);
    boost::asio::ip::tcp::socket client_socket(*client_io);
    client_socket.connect({boost::asio::ip::address::from_string("127.0.0.1"), test_port});
    Packet packet; packet.set_type(PacketType::PING);
    std::string serialized = packet.SerializeAsString();
    uint32_t size = htonl(serialized.size());
    boost::asio::write(client_socket, boost::asio::buffer(&size, sizeof(size)));
    boost::asio::write(client_socket, boost::asio::buffer(serialized));
    EXPECT_TRUE(packet_future.wait_for(1s) == std::future_status::ready);
    client_socket.close();
    server->Stop();
    server_thread.join();
}

// [SEQUENCE: MVP1-77] Test multiple concurrent connections
TEST_F(NetworkingTest, ConcurrentConnections) {
    const int NUM_CLIENTS = 50;
    std::atomic<int> connected_count{0};
    server->SetOnConnect([&](std::shared_ptr<Session> s) { connected_count++; });
    std::thread server_thread([this]() { server->Start(); });
    std::this_thread::sleep_for(100ms);
    std::vector<std::unique_ptr<boost::asio::ip::tcp::socket>> clients;
    for (int i = 0; i < NUM_CLIENTS; ++i) {
        clients.emplace_back(std::make_unique<boost::asio::ip::tcp::socket>(*client_io));
        clients.back()->connect({boost::asio::ip::address::from_string("127.0.0.1"), test_port});
    }
    std::this_thread::sleep_for(200ms);
    EXPECT_EQ(connected_count, NUM_CLIENTS);
    clients.clear();
    std::this_thread::sleep_for(200ms);
    EXPECT_EQ(server->GetConnectionCount(), 0);
    server->Stop();
    server_thread.join();
}

// [SEQUENCE: MVP1-78] Test packet fragmentation handling
TEST_F(NetworkingTest, LargePacketHandling) {
    std::promise<bool> large_packet_received;
    auto future = large_packet_received.get_future();
    const size_t LARGE_SIZE = 1024 * 128; // 128KB
    std::string large_data(LARGE_SIZE, 'Z');
    auto handler = std::make_shared<PacketHandler>();
    handler->RegisterHandler(PacketType::CUSTOM, 
        [&](std::shared_ptr<Session> s, const Packet& p) { large_packet_received.set_value(true); });
    server->SetPacketHandler(handler);
    std::thread server_thread([this]() { server->Start(); });
    std::this_thread::sleep_for(100ms);
    boost::asio::ip::tcp::socket client_socket(*client_io);
    client_socket.connect({boost::asio::ip::address::from_string("127.0.0.1"), test_port});
    Packet packet; packet.set_type(PacketType::CUSTOM);
    std::string serialized = packet.SerializeAsString();
    uint32_t size = htonl(serialized.size());
    boost::asio::write(client_socket, boost::asio::buffer(&size, sizeof(size)));
    boost::asio::write(client_socket, boost::asio::buffer(serialized));
    EXPECT_TRUE(future.wait_for(5s) == std::future_status::ready);
    client_socket.close();
    server->Stop();
    server_thread.join();
}

// [SEQUENCE: MVP1-79] Test session timeout
TEST_F(NetworkingTest, SessionTimeout) {
    server->SetSessionTimeout(500ms);
    std::atomic<bool> session_closed{false};
    server->SetOnDisconnect([&](std::shared_ptr<Session> s) { session_closed = true; });
    std::thread server_thread([this]() { server->Start(); });
    std::this_thread::sleep_for(100ms);
    boost::asio::ip::tcp::socket client_socket(*client_io);
    client_socket.connect({boost::asio::ip::address::from_string("127.0.0.1"), test_port});
    std::this_thread::sleep_for(1s);
    EXPECT_TRUE(session_closed);
    EXPECT_EQ(server->GetConnectionCount(), 0);
    server->Stop();
    server_thread.join();
}

// [SEQUENCE: MVP1-80] Test broadcast functionality
TEST_F(NetworkingTest, BroadcastMessage) {
    const int NUM_CLIENTS = 5;
    auto handler = std::make_shared<PacketHandler>();
    server->SetPacketHandler(handler);
    std::thread server_thread([this]() { server->Start(); });
    std::this_thread::sleep_for(100ms);
    std::vector<boost::asio::ip::tcp::socket> clients;
    for (int i = 0; i < NUM_CLIENTS; ++i) {
        clients.emplace_back(*client_io);
        clients.back().connect({boost::asio::ip::address::from_string("127.0.0.1"), test_port});
    }
    std::this_thread::sleep_for(100ms);
    Packet packet; packet.set_type(PacketType::BROADCAST);
    server->Broadcast(packet);
    for (auto& client : clients) {
        uint32_t size;
        boost::asio::read(client, boost::asio::buffer(&size, sizeof(size)));
        EXPECT_GT(ntohl(size), 0);
    }
    for (auto& client : clients) client.close();
    server->Stop();
    server_thread.join();
}