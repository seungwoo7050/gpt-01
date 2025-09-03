#include "pvp_handler.h"

namespace mmorpg::game::handlers {

// [SEQUENCE: MVP5-23] A new packet handler was created to process PvP-related packets and call the appropriate methods on the `PvPManager` singleton.
PvPHandler::PvPHandler(std::shared_ptr<network::SessionManager> session_manager)
    : pvp_manager_(pvp::PvPManager::Instance()), session_manager_(session_manager) {
}

void PvPHandler::HandleDuelAcceptRequest(std::shared_ptr<network::Session> session, const proto::DuelAcceptRequest& packet) {
    uint64_t player_id = session_manager_->GetPlayerIdForSession(session->GetSessionId());
    if (player_id == 0) return; // Not authenticated

    pvp_manager_.AcceptDuel(player_id, packet.challenger_id());
}

void PvPHandler::HandleDuelDeclineRequest(std::shared_ptr<network::Session> session, const proto::DuelDeclineRequest& packet) {
    uint64_t player_id = session_manager_->GetPlayerIdForSession(session->GetSessionId());
    if (player_id == 0) return; // Not authenticated

    pvp_manager_.DeclineDuel(player_id, packet.challenger_id());
}

}
