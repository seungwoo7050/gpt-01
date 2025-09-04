#pragma once

#include <boost/asio/ssl.hpp>
#include <string>
#include <vector>
#include <atomic>
#include <memory>
#include <boost/asio.hpp>

namespace mmorpg::tests {

class LoadTestClient {
public:
    struct Config {
        std::string host = "127.0.0.1";
        uint16_t port = 8080;
        uint16_t udp_port = 8081;
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
    class ClientSession : public std::enable_shared_from_this<ClientSession> {
    public:
        ClientSession(boost::asio::io_context& io_context, boost::asio::ssl::context& ssl_context, const Config& config, Metrics& metrics);
        void Start();
    private:
        void DoReadHeader();
        void DoReadBody(uint32_t body_size);
        void Connect();
        void Handshake();
        void Login();
        void StartUdp();
        void SendMovementLoop(const boost::system::error_code& ec);

        using SslSocket = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;
        boost::asio::io_context& m_io_context;
        SslSocket m_tcp_socket;
        boost::asio::ip::tcp::resolver m_resolver;
        boost::asio::ip::udp::socket m_udp_socket;
        boost::asio::steady_timer m_timer;

        std::vector<std::byte> m_read_buffer;
        std::vector<std::byte> m_write_buffer;
        uint32_t m_packet_size = 0;
        uint32_t m_packet_type = 0;

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
