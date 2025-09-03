#pragma once

#include "ui_framework.h"
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <spdlog/spdlog.h>

namespace mmorpg::ui {

// [SEQUENCE: 2394] HUD (Heads-Up Display) elements for gameplay interface
// HUD ”Œ - Œ„t x0˜t¤| \ Ü¤\

// [SEQUENCE: 2395] Health bar widget
class HealthBar : public UIElement {
public:
    HealthBar(const std::string& name) : UIElement(name) {
        // Create background
        background_ = std::make_shared<UIPanel>("Background");
        background_->SetBackgroundColor({0.1f, 0.1f, 0.1f, 0.8f});
        background_->SetSize({200, 30});
        AddChild(background_);
        
        // Create health fill
        health_fill_ = std::make_shared<UIProgressBar>("HealthFill");
        health_fill_->SetColors({0.2f, 0.0f, 0.0f, 0.8f}, {0.8f, 0.2f, 0.2f, 1.0f});
        health_fill_->SetSize({196, 26});
        health_fill_->SetPosition({2, 2});
        health_fill_->SetShowText(false);
        background_->AddChild(health_fill_);
        
        // Create shield fill (overlay)
        shield_fill_ = std::make_shared<UIProgressBar>("ShieldFill");
        shield_fill_->SetColors({0.0f, 0.0f, 0.0f, 0.0f}, {0.4f, 0.6f, 1.0f, 0.8f});
        shield_fill_->SetSize({196, 26});
        shield_fill_->SetPosition({2, 2});
        shield_fill_->SetShowText(false);
        background_->AddChild(shield_fill_);
        
        // Create text label
        health_text_ = std::make_shared<UILabel>("HealthText");
        health_text_->SetTextAlign(UILabel::TextAlign::CENTER);
        health_text_->SetSize({200, 30});
        health_text_->SetTextColor(Color::White());
        AddChild(health_text_);
        
        // Size to contain all elements
        SetSize({200, 30});
    }
    
    // [SEQUENCE: 2396] Update health values
    void SetHealth(float current, float max) {
        current_health_ = current;
        max_health_ = max;
        
        float health_percent = (max > 0) ? current / max : 0;
        health_fill_->SetValue(health_percent);
        
        // Update text
        std::string text = std::to_string(static_cast<int>(current)) + " / " + 
                          std::to_string(static_cast<int>(max));
        health_text_->SetText(text);
        
        // Flash effect on damage
        if (current < last_health_) {
            StartFlashEffect({1.0f, 0.0f, 0.0f, 0.5f});
        }
        
        last_health_ = current;
    }
    
    void SetShield(float current, float max) {
        float shield_percent = (max > 0) ? current / max : 0;
        shield_fill_->SetValue(shield_percent);
    }
    
    void ShowCombatText(const std::string& text, const Color& color) {
        // Create floating combat text
        auto combat_text = std::make_shared<FloatingText>(text, color);
        combat_text->SetPosition({100, -10});
        AddChild(combat_text);
        floating_texts_.push_back(combat_text);
    }
    
protected:
    void OnUpdate(float delta_time) override {
        // Update flash effect
        if (flash_timer_ > 0) {
            flash_timer_ -= delta_time;
            float alpha = flash_timer_ / flash_duration_;
            flash_overlay_->SetAlpha(alpha);
        }
        
        // Clean up expired floating texts
        floating_texts_.erase(
            std::remove_if(floating_texts_.begin(), floating_texts_.end(),
                [](const std::shared_ptr<FloatingText>& text) {
                    return text->IsExpired();
                }),
            floating_texts_.end()
        );
    }
    
private:
    std::shared_ptr<UIPanel> background_;
    std::shared_ptr<UIProgressBar> health_fill_;
    std::shared_ptr<UIProgressBar> shield_fill_;
    std::shared_ptr<UILabel> health_text_;
    std::shared_ptr<UIPanel> flash_overlay_;
    
    float current_health_ = 100;
    float max_health_ = 100;
    float last_health_ = 100;
    
    float flash_timer_ = 0;
    float flash_duration_ = 0.3f;
    
    std::vector<std::shared_ptr<FloatingText>> floating_texts_;
    
    void StartFlashEffect(const Color& color) {
        if (!flash_overlay_) {
            flash_overlay_ = std::make_shared<UIPanel>("FlashOverlay");
            flash_overlay_->SetSize(GetSize());
            AddChild(flash_overlay_);
        }
        
        flash_overlay_->SetBackgroundColor(color);
        flash_timer_ = flash_duration_;
    }
};

// [SEQUENCE: 2397] Mana/Resource bar
class ResourceBar : public UIElement {
public:
    enum class ResourceType {
        MANA,               // È˜ (€É)
        ENERGY,             // ÐÀ (x€É)
        RAGE,               // „x (hÉ)
        FOCUS,              // Ñ (üiÉ)
        RUNIC_POWER,        // ì È% (X˜É)
        CUSTOM              // ä¤@
    };
    
