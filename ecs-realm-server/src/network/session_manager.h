#pragma once

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

// [SEQUENCE: MVP1-19] Manages all active client sessions
class SessionManager {
public:
    SessionManager() = default;
    ~SessionManager() = default;

    // Managers are typically non-copyable and non-movable
    SessionManager(const SessionManager&) = delete;
    SessionManager& operator=(const SessionManager&) = delete;
    SessionManager(SessionManager&&) = delete;
    SessionManager& operator=(SessionManager&&) = delete;

    // [SEQUENCE: MVP1-20] `SessionManager::Register()`: 새로운 세션을 등록합니다.
    // [SEQUENCE: MVP1-33] Register a new session
    void Register(const std::shared_ptr<Session>& session);

    // [SEQUENCE: MVP1-21] `SessionManager::Unregister()`: 세션을 등록 해제합니다.
    // [SEQUENCE: MVP1-34] Unregister a session
    void Unregister(uint32_t session_id);

    // [SEQUENCE: MVP1-22] `SessionManager::GetSession()`: 특정 ID의 세션을 찾습니다.
    // [SEQUENCE: MVP1-35] Get a session by its ID
    std::shared_ptr<Session> GetSession(uint32_t session_id) const;

    // [SEQUENCE: MVP1-23] `SessionManager::Broadcast()`: 모든 세션에 패킷을 전송합니다.
    // [SEQUENCE: MVP1-36] Broadcast a packet to all sessions
    void Broadcast(const google::protobuf::Message& message);

    // [SEQUENCE: MVP1-37] Send a packet to a specific player/session
    void SendToSession(uint32_t session_id, const google::protobuf::Message& message);

    // [SEQUENCE: MVP1-38] Get the number of active sessions
    size_t GetSessionCount() const;

    // [SEQUENCE: MVP5-24] The SessionManager was updated to track the mapping between session IDs and player IDs, enabling handlers to identify the player associated with a session.
    void SetPlayerIdForSession(uint32_t session_id, uint64_t player_id);
    uint64_t GetPlayerIdForSession(uint32_t session_id) const;

private:
    mutable std::shared_mutex m_mutex;
    std::unordered_map<uint32_t, std::shared_ptr<Session>> m_sessions;
    std::unordered_map<uint32_t, uint64_t> m_session_to_player_id;
};

}
