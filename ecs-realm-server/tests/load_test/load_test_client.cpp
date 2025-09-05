#include "load_test_client.h"
#include <spdlog/spdlog.h>
#include <iostream>
#include <thread>

// [SEQUENCE: MVP9-8] SSL Info Callback for debugging handshake issues.
void ssl_info_callback(const SSL *, [[maybe_unused]] int where, int ret) {
    if (ret == 0) {
        spdlog::error("ssl_info_callback: error occurred.");
        return;
    }
    // spdlog::debug("[SSL_INFO] Client State: {}", SSL_state_string_long(ssl));
}

namespace mmorpg::tests {

// --- LoadTestClient Implementation ---

LoadTestClient::LoadTestClient(Config config)
    : m_config(std::move(config)),
      m_ssl_context(boost::asio::ssl::context::sslv23_client) {
    m_ssl_context.set_verify_mode(boost::asio::ssl::verify_none);
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

    size_t num_threads = std::thread::hardware_concurrency();
    for (size_t i = 0; i < num_threads; ++i) {
        m_threads.emplace_back([this]() {
            m_io_context.run();
        });
    }

    spdlog::info("Load test running...");
    std::this_thread::sleep_for(std::chrono::seconds(m_config.test_duration_sec));

    spdlog::info("Stopping load test...");
    m_io_context.stop();
    
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
      m_resolver(io_context),
      m_timer(io_context),
      m_config(config),
      m_metrics(metrics) {
}

void LoadTestClient::ClientSession::Start() {
    Connect();
}

// [SEQUENCE: MVP9-9] Starts the connection process by resolving the server endpoint.
void LoadTestClient::ClientSession::Connect() {
    m_resolver.async_resolve(m_config.host, std::to_string(m_config.port),
        std::bind(&ClientSession::OnResolve, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
}

void LoadTestClient::ClientSession::OnResolve(const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::results_type endpoints) {
    if (ec) {
        spdlog::error("Resolve failed: {}", ec.message());
        m_metrics.connections_failed++;
        return;
    }
    boost::asio::async_connect(m_tcp_socket.lowest_layer(), endpoints,
        std::bind(&ClientSession::OnConnect, shared_from_this(), std::placeholders::_1));
}

// [SEQUENCE: MVP9-10] Once connected, performs the SSL handshake.
void LoadTestClient::ClientSession::OnConnect(const boost::system::error_code& ec) {
    if (ec) {
        spdlog::error("Connect failed: {}", ec.message());
        m_metrics.connections_failed++;
        return;
    }
    Handshake();
}

void LoadTestClient::ClientSession::Handshake() {
    m_tcp_socket.async_handshake(boost::asio::ssl::stream_base::client,
        std::bind(&ClientSession::OnHandshake, shared_from_this(), std::placeholders::_1));
}

// [SEQUENCE: MVP9-11] After a successful handshake, sends a login request.
void LoadTestClient::ClientSession::OnHandshake(const boost::system::error_code& ec) {
    if (ec) {
        spdlog::error("Handshake failed: {}", ec.message());
        m_metrics.connections_failed++;
        return;
    }
    m_metrics.connections_succeeded++;
    Login();
}

void LoadTestClient::ClientSession::Login() {
    proto::LoginRequest req;
    req.set_username("test_user");
    req.set_password_hash("password");
    Send(req);
    DoReadHeader(); // Start reading responses from the server
}

// [SEQUENCE: MVP9-12] Implements the asynchronous read loop to process server messages.
void LoadTestClient::ClientSession::DoReadHeader() {
    m_read_buffer.resize(4);
    boost::asio::async_read(m_tcp_socket, boost::asio::buffer(m_read_buffer),
        std::bind(&ClientSession::OnReadHeader, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
}

void LoadTestClient::ClientSession::OnReadHeader(const boost::system::error_code& ec, size_t) {
    if (ec) {
        return;
    }
    uint32_t body_size = 0;
    memcpy(&body_size, m_read_buffer.data(), sizeof(uint32_t));
    body_size = ntohl(body_size);
    DoReadBody(body_size);
}

void LoadTestClient::ClientSession::DoReadBody(uint32_t body_size) {
    m_read_buffer.resize(body_size);
    boost::asio::async_read(m_tcp_socket, boost::asio::buffer(m_read_buffer),
        std::bind(&ClientSession::OnReadBody, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
}

void LoadTestClient::ClientSession::OnReadBody(const boost::system::error_code& ec, size_t) {
    if (ec) {
        return;
    }
    m_metrics.packets_received++;
    ProcessPacket(std::move(m_read_buffer));
    DoReadHeader(); // Continue reading the next packet
}

void LoadTestClient::ClientSession::ProcessPacket(std::vector<std::byte>&& data) {
    auto packet = network::PacketSerializer::Deserialize(data.data(), data.size());
    if (!packet) return;

    if (packet->header().type() == proto::PACKET_LOGIN_RESPONSE) {
        proto::LoginResponse resp;
        if (resp.ParseFromString(packet->payload())) {
            m_player_id = resp.player_id();
            SendMovementLoop({}); // Start sending movement packets
        }
    }
}

// [SEQUENCE: MVP9-13] Implements the asynchronous write loop.
void LoadTestClient::ClientSession::DoWrite() {
    boost::asio::async_write(m_tcp_socket, boost::asio::buffer(m_write_queue.front()),
        std::bind(&ClientSession::OnWrite, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
}

void LoadTestClient::ClientSession::OnWrite(const boost::system::error_code& ec, size_t) {
    if (ec) {
        return;
    }
    m_write_queue.pop_front();
    if (!m_write_queue.empty()) {
        DoWrite();
    }
}

void LoadTestClient::ClientSession::Send(const google::protobuf::Message& message) {
    auto buffer = network::PacketSerializer::Serialize(message);
    if (buffer.empty()) return;

    bool write_in_progress = !m_write_queue.empty();
    m_write_queue.push_back(std::move(buffer));
    if (!write_in_progress) {
        DoWrite();
    }
    m_metrics.packets_sent++;
}

// [SEQUENCE: MVP9-14] A timer-based loop to periodically send movement packets.
void LoadTestClient::ClientSession::SendMovementLoop(const boost::system::error_code& ec) {
    if (ec) return;

    proto::MovementUpdate update;
    update.set_entity_id(m_player_id);
    update.mutable_position()->set_x(1.0f);
    update.mutable_position()->set_y(2.0f);
    update.mutable_position()->set_z(3.0f);
    Send(update);

    m_timer.expires_at(m_timer.expiry() + std::chrono::milliseconds(1000 / m_config.packets_per_sec));
    m_timer.async_wait(std::bind(&ClientSession::SendMovementLoop, shared_from_this(), std::placeholders::_1));
}

} // namespace mmorpg::tests
