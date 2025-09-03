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

// [SEQUENCE: MVP6-15] Custom hasher for udp::endpoint to use it in unordered_map.
struct UdpEndpointHasher {
    std::size_t operator()(const boost::asio::ip::udp::endpoint& endpoint) const {
        std::size_t seed = 0;
        boost::hash_combine(seed, endpoint.address().to_v4().to_uint());
        boost::hash_combine(seed, endpoint.port());
        return seed;
    }
};

// [SEQUENCE: MVP1-19] Manages all active client sessions
class SessionManager {
public:
    SessionManager() = default;
    ~SessionManager() = default;

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

    // [SEQUENCE: MVP6-16] Methods for UDP endpoint management.
    void RegisterUdpEndpoint(uint32_t session_id, const boost::asio::ip::udp::endpoint& endpoint);
    std::shared_ptr<Session> GetSessionByUdpEndpoint(const boost::asio::ip::udp::endpoint& endpoint) const;

private:
    mutable std::shared_mutex m_mutex;
    std::unordered_map<uint32_t, std::shared_ptr<Session>> m_sessions;
    std::unordered_map<uint32_t, uint64_t> m_session_to_player_id;

    // [SEQUENCE: MVP6-17] Map from UDP endpoint to session ID for fast lookups.
    std::unordered_map<boost::asio::ip::udp::endpoint, uint32_t, UdpEndpointHasher> m_udp_endpoint_to_session_id;
};

}
