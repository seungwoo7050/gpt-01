#include "reward_system.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace mmorpg::rewards {

// [SEQUENCE: 2912] Reward system integration
class RewardIntegration {
public:
    // [SEQUENCE: 2913] Initialize reward system with game server
    static void InitializeWithGameServer(GameServer* server,
                                       RewardService* reward_service) {
        
        // [SEQUENCE: 2914] Set up level-up rewards
        server->OnPlayerLevelUp = [reward_service](uint64_t player_id, 
                                                  uint32_t old_level, 
                                                  uint32_t new_level) {
            // Grant level-up rewards
            auto level_rewards = GetLevelUpRewards(new_level);
            
            RewardSource source;
            source.type = RewardSource::SourceType::LEVEL_UP;
            source.source_name = "Level " + std::to_string(new_level);
            source.timestamp = std::chrono::system_clock::now();
            
            for (const auto& reward : level_rewards) {
                reward_service->GrantReward(player_id, reward, source);
            }
            
            // Check level milestones
            reward_service->CheckMilestoneRewards(player_id, "character_level", new_level);
            
            // Special milestone levels
            if (new_level % 10 == 0) {
                GrantLevelMilestoneReward(reward_service, player_id, new_level);
            }
        };
        
        // [SEQUENCE: 2915] Set up quest completion rewards
        server->OnQuestComplete = [reward_service](uint64_t player_id,
                                                 const Quest& quest) {
            RewardSource source;
            source.type = RewardSource::SourceType::QUEST;
            source.source_id = quest.quest_id;
            source.source_name = quest.name;
            source.timestamp = std::chrono::system_clock::now();
            
            // Grant quest rewards
            for (const auto& reward : quest.rewards) {
                reward_service->GrantReward(player_id, reward, source);
            }
            
            // Bonus rewards for first completion
            if (quest.is_first_completion) {
                auto bonus = CreateFirstCompletionBonus(quest);
                reward_service->GrantReward(player_id, bonus, source);
            }
        };
        
        // [SEQUENCE: 2916] Set up achievement rewards
        server->OnAchievementUnlocked = [reward_service](uint64_t player_id,
                                                        const Achievement& achievement) {
            RewardSource source;
            source.type = RewardSource::SourceType::ACHIEVEMENT;
            source.source_id = achievement.achievement_id;
            source.source_name = achievement.name;
            source.timestamp = std::chrono::system_clock::now();
            
            // Grant achievement rewards
            if (achievement.has_reward) {
                reward_service->GrantRewardPackage(player_id, 
                                                 achievement.reward_package, 
                                                 source);
            }
            
            // Achievement points
            auto points_reward = RewardService::CreateCurrencyReward(
                CurrencyType::ACHIEVEMENT_POINTS, achievement.points);
            reward_service->GrantReward(player_id, points_reward, source);
        };
        
        // [SEQUENCE: 2917] Set up arena match rewards
        server->OnArenaMatchComplete = [reward_service](uint64_t player_id,
                                                      const ArenaMatchResult& result) {
            RewardSource source;
            source.type = RewardSource::SourceType::ARENA_MATCH;
            source.source_id = std::to_string(result.match_id);
            source.source_name = "Arena " + GetArenaTypeName(result.arena_type);
            source.timestamp = std::chrono::system_clock::now();
            
            // Base rewards
            if (result.is_winner) {
                auto honor = RewardService::CreateCurrencyReward(
                    CurrencyType::HONOR_POINTS, result.winner_honor);
                reward_service->GrantReward(player_id, honor, source);
            } else {
                auto honor = RewardService::CreateCurrencyReward(
                    CurrencyType::HONOR_POINTS, result.loser_honor);
                reward_service->GrantReward(player_id, honor, source);
            }
            
            // MVP bonus
            if (result.is_mvp) {
                auto mvp_bonus = RewardService::CreateCurrencyReward(
                    CurrencyType::HONOR_POINTS, 25);
                reward_service->GrantReward(player_id, mvp_bonus, source);
            }
            
            // Win streak bonus
            if (result.win_streak >= 3) {
                auto streak_bonus = CreateWinStreakReward(result.win_streak);
                reward_service->GrantReward(player_id, streak_bonus, source);
            }
            
            // Check PvP milestones
            reward_service->CheckMilestoneRewards(player_id, "arena_wins", 
                                                result.total_wins);
        };
        
        // [SEQUENCE: 2918] Set up dungeon completion rewards
        server->OnDungeonComplete = [reward_service](uint64_t player_id,
                                                   const DungeonResult& result) {
            RewardSource source;
            source.type = RewardSource::SourceType::DUNGEON;
            source.source_id = result.dungeon_id;
            source.source_name = result.dungeon_name;
            source.timestamp = std::chrono::system_clock::now();
            
            // Base completion reward
            auto tokens = RewardService::CreateCurrencyReward(
                CurrencyType::DUNGEON_TOKENS, result.token_reward);
            reward_service->GrantReward(player_id, tokens, source);
            
            // Bonus for speed clear
            if (result.completion_time < result.par_time) {
                auto speed_bonus = CreateSpeedClearBonus(result);
                reward_service->GrantReward(player_id, speed_bonus, source);
            }
            
            // Perfect run bonus (no deaths)
            if (result.death_count == 0) {
                auto perfect_bonus = RewardService::CreateCurrencyReward(
                    CurrencyType::DUNGEON_TOKENS, 50);
                reward_service->GrantReward(player_id, perfect_bonus, source);
            }
            
            // Loot roll rewards
            for (const auto& loot : result.loot_rolls) {
                if (loot.winner_id == player_id) {
                    auto item_reward = RewardService::CreateItemReward(
                        loot.item_id, 1);
                    item_reward.item_quality = loot.quality;
                    reward_service->GrantReward(player_id, item_reward, source);
                }
            }
        };
        
        // [SEQUENCE: 2919] Set up tournament rewards
        server->OnTournamentComplete = [reward_service](
            const TournamentResult& tournament_result) {
            
            for (const auto& participant : tournament_result.final_standings) {
                RewardSource source;
                source.type = RewardSource::SourceType::TOURNAMENT;
                source.source_id = std::to_string(tournament_result.tournament_id);
                source.source_name = tournament_result.tournament_name;
                source.timestamp = std::chrono::system_clock::now();
                
                // Get placement rewards
                auto placement_rewards = GetTournamentPlacementRewards(
                    participant.placement, tournament_result.reward_tier);
                
                for (const auto& reward : placement_rewards) {
                    reward_service->GrantReward(participant.player_id, reward, source);
                }
                
                // Participation reward
                if (participant.matches_played > 0) {
                    auto participation = RewardService::CreateCurrencyReward(
                        CurrencyType::TOURNAMENT_TOKENS, 10);
                    reward_service->GrantReward(participant.player_id, 
                                              participation, source);
                }
                
                // Special title for winners
                if (participant.placement == 1) {
                    auto title = RewardService::CreateTitleReward(
                        tournament_result.champion_title_id);
                    reward_service->GrantReward(participant.player_id, title, source);
                }
            }
        };
        
        // [SEQUENCE: 2920] Set up daily login processing
        server->OnPlayerLogin = [reward_service](uint64_t player_id) {
            // Process daily rewards
            reward_service->ProcessDailyRewards(player_id);
            
            // Check weekly activities
            CheckWeeklyActivities(reward_service, player_id);
            
            // Special event rewards
            CheckEventRewards(reward_service, player_id);
        };
        
        // [SEQUENCE: 2921] Set up weekly reset
        server->ScheduleWeeklyTask("weekly_reward_reset", 
            WeeklyResetDay::TUESDAY, 4, 0, // Tuesday 4:00 AM
            [reward_service]() {
                reward_service->ProcessWeeklyReset();
                
                // Reset weekly quest rewards
                ResetWeeklyQuests();
                
                // Reset weekly dungeon bonuses
                ResetWeeklyDungeonBonuses();
            });
        
        // [SEQUENCE: 2922] Set up reward claim commands
        server->RegisterCommand("claim", [reward_service, server](
            uint64_t player_id, const std::vector<std::string>& args) {
            
            if (args.empty()) {
                ShowClaimableRewards(server, player_id, reward_service);
                return;
            }
            
            const std::string& reward_id = args[0];
            ClaimPendingReward(server, player_id, reward_service, reward_id);
        });
        
        // [SEQUENCE: 2923] Set up special event rewards
        SetupSeasonalEvents(server, reward_service);
        SetupHolidayEvents(server, reward_service);
        SetupSpecialPromotions(server, reward_service);
    }
    
private:
    // [SEQUENCE: 2924] Get level-up rewards
    static std::vector<Reward> GetLevelUpRewards(uint32_t level) {
        std::vector<Reward> rewards;
        
        // Gold reward scales with level
        uint32_t gold_amount = level * 100;
        rewards.push_back(RewardService::CreateCurrencyReward(
            CurrencyType::GOLD, gold_amount));
        
        // Skill point every 5 levels
        if (level % 5 == 0) {
            Reward skill_point;
            skill_point.type = RewardType::SKILL_POINT;
            skill_point.skill_points = 1;
            skill_point.display_name = "1 Skill Point";
            rewards.push_back(skill_point);
        }
        
        // Special rewards at certain levels
        switch (level) {
            case 10:
                // First mount
                rewards.push_back(CreateMountReward(10001, "Starter Mount"));
                break;
                
            case 20:
                // Bag upgrade
                rewards.push_back(RewardService::CreateItemReward(20001)); // 16-slot bag
                break;
                
            case 40:
                // Epic mount
                rewards.push_back(CreateMountReward(10002, "Swift Mount"));
                break;
                
            case 60:
                // Max level rewards
                rewards.push_back(RewardService::CreateTitleReward("max_level"));
                rewards.push_back(CreateMountReward(10003, "Elite Mount"));
                rewards.push_back(RewardService::CreateCurrencyReward(
                    CurrencyType::GOLD, 10000));
                break;
        }
        
        return rewards;
    }
    