    ResourceBar(const std::string& name, ResourceType type = ResourceType::MANA) 
        : UIElement(name), resource_type_(type) {
        
        // Create background
        background_ = std::make_shared<UIPanel>("Background");
        background_->SetBackgroundColor({0.1f, 0.1f, 0.1f, 0.8f});
        background_->SetSize({200, 20});
        AddChild(background_);
        
        // Create resource fill
        resource_fill_ = std::make_shared<UIProgressBar>("ResourceFill");
        SetResourceColor(type);
        resource_fill_->SetSize({196, 16});
        resource_fill_->SetPosition({2, 2});
        resource_fill_->SetShowText(false);
        background_->AddChild(resource_fill_);
        
        // Create text
        resource_text_ = std::make_shared<UILabel>("ResourceText");
        resource_text_->SetTextAlign(UILabel::TextAlign::CENTER);
        resource_text_->SetSize({200, 20});
        resource_text_->SetFontSize(12.0f);
        AddChild(resource_text_);
        
        SetSize({200, 20});
    }
    
    void SetResource(float current, float max) {
        current_resource_ = current;
        max_resource_ = max;
        
        float percent = (max > 0) ? current / max : 0;
        resource_fill_->SetValue(percent);
        
        // Update text
        std::string text = std::to_string(static_cast<int>(current)) + " / " + 
                          std::to_string(static_cast<int>(max));
        resource_text_->SetText(text);
    }
    
private:
    ResourceType resource_type_;
    std::shared_ptr<UIPanel> background_;
    std::shared_ptr<UIProgressBar> resource_fill_;
    std::shared_ptr<UILabel> resource_text_;
    
    float current_resource_ = 100;
    float max_resource_ = 100;
    
    void SetResourceColor(ResourceType type) {
        Color fill_color;
        
        switch (type) {
            case ResourceType::MANA:
                fill_color = {0.2f, 0.4f, 1.0f, 1.0f};
                break;
            case ResourceType::ENERGY:
                fill_color = {1.0f, 1.0f, 0.2f, 1.0f};
                break;
            case ResourceType::RAGE:
                fill_color = {1.0f, 0.2f, 0.2f, 1.0f};
                break;
            case ResourceType::FOCUS:
                fill_color = {1.0f, 0.6f, 0.2f, 1.0f};
                break;
            case ResourceType::RUNIC_POWER:
                fill_color = {0.2f, 0.8f, 1.0f, 1.0f};
                break;
            default:
                fill_color = {0.5f, 0.5f, 0.5f, 1.0f};
                break;
        }
        
        resource_fill_->SetColors({0.1f, 0.1f, 0.1f, 0.8f}, fill_color);
    }
};

// [SEQUENCE: 2398] Experience bar
class ExperienceBar : public UIElement {
public:
    ExperienceBar(const std::string& name) : UIElement(name) {
        // Create slim background
        background_ = std::make_shared<UIPanel>("Background");
        background_->SetBackgroundColor({0.1f, 0.1f, 0.1f, 0.8f});
        background_->SetSize({800, 8});
        AddChild(background_);
        
        // Create XP fill
        xp_fill_ = std::make_shared<UIProgressBar>("XPFill");
        xp_fill_->SetColors({0.2f, 0.1f, 0.4f, 0.8f}, {0.6f, 0.3f, 1.0f, 1.0f});
        xp_fill_->SetSize({796, 4});
        xp_fill_->SetPosition({2, 2});
        xp_fill_->SetShowText(false);
        background_->AddChild(xp_fill_);
        
        // Create rested XP indicator
        rested_fill_ = std::make_shared<UIProgressBar>("RestedFill");
        rested_fill_->SetColors({0.0f, 0.0f, 0.0f, 0.0f}, {0.4f, 0.6f, 1.0f, 0.5f});
        rested_fill_->SetSize({796, 4});
        rested_fill_->SetPosition({2, 2});
        rested_fill_->SetShowText(false);
        background_->AddChild(rested_fill_);
        
        // Level indicators
        level_text_ = std::make_shared<UILabel>("LevelText");
        level_text_->SetTextAlign(UILabel::TextAlign::CENTER);
        level_text_->SetPosition({-50, -5});
        level_text_->SetSize({40, 20});
        AddChild(level_text_);
        
        // Tooltip on hover
        SetSize({800, 8});
    }
    
    // [SEQUENCE: 2399] Update experience values
    void SetExperience(uint64_t current, uint64_t needed, uint32_t level) {
        current_xp_ = current;
        needed_xp_ = needed;
        current_level_ = level;
        
        float xp_percent = (needed > 0) ? static_cast<float>(current) / needed : 0;
        xp_fill_->SetValue(xp_percent);
        
        level_text_->SetText("Lv " + std::to_string(level));
    }
    
