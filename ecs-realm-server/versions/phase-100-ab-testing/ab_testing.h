#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <random>
#include <atomic>
#include <mutex>
#include <chrono>
#include <functional>
#include <optional>
#include <json/json.h>
#include <spdlog/spdlog.h>

namespace mmorpg::monitoring {

// [SEQUENCE: 2576] A/B testing framework for game feature experimentation
// A/B 테스트 프레임워크 - 게임 기능 실험 및 최적화

// [SEQUENCE: 2577] Experiment variant definition
struct Variant {
    std::string name;                      // 변종 이름 ("control", "variant_a" 등)
    double allocation_percentage;          // 할당 비율 (0-100)
    Json::Value parameters;                // 변종별 파라미터
    
    // [SEQUENCE: 2578] Variant metrics tracking
    struct Metrics {
        std::atomic<uint64_t> player_count{0};        // 참여 플레이어 수
        std::atomic<uint64_t> session_count{0};       // 세션 수
        std::atomic<double> total_playtime{0.0};      // 총 플레이 시간
        std::atomic<double> total_revenue{0.0};       // 총 수익
        std::atomic<uint64_t> conversion_count{0};    // 전환 수
        std::atomic<uint64_t> event_count{0};         // 이벤트 발생 수
        
        // Custom metrics
        std::unordered_map<std::string, std::atomic<double>> custom_metrics;
    } metrics;
};

// [SEQUENCE: 2579] Experiment configuration
struct ExperimentConfig {
    std::string id;                        // 실험 ID
    std::string name;                      // 실험 이름
    std::string description;               // 실험 설명
    std::vector<Variant> variants;         // 실험 변종들
    
    // [SEQUENCE: 2580] Targeting criteria
    struct TargetingCriteria {
        std::optional<int> min_level;                 // 최소 레벨
        std::optional<int> max_level;                 // 최대 레벨
        std::optional<std::string> region;            // 지역
        std::optional<std::string> platform;          // 플랫폼 (PC/Mobile)
        std::optional<double> sample_rate;            // 샘플링 비율
        std::vector<std::string> required_features;   // 필수 기능
        
        bool Matches(const PlayerProfile& player) const;
    } targeting;
    
    // [SEQUENCE: 2581] Experiment schedule
    struct Schedule {
        std::chrono::system_clock::time_point start_time;
        std::chrono::system_clock::time_point end_time;
        bool is_active{true};
        
        bool IsRunning() const {
            auto now = std::chrono::system_clock::now();
            return is_active && now >= start_time && now <= end_time;
        }
    } schedule;
    
    // [SEQUENCE: 2582] Success metrics definition
    struct SuccessMetrics {
        std::string primary_metric;            // 주요 성공 지표
        std::vector<std::string> secondary_metrics; // 보조 지표
        double minimum_sample_size{1000};      // 최소 샘플 크기
        double confidence_level{0.95};         // 신뢰 수준
    } success_metrics;
};

// [SEQUENCE: 2583] Player profile for targeting
struct PlayerProfile {
    uint64_t player_id;
    int level;
    std::string region;
    std::string platform;
    std::chrono::system_clock::time_point registration_date;
    double lifetime_value;
    std::vector<std::string> enabled_features;
};

// [SEQUENCE: 2584] A/B test assignment
struct TestAssignment {
    std::string experiment_id;
    std::string variant_name;
    std::chrono::system_clock::time_point assignment_time;
    Json::Value parameters;                    // 적용할 파라미터
};

// [SEQUENCE: 2585] A/B testing service
class ABTestingService {
public:
    ABTestingService() : rng_(std::random_device{}()) {}
    
    // [SEQUENCE: 2586] Load experiments from configuration
    void LoadExperiments(const std::string& config_file) {
        Json::Value root;
        std::ifstream file(config_file);
        file >> root;
        
        std::lock_guard<std::shared_mutex> lock(experiments_mutex_);
        experiments_.clear();
        
        for (const auto& exp_json : root["experiments"]) {
            auto experiment = ParseExperiment(exp_json);
            experiments_[experiment.id] = experiment;
            spdlog::info("Loaded A/B test experiment: {} with {} variants", 
                        experiment.name, experiment.variants.size());
        }
    }
    
