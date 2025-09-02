#pragma once

#include "ui_framework.h"
#include "../social/chat_system.h"
#include <string>
#include <vector>
#include <deque>
#include <memory>
#include <chrono>
#include <regex>
#include <spdlog/spdlog.h>

namespace mmorpg::ui {

// [SEQUENCE: 2461] Chat interface system for player communication
// 채팅 인터페이스 - 플레이어 간 소통을 위한 UI 시스템

// [SEQUENCE: 2462] Chat message display
class ChatMessage {
public:
    struct MessageData {
        social::ChatChannel channel;
        std::string sender_name;
        std::string message_text;
        std::chrono::system_clock::time_point timestamp;
        Color channel_color;
        bool is_system_message = false;
        
        // [SEQUENCE: 2463] Additional message properties
        std::string sender_title;      // Guild rank, achievement title, etc.
        uint32_t sender_level = 0;
        bool is_gm = false;            // Game Master message
    };
    
    ChatMessage(const MessageData& data) : data_(data) {
        // [SEQUENCE: 2464] Format message for display
        formatted_text_ = FormatMessage();
        
        // Calculate message height based on text wrapping
        CalculateHeight();
    }
    
    const std::string& GetFormattedText() const { return formatted_text_; }
    float GetHeight() const { return height_; }
    const MessageData& GetData() const { return data_; }
    
    // [SEQUENCE: 2465] Check if message matches filter
    bool MatchesFilter(const std::string& filter) const {
        if (filter.empty()) return true;
        
        // Case-insensitive search
        std::string lower_text = formatted_text_;
        std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);
        
        std::string lower_filter = filter;
        std::transform(lower_filter.begin(), lower_filter.end(), lower_filter.begin(), ::tolower);
        
        return lower_text.find(lower_filter) != std::string::npos;
    }
    
private:
    MessageData data_;
    std::string formatted_text_;
    float height_ = 16.0f;  // Default single line height
    
    // [SEQUENCE: 2466] Format message with timestamp and channel
    std::string FormatMessage() {
        std::string result;
        
        // Add timestamp
        auto time_t = std::chrono::system_clock::to_time_t(data_.timestamp);
        struct tm* tm = localtime(&time_t);
        char time_buffer[16];
        strftime(time_buffer, sizeof(time_buffer), "[%H:%M:%S]", tm);
        result += time_buffer;
        result += " ";
        
        // Add channel prefix
        result += GetChannelPrefix();
        
        // Add sender info
        if (!data_.is_system_message) {
            if (data_.is_gm) {
                result += "[GM]";
            }
            
            if (!data_.sender_title.empty()) {
                result += "<" + data_.sender_title + ">";
            }
            
            result += "[" + data_.sender_name;
            if (data_.sender_level > 0) {
                result += ":" + std::to_string(data_.sender_level);
            }
            result += "]: ";
        }
        
        // Add message text
        result += data_.message_text;
        
        return result;
    }
    
    // [SEQUENCE: 2467] Get channel-specific prefix
    std::string GetChannelPrefix() const {
        switch (data_.channel) {
            case social::ChatChannel::SAY:
                return "[Say] ";
            case social::ChatChannel::YELL:
                return "[Yell] ";
            case social::ChatChannel::PARTY:
                return "[Party] ";
            case social::ChatChannel::GUILD:
                return "[Guild] ";
            case social::ChatChannel::RAID:
                return "[Raid] ";
            case social::ChatChannel::TRADE:
                return "[Trade] ";
            case social::ChatChannel::GENERAL:
                return "[General] ";
            case social::ChatChannel::WHISPER:
                return "[Whisper] ";
            case social::ChatChannel::SYSTEM:
                return "[System] ";
            default:
                return "";
        }
    }
    
    void CalculateHeight() {
        // Would calculate actual height based on text wrapping
        // For now, assume fixed height per line
        const float line_height = 16.0f;
        const float max_width = 400.0f;
        const float char_width = 7.0f;  // Average character width
        
        int estimated_lines = static_cast<int>(
            (formatted_text_.length() * char_width) / max_width) + 1;
        
        height_ = estimated_lines * line_height;
    }
};

