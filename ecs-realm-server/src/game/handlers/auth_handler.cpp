#include "game/handlers/auth_handler.h"
#include "core/network/session.h"
#include <spdlog/spdlog.h>
#include "../../core/security/security_manager.h"

// [SEQUENCE: MVP1-62] Game server authentication handler implementation.

namespace mmorpg::game::handlers {

void AuthHandler::HandleLoginRequest(core::network::SessionPtr session, 
                                    const mmorpg::proto::Packet& packet) {
    mmorpg::proto::LoginRequest request;
    if (!request.ParseFromString(packet.payload())) {
        spdlog::error("Failed to parse LoginRequest from session {}", session->GetSessionId());
        return;
    }
    
    spdlog::info("Login request from {} for user {}", 
                 session->GetRemoteAddress(), request.username());
    
    if (!core::security::SecurityManager::Instance().ValidateLoginAttempt(session->GetRemoteAddress())) {
        spdlog::warn("Login rate limit exceeded for IP: {}", session->GetRemoteAddress());
        
        mmorpg::proto::LoginResponse response;
        response.set_success(false);
        response.set_error_message("Too many login attempts. Please try again later.");
        
        mmorpg::proto::Packet response_packet;
        response_packet.set_type(mmorpg::proto::PACKET_LOGIN_RESPONSE);
        response_packet.set_payload(response.SerializeAsString());
        
        session->SendPacket(response_packet);
        return;
    }
    
    auto result = auth_service_->Authenticate(
        request.username(), 
        request.password_hash(),
        session->GetRemoteAddress()
    );
    
    mmorpg::proto::LoginResponse response;
    response.set_success(result.success);
    
    if (result.success) {
        response.set_session_token(result.access_token);
        response.set_player_id(result.player_id);
        
        if (session->Authenticate(result.access_token)) {
            spdlog::info("Session {} authenticated for player {}", 
                        session->GetSessionId(), result.player_id);
        }
        
        // TODO: Add server list
    } else {
        response.set_error_code(mmorpg::proto::ERROR_INVALID_CREDENTIALS);
        response.set_error_message(result.error_message);
    }
    
    session->SendPacket(mmorpg::proto::PACKET_LOGIN_RESPONSE, response);
}

void AuthHandler::HandleLogoutRequest(core::network::SessionPtr session, 
                                     const mmorpg::proto::Packet& packet) {
    if (!session->IsAuthenticated()) {
        spdlog::warn("Logout request from unauthenticated session {}", session->GetSessionId());
        return;
    }
    
    mmorpg::proto::LogoutRequest request;
    if (!request.ParseFromString(packet.payload())) {
        spdlog::error("Failed to parse LogoutRequest");
        return;
    }
    
    auth_service_->Logout(session->GetJwtToken());
    
    mmorpg::proto::LogoutResponse response;
    response.set_success(true);
    
    session->SendPacket(mmorpg::proto::PACKET_LOGOUT_RESPONSE, response);
    
    session->Disconnect();
}

void AuthHandler::HandleHeartbeatRequest(core::network::SessionPtr session, 
                                        const mmorpg::proto::Packet& packet) {
    mmorpg::proto::HeartbeatRequest request;
    if (!request.ParseFromString(packet.payload())) {
        return;
    }
    
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    auto latency_ns = now - request.timestamp();
    auto latency_ms = latency_ns / 1'000'000;
    
    mmorpg::proto::HeartbeatResponse response;
    response.set_server_timestamp(now);
    response.set_latency_ms(static_cast<uint32_t>(latency_ms));
    
    session->SendPacket(mmorpg::proto::PACKET_HEARTBEAT_RESPONSE, response);
}

} // namespace mmorpg::game::handlers