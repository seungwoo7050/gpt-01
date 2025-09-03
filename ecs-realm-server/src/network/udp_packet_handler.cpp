#include "network/udp_packet_handler.h"
#include "network/session_manager.h"
#include "network/session.h"
#include "proto/udp_handshake.pb.h"
#include <iostream>

namespace mmorpg::network {

// [SEQUENCE: MVP6-33] Implementation of the UdpPacketHandler.
UdpPacketHandler::UdpPacketHandler(SessionManager& session_manager)
    : m_session_manager(session_manager) {
}

void UdpPacketHandler::Handle(std::shared_ptr<Session> session, const boost::asio::ip::udp::endpoint& endpoint, const std::vector<std::byte>& buffer, size_t size) {
    // A session is passed if the endpoint is already known.
    // If the session is null, this might be a new client sending a handshake.

    // First, try to handle it as a handshake packet, regardless of whether the session is known.
    mmorpg::proto::UdpHandshake handshake;
    if (handshake.ParseFromArray(buffer.data(), size)) {
        // It's a handshake. Register the endpoint.
        // This will associate the endpoint with the session_id provided in the handshake.
        m_session_manager.RegisterUdpEndpoint(handshake.session_id(), endpoint);
        std::cout << "[UdpPacketHandler] UDP handshake processed for session " << handshake.session_id() 
                  << " from " << endpoint << std::endl;
        return;
    }

    // If it's not a handshake, it must be a gameplay packet from an already-registered endpoint.
    if (!session) {
        // We received a non-handshake packet from an unknown endpoint. Ignore it for security.
        return;
    }

    // TODO: Handle other gameplay packets (e.g., movement) based on a packet type identifier in the buffer.
    // Example:
    // PacketType packet_type = static_cast<PacketType>(buffer[0]);
    // switch (packet_type) {
    //     case PacketType::PlayerMovement:
    //         // process movement
    //         break;
    // }
}

}
