#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <queue>
#include <functional>
#include <chrono>
#include <random>
#include <algorithm>

namespace mmorpg::game::ai {

// [SEQUENCE: 951] 적응형 AI 엔진
class AdaptiveAIEngine {
public:
    struct PlayerBehaviorProfile {
        uint64_t player_id;
        
        // 플레이 스타일 분석
        float aggression_level{0.5f};        // 0.0 (방어적) ~ 1.0 (공격적)
        float skill_level{0.5f};             // 0.0 (초보) ~ 1.0 (전문가)
        float patience_level{0.5f};          // 0.0 (성급) ~ 1.0 (신중)
        float exploration_tendency{0.5f};    // 0.0 (보수적) ~ 1.0 (탐험적)
        
        // 게임 패턴
        std::chrono::seconds average_session_time{3600}; // 1시간 기본
        float preferred_difficulty{0.5f};    // 0.0 (쉬움) ~ 1.0 (어려움)
        std::vector<std::string> preferred_activities;
        std::unordered_map<std::string, int> action_frequencies;
        
        // 학습 데이터
        int total_battles{0};
        int wins{0};
        int losses{0};
        float win_rate{0.0f};
        std::chrono::steady_clock::time_point last_updated;
    };

    struct AIBehaviorConfig {
        std::string behavior_id;
        std::string behavior_name;
        float base_difficulty{0.5f};
        std::vector<std::string> available_actions;
        std::unordered_map<std::string, float> action_weights;
        
        // 적응형 매개변수
        float adaptation_rate{0.1f};         // 얼마나 빨리 적응할지
        float randomness_factor{0.2f};       // 예측 불가능성
        float challenge_scaling{1.0f};       // 도전 레벨 스케일링
    };

    struct AdaptiveDifficulty {
        float current_difficulty{0.5f};
        float target_difficulty{0.5f};
        float adjustment_rate{0.05f};
        std::chrono::steady_clock::time_point last_adjustment;
        
        // 성과 기반 조정
        std::queue<bool> recent_outcomes;    // true=플레이어 승리, false=AI 승리
        size_t outcome_window_size{10};      // 최근 N게임 분석
        float target_win_rate{0.6f};         // 플레이어 목표 승률
    };

    explicit AdaptiveAIEngine(size_t max_profiles = 10000)
        : max_player_profiles_(max_profiles)
        , random_generator_(std::random_device{}()) {
        InitializeDefaultBehaviors();
        StartAnalysisThread();
    }

    ~AdaptiveAIEngine() {
        Shutdown();
    }

    // [SEQUENCE: 952] 플레이어 행동 학습
    void LearnFromPlayerAction(uint64_t player_id, const std::string& action, 
                              const std::unordered_map<std::string, float>& context) {
        std::lock_guard<std::mutex> lock(profiles_mutex_);
        
        auto& profile = GetOrCreateProfile(player_id);
        
        // 액션 빈도 업데이트
        profile.action_frequencies[action]++;
        
        // 컨텍스트 기반 스타일 분석
        UpdatePlayerStyle(profile, action, context);
        
        profile.last_updated = std::chrono::steady_clock::now();
        
        // 실시간 적응 신호
        TriggerAdaptation(player_id);
    }

    // [SEQUENCE: 953] 전투 결과 학습
    void LearnFromBattleOutcome(uint64_t player_id, bool player_won, 
                               float battle_duration_seconds,
                               const std::vector<std::string>& player_actions) {
        std::lock_guard<std::mutex> lock(profiles_mutex_);
        
        auto& profile = GetOrCreateProfile(player_id);
        auto& difficulty = GetOrCreateDifficulty(player_id);
        
        // 전투 통계 업데이트
        profile.total_battles++;
        if (player_won) {
            profile.wins++;
        } else {
            profile.losses++;
        }
        profile.win_rate = static_cast<float>(profile.wins) / profile.total_battles;
        
        // 난이도 조정
        difficulty.recent_outcomes.push(player_won);
        if (difficulty.recent_outcomes.size() > difficulty.outcome_window_size) {
            difficulty.recent_outcomes.pop();
        }
        
        AdjustDifficulty(difficulty);
        
        // 행동 패턴 분석
        AnalyzeBattleActions(profile, player_actions, player_won, battle_duration_seconds);
    }