    // [SEQUENCE: 2925] Grant level milestone reward
    static void GrantLevelMilestoneReward(RewardService* reward_service,
                                        uint64_t player_id,
                                        uint32_t level) {
        RewardPackage milestone_package;
        milestone_package.package_id = "level_milestone_" + std::to_string(level);
        milestone_package.package_name = "Level " + std::to_string(level) + " Milestone";
        
        // Every 10 levels gets special rewards
        milestone_package.rewards.push_back(
            RewardService::CreateCurrencyReward(CurrencyType::GOLD, level * 500));
        
        // Consumables
        milestone_package.rewards.push_back(
            RewardService::CreateItemReward(30001, 10)); // Health potions
        milestone_package.rewards.push_back(
            RewardService::CreateItemReward(30002, 10)); // Mana potions
        
        RewardSource source;
        source.type = RewardSource::SourceType::MILESTONE;
        source.source_name = "Level " + std::to_string(level) + " Milestone";
        source.timestamp = std::chrono::system_clock::now();
        
        reward_service->GrantRewardPackage(player_id, milestone_package, source);
    }
    
    // [SEQUENCE: 2926] Create first completion bonus
    static Reward CreateFirstCompletionBonus(const Quest& quest) {
        // Double gold for first completion
        return RewardService::CreateCurrencyReward(
            CurrencyType::GOLD, quest.gold_reward);
    }
    
