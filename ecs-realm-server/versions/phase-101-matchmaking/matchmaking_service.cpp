#include "matchmaking_service.h"
#include <random>
#include <cmath>

namespace mmorpg::matchmaking {

// [SEQUENCE: 2657] Matchmaking service integration examples
class MatchmakingIntegration {
public:
    // [SEQUENCE: 2658] Initialize matchmaking with game server
    static void InitializeWithGameServer(GameServer* server, 
                                       MatchmakingService* matchmaking) {
        // 매치 생성 시 게임 서버에 알림
        matchmaking->OnMatchCreated = [server](const MatchGroup& match) {
            // 매치 인스턴스 생성
            CreateMatchInstance(server, match);
            
            // 플레이어들에게 매치 정보 전송
            for (const auto& entry : match.players) {
                NotifyMatchFound(server, entry->player->player_id, match);
            }
            
            // 매치 시작 카운트다운
            StartMatchCountdown(server, match.match_id);
        };
        
        // 큐 타임아웃 시 처리
        matchmaking->OnQueueTimeout = [server](const QueueEntry& entry) {
            // 플레이어에게 타임아웃 알림
            server->SendNotification(entry.player->player_id, 
                "Queue timeout - please try again");
            
            // 보상으로 작은 버프 제공 (긴 대기에 대한 보상)
            ApplyQueueCompensation(server, entry.player->player_id);
        };
    }
    
private:
    // [SEQUENCE: 2659] Create match instance on server
    static void CreateMatchInstance(GameServer* server, const MatchGroup& match) {
        // 매치 타입에 따른 맵 선택
        std::string map_name = SelectMapForMatchType(match.match_type);
        
        // 인스턴스 생성
        auto instance = server->CreateInstance(map_name, match.match_id);
        
        // 매치 규칙 설정
        ConfigureMatchRules(instance, match);
        
        // 플레이어 예약
        for (const auto& team : match.teams) {
            for (uint64_t player_id : team.player_ids) {
                instance->ReserveSlot(player_id, team.team_id);
            }
        }
        
        spdlog::info("Match instance created: {} on map {}", 
                    match.match_id, map_name);
    }
    
    // [SEQUENCE: 2660] Notify players about match
    static void NotifyMatchFound(GameServer* server, uint64_t player_id, 
                                const MatchGroup& match) {
        MatchFoundPacket packet;
        packet.match_id = match.match_id;
        packet.match_type = match.match_type;
        
        // 팀 정보 찾기
        for (const auto& team : match.teams) {
            auto it = std::find(team.player_ids.begin(), 
                              team.player_ids.end(), player_id);
            if (it != team.player_ids.end()) {
                packet.team_id = team.team_id;
                packet.teammates = team.player_ids;
                break;
            }
        }
        
        // 상대 팀 정보
        for (const auto& team : match.teams) {
            if (team.team_id != packet.team_id) {
                packet.opponents.insert(packet.opponents.end(),
                                      team.player_ids.begin(),
                                      team.player_ids.end());
            }
        }
        
        // 매치 품질 정보
        packet.match_quality = match.quality_metrics.overall_quality;
        packet.countdown_seconds = 30; // 30초 준비 시간
        
        server->SendPacket(player_id, packet);
    }
    
    // [SEQUENCE: 2661] Select appropriate map
    static std::string SelectMapForMatchType(MatchType type) {
        static const std::unordered_map<MatchType, std::vector<std::string>> maps = {
            {MatchType::ARENA_1V1, {"arena_ring", "arena_pillars", "arena_bridge"}},
            {MatchType::ARENA_2V2, {"arena_courtyard", "arena_ruins"}},
            {MatchType::ARENA_3V3, {"arena_colosseum", "arena_temple"}},
            {MatchType::BATTLEGROUND_10V10, {"bg_capture_the_flag", "bg_domination"}},
            {MatchType::BATTLEGROUND_20V20, {"bg_alterac_valley", "bg_isle_of_conquest"}}
        };
        
        auto it = maps.find(type);
        if (it != maps.end() && !it->second.empty()) {
            // 랜덤하게 맵 선택
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, it->second.size() - 1);
            return it->second[dis(gen)];
        }
        
        return "default_arena";
    }
    