// [SEQUENCE: 2468] Chat display window
class ChatWindow : public UIPanel {
public:
    ChatWindow(const std::string& name) : UIPanel(name) {
        SetSize({500, 300});
        SetBackgroundColor({0.0f, 0.0f, 0.0f, 0.7f});
        
        // [SEQUENCE: 2469] Create scrollable message area
        message_area_ = std::make_shared<UIPanel>("MessageArea");
        message_area_->SetPosition({5, 5});
        message_area_->SetSize({490, 240});
        message_area_->SetBackgroundColor({0.0f, 0.0f, 0.0f, 0.0f});
        AddChild(message_area_);
        
        // [SEQUENCE: 2470] Create input box
        input_box_ = std::make_shared<ChatInputBox>("InputBox");
        input_box_->SetPosition({5, 250});
        input_box_->SetSize({490, 25});
        input_box_->SetOnSubmit([this](const std::string& text) {
            OnChatSubmit(text);
        });
        AddChild(input_box_);
        
        // [SEQUENCE: 2471] Create channel selector
        channel_selector_ = std::make_shared<UIButton>("ChannelSelector");
        channel_selector_->SetPosition({5, 280});
        channel_selector_->SetSize({80, 20});
        channel_selector_->SetText("Say");
        channel_selector_->SetOnClick([this]() { CycleChannel(); });
        AddChild(channel_selector_);
        
        // Create scroll buttons
        scroll_up_ = std::make_shared<UIButton>("ScrollUp");
        scroll_up_->SetText("▲");
        scroll_up_->SetPosition({470, 5});
        scroll_up_->SetSize({20, 20});
        scroll_up_->SetOnClick([this]() { ScrollUp(); });
        AddChild(scroll_up_);
        
        scroll_down_ = std::make_shared<UIButton>("ScrollDown");
        scroll_down_->SetText("▼");
        scroll_down_->SetPosition({470, 220});
        scroll_down_->SetSize({20, 20});
        scroll_down_->SetOnClick([this]() { ScrollDown(); });
        AddChild(scroll_down_);
    }
    
    // [SEQUENCE: 2472] Add new message to chat
    void AddMessage(const ChatMessage::MessageData& data) {
        auto message = std::make_shared<ChatMessage>(data);
        messages_.push_back(message);
        
        // Limit message history
        if (messages_.size() > max_messages_) {
            messages_.pop_front();
        }
        
        // Apply channel filter
        if (IsChannelEnabled(data.channel)) {
            UpdateMessageDisplay();
            
            // Auto-scroll to bottom if already at bottom
            if (scroll_position_ >= GetMaxScroll() - 10) {
                ScrollToBottom();
            }
        }
    }
    
    // [SEQUENCE: 2473] Set active chat channel
    void SetActiveChannel(social::ChatChannel channel) {
        active_channel_ = channel;
        UpdateChannelButton();
    }
    
    // [SEQUENCE: 2474] Toggle channel visibility
    void SetChannelEnabled(social::ChatChannel channel, bool enabled) {
        enabled_channels_[channel] = enabled;
        UpdateMessageDisplay();
    }
    
    // [SEQUENCE: 2475] Set message received callback
    void SetOnChatMessage(std::function<void(const std::string&, social::ChatChannel)> callback) {
        on_chat_message_ = callback;
    }
    
protected:
    bool HandleMouseButton(int button, bool pressed, float x, float y) override {
        // Handle mouse wheel for scrolling
        if (button == 3 && pressed) {  // Wheel up
            ScrollUp();
            return true;
        } else if (button == 4 && pressed) {  // Wheel down
            ScrollDown();
            return true;
        }
        
        return UIPanel::HandleMouseButton(button, pressed, x, y);
    }
    
private:
    static constexpr size_t max_messages_ = 1000;
    static constexpr float message_spacing_ = 2.0f;
    
