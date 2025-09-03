#include <gtest/gtest.h>
#include "game/pvp/pvp_system.h"

TEST(PvPSystemTest, SendDuelRequest) {
    auto& pvp_manager = mmorpg::game::pvp::PvPManager::Instance();
    auto p1 = pvp_manager.CreateController(101);
    auto p2 = pvp_manager.CreateController(102);

    p1->SetCurrentZone(mmorpg::game::pvp::PvPZoneType::DUEL_ZONE);
    p2->SetCurrentZone(mmorpg::game::pvp::PvPZoneType::DUEL_ZONE);

    bool success = pvp_manager.SendDuelRequest(101, 102);
    ASSERT_TRUE(success);
}

TEST(PvPSystemTest, AcceptDuelRequest) {
    auto& pvp_manager = mmorpg::game::pvp::PvPManager::Instance();
    auto p1 = pvp_manager.CreateController(201);
    auto p2 = pvp_manager.CreateController(202);

    p1->SetCurrentZone(mmorpg::game::pvp::PvPZoneType::DUEL_ZONE);
    p2->SetCurrentZone(mmorpg::game::pvp::PvPZoneType::DUEL_ZONE);

    pvp_manager.SendDuelRequest(201, 202);

    bool success = pvp_manager.AcceptDuel(202, 201);
    ASSERT_TRUE(success);

    auto match = pvp_manager.GetMatch(p1->GetCurrentMatch());
    ASSERT_NE(match, nullptr);
    ASSERT_EQ(match->type, mmorpg::game::pvp::PvPType::DUEL);
    ASSERT_EQ(match->state, mmorpg::game::pvp::PvPState::IN_PROGRESS);
}

TEST(PvPSystemTest, DeclineDuelRequest) {
    auto& pvp_manager = mmorpg::game::pvp::PvPManager::Instance();
    auto p1 = pvp_manager.CreateController(301);
    auto p2 = pvp_manager.CreateController(302);

    p1->SetCurrentZone(mmorpg::game::pvp::PvPZoneType::DUEL_ZONE);
    p2->SetCurrentZone(mmorpg::game::pvp::PvPZoneType::DUEL_ZONE);

    pvp_manager.SendDuelRequest(301, 302);

    bool success = pvp_manager.DeclineDuel(302, 301);
    ASSERT_TRUE(success);

    ASSERT_EQ(p1->GetCurrentMatch(), 0);
    ASSERT_EQ(p2->GetCurrentMatch(), 0);
}