    // [SEQUENCE: 2662] Configure match rules
    static void ConfigureMatchRules(MatchInstance* instance, const MatchGroup& match) {
        // 기본 규칙 설정
        instance->SetTimeLimit(GetMatchDuration(match.match_type));
        instance->SetScoreLimit(GetScoreLimit(match.match_type));
        
        // PvP 규칙
        instance->EnablePvP(true);
        instance->SetRespawnTime(5); // 5초 리스폰
        instance->SetDeathPenalty(false); // 아레나에서는 죽음 페널티 없음
        
        // 랭크 매치 설정
        if (IsRankedMatch(match.match_type)) {
            instance->EnableRatingChanges(true);
            instance->SetMinimumPlayersToStart(match.players.size());
            instance->SetAbandonPenalty(true);
        }
    }
    
    static std::chrono::minutes GetMatchDuration(MatchType type) {
        switch (type) {
            case MatchType::ARENA_1V1:
            case MatchType::ARENA_2V2:
            case MatchType::ARENA_3V3:
                return std::chrono::minutes(10);
            
            case MatchType::BATTLEGROUND_10V10:
                return std::chrono::minutes(20);
            
            case MatchType::BATTLEGROUND_20V20:
                return std::chrono::minutes(30);
            
            default:
                return std::chrono::minutes(15);
        }
    }
    
    static int GetScoreLimit(MatchType type) {
        switch (type) {
            case MatchType::ARENA_1V1:
            case MatchType::ARENA_2V2:
            case MatchType::ARENA_3V3:
                return 1; // First kill wins
            
            case MatchType::BATTLEGROUND_10V10:
                return 3; // Capture 3 flags
            
            case MatchType::BATTLEGROUND_20V20:
                return 1000; // Resource points
            
            default:
                return 10;
        }
    }
    
    static bool IsRankedMatch(MatchType type) {
        return type == MatchType::ARENA_1V1 || 
               type == MatchType::ARENA_2V2 ||
               type == MatchType::ARENA_3V3 ||
               type == MatchType::RANKED_SOLO ||
               type == MatchType::RANKED_TEAM;
    }
    
    // [SEQUENCE: 2663] Apply compensation for long queue times
    static void ApplyQueueCompensation(GameServer* server, uint64_t player_id) {
        // 경험치 부스트 버프 (1시간)
        BuffData buff;
        buff.buff_id = "queue_compensation_xp";
        buff.duration = std::chrono::hours(1);
        buff.effect_type = BuffType::EXPERIENCE_BOOST;
        buff.effect_value = 1.1f; // 10% 경험치 증가
        
        server->ApplyBuff(player_id, buff);
        
        // 소량의 명예 포인트 지급
        server->GrantCurrency(player_id, CurrencyType::HONOR, 50);
        
        spdlog::info("Queue compensation applied to player {}", player_id);
    }
    
    // [SEQUENCE: 2664] Start countdown timer
    static void StartMatchCountdown(GameServer* server, const std::string& match_id) {
        // 30초 카운트다운 시작
        server->ScheduleTask(std::chrono::seconds(30), [server, match_id]() {
            // 모든 플레이어를 매치 인스턴스로 이동
            auto instance = server->GetInstance(match_id);
            if (instance) {
                instance->TeleportAllPlayers();
                instance->StartMatch();
                
                spdlog::info("Match {} started", match_id);
            }
        });
        
        // 10초마다 카운트다운 알림
        for (int i = 20; i >= 10; i -= 10) {
            server->ScheduleTask(std::chrono::seconds(30 - i), 
                [server, match_id, i]() {
                    BroadcastCountdown(server, match_id, i);
                });
        }
    }
    
    static void BroadcastCountdown(GameServer* server, 
                                   const std::string& match_id, 
                                   int seconds) {
        auto instance = server->GetInstance(match_id);
        if (instance) {
            instance->BroadcastMessage("Match starting in " + 
                                      std::to_string(seconds) + " seconds!");
        }
    }
};

