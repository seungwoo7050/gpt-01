#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>
#include "../core/types.h"

namespace mmorpg::rewards {

// [SEQUENCE: MVP13-172] Reward types
enum class RewardType {
    CURRENCY,
    ITEM,
    EXPERIENCE,
    REPUTATION,
    TITLE,
    ACHIEVEMENT,
    MOUNT,
    PET,
    BUFF,
    QUEST_ITEM,
    CHOICE_OF_ITEM
};

// [SEQUENCE: MVP13-173] Reward item data
struct RewardItem {
    uint32_t item_id;
    uint32_t quantity;
    double drop_chance{1.0};
};

// [SEQUENCE: MVP13-174] Reward definition
struct Reward {
    RewardType type;
    uint32_t id; // Currency ID, Item ID, Buff ID, etc.
    uint64_t quantity;
    
    // For choice rewards
    std::vector<RewardItem> item_choices;
};

// [SEQUENCE: MVP13-175] Reward source
enum class RewardSource {
    QUEST,
    MONSTER_KILL,
    BOSS_KILL,
    PVP_VICTORY,
    ARENA_MATCH,
    TOURNAMENT,
    ACHIEVEMENT,
    DAILY_LOGIN,
    LEVEL_UP,
    CRAFTING,
    GATHERING,
    SPECIAL_EVENT
};

// [SEQUENCE: MVP13-176] Reward system
class RewardsSystem {
public:
    // [SEQUENCE: MVP13-177] Grant rewards to a player
    void GrantRewards(uint64_t player_id, RewardSource source, uint32_t source_id);

    // [SEQUENCE: MVP13-178] Register reward table
    void RegisterRewardTable(uint32_t table_id, const std::vector<Reward>& rewards);

private:
    std::unordered_map<uint32_t, std::vector<Reward>> reward_tables_;

    // [SEQUENCE: MVP13-179] Get rewards for a specific source
    std::vector<Reward> GetRewardsForSource(RewardSource source, uint32_t source_id);
};

} // namespace mmorpg::rewards
