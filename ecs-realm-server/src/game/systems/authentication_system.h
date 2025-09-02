#pragma once

#include "core/ecs/system.h"
#include <string>

namespace mmorpg::game::systems {

// [SEQUENCE: MVP7-1] Authentication system for handling JWT-based auth
class AuthenticationSystem : public core::ecs::System {
public:
    AuthenticationSystem() : System("AuthenticationSystem") {}

    // [SEQUENCE: MVP7-2] Authenticate a session using a JWT
    bool AuthenticateSession(core::ecs::EntityId session_entity_id, const std::string& jwt_token);

    // [SEQUENCE: MVP7-3] Check if a session is authenticated
    bool IsAuthenticated(core::ecs::EntityId session_entity_id) const;

private:
    // [SEQUENCE: MVP7-4] In a real implementation, this would hold authenticated session data
    std::unordered_set<core::ecs::EntityId> authenticated_sessions_;
};

} // namespace mmorpg::game::systems