    // [SEQUENCE: 954] 적응형 AI 행동 생성
    std::string GenerateAIAction(uint64_t player_id, const std::string& situation_context,
                                const std::vector<std::string>& available_actions) {
        std::lock_guard<std::mutex> lock(profiles_mutex_);
        
        auto profile_it = player_profiles_.find(player_id);
        if (profile_it == player_profiles_.end()) {
            // 새 플레이어: 기본 행동
            return SelectRandomAction(available_actions);
        }
        
        const auto& profile = profile_it->second;
        const auto& difficulty = GetDifficulty(player_id);
        
        // 플레이어 스타일에 맞는 AI 행동 선택
        return SelectAdaptiveAction(profile, difficulty, situation_context, available_actions);
    }

    // [SEQUENCE: 955] 동적 난이도 조정
    float GetCurrentDifficulty(uint64_t player_id) {
        std::lock_guard<std::mutex> lock(profiles_mutex_);
        
        auto difficulty_it = player_difficulties_.find(player_id);
        if (difficulty_it != player_difficulties_.end()) {
            return difficulty_it->second.current_difficulty;
        }
        
        return 0.5f; // 기본 난이도
    }

    void SetTargetDifficulty(uint64_t player_id, float target_difficulty) {
        std::lock_guard<std::mutex> lock(profiles_mutex_);
        
        auto& difficulty = GetOrCreateDifficulty(player_id);
        difficulty.target_difficulty = std::clamp(target_difficulty, 0.0f, 1.0f);
    }

    // [SEQUENCE: 956] 플레이어 행동 예측
    struct BehaviorPrediction {
        std::string most_likely_action;
        float confidence;
        std::unordered_map<std::string, float> action_probabilities;
        std::string predicted_strategy;
    };

    BehaviorPrediction PredictPlayerBehavior(uint64_t player_id, 
                                           const std::string& current_situation) {
        std::lock_guard<std::mutex> lock(profiles_mutex_);
        
        BehaviorPrediction prediction;
        
        auto profile_it = player_profiles_.find(player_id);
        if (profile_it == player_profiles_.end()) {
            prediction.confidence = 0.1f;
            prediction.most_likely_action = "unknown";
            return prediction;
        }
        
        const auto& profile = profile_it->second;
        
        // 과거 행동 패턴 기반 예측
        prediction.action_probabilities = CalculateActionProbabilities(profile, current_situation);
        
        // 가장 가능성 높은 행동 선택
        auto max_it = std::max_element(prediction.action_probabilities.begin(),
                                      prediction.action_probabilities.end(),
                                      [](const auto& a, const auto& b) {
                                          return a.second < b.second;
                                      });
        
        if (max_it != prediction.action_probabilities.end()) {
            prediction.most_likely_action = max_it->first;
            prediction.confidence = max_it->second;
        }
        
        // 전략 예측
        prediction.predicted_strategy = PredictStrategy(profile);
        
        return prediction;
    }

    // [SEQUENCE: 957] 프로시저럴 콘텐츠 생성
    struct GeneratedChallenge {
        std::string challenge_id;
        std::string challenge_type;
        float difficulty_level;
        std::unordered_map<std::string, std::variant<int, float, std::string>> parameters;
        std::vector<std::string> required_skills;
        float estimated_duration_minutes;
    };

    GeneratedChallenge GeneratePersonalizedChallenge(uint64_t player_id) {
        std::lock_guard<std::mutex> lock(profiles_mutex_);
        
        auto profile_it = player_profiles_.find(player_id);
        GeneratedChallenge challenge;
        
        if (profile_it == player_profiles_.end()) {
            // 새 플레이어를 위한 기본 챌린지
            challenge = GenerateDefaultChallenge();
        } else {
            const auto& profile = profile_it->second;
            challenge = GenerateAdaptiveChallenge(profile);
        }
        
        challenge.challenge_id = GenerateChallengeId();
        return challenge;
    }