    std::deque<std::shared_ptr<ChatMessage>> messages_;
    std::shared_ptr<UIPanel> message_area_;
    std::shared_ptr<ChatInputBox> input_box_;
    std::shared_ptr<UIButton> channel_selector_;
    std::shared_ptr<UIButton> scroll_up_;
    std::shared_ptr<UIButton> scroll_down_;
    
    social::ChatChannel active_channel_ = social::ChatChannel::SAY;
    std::unordered_map<social::ChatChannel, bool> enabled_channels_;
    
    float scroll_position_ = 0;
    std::vector<std::shared_ptr<UILabel>> message_labels_;
    
    std::function<void(const std::string&, social::ChatChannel)> on_chat_message_;
    
    // [SEQUENCE: 2476] Update visible messages
    void UpdateMessageDisplay() {
        // Clear existing labels
        for (auto& label : message_labels_) {
            message_area_->RemoveChild(label.get());
        }
        message_labels_.clear();
        
        // Calculate visible messages
        float y_offset = -scroll_position_;
        float area_height = message_area_->GetSize().y;
        
        for (const auto& message : messages_) {
            if (!IsChannelEnabled(message->GetData().channel)) {
                continue;
            }
            
            float message_height = message->GetHeight();
            
            // Check if message is visible
            if (y_offset + message_height > 0 && y_offset < area_height) {
                auto label = std::make_shared<UILabel>("Message");
                label->SetText(message->GetFormattedText());
                label->SetPosition({0, y_offset});
                label->SetSize({message_area_->GetSize().x - 20, message_height});
                label->SetTextColor(message->GetData().channel_color);
                label->SetFontSize(14.0f);
                
                message_labels_.push_back(label);
                message_area_->AddChild(label);
            }
            
            y_offset += message_height + message_spacing_;
            
            // Stop if we've filled the visible area
            if (y_offset > area_height + 100) {
                break;
            }
        }
    }
    
    // [SEQUENCE: 2477] Handle chat submission
    void OnChatSubmit(const std::string& text) {
        if (text.empty()) return;
        
        // Check for commands
        if (text[0] == '/') {
            ProcessCommand(text);
        } else {
            // Send regular message
            if (on_chat_message_) {
                on_chat_message_(text, active_channel_);
            }
        }
        
        // Clear input
        input_box_->Clear();
    }
    
    // [SEQUENCE: 2478] Process chat commands
    void ProcessCommand(const std::string& command) {
        // Parse command and arguments
        std::istringstream iss(command);
        std::string cmd;
        iss >> cmd;
        
        if (cmd == "/say" || cmd == "/s") {
            SetActiveChannel(social::ChatChannel::SAY);
        } else if (cmd == "/party" || cmd == "/p") {
            SetActiveChannel(social::ChatChannel::PARTY);
        } else if (cmd == "/guild" || cmd == "/g") {
            SetActiveChannel(social::ChatChannel::GUILD);
        } else if (cmd == "/whisper" || cmd == "/w") {
            std::string target;
            iss >> target;
            if (!target.empty()) {
                SetActiveChannel(social::ChatChannel::WHISPER);
                // Set whisper target
            }
        } else if (cmd == "/help") {
            ShowHelpMessage();
        } else {
            // Unknown command - send to server for processing
            if (on_chat_message_) {
                on_chat_message_(command, social::ChatChannel::SYSTEM);
            }
        }
    }
    
    void CycleChannel() {
        // Cycle through available channels
        switch (active_channel_) {
            case social::ChatChannel::SAY:
                SetActiveChannel(social::ChatChannel::PARTY);
                break;
            case social::ChatChannel::PARTY:
                SetActiveChannel(social::ChatChannel::GUILD);
                break;
            case social::ChatChannel::GUILD:
                SetActiveChannel(social::ChatChannel::TRADE);
                break;
            case social::ChatChannel::TRADE:
                SetActiveChannel(social::ChatChannel::SAY);
                break;
            default:
                SetActiveChannel(social::ChatChannel::SAY);
                break;
        }
    }
    
