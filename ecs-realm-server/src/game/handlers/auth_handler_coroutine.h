#pragma once

#include <boost/asio.hpp>
#include <memory>
#include "../../core/auth/auth_service.h"
#include "../../core/network/session_coroutine.h"
#include "proto/packet.pb.h"

namespace mmorpg::game::handlers {

// [SEQUENCE: 507] Modern coroutine-based auth handler
class CoroutineAuthHandler {
public:
    using awaitable = boost::asio::awaitable<void>;
    
    explicit CoroutineAuthHandler(std::shared_ptr<mmorpg::auth::AuthService> auth_service)
        : auth_service_(auth_service) {}
    
    // [SEQUENCE: 508] Async authentication methods
    awaitable HandleLoginRequest(std::shared_ptr<mmorpg::core::network::CoroutineSession> session, 
                                const mmorpg::proto::Packet& packet);
    
    awaitable HandleLogoutRequest(std::shared_ptr<mmorpg::core::network::CoroutineSession> session,
                                 const mmorpg::proto::Packet& packet);
    
    awaitable HandleHeartbeatRequest(std::shared_ptr<mmorpg::core::network::CoroutineSession> session,
                                    const mmorpg::proto::Packet& packet);
    
    // [SEQUENCE: 509] Database operations as coroutines
    awaitable AuthenticateUserAsync(const std::string& username, 
                                   const std::string& password_hash,
                                   const std::string& ip_address);
    
    awaitable ValidateSessionAsync(const std::string& session_id);
    
    // [SEQUENCE: 510] Response sending with coroutines
    awaitable SendLoginResponse(std::shared_ptr<mmorpg::core::network::CoroutineSession> session,
                               bool success, 
                               const std::string& error_message = "",
                               const std::string& access_token = "",
                               uint64_t player_id = 0);
    
    awaitable SendLogoutResponse(std::shared_ptr<mmorpg::core::network::CoroutineSession> session,
                                bool success);
    
    awaitable SendHeartbeatResponse(std::shared_ptr<mmorpg::core::network::CoroutineSession> session);
    
private:
    // [SEQUENCE: 511] Async helper methods
    awaitable LogAuthenticationAttempt(const std::string& username, 
                                      const std::string& ip_address,
                                      bool success);
    
    awaitable UpdateLastLoginAsync(uint64_t player_id);
    
    awaitable CheckRateLimitAsync(const std::string& ip_address);
    
    std::shared_ptr<mmorpg::auth::AuthService> auth_service_;
};

// [SEQUENCE: 512] Async packet handler integration
class CoroutinePacketHandler {
public:
    using awaitable = boost::asio::awaitable<void>;
    
    CoroutinePacketHandler(std::shared_ptr<CoroutineAuthHandler> auth_handler)
        : auth_handler_(auth_handler) {}
    
    // [SEQUENCE: 513] Main async packet dispatch
    awaitable HandlePacketAsync(std::shared_ptr<mmorpg::core::network::CoroutineSession> session,
                               const mmorpg::proto::Packet& packet);
    
private:
    std::shared_ptr<CoroutineAuthHandler> auth_handler_;
};

} // namespace mmorpg::game::handlers