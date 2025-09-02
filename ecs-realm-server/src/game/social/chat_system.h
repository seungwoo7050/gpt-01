#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <chrono>
#include <functional>
#include <regex>
#include <spdlog/spdlog.h>

namespace mmorpg::game::social {

// [SEQUENCE: MVP11-167] Chat system for player communication
// 채팅 시스템으로 플레이어 간 커뮤니케이션 관리

// [SEQUENCE: MVP11-168] Chat channel types
enum class ChatChannelType {
    SAY,            // Nearby players only
    YELL,           // Larger radius
    WHISPER,        // Private message
    PARTY,          // Party members
    GUILD,          // Guild members
    TRADE,          // Trade channel
    GENERAL,        // Zone-wide general
    WORLD,          // Server-wide (limited)
    SYSTEM,         // System messages
    COMBAT,         // Combat log
    CUSTOM          // Custom channels
};

// [SEQUENCE: MVP11-169] Chat message
struct ChatMessage {
    uint64_t sender_id;
    std::string sender_name;
    std::string message;
    ChatChannelType channel;
    std::chrono::system_clock::time_point timestamp;
    
    // Additional metadata
    uint32_t zone_id = 0;           // For area-based channels
    float x = 0, y = 0, z = 0;      // Sender position for proximity
    uint64_t target_id = 0;         // For whispers
    std::string target_name;        // For whispers
    uint32_t language_id = 0;       // Language system
    
    // Moderation
    bool is_reported = false;
    bool is_filtered = false;
};

// [SEQUENCE: MVP11-170] Chat filter settings
struct ChatFilterSettings {
    bool enable_profanity_filter = true;
    bool enable_spam_filter = true;
    bool enable_caps_filter = true;
    bool enable_link_filter = true;
    bool enable_gold_seller_filter = true;
    
    // Channel toggles
    std::unordered_set<ChatChannelType> enabled_channels = {
        ChatChannelType::SAY,
        ChatChannelType::PARTY,
        ChatChannelType::GUILD,
        ChatChannelType::WHISPER,
        ChatChannelType::SYSTEM
    };
    
    // Ignore list
    std::unordered_set<uint64_t> ignored_players;
};

// [SEQUENCE: MVP11-171] Chat channel configuration
struct ChatChannelConfig {
    std::string channel_name;
    ChatChannelType type;
    uint32_t message_cooldown_ms = 1000;  // Anti-spam
    uint32_t max_message_length = 255;
    float range = 0.0f;  // 0 = unlimited
    bool requires_permission = false;
    uint32_t min_level = 1;
    uint64_t cost_per_message = 0;  // For world chat
    
    // Moderation
    bool is_moderated = false;
    std::vector<std::string> banned_words;
};

// [SEQUENCE: MVP11-172] Chat history
class ChatHistory {
public:
    ChatHistory(size_t max_messages = 100) : max_messages_(max_messages) {}
    
    // [SEQUENCE: MVP11-173] Add message to history
    void AddMessage(const ChatMessage& message) {
        messages_.push_back(message);
        
        // Trim old messages
        while (messages_.size() > max_messages_) {
            messages_.pop_front();
        }
        
        // Index by channel
        channel_messages_[message.channel].push_back(message);
        
        // Trim channel history
        auto& channel_msgs = channel_messages_[message.channel];
        while (channel_msgs.size() > max_messages_ / 10) {
            channel_msgs.pop_front();
        }
    }
    
    // [SEQUENCE: MVP11-174] Get recent messages
    std::vector<ChatMessage> GetRecentMessages(size_t count = 50) const {
        std::vector<ChatMessage> result;
        size_t start = messages_.size() > count ? messages_.size() - count : 0;
        
        for (size_t i = start; i < messages_.size(); ++i) {
            result.push_back(messages_[i]);
        }
        
        return result;
    }
    
    // [SEQUENCE: MVP11-175] Get messages by channel
    std::vector<ChatMessage> GetChannelMessages(ChatChannelType channel, 
                                               size_t count = 50) const {
        auto it = channel_messages_.find(channel);
        if (it == channel_messages_.end()) {
            return {};
        }
        
        const auto& channel_msgs = it->second;
        std::vector<ChatMessage> result;
        size_t start = channel_msgs.size() > count ? channel_msgs.size() - count : 0;
        
        for (size_t i = start; i < channel_msgs.size(); ++i) {
            result.push_back(channel_msgs[i]);
        }
        
        return result;
    }
    