    // [SEQUENCE: 2587] Get player's test assignments
    std::vector<TestAssignment> GetPlayerAssignments(const PlayerProfile& player) {
        std::vector<TestAssignment> assignments;
        std::shared_lock<std::shared_mutex> lock(experiments_mutex_);
        
        for (const auto& [exp_id, experiment] : experiments_) {
            // Check if experiment is running
            if (!experiment.schedule.IsRunning()) {
                continue;
            }
            
            // Check targeting criteria
            if (!experiment.targeting.Matches(player)) {
                continue;
            }
            
            // Get or create assignment
            auto assignment = GetOrCreateAssignment(player.player_id, experiment);
            if (assignment.has_value()) {
                assignments.push_back(assignment.value());
            }
        }
        
        return assignments;
    }
    
    // [SEQUENCE: 2588] Track experiment event
    void TrackEvent(uint64_t player_id, const std::string& experiment_id, 
                   const std::string& event_name, double value = 1.0) {
        std::shared_lock<std::shared_mutex> exp_lock(experiments_mutex_);
        
        auto exp_it = experiments_.find(experiment_id);
        if (exp_it == experiments_.end()) {
            return;
        }
        
        // Find player's variant
        std::shared_lock<std::shared_mutex> assign_lock(assignments_mutex_);
        auto key = std::make_pair(player_id, experiment_id);
        auto assign_it = player_assignments_.find(key);
        
        if (assign_it != player_assignments_.end()) {
            const auto& variant_name = assign_it->second.variant_name;
            
            // Update metrics
            for (auto& variant : exp_it->second.variants) {
                if (variant.name == variant_name) {
                    variant.metrics.event_count.fetch_add(1);
                    
                    // Update custom metric
                    auto& custom = variant.metrics.custom_metrics[event_name];
                    custom.fetch_add(value);
                    
                    // Log significant events
                    if (event_name == "conversion" || event_name == "purchase") {
                        spdlog::info("A/B test event: player={}, experiment={}, variant={}, event={}, value={}",
                                    player_id, experiment_id, variant_name, event_name, value);
                    }
                    
                    break;
                }
            }
        }
    }
    
    // [SEQUENCE: 2589] Get experiment results
    Json::Value GetExperimentResults(const std::string& experiment_id) {
        std::shared_lock<std::shared_mutex> lock(experiments_mutex_);
        
        auto it = experiments_.find(experiment_id);
        if (it == experiments_.end()) {
            return Json::Value::null;
        }
        
        const auto& experiment = it->second;
        Json::Value results;
        
        results["experiment_id"] = experiment_id;
        results["experiment_name"] = experiment.name;
        results["status"] = experiment.schedule.IsRunning() ? "running" : "completed";
        
        // [SEQUENCE: 2590] Calculate statistics for each variant
        Json::Value variants_results;
        for (const auto& variant : experiment.variants) {
            Json::Value variant_result;
            variant_result["name"] = variant.name;
            variant_result["allocation"] = variant.allocation_percentage;
            
            // Basic metrics
            variant_result["player_count"] = static_cast<Json::UInt64>(variant.metrics.player_count.load());
            variant_result["session_count"] = static_cast<Json::UInt64>(variant.metrics.session_count.load());
            variant_result["total_playtime"] = variant.metrics.total_playtime.load();
            variant_result["total_revenue"] = variant.metrics.total_revenue.load();
            variant_result["conversion_count"] = static_cast<Json::UInt64>(variant.metrics.conversion_count.load());
            
            // Calculate rates
            uint64_t players = variant.metrics.player_count.load();
            if (players > 0) {
                variant_result["conversion_rate"] = 
                    static_cast<double>(variant.metrics.conversion_count.load()) / players;
                variant_result["avg_playtime"] = 
                    variant.metrics.total_playtime.load() / players;
                variant_result["avg_revenue"] = 
                    variant.metrics.total_revenue.load() / players;
            }
            
            // Custom metrics
            Json::Value custom_metrics;
            for (const auto& [name, value] : variant.metrics.custom_metrics) {
                custom_metrics[name] = value.load();
            }
            variant_result["custom_metrics"] = custom_metrics;
            
            variants_results.append(variant_result);
        }
        
        results["variants"] = variants_results;
        
        // [SEQUENCE: 2591] Statistical significance testing
        if (experiment.variants.size() == 2) {
            results["statistical_analysis"] = 
                PerformStatisticalAnalysis(experiment.variants[0], experiment.variants[1]);
        }
        
        return results;
    }
    
