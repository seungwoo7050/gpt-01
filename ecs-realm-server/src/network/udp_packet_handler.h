#pragma once

#include "network/i_udp_packet_handler.h"

// Forward declarations
namespace mmorpg::network {
class SessionManager;
}

namespace mmorpg::network {

// [SEQUENCE: MVP6-34] The concrete implementation of the UDP packet handler.
class UdpPacketHandler : public IUdpPacketHandler {
public:
    explicit UdpPacketHandler(SessionManager& session_manager);

    void Handle(std::shared_ptr<Session> session, const boost::asio::ip::udp::endpoint& endpoint, const std::vector<std::byte>& buffer, size_t size) override;

private:
    SessionManager& m_session_manager;
};

}