    // [SEQUENCE: 2927] Create win streak reward
    static Reward CreateWinStreakReward(uint32_t streak) {
        uint32_t bonus_honor = streak * 10;
        return RewardService::CreateCurrencyReward(
            CurrencyType::HONOR_POINTS, bonus_honor);
    }
    
    // [SEQUENCE: 2928] Create speed clear bonus
    static Reward CreateSpeedClearBonus(const DungeonResult& result) {
        // Bonus tokens based on how much faster than par time
        double time_ratio = static_cast<double>(result.completion_time) / result.par_time;
        uint32_t bonus_tokens = 0;
        
        if (time_ratio < 0.5) {
            bonus_tokens = 100; // 50% faster
        } else if (time_ratio < 0.75) {
            bonus_tokens = 50;  // 25% faster
        } else if (time_ratio < 0.9) {
            bonus_tokens = 25;  // 10% faster
        }
        
        return RewardService::CreateCurrencyReward(
            CurrencyType::DUNGEON_TOKENS, bonus_tokens);
    }
    
    // [SEQUENCE: 2929] Get tournament placement rewards
    static std::vector<Reward> GetTournamentPlacementRewards(uint32_t placement,
                                                           uint32_t reward_tier) {
        std::vector<Reward> rewards;
        
        // Base rewards by placement
        struct PlacementReward {
            uint32_t gold;
            uint32_t tokens;
            uint32_t honor;
        };
        
        std::unordered_map<uint32_t, PlacementReward> base_rewards = {
            {1, {5000, 100, 500}},
            {2, {2500, 50, 250}},
            {3, {1000, 25, 100}},
            {4, {500, 10, 50}},
            {5, {500, 10, 50}},
            {6, {500, 10, 50}},
            {7, {500, 10, 50}},
            {8, {500, 10, 50}}
        };
        
        auto it = base_rewards.find(placement);
        if (it != base_rewards.end()) {
            const auto& base = it->second;
            
            // Scale by reward tier
            uint32_t tier_multiplier = reward_tier;
            
            rewards.push_back(RewardService::CreateCurrencyReward(
                CurrencyType::GOLD, base.gold * tier_multiplier));
            rewards.push_back(RewardService::CreateCurrencyReward(
                CurrencyType::TOURNAMENT_TOKENS, base.tokens * tier_multiplier));
            rewards.push_back(RewardService::CreateCurrencyReward(
                CurrencyType::HONOR_POINTS, base.honor * tier_multiplier));
        }
        
        // Special items for top 3
        if (placement <= 3) {
            uint32_t item_id = 40000 + placement + (reward_tier * 10);
            rewards.push_back(RewardService::CreateItemReward(item_id));
        }
        
        return rewards;
    }
    