    // [SEQUENCE: MVP11-176] Search messages
    std::vector<ChatMessage> SearchMessages(const std::string& query) const {
        std::vector<ChatMessage> result;
        std::regex search_regex(query, std::regex_constants::icase);
        
        for (const auto& msg : messages_) {
            if (std::regex_search(msg.message, search_regex) ||
                std::regex_search(msg.sender_name, search_regex)) {
                result.push_back(msg);
            }
        }
        
        return result;
    }
    
private:
    size_t max_messages_;
    std::deque<ChatMessage> messages_;
    std::unordered_map<ChatChannelType, std::deque<ChatMessage>> channel_messages_;
};

// [SEQUENCE: MVP11-177] Chat participant
class ChatParticipant {
public:
    ChatParticipant(uint64_t player_id) 
        : player_id_(player_id), chat_history_(200) {}
    
    // [SEQUENCE: MVP11-178] Send message
    bool CanSendMessage(ChatChannelType channel) {
        auto now = std::chrono::steady_clock::now();
        
        // Check cooldown
        auto it = last_message_time_.find(channel);
        if (it != last_message_time_.end()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - it->second
            ).count();
            
            if (elapsed < GetChannelCooldown(channel)) {
                return false;  // Still on cooldown
            }
        }
        
        // Check mute status
        if (is_muted_ && mute_end_time_ > now) {
            return false;
        }
        
        last_message_time_[channel] = now;
        return true;
    }
    
    // [SEQUENCE: MVP11-179] Receive message
    void ReceiveMessage(const ChatMessage& message) {
        // Check if sender is ignored
        if (filter_settings_.ignored_players.find(message.sender_id) != 
            filter_settings_.ignored_players.end()) {
            return;
        }
        
        // Check if channel is enabled
        if (filter_settings_.enabled_channels.find(message.channel) == 
            filter_settings_.enabled_channels.end()) {
            return;
        }
        
        // Add to history
        chat_history_.AddMessage(message);
        
        // Notify callbacks
        if (message_callback_) {
            message_callback_(message);
        }
    }
    
    // [SEQUENCE: MVP11-180] Mute player
    void Mute(std::chrono::seconds duration) {
        is_muted_ = true;
        mute_end_time_ = std::chrono::steady_clock::now() + duration;
    }
    
    void Unmute() {
        is_muted_ = false;
    }
    
    // [SEQUENCE: MVP11-181] Ignore player
    void IgnorePlayer(uint64_t player_id) {
        filter_settings_.ignored_players.insert(player_id);
    }
    
    void UnignorePlayer(uint64_t player_id) {
        filter_settings_.ignored_players.erase(player_id);
    }
    
    // [SEQUENCE: MVP11-182] Toggle channel
    void EnableChannel(ChatChannelType channel) {
        filter_settings_.enabled_channels.insert(channel);
    }
    
    void DisableChannel(ChatChannelType channel) {
        filter_settings_.enabled_channels.erase(channel);
    }
    
    // Getters
    const ChatHistory& GetHistory() const { return chat_history_; }
    const ChatFilterSettings& GetFilterSettings() const { return filter_settings_; }
    bool IsMuted() const { return is_muted_; }
    
    // Set message callback
    void SetMessageCallback(std::function<void(const ChatMessage&)> callback) {
        message_callback_ = callback;
    }
    
private:
    uint64_t player_id_;
    ChatHistory chat_history_;
    ChatFilterSettings filter_settings_;
    
    // Spam prevention
    std::unordered_map<ChatChannelType, std::chrono::steady_clock::time_point> last_message_time_;
    
    // Mute status
    bool is_muted_ = false;
    std::chrono::steady_clock::time_point mute_end_time_;
    
    // Callback for new messages
    std::function<void(const ChatMessage&)> message_callback_;
    
