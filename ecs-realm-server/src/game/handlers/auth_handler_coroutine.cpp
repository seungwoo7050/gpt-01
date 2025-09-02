#include "auth_handler_coroutine.h"
#include "../../core/security/security_manager.h"
#include <spdlog/spdlog.h>
#include <boost/asio/experimental/awaitable_operators.hpp>

namespace mmorpg::game::handlers {

using namespace boost::asio::experimental::awaitable_operators;

// [SEQUENCE: 514] Async login request handler - no more callback hell!
boost::asio::awaitable<void> CoroutineAuthHandler::HandleLoginRequest(
    std::shared_ptr<mmorpg::core::network::CoroutineSession> session, 
    const mmorpg::proto::Packet& packet) {
    
    try {
        // [SEQUENCE: 515] Parse login request
        mmorpg::proto::LoginRequest request;
        if (!request.ParseFromString(packet.payload())) {
            spdlog::error("Failed to parse LoginRequest from session {}", session->GetSessionId());
            co_await SendLoginResponse(session, false, "Invalid request format");
            co_return;
        }
        
        spdlog::info("Async login request from {} for user {}", 
                     session->GetRemoteAddress(), request.username());
        
        // [SEQUENCE: 516] Rate limiting check (async)
        bool rate_limit_ok = co_await CheckRateLimitAsync(session->GetRemoteAddress());
        if (!rate_limit_ok) {
            spdlog::warn("Login rate limit exceeded for IP: {}", session->GetRemoteAddress());
            co_await SendLoginResponse(session, false, 
                "Too many login attempts. Please try again later.");
            co_return;
        }
        
        // [SEQUENCE: 517] Authenticate user asynchronously
        // This is where the magic happens - database calls are now async!
        auto auth_result = co_await AuthenticateUserAsync(
            request.username(), 
            request.password_hash(),
            session->GetRemoteAddress()
        );
        
        // [SEQUENCE: 518] Handle authentication result
        if (auth_result.success) {
            session->SetPlayerId(auth_result.player_id);
            session->SetAccessToken(auth_result.access_token);
            session->SetState(mmorpg::core::network::SessionState::AUTHENTICATED);
            
            // [SEQUENCE: 519] Update last login asynchronously
            co_await UpdateLastLoginAsync(auth_result.player_id);
            
            // [SEQUENCE: 520] Log successful authentication
            co_await LogAuthenticationAttempt(request.username(), 
                                            session->GetRemoteAddress(), true);
            
            spdlog::info("Player {} authenticated successfully", request.username());
            
            co_await SendLoginResponse(session, true, "", 
                                     auth_result.access_token, auth_result.player_id);
        } else {
            // [SEQUENCE: 521] Log failed authentication
            co_await LogAuthenticationAttempt(request.username(), 
                                            session->GetRemoteAddress(), false);
            
            co_await SendLoginResponse(session, false, auth_result.error_message);
        }
        
    } catch (const std::exception& e) {
        spdlog::error("Exception in async login handler: {}", e.what());
        co_await SendLoginResponse(session, false, "Internal server error");
    }
}

// [SEQUENCE: 522] Async logout handler
boost::asio::awaitable<void> CoroutineAuthHandler::HandleLogoutRequest(
    std::shared_ptr<mmorpg::core::network::CoroutineSession> session,
    const mmorpg::proto::Packet& packet) {
    
    try {
        spdlog::info("Logout request from session {}", session->GetSessionId());
        
        // [SEQUENCE: 523] Validate session asynchronously
        bool session_valid = co_await ValidateSessionAsync(
            std::to_string(session->GetSessionId())
        );
        
        if (session_valid) {
            // [SEQUENCE: 524] Perform logout operations
            session->SetState(mmorpg::core::network::SessionState::CONNECTED);
            session->SetAccessToken("");
            
            spdlog::info("Player {} logged out successfully", session->GetPlayerId());
            co_await SendLogoutResponse(session, true);
        } else {
            co_await SendLogoutResponse(session, false);
        }
        
    } catch (const std::exception& e) {
        spdlog::error("Exception in async logout handler: {}", e.what());
        co_await SendLogoutResponse(session, false);
    }
}

// [SEQUENCE: 525] Async heartbeat handler
boost::asio::awaitable<void> CoroutineAuthHandler::HandleHeartbeatRequest(
    std::shared_ptr<mmorpg::core::network::CoroutineSession> session,
    const mmorpg::proto::Packet& packet) {
    
    try {
        // [SEQUENCE: 526] Simple heartbeat response
        co_await SendHeartbeatResponse(session);
        
        spdlog::debug("Heartbeat from session {}", session->GetSessionId());
        
    } catch (const std::exception& e) {
        spdlog::error("Exception in async heartbeat handler: {}", e.what());
    }
}

// [SEQUENCE: 527] Async authentication with database
boost::asio::awaitable<mmorpg::auth::AuthResult> 
CoroutineAuthHandler::AuthenticateUserAsync(const std::string& username, 
                                           const std::string& password_hash,
                                           const std::string& ip_address) {
    
    // [SEQUENCE: 528] Create a timer to simulate async database operation
    // In real implementation, this would be actual async database calls
    auto executor = co_await boost::asio::this_coro::executor;
    auto timer = boost::asio::steady_timer(executor);
    timer.expires_after(std::chrono::milliseconds(10)); // Simulate DB latency
    co_await timer.async_wait(boost::asio::use_awaitable);
    
    // [SEQUENCE: 529] Call synchronous auth service (for now)
    // TODO: Make AuthService fully async in future phases
    auto result = auth_service_->Authenticate(username, password_hash, ip_address);
    
    co_return result;
}

// [SEQUENCE: 530] Async session validation
boost::asio::awaitable<bool> CoroutineAuthHandler::ValidateSessionAsync(
    const std::string& session_id) {
    
    // [SEQUENCE: 531] Simulate async session validation
    auto executor = co_await boost::asio::this_coro::executor;
    auto timer = boost::asio::steady_timer(executor);
    timer.expires_after(std::chrono::milliseconds(5));
    co_await timer.async_wait(boost::asio::use_awaitable);
    
    // For now, assume all sessions are valid
    // TODO: Implement actual session validation
    co_return true;
}

// [SEQUENCE: 532] Async rate limit checking
boost::asio::awaitable<bool> CoroutineAuthHandler::CheckRateLimitAsync(
    const std::string& ip_address) {
    
    // [SEQUENCE: 533] Rate limit check (already fast, but made async for consistency)
    auto executor = co_await boost::asio::this_coro::executor;
    auto timer = boost::asio::steady_timer(executor);
    timer.expires_after(std::chrono::milliseconds(1));
    co_await timer.async_wait(boost::asio::use_awaitable);
    
    bool allowed = mmorpg::core::security::SecurityManager::Instance()
        .ValidateLoginAttempt(ip_address);
    
    co_return allowed;
}

// [SEQUENCE: 534] Async login response sending
boost::asio::awaitable<void> CoroutineAuthHandler::SendLoginResponse(
    std::shared_ptr<mmorpg::core::network::CoroutineSession> session,
    bool success, 
    const std::string& error_message,
    const std::string& access_token,
    uint64_t player_id) {
    
    // [SEQUENCE: 535] Build response
    mmorpg::proto::LoginResponse response;
    response.set_success(success);
    
    if (!success) {
        response.set_error_message(error_message);
    } else {
        response.set_access_token(access_token);
        response.set_player_id(player_id);
        response.set_server_time(std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    }
    
    // [SEQUENCE: 536] Send response asynchronously
    co_await session->SendPacketAsync(mmorpg::proto::PACKET_LOGIN_RESPONSE, response);
}

// [SEQUENCE: 537] Async logout response
boost::asio::awaitable<void> CoroutineAuthHandler::SendLogoutResponse(
    std::shared_ptr<mmorpg::core::network::CoroutineSession> session,
    bool success) {
    
    mmorpg::proto::LogoutResponse response;
    response.set_success(success);
    
    co_await session->SendPacketAsync(mmorpg::proto::PACKET_LOGOUT_RESPONSE, response);
}

// [SEQUENCE: 538] Async heartbeat response
boost::asio::awaitable<void> CoroutineAuthHandler::SendHeartbeatResponse(
    std::shared_ptr<mmorpg::core::network::CoroutineSession> session) {
    
    mmorpg::proto::HeartbeatResponse response;
    response.set_server_time(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    
    co_await session->SendPacketAsync(mmorpg::proto::PACKET_HEARTBEAT_RESPONSE, response);
}

// [SEQUENCE: 539] Async authentication logging
boost::asio::awaitable<void> CoroutineAuthHandler::LogAuthenticationAttempt(
    const std::string& username, 
    const std::string& ip_address,
    bool success) {
    
    // [SEQUENCE: 540] Simulate async logging
    auto executor = co_await boost::asio::this_coro::executor;
    auto timer = boost::asio::steady_timer(executor);
    timer.expires_after(std::chrono::milliseconds(2));
    co_await timer.async_wait(boost::asio::use_awaitable);
    
    if (success) {
        spdlog::info("Authentication success: {} from {}", username, ip_address);
    } else {
        spdlog::warn("Authentication failed: {} from {}", username, ip_address);
    }
    
    co_return;
}

// [SEQUENCE: 541] Async last login update
boost::asio::awaitable<void> CoroutineAuthHandler::UpdateLastLoginAsync(uint64_t player_id) {
    
    // [SEQUENCE: 542] Simulate async database update
    auto executor = co_await boost::asio::this_coro::executor;
    auto timer = boost::asio::steady_timer(executor);
    timer.expires_after(std::chrono::milliseconds(15)); // Simulate DB write latency
    co_await timer.async_wait(boost::asio::use_awaitable);
    
    spdlog::debug("Updated last login for player {}", player_id);
    
    co_return;
}

// [SEQUENCE: 543] Coroutine packet handler implementation
boost::asio::awaitable<void> CoroutinePacketHandler::HandlePacketAsync(
    std::shared_ptr<mmorpg::core::network::CoroutineSession> session,
    const mmorpg::proto::Packet& packet) {
    
    try {
        // [SEQUENCE: 544] Dispatch packets to appropriate async handlers
        switch (packet.type()) {
            case mmorpg::proto::PACKET_LOGIN_REQUEST:
                co_await auth_handler_->HandleLoginRequest(session, packet);
                break;
                
            case mmorpg::proto::PACKET_LOGOUT_REQUEST:
                co_await auth_handler_->HandleLogoutRequest(session, packet);
                break;
                
            case mmorpg::proto::PACKET_HEARTBEAT_REQUEST:
                co_await auth_handler_->HandleHeartbeatRequest(session, packet);
                break;
                
            default:
                spdlog::warn("Unknown packet type: {} from session {}", 
                           packet.type(), session->GetSessionId());
                break;
        }
        
    } catch (const std::exception& e) {
        spdlog::error("Exception in async packet handler: {}", e.what());
    }
}

} // namespace mmorpg::game::handlers