#pragma once

#include "core/network/packet_handler.h"
#include "core/auth/auth_service.h"
#include "core/database/database_manager.h"
#include "core/cache/session_manager.h"
#include "proto/auth.pb.h"

namespace mmorpg::game::handlers {

// [SEQUENCE: 389] Authentication packet handler
class AuthHandler {
public:
    AuthHandler(std::shared_ptr<auth::AuthService> auth_service)
        : auth_service_(auth_service) {}
    
    // [SEQUENCE: 390] Handle login request
    void HandleLoginRequest(core::network::SessionPtr session, 
                           const mmorpg::proto::Packet& packet);
    
    // [SEQUENCE: 391] Handle logout request
    void HandleLogoutRequest(core::network::SessionPtr session, 
                            const mmorpg::proto::Packet& packet);
    
    // [SEQUENCE: 392] Handle heartbeat
    void HandleHeartbeatRequest(core::network::SessionPtr session, 
                               const mmorpg::proto::Packet& packet);
    
private:
    std::shared_ptr<auth::AuthService> auth_service_;
};

} // namespace mmorpg::game::handlers