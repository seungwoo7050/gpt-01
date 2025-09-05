#include "network/session_manager.h"
#include "network/session.h"

#include <google/protobuf/message.h>
#include <shared_mutex>

namespace mmorpg::network {

void SessionManager::Register(const std::shared_ptr<Session>& session) {
    if (!session) return;
    std::unique_lock lock(m_mutex);
    m_sessions[session->GetSessionId()] = session;
}

// [SEQUENCE: MVP6-24] When a session is unregistered, its UDP endpoint mapping must also be removed to prevent stale entries.
void SessionManager::Unregister(uint32_t session_id) {
    std::unique_lock lock(m_mutex);
    auto it = m_sessions.find(session_id);
    if (it != m_sessions.end()) {
        if (auto udp_endpoint = it->second->GetUdpEndpoint(); udp_endpoint) {
            m_udp_endpoint_to_session_id.erase(*udp_endpoint);
        }
        m_sessions.erase(it);
    }
    m_session_to_player_id.erase(session_id);
}

std::shared_ptr<Session> SessionManager::GetSession(uint32_t session_id) const {
    std::shared_lock lock(m_mutex);
    auto it = m_sessions.find(session_id);
    return (it != m_sessions.end()) ? it->second : nullptr;
}

void SessionManager::Broadcast(const google::protobuf::Message& message) {
    std::shared_lock lock(m_mutex);
    for (const auto& [id, session] : m_sessions) {
        if (session && session->GetState() == SessionState::Connected && session->IsAuthenticated()) {
            session->Send(message);
        }
    }
}

void SessionManager::SendToSession(uint32_t session_id, const google::protobuf::Message& message) {
    auto session = GetSession(session_id);
    if (session && session->GetState() == SessionState::Connected) {
        session->Send(message);
    }
}

size_t SessionManager::GetSessionCount() const {
    std::shared_lock lock(m_mutex);
    return m_sessions.size();
}

void SessionManager::SetPlayerIdForSession(uint32_t session_id, uint64_t player_id) {
    std::unique_lock lock(m_mutex);
    m_session_to_player_id[session_id] = player_id;
}

uint64_t SessionManager::GetPlayerIdForSession(uint32_t session_id) const {
    std::shared_lock lock(m_mutex);
    auto it = m_session_to_player_id.find(session_id);
    return (it != m_session_to_player_id.end()) ? it->second : 0;
}

std::shared_ptr<Session> SessionManager::GetSessionByPlayerId(uint64_t player_id) const {
    std::shared_lock lock(m_mutex);
    for (const auto& pair : m_session_to_player_id) {
        if (pair.second == player_id) {
            return GetSession(pair.first);
        }
    }
    return nullptr;
}

// [SEQUENCE: MVP6-22] Associates a UDP endpoint with a session ID after a successful handshake.
void SessionManager::RegisterUdpEndpoint(uint32_t session_id, const boost::asio::ip::udp::endpoint& endpoint) {
    std::unique_lock lock(m_mutex);
    auto it = m_sessions.find(session_id);
    if (it != m_sessions.end()) {
        it->second->SetUdpEndpoint(endpoint);
        m_udp_endpoint_to_session_id[endpoint] = session_id;
    }
}

// [SEQUENCE: MVP6-23] Finds a session based on its UDP endpoint.
std::shared_ptr<Session> SessionManager::GetSessionByUdpEndpoint(const boost::asio::ip::udp::endpoint& endpoint) const {
    std::shared_lock lock(m_mutex);
    auto it = m_udp_endpoint_to_session_id.find(endpoint);
    if (it != m_udp_endpoint_to_session_id.end()) {
        return GetSession(it->second);
    }
    return nullptr;
}

}