    // [SEQUENCE: 2930] Check weekly activities
    static void CheckWeeklyActivities(RewardService* reward_service,
                                    uint64_t player_id) {
        // Check if player completed weekly activities
        auto activities = GetPlayerWeeklyActivities(player_id);
        
        // Award progress rewards
        uint32_t completed = activities.completed_count;
        
        if (completed >= 3 && !activities.claimed_3) {
            auto reward = RewardService::CreateCurrencyReward(
                CurrencyType::GOLD, 1000);
            GrantWeeklyReward(reward_service, player_id, reward, "3 Activities");
        }
        
        if (completed >= 5 && !activities.claimed_5) {
            auto reward = RewardService::CreateItemReward(30010); // Weekly cache
            GrantWeeklyReward(reward_service, player_id, reward, "5 Activities");
        }
        
        if (completed >= 7 && !activities.claimed_7) {
            auto reward = CreateWeeklyCompletionReward();
            GrantWeeklyReward(reward_service, player_id, reward, "Weekly Completion");
        }
    }
    
    // [SEQUENCE: 2931] Check event rewards
    static void CheckEventRewards(RewardService* reward_service,
                                uint64_t player_id) {
        auto active_events = GetActiveEvents();
        
        for (const auto& event : active_events) {
            // Check login rewards
            if (event.has_login_reward && !HasClaimedEventLogin(player_id, event.event_id)) {
                RewardSource source;
                source.type = RewardSource::SourceType::EVENT;
                source.source_id = event.event_id;
                source.source_name = event.name + " Login Bonus";
                source.timestamp = std::chrono::system_clock::now();
                
                reward_service->GrantReward(player_id, event.login_reward, source);
                MarkEventLoginClaimed(player_id, event.event_id);
            }
            
            // Check event currency multipliers
            if (event.has_currency_bonus) {
                ApplyEventCurrencyBonus(player_id, event);
            }
        }
    }
    
    // [SEQUENCE: 2932] Show claimable rewards
    static void ShowClaimableRewards(GameServer* server, uint64_t player_id,
                                   RewardService* reward_service) {
        auto pending = GetPendingRewards(player_id);
        
        std::stringstream msg;
        msg << "=== Claimable Rewards ===\n";
        
        if (pending.empty()) {
            msg << "No rewards available to claim.\n";
        } else {
            for (const auto& reward : pending) {
                msg << "[" << reward.claim_id << "] " 
                    << reward.display_name << " - " 
                    << reward.source_name << "\n";
            }
            msg << "\nUse /claim <id> to claim a reward.\n";
        }
        
        server->SendMessage(player_id, msg.str());
    }
    
