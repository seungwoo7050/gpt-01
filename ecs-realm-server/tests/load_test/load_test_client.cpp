#include "load_test_client.h"
#include "core/network/packet_serializer.h"
#include "proto/packet.pb.h"
#include "proto/auth.pb.h"
#include <iostream>
#include <boost/asio/steady_timer.hpp>

namespace mmorpg::tests {

// Private implementation of a single client connection
class LoadTestClient::ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    ClientSession(boost::asio::io_context& io_context, LoadTestClient* parent)
        : _socket(io_context), _timer(io_context), _parent(parent) {}

    void Connect(const std::string& host, uint16_t port) {
        auto endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::address::from_string(host), port);
        _socket.async_connect(endpoint, [self = shared_from_this()](const boost::system::error_code& ec) {
            if (!ec) {
                self->_parent->_metrics.connections_succeeded++;
                // For simplicity, we assume connection means logged in and start sending packets.
                self->ScheduleSend();
            } else {
                self->_parent->_metrics.connections_failed++;
            }
        });
    }

    void ScheduleSend() {
        _timer.expires_after(std::chrono::milliseconds(1000 / _parent->_config.packets_per_sec));
        _timer.async_wait([self = shared_from_this()](const boost::system::error_code& ec) {
            if (!ec) {
                self->SendPacket();
                self->ScheduleSend();
            }
        });
    }

    void SendPacket() {
        // Create a dummy packet to send. In a real test, this would be more varied.
        proto::Packet packet;
        packet.set_type(proto::PacketType::HEARTBEAT_REQUEST);
        
        auto buffer = core::network::PacketSerializer::SerializeWithHeader(packet);
        boost::asio::async_write(_socket, boost::asio::buffer(buffer), 
            [self = shared_from_this()](const boost::system::error_code& ec, size_t bytes_transferred) {
            if (!ec) {
                self->_parent->_metrics.packets_sent++;
            }
        });
    }

    void Stop() {
        boost::system::error_code ec;
        _timer.cancel(ec);
        _socket.close(ec);
    }

private:
    boost::asio::ip::tcp::socket _socket;
    boost::asio::steady_timer _timer;
    LoadTestClient* _parent;
};

// LoadTestClient implementation
LoadTestClient::LoadTestClient(Config config) : _config(config) {}

LoadTestClient::~LoadTestClient() {
    for (auto& thread : _threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void LoadTestClient::Run() {
    std::cout << "Starting load test..." << std::endl;
    std::cout << "Clients: " << _config.num_clients << ", Duration: " << _config.test_duration_sec << "s" << std::endl;

    // Start IO context threads
    auto work_guard = boost::asio::make_work_guard(_io_context);
    unsigned int num_threads = std::thread::hardware_concurrency();
    for (unsigned int i = 0; i < num_threads; ++i) {
        _threads.emplace_back([this]() { _io_context.run(); });
    }

    // Create and connect clients
    for (uint32_t i = 0; i < _config.num_clients; ++i) {
        auto client = std::make_shared<ClientSession>(_io_context, this);
        client->Connect(_config.host, _config.port);
        _clients.push_back(client);
    }

    // Wait for the test duration
    std::this_thread::sleep_for(std::chrono::seconds(_config.test_duration_sec));

    // Stop clients and IO context
    std::cout << "\nStopping load test..." << std::endl;
    for (auto& client : _clients) {
        client->Stop();
    }
    _io_context.stop();

    PrintResults();
}

void LoadTestClient::PrintResults() {
    std::cout << "\n--- Load Test Results ---" << std::endl;
    std::cout << "Connections Succeeded: " << _metrics.connections_succeeded << std::endl;
    std::cout << "Connections Failed:    " << _metrics.connections_failed << std::endl;
    std::cout << "Total Packets Sent:    " << _metrics.packets_sent << std::endl;
    double pps = _metrics.packets_sent.load() / (double)_config.test_duration_sec;
    std::cout << "Average PPS:           " << std::fixed << std::setprecision(2) << pps << std::endl;
    std::cout << "-------------------------" << std::endl;
}

} // namespace mmorpg::tests