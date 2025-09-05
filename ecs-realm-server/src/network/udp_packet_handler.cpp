#include "network/udp_packet_handler.h"
#include "network/session_manager.h"
#include "network/session.h"
#include "proto/udp_handshake.pb.h"
#include "core/logger.h"

#include <iostream>

namespace mmorpg::network {

// [SEQUENCE: MVP6-35] Constructor: Initializes the handler with a reference to the SessionManager.
UdpPacketHandler::UdpPacketHandler(SessionManager& session_manager)
    : m_session_manager(session_manager) {
}

// [SEQUENCE: MVP6-36] Handles an incoming raw UDP packet.
// Its primary responsibility is to process the initial handshake packet.
void UdpPacketHandler::Handle(std::shared_ptr<Session> session, const boost::asio::ip::udp::endpoint& endpoint, const std::vector<std::byte>& buffer, size_t size) {
    // First, try to parse the packet as a UdpHandshake message.
    mmorpg::proto::UdpHandshake handshake;
    if (handshake.ParseFromArray(buffer.data(), size)) {
        // It's a valid handshake packet. Find the corresponding TCP session via the player_id.
        auto player_id = handshake.player_id();
        auto session_to_register = m_session_manager.GetSessionByPlayerId(player_id);
        if (session_to_register) {
            // If the session is found, register the UDP endpoint with it.
            m_session_manager.RegisterUdpEndpoint(session_to_register->GetSessionId(), endpoint);
            LOG_INFO("[UdpPacketHandler] UDP handshake processed for player {}", player_id);
        }
        return;
    }

    // If it's not a handshake, it must be a gameplay packet from an already-registered endpoint.
    if (!session) {
        // A non-handshake packet from an unknown source is ignored for security.
        return;
    }

    // TODO: Handle other gameplay packets (e.g., movement) based on a packet type identifier.
}

}