    // [SEQUENCE: 958] AI 성능 분석
    struct AIPerformanceMetrics {
        uint64_t total_interactions{0};
        uint64_t successful_adaptations{0};
        float average_player_satisfaction{0.0f};
        float adaptation_accuracy{0.0f};
        std::unordered_map<std::string, float> behavior_effectiveness;
        std::chrono::steady_clock::time_point last_analysis;
    };

    AIPerformanceMetrics GetPerformanceMetrics() const {
        std::lock_guard<std::mutex> lock(profiles_mutex_);
        
        AIPerformanceMetrics metrics;
        metrics.last_analysis = std::chrono::steady_clock::now();
        
        // 전체 플레이어 프로필 분석
        float total_satisfaction = 0.0f;
        int total_adaptations = 0;
        int successful_adaptations = 0;
        
        for (const auto& [player_id, profile] : player_profiles_) {
            metrics.total_interactions += profile.total_battles;
            
            // 만족도 계산 (승률 기반)
            float satisfaction = CalculatePlayerSatisfaction(profile);
            total_satisfaction += satisfaction;
            
            // 적응 성공률 계산
            if (profile.total_battles > 5) { // 충분한 데이터가 있는 경우
                total_adaptations++;
                if (satisfaction > 0.6f) { // 만족도 임계값
                    successful_adaptations++;
                }
            }
        }
        
        if (!player_profiles_.empty()) {
            metrics.average_player_satisfaction = total_satisfaction / player_profiles_.size();
        }
        
        if (total_adaptations > 0) {
            metrics.adaptation_accuracy = static_cast<float>(successful_adaptations) / total_adaptations;
        }
        
        metrics.successful_adaptations = successful_adaptations;
        
        return metrics;
    }

    void Shutdown() {
        is_running_ = false;
        if (analysis_thread_.joinable()) {
            analysis_thread_.join();
        }
    }

private:
    size_t max_player_profiles_;
    std::unordered_map<uint64_t, PlayerBehaviorProfile> player_profiles_;
    std::unordered_map<uint64_t, AdaptiveDifficulty> player_difficulties_;
    std::vector<AIBehaviorConfig> ai_behaviors_;
    mutable std::mutex profiles_mutex_;
    
    std::atomic<bool> is_running_{true};
    std::thread analysis_thread_;
    std::mt19937 random_generator_;

    // [SEQUENCE: 959] 프로필 관리
    PlayerBehaviorProfile& GetOrCreateProfile(uint64_t player_id) {
        auto it = player_profiles_.find(player_id);
        if (it != player_profiles_.end()) {
            return it->second;
        }
        
        // 메모리 관리: 최대 프로필 수 제한
        if (player_profiles_.size() >= max_player_profiles_) {
            RemoveOldestProfile();
        }
        
        PlayerBehaviorProfile new_profile;
        new_profile.player_id = player_id;
        new_profile.last_updated = std::chrono::steady_clock::now();
        
        return player_profiles_[player_id] = new_profile;
    }

    AdaptiveDifficulty& GetOrCreateDifficulty(uint64_t player_id) {
        auto it = player_difficulties_.find(player_id);
        if (it != player_difficulties_.end()) {
            return it->second;
        }
        
        AdaptiveDifficulty new_difficulty;
        new_difficulty.last_adjustment = std::chrono::steady_clock::now();
        
        return player_difficulties_[player_id] = new_difficulty;
    }

    const AdaptiveDifficulty& GetDifficulty(uint64_t player_id) const {
        static AdaptiveDifficulty default_difficulty;
        
        auto it = player_difficulties_.find(player_id);
        if (it != player_difficulties_.end()) {
            return it->second;
        }
        
        return default_difficulty;
    }