    void SetRestedExperience(uint64_t rested) {
        rested_xp_ = rested;
        
        float rested_percent = (needed_xp_ > 0) ? 
            static_cast<float>(rested) / needed_xp_ : 0;
        rested_fill_->SetValue(rested_percent);
    }
    
protected:
    void OnMouseEnter() override {
        // Show detailed tooltip
        std::string tooltip = "Level " + std::to_string(current_level_) + "\n";
        tooltip += "Experience: " + std::to_string(current_xp_) + " / " + 
                   std::to_string(needed_xp_) + "\n";
        
        if (rested_xp_ > 0) {
            tooltip += "Rested: " + std::to_string(rested_xp_);
        }
        
        UIManager::Instance().ShowTooltip(tooltip, 
            GetWorldPosition().x, GetWorldPosition().y - 50);
    }
    
    void OnMouseLeave() override {
        UIManager::Instance().HideTooltip();
    }
    
private:
    std::shared_ptr<UIPanel> background_;
    std::shared_ptr<UIProgressBar> xp_fill_;
    std::shared_ptr<UIProgressBar> rested_fill_;
    std::shared_ptr<UILabel> level_text_;
    
    uint64_t current_xp_ = 0;
    uint64_t needed_xp_ = 1000;
    uint64_t rested_xp_ = 0;
    uint32_t current_level_ = 1;
};

// [SEQUENCE: 2400] Action bar for abilities
class ActionBar : public UIElement {
public:
    ActionBar(const std::string& name, int slot_count = 12) 
        : UIElement(name), slot_count_(slot_count) {
        
        // Create background
        background_ = std::make_shared<UIPanel>("Background");
        background_->SetBackgroundColor({0.0f, 0.0f, 0.0f, 0.6f});
        
        // Create slots
        float slot_size = 50.0f;
        float slot_spacing = 5.0f;
        
        for (int i = 0; i < slot_count; ++i) {
            auto slot = std::make_shared<ActionSlot>("Slot" + std::to_string(i));
            slot->SetSize({slot_size, slot_size});
            slot->SetPosition({i * (slot_size + slot_spacing), 0});
            slot->SetSlotIndex(i);
            slot->SetKeybind(GetDefaultKeybind(i));
            
            slots_.push_back(slot);
            AddChild(slot);
        }
        
        // Size to fit all slots
        float total_width = slot_count * slot_size + (slot_count - 1) * slot_spacing;
        SetSize({total_width, slot_size});
        background_->SetSize(GetSize());
    }
    
    // [SEQUENCE: 2401] Set ability in slot
    void SetAbility(int slot_index, uint32_t ability_id, uint32_t icon_id) {
        if (slot_index >= 0 && slot_index < slots_.size()) {
            slots_[slot_index]->SetAbility(ability_id, icon_id);
        }
    }
    
    void SetCooldown(int slot_index, float remaining, float total) {
        if (slot_index >= 0 && slot_index < slots_.size()) {
            slots_[slot_index]->SetCooldown(remaining, total);
        }
    }
    
    void SetCharges(int slot_index, int current, int max) {
        if (slot_index >= 0 && slot_index < slots_.size()) {
            slots_[slot_index]->SetCharges(current, max);
        }
    }
    
private:
    int slot_count_;
    std::shared_ptr<UIPanel> background_;
    std::vector<std::shared_ptr<ActionSlot>> slots_;
    
