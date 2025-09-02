#pragma once

#include "game/components/pvp_stats_component.h"

namespace mmorpg::game::utils {

inline components::PvPRank GetRankFromRating(int32_t rating) {
    if (rating < 1200) return components::PvPRank::BRONZE_I;
    if (rating < 1300) return components::PvPRank::BRONZE_II;
    if (rating < 1400) return components::PvPRank::BRONZE_III;
    if (rating < 1500) return components::PvPRank::SILVER_I;
    if (rating < 1600) return components::PvPRank::SILVER_II;
    if (rating < 1700) return components::PvPRank::SILVER_III;
    if (rating < 1800) return components::PvPRank::GOLD_I;
    if (rating < 1900) return components::PvPRank::GOLD_II;
    if (rating < 2000) return components::PvPRank::GOLD_III;
    if (rating < 2100) return components::PvPRank::PLATINUM_I;
    if (rating < 2200) return components::PvPRank::PLATINUM_II;
    if (rating < 2300) return components::PvPRank::PLATINUM_III;
    if (rating < 2400) return components::PvPRank::DIAMOND_I;
    if (rating < 2500) return components::PvPRank::DIAMOND_II;
    if (rating < 2600) return components::PvPRank::DIAMOND_III;
    if (rating < 2800) return components::PvPRank::MASTER;
    return components::PvPRank::GRANDMASTER;
}

} // namespace mmorpg::game::utils
