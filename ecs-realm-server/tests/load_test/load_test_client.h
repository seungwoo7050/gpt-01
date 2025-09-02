#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <memory>
#include <boost/asio.hpp>

// [SEQUENCE: MVP6.5-9] Code Components
namespace mmorpg::tests {

class LoadTestClient {
public:
    struct Config {
        std::string host = "127.0.0.1";
        uint16_t port = 12345;
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
    class ClientSession;

    void PrintResults();

    Config _config;
    Metrics _metrics;
    boost::asio::io_context _io_context;
    std::vector<std::thread> _threads;
    std::vector<std::shared_ptr<ClientSession>> _clients;
};

} // namespace mmorpg::tests