    std::string GetDefaultKeybind(int index) {
        if (index < 10) {
            return std::to_string((index + 1) % 10);
        } else if (index == 10) {
            return "-";
        } else if (index == 11) {
            return "=";
        }
        return "";
    }
};

// [SEQUENCE: 2402] Individual action slot
class ActionSlot : public UIButton {
public:
    ActionSlot(const std::string& name) : UIButton(name) {
        // Create icon
        icon_ = std::make_shared<UIImage>("Icon");
        icon_->SetSize({46, 46});
        icon_->SetPosition({2, 2});
        AddChild(icon_);
        
        // Create cooldown overlay
        cooldown_overlay_ = std::make_shared<UIPanel>("CooldownOverlay");
        cooldown_overlay_->SetBackgroundColor({0.0f, 0.0f, 0.0f, 0.7f});
        cooldown_overlay_->SetSize({46, 46});
        cooldown_overlay_->SetPosition({2, 2});
        cooldown_overlay_->SetVisibility(Visibility::HIDDEN);
        AddChild(cooldown_overlay_);
        
        // Create cooldown text
        cooldown_text_ = std::make_shared<UILabel>("CooldownText");
        cooldown_text_->SetTextAlign(UILabel::TextAlign::CENTER);
        cooldown_text_->SetSize({50, 50});
        cooldown_text_->SetFontSize(18.0f);
        cooldown_text_->SetTextColor({1.0f, 1.0f, 0.0f, 1.0f});
        AddChild(cooldown_text_);
        
        // Create keybind text
        keybind_text_ = std::make_shared<UILabel>("KeybindText");
        keybind_text_->SetText("");
        keybind_text_->SetPosition({2, 2});
        keybind_text_->SetFontSize(10.0f);
        AddChild(keybind_text_);
        
        // Create charges text
        charges_text_ = std::make_shared<UILabel>("ChargesText");
        charges_text_->SetTextAlign(UILabel::TextAlign::RIGHT);
        charges_text_->SetPosition({30, 30});
        charges_text_->SetSize({18, 18});
        charges_text_->SetFontSize(12.0f);
        AddChild(charges_text_);
        
        // Border for empty slots
        SetButtonColors(
            {0.2f, 0.2f, 0.2f, 0.8f},  // Normal
            {0.3f, 0.3f, 0.3f, 0.8f},  // Hover
            {0.4f, 0.4f, 0.2f, 0.8f},  // Pressed
            {0.1f, 0.1f, 0.1f, 0.5f}   // Disabled
        );
    }
    
    void SetSlotIndex(int index) { slot_index_ = index; }
    
    void SetKeybind(const std::string& keybind) {
        keybind_text_->SetText(keybind);
    }
    
    void SetAbility(uint32_t ability_id, uint32_t icon_id) {
        ability_id_ = ability_id;
        icon_->SetTexture(icon_id);
        
        if (ability_id == 0) {
            icon_->SetVisibility(Visibility::HIDDEN);
        } else {
            icon_->SetVisibility(Visibility::VISIBLE);
        }
    }
    
    // [SEQUENCE: 2403] Cooldown management
    void SetCooldown(float remaining, float total) {
        cooldown_remaining_ = remaining;
        cooldown_total_ = total;
        
        if (remaining > 0) {
            cooldown_overlay_->SetVisibility(Visibility::VISIBLE);
            
            // Update cooldown sweep
            float percent = 1.0f - (remaining / total);
            UpdateCooldownSweep(percent);
            
            // Update text
            if (remaining > 60) {
                int minutes = static_cast<int>(remaining / 60);
                cooldown_text_->SetText(std::to_string(minutes) + "m");
            } else if (remaining > 10) {
                cooldown_text_->SetText(std::to_string(static_cast<int>(remaining)));
            } else {
                cooldown_text_->SetText(std::to_string(remaining).substr(0, 3));
            }
        } else {
            cooldown_overlay_->SetVisibility(Visibility::HIDDEN);
            cooldown_text_->SetText("");
        }
    }
    
    void SetCharges(int current, int max) {
        if (max > 1) {
            charges_text_->SetText(std::to_string(current));
            charges_text_->SetTextColor(current == 0 ? 
                Color::Red() : Color::White());
        } else {
            charges_text_->SetText("");
        }
    }
    
protected:
    void OnClick() override {
        if (ability_id_ != 0 && cooldown_remaining_ <= 0) {
            // Trigger ability use
            spdlog::info("Action slot {} clicked: ability {}", 
                        slot_index_, ability_id_);
        }
    }
    
    void OnMouseEnter() override {
        UIButton::OnMouseEnter();
        
        if (ability_id_ != 0) {
            // Show ability tooltip
            ShowAbilityTooltip();
        }
    }
    
    void OnMouseLeave() override {
        UIButton::OnMouseLeave();
        UIManager::Instance().HideTooltip();
    }
    
private:
    int slot_index_ = 0;
    uint32_t ability_id_ = 0;
    
    std::shared_ptr<UIImage> icon_;
    std::shared_ptr<UIPanel> cooldown_overlay_;
    std::shared_ptr<UILabel> cooldown_text_;
    std::shared_ptr<UILabel> keybind_text_;
    std::shared_ptr<UILabel> charges_text_;
    
    float cooldown_remaining_ = 0;
    float cooldown_total_ = 0;
    
    void UpdateCooldownSweep(float percent) {
        // This would update a radial sweep effect
        // For now, just adjust overlay alpha
        float alpha = 0.7f * (1.0f - percent);
        cooldown_overlay_->SetBackgroundColor({0.0f, 0.0f, 0.0f, alpha});
    }
    
