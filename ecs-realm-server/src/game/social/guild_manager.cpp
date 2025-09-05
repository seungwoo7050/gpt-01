#include "guild_manager.h"

namespace mmorpg::game::social {

// [SEQUENCE: MVP5-93] Implements the GuildManager singleton and its methods.
GuildManager& GuildManager::Instance() {
    static GuildManager instance;
    return instance;
}

Guild* GuildManager::CreateGuild(std::string name, uint64_t leader_id, const std::vector<uint64_t>& signers) {
    uint32_t guild_id = m_next_guild_id++;
    m_guilds.emplace(guild_id, Guild(guild_id, name, leader_id, signers));
    auto& guild = m_guilds.at(guild_id);
    m_player_to_guild[leader_id] = guild_id;
    for (auto signer_id : signers) {
        m_player_to_guild[signer_id] = guild_id;
    }
    return &guild;
}

bool GuildManager::InviteToGuild([[maybe_unused]] uint32_t guild_id, [[maybe_unused]] uint64_t inviter_id, [[maybe_unused]] uint64_t invitee_id, std::string invitee_name) {
    // TODO: Check if inviter has permission
    m_pending_invites[invitee_name] = guild_id;
    return true;
}

bool GuildManager::AcceptGuildInvite(uint64_t invitee_id, std::string invitee_name) {
    auto it = m_pending_invites.find(invitee_name);
    if (it == m_pending_invites.end()) {
        return false;
    }
    uint32_t guild_id = it->second;
    m_pending_invites.erase(it);

    auto guild_it = m_guilds.find(guild_id);
    if (guild_it == m_guilds.end()) {
        return false;
    }

    guild_it->second.AddMember(invitee_id, invitee_name);
    m_player_to_guild[invitee_id] = guild_id;
    return true;
}

bool GuildManager::LeaveGuild(uint64_t member_id) {
    auto it = m_player_to_guild.find(member_id);
    if (it == m_player_to_guild.end()) {
        return false;
    }
    uint32_t guild_id = it->second;
    m_player_to_guild.erase(it);

    auto guild_it = m_guilds.find(guild_id);
    if (guild_it != m_guilds.end()) {
        guild_it->second.RemoveMember(member_id);
    }

    return true;
}

}