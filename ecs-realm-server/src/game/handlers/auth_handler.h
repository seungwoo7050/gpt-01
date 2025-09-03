#pragma once

#include "core/network/packet_handler.h"
#include "core/auth/auth_service.h"
#include "core/database/database_manager.h"
#include "core/cache/session_manager.h"
#include "proto/auth.pb.h"

#include "network/session_manager.h"

namespace mmorpg::game::handlers {

class AuthHandler {
public:
    AuthHandler(std::shared_ptr<auth::AuthService> auth_service, std::shared_ptr<network::SessionManager> session_manager);

    void HandleLoginRequest(core::network::SessionPtr session, 
                                    const mmorpg::proto::Packet& packet);
    void HandleLogoutRequest(core::network::SessionPtr session, 
                                     const mmorpg::proto::Packet& packet);
    void HandleHeartbeatRequest(core::network::SessionPtr session, 
                                        const mmorpg::proto::Packet& packet);

private:
    std::shared_ptr<auth::AuthService> auth_service_;
    std::shared_ptr<network::SessionManager> session_manager_;
};