    void ShowAbilityTooltip() {
        // Would fetch ability data and show detailed tooltip
        std::string tooltip = "Ability " + std::to_string(ability_id_) + "\n";
        tooltip += "Click to cast\n";
        
        if (cooldown_total_ > 0) {
            tooltip += "Cooldown: " + std::to_string(static_cast<int>(cooldown_total_)) + "s";
        }
        
        UIManager::Instance().ShowTooltip(tooltip, 
            GetWorldPosition().x, GetWorldPosition().y - 100);
    }
};

// [SEQUENCE: 2404] Target frame
class TargetFrame : public UIElement {
public:
    TargetFrame(const std::string& name) : UIElement(name) {
        // Create background panel
        background_ = std::make_shared<UIPanel>("Background");
        background_->SetBackgroundColor({0.1f, 0.1f, 0.1f, 0.9f});
        background_->SetSize({250, 80});
        AddChild(background_);
        
        // Portrait
        portrait_ = std::make_shared<UIImage>("Portrait");
        portrait_->SetSize({64, 64});
        portrait_->SetPosition({8, 8});
        background_->AddChild(portrait_);
        
        // Name label
        name_label_ = std::make_shared<UILabel>("NameLabel");
        name_label_->SetPosition({80, 8});
        name_label_->SetSize({160, 20});
        name_label_->SetFontSize(14.0f);
        background_->AddChild(name_label_);
        
        // Level label
        level_label_ = std::make_shared<UILabel>("LevelLabel");
        level_label_->SetPosition({80, 28});
        level_label_->SetSize({160, 16});
        level_label_->SetFontSize(12.0f);
        background_->AddChild(level_label_);
        
        // Health bar
        health_bar_ = std::make_shared<HealthBar>("TargetHealth");
        health_bar_->SetPosition({80, 46});
        health_bar_->SetSize({160, 20});
        background_->AddChild(health_bar_);
        
        // Cast bar
        cast_bar_ = std::make_shared<CastBar>("TargetCast");
        cast_bar_->SetPosition({0, 85});
        cast_bar_->SetSize({250, 20});
        cast_bar_->SetVisibility(Visibility::HIDDEN);
        AddChild(cast_bar_);
        
        // Buffs/Debuffs
        buff_container_ = std::make_shared<BuffContainer>("Buffs");
        buff_container_->SetPosition({0, 110});
        AddChild(buff_container_);
        
        SetSize({250, 150});
        SetVisibility(Visibility::HIDDEN);  // Hidden until target selected
    }
    
    // [SEQUENCE: 2405] Set target information
    void SetTarget(uint64_t target_id, const std::string& name, uint32_t level,
                  const std::string& class_name, uint32_t portrait_id) {
        
        if (target_id == 0) {
            SetVisibility(Visibility::HIDDEN);
            return;
        }
        
        SetVisibility(Visibility::VISIBLE);
        target_id_ = target_id;
        
        name_label_->SetText(name);
        level_label_->SetText("Level " + std::to_string(level) + " " + class_name);
        portrait_->SetTexture(portrait_id);
        
        // Color name based on hostility
        UpdateNameColor();
    }
    
    void UpdateHealth(float current, float max) {
        health_bar_->SetHealth(current, max);
    }
    
    void ShowCasting(const std::string& spell_name, float cast_time, bool interruptible) {
        cast_bar_->StartCast(spell_name, cast_time, interruptible);
        cast_bar_->SetVisibility(Visibility::VISIBLE);
    }
    
    void StopCasting() {
        cast_bar_->StopCast();
        cast_bar_->SetVisibility(Visibility::HIDDEN);
    }
    
private:
    uint64_t target_id_ = 0;
    
    std::shared_ptr<UIPanel> background_;
    std::shared_ptr<UIImage> portrait_;
    std::shared_ptr<UILabel> name_label_;
    std::shared_ptr<UILabel> level_label_;
    std::shared_ptr<HealthBar> health_bar_;
    std::shared_ptr<CastBar> cast_bar_;
    std::shared_ptr<BuffContainer> buff_container_;
    
    void UpdateNameColor() {
        // Would check faction/hostility
        // Red for hostile, Yellow for neutral, Green for friendly
        name_label_->SetTextColor({1.0f, 0.2f, 0.2f, 1.0f});  // Hostile
    }
};

// [SEQUENCE: 2406] Cast bar widget
class CastBar : public UIElement {
public:
    CastBar(const std::string& name) : UIElement(name) {
        // Background
        background_ = std::make_shared<UIPanel>("Background");
        background_->SetBackgroundColor({0.1f, 0.1f, 0.1f, 0.9f});
        background_->SetSize({250, 20});
        AddChild(background_);
        
        // Cast progress
        cast_fill_ = std::make_shared<UIProgressBar>("CastFill");
        cast_fill_->SetColors({0.2f, 0.2f, 0.2f, 0.8f}, {1.0f, 0.8f, 0.2f, 1.0f});
        cast_fill_->SetSize({246, 16});
        cast_fill_->SetPosition({2, 2});
        cast_fill_->SetShowText(false);
        background_->AddChild(cast_fill_);
        
        // Spell name
        spell_label_ = std::make_shared<UILabel>("SpellName");
        spell_label_->SetTextAlign(UILabel::TextAlign::CENTER);
        spell_label_->SetSize({250, 20});
        spell_label_->SetFontSize(12.0f);
        AddChild(spell_label_);
        
        // Cast time
        time_label_ = std::make_shared<UILabel>("CastTime");
        time_label_->SetTextAlign(UILabel::TextAlign::RIGHT);
        time_label_->SetPosition({200, 0});
        time_label_->SetSize({45, 20});
        time_label_->SetFontSize(10.0f);
        AddChild(time_label_);
        
        SetSize({250, 20});
    }
    
