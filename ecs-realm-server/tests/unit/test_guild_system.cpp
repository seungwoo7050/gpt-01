#include <gtest/gtest.h>
#include "game/social/guild_manager.h"

// [SEQUENCE: MVP5-104] Tests the creation of a new guild.
TEST(GuildSystemTest, CreateGuild) {
    auto& guild_manager = mmorpg::game::social::GuildManager::Instance();
    std::vector<uint64_t> signers = {2, 3, 4, 5};
    auto guild = guild_manager.CreateGuild("Test Guild", 1, signers);
    ASSERT_NE(guild, nullptr);
    ASSERT_EQ(guild->GetName(), "Test Guild");
    ASSERT_EQ(guild->GetMemberCount(), 5);
}

// [SEQUENCE: MVP5-105] Tests inviting a player to a guild.
TEST(GuildSystemTest, InviteToGuild) {
    auto& guild_manager = mmorpg::game::social::GuildManager::Instance();
    std::vector<uint64_t> signers = {12, 13, 14, 15};
    auto guild = guild_manager.CreateGuild("Invite Test Guild", 11, signers);
    ASSERT_NE(guild, nullptr);

    bool success = guild_manager.InviteToGuild(guild->GetId(), 11, 16, "Newbie");
    ASSERT_TRUE(success);
}

// [SEQUENCE: MVP5-106] Tests a player accepting a guild invite.
TEST(GuildSystemTest, AcceptGuildInvite) {
    auto& guild_manager = mmorpg::game::social::GuildManager::Instance();
    std::vector<uint64_t> signers = {22, 23, 24, 25};
    auto guild = guild_manager.CreateGuild("Accept Test Guild", 21, signers);
    ASSERT_NE(guild, nullptr);

    guild_manager.InviteToGuild(guild->GetId(), 21, 26, "Newbie");

    bool success = guild_manager.AcceptGuildInvite(26, "Newbie");
    ASSERT_TRUE(success);

    auto new_member = guild->GetMember(26);
    ASSERT_NE(new_member, nullptr);
    ASSERT_EQ(new_member->character_name, "Newbie");
}

// [SEQUENCE: MVP5-107] Tests a player leaving a guild.
TEST(GuildSystemTest, LeaveGuild) {
    auto& guild_manager = mmorpg::game::social::GuildManager::Instance();
    std::vector<uint64_t> signers = {32, 33, 34, 35};
    auto guild = guild_manager.CreateGuild("Leave Test Guild", 31, signers);
    ASSERT_NE(guild, nullptr);

    bool success = guild_manager.LeaveGuild(32);
    ASSERT_TRUE(success);

    auto member = guild->GetMember(32);
    ASSERT_EQ(member, nullptr);
}