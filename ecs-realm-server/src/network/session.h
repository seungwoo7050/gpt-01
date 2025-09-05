#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ip/udp.hpp>
#include <memory>
#include <vector>
#include <deque>
#include <atomic>
#include <string>
#include <cstdint>
#include <optional>

#include "proto/packet.pb.h"

// Forward declarations
namespace google::protobuf {
class Message;
}
namespace mmorpg::network {
class IPacketHandler;
}

namespace mmorpg::network {

using boost::asio::ip::tcp;
using boost::asio::ip::udp;

enum class SessionState {
    Connecting,
    Handshake,
    Connected,
    Disconnected
};

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, boost::asio::ssl::context& context, uint32_t session_id, std::shared_ptr<IPacketHandler> handler);
    ~Session();

    void Start();
    void Disconnect();
    void Send(const google::protobuf::Message& message);

    tcp::socket& GetSocket() { return m_ssl_stream.next_layer(); }
    uint32_t GetSessionId() const { return m_sessionId; }
    SessionState GetState() const { return m_state; }
    bool IsAuthenticated() const { return m_isAuthenticated; }
    std::string GetRemoteAddress() const;
    void SetAuthenticated(bool authenticated) { m_isAuthenticated = authenticated; }
    void Authenticate();
    void SetPlayerId(uint64_t player_id);

    // [SEQUENCE: MVP6-26] Methods for UDP endpoint management within the session.
    void SetUdpEndpoint(const udp::endpoint& endpoint);
    std::optional<udp::endpoint> GetUdpEndpoint() const;

private:
    void DoHandshake();
    void DoReadHeader();
    void DoReadBody(uint32_t body_size);
    void ProcessPacket(std::vector<std::byte>&& buffer);
    void DoWrite();
    void HandleError(const boost::system::error_code& ec);

    boost::asio::ssl::stream<tcp::socket> m_ssl_stream;
    boost::asio::strand<boost::asio::any_io_executor> m_strand;
    std::atomic<SessionState> m_state;
    const uint32_t m_sessionId;
    std::shared_ptr<IPacketHandler> m_packetHandler;

    std::vector<std::byte> m_readBuffer;
    std::deque<std::vector<std::byte>> m_writeQueue;

    std::atomic<bool> m_isAuthenticated;
    uint64_t m_player_id = 0;

    // [SEQUENCE: MVP6-25] Stores the associated UDP endpoint for this session after a successful handshake.
    std::optional<udp::endpoint> m_udp_endpoint;
};

}
