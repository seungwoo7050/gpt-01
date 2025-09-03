#include "network/session_manager.h"
#include "network/session.h"

#include <google/protobuf/message.h>
#include <shared_mutex>

namespace mmorpg::network {

// [SEQUENCE: MVP1-20] `SessionManager::Register()`: 새로운 세션을 등록합니다.
void SessionManager::Register(const std::shared_ptr<Session>& session) {
    if (!session) return;
    std::unique_lock lock(m_mutex);
    m_sessions[session->GetSessionId()] = session;
}

// [SEQUENCE: MVP1-21] `SessionManager::Unregister()`: 세션을 등록 해제합니다.
void SessionManager::Unregister(uint32_t session_id) {
    std::unique_lock lock(m_mutex);
    m_sessions.erase(session_id);
}

// [SEQUENCE: MVP1-22] `SessionManager::GetSession()`: 특정 ID의 세션을 찾습니다.
std::shared_ptr<Session> SessionManager::GetSession(uint32_t session_id) const {
    std::shared_lock lock(m_mutex);
    auto it = m_sessions.find(session_id);
    return (it != m_sessions.end()) ? it->second : nullptr;
}

// [SEQUENCE: MVP1-23] `SessionManager::Broadcast()`: 모든 세션에 패킷을 전송합니다.
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

// [SEQUENCE: MVP5-24] The SessionManager was updated to track the mapping between session IDs and player IDs, enabling handlers to identify the player associated with a session.
void SessionManager::SetPlayerIdForSession(uint32_t session_id, uint64_t player_id) {
    std::unique_lock lock(m_mutex);
    m_session_to_player_id[session_id] = player_id;
}

// [SEQUENCE: MVP5-24] The SessionManager was updated to track the mapping between session IDs and player IDs, enabling handlers to identify the player associated with a session.
uint64_t SessionManager::GetPlayerIdForSession(uint32_t session_id) const {
    std::shared_lock lock(m_mutex);
    auto it = m_session_to_player_id.find(session_id);
    return (it != m_session_to_player_id.end()) ? it->second : 0;
}

}