    void RemoveOldestProfile() {
        if (player_profiles_.empty()) return;
        
        auto oldest_it = std::min_element(player_profiles_.begin(), player_profiles_.end(),
            [](const auto& a, const auto& b) {
                return a.second.last_updated < b.second.last_updated;
            });
        
        if (oldest_it != player_profiles_.end()) {
            player_difficulties_.erase(oldest_it->first);
            player_profiles_.erase(oldest_it);
        }
    }

    // [SEQUENCE: 960] 행동 분석
    void UpdatePlayerStyle(PlayerBehaviorProfile& profile, const std::string& action,
                          const std::unordered_map<std::string, float>& context) {
        
        // 컨텍스트 기반 스타일 업데이트
        auto health_it = context.find("health_percentage");
        auto enemy_count_it = context.find("enemy_count");
        auto time_pressure_it = context.find("time_pressure");
        
        if (health_it != context.end()) {
            // 체력에 따른 행동으로 aggression_level 조정
            if (action == "attack" && health_it->second < 0.3f) {
                profile.aggression_level = std::min(1.0f, profile.aggression_level + 0.05f);
            } else if (action == "defend" && health_it->second > 0.7f) {
                profile.aggression_level = std::max(0.0f, profile.aggression_level - 0.05f);
            }
        }
        
        if (enemy_count_it != context.end()) {
            // 적 수에 따른 행동으로 courage 측정
            if (enemy_count_it->second > 1.0f && action == "attack") {
                profile.aggression_level = std::min(1.0f, profile.aggression_level + 0.03f);
            }
        }
        
        if (time_pressure_it != context.end()) {
            // 시간 압박 상황에서의 행동으로 patience_level 조정
            if (time_pressure_it->second > 0.8f && action == "wait") {
                profile.patience_level = std::min(1.0f, profile.patience_level + 0.05f);
            }
        }
        
        // 탐험 성향 업데이트
        if (action == "explore" || action == "investigate") {
            profile.exploration_tendency = std::min(1.0f, profile.exploration_tendency + 0.02f);
        } else if (action == "retreat" || action == "stay") {
            profile.exploration_tendency = std::max(0.0f, profile.exploration_tendency - 0.01f);
        }
    }

    void AnalyzeBattleActions(PlayerBehaviorProfile& profile, 
                             const std::vector<std::string>& actions,
                             bool player_won, float duration_seconds) {
        
        // 스킬 레벨 추정 (전투 시간과 승률 기반)
        float expected_duration = 120.0f; // 2분 예상
        if (duration_seconds < expected_duration * 0.7f && player_won) {
            profile.skill_level = std::min(1.0f, profile.skill_level + 0.02f);
        } else if (duration_seconds > expected_duration * 1.5f && !player_won) {
            profile.skill_level = std::max(0.0f, profile.skill_level - 0.01f);
        }
        
        // 선호 활동 업데이트
        std::unordered_map<std::string, int> action_counts;
        for (const auto& action : actions) {
            action_counts[action]++;
        }
        
        // 가장 많이 사용한 액션을 선호 활동에 추가
        auto max_action = std::max_element(action_counts.begin(), action_counts.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; });
        
        if (max_action != action_counts.end()) {
            auto& preferred = profile.preferred_activities;
            if (std::find(preferred.begin(), preferred.end(), max_action->first) == preferred.end()) {
                preferred.push_back(max_action->first);
                
                // 최대 5개 활동만 추적
                if (preferred.size() > 5) {
                    preferred.erase(preferred.begin());
                }
            }
        }
    }