// [SEQUENCE: 2665] Rating calculation system (ELO)
class RatingCalculator {
public:
    // [SEQUENCE: 2666] Calculate new ratings after match
    static void CalculateNewRatings(
        const std::vector<MatchmakingProfile>& winners,
        const std::vector<MatchmakingProfile>& losers,
        MatchType match_type,
        std::unordered_map<uint64_t, int32_t>& rating_changes) {
        
        // 팀 평균 레이팅 계산
        double winner_avg_rating = CalculateAverageRating(winners, match_type);
        double loser_avg_rating = CalculateAverageRating(losers, match_type);
        
        // 예상 승률 계산
        double expected_winner = CalculateExpectedScore(winner_avg_rating, 
                                                       loser_avg_rating);
        double expected_loser = 1.0 - expected_winner;
        
        // K-factor (레이팅 변화량)
        const double K = GetKFactor(match_type);
        
        // 승자 레이팅 변화
        for (const auto& player : winners) {
            int32_t change = static_cast<int32_t>(K * (1.0 - expected_winner));
            rating_changes[player.player_id] = change;
        }
        
        // 패자 레이팅 변화
        for (const auto& player : losers) {
            int32_t change = static_cast<int32_t>(K * (0.0 - expected_loser));
            rating_changes[player.player_id] = change;
        }
        
        // 개인 성과에 따른 조정
        ApplyPerformanceAdjustments(rating_changes, match_type);
    }
    
private:
    // [SEQUENCE: 2667] Calculate average team rating
    static double CalculateAverageRating(
        const std::vector<MatchmakingProfile>& team,
        MatchType match_type) {
        
        if (team.empty()) return 1500.0;
        
        double total = 0;
        for (const auto& player : team) {
            auto it = player.ratings.find(match_type);
            if (it != player.ratings.end()) {
                total += it->second.current_rating;
            } else {
                total += 1500; // 기본 레이팅
            }
        }
        
        return total / team.size();
    }
    
    // [SEQUENCE: 2668] ELO expected score formula
    static double CalculateExpectedScore(double rating_a, double rating_b) {
        return 1.0 / (1.0 + std::pow(10.0, (rating_b - rating_a) / 400.0));
    }
    
    // [SEQUENCE: 2669] Get K-factor based on match type
    static double GetKFactor(MatchType match_type) {
        switch (match_type) {
            case MatchType::RANKED_SOLO:
            case MatchType::RANKED_TEAM:
                return 32.0; // 높은 K-factor for 랭크 매치
            
            case MatchType::ARENA_1V1:
            case MatchType::ARENA_2V2:
            case MatchType::ARENA_3V3:
                return 24.0; // 중간 K-factor for 아레나
            
            default:
                return 16.0; // 낮은 K-factor for 일반 매치
        }
    }
    
    // [SEQUENCE: 2670] Apply individual performance adjustments
    static void ApplyPerformanceAdjustments(
        std::unordered_map<uint64_t, int32_t>& rating_changes,
        MatchType match_type) {
        
        // 개인 성과 지표에 따라 레이팅 변화 조정
        // 예: MVP는 추가 포인트, 최하위 성과는 감소
        
        // 이 부분은 실제 매치 통계가 필요하므로 
        // 여기서는 간단한 예시만 제공
        
        for (auto& [player_id, change] : rating_changes) {
            // MVP 보너스 (예시)
            if (IsMVP(player_id)) {
                change = static_cast<int32_t>(change * 1.2);
            }
            
            // 최소/최대 변화량 제한
            change = std::clamp(change, -50, 50);
        }
    }
    
    static bool IsMVP(uint64_t player_id) {
        // 실제로는 매치 통계를 확인해야 함
        return false;
    }
};

// [SEQUENCE: 2671] Matchmaking algorithm improvements
class AdvancedMatchmaking {
public:
    // [SEQUENCE: 2672] Role-based team composition
    struct RoleRequirements {
        int tanks{1};
        int healers{1};
        int damage_dealers{3};
        
        bool IsSatisfied(const std::vector<PlayerRole>& roles) const {
            int tank_count = 0, healer_count = 0, dd_count = 0;
            
            for (auto role : roles) {
                switch (role) {
                    case PlayerRole::TANK: tank_count++; break;
                    case PlayerRole::HEALER: healer_count++; break;
                    case PlayerRole::DAMAGE: dd_count++; break;
                }
            }
            
            return tank_count >= tanks && 
                   healer_count >= healers && 
                   dd_count >= damage_dealers;
        }
    };
    
    // [SEQUENCE: 2673] Premade group handling
    static bool CanMatchPremadeGroups(
        const std::vector<uint64_t>& group1,
        const std::vector<uint64_t>& group2,
        const MatchRequirements& requirements) {
        
        // 프리메이드 그룹 크기 제한
        if (group1.size() > 3 || group2.size() > 3) {
            // 큰 프리메이드는 비슷한 크기끼리만 매칭
            return std::abs(static_cast<int>(group1.size() - group2.size())) <= 1;
        }
        
        return true;
    }
    
