#pragma once

#include <string>
#include <cstdint>

namespace mmorpg::game::components {

enum class GuildRank : uint8_t {
    MEMBER = 0,
    OFFICER = 1,
    VICE_LEADER = 2,
    LEADER = 3
};

// [SEQUENCE: MVP5-1] Stores an entity's guild information, such as guild ID, rank, and war status.
struct GuildComponent {
    uint32_t guild_id = 0;
    std::string guild_name;
    uint32_t guild_level = 1;
    GuildRank member_rank = GuildRank::MEMBER;
    
    // Guild war participation
    bool in_guild_war = false;
    uint32_t war_contribution = 0;  // Points earned in current war
    uint32_t total_war_participation = 0;  // Historical count
};

enum class GuildWarState : uint8_t {
    PEACE = 0,
    WAR_DECLARED = 1,
    WAR_ACTIVE = 2,
    WAR_ENDING = 3
};

struct GuildWarStats {
    uint32_t wars_won = 0;
    uint32_t wars_lost = 0;
    uint32_t wars_drawn = 0;
    uint32_t total_kills = 0;
    uint32_t total_deaths = 0;
    uint32_t objectives_captured = 0;
};

} // namespace mmorpg::game::components