    // [SEQUENCE: MVP11-183] Get channel cooldown
    uint32_t GetChannelCooldown(ChatChannelType channel) {
        switch (channel) {
            case ChatChannelType::WORLD:
                return 30000;  // 30 seconds for world chat
            case ChatChannelType::TRADE:
                return 5000;   // 5 seconds for trade
            case ChatChannelType::YELL:
                return 3000;   // 3 seconds for yell
            default:
                return 1000;   // 1 second default
        }
    }
};

// [SEQUENCE: MVP11-184] Chat moderation
class ChatModerator {
public:
    // [SEQUENCE: MVP11-185] Filter message
    static bool FilterMessage(std::string& message, const ChatFilterSettings& settings) {
        bool modified = false;
        
        if (settings.enable_profanity_filter) {
            modified |= FilterProfanity(message);
        }
        
        if (settings.enable_caps_filter) {
            modified |= FilterExcessiveCaps(message);
        }
        
        if (settings.enable_link_filter) {
            modified |= FilterLinks(message);
        }
        
        return modified;
    }
    
    // [SEQUENCE: MVP11-186] Check for spam
    static bool IsSpam(const std::string& message, 
                      const std::vector<std::string>& recent_messages) {
        // Check for repeated messages
        int similar_count = 0;
        for (const auto& recent : recent_messages) {
            if (GetSimilarity(message, recent) > 0.8f) {
                similar_count++;
            }
        }
        
        return similar_count >= 3;
    }
    
    // [SEQUENCE: MVP11-187] Check for gold seller patterns
    static bool IsGoldSellerMessage(const std::string& message) {
        static const std::vector<std::regex> patterns = {
            std::regex("(www|http|\\.).*gold", std::regex_constants::icase),
            std::regex("cheap.*gold.*delivery", std::regex_constants::icase),
            std::regex("\\$\d+.*=.*\\d+k", std::regex_constants::icase),
            std::regex("gold.*stock.*fast", std::regex_constants::icase)
        };
        
        for (const auto& pattern : patterns) {
            if (std::regex_search(message, pattern)) {
                return true;
            }
        }
        
        return false;
    }
    
private:
    // [SEQUENCE: MVP11-188] Filter profanity
    static bool FilterProfanity(std::string& message) {
        static const std::vector<std::string> bad_words = {
            // Add actual bad words in production
            "badword1", "badword2"
        };
        
        bool found = false;
        for (const auto& word : bad_words) {
            size_t pos = 0;
            while ((pos = message.find(word, pos)) != std::string::npos) {
                message.replace(pos, word.length(), std::string(word.length(), '*'));
                found = true;
                pos += word.length();
            }
        }
        
        return found;
    }
    
    // [SEQUENCE: MVP11-189] Filter excessive caps
    static bool FilterExcessiveCaps(std::string& message) {
        if (message.length() < 10) return false;
        
        size_t caps_count = 0;
        for (char c : message) {
            if (std::isupper(c)) caps_count++;
        }
        
        float caps_ratio = static_cast<float>(caps_count) / message.length();
        if (caps_ratio > 0.7f) {
            // Convert to sentence case
            bool first = true;
            for (char& c : message) {
                if (std::isalpha(c)) {
                    c = first ? std::toupper(c) : std::tolower(c);
                    first = false;
                } else if (c == '.' || c == '!' || c == '?') {
                    first = true;
                }
            }
            return true;
        }
        
        return false;
    }
    
    // [SEQUENCE: MVP11-190] Filter links
    static bool FilterLinks(std::string& message) {
        static const std::regex url_pattern(
            R"((https?:\\/\\/)?([\\da-z\\.-]+)\\.(.a-z\\.){2,6})([\\/\\w \\.-]*)*\\/?)"
        );
        
        return std::regex_replace(message, url_pattern, "[LINK REMOVED]") != message;
    }
    
    // [SEQUENCE: MVP11-191] Calculate message similarity
    static float GetSimilarity(const std::string& str1, const std::string& str2) {
        if (str1.empty() || str2.empty()) return 0.0f;
        
        // Simple character-based similarity
        size_t matches = 0;
        size_t max_len = std::max(str1.length(), str2.length());
        size_t min_len = std::min(str1.length(), str2.length());
        
        for (size_t i = 0; i < min_len; ++i) {
            if (std::tolower(str1[i]) == std::tolower(str2[i])) {
                matches++;
            }
        }
        
        return static_cast<float>(matches) / max_len;
    }
};

