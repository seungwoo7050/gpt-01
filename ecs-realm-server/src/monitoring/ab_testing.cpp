#include "ab_testing.h"
#include <fstream>
#include <cmath>

namespace mmorpg::monitoring {

// [SEQUENCE: MVP14-652] A/B testing integration example
class ABTestingIntegration {
public:
    // [SEQUENCE: MVP14-653] Initialize A/B testing with game server
    static void InitializeWithGameServer(GameServer* server, 
                                       ABTestingService* ab_service) {
        // Load experiment configurations
        ab_service->LoadExperiments("config/experiments.json");
        
        // [SEQUENCE: MVP14-654] Hook into player login
        server->RegisterLoginHandler([ab_service, server](uint64_t player_id) {
            // Build player profile
            PlayerProfile profile;
            profile.player_id = player_id;
            
            auto player = server->GetPlayer(player_id);
            if (player) {
                profile.level = player->GetLevel();
                profile.region = player->GetRegion();
                profile.platform = player->GetPlatform();
                profile.registration_date = player->GetRegistrationDate();
                profile.lifetime_value = player->GetLifetimeValue();
                profile.enabled_features = player->GetEnabledFeatures();
                
                // Get test assignments
                auto assignments = ab_service->GetPlayerAssignments(profile);
                
                // [SEQUENCE: MVP14-655] Apply experiment parameters
                for (const auto& assignment : assignments) {
                    ApplyExperimentParameters(player, assignment);
                    
                    spdlog::info("Player {} assigned to experiment {} variant {}",
                                player_id, assignment.experiment_id, 
                                assignment.variant_name);
                }
                
                // Store assignments for session tracking
                player->SetTestAssignments(assignments);
            }
        });
        
        // [SEQUENCE: MVP14-656] Hook into session end
        server->RegisterLogoutHandler([ab_service](uint64_t player_id, 
                                                  const SessionStats& stats) {
            ab_service->UpdateSessionMetrics(player_id, 
                                           stats.duration_seconds,
                                           stats.revenue);
        });
        
        // [SEQUENCE: MVP14-657] Hook into game events
        server->RegisterEventHandler([ab_service](const GameEvent& event) {
            // Track relevant events for experiments
            if (IsExperimentRelevantEvent(event)) {
                ab_service->TrackEvent(event.player_id, 
                                     event.experiment_id,
                                     event.event_name,
                                     event.value);
            }
        });
    }
    
private:
    // [SEQUENCE: MVP14-658] Apply experiment parameters to player
    static void ApplyExperimentParameters(Player* player, 
                                        const TestAssignment& assignment) {
        const auto& params = assignment.parameters;
        
        // Feature flags
        if (params.isMember("new_combat_system_enabled")) {
            player->SetFeatureEnabled("new_combat_system", 
                                    params["new_combat_system_enabled"].asBool());
        }
        
        // Parameter tuning
        if (params.isMember("xp_multiplier")) {
            player->SetXPMultiplier(params["xp_multiplier"].asDouble());
        }
        
        if (params.isMember("drop_rate_multiplier")) {
            player->SetDropRateMultiplier(params["drop_rate_multiplier"].asDouble());
        }
        
        // UI variations
        if (params.isMember("ui_theme")) {
            player->SetUITheme(params["ui_theme"].asString());
        }
        
        // Game mechanics
        if (params.isMember("skill_cooldown_reduction")) {
            player->SetSkillCooldownReduction(
                params["skill_cooldown_reduction"].asDouble());
        }
    }
    