    // [SEQUENCE: 2933] Claim pending reward
    static void ClaimPendingReward(GameServer* server, uint64_t player_id,
                                 RewardService* reward_service,
                                 const std::string& reward_id) {
        auto pending = GetPendingReward(player_id, reward_id);
        if (!pending.has_value()) {
            server->SendMessage(player_id, "Invalid reward ID.");
            return;
        }
        
        // Check claim conditions
        if (!CheckClaimConditions(player_id, pending->conditions)) {
            server->SendMessage(player_id, "You don't meet the requirements to claim this reward.");
            return;
        }
        
        // Grant reward
        if (reward_service->GrantReward(player_id, pending->reward, pending->source)) {
            MarkRewardClaimed(player_id, reward_id);
            server->SendMessage(player_id, "Reward claimed successfully!");
        } else {
            server->SendMessage(player_id, "Failed to claim reward. Check inventory space.");
        }
    }
    
    // [SEQUENCE: 2934] Setup seasonal events
    static void SetupSeasonalEvents(GameServer* server, RewardService* reward_service) {
        // Spring event
        ScheduleSeasonalEvent(server, reward_service, "Spring Festival",
            std::chrono::months(3), std::chrono::weeks(2),
            CreateSpringEventRewards());
        
        // Summer event
        ScheduleSeasonalEvent(server, reward_service, "Summer Games",
            std::chrono::months(6), std::chrono::weeks(3),
            CreateSummerEventRewards());
        
        // Fall event
        ScheduleSeasonalEvent(server, reward_service, "Harvest Festival",
            std::chrono::months(9), std::chrono::weeks(2),
            CreateHarvestEventRewards());
        
        // Winter event
        ScheduleSeasonalEvent(server, reward_service, "Winter Celebration",
            std::chrono::months(12), std::chrono::weeks(4),
            CreateWinterEventRewards());
    }
    
    // [SEQUENCE: 2935] Setup holiday events
    static void SetupHolidayEvents(GameServer* server, RewardService* reward_service) {
        // Valentine's Day
        ScheduleHolidayEvent(server, reward_service, "Love is in the Air",
            2, 14, 7, CreateValentinesRewards());
        
        // Halloween
        ScheduleHolidayEvent(server, reward_service, "Hallow's End",
            10, 31, 10, CreateHalloweenRewards());
        
        // New Year
        ScheduleHolidayEvent(server, reward_service, "New Year Celebration",
            1, 1, 7, CreateNewYearRewards());
    }
    
    // [SEQUENCE: 2936] Setup special promotions
    static void SetupSpecialPromotions(GameServer* server, RewardService* reward_service) {
        // Double XP weekends
        server->ScheduleRecurringTask("double_xp_weekend",
            std::chrono::hours(24 * 7), // Weekly check
            [reward_service]() {
                if (IsFirstWeekendOfMonth()) {
                    ActivateDoubleXPWeekend(reward_service);
                }
            });
        
        // Bonus loot events
        server->ScheduleRecurringTask("bonus_loot_check",
            std::chrono::hours(24), // Daily check
            [reward_service]() {
                if (ShouldActivateBonusLoot()) {
                    ActivateBonusLootEvent(reward_service);
                }
            });
    }
    
    // Helper functions
    static std::string GetArenaTypeName(ArenaType type) {
        // Implementation
        return "Arena";
    }
    
    static Reward CreateMountReward(uint32_t mount_id, const std::string& name) {
        Reward reward;
        reward.type = RewardType::MOUNT;
        reward.mount_id = mount_id;
        reward.display_name = name;
        reward.icon_path = "icons/mounts/" + std::to_string(mount_id) + ".png";
        return reward;
    }
    
    static Reward CreateWeeklyCompletionReward() {
        // Weekly completion chest
        return RewardService::CreateItemReward(30100);
    }
    
    static void GrantWeeklyReward(RewardService* reward_service,
                                uint64_t player_id,
                                const Reward& reward,
                                const std::string& activity_name) {
        RewardSource source;
        source.type = RewardSource::SourceType::MILESTONE;
        source.source_name = "Weekly: " + activity_name;
        source.timestamp = std::chrono::system_clock::now();
        
        reward_service->GrantReward(player_id, reward, source);
    }
    
    // Stub implementations
    struct WeeklyActivities {
        uint32_t completed_count;
        bool claimed_3;
        bool claimed_5;
        bool claimed_7;
    };
    
    static WeeklyActivities GetPlayerWeeklyActivities(uint64_t player_id) {
        return {};
    }
    
    struct Event {
        std::string event_id;
        std::string name;
        bool has_login_reward;
        Reward login_reward;
        bool has_currency_bonus;
        double currency_multiplier;
    };
    
