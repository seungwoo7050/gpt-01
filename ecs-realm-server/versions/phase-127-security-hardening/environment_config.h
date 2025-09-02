#pragma once

#include <string>
#include <optional>
#include <map>
#include <stdexcept>
#include <fstream>
#include <spdlog/spdlog.h>

namespace mmorpg::core::config {

// [SEQUENCE: 412] Environment configuration manager for secure settings
class EnvironmentConfig {
public:
    static EnvironmentConfig& Instance() {
        static EnvironmentConfig instance;
        return instance;
    }
    
    // [SEQUENCE: 413] Load configuration from .env file and environment
    void LoadConfiguration(const std::string& env_file_path = ".env") {
        LoadFromFile(env_file_path);
        LoadFromEnvironment();
        ValidateRequiredSettings();
        
        spdlog::info("Configuration loaded successfully");
    }
    
    // [SEQUENCE: 414] Get configuration values with validation
    std::string GetString(const std::string& key, const std::string& default_value = "") const {
        auto it = config_map_.find(key);
        if (it != config_map_.end()) {
            return it->second;
        }
        
        if (default_value.empty() && IsRequired(key)) {
            throw std::runtime_error("Required configuration key not found: " + key);
        }
        
        return default_value;
    }
    
    int GetInt(const std::string& key, int default_value = 0) const {
        auto value = GetString(key);
        if (value.empty()) {
            return default_value;
        }
        
        try {
            return std::stoi(value);
        } catch (const std::exception& e) {
            spdlog::error("Invalid integer value for {}: {}", key, value);
            return default_value;
        }
    }
    
    bool GetBool(const std::string& key, bool default_value = false) const {
        auto value = GetString(key);
        if (value.empty()) {
            return default_value;
        }
        
        std::string lower_value = value;
        std::transform(lower_value.begin(), lower_value.end(), lower_value.begin(), ::tolower);
        
        return lower_value == "true" || lower_value == "1" || lower_value == "yes";
    }
    
    // [SEQUENCE: 415] Environment detection
    enum class Environment {
        DEVELOPMENT,
        STAGING, 
        PRODUCTION
    };
    
    Environment GetEnvironment() const {
        std::string env = GetString("MMORPG_ENV", "development");
        std::transform(env.begin(), env.end(), env.begin(), ::tolower);
        
        if (env == "production" || env == "prod") {
            return Environment::PRODUCTION;
        } else if (env == "staging" || env == "stage") {
            return Environment::STAGING;
        }
        
        return Environment::DEVELOPMENT;
    }
    
    bool IsProduction() const {
        return GetEnvironment() == Environment::PRODUCTION;
    }
    
    bool IsDevelopment() const {
        return GetEnvironment() == Environment::DEVELOPMENT;
    }
    
    // [SEQUENCE: 416] Security-focused getters
    std::string GetJWTSecret() const {
        auto secret = GetString("JWT_SECRET");
        if (secret.length() < 32) {
            throw std::runtime_error("JWT_SECRET must be at least 32 characters long");
        }
        return secret;
    }
    
    std::string GetDatabasePassword() const {
        return GetString("DB_PASSWORD");
    }
    
    std::string GetRedisPassword() const {
        return GetString("REDIS_PASSWORD", "");
    }
    
    // [SEQUENCE: 417] Network configuration
    struct NetworkConfig {
        std::string game_server_host;
        int game_server_port;
        std::string login_server_host;
        int login_server_port;
        int worker_threads;
        int max_connections;
    };
    
    NetworkConfig GetNetworkConfig() const {
        NetworkConfig config;
        config.game_server_host = GetString("GAME_SERVER_HOST", "0.0.0.0");
        config.game_server_port = GetInt("GAME_SERVER_PORT", 8081);
        config.login_server_host = GetString("LOGIN_SERVER_HOST", "0.0.0.0");
        config.login_server_port = GetInt("LOGIN_SERVER_PORT", 8080);
        config.worker_threads = GetInt("WORKER_THREADS", 8);
        config.max_connections = GetInt("MAX_CONNECTIONS", 5000);
        
        return config;
    }
    
    // [SEQUENCE: 418] Database configuration
    struct DatabaseConfig {
        std::string host;
        int port;
        std::string username;
        std::string password;
        std::string database;
        int pool_size;
    };
    
