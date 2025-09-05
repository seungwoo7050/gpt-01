#pragma once

#include "guild.h"
#include <vector>
#include <string>
#include <cstdint>
#include <unordered_map>

namespace mmorpg::game::social {

// [SEQUENCE: MVP5-92] Manages all guilds in the game world.
class GuildManager {
public:
    static GuildManager& Instance();

    Guild* CreateGuild(std::string name, uint64_t leader_id, const std::vector<uint64_t>& signers);
    bool InviteToGuild(uint32_t guild_id, uint64_t inviter_id, uint64_t invitee_id, std::string invitee_name);
    bool AcceptGuildInvite(uint64_t invitee_id, std::string invitee_name);
    bool LeaveGuild(uint64_t member_id);

private:
    GuildManager() = default;
    ~GuildManager() = default;
    GuildManager(const GuildManager&) = delete;
    GuildManager& operator=(const GuildManager&) = delete;

    uint32_t m_next_guild_id = 1;
    std::unordered_map<uint32_t, Guild> m_guilds;
    std::unordered_map<std::string, uint32_t> m_pending_invites; // invitee_name -> guild_id
    std::unordered_map<uint64_t, uint32_t> m_player_to_guild;
};

}
