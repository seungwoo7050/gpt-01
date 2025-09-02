#pragma once

#include "core/singleton.h"
#include "game/components/quest_component.h"

namespace mmorpg::game::systems {

// [SEQUENCE: MVP10-16] Quest reward system for calculating and distributing rewards
class QuestRewardSystem : public Singleton<QuestRewardSystem> {
public:
    // [SEQUENCE: MVP10-17] Grant rewards for a completed quest
    bool GrantRewards(core::ecs::EntityId player_id, uint32_t quest_id);

private:
    friend class Singleton<QuestRewardSystem>;
    QuestRewardSystem() = default;

    // [SEQUENCE: MVP10-18] Calculate rewards based on quest data and player level
    QuestReward CalculateRewards(uint32_t quest_id, int player_level);
};

} // namespace mmorpg::game::systems