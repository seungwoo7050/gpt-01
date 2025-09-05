#pragma once

#include <boost/asio/ip/udp.hpp>
#include <boost/functional/hash.hpp>
#include <memory>
#include <unordered_map>
#include <shared_mutex>
#include <cstdint>

// Forward declarations
namespace google::protobuf {
class Message;
}
namespace mmorpg::network {
class Session;
}

namespace mmorpg::network {

// [SEQUENCE: MVP6-19] A custom hasher is required to use boost::asio::ip::udp::endpoint as a key in an unordered_map.
struct UdpEndpointHasher {
    std::size_t operator()(const boost::asio::ip::udp::endpoint& endpoint) const {
        std::size_t seed = 0;
        boost::hash_combine(seed, endpoint.address().to_v4().to_uint());
        boost::hash_combine(seed, endpoint.port());
        return seed;
    }
};

class SessionManager {
public:
    SessionManager() : m_next_session_id(0) {}
    ~SessionManager() = default;

    uint32_t get_next_session_id() {
        return ++m_next_session_id;
    }

    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;
    SessionManager(SessionManager&&) = delete;
    SessionManager& operator=(SessionManager&&) = delete;

    void Register(const std::shared_ptr<Session>& session);
    void Unregister(uint32_t session_id);
    std::shared_ptr<Session> GetSession(uint32_t session_id) const;
    void Broadcast(const google::protobuf::Message& message);
    void SendToSession(uint32_t session_id, const google::protobuf::Message& message);
    size_t GetSessionCount() const;

    void SetPlayerIdForSession(uint32_t session_id, uint64_t player_id);
    uint64_t GetPlayerIdForSession(uint32_t session_id) const;
    std::shared_ptr<Session> GetSessionByPlayerId(uint64_t player_id) const;

    // [SEQUENCE: MVP6-21] Methods for UDP endpoint management.
    void RegisterUdpEndpoint(uint32_t session_id, const boost::asio::ip::udp::endpoint& endpoint);
    std::shared_ptr<Session> GetSessionByUdpEndpoint(const boost::asio::ip::udp::endpoint& endpoint) const;

private:
    std::atomic<uint32_t> m_next_session_id;
    mutable std::shared_mutex m_mutex;
    std::unordered_map<uint32_t, std::shared_ptr<Session>> m_sessions;
    std::unordered_map<uint32_t, uint64_t> m_session_to_player_id;

    // [SEQUENCE: MVP6-20] A map from a UDP endpoint to a session ID enables quick O(1) lookup of the session associated with an incoming UDP packet.
    std::unordered_map<boost::asio::ip::udp::endpoint, uint32_t, UdpEndpointHasher> m_udp_endpoint_to_session_id;
};

}
