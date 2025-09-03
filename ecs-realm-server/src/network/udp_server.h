#pragma once

#include "network/i_udp_packet_handler.h" // [SEQUENCE: MVP6-34] Use IUdpPacketHandler interface
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

// [SEQUENCE: MVP6-13] A UDP server to handle real-time, unreliable data.
class UdpServer {
public:
    // [SEQUENCE: MVP6-22] Modified constructor for self-contained operation.
    UdpServer(uint16_t port, SessionManager& session_manager);
    ~UdpServer();

    bool Start();
    void Stop();

    // [SEQUENCE: MVP6-34] Use IUdpPacketHandler interface
    void SetPacketHandler(std::shared_ptr<IUdpPacketHandler> handler) { m_packetHandler = std::move(handler); }

private:
    void DoReceive();
    void HandleReceive(const boost::system::error_code& ec, std::size_t bytes_recvd);

    // [SEQUENCE: MVP6-23] UdpServer now owns its io_context and runs it in a dedicated thread.
    std::unique_ptr<boost::asio::io_context> m_io_context;
    std::thread m_thread;
    udp::socket m_socket;
    uint16_t m_port;

    udp::endpoint m_remote_endpoint;
    std::vector<std::byte> m_recv_buffer;

    SessionManager& m_session_manager;
    // [SEQUENCE: MVP6-34] Use IUdpPacketHandler interface
    std::shared_ptr<IUdpPacketHandler> m_packetHandler;
};

} // namespace mmorpg::network
