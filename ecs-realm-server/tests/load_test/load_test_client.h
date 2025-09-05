#pragma once

#include <boost/asio/ssl.hpp>
#include <string>
#include <vector>
#include <atomic>
#include <memory>
#include <boost/asio.hpp>
#include <deque>

#include "network/packet_serializer.h"
#include "proto/auth.pb.h"
#include "proto/game.pb.h"

namespace mmorpg::tests {

// [SEQUENCE: MVP9-6] Defines the configuration and metrics for the load test.
class LoadTestClient {
public:
    struct Config {
        std::string host = "127.0.0.1";
        uint16_t port = 8080;
        uint32_t num_clients = 100;
        uint32_t test_duration_sec = 30;
        uint32_t packets_per_sec = 5;
    };

    struct Metrics {
        std::atomic<uint32_t> connections_succeeded = 0;
        std::atomic<uint32_t> connections_failed = 0;
        std::atomic<uint64_t> packets_sent = 0;
        std::atomic<uint64_t> packets_received = 0;
    };

    explicit LoadTestClient(Config config);
    ~LoadTestClient();

    void Run();

private:
    // [SEQUENCE: MVP9-7] Represents a single client session in the load test.
    class ClientSession : public std::enable_shared_from_this<ClientSession> {
    public:
        ClientSession(boost::asio::io_context& io_context, boost::asio::ssl::context& ssl_context, const Config& config, Metrics& metrics);
        void Start();
    private:
        // Connection Logic
        void Connect();
        void OnResolve(const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::results_type endpoints);
        void OnConnect(const boost::system::error_code& ec);
        void Handshake();
        void OnHandshake(const boost::system::error_code& ec);
        void Login();

        // Read/Write Logic
        void DoReadHeader();
        void OnReadHeader(const boost::system::error_code& ec, size_t bytes);
        void DoReadBody(uint32_t body_size);
        void OnReadBody(const boost::system::error_code& ec, size_t bytes);
        void ProcessPacket(std::vector<std::byte>&& buffer);
        void DoWrite();
        void OnWrite(const boost::system::error_code& ec, size_t bytes);
        void Send(const google::protobuf::Message& message);

        // Gameplay Loop
        void SendMovementLoop(const boost::system::error_code& ec);

        using SslSocket = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;
        boost::asio::io_context& m_io_context;
        SslSocket m_tcp_socket;
        boost::asio::ip::tcp::resolver m_resolver;
        boost::asio::steady_timer m_timer;

        std::vector<std::byte> m_read_buffer;
        std::deque<std::vector<std::byte>> m_write_queue;

        const Config& m_config;
        Metrics& m_metrics;
        uint64_t m_player_id = 0;
    };

    void PrintResults();

    Config m_config;
    Metrics m_metrics;
    boost::asio::io_context m_io_context;
    boost::asio::ssl::context m_ssl_context;
    std::vector<std::thread> m_threads;
    std::vector<std::shared_ptr<ClientSession>> m_clients;
};

} // namespace mmorpg::tests