    void UpdateChannelButton() {
        std::string channel_name;
        Color channel_color;
        
        switch (active_channel_) {
            case social::ChatChannel::SAY:
                channel_name = "Say";
                channel_color = {1.0f, 1.0f, 1.0f, 1.0f};
                break;
            case social::ChatChannel::PARTY:
                channel_name = "Party";
                channel_color = {0.4f, 0.7f, 1.0f, 1.0f};
                break;
            case social::ChatChannel::GUILD:
                channel_name = "Guild";
                channel_color = {0.4f, 1.0f, 0.4f, 1.0f};
                break;
            case social::ChatChannel::TRADE:
                channel_name = "Trade";
                channel_color = {1.0f, 0.6f, 0.4f, 1.0f};
                break;
            default:
                channel_name = "Say";
                channel_color = {1.0f, 1.0f, 1.0f, 1.0f};
                break;
        }
        
        channel_selector_->SetText(channel_name);
        channel_selector_->SetTextColor(channel_color);
    }
    
    bool IsChannelEnabled(social::ChatChannel channel) const {
        auto it = enabled_channels_.find(channel);
        return it != enabled_channels_.end() ? it->second : true;
    }
    
    void ScrollUp() {
        scroll_position_ = std::max(0.0f, scroll_position_ - 50.0f);
        UpdateMessageDisplay();
    }
    
    void ScrollDown() {
        scroll_position_ = std::min(GetMaxScroll(), scroll_position_ + 50.0f);
        UpdateMessageDisplay();
    }
    
    void ScrollToBottom() {
        scroll_position_ = GetMaxScroll();
        UpdateMessageDisplay();
    }
    
    float GetMaxScroll() const {
        float total_height = 0;
        for (const auto& message : messages_) {
            if (IsChannelEnabled(message->GetData().channel)) {
                total_height += message->GetHeight() + message_spacing_;
            }
        }
        
        return std::max(0.0f, total_height - message_area_->GetSize().y);
    }
    
    void ShowHelpMessage() {
        ChatMessage::MessageData help_msg;
        help_msg.channel = social::ChatChannel::SYSTEM;
        help_msg.is_system_message = true;
        help_msg.message_text = "Available commands: /say, /party, /guild, /whisper <player>, /help";
        help_msg.channel_color = {1.0f, 1.0f, 0.0f, 1.0f};
        help_msg.timestamp = std::chrono::system_clock::now();
        
        AddMessage(help_msg);
    }
};

// [SEQUENCE: 2479] Chat input box with text editing
class ChatInputBox : public UIElement {
public:
    ChatInputBox(const std::string& name) : UIElement(name) {
        // Create background
        background_ = std::make_shared<UIPanel>("Background");
        background_->SetSize(GetSize());
        background_->SetBackgroundColor({0.1f, 0.1f, 0.1f, 0.9f});
        background_->SetBorderColor({0.5f, 0.5f, 0.5f, 1.0f});
        background_->SetBorderWidth(1.0f);
        AddChild(background_);
        
        // Create text display
        text_label_ = std::make_shared<UILabel>("TextLabel");
        text_label_->SetPosition({5, 3});
        text_label_->SetSize({GetSize().x - 10, GetSize().y - 6});
        text_label_->SetTextAlign(UILabel::TextAlign::LEFT);
        text_label_->SetFontSize(14.0f);
        AddChild(text_label_);
        
        // Create cursor
        cursor_ = std::make_shared<UIPanel>("Cursor");
        cursor_->SetSize({2, 18});
        cursor_->SetPosition({5, 3});
        cursor_->SetBackgroundColor({1.0f, 1.0f, 1.0f, 1.0f});
        AddChild(cursor_);
    }
    
