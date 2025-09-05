#pragma once

#include "network/i_udp_packet_handler.h"
#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include <thread>
#include <cstdint>
#include <string>

// Forward declarations
namespace mmorpg::network {
class SessionManager;
}

namespace mmorpg::network {

using boost::asio::ip::udp;

// [SEQUENCE: MVP6-29] A UDP server to handle real-time, unreliable data like player movement.
// It runs in its own thread with its own io_context to avoid interfering with the main TCP server.
class UdpServer {
public:
    UdpServer(uint16_t port, SessionManager& session_manager);
    ~UdpServer();

    bool Start();
    void Stop();

    // [SEQUENCE: MVP6-30] Sets the packet handler that will process incoming UDP data.
    void SetPacketHandler(std::shared_ptr<IUdpPacketHandler> handler) { m_packetHandler = std::move(handler); }

private:
    void DoReceive();
    void HandleReceive(const boost::system::error_code& ec, std::size_t bytes_recvd);

    std::unique_ptr<boost::asio::io_context> m_io_context;
    std::thread m_thread;
    udp::socket m_socket;
    uint16_t m_port;

    udp::endpoint m_remote_endpoint;
    std::vector<std::byte> m_recv_buffer;

    SessionManager& m_session_manager;
    std::shared_ptr<IUdpPacketHandler> m_packetHandler;
};

} // namespace mmorpg::network