// [SEQUENCE: MVP11-192] Chat manager
class ChatManager {
public:
    static ChatManager& Instance() {
        static ChatManager instance;
        return instance;
    }
    
    // [SEQUENCE: MVP11-193] Initialize system
    void Initialize() {
        InitializeChannels();
        spdlog::info("Chat system initialized");
    }
    
    // [SEQUENCE: MVP11-194] Send message
    bool SendMessage(uint64_t sender_id, const std::string& sender_name,
                    const std::string& message, ChatChannelType channel,
                    uint64_t target_id = 0) {
        // Get sender participant
        auto sender = GetOrCreateParticipant(sender_id);
        if (!sender->CanSendMessage(channel)) {
            spdlog::warn("Player {} cannot send message (cooldown/muted)", sender_id);
            return false;
        }
        
        // Create message
        ChatMessage chat_msg;
        chat_msg.sender_id = sender_id;
        chat_msg.sender_name = sender_name;
        chat_msg.message = message;
        chat_msg.channel = channel;
        chat_msg.timestamp = std::chrono::system_clock::now();
        chat_msg.target_id = target_id;
        
        // Filter message
        std::string filtered_message = message;
        ChatModerator::FilterMessage(filtered_message, sender->GetFilterSettings());
        chat_msg.message = filtered_message;
        
        // Check for spam
        if (CheckSpam(sender_id, filtered_message)) {
            spdlog::warn("Spam detected from player {}", sender_id);
            return false;
        }
        
        // Check for gold sellers
        if (ChatModerator::IsGoldSellerMessage(filtered_message)) {
            spdlog::warn("Gold seller message detected from player {}", sender_id);
            // Auto-mute for 24 hours
            sender->Mute(std::chrono::hours(24));
            return false;
        }
        
        // Route message based on channel
        RouteMessage(chat_msg);
        
        // Add to sender's history
        sender->ReceiveMessage(chat_msg);
        
        // Track for spam detection
        AddToRecentMessages(sender_id, filtered_message);
        
        return true;
    }
    
    // [SEQUENCE: MVP11-195] Send whisper
    bool SendWhisper(uint64_t sender_id, const std::string& sender_name,
                     uint64_t target_id, const std::string& target_name,
                     const std::string& message) {
        ChatMessage whisper;
        whisper.sender_id = sender_id;
        whisper.sender_name = sender_name;
        whisper.target_id = target_id;
        whisper.target_name = target_name;
        whisper.message = message;
        whisper.channel = ChatChannelType::WHISPER;
        whisper.timestamp = std::chrono::system_clock::now();
        
        // Send to target
        auto target = GetOrCreateParticipant(target_id);
        target->ReceiveMessage(whisper);
        
        // Echo back to sender
        auto sender = GetOrCreateParticipant(sender_id);
        sender->ReceiveMessage(whisper);
        
        return true;
    }
    
    // [SEQUENCE: MVP11-196] Join custom channel
    void JoinChannel(uint64_t player_id, const std::string& channel_name) {
        custom_channels_[channel_name].insert(player_id);
        
        spdlog::info("Player {} joined channel {}", player_id, channel_name);
    }
    
    // [SEQUENCE: MVP11-197] Leave custom channel
    void LeaveChannel(uint64_t player_id, const std::string& channel_name) {
        auto it = custom_channels_.find(channel_name);
        if (it != custom_channels_.end()) {
            it->second.erase(player_id);
            
            // Remove empty channels
            if (it->second.empty()) {
                custom_channels_.erase(it);
            }
        }
    }
    
    // [SEQUENCE: MVP11-198] Get participant
    std::shared_ptr<ChatParticipant> GetOrCreateParticipant(uint64_t player_id) {
        auto it = participants_.find(player_id);
        if (it == participants_.end()) {
            auto participant = std::make_shared<ChatParticipant>(player_id);
            participants_[player_id] = participant;
            return participant;
        }
        return it->second;
    }
    
    // [SEQUENCE: MVP11-199] Mute player (admin command)
    void MutePlayer(uint64_t player_id, std::chrono::seconds duration) {
        auto participant = GetOrCreateParticipant(player_id);
        participant->Mute(duration);
        
        spdlog::info("Player {} muted for {} seconds", player_id, duration.count());
    }
    
private:
    ChatManager() = default;
    
