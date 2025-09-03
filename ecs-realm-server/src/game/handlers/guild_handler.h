#pragma once

#include "game/social/guild_system.h"
#include "network/session.h"
#include "proto/packet.pb.h"

#include "network/session_manager.h"

namespace mmorpg::game::handlers {

// [SEQUENCE: MVP5-22] A new packet handler was created to process guild-related packets and call the appropriate methods on the `GuildManager` singleton.
class GuildHandler {
public:
    GuildHandler(std::shared_ptr<network::SessionManager> session_manager);

    void HandleGuildCreateRequest(std::shared_ptr<network::Session> session, const proto::GuildCreateRequest& packet);
    void HandleGuildInviteRequest(std::shared_ptr<network::Session> session, const proto::GuildInviteRequest& packet);
    void HandleGuildInviteAcceptRequest(std::shared_ptr<network::Session> session, const proto::GuildInviteAcceptRequest& packet);
    void HandleGuildLeaveRequest(std::shared_ptr<network::Session> session, const proto::GuildLeaveRequest& packet);

private:
    social::GuildManager& guild_manager_;
    std::shared_ptr<network::SessionManager> session_manager_;
};

}