    void StartCast(const std::string& spell_name, float cast_time, bool interruptible) {
        spell_name_ = spell_name;
        cast_time_ = cast_time;
        elapsed_time_ = 0;
        is_casting_ = true;
        interruptible_ = interruptible;
        
        spell_label_->SetText(spell_name);
        
        // Set color based on interruptible
        if (interruptible) {
            cast_fill_->SetColors({0.2f, 0.2f, 0.2f, 0.8f}, {1.0f, 0.8f, 0.2f, 1.0f});
        } else {
            cast_fill_->SetColors({0.2f, 0.2f, 0.2f, 0.8f}, {0.8f, 0.2f, 0.2f, 1.0f});
        }
    }
    
    void StopCast() {
        is_casting_ = false;
        cast_fill_->SetValue(0);
    }
    
protected:
    void OnUpdate(float delta_time) override {
        if (is_casting_) {
            elapsed_time_ += delta_time;
            
            if (elapsed_time_ >= cast_time_) {
                // Cast complete
                is_casting_ = false;
                cast_fill_->SetValue(1.0f);
            } else {
                // Update progress
                float progress = elapsed_time_ / cast_time_;
                cast_fill_->SetValue(progress);
                
                // Update time text
                float remaining = cast_time_ - elapsed_time_;
                time_label_->SetText(std::to_string(remaining).substr(0, 3));
            }
        }
    }
    
private:
    std::shared_ptr<UIPanel> background_;
    std::shared_ptr<UIProgressBar> cast_fill_;
    std::shared_ptr<UILabel> spell_label_;
    std::shared_ptr<UILabel> time_label_;
    
    std::string spell_name_;
    float cast_time_ = 0;
    float elapsed_time_ = 0;
    bool is_casting_ = false;
    bool interruptible_ = true;
};

// [SEQUENCE: 2407] Buff/Debuff container
class BuffContainer : public UIElement {
public:
    BuffContainer(const std::string& name) : UIElement(name) {
        SetSize({320, 32});
    }
    
    void AddBuff(uint32_t buff_id, uint32_t icon_id, float duration, int stacks = 0) {
        auto buff_icon = std::make_shared<BuffIcon>("Buff" + std::to_string(buff_id));
        buff_icon->SetBuff(buff_id, icon_id, duration, stacks);
        buff_icon->SetSize({32, 32});
        
        // Position in grid
        int index = buff_icons_.size();
        int row = index / 10;
        int col = index % 10;
        buff_icon->SetPosition({col * 34.0f, row * 34.0f});
        
        buff_icons_[buff_id] = buff_icon;
        AddChild(buff_icon);
    }
    
    void RemoveBuff(uint32_t buff_id) {
        auto it = buff_icons_.find(buff_id);
        if (it != buff_icons_.end()) {
            RemoveChild(it->second.get());
            buff_icons_.erase(it);
            
            // Reposition remaining buffs
            RepositionBuffs();
        }
    }
    
    void UpdateBuff(uint32_t buff_id, float remaining_duration, int stacks) {
        auto it = buff_icons_.find(buff_id);
        if (it != buff_icons_.end()) {
            it->second->UpdateDuration(remaining_duration);
            it->second->UpdateStacks(stacks);
        }
    }
    
private:
    std::unordered_map<uint32_t, std::shared_ptr<BuffIcon>> buff_icons_;
    
