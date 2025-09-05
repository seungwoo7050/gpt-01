#pragma once

#include <boost/asio/ip/udp.hpp>
#include <memory>
#include <vector>
#include <cstddef>

// Forward declarations
namespace mmorpg::network {
class Session;
}

namespace mmorpg::network {

// [SEQUENCE: MVP6-28] Defines an interface for handling raw UDP packets.
// This decouples the UdpServer from the specific logic of how packets are processed.
class IUdpPacketHandler {
public:
    virtual ~IUdpPacketHandler() = default;
    virtual void Handle(std::shared_ptr<Session> session, const boost::asio::ip::udp::endpoint& endpoint, const std::vector<std::byte>& buffer, size_t size) = 0;
};

}