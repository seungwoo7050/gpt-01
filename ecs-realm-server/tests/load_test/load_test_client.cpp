#include "load_test_client.h"
#include <spdlog/spdlog.h>
#include <iostream>
#include <thread>

// [SEQUENCE: MVP9-27] SSL Info Callback for debugging handshake issues.
void ssl_info_callback(const SSL *ssl, [[maybe_unused]] int where, int ret) {
    if (ret == 0) {
        spdlog::error("ssl_info_callback: error occurred.");
        return;
    }
    spdlog::debug("[SSL_INFO] Client State: {}", SSL_state_string_long(ssl));
}

namespace mmorpg::tests {

// --- LoadTestClient Implementation ---

LoadTestClient::LoadTestClient(Config config)
    : m_config(std::move(config)),
      m_ssl_context(boost::asio::ssl::context::sslv23) {
    // For simplicity, this client trusts any certificate from the server.
    m_ssl_context.set_verify_mode(boost::asio::ssl::verify_none);

    // [SEQUENCE: MVP9-28] Set the SSL info callback for detailed logging.
    SSL_CTX_set_info_callback(m_ssl_context.native_handle(), ssl_info_callback);

    for (uint32_t i = 0; i < m_config.num_clients; ++i) {
        m_clients.push_back(std::make_shared<ClientSession>(m_io_context, m_ssl_context, m_config, m_metrics));
    }
}

LoadTestClient::~LoadTestClient() {
    if (!m_io_context.stopped()) {
        m_io_context.stop();
    }
    for (auto& t : m_threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}

void LoadTestClient::Run() {
    spdlog::info("Starting load test with {} clients for {} seconds...", m_config.num_clients, m_config.test_duration_sec);

    for (const auto& client : m_clients) {
        client->Start();
    }

    // Create a thread pool to run the io_context
    size_t num_threads = std::thread::hardware_concurrency();
    for (size_t i = 0; i < num_threads; ++i) {
        m_threads.emplace_back([this]() {
            m_io_context.run();
        });
    }

    // Let the test run for the specified duration
    spdlog::info("Load test running...");
    std::this_thread::sleep_for(std::chrono::seconds(m_config.test_duration_sec));

    spdlog::info("Stopping load test...");
    m_io_context.stop();

    for (auto& t : m_threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    
    PrintResults();
}

void LoadTestClient::PrintResults() {
    spdlog::info("--- Load Test Results ---");
    spdlog::info("Successful connections: {}", m_metrics.connections_succeeded.load());
    spdlog::info("Failed connections:     {}", m_metrics.connections_failed.load());
    spdlog::info("Packets sent:           {}", m_metrics.packets_sent.load());
    spdlog::info("Packets received:       {}", m_metrics.packets_received.load());
    double pps = m_config.test_duration_sec > 0 ? static_cast<double>(m_metrics.packets_sent.load()) / m_config.test_duration_sec : 0.0;
    spdlog::info("Average packets per second (sent): {:.2f}", pps);
}


// --- ClientSession Implementation ---

LoadTestClient::ClientSession::ClientSession(boost::asio::io_context& io_context, boost::asio::ssl::context& ssl_context, const Config& config, Metrics& metrics)
    : m_io_context(io_context),
      m_tcp_socket(io_context, ssl_context),
      m_udp_socket(io_context),
      m_timer(io_context),
      m_config(config),
      m_metrics(metrics) {
}

void LoadTestClient::ClientSession::Start() {
    Connect();
}

void LoadTestClient::ClientSession::Connect() {
    // TODO: Implement async connect and the rest of the client logic
    spdlog::debug("Client connecting...");
    // For now, we'll just increment the success metric for demonstration
    m_metrics.connections_succeeded++;
}

void LoadTestClient::ClientSession::Handshake() {
    // TODO: Implement async SSL handshake
}

void LoadTestClient::ClientSession::Login() {
    // TODO: Implement login packet sending
}

void LoadTestClient::ClientSession::StartUdp() {
    // TODO: Implement UDP handshake
}

void LoadTestClient::ClientSession::SendMovementLoop([[maybe_unused]] const boost::system::error_code& ec) {
    // TODO: Implement movement packet sending loop
}

} // namespace mmorpg::tests