    void RepositionBuffs() {
        int index = 0;
        for (auto& [buff_id, icon] : buff_icons_) {
            int row = index / 10;
            int col = index % 10;
            icon->SetPosition({col * 34.0f, row * 34.0f});
            index++;
        }
    }
};

// [SEQUENCE: 2408] Individual buff icon
class BuffIcon : public UIElement {
public:
    BuffIcon(const std::string& name) : UIElement(name) {
        // Icon background
        background_ = std::make_shared<UIPanel>("Background");
        background_->SetBackgroundColor({0.1f, 0.1f, 0.1f, 0.5f});
        background_->SetSize({32, 32});
        AddChild(background_);
        
        // Icon image
        icon_ = std::make_shared<UIImage>("Icon");
        icon_->SetSize({30, 30});
        icon_->SetPosition({1, 1});
        background_->AddChild(icon_);
        
        // Duration text
        duration_text_ = std::make_shared<UILabel>("Duration");
        duration_text_->SetTextAlign(UILabel::TextAlign::CENTER);
        duration_text_->SetPosition({0, 20});
        duration_text_->SetSize({32, 12});
        duration_text_->SetFontSize(10.0f);
        duration_text_->SetTextColor({1.0f, 1.0f, 0.0f, 1.0f});
        AddChild(duration_text_);
        
        // Stack count
        stack_text_ = std::make_shared<UILabel>("Stacks");
        stack_text_->SetTextAlign(UILabel::TextAlign::RIGHT);
        stack_text_->SetPosition({16, 16});
        stack_text_->SetSize({14, 14});
        stack_text_->SetFontSize(12.0f);
        stack_text_->SetTextColor(Color::White());
        AddChild(stack_text_);
        
        SetSize({32, 32});
    }
    
    void SetBuff(uint32_t buff_id, uint32_t icon_id, float duration, int stacks) {
        buff_id_ = buff_id;
        icon_->SetTexture(icon_id);
        total_duration_ = duration;
        remaining_duration_ = duration;
        
        if (stacks > 1) {
            stack_text_->SetText(std::to_string(stacks));
        }
    }
    
    void UpdateDuration(float remaining) {
        remaining_duration_ = remaining;
    }
    
    void UpdateStacks(int stacks) {
        if (stacks > 1) {
            stack_text_->SetText(std::to_string(stacks));
        } else {
            stack_text_->SetText("");
        }
    }
    
protected:
    void OnUpdate(float delta_time) override {
        if (remaining_duration_ > 0) {
            remaining_duration_ -= delta_time;
            
            // Update duration text
            if (remaining_duration_ > 60) {
                int minutes = static_cast<int>(remaining_duration_ / 60);
                duration_text_->SetText(std::to_string(minutes) + "m");
            } else if (remaining_duration_ > 10) {
                duration_text_->SetText(std::to_string(static_cast<int>(remaining_duration_)));
            } else if (remaining_duration_ > 0) {
                duration_text_->SetText(std::to_string(remaining_duration_).substr(0, 3));
                
                // Flash when about to expire
                if (remaining_duration_ < 3) {
                    float alpha = 0.5f + 0.5f * sin(remaining_duration_ * 10);
                    icon_->SetAlpha(alpha);
                }
            }
        }
    }
    
    void OnMouseEnter() override {
        // Show buff tooltip
        ShowBuffTooltip();
    }
    
    void OnMouseLeave() override {
        UIManager::Instance().HideTooltip();
    }
    
private:
    uint32_t buff_id_ = 0;
    float total_duration_ = 0;
    float remaining_duration_ = 0;
    
    std::shared_ptr<UIPanel> background_;
    std::shared_ptr<UIImage> icon_;
    std::shared_ptr<UILabel> duration_text_;
    std::shared_ptr<UILabel> stack_text_;
    
    void ShowBuffTooltip() {
        // Would fetch buff data and show detailed tooltip
        std::string tooltip = "Buff " + std::to_string(buff_id_) + "\n";
        tooltip += "Duration: " + std::to_string(static_cast<int>(remaining_duration_)) + "s";
        
        UIManager::Instance().ShowTooltip(tooltip, 
            GetWorldPosition().x, GetWorldPosition().y - 50);
    }
};

// [SEQUENCE: 2409] Floating combat text
class FloatingText : public UILabel {
public:
    FloatingText(const std::string& text, const Color& color) 
        : UILabel("FloatingText") {
        
        SetText(text);
        SetTextColor(color);
        SetFontSize(16.0f);
        SetTextAlign(TextAlign::CENTER);
        
        start_time_ = std::chrono::steady_clock::now();
    }
    
    bool IsExpired() const {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - start_time_).count() / 1000.0f;
        return elapsed > lifetime_;
    }
    
protected:
    void OnUpdate(float delta_time) override {
        auto now = std::chrono::steady_clock::now();
        float elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - start_time_).count() / 1000.0f;
        
        // Float upward
        float y = GetPosition().y - float_speed_ * delta_time;
        SetPosition({GetPosition().x, y});
        
        // Fade out
        float alpha = 1.0f - (elapsed / lifetime_);
        SetAlpha(alpha);
    }
    
private:
    std::chrono::steady_clock::time_point start_time_;
    float lifetime_ = 2.0f;
    float float_speed_ = 50.0f;
};

// [SEQUENCE: 2410] HUD manager
class HUDManager {
public:
    static HUDManager& Instance() {
        static HUDManager instance;
        return instance;
    }
    
