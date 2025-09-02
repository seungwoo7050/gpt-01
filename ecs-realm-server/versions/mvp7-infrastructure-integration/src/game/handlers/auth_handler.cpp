#include "game/handlers/auth_handler.h"
#include "core/network/session.h"
#include <spdlog/spdlog.h>

namespace mmorpg::game::handlers {

// [SEQUENCE: 393] Login request handler implementation
void AuthHandler::HandleLoginRequest(core::network::SessionPtr session, 
                                    const mmorpg::proto::Packet& packet) {
    // [SEQUENCE: 394] Parse login request
    mmorpg::proto::LoginRequest request;
    if (!request.ParseFromString(packet.payload())) {
        spdlog::error("Failed to parse LoginRequest from session {}", session->GetSessionId());
        return;
    }
    
    spdlog::info("Login request from {} for user {}", 
                 session->GetRemoteAddress(), request.username());
    
    // [SEQUENCE: 395] Authenticate user
    auto result = auth_service_->Authenticate(
        request.username(), 
        request.password_hash(),
        session->GetRemoteAddress()
    );
    
    // [SEQUENCE: 396] Build response
    mmorpg::proto::LoginResponse response;
    response.set_success(result.success);
    
    if (result.success) {
        // [SEQUENCE: 397] Set JWT token in response
        response.set_session_token(result.access_token);
        response.set_player_id(result.player_id);
        
        // [SEQUENCE: 398] Authenticate session with JWT
        if (session->Authenticate(result.access_token)) {
            spdlog::info("Session {} authenticated for player {}", 
                        session->GetSessionId(), result.player_id);
        }
        
        // TODO: Add server list
    } else {
        response.set_error_code(mmorpg::proto::ERROR_INVALID_CREDENTIALS);
        response.set_error_message(result.error_message);
    }
    
    // [SEQUENCE: 399] Send response
    session->SendPacket(mmorpg::proto::PACKET_LOGIN_RESPONSE, response);
}

// [SEQUENCE: 400] Logout request handler
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
    
    // [SEQUENCE: 401] Logout from auth service
    auth_service_->Logout(session->GetJwtToken());
    
    // [SEQUENCE: 402] Build and send response
    mmorpg::proto::LogoutResponse response;
    response.set_success(true);
    
    session->SendPacket(mmorpg::proto::PACKET_LOGOUT_RESPONSE, response);
    
    // [SEQUENCE: 403] Disconnect session
    session->Disconnect();
}

// [SEQUENCE: 404] Heartbeat handler
void AuthHandler::HandleHeartbeatRequest(core::network::SessionPtr session, 
                                        const mmorpg::proto::Packet& packet) {
    mmorpg::proto::HeartbeatRequest request;
    if (!request.ParseFromString(packet.payload())) {
        return;
    }
    
    // [SEQUENCE: 405] Calculate latency
    auto now = std::chrono::system_clock::now().time_since_epoch().count();
    auto latency_ns = now - request.timestamp();
    auto latency_ms = latency_ns / 1'000'000;
    
    // [SEQUENCE: 406] Send response
    mmorpg::proto::HeartbeatResponse response;
    response.set_server_timestamp(now);
    response.set_latency_ms(static_cast<uint32_t>(latency_ms));
    
    session->SendPacket(mmorpg::proto::PACKET_HEARTBEAT_RESPONSE, response);
}

} // namespace mmorpg::game::handlers