    // [SEQUENCE: 2592] Update player session metrics
    void UpdateSessionMetrics(uint64_t player_id, double session_duration, 
                             double session_revenue) {
        std::shared_lock<std::shared_mutex> lock(assignments_mutex_);
        
        // Update metrics for all player's experiments
        for (const auto& [key, assignment] : player_assignments_) {
            if (key.first == player_id) {
                UpdateVariantMetrics(assignment.experiment_id, assignment.variant_name,
                                   session_duration, session_revenue);
            }
        }
    }
    
private:
    std::unordered_map<std::string, ExperimentConfig> experiments_;
    std::map<std::pair<uint64_t, std::string>, TestAssignment> player_assignments_;
    
    mutable std::shared_mutex experiments_mutex_;
    mutable std::shared_mutex assignments_mutex_;
    
    std::mt19937 rng_;
    
    // [SEQUENCE: 2593] Parse experiment from JSON
    ExperimentConfig ParseExperiment(const Json::Value& json) {
        ExperimentConfig config;
        
        config.id = json["id"].asString();
        config.name = json["name"].asString();
        config.description = json["description"].asString();
        
        // Parse variants
        double total_allocation = 0;
        for (const auto& var_json : json["variants"]) {
            Variant variant;
            variant.name = var_json["name"].asString();
            variant.allocation_percentage = var_json["allocation"].asDouble();
            variant.parameters = var_json["parameters"];
            
            total_allocation += variant.allocation_percentage;
            config.variants.push_back(variant);
        }
        
        // Validate allocation adds up to 100%
        if (std::abs(total_allocation - 100.0) > 0.01) {
            spdlog::warn("Experiment {} allocation doesn't sum to 100%: {}", 
                        config.id, total_allocation);
        }
        
        // Parse targeting
        if (json.isMember("targeting")) {
            const auto& targeting = json["targeting"];
            if (targeting.isMember("min_level"))
                config.targeting.min_level = targeting["min_level"].asInt();
            if (targeting.isMember("max_level"))
                config.targeting.max_level = targeting["max_level"].asInt();
            if (targeting.isMember("region"))
                config.targeting.region = targeting["region"].asString();
            if (targeting.isMember("sample_rate"))
                config.targeting.sample_rate = targeting["sample_rate"].asDouble();
        }
        
        // Parse schedule
        auto parse_time = [](const std::string& str) {
            std::tm tm = {};
            std::istringstream ss(str);
            ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
            return std::chrono::system_clock::from_time_t(std::mktime(&tm));
        };
        
        config.schedule.start_time = parse_time(json["start_time"].asString());
        config.schedule.end_time = parse_time(json["end_time"].asString());
        config.schedule.is_active = json["is_active"].asBool();
        
        return config;
    }
    
    // [SEQUENCE: 2594] Get or create assignment for player
    std::optional<TestAssignment> GetOrCreateAssignment(uint64_t player_id, 
                                                       const ExperimentConfig& experiment) {
        auto key = std::make_pair(player_id, experiment.id);
        
        // Check existing assignment
        {
            std::shared_lock<std::shared_mutex> lock(assignments_mutex_);
            auto it = player_assignments_.find(key);
            if (it != player_assignments_.end()) {
                return it->second;
            }
        }
        
        // Create new assignment
        std::unique_lock<std::shared_mutex> lock(assignments_mutex_);
        
        // Double-check after acquiring write lock
        auto it = player_assignments_.find(key);
        if (it != player_assignments_.end()) {
            return it->second;
        }
        
        // Assign to variant based on allocation
        auto variant_opt = AssignToVariant(player_id, experiment);
        if (!variant_opt.has_value()) {
            return std::nullopt;
        }
        
        const auto& variant = variant_opt.value();
        
        TestAssignment assignment;
        assignment.experiment_id = experiment.id;
        assignment.variant_name = variant.name;
        assignment.assignment_time = std::chrono::system_clock::now();
        assignment.parameters = variant.parameters;
        
        player_assignments_[key] = assignment;
        
        // Update variant metrics
        for (auto& var : experiments_[experiment.id].variants) {
            if (var.name == variant.name) {
                var.metrics.player_count.fetch_add(1);
                break;
            }
        }
        
        spdlog::debug("Assigned player {} to experiment {} variant {}",
                     player_id, experiment.id, variant.name);
        
        return assignment;
    }
    
