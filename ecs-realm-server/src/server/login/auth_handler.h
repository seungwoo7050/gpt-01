#pragma once

#include <memory>
#include <unordered_map>
#include <mutex>
#include "core/network/packet_handler.h"
#include "proto/auth.pb.h"

namespace mmorpg::server::login {

// [SEQUENCE: 1] Simple player data for MVP1 (will be replaced with database later)
struct PlayerData {
    uint64_t player_id;
    std::string username;
    std::string password_hash;
    uint32_t level = 1;
    bool is_banned = false;
};

// [SEQUENCE: 2] Authentication handler for login/logout operations
class AuthHandler {
public:
    AuthHandler();
    ~AuthHandler() = default;
    
    // Register handlers with packet handler
    void RegisterHandlers(std::shared_ptr<mmorpg::core::network::IPacketHandler> packet_handler);
    
    // Handler methods
    void HandleLoginRequest(mmorpg::core::network::SessionPtr session, const mmorpg::proto::Packet& packet);
    void HandleLogoutRequest(mmorpg::core::network::SessionPtr session, const mmorpg::proto::Packet& packet);
    void HandleHeartbeat(mmorpg::core::network::SessionPtr session, const mmorpg::proto::Packet& packet);
    
    // Player management (temporary in-memory storage for MVP1)
    bool CreatePlayer(const std::string& username, const std::string& password_hash);
    PlayerData* GetPlayerByUsername(const std::string& username);
    
private:
    // Validate login credentials
    bool ValidateCredentials(const std::string& username, const std::string& password_hash);
    std::string GenerateSessionToken();
    
    // In-memory player storage (MVP1 only)
    std::mutex players_mutex_;
    std::unordered_map<std::string, PlayerData> players_by_username_;
    std::unordered_map<uint64_t, std::string> usernames_by_id_;
    
    // Session tokens
    std::mutex sessions_mutex_;
    std::unordered_map<std::string, uint64_t> sessions_; // token -> player_id
    
    // ID generator
    std::atomic<uint64_t> next_player_id_{1000};
};

} // namespace mmorpg::server::login