    std::unordered_map<uint64_t, std::shared_ptr<ChatParticipant>> participants_;
    std::unordered_map<ChatChannelType, ChatChannelConfig> channel_configs_;
    std::unordered_map<std::string, std::unordered_set<uint64_t>> custom_channels_;
    
    // Spam tracking
    std::unordered_map<uint64_t, std::deque<std::string>> recent_messages_;
    
    // [SEQUENCE: MVP11-200] Initialize channels
    void InitializeChannels() {
        // Say channel
        ChatChannelConfig say_config;
        say_config.channel_name = "Say";
        say_config.type = ChatChannelType::SAY;
        say_config.range = 30.0f;
        channel_configs_[ChatChannelType::SAY] = say_config;
        
        // Yell channel
        ChatChannelConfig yell_config;
        yell_config.channel_name = "Yell";
        yell_config.type = ChatChannelType::YELL;
        yell_config.range = 100.0f;
        yell_config.message_cooldown_ms = 3000;
        channel_configs_[ChatChannelType::YELL] = yell_config;
        
        // World channel
        ChatChannelConfig world_config;
        world_config.channel_name = "World";
        world_config.type = ChatChannelType::WORLD;
        world_config.message_cooldown_ms = 30000;
        world_config.min_level = 10;
        world_config.cost_per_message = 100;
        channel_configs_[ChatChannelType::WORLD] = world_config;
    }
    
    // [SEQUENCE: MVP11-201] Route message to recipients
    void RouteMessage(const ChatMessage& message) {
        switch (message.channel) {
            case ChatChannelType::SAY:
            case ChatChannelType::YELL:
                RouteProximityMessage(message);
                break;
                
            case ChatChannelType::PARTY:
                RoutePartyMessage(message);
                break;
                
            case ChatChannelType::GUILD:
                RouteGuildMessage(message);
                break;
                
            case ChatChannelType::WORLD:
            case ChatChannelType::TRADE:
            case ChatChannelType::GENERAL:
                RouteZoneMessage(message);
                break;
                
            case ChatChannelType::WHISPER:
                // Handled separately
                break;
                
            default:
                break;
        }
    }
    
    // [SEQUENCE: MVP11-202] Route proximity-based message
    void RouteProximityMessage(const ChatMessage& message) {
        auto config_it = channel_configs_.find(message.channel);
        if (config_it == channel_configs_.end()) return;
        
        float range = config_it->second.range;
        
        // TODO: Get nearby players within range
        // For now, just log
        spdlog::debug("Proximity message from {} in range {}", 
                     message.sender_id, range);
    }
    
    // [SEQUENCE: MVP11-203] Route party message
    void RoutePartyMessage(const ChatMessage& message) {
        // TODO: Get party members and send to each
        spdlog::debug("Party message from {}", message.sender_id);
    }
    
    // [SEQUENCE: MVP11-204] Route guild message
    void RouteGuildMessage(const ChatMessage& message) {
        // TODO: Get guild members and send to each
        spdlog::debug("Guild message from {}", message.sender_id);
    }
    
    // [SEQUENCE: MVP11-205] Route zone-wide message
    void RouteZoneMessage(const ChatMessage& message) {
        // TODO: Get all players in zone/world and send
        spdlog::debug("Zone message from {} on channel {}", 
                     message.sender_id, static_cast<int>(message.channel));
    }
    
    // [SEQUENCE: MVP11-206] Check for spam
    bool CheckSpam(uint64_t player_id, const std::string& message) {
        auto& recent = recent_messages_[player_id];
        
        // Convert deque to vector for spam check
        std::vector<std::string> recent_vec(recent.begin(), recent.end());
        
        return ChatModerator::IsSpam(message, recent_vec);
    }
    
    // [SEQUENCE: MVP11-207] Add to recent messages
    void AddToRecentMessages(uint64_t player_id, const std::string& message) {
        auto& recent = recent_messages_[player_id];
        recent.push_back(message);
        
        // Keep only last 10 messages
        while (recent.size() > 10) {
            recent.pop_front();
        }
    }
};
