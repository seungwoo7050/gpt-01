#include "server/login/auth_handler.h"
#include "core/network/session.h"
#include "core/network/packet_serializer.h"
#include <spdlog/spdlog.h>
#include <random>
#include <sstream>
#include <iomanip>

namespace mmorpg::server::login {

// [SEQUENCE: 1] Constructor - create test accounts for MVP1
AuthHandler::AuthHandler() {
    // Create some test accounts
    CreatePlayer("test1", "password1");
    CreatePlayer("test2", "password2");
    CreatePlayer("admin", "adminpass");
    
    spdlog::info("AuthHandler initialized with {} test accounts", players_by_username_.size());
}

// [SEQUENCE: 2] Register packet handlers
void AuthHandler::RegisterHandlers(std::shared_ptr<mmorpg::core::network::IPacketHandler> packet_handler) {
    using namespace mmorpg::proto;
    
    packet_handler->RegisterHandler(PACKET_LOGIN_REQUEST,
        [this](auto session, auto& packet) { HandleLoginRequest(session, packet); });
        
    packet_handler->RegisterHandler(PACKET_LOGOUT_REQUEST,
        [this](auto session, auto& packet) { HandleLogoutRequest(session, packet); });
        
    packet_handler->RegisterHandler(PACKET_HEARTBEAT_REQUEST,
        [this](auto session, auto& packet) { HandleHeartbeat(session, packet); });
}

// [SEQUENCE: 3] Handle login request
void AuthHandler::HandleLoginRequest(mmorpg::core::network::SessionPtr session, const mmorpg::proto::Packet& packet) {
    mmorpg::proto::LoginRequest request;
    if (!mmorpg::core::network::PacketSerializer::ExtractMessage(packet, request)) {
        spdlog::error("Failed to parse LoginRequest from session {}", session->GetSessionId());
        session->Disconnect();
        return;
    }
    
    spdlog::info("Login request from session {} for user '{}'", 
                 session->GetSessionId(), request.username());
    
    mmorpg::proto::LoginResponse response;
    
    // Validate credentials
    if (ValidateCredentials(request.username(), request.password_hash())) {
        auto* player = GetPlayerByUsername(request.username());
        
        if (player->is_banned) {
            response.set_success(false);
            response.set_error_code(mmorpg::proto::ERROR_BANNED);
            response.set_error_message("Account is banned");
        } else {
            // Generate session token
            std::string token = GenerateSessionToken();
            
            {
                std::lock_guard<std::mutex> lock(sessions_mutex_);
                sessions_[token] = player->player_id;
            }
            
            response.set_success(true);
            response.set_error_code(mmorpg::proto::ERROR_NONE);
            response.set_session_token(token);
            response.set_player_id(player->player_id);
            
            // Add available game servers (hardcoded for MVP1)
            auto* server_info = response.add_game_servers();
            server_info->set_server_id(1);
            server_info->set_server_name("Game Server 1");
            server_info->set_ip_address("127.0.0.1");
            server_info->set_port(8081);
            server_info->set_current_players(150);
            server_info->set_max_players(5000);
            server_info->set_load_percentage(3.0f);
            
            // Update session state
            session->SetPlayerId(player->player_id);
            session->SetState(mmorpg::core::network::SessionState::AUTHENTICATED);
            
            spdlog::info("User '{}' (ID: {}) logged in successfully", 
                        request.username(), player->player_id);
        }
    } else {
        response.set_success(false);
        response.set_error_code(mmorpg::proto::ERROR_INVALID_CREDENTIALS);
        response.set_error_message("Invalid username or password");
        
        spdlog::warn("Failed login attempt for user '{}'", request.username());
    }
    
    // Send response
    session->SendPacket(mmorpg::proto::PACKET_LOGIN_RESPONSE, response);
}

// [SEQUENCE: 4] Handle logout request
void AuthHandler::HandleLogoutRequest(mmorpg::core::network::SessionPtr session, const mmorpg::proto::Packet& packet) {
    mmorpg::proto::LogoutRequest request;
    if (!mmorpg::core::network::PacketSerializer::ExtractMessage(packet, request)) {
        spdlog::error("Failed to parse LogoutRequest from session {}", session->GetSessionId());
        return;
    }
    
    mmorpg::proto::LogoutResponse response;
    
    // Remove session token
    {
        std::lock_guard<std::mutex> lock(sessions_mutex_);
        auto it = std::find_if(sessions_.begin(), sessions_.end(),
            [&](const auto& pair) { return pair.second == request.player_id(); });
        
        if (it != sessions_.end()) {
            sessions_.erase(it);
            response.set_success(true);
            response.set_error_code(mmorpg::proto::ERROR_NONE);
            
            spdlog::info("Player {} logged out successfully", request.player_id());
        } else {
            response.set_success(false);
            response.set_error_code(mmorpg::proto::ERROR_INVALID_PACKET);
        }
    }
    
    // Send response and disconnect
    session->SendPacket(mmorpg::proto::PACKET_LOGOUT_RESPONSE, response);
    session->SetState(mmorpg::core::network::SessionState::DISCONNECTING);
    session->Disconnect();
}

// [SEQUENCE: 5] Handle heartbeat
void AuthHandler::HandleHeartbeat(mmorpg::core::network::SessionPtr session, const mmorpg::proto::Packet& packet) {
    mmorpg::proto::HeartbeatRequest request;
    if (!mmorpg::core::network::PacketSerializer::ExtractMessage(packet, request)) {
        return;
    }
    
    mmorpg::proto::HeartbeatResponse response;
    response.set_server_timestamp(std::chrono::system_clock::now().time_since_epoch().count());
    
    // Calculate latency (simplified)
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    uint32_t latency_ms = static_cast<uint32_t>((now - request.timestamp()) / 1000000);
    response.set_latency_ms(latency_ms);
    
    session->SendPacket(mmorpg::proto::PACKET_HEARTBEAT_RESPONSE, response);
}

// [SEQUENCE: 6] Create player (in-memory for MVP1)
bool AuthHandler::CreatePlayer(const std::string& username, const std::string& password_hash) {
    std::lock_guard<std::mutex> lock(players_mutex_);
    
    if (players_by_username_.find(username) != players_by_username_.end()) {
        return false; // Username already exists
    }
    
    PlayerData player;
    player.player_id = next_player_id_.fetch_add(1);
    player.username = username;
    player.password_hash = password_hash;
    player.level = 1;
    player.is_banned = false;
    
    players_by_username_[username] = player;
    usernames_by_id_[player.player_id] = username;
    
    return true;
}

// [SEQUENCE: 7] Get player by username
PlayerData* AuthHandler::GetPlayerByUsername(const std::string& username) {
    std::lock_guard<std::mutex> lock(players_mutex_);
    auto it = players_by_username_.find(username);
    return (it != players_by_username_.end()) ? &it->second : nullptr;
}

// [SEQUENCE: 8] Validate credentials
bool AuthHandler::ValidateCredentials(const std::string& username, const std::string& password_hash) {
    auto* player = GetPlayerByUsername(username);
    return player && player->password_hash == password_hash;
}

// [SEQUENCE: 9] Generate random session token
std::string AuthHandler::GenerateSessionToken() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 255);
    
    std::stringstream ss;
    for (int i = 0; i < 32; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << dis(gen);
    }
    
    return ss.str();
}

} // namespace mmorpg::server::login