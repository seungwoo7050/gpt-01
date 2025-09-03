// [SEQUENCE: MVP5-17] A new utility file created to house the GetRankFromRating function.
#pragma once

#include <cstdint>

namespace mmorpg::game::utils {

// [SEQUENCE: 10] PvP rank tiers
enum class PvPRank {
    BRONZE_I,
    BRONZE_II,
    BRONZE_III,
    SILVER_I,
    SILVER_II,
    SILVER_III,
    GOLD_I,
    GOLD_II,
    GOLD_III,
    PLATINUM_I,
    PLATINUM_II,
    PLATINUM_III,
    DIAMOND_I,
    DIAMOND_II,
    DIAMOND_III,
    MASTER,
    GRANDMASTER
};

inline PvPRank GetRankFromRating(int32_t rating) {
    if (rating < 1200) return PvPRank::BRONZE_I;
    if (rating < 1300) return PvPRank::BRONZE_II;
    if (rating < 1400) return PvPRank::BRONZE_III;
    if (rating < 1500) return PvPRank::SILVER_I;
    if (rating < 1600) return PvPRank::SILVER_II;
    if (rating < 1700) return PvPRank::SILVER_III;
    if (rating < 1800) return PvPRank::GOLD_I;
    if (rating < 1900) return PvPRank::GOLD_II;
    if (rating < 2000) return PvPRank::GOLD_III;
    if (rating < 2100) return PvPRank::PLATINUM_I;
    if (rating < 2200) return PvPRank::PLATINUM_II;
    if (rating < 2300) return PvPRank::PLATINUM_III;
    if (rating < 2400) return PvPRank::DIAMOND_I;
    if (rating < 2500) return PvPRank::DIAMOND_II;
    if (rating < 2600) return PvPRank::DIAMOND_III;
    if (rating < 2800) return PvPRank::MASTER;
    return PvPRank::GRANDMASTER;
}

} // namespace mmorpg::game::utils