    // [SEQUENCE: 961] 난이도 조정
    void AdjustDifficulty(AdaptiveDifficulty& difficulty) {
        if (difficulty.recent_outcomes.size() < difficulty.outcome_window_size) {
            return; // 충분한 데이터 없음
        }
        
        // 최근 승률 계산
        int wins = 0;
        std::queue<bool> temp_queue = difficulty.recent_outcomes;
        while (!temp_queue.empty()) {
            if (temp_queue.front()) wins++;
            temp_queue.pop();
        }
        
        float current_win_rate = static_cast<float>(wins) / difficulty.recent_outcomes.size();
        
        // 목표 승률과 비교하여 난이도 조정
        if (current_win_rate > difficulty.target_win_rate + 0.1f) {
            // 플레이어가 너무 쉽게 이기고 있음 -> 난이도 증가
            difficulty.target_difficulty = std::min(1.0f, difficulty.target_difficulty + 0.05f);
        } else if (current_win_rate < difficulty.target_win_rate - 0.1f) {
            // 플레이어가 너무 많이 지고 있음 -> 난이도 감소
            difficulty.target_difficulty = std::max(0.0f, difficulty.target_difficulty - 0.05f);
        }
        
        // 점진적 난이도 조정
        float diff = difficulty.target_difficulty - difficulty.current_difficulty;
        difficulty.current_difficulty += diff * difficulty.adjustment_rate;
        
        difficulty.last_adjustment = std::chrono::steady_clock::now();
    }

    // [SEQUENCE: 962] AI 행동 선택
    std::string SelectAdaptiveAction(const PlayerBehaviorProfile& profile,
                                    const AdaptiveDifficulty& difficulty,
                                    const std::string& context,
                                    const std::vector<std::string>& available_actions) {
        
        if (available_actions.empty()) return "";
        
        // 플레이어 스타일에 대항하는 전략 선택
        std::unordered_map<std::string, float> action_scores;
        
        for (const auto& action : available_actions) {
            float score = CalculateActionScore(action, profile, difficulty, context);
            action_scores[action] = score;
        }
        
        // 난이도에 따른 최적/준최적 행동 선택
        if (difficulty.current_difficulty > 0.8f) {
            // 높은 난이도: 최적 행동
            auto best_action = std::max_element(action_scores.begin(), action_scores.end(),
                [](const auto& a, const auto& b) { return a.second < b.second; });
            return best_action->first;
        } else if (difficulty.current_difficulty < 0.3f) {
            // 낮은 난이도: 의도적으로 덜 최적인 행동
            std::vector<std::pair<std::string, float>> sorted_actions(action_scores.begin(), action_scores.end());
            std::sort(sorted_actions.begin(), sorted_actions.end(),
                [](const auto& a, const auto& b) { return a.second > b.second; });
            
            // 하위 50% 중에서 선택
            size_t start_index = sorted_actions.size() / 2;
            if (start_index >= sorted_actions.size()) start_index = sorted_actions.size() - 1;
            
            std::uniform_int_distribution<size_t> dist(start_index, sorted_actions.size() - 1);
            return sorted_actions[dist(random_generator_)].first;
        } else {
            // 중간 난이도: 확률적 선택
            return SelectWeightedRandomAction(action_scores);
        }
    }

    float CalculateActionScore(const std::string& action, const PlayerBehaviorProfile& profile,
                              const AdaptiveDifficulty& difficulty, const std::string& context) {
        float score = 0.5f; // 기본 점수
        
        // 플레이어 성향에 따른 대응 전략
        if (profile.aggression_level > 0.7f) {
            // 공격적인 플레이어 -> 방어적 행동에 높은 점수
            if (action == "defend" || action == "block" || action == "evade") {
                score += 0.3f;
            }
        } else if (profile.aggression_level < 0.3f) {
            // 방어적인 플레이어 -> 압박적 행동에 높은 점수
            if (action == "attack" || action == "advance" || action == "pursue") {
                score += 0.3f;
            }
        }
        
        // 스킬 레벨에 따른 복잡한 행동
        if (profile.skill_level > 0.6f) {
            if (action.find("combo") != std::string::npos || action.find("advanced") != std::string::npos) {
                score += 0.2f;
            }
        }
        
        // 난이도 기반 조정
        score *= (0.5f + difficulty.current_difficulty * 0.5f);
        
        return std::clamp(score, 0.0f, 1.0f);
    }