    // [SEQUENCE: 2674] Fair match prediction
    static double PredictMatchFairness(const MatchGroup& match) {
        // 여러 요소를 고려한 공정성 예측
        double fairness = 1.0;
        
        // 레이팅 밸런스
        fairness *= match.quality_metrics.rating_balance;
        
        // 프리메이드 vs 솔로 불이익
        double premade_penalty = CalculatePremadePenalty(match);
        fairness *= (1.0 - premade_penalty);
        
        // 연승/연패 보정
        double streak_adjustment = CalculateStreakAdjustment(match);
        fairness *= streak_adjustment;
        
        return fairness;
    }
    
private:
    static double CalculatePremadePenalty(const MatchGroup& match) {
        // 한 팀은 프리메이드, 다른 팀은 솔로인 경우 페널티
        // 실제 구현에서는 더 정교한 계산 필요
        return 0.0;
    }
    
    static double CalculateStreakAdjustment(const MatchGroup& match) {
        // 연승/연패 중인 플레이어들을 고려한 조정
        return 1.0;
    }
};

// [SEQUENCE: 2675] Matchmaking analytics
class MatchmakingAnalytics {
public:
    // [SEQUENCE: 2676] Track queue abandonment
    struct AbandonmentData {
        uint64_t player_id;
        MatchType match_type;
        std::chrono::seconds wait_time;
        std::string reason; // "manual", "timeout", "disconnect"
    };
    
    void RecordAbandonment(const AbandonmentData& data) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        abandonments_.push_back(data);
        
        // 매치 타입별 포기율 계산
        abandonment_rates_[data.match_type]++;
        
        // 대기 시간별 포기율 분석
        int wait_bucket = data.wait_time.count() / 30; // 30초 단위
        wait_time_abandonments_[wait_bucket]++;
    }
    
    // [SEQUENCE: 2677] Analyze match quality over time
    void AnalyzeMatchQuality(const MatchGroup& match) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 시간대별 매치 품질 추적
        auto hour = GetCurrentHour();
        hourly_quality_[hour].push_back(match.quality_metrics.overall_quality);
        
        // 이동 평균 계산
        if (hourly_quality_[hour].size() > 100) {
            hourly_quality_[hour].pop_front();
        }
        
        // 품질 저하 경고
        double avg_quality = CalculateAverageQuality(hour);
        if (avg_quality < 0.5) {
            spdlog::warn("Low match quality detected at hour {}: {:.2f}", 
                        hour, avg_quality);
        }
    }
    
    // [SEQUENCE: 2678] Generate recommendations
    struct Recommendations {
        std::vector<std::string> suggestions;
        
        void AddSuggestion(const std::string& suggestion) {
            suggestions.push_back(suggestion);
        }
    };
    
    Recommendations GenerateRecommendations() {
        std::lock_guard<std::mutex> lock(mutex_);
        Recommendations rec;
        
        // 포기율이 높은 매치 타입 확인
        for (const auto& [type, count] : abandonment_rates_) {
            if (count > 100) { // 임계값
                rec.AddSuggestion("High abandonment rate for " + 
                                std::to_string(static_cast<int>(type)) + 
                                ". Consider adjusting queue parameters.");
            }
        }
        
        // 특정 시간대 품질 문제
        for (int hour = 0; hour < 24; ++hour) {
            double quality = CalculateAverageQuality(hour);
            if (quality < 0.5) {
                rec.AddSuggestion("Low match quality at hour " + 
                                std::to_string(hour) + 
                                ". Consider relaxing constraints.");
            }
        }
        
        return rec;
    }
    
private:
    std::mutex mutex_;
    std::vector<AbandonmentData> abandonments_;
    std::unordered_map<MatchType, uint64_t> abandonment_rates_;
    std::unordered_map<int, uint64_t> wait_time_abandonments_;
    std::array<std::deque<double>, 24> hourly_quality_;
    
    int GetCurrentHour() {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm* tm = std::localtime(&time_t);
        return tm->tm_hour;
    }
    
    double CalculateAverageQuality(int hour) {
        if (hourly_quality_[hour].empty()) return 1.0;
        
        double sum = 0;
        for (double quality : hourly_quality_[hour]) {
            sum += quality;
        }
        
        return sum / hourly_quality_[hour].size();
    }
};

} // namespace mmorpg::matchmaking