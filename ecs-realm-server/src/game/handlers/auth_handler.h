#pragma once

#include "core/network/packet_handler.h"
#include "core/auth/auth_service.h"
#include "core/database/database_manager.h"
#include "core/cache/session_manager.h"
#include "proto/auth.pb.h"

// [SEQUENCE: MVP1-61] Game server authentication packet handler.

namespace mmorpg::game::handlers {

class AuthHandler {
public:
    AuthHandler(std::shared_ptr<auth::AuthService> auth_service)
        : auth_service_(auth_service) {}
    
    void HandleLoginRequest(core::network::SessionPtr session, 
                           const mmorpg::proto::Packet& packet);
    
    void HandleLogoutRequest(core::network::SessionPtr session, 
                            const mmorpg::proto::Packet& packet);
    
    void HandleHeartbeatRequest(core::network::SessionPtr session, 
                               const mmorpg::proto::Packet& packet);
    
private:
    std::shared_ptr<auth::AuthService> auth_service_;
};

} // namespace mmorpg::game::handlers