    // [SEQUENCE: 2480] Text input handling
    void SetText(const std::string& text) {
        text_ = text;
        cursor_position_ = text.length();
        UpdateDisplay();
    }
    
    void Clear() {
        text_.clear();
        cursor_position_ = 0;
        UpdateDisplay();
    }
    
    void SetOnSubmit(std::function<void(const std::string&)> callback) {
        on_submit_ = callback;
    }
    
protected:
    void OnMouseDown() override {
        // Request focus for keyboard input
        UIManager::Instance().SetFocusedElement(this);
        has_focus_ = true;
        UpdateDisplay();
    }
    
    void OnKeyDown(int key) override {
        switch (key) {
            case 8:  // Backspace
                if (cursor_position_ > 0) {
                    text_.erase(cursor_position_ - 1, 1);
                    cursor_position_--;
                    UpdateDisplay();
                }
                break;
                
            case 127:  // Delete
                if (cursor_position_ < text_.length()) {
                    text_.erase(cursor_position_, 1);
                    UpdateDisplay();
                }
                break;
                
            case 13:  // Enter
                if (on_submit_) {
                    on_submit_(text_);
                }
                break;
                
            case 37:  // Left arrow
                if (cursor_position_ > 0) {
                    cursor_position_--;
                    UpdateDisplay();
                }
                break;
                
            case 39:  // Right arrow
                if (cursor_position_ < text_.length()) {
                    cursor_position_++;
                    UpdateDisplay();
                }
                break;
                
            case 36:  // Home
                cursor_position_ = 0;
                UpdateDisplay();
                break;
                
            case 35:  // End
                cursor_position_ = text_.length();
                UpdateDisplay();
                break;
                
            default:
                // Regular character input
                if (key >= 32 && key < 127) {
                    text_.insert(cursor_position_, 1, static_cast<char>(key));
                    cursor_position_++;
                    UpdateDisplay();
                }
                break;
        }
    }
    
    void OnUpdate(float delta_time) override {
        // Blink cursor
        if (has_focus_) {
            cursor_blink_timer_ += delta_time;
            if (cursor_blink_timer_ > 0.5f) {
                cursor_visible_ = !cursor_visible_;
                cursor_blink_timer_ = 0;
                cursor_->SetVisibility(cursor_visible_ ? 
                    Visibility::VISIBLE : Visibility::HIDDEN);
            }
        } else {
            cursor_->SetVisibility(Visibility::HIDDEN);
        }
    }
    
private:
    std::string text_;
    size_t cursor_position_ = 0;
    
    std::shared_ptr<UIPanel> background_;
    std::shared_ptr<UILabel> text_label_;
    std::shared_ptr<UIPanel> cursor_;
    
    bool cursor_visible_ = true;
    float cursor_blink_timer_ = 0;
    
    std::function<void(const std::string&)> on_submit_;
    
    void UpdateDisplay() {
        // Update text display
        text_label_->SetText(text_);
        
        // Update cursor position
        float char_width = 7.0f;  // Approximate character width
        float cursor_x = 5 + cursor_position_ * char_width;
        cursor_->SetPosition({cursor_x, 3});
        
        // Show cursor when focused
        if (has_focus_) {
            cursor_visible_ = true;
            cursor_blink_timer_ = 0;
            cursor_->SetVisibility(Visibility::VISIBLE);
        }
    }
};

// [SEQUENCE: 2481] Chat tab system for multiple chat windows
class ChatTabContainer : public UIPanel {
public:
    ChatTabContainer(const std::string& name) : UIPanel(name) {
        SetSize({600, 350});
        
        // [SEQUENCE: 2482] Create tab bar
        tab_bar_ = std::make_shared<UIPanel>("TabBar");
        tab_bar_->SetPosition({0, 0});
        tab_bar_->SetSize({600, 25});
        tab_bar_->SetBackgroundColor({0.1f, 0.1f, 0.1f, 0.9f});
        AddChild(tab_bar_);
        
        // Create default tabs
        CreateTab("General", {"General", "Trade", "LocalDefense"});
        CreateTab("Combat", {"Combat", "Loot", "Experience"});
        CreateTab("Guild", {"Guild", "Officer"});
        
        // Select first tab
        SelectTab(0);
    }
    