    // [SEQUENCE: 2595] Assign player to variant
    std::optional<std::reference_wrapper<const Variant>> AssignToVariant(
        uint64_t player_id, const ExperimentConfig& experiment) {
        
        // Use consistent hashing for assignment
        std::string hash_input = std::to_string(player_id) + experiment.id;
        std::hash<std::string> hasher;
        size_t hash_value = hasher(hash_input);
        
        // Convert to percentage (0-100)
        double assignment_value = (hash_value % 10000) / 100.0;
        
        // Find variant based on allocation
        double cumulative = 0;
        for (const auto& variant : experiment.variants) {
            cumulative += variant.allocation_percentage;
            if (assignment_value < cumulative) {
                return std::cref(variant);
            }
        }
        
        // Shouldn't reach here if allocations sum to 100%
        return std::nullopt;
    }
    
    // [SEQUENCE: 2596] Update variant metrics
    void UpdateVariantMetrics(const std::string& experiment_id, 
                             const std::string& variant_name,
                             double session_duration, 
                             double session_revenue) {
        std::shared_lock<std::shared_mutex> lock(experiments_mutex_);
        
        auto exp_it = experiments_.find(experiment_id);
        if (exp_it != experiments_.end()) {
            for (auto& variant : exp_it->second.variants) {
                if (variant.name == variant_name) {
                    variant.metrics.session_count.fetch_add(1);
                    
                    // Atomic addition for doubles
                    double prev_playtime = variant.metrics.total_playtime.load();
                    while (!variant.metrics.total_playtime.compare_exchange_weak(
                        prev_playtime, prev_playtime + session_duration));
                    
                    double prev_revenue = variant.metrics.total_revenue.load();
                    while (!variant.metrics.total_revenue.compare_exchange_weak(
                        prev_revenue, prev_revenue + session_revenue));
                    
                    if (session_revenue > 0) {
                        variant.metrics.conversion_count.fetch_add(1);
                    }
                    
                    break;
                }
            }
        }
    }
    
    // [SEQUENCE: 2597] Perform statistical analysis
    Json::Value PerformStatisticalAnalysis(const Variant& control, 
                                          const Variant& treatment) {
        Json::Value analysis;
        
        // Calculate conversion rate lift
        uint64_t control_players = control.metrics.player_count.load();
        uint64_t treatment_players = treatment.metrics.player_count.load();
        
        if (control_players > 0 && treatment_players > 0) {
            double control_conversion = 
                static_cast<double>(control.metrics.conversion_count.load()) / control_players;
            double treatment_conversion = 
                static_cast<double>(treatment.metrics.conversion_count.load()) / treatment_players;
            
            double lift = (treatment_conversion - control_conversion) / control_conversion * 100;
            analysis["conversion_lift_percentage"] = lift;
            
            // Calculate statistical significance (simplified)
            double z_score = CalculateZScore(control_conversion, control_players,
                                           treatment_conversion, treatment_players);
            double p_value = CalculatePValue(z_score);
            
            analysis["z_score"] = z_score;
            analysis["p_value"] = p_value;
            analysis["is_significant"] = (p_value < 0.05);
            
            // Revenue analysis
            double control_arpu = control.metrics.total_revenue.load() / control_players;
            double treatment_arpu = treatment.metrics.total_revenue.load() / treatment_players;
            double revenue_lift = (treatment_arpu - control_arpu) / control_arpu * 100;
            
            analysis["revenue_lift_percentage"] = revenue_lift;
            analysis["control_arpu"] = control_arpu;
            analysis["treatment_arpu"] = treatment_arpu;
        }
        
        return analysis;
    }
    