    std::string SelectWeightedRandomAction(const std::unordered_map<std::string, float>& action_scores) {
        if (action_scores.empty()) return "";
        
        // 가중치 기반 랜덤 선택
        float total_weight = 0.0f;
        for (const auto& [action, score] : action_scores) {
            total_weight += score;
        }
        
        std::uniform_real_distribution<float> dist(0.0f, total_weight);
        float random_value = dist(random_generator_);
        
        float cumulative_weight = 0.0f;
        for (const auto& [action, score] : action_scores) {
            cumulative_weight += score;
            if (random_value <= cumulative_weight) {
                return action;
            }
        }
        
        return action_scores.begin()->first; // fallback
    }

    std::string SelectRandomAction(const std::vector<std::string>& actions) {
        if (actions.empty()) return "";
        
        std::uniform_int_distribution<size_t> dist(0, actions.size() - 1);
        return actions[dist(random_generator_)];
    }

    // [SEQUENCE: 963] 예측 및 분석
    std::unordered_map<std::string, float> CalculateActionProbabilities(
        const PlayerBehaviorProfile& profile, const std::string& situation) {
        
        std::unordered_map<std::string, float> probabilities;
        
        // 과거 행동 빈도 기반 확률
        int total_actions = 0;
        for (const auto& [action, count] : profile.action_frequencies) {
            total_actions += count;
        }
        
        if (total_actions > 0) {
            for (const auto& [action, count] : profile.action_frequencies) {
                probabilities[action] = static_cast<float>(count) / total_actions;
            }
        }
        
        // 상황별 조정 (시뮬레이션)
        if (situation == "low_health") {
            probabilities["heal"] *= 2.0f;
            probabilities["retreat"] *= 1.5f;
            probabilities["attack"] *= 0.5f;
        } else if (situation == "multiple_enemies") {
            if (profile.aggression_level > 0.6f) {
                probabilities["attack"] *= 1.3f;
            } else {
                probabilities["retreat"] *= 1.5f;
            }
        }
        
        // 정규화
        float total_prob = 0.0f;
        for (const auto& [action, prob] : probabilities) {
            total_prob += prob;
        }
        
        if (total_prob > 0.0f) {
            for (auto& [action, prob] : probabilities) {
                prob /= total_prob;
            }
        }
        
        return probabilities;
    }

    std::string PredictStrategy(const PlayerBehaviorProfile& profile) {
        if (profile.aggression_level > 0.7f && profile.skill_level > 0.6f) {
            return "aggressive_expert";
        } else if (profile.aggression_level < 0.3f && profile.patience_level > 0.6f) {
            return "defensive_patient";
        } else if (profile.exploration_tendency > 0.7f) {
            return "explorer";
        } else if (profile.skill_level < 0.3f) {
            return "beginner_learning";
        } else {
            return "balanced";
        }
    }

    // [SEQUENCE: 964] 콘텐츠 생성
    GeneratedChallenge GenerateDefaultChallenge() {
        GeneratedChallenge challenge;
        challenge.challenge_type = "basic_combat";
        challenge.difficulty_level = 0.3f; // 쉬운 난이도
        challenge.parameters["enemy_count"] = 1;
        challenge.parameters["enemy_health"] = 100.0f;
        challenge.parameters["time_limit"] = 300.0f; // 5분
        challenge.required_skills = {"basic_attack", "movement"};
        challenge.estimated_duration_minutes = 5.0f;
        
        return challenge;
    }