    // [SEQUENCE: 2483] Create new chat tab
    void CreateTab(const std::string& name, const std::vector<std::string>& channels) {
        auto tab_button = std::make_shared<UIButton>("Tab_" + name);
        tab_button->SetText(name);
        tab_button->SetSize({80, 23});
        tab_button->SetPosition({static_cast<float>(tabs_.size() * 85), 1});
        
        int tab_index = tabs_.size();
        tab_button->SetOnClick([this, tab_index]() { SelectTab(tab_index); });
        
        tab_bar_->AddChild(tab_button);
        
        // Create chat window for this tab
        auto chat_window = std::make_shared<ChatWindow>("ChatWindow_" + name);
        chat_window->SetPosition({0, 25});
        chat_window->SetSize({600, 325});
        
        // Enable specified channels
        for (const auto& channel_name : channels) {
            // Map channel name to enum
            auto channel = GetChannelFromName(channel_name);
            chat_window->SetChannelEnabled(channel, true);
        }
        
        tabs_.push_back({name, tab_button, chat_window, channels});
        AddChild(chat_window);
        
        // Hide all but first tab
        if (tabs_.size() > 1) {
            chat_window->SetVisibility(Visibility::HIDDEN);
        }
    }
    
    // [SEQUENCE: 2484] Add message to all relevant tabs
    void AddMessage(const ChatMessage::MessageData& data) {
        for (auto& tab : tabs_) {
            tab.window->AddMessage(data);
        }
    }
    
    void SelectTab(int index) {
        if (index < 0 || index >= tabs_.size()) return;
        
        // Hide current tab
        if (current_tab_ >= 0 && current_tab_ < tabs_.size()) {
            tabs_[current_tab_].window->SetVisibility(Visibility::HIDDEN);
            tabs_[current_tab_].button->SetButtonColors(
                {0.2f, 0.2f, 0.2f, 0.8f},
                {0.3f, 0.3f, 0.3f, 0.8f},
                {0.4f, 0.4f, 0.2f, 0.8f},
                {0.1f, 0.1f, 0.1f, 0.5f}
            );
        }
        
        // Show new tab
        current_tab_ = index;
        tabs_[current_tab_].window->SetVisibility(Visibility::VISIBLE);
        tabs_[current_tab_].button->SetButtonColors(
            {0.4f, 0.4f, 0.4f, 1.0f},
            {0.5f, 0.5f, 0.5f, 1.0f},
            {0.3f, 0.3f, 0.3f, 1.0f},
            {0.2f, 0.2f, 0.2f, 0.5f}
        );
    }
    
private:
    struct TabData {
        std::string name;
        std::shared_ptr<UIButton> button;
        std::shared_ptr<ChatWindow> window;
        std::vector<std::string> channels;
    };
    
    std::vector<TabData> tabs_;
    std::shared_ptr<UIPanel> tab_bar_;
    int current_tab_ = -1;
    
    social::ChatChannel GetChannelFromName(const std::string& name) {
        static std::unordered_map<std::string, social::ChatChannel> channel_map = {
            {"General", social::ChatChannel::GENERAL},
            {"Trade", social::ChatChannel::TRADE},
            {"Guild", social::ChatChannel::GUILD},
            {"Party", social::ChatChannel::PARTY},
            {"Raid", social::ChatChannel::RAID},
            {"Say", social::ChatChannel::SAY},
            {"Yell", social::ChatChannel::YELL},
            {"Combat", social::ChatChannel::SYSTEM},
            {"Loot", social::ChatChannel::SYSTEM},
            {"Experience", social::ChatChannel::SYSTEM},
            {"Officer", social::ChatChannel::GUILD}
        };
        
        auto it = channel_map.find(name);
        return it != channel_map.end() ? it->second : social::ChatChannel::GENERAL;
    }
};