    // [SEQUENCE: 2598] Calculate Z-score for proportion test
    double CalculateZScore(double p1, uint64_t n1, double p2, uint64_t n2) {
        double p_pooled = (p1 * n1 + p2 * n2) / (n1 + n2);
        double se = std::sqrt(p_pooled * (1 - p_pooled) * (1.0/n1 + 1.0/n2));
        return (p2 - p1) / se;
    }
    
    // [SEQUENCE: 2599] Calculate P-value from Z-score
    double CalculatePValue(double z_score) {
        // Simplified normal CDF calculation
        // In production, use proper statistical library
        return 2 * (1 - 0.5 * std::erfc(-std::abs(z_score) / std::sqrt(2)));
    }
};

// [SEQUENCE: 2600] A/B test configuration manager
class ABTestConfigManager {
public:
    // [SEQUENCE: 2601] Create experiment configuration
    static Json::Value CreateExperimentConfig(
        const std::string& id,
        const std::string& name,
        const std::vector<std::pair<std::string, double>>& variants,
        const Json::Value& parameters) {
        
        Json::Value config;
        config["id"] = id;
        config["name"] = name;
        config["description"] = "";
        config["is_active"] = true;
        
        // Set default schedule (30 days)
        auto now = std::chrono::system_clock::now();
        auto end = now + std::chrono::hours(24 * 30);
        
        config["start_time"] = FormatTime(now);
        config["end_time"] = FormatTime(end);
        
        // Create variants
        Json::Value variants_json;
        for (const auto& [variant_name, allocation] : variants) {
            Json::Value variant;
            variant["name"] = variant_name;
            variant["allocation"] = allocation;
            variant["parameters"] = parameters;
            variants_json.append(variant);
        }
        config["variants"] = variants_json;
        
        return config;
    }
    
    // [SEQUENCE: 2602] Common experiment templates
    static Json::Value CreateFeatureFlagExperiment(
        const std::string& feature_name,
        double rollout_percentage) {
        
        Json::Value params;
        params[feature_name + "_enabled"] = true;
        
        return CreateExperimentConfig(
            "feature_" + feature_name,
            "Feature Flag: " + feature_name,
            {{"control", 100 - rollout_percentage}, {"enabled", rollout_percentage}},
            params
        );
    }
    
    static Json::Value CreateParameterTuningExperiment(
        const std::string& parameter_name,
        const std::vector<double>& values) {
        
        std::vector<std::pair<std::string, double>> variants;
        double allocation = 100.0 / values.size();
        
        for (size_t i = 0; i < values.size(); ++i) {
            std::string variant_name = "variant_" + std::to_string(i);
            variants.push_back({variant_name, allocation});
        }
        
        Json::Value params;
        // Parameters will be set per variant
        
        return CreateExperimentConfig(
            "param_" + parameter_name,
            "Parameter Tuning: " + parameter_name,
            variants,
            params
        );
    }
    
private:
    static std::string FormatTime(const std::chrono::system_clock::time_point& tp) {
        auto time_t = std::chrono::system_clock::to_time_t(tp);
        char buffer[64];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&time_t));
        return buffer;
    }
};

// [SEQUENCE: 2603] Targeting criteria implementation
bool ExperimentConfig::TargetingCriteria::Matches(const PlayerProfile& player) const {
    // Level check
    if (min_level.has_value() && player.level < min_level.value()) {
        return false;
    }
    if (max_level.has_value() && player.level > max_level.value()) {
        return false;
    }
    
    // Region check
    if (region.has_value() && player.region != region.value()) {
        return false;
    }
    
    // Platform check
    if (platform.has_value() && player.platform != platform.value()) {
        return false;
    }
    
    // Feature requirements
    for (const auto& required : required_features) {
        if (std::find(player.enabled_features.begin(), 
                     player.enabled_features.end(), 
                     required) == player.enabled_features.end()) {
            return false;
        }
    }
    
    // Sample rate check
    if (sample_rate.has_value()) {
        // Use player ID for consistent sampling
        std::hash<uint64_t> hasher;
        double hash_value = (hasher(player.player_id) % 10000) / 10000.0;
        if (hash_value > sample_rate.value()) {
            return false;
        }
    }
    
    return true;
}

} // namespace mmorpg::monitoring