    // [SEQUENCE: MVP14-659] Check if event is relevant for experiments
    static bool IsExperimentRelevantEvent(const GameEvent& event) {
        // Events to track for A/B testing
        static const std::set<std::string> relevant_events = {
            "player_login",
            "player_logout", 
            "level_up",
            "purchase_completed",
            "quest_completed",
            "pvp_match_won",
            "guild_joined",
            "premium_upgrade",
            "tutorial_completed",
            "first_purchase",
            "retention_day_1",
            "retention_day_7",
            "retention_day_30"
        };
        
        return relevant_events.count(event.event_name) > 0;
    }
};

// [SEQUENCE: MVP14-660] Real-time experiment monitoring
class ExperimentMonitor {
public:
    // [SEQUENCE: MVP14-661] Monitor experiment health
    static void MonitorExperiments(ABTestingService* ab_service) {
        std::thread monitor_thread([ab_service]() {
            while (true) {
                // Check every 5 minutes
                std::this_thread::sleep_for(std::chrono::minutes(5));
                
                // Get all active experiments
                Json::Value all_experiments;
                // Would need to add method to get all experiment IDs
                
                for (const auto& exp_id : GetActiveExperimentIds(ab_service)) {
                    auto results = ab_service->GetExperimentResults(exp_id);
                    
                    // [SEQUENCE: MVP14-662] Check for sample ratio mismatch
                    CheckSampleRatioMismatch(results);
                    
                    // [SEQUENCE: MVP14-663] Check for metric anomalies
                    CheckMetricAnomalies(results);
                    
                    // [SEQUENCE: MVP14-664] Auto-stop harmful experiments
                    if (IsExperimentHarmful(results)) {
                        spdlog::error("Experiment {} showing harmful effects, stopping",
                                    exp_id);
                        // Would need to add method to stop experiment
                        // ab_service->StopExperiment(exp_id);
                    }
                }
            }
        });
        
        monitor_thread.detach();
    }
    
private:
    // [SEQUENCE: MVP14-665] Check sample ratio mismatch (SRM)
    static void CheckSampleRatioMismatch(const Json::Value& results) {
        if (!results.isMember("variants")) return;
        
        const auto& variants = results["variants"];
        if (variants.size() < 2) return;
        
        // Calculate expected vs actual ratios
        std::vector<double> expected_ratios;
        std::vector<uint64_t> actual_counts;
        uint64_t total_count = 0;
        
        for (const auto& variant : variants) {
            expected_ratios.push_back(variant["allocation"].asDouble() / 100.0);
            uint64_t count = variant["player_count"].asUInt64();
            actual_counts.push_back(count);
            total_count += count;
        }
        
        if (total_count < 1000) return; // Need sufficient sample
        
        // Chi-square test for SRM
        double chi_square = 0;
        for (size_t i = 0; i < expected_ratios.size(); ++i) {
            double expected = expected_ratios[i] * total_count;
            double actual = actual_counts[i];
            chi_square += std::pow(actual - expected, 2) / expected;
        }
        
        // Critical value for p=0.01 with df=variants-1
        double critical_value = 6.635; // For 2 variants
        
        if (chi_square > critical_value) {
            spdlog::warn("Sample Ratio Mismatch detected in experiment {}: chi2={}",
                        results["experiment_id"].asString(), chi_square);
        }
    }
    
    // [SEQUENCE: MVP14-666] Check for metric anomalies
    static void CheckMetricAnomalies(const Json::Value& results) {
        if (!results.isMember("statistical_analysis")) return;
        
        const auto& analysis = results["statistical_analysis"];
        
        // Check for extreme revenue changes
        if (analysis.isMember("revenue_lift_percentage")) {
            double lift = analysis["revenue_lift_percentage"].asDouble();
            
            if (lift < -20) {
                spdlog::error("Experiment {} showing significant revenue drop: {}%",
                            results["experiment_id"].asString(), lift);
            } else if (lift > 100) {
                spdlog::warn("Experiment {} showing unusually high revenue lift: {}%",
                           results["experiment_id"].asString(), lift);
            }
        }
        
        // Check conversion rates
        if (analysis.isMember("conversion_lift_percentage")) {
            double lift = analysis["conversion_lift_percentage"].asDouble();
            
            if (lift < -30) {
                spdlog::error("Experiment {} showing significant conversion drop: {}%",
                            results["experiment_id"].asString(), lift);
            }
        }
    }
    
    // [SEQUENCE: MVP14-667] Check if experiment is harmful
    static bool IsExperimentHarmful(const Json::Value& results) {
        if (!results.isMember("statistical_analysis")) return false;
        
        const auto& analysis = results["statistical_analysis"];
        
        // Harmful if:
        // 1. Statistically significant negative impact on revenue
        bool is_significant = analysis["is_significant"].asBool();
        double revenue_lift = analysis["revenue_lift_percentage"].asDouble();
        
        if (is_significant && revenue_lift < -10) {
            return true;
        }
        
        // 2. Major drop in player retention (would need this metric)
        // 3. Significant increase in crash rate (would need this metric)
        
        return false;
    }
    