// [SEQUENCE: 2485] Combat log window
class CombatLogWindow : public ChatWindow {
public:
    CombatLogWindow(const std::string& name) : ChatWindow(name) {
        SetTitle("Combat Log");
        
        // [SEQUENCE: 2486] Create filter buttons
        CreateFilterButtons();
        
        // Enable combat-related channels
        SetChannelEnabled(social::ChatChannel::SYSTEM, true);
        
        // Set combat-specific colors
        damage_color_ = {1.0f, 0.5f, 0.5f, 1.0f};
        healing_color_ = {0.5f, 1.0f, 0.5f, 1.0f};
        buff_color_ = {0.5f, 0.5f, 1.0f, 1.0f};
    }
    
    // [SEQUENCE: 2487] Add combat event
    void AddCombatEvent(const std::string& event_type, const std::string& source,
                       const std::string& target, int amount, const std::string& ability = "") {
        
        ChatMessage::MessageData msg;
        msg.channel = social::ChatChannel::SYSTEM;
        msg.is_system_message = true;
        msg.timestamp = std::chrono::system_clock::now();
        
        // Format combat message
        if (event_type == "damage") {
            msg.message_text = source + " hits " + target + " for " + 
                              std::to_string(amount) + " damage";
            if (!ability.empty()) {
                msg.message_text += " with " + ability;
            }
            msg.channel_color = damage_color_;
        } else if (event_type == "heal") {
            msg.message_text = source + " heals " + target + " for " + 
                              std::to_string(amount);
            msg.channel_color = healing_color_;
        } else if (event_type == "buff") {
            msg.message_text = source + " casts " + ability + " on " + target;
            msg.channel_color = buff_color_;
        }
        
        AddMessage(msg);
    }
    
private:
    Color damage_color_;
    Color healing_color_;
    Color buff_color_;
    
    std::unordered_map<std::string, bool> event_filters_;
    
    void CreateFilterButtons() {
        // Create filter checkboxes for different combat events
        float y_offset = 305;
        
        auto damage_filter = std::make_shared<UIButton>("DamageFilter");
        damage_filter->SetText("Damage");
        damage_filter->SetPosition({10, y_offset});
        damage_filter->SetSize({70, 20});
        damage_filter->SetOnClick([this]() { ToggleFilter("damage"); });
        AddChild(damage_filter);
        
        auto healing_filter = std::make_shared<UIButton>("HealingFilter");
        healing_filter->SetText("Healing");
        healing_filter->SetPosition({85, y_offset});
        healing_filter->SetSize({70, 20});
        healing_filter->SetOnClick([this]() { ToggleFilter("healing"); });
        AddChild(healing_filter);
        
        auto buff_filter = std::make_shared<UIButton>("BuffFilter");
        buff_filter->SetText("Buffs");
        buff_filter->SetPosition({160, y_offset});
        buff_filter->SetSize({70, 20});
        buff_filter->SetOnClick([this]() { ToggleFilter("buffs"); });
        AddChild(buff_filter);
    }
    
    void ToggleFilter(const std::string& filter) {
        event_filters_[filter] = !event_filters_[filter];
        UpdateMessageDisplay();
    }
};

// [SEQUENCE: 2488] Chat UI manager
class ChatUIManager {
public:
    static ChatUIManager& Instance() {
        static ChatUIManager instance;
        return instance;
    }
    
    void Initialize() {
        auto root = UIManager::Instance().GetRoot();
        if (!root) return;
        
        // [SEQUENCE: 2489] Create main chat window
        main_chat_ = std::make_shared<ChatTabContainer>("MainChat");
        main_chat_->SetPosition({10, 400});
        main_chat_->SetAnchor(AnchorType::BOTTOM_LEFT);
        root->AddChild(main_chat_);
        
        // [SEQUENCE: 2490] Create combat log
        combat_log_ = std::make_shared<CombatLogWindow>("CombatLog");
        combat_log_->SetPosition({620, 400});
        combat_log_->SetAnchor(AnchorType::BOTTOM_LEFT);
        combat_log_->SetVisibility(Visibility::HIDDEN);
        root->AddChild(combat_log_);
        
        // Set default channel colors
        InitializeChannelColors();
    }
    