    void Initialize() {
        auto root = UIManager::Instance().GetRoot();
        if (!root) return;
        
        // Create health bar (top left)
        player_health_ = std::make_shared<HealthBar>("PlayerHealth");
        player_health_->SetPosition({20, 20});
        player_health_->SetAnchor(AnchorType::TOP_LEFT);
        root->AddChild(player_health_);
        
        // Create resource bar
        player_resource_ = std::make_shared<ResourceBar>("PlayerResource");
        player_resource_->SetPosition({20, 55});
        player_resource_->SetAnchor(AnchorType::TOP_LEFT);
        root->AddChild(player_resource_);
        
        // Create target frame (top center-left)
        target_frame_ = std::make_shared<TargetFrame>("TargetFrame");
        target_frame_->SetPosition({300, 20});
        target_frame_->SetAnchor(AnchorType::TOP_LEFT);
        root->AddChild(target_frame_);
        
        // Create XP bar (bottom)
        xp_bar_ = std::make_shared<ExperienceBar>("XPBar");
        xp_bar_->SetPosition({0, -10});
        xp_bar_->SetAnchor(AnchorType::BOTTOM_CENTER);
        root->AddChild(xp_bar_);
        
        // Create action bars (bottom center)
        main_action_bar_ = std::make_shared<ActionBar>("MainActionBar");
        main_action_bar_->SetPosition({0, -80});
        main_action_bar_->SetAnchor(AnchorType::BOTTOM_CENTER);
        root->AddChild(main_action_bar_);
        
        // Create cast bar (center)
        player_cast_bar_ = std::make_shared<CastBar>("PlayerCastBar");
        player_cast_bar_->SetPosition({0, 100});
        player_cast_bar_->SetAnchor(AnchorType::CENTER);
        player_cast_bar_->SetVisibility(Visibility::HIDDEN);
        root->AddChild(player_cast_bar_);
    }
    
    // Player status updates
    void UpdatePlayerHealth(float current, float max) {
        if (player_health_) {
            player_health_->SetHealth(current, max);
        }
    }
    
    void UpdatePlayerResource(float current, float max) {
        if (player_resource_) {
            player_resource_->SetResource(current, max);
        }
    }
    
    void UpdatePlayerExperience(uint64_t current, uint64_t needed, uint32_t level) {
        if (xp_bar_) {
            xp_bar_->SetExperience(current, needed, level);
        }
    }
    
    // Target updates
    void SetTarget(uint64_t target_id, const std::string& name, uint32_t level,
                  const std::string& class_name, uint32_t portrait_id) {
        if (target_frame_) {
            target_frame_->SetTarget(target_id, name, level, class_name, portrait_id);
        }
    }
    
    void UpdateTargetHealth(float current, float max) {
        if (target_frame_) {
            target_frame_->UpdateHealth(current, max);
        }
    }
    
    // Action bar updates
    void SetActionBarAbility(int slot, uint32_t ability_id, uint32_t icon_id) {
        if (main_action_bar_) {
            main_action_bar_->SetAbility(slot, ability_id, icon_id);
        }
    }
    
    void UpdateAbilityCooldown(int slot, float remaining, float total) {
        if (main_action_bar_) {
            main_action_bar_->SetCooldown(slot, remaining, total);
        }
    }
    
    // Casting
    void ShowPlayerCasting(const std::string& spell_name, float cast_time) {
        if (player_cast_bar_) {
            player_cast_bar_->StartCast(spell_name, cast_time, true);
            player_cast_bar_->SetVisibility(Visibility::VISIBLE);
        }
    }
    
    void StopPlayerCasting() {
        if (player_cast_bar_) {
            player_cast_bar_->StopCast();
            player_cast_bar_->SetVisibility(Visibility::HIDDEN);
        }
    }
    
    // Combat text
    void ShowDamageText(float damage, bool is_critical = false) {
        if (player_health_) {
            Color color = is_critical ? Color::Yellow() : Color::Red();
            player_health_->ShowCombatText("-" + std::to_string(static_cast<int>(damage)), color);
        }
    }
    
    void ShowHealingText(float healing, bool is_critical = false) {
        if (player_health_) {
            Color color = is_critical ? Color::Yellow() : Color::Green();
            player_health_->ShowCombatText("+" + std::to_string(static_cast<int>(healing)), color);
        }
    }
    
private:
    HUDManager() = default;
    
    // Core HUD elements
    std::shared_ptr<HealthBar> player_health_;
    std::shared_ptr<ResourceBar> player_resource_;
    std::shared_ptr<ExperienceBar> xp_bar_;
    std::shared_ptr<TargetFrame> target_frame_;
    std::shared_ptr<ActionBar> main_action_bar_;
    std::shared_ptr<CastBar> player_cast_bar_;
};

} // namespace mmorpg::ui