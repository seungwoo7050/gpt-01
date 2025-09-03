#pragma once

#include "game/pvp/pvp_system.h"
#include "network/session.h"
#include "network/session_manager.h"
#include "proto/packet.pb.h"

namespace mmorpg::game::handlers {

// [SEQUENCE: MVP5-23] A new packet handler was created to process PvP-related packets and call the appropriate methods on the `PvPManager` singleton.
class PvPHandler {
public:
    PvPHandler(std::shared_ptr<network::SessionManager> session_manager);

    void HandleDuelAcceptRequest(std::shared_ptr<network::Session> session, const proto::DuelAcceptRequest& packet);
    void HandleDuelDeclineRequest(std::shared_ptr<network::Session> session, const proto::DuelDeclineRequest& packet);

private:
    pvp::PvPManager& pvp_manager_;
    std::shared_ptr<network::SessionManager> session_manager_;
};

}
