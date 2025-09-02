#pragma once

#include "rate_limiter.h"
#include "../config/environment_config.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <spdlog/spdlog.h>

namespace mmorpg::core::security {

// [SEQUENCE: 434] Security manager for centralized security policies
class SecurityManager {
public:
    static SecurityManager& Instance() {
        static SecurityManager instance;
        return instance;
    }
    
    // [SEQUENCE: 435] Initialize security manager with environment config
    void Initialize() {
        auto& env_config = config::EnvironmentConfig::Instance();
        auto rate_config = env_config.GetRateLimitConfig();
        
        if (!env_config.GetBool("ENABLE_RATE_LIMITING", true)) {
            spdlog::warn("Rate limiting is disabled in configuration");
            rate_limiting_enabled_ = false;
            return;
        }
        
        // [SEQUENCE: 436] Setup hierarchical rate limiter
        rate_limiter_ = std::make_unique<HierarchicalRateLimiter>();
        
        // Login rate limiting
        rate_limiter_->SetRateLimit("login", 
            rate_config.login_requests_per_minute, 
            std::chrono::seconds(60));
            
        // Game actions rate limiting  
        rate_limiter_->SetRateLimit("game_action",
            rate_config.game_actions_per_second,
            std::chrono::seconds(1));
            
        // Chat rate limiting
        rate_limiter_->SetRateLimit("chat",
            rate_config.chat_messages_per_minute,
            std::chrono::seconds(60));
            
        // API rate limiting
        rate_limiter_->SetRateLimit("api",
            rate_config.api_requests_per_minute,
            std::chrono::seconds(60));
        
        rate_limiting_enabled_ = true;
        
        spdlog::info("Security manager initialized with rate limiting:");
        spdlog::info("  Login: {} requests/minute", rate_config.login_requests_per_minute);
        spdlog::info("  Actions: {} requests/second", rate_config.game_actions_per_second);
        spdlog::info("  Chat: {} messages/minute", rate_config.chat_messages_per_minute);
        spdlog::info("  API: {} requests/minute", rate_config.api_requests_per_minute);
    }
    
    // [SEQUENCE: 437] Check if action is allowed for client
    bool IsActionAllowed(const std::string& client_id, const std::string& action) {
        if (!rate_limiting_enabled_) {
            return true;
        }
        
        if (!rate_limiter_) {
            spdlog::error("Rate limiter not initialized");
            return false;
        }
        
        bool allowed = rate_limiter_->AllowAction(client_id, action);
        
        if (!allowed) {
            // [SEQUENCE: 438] Log rate limit violations
            violation_count_[client_id]++;
            auto count = violation_count_[client_id];
            
            spdlog::warn("Rate limit violation - Client: {}, Action: {}, Count: {}", 
                        client_id, action, count);
            
            // [SEQUENCE: 439] Escalate severe violations
            if (count > 10) {
                spdlog::error("Severe rate limit violations from client: {} ({}x)", 
                             client_id, count);
                // TODO: Consider temporary ban or stricter limits
            }
        }
        
        return allowed;
    }
    
    // [SEQUENCE: 440] Login attempt validation
    bool ValidateLoginAttempt(const std::string& ip_address) {
        return IsActionAllowed(ip_address, "login");
    }
    
    // [SEQUENCE: 441] Game action validation
    bool ValidateGameAction(const std::string& player_id) {
        return IsActionAllowed(player_id, "game_action");
    }
    
    // [SEQUENCE: 442] Chat message validation
    bool ValidateChatMessage(const std::string& player_id) {
        return IsActionAllowed(player_id, "chat");
    }
    
    // [SEQUENCE: 443] API request validation
    bool ValidateAPIRequest(const std::string& api_key_or_ip) {
        return IsActionAllowed(api_key_or_ip, "api");
    }
    
    // [SEQUENCE: 444] Get client rate limit status
    std::unordered_map<std::string, size_t> GetClientStatus(const std::string& client_id) {
        if (!rate_limiter_) {
            return {};
        }
        
        return rate_limiter_->GetClientStatus(client_id);
    }
    
    // [SEQUENCE: 445] Security metrics for monitoring
    struct SecurityMetrics {
        uint64_t total_rate_limit_violations{0};
        uint64_t login_blocks{0};
        uint64_t action_blocks{0};
        uint64_t chat_blocks{0};
        uint64_t api_blocks{0};
        uint64_t clients_with_violations{0};
    };
    
    SecurityMetrics GetSecurityMetrics() const {
        SecurityMetrics metrics;
        
        for (const auto& [client_id, count] : violation_count_) {
            metrics.total_rate_limit_violations += count;
            if (count > 0) {
                metrics.clients_with_violations++;
            }
        }
        
        return metrics;
    }
    
    // [SEQUENCE: 446] Clear violation history (for testing/reset)
    void ClearViolationHistory() {
        violation_count_.clear();
        spdlog::info("Rate limit violation history cleared");
    }
    
    // [SEQUENCE: 447] Get current configuration
    struct SecurityConfig {
        bool rate_limiting_enabled;
        bool ddos_protection_enabled;
        bool packet_encryption_enabled;
        std::string environment;
    };
    
    SecurityConfig GetSecurityConfig() const {
        auto& env_config = config::EnvironmentConfig::Instance();
        
        SecurityConfig config;
        config.rate_limiting_enabled = rate_limiting_enabled_;
        config.ddos_protection_enabled = env_config.GetBool("ENABLE_DDOS_PROTECTION", true);
        config.packet_encryption_enabled = env_config.GetBool("ENABLE_PACKET_ENCRYPTION", false);
        
        switch (env_config.GetEnvironment()) {
            case config::EnvironmentConfig::Environment::PRODUCTION:
                config.environment = "production";
                break;
            case config::EnvironmentConfig::Environment::STAGING:
                config.environment = "staging";
                break;
            default:
                config.environment = "development";
                break;
        }
        
        return config;
    }
    
    // [SEQUENCE: 448] Validate security requirements for environment
    bool ValidateSecurityRequirements() const {
        auto& env_config = config::EnvironmentConfig::Instance();
        
        if (env_config.IsProduction()) {
            // Production security requirements
            if (!env_config.GetBool("ENABLE_RATE_LIMITING", true)) {
                spdlog::error("Rate limiting must be enabled in production");
                return false;
            }
            
            if (!env_config.GetBool("ENABLE_DDOS_PROTECTION", true)) {
                spdlog::error("DDoS protection must be enabled in production");
                return false;
            }
            
            auto jwt_secret = env_config.GetString("JWT_SECRET");
            if (jwt_secret.length() < 64) {
                spdlog::error("JWT secret must be at least 64 characters in production");
                return false;
            }
        }
        
        return true;
    }
    
private:
    SecurityManager() = default;
    
    std::unique_ptr<HierarchicalRateLimiter> rate_limiter_;
    std::unordered_map<std::string, uint64_t> violation_count_;
    bool rate_limiting_enabled_{false};
};

// [SEQUENCE: 449] Convenience macros for security checks
#define SECURITY_MANAGER SecurityManager::Instance()
#define CHECK_LOGIN_RATE_LIMIT(ip) SECURITY_MANAGER.ValidateLoginAttempt(ip)
#define CHECK_ACTION_RATE_LIMIT(player_id) SECURITY_MANAGER.ValidateGameAction(player_id)
#define CHECK_CHAT_RATE_LIMIT(player_id) SECURITY_MANAGER.ValidateChatMessage(player_id)
#define CHECK_API_RATE_LIMIT(key) SECURITY_MANAGER.ValidateAPIRequest(key)

} // namespace mmorpg::core::security