    static std::vector<Event> GetActiveEvents() {
        return {};
    }
    
    static bool HasClaimedEventLogin(uint64_t player_id, const std::string& event_id) {
        return false;
    }
    
    static void MarkEventLoginClaimed(uint64_t player_id, const std::string& event_id) {
        // Implementation
    }
    
    static void ApplyEventCurrencyBonus(uint64_t player_id, const Event& event) {
        // Implementation
    }
    
    struct PendingReward {
        std::string claim_id;
        std::string display_name;
        std::string source_name;
        Reward reward;
        RewardSource source;
        std::vector<ClaimCondition> conditions;
    };
    
    static std::vector<PendingReward> GetPendingRewards(uint64_t player_id) {
        return {};
    }
    
    static std::optional<PendingReward> GetPendingReward(uint64_t player_id,
                                                        const std::string& reward_id) {
        return std::nullopt;
    }
    
    static bool CheckClaimConditions(uint64_t player_id,
                                   const std::vector<ClaimCondition>& conditions) {
        return true;
    }
    
    static void MarkRewardClaimed(uint64_t player_id, const std::string& reward_id) {
        // Implementation
    }
    
    static void ResetWeeklyQuests() {
        // Implementation
    }
    
    static void ResetWeeklyDungeonBonuses() {
        // Implementation
    }
    
    static void ScheduleSeasonalEvent(GameServer* server,
                                    RewardService* reward_service,
                                    const std::string& event_name,
                                    std::chrono::months start_month,
                                    std::chrono::weeks duration,
                                    const std::vector<Reward>& event_rewards) {
        // Implementation
    }
    
    static void ScheduleHolidayEvent(GameServer* server,
                                   RewardService* reward_service,
                                   const std::string& event_name,
                                   int month, int day, int duration_days,
                                   const std::vector<Reward>& event_rewards) {
        // Implementation
    }
    
    static std::vector<Reward> CreateSpringEventRewards() {
        return {};
    }
    
    static std::vector<Reward> CreateSummerEventRewards() {
        return {};
    }
    
    static std::vector<Reward> CreateHarvestEventRewards() {
        return {};
    }
    
    static std::vector<Reward> CreateWinterEventRewards() {
        return {};
    }
    
    static std::vector<Reward> CreateValentinesRewards() {
        return {};
    }
    
    static std::vector<Reward> CreateHalloweenRewards() {
        return {};
    }
    
    static std::vector<Reward> CreateNewYearRewards() {
        return {};
    }
    
    static bool IsFirstWeekendOfMonth() {
        return false;
    }
    
    static void ActivateDoubleXPWeekend(RewardService* reward_service) {
        // Implementation
    }
    
    static bool ShouldActivateBonusLoot() {
        return false;
    }
    
    static void ActivateBonusLootEvent(RewardService* reward_service) {
        // Implementation
    }
    
    // Data structures
    struct Quest {
        std::string quest_id;
        std::string name;
        std::vector<Reward> rewards;
        uint32_t gold_reward;
        bool is_first_completion;
    };
    
    struct Achievement {
        std::string achievement_id;
        std::string name;
        bool has_reward;
        RewardPackage reward_package;
        uint32_t points;
    };
    
    struct ArenaMatchResult {
        uint64_t match_id;
        ArenaType arena_type;
        bool is_winner;
        bool is_mvp;
        uint32_t winner_honor;
        uint32_t loser_honor;
        uint32_t win_streak;
        uint32_t total_wins;
    };
    
    struct DungeonResult {
        std::string dungeon_id;
        std::string dungeon_name;
        uint32_t token_reward;
        uint32_t completion_time;
        uint32_t par_time;
        uint32_t death_count;
        
        struct LootRoll {
            uint32_t item_id;
            ItemQuality quality;
            uint64_t winner_id;
        };
        std::vector<LootRoll> loot_rolls;
    };
    
    struct TournamentResult {
        uint64_t tournament_id;
        std::string tournament_name;
        uint32_t reward_tier;
        std::string champion_title_id;
        
        struct Participant {
            uint64_t player_id;
            uint32_t placement;
            uint32_t matches_played;
        };
        std::vector<Participant> final_standings;
    };
    
    enum class WeeklyResetDay {
        TUESDAY
    };
};

} // namespace mmorpg::rewards