    static std::vector<std::string> GetActiveExperimentIds(ABTestingService* service) {
        // This would need to be implemented in ABTestingService
        return {};
    }
};

// [SEQUENCE: MVP14-668] Example experiment configurations
class ExperimentExamples {
public:
    // [SEQUENCE: MVP14-669] XP progression experiment
    static Json::Value CreateXPProgressionExperiment() {
        Json::Value config;
        config["id"] = "xp_progression_test";
        config["name"] = "XP Progression Rate Test";
        config["description"] = "Testing different XP gain rates for player retention";
        config["is_active"] = true;
        
        // Schedule
        auto now = std::chrono::system_clock::now();
        auto end = now + std::chrono::hours(24 * 14); // 2 weeks
        
        config["start_time"] = FormatTime(now);
        config["end_time"] = FormatTime(end);
        
        // Variants
        Json::Value variants;
        
        // Control group
        Json::Value control;
        control["name"] = "control";
        control["allocation"] = 50.0;
        Json::Value control_params;
        control_params["xp_multiplier"] = 1.0;
        control["parameters"] = control_params;
        variants.append(control);
        
        // Treatment group - 25% faster progression
        Json::Value treatment;
        treatment["name"] = "faster_progression";
        treatment["allocation"] = 50.0;
        Json::Value treatment_params;
        treatment_params["xp_multiplier"] = 1.25;
        treatment["parameters"] = treatment_params;
        variants.append(treatment);
        
        config["variants"] = variants;
        
        // Targeting - only new players
        Json::Value targeting;
        targeting["max_level"] = 10;
        config["targeting"] = targeting;
        
        return config;
    }
    
    // [SEQUENCE: MVP14-670] Store UI redesign experiment
    static Json::Value CreateStoreUIExperiment() {
        Json::Value config;
        config["id"] = "store_ui_redesign";
        config["name"] = "Store UI Redesign Test";
        config["description"] = "Testing new store layout for conversion";
        config["is_active"] = true;
        
        auto now = std::chrono::system_clock::now();
        auto end = now + std::chrono::hours(24 * 30); // 30 days
        
        config["start_time"] = FormatTime(now);
        config["end_time"] = FormatTime(end);
        
        // Three-way test
        Json::Value variants;
        
        // Control
        Json::Value control;
        control["name"] = "control";
        control["allocation"] = 33.33;
        Json::Value control_params;
        control_params["store_layout"] = "classic";
        control["parameters"] = control_params;
        variants.append(control);
        
        // Grid layout
        Json::Value grid;
        grid["name"] = "grid_layout";
        grid["allocation"] = 33.33;
        Json::Value grid_params;
        grid_params["store_layout"] = "grid";
        grid_params["items_per_page"] = 12;
        grid["parameters"] = grid_params;
        variants.append(grid);
        
        // Featured items layout
        Json::Value featured;
        featured["name"] = "featured_layout";
        featured["allocation"] = 33.34;
        Json::Value featured_params;
        featured_params["store_layout"] = "featured";
        featured_params["featured_item_count"] = 3;
        featured["parameters"] = featured_params;
        variants.append(featured);
        
        config["variants"] = variants;
        
        // Success metrics
        Json::Value success_metrics;
        success_metrics["primary_metric"] = "conversion_rate";
        Json::Value secondary;
        secondary.append("average_purchase_value");
        secondary.append("items_viewed_per_session");
        success_metrics["secondary_metrics"] = secondary;
        config["success_metrics"] = success_metrics;
        
        return config;
    }
    