    // [SEQUENCE: 2491] Add chat message
    void AddChatMessage(const std::string& sender, const std::string& message,
                       social::ChatChannel channel) {
        
        ChatMessage::MessageData msg_data;
        msg_data.sender_name = sender;
        msg_data.message_text = message;
        msg_data.channel = channel;
        msg_data.timestamp = std::chrono::system_clock::now();
        msg_data.channel_color = GetChannelColor(channel);
        
        if (main_chat_) {
            main_chat_->AddMessage(msg_data);
        }
    }
    
    void AddSystemMessage(const std::string& message) {
        ChatMessage::MessageData msg_data;
        msg_data.message_text = message;
        msg_data.channel = social::ChatChannel::SYSTEM;
        msg_data.is_system_message = true;
        msg_data.timestamp = std::chrono::system_clock::now();
        msg_data.channel_color = GetChannelColor(social::ChatChannel::SYSTEM);
        
        if (main_chat_) {
            main_chat_->AddMessage(msg_data);
        }
    }
    
    void AddCombatEvent(const std::string& event_type, const std::string& source,
                       const std::string& target, int amount, const std::string& ability = "") {
        if (combat_log_) {
            combat_log_->AddCombatEvent(event_type, source, target, amount, ability);
        }
    }
    
    void ToggleCombatLog() {
        if (combat_log_) {
            auto vis = combat_log_->IsVisible() ? 
                Visibility::HIDDEN : Visibility::VISIBLE;
            combat_log_->SetVisibility(vis);
        }
    }
    
    // [SEQUENCE: 2492] Set chat message callback
    void SetOnChatMessage(std::function<void(const std::string&, social::ChatChannel)> callback) {
        on_chat_message_ = callback;
        
        // Set callback for all chat windows
        if (main_chat_) {
            for (int i = 0; i < 3; ++i) {  // Assuming 3 tabs
                // Would set callback for each tab's window
            }
        }
    }
    
private:
    ChatUIManager() = default;
    
    std::shared_ptr<ChatTabContainer> main_chat_;
    std::shared_ptr<CombatLogWindow> combat_log_;
    
    std::unordered_map<social::ChatChannel, Color> channel_colors_;
    std::function<void(const std::string&, social::ChatChannel)> on_chat_message_;
    
    void InitializeChannelColors() {
        channel_colors_[social::ChatChannel::SAY] = {1.0f, 1.0f, 1.0f, 1.0f};
        channel_colors_[social::ChatChannel::YELL] = {1.0f, 0.4f, 0.4f, 1.0f};
        channel_colors_[social::ChatChannel::PARTY] = {0.4f, 0.7f, 1.0f, 1.0f};
        channel_colors_[social::ChatChannel::GUILD] = {0.4f, 1.0f, 0.4f, 1.0f};
        channel_colors_[social::ChatChannel::RAID] = {1.0f, 0.5f, 0.0f, 1.0f};
        channel_colors_[social::ChatChannel::TRADE] = {1.0f, 0.6f, 0.4f, 1.0f};
        channel_colors_[social::ChatChannel::GENERAL] = {1.0f, 0.8f, 0.6f, 1.0f};
        channel_colors_[social::ChatChannel::WHISPER] = {1.0f, 0.5f, 1.0f, 1.0f};
        channel_colors_[social::ChatChannel::SYSTEM] = {1.0f, 1.0f, 0.0f, 1.0f};
    }
    
    Color GetChannelColor(social::ChatChannel channel) const {
        auto it = channel_colors_.find(channel);
        return it != channel_colors_.end() ? it->second : Color::White();
    }
};

} // namespace mmorpg::ui