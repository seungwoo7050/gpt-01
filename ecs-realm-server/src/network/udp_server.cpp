#include "network/udp_server.h"
#include "network/session_manager.h"
#include "network/i_udp_packet_handler.h"
#include "core/logger.h"

#include <iostream>

namespace mmorpg::network {

// [SEQUENCE: MVP6-31] Constructor: Initializes the UDP server components.
UdpServer::UdpServer(uint16_t port, SessionManager& session_manager)
    : m_io_context(std::make_unique<boost::asio::io_context>()),
      m_socket(*m_io_context),
      m_port(port),
      m_session_manager(session_manager) {
    m_recv_buffer.resize(65507); // Max UDP packet size
}

UdpServer::~UdpServer() {
    Stop();
}

// [SEQUENCE: MVP6-32] Starts the UDP server, opens the socket, and runs the io_context in a new thread.
bool UdpServer::Start() {
    try {
        udp::endpoint endpoint(udp::v4(), m_port);
        m_socket.open(endpoint.protocol());
        m_socket.bind(endpoint);
    } catch (const boost::system::system_error& e) {
        LOG_ERROR("[UdpServer] Error starting server: {}", e.what());
        return false;
    }
    
    LOG_INFO("[UdpServer] Started on port {}", m_port);
    DoReceive();

    m_thread = std::thread([this]() {
        m_io_context->run();
    });

    return true;
}

void UdpServer::Stop() {
    if (m_io_context && !m_io_context->stopped()) {
        m_io_context->stop();
    }
    if (m_thread.joinable()) {
        m_thread.join();
    }
    boost::system::error_code ec;
    if (m_socket.is_open()) {
        m_socket.close(ec);
    }
}

// [SEQUENCE: MVP6-33] The core asynchronous receive loop and its handler.
// It waits for a packet, and upon receipt, delegates processing to the registered IUdpPacketHandler.
void UdpServer::DoReceive() {
    m_socket.async_receive_from(
        boost::asio::buffer(m_recv_buffer),
        m_remote_endpoint,
        [this](const boost::system::error_code& ec, std::size_t bytes_recvd) {
            HandleReceive(ec, bytes_recvd);
        });
}

void UdpServer::HandleReceive(const boost::system::error_code& ec, std::size_t bytes_recvd) {
    if (!ec && bytes_recvd > 0) {
        if (m_packetHandler) {
            // Look up the session associated with the incoming endpoint.
            auto session = m_session_manager.GetSessionByUdpEndpoint(m_remote_endpoint);
            // The handler is responsible for dealing with both known (session != nullptr) and unknown endpoints (e.g., for handshakes).
            m_packetHandler->Handle(session, m_remote_endpoint, m_recv_buffer, bytes_recvd);
        }
    }

    if (m_socket.is_open()) {
        DoReceive();
    }
}

} // namespace mmorpg::network