    // [SEQUENCE: MVP14-671] Guild feature rollout
    static Json::Value CreateGuildFeatureRollout() {
        Json::Value config;
        config["id"] = "guild_wars_feature";
        config["name"] = "Guild Wars Feature Rollout";
        config["description"] = "Gradual rollout of new guild wars system";
        config["is_active"] = true;
        
        auto now = std::chrono::system_clock::now();
        auto end = now + std::chrono::hours(24 * 7); // 1 week
        
        config["start_time"] = FormatTime(now);
        config["end_time"] = FormatTime(end);
        
        Json::Value variants;
        
        // Control - no access
        Json::Value control;
        control["name"] = "control";
        control["allocation"] = 90.0; // 10% rollout
        Json::Value control_params;
        control_params["guild_wars_enabled"] = false;
        control["parameters"] = control_params;
        variants.append(control);
        
        // Treatment - has access
        Json::Value treatment;
        treatment["name"] = "enabled";
        treatment["allocation"] = 10.0;
        Json::Value treatment_params;
        treatment_params["guild_wars_enabled"] = true;
        treatment_params["max_guild_size"] = 100;
        treatment_params["war_duration_hours"] = 24;
        treatment["parameters"] = treatment_params;
        variants.append(treatment);
        
        config["variants"] = variants;
        
        // Targeting - only guilds above certain size
        Json::Value targeting;
        targeting["min_level"] = 20;
        Json::Value required_features;
        required_features.append("guild_member");
        targeting["required_features"] = required_features;
        config["targeting"] = targeting;
        
        return config;
    }
    
private:
    static std::string FormatTime(const std::chrono::system_clock::time_point& tp) {
        auto time_t = std::chrono::system_clock::to_time_t(tp);
        char buffer[64];
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", std::localtime(&time_t));
        return buffer;
    }
};

// [SEQUENCE: MVP14-672] A/B test results analyzer
class ABTestResultsAnalyzer {
public:
    // [SEQUENCE: MVP14-673] Generate experiment report
    static std::string GenerateExperimentReport(const Json::Value& results) {
        std::stringstream report;
        
        report << "=== A/B Test Results Report ===" << std::endl;
        report << "Experiment: " << results["experiment_name"].asString() << std::endl;
        report << "ID: " << results["experiment_id"].asString() << std::endl;
        report << "Status: " << results["status"].asString() << std::endl;
        report << std::endl;
        
        // Variant results
        report << "=== Variant Performance ===" << std::endl;
        const auto& variants = results["variants"];
        
        for (const auto& variant : variants) {
            report << "\nVariant: " << variant["name"].asString() << std::endl;
            report << "  Allocation: " << variant["allocation"].asDouble() << "%" << std::endl;
            report << "  Players: " << variant["player_count"].asUInt64() << std::endl;
            report << "  Sessions: " << variant["session_count"].asUInt64() << std::endl;
            report << "  Conversion Rate: " 
                   << std::fixed << std::setprecision(2)
                   << variant["conversion_rate"].asDouble() * 100 << "%" << std::endl;
            report << "  Avg Revenue: $" << variant["avg_revenue"].asDouble() << std::endl;
            report << "  Avg Playtime: " << variant["avg_playtime"].asDouble() / 3600 
                   << " hours" << std::endl;
        }
        
        // Statistical analysis
        if (results.isMember("statistical_analysis")) {
            const auto& analysis = results["statistical_analysis"];
            
            report << "\n=== Statistical Analysis ===" << std::endl;
            report << "Conversion Lift: " 
                   << analysis["conversion_lift_percentage"].asDouble() << "%" << std::endl;
            report << "Revenue Lift: " 
                   << analysis["revenue_lift_percentage"].asDouble() << "%" << std::endl;
            report << "P-value: " << analysis["p_value"].asDouble() << std::endl;
            report << "Statistical Significance: " 
                   << (analysis["is_significant"].asBool() ? "YES" : "NO") << std::endl;
            
            // [SEQUENCE: MVP14-674] Recommendation based on results
            report << "\n=== Recommendation ===" << std::endl;
            if (analysis["is_significant"].asBool()) {
                if (analysis["revenue_lift_percentage"].asDouble() > 0) {
                    report << "SHIP IT! Treatment shows significant improvement." << std::endl;
                    report << "Expected annual revenue impact: $" 
                           << CalculateAnnualImpact(analysis) << std::endl;
                } else {
                    report << "DO NOT SHIP. Treatment shows significant negative impact." << std::endl;
                }
            } else {
                report << "INCONCLUSIVE. Continue testing for statistical significance." << std::endl;
                report << "Estimated additional sample needed: " 
                       << EstimateRequiredSample(results) << " players" << std::endl;
            }
        }
        
        return report.str();
    }
    
private:
    // [SEQUENCE: MVP14-675] Calculate annual revenue impact
    static double CalculateAnnualImpact(const Json::Value& analysis) {
        // Simplified calculation
        double control_arpu = analysis["control_arpu"].asDouble();
        double lift_percentage = analysis["revenue_lift_percentage"].asDouble();
        
        // Assume 100k monthly active users
        double mau = 100000;
        double monthly_impact = mau * control_arpu * (lift_percentage / 100);
        
        return monthly_impact * 12;
    }
    
    // [SEQUENCE: MVP14-676] Estimate required sample size
    static uint64_t EstimateRequiredSample(const Json::Value& results) {
        // Simplified sample size calculation
        // In production, use proper power analysis
        
        const auto& variants = results["variants"];
        if (variants.size() < 2) return 0;
        
        uint64_t current_total = 0;
        for (const auto& variant : variants) {
            current_total += variant["player_count"].asUInt64();
        }
        
        // Rule of thumb: need at least 10k per variant
        uint64_t minimum_needed = variants.size() * 10000;
        
        if (current_total >= minimum_needed) {
            // Need more for smaller effect sizes
            return minimum_needed * 2 - current_total;
        }
        
        return minimum_needed - current_total;
    }
};

} // namespace mmorpg::monitoring