    GeneratedChallenge GenerateAdaptiveChallenge(const PlayerBehaviorProfile& profile) {
        GeneratedChallenge challenge;
        
        // 플레이어 선호도 기반 챌린지 타입 선택
        if (std::find(profile.preferred_activities.begin(), profile.preferred_activities.end(), "explore") 
            != profile.preferred_activities.end()) {
            challenge.challenge_type = "exploration_quest";
            challenge.parameters["area_size"] = 1000.0f;
            challenge.parameters["hidden_items"] = 5;
            challenge.required_skills = {"movement", "investigation"};
        } else if (profile.aggression_level > 0.6f) {
            challenge.challenge_type = "combat_gauntlet";
            challenge.parameters["enemy_count"] = std::max(1, static_cast<int>(profile.skill_level * 5));
            challenge.parameters["enemy_health"] = 80.0f + (profile.skill_level * 120.0f);
            challenge.required_skills = {"combat", "tactics"};
        } else {
            challenge.challenge_type = "puzzle_challenge";
            challenge.parameters["complexity"] = profile.skill_level;
            challenge.parameters["time_pressure"] = 1.0f - profile.patience_level;
            challenge.required_skills = {"logic", "patience"};
        }
        
        challenge.difficulty_level = profile.preferred_difficulty;
        challenge.estimated_duration_minutes = 
            profile.average_session_time.count() / 60.0f * 0.2f; // 세션의 20%
        
        return challenge;
    }

    // [SEQUENCE: 965] 분석 및 유틸리티
    void InitializeDefaultBehaviors() {
        AIBehaviorConfig aggressive;
        aggressive.behavior_id = "aggressive";
        aggressive.behavior_name = "Aggressive Fighter";
        aggressive.base_difficulty = 0.7f;
        aggressive.available_actions = {"attack", "charge", "combo", "pursue"};
        aggressive.action_weights = {{"attack", 0.4f}, {"charge", 0.3f}, {"combo", 0.2f}, {"pursue", 0.1f}};
        
        AIBehaviorConfig defensive;
        defensive.behavior_id = "defensive";
        defensive.behavior_name = "Defensive Tactician";
        defensive.base_difficulty = 0.6f;
        defensive.available_actions = {"defend", "block", "counter", "evade"};
        defensive.action_weights = {{"defend", 0.3f}, {"block", 0.3f}, {"counter", 0.2f}, {"evade", 0.2f}};
        
        AIBehaviorConfig balanced;
        balanced.behavior_id = "balanced";
        balanced.behavior_name = "Balanced Fighter";
        balanced.base_difficulty = 0.5f;
        balanced.available_actions = {"attack", "defend", "move", "special"};
        balanced.action_weights = {{"attack", 0.25f}, {"defend", 0.25f}, {"move", 0.25f}, {"special", 0.25f}};
        
        ai_behaviors_ = {aggressive, defensive, balanced};
    }

    void StartAnalysisThread() {
        analysis_thread_ = std::thread([this]() {
            while (is_running_) {
                PerformBatchAnalysis();
                std::this_thread::sleep_for(std::chrono::minutes(5)); // 5분마다 분석
            }
        });
    }

    void PerformBatchAnalysis() {
        std::lock_guard<std::mutex> lock(profiles_mutex_);
        
        // 오래된 프로필 정리
        auto now = std::chrono::steady_clock::now();
        auto cutoff = now - std::chrono::hours(24); // 24시간 기준
        
        for (auto it = player_profiles_.begin(); it != player_profiles_.end();) {
            if (it->second.last_updated < cutoff) {
                player_difficulties_.erase(it->first);
                it = player_profiles_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void TriggerAdaptation(uint64_t player_id) {
        // 실시간 적응 신호 (필요시 즉시 난이도 조정)
        // 현재는 배치 분석에서 처리
    }

    float CalculatePlayerSatisfaction(const PlayerBehaviorProfile& profile) const {
        // 승률 기반 만족도 계산
        float win_rate_satisfaction = std::abs(profile.win_rate - 0.6f) < 0.1f ? 1.0f : 
                                     1.0f - std::abs(profile.win_rate - 0.6f) * 2.0f;
        
        // 세션 지속 시간 기반 (더 오래 플레이할수록 만족도 높음)
        float duration_satisfaction = std::min(1.0f, profile.average_session_time.count() / 3600.0f);
        
        return (win_rate_satisfaction + duration_satisfaction) / 2.0f;
    }

    std::string GenerateChallengeId() {
        static std::atomic<uint64_t> counter{0};
        return "challenge_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
               "_" + std::to_string(counter.fetch_add(1));
    }
};

} // namespace mmorpg::game::ai