    DatabaseConfig GetDatabaseConfig() const {
        DatabaseConfig config;
        config.host = GetString("DB_HOST", "localhost");
        config.port = GetInt("DB_PORT", 3306);
        config.username = GetString("DB_USERNAME", "mmorpg_user");
        config.password = GetDatabasePassword();
        config.database = GetString("DB_DATABASE", "mmorpg");
        config.pool_size = GetInt("DB_POOL_SIZE", 20);
        
        return config;
    }
    
    // [SEQUENCE: 419] Rate limiting configuration
    struct RateLimitConfig {
        int login_requests_per_minute;
        int game_actions_per_second;
        int chat_messages_per_minute;
        int api_requests_per_minute;
    };
    
    RateLimitConfig GetRateLimitConfig() const {
        RateLimitConfig config;
        config.login_requests_per_minute = GetInt("RATE_LIMIT_LOGIN", 5);
        config.game_actions_per_second = GetInt("RATE_LIMIT_ACTIONS", 10);
        config.chat_messages_per_minute = GetInt("RATE_LIMIT_CHAT", 60);
        config.api_requests_per_minute = GetInt("RATE_LIMIT_API", 100);
        
        return config;
    }
    
    // [SEQUENCE: 420] Configuration validation
    void ValidateConfiguration() const {
        // Security validation
        if (IsProduction()) {
            auto jwt_secret = GetString("JWT_SECRET");
            if (jwt_secret.length() < 64) {
                throw std::runtime_error("Production JWT_SECRET must be at least 64 characters");
            }
            
            if (GetString("DB_PASSWORD").empty()) {
                throw std::runtime_error("DB_PASSWORD is required in production");
            }
        }
        
        // Network validation
        auto net_config = GetNetworkConfig();
        if (net_config.max_connections < 100 || net_config.max_connections > 10000) {
            throw std::runtime_error("MAX_CONNECTIONS must be between 100 and 10000");
        }
        
        spdlog::info("Configuration validation passed");
    }
    
private:
    std::map<std::string, std::string> config_map_;
    
    // [SEQUENCE: 421] Load from .env file
    void LoadFromFile(const std::string& file_path) {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            spdlog::warn("Environment file not found: {}", file_path);
            return;
        }
        
        std::string line;
        while (std::getline(file, line)) {
            // Skip comments and empty lines
            if (line.empty() || line[0] == '#') {
                continue;
            }
            
            auto pos = line.find('=');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);
                
                // Remove quotes if present
                if (value.front() == '"' && value.back() == '"') {
                    value = value.substr(1, value.length() - 2);
                }
                
                config_map_[key] = value;
            }
        }
        
        spdlog::info("Loaded {} configuration entries from {}", config_map_.size(), file_path);
    }
    
    // [SEQUENCE: 422] Load from system environment (overrides file)
    void LoadFromEnvironment() {
        for (const auto& required_key : GetRequiredKeys()) {
            const char* env_value = std::getenv(required_key.c_str());
            if (env_value) {
                config_map_[required_key] = std::string(env_value);
            }
        }
        
        spdlog::info("Environment variables loaded");
    }
    
    // [SEQUENCE: 423] Define required configuration keys
    std::vector<std::string> GetRequiredKeys() const {
        return {
            "JWT_SECRET",
            "DB_PASSWORD",
            "MMORPG_ENV"
        };
    }
    
    bool IsRequired(const std::string& key) const {
        auto required = GetRequiredKeys();
        return std::find(required.begin(), required.end(), key) != required.end();
    }
    
    void ValidateRequiredSettings() {
        for (const auto& key : GetRequiredKeys()) {
            if (config_map_.find(key) == config_map_.end()) {
                throw std::runtime_error("Required configuration key missing: " + key);
            }
        }
    }
};

// [SEQUENCE: 424] Convenience macros
#define ENV_CONFIG EnvironmentConfig::Instance()
#define GET_CONFIG_STRING(key, default_val) ENV_CONFIG.GetString(key, default_val)
#define GET_CONFIG_INT(key, default_val) ENV_CONFIG.GetInt(key, default_val)
#define GET_CONFIG_BOOL(key, default_val) ENV_CONFIG.GetBool(key, default_val)

} // namespace mmorpg::core::config