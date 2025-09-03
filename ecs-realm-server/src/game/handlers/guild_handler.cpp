#include "guild_handler.h"

namespace mmorpg::game::handlers {

// [SEQUENCE: MVP5-22] A new packet handler was created to process guild-related packets and call the appropriate methods on the `GuildManager` singleton.
GuildHandler::GuildHandler(std::shared_ptr<network::SessionManager> session_manager)
    : guild_manager_(social::GuildManager::Instance()), session_manager_(session_manager) {
}

void GuildHandler::HandleGuildCreateRequest([[maybe_unused]] std::shared_ptr<network::Session> session, const proto::GuildCreateRequest& packet) {
    uint64_t player_id = session_manager_->GetPlayerIdForSession(session->GetSessionId());
    if (player_id == 0) return; // Not authenticated

    std::vector<uint64_t> charter_signers;
    for (const auto& signer : packet.charter_signers()) {
        charter_signers.push_back(signer);
    }
    guild_manager_.CreateGuild(packet.guild_name(), player_id, charter_signers);
}

void GuildHandler::HandleGuildInviteRequest([[maybe_unused]] std::shared_ptr<network::Session> session, const proto::GuildInviteRequest& packet) {
    uint64_t inviter_id = session_manager_->GetPlayerIdForSession(session->GetSessionId());
    if (inviter_id == 0) return; // Not authenticated

    guild_manager_.InviteToGuild(packet.guild_id(), inviter_id, packet.target_id(), packet.target_name());
}

void GuildHandler::HandleGuildInviteAcceptRequest([[maybe_unused]] std::shared_ptr<network::Session> session, [[maybe_unused]] const proto::GuildInviteAcceptRequest& packet) {
    uint64_t player_id = session_manager_->GetPlayerIdForSession(session->GetSessionId());
    if (player_id == 0) return; // Not authenticated

    guild_manager_.AcceptGuildInvite(player_id, ""); // TODO: get player name
}

void GuildHandler::HandleGuildLeaveRequest([[maybe_unused]] std::shared_ptr<network::Session> session, [[maybe_unused]] const proto::GuildLeaveRequest& packet) {
    uint64_t player_id = session_manager_->GetPlayerIdForSession(session->GetSessionId());
    if (player_id == 0) return; // Not authenticated

    guild_manager_.LeaveGuild(player_id);
}

}
