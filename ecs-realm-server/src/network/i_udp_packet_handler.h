#pragma once

#include <boost/asio/ip/udp.hpp> // [SEQUENCE: MVP6-38] Include full header for udp::endpoint
#include <memory>
#include <vector>
#include <cstddef>

// Forward declarations
namespace mmorpg::network {
class Session;
}

namespace mmorpg::network {

// [SEQUENCE: MVP6-31] Interface for handling UDP packets.
class IUdpPacketHandler {
public:
    virtual ~IUdpPacketHandler() = default;
    virtual void Handle(std::shared_ptr<Session> session, const boost::asio::ip::udp::endpoint& endpoint, const std::vector<std::byte>& buffer, size_t size) = 0;
};

}
