#pragma once

#include "ui_framework.h"
#include "../inventory/inventory_system.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <spdlog/spdlog.h>

namespace mmorpg::ui {

// [SEQUENCE: MVP12-18] Inventory UI system for item management
// 인벤토리 UI - 아이템 관리를 위한 사용자 인터페이스

// [SEQUENCE: MVP12-19] Item slot for inventory grid
class ItemSlot : public UIButton {
public:
    ItemSlot(const std::string& name) : UIButton(name) {
        // [SEQUENCE: MVP12-20] Create item icon
        item_icon_ = std::make_shared<UIImage>("ItemIcon");
        item_icon_->SetSize({44, 44});
        item_icon_->SetPosition({3, 3});
        item_icon_->SetVisibility(Visibility::HIDDEN);
        AddChild(item_icon_);
        
        // [SEQUENCE: MVP12-21] Create quantity text
        quantity_text_ = std::make_shared<UILabel>("Quantity");
        quantity_text_->SetTextAlign(UILabel::TextAlign::RIGHT);
        quantity_text_->SetPosition({26, 30});
        quantity_text_->SetSize({18, 16});
        quantity_text_->SetFontSize(11.0f);
        quantity_text_->SetTextColor(Color::White());
        AddChild(quantity_text_);
        
        // [SEQUENCE: MVP12-22] Create quality border
        quality_border_ = std::make_shared<UIPanel>("QualityBorder");
        quality_border_->SetSize({48, 48});
        quality_border_->SetPosition({1, 1});
        quality_border_->SetBackgroundColor({0, 0, 0, 0});
        quality_border_->SetBorderWidth(2.0f);
        AddChild(quality_border_);
        
        // [SEQUENCE: MVP12-23] Create cooldown overlay
        cooldown_overlay_ = std::make_shared<UIPanel>("CooldownOverlay");
        cooldown_overlay_->SetSize({44, 44});
        cooldown_overlay_->SetPosition({3, 3});
        cooldown_overlay_->SetBackgroundColor({0, 0, 0, 0.6f});
        cooldown_overlay_->SetVisibility(Visibility::HIDDEN);
        AddChild(cooldown_overlay_);
        
        // Set default slot appearance
        SetButtonColors(
            {0.2f, 0.2f, 0.2f, 0.6f},  // Normal
            {0.3f, 0.3f, 0.3f, 0.8f},  // Hover
            {0.4f, 0.4f, 0.2f, 0.8f},  // Pressed
            {0.1f, 0.1f, 0.1f, 0.4f}   // Disabled
        );
        
        SetSize({50, 50});
    }
    
    // [SEQUENCE: MVP12-24] Set item in slot
    void SetItem(const inventory::Item* item) {
        if (item) {
            item_id_ = item->item_id;
            item_icon_->SetTexture(item->template_data->icon_id);
            item_icon_->SetVisibility(Visibility::VISIBLE);
            
            // Set quantity for stackable items
            if (item->quantity > 1) {
                quantity_text_->SetText(std::to_string(item->quantity));
            } else {
                quantity_text_->SetText("");
            }
            
            // Set quality border color
            SetQualityBorder(item->template_data->quality);
            
            // Check if item is on cooldown
            if (item->template_data->use_cooldown > 0) {
                // Would check cooldown status
            }
        } else {
            ClearSlot();
        }
    }
    
    void ClearSlot() {
        item_id_ = 0;
        item_icon_->SetVisibility(Visibility::HIDDEN);
        quantity_text_->SetText("");
        quality_border_->SetBorderColor({0, 0, 0, 0});
        cooldown_overlay_->SetVisibility(Visibility::HIDDEN);
    }
    
    // [SEQUENCE: MVP12-25] Drag and drop support
    void StartDrag() {
        if (item_id_ != 0) {
            is_dragging_ = true;
            // Create drag visual
            drag_visual_ = std::make_shared<UIImage>("DragVisual");
            drag_visual_->SetTexture(item_icon_->GetTexture());
            drag_visual_->SetSize({44, 44});
            drag_visual_->SetAlpha(0.7f);
            
            // Start drag operation
            if (on_drag_start_) {
                on_drag_start_(slot_index_, item_id_);
            }
        }
    }
    
    void EndDrag(ItemSlot* target_slot) {
        if (is_dragging_) {
            is_dragging_ = false;
            
            if (target_slot && on_drag_end_) {
                on_drag_end_(slot_index_, target_slot->GetSlotIndex());
            }
            
            drag_visual_ = nullptr;
        }
    }
    
    // [SEQUENCE: MVP12-26] Slot callbacks
    void SetSlotIndex(int index) { slot_index_ = index; }
    int GetSlotIndex() const { return slot_index_; }
    uint32_t GetItemId() const { return item_id_; }
    
    void SetOnDragStart(std::function<void(int, uint32_t)> callback) {
        on_drag_start_ = callback;
    }
    
    void SetOnDragEnd(std::function<void(int, int)> callback) {
        on_drag_end_ = callback;
    }
    
    void SetOnRightClick(std::function<void(int)> callback) {
        on_right_click_ = callback;
    }
    
protected:
    void OnMouseEnter() override {
        UIButton::OnMouseEnter();
        
        if (item_id_ != 0) {
            ShowItemTooltip();
        }
    }
    
    void OnMouseLeave() override {
        UIButton::OnMouseLeave();
        UIManager::Instance().HideTooltip();
    }
    
    bool HandleMouseButton(int button, bool pressed, float x, float y) override {
        if (button == 0) {  // Left button
            if (pressed && item_id_ != 0) {
                StartDrag();
            } else if (!pressed && is_dragging_) {
                // Find target slot
                // EndDrag(target_slot);
            }
        } else if (button == 1 && pressed) {  // Right button
            if (on_right_click_ && item_id_ != 0) {
                on_right_click_(slot_index_);
            }
        }
        
        return UIButton::HandleMouseButton(button, pressed, x, y);
    }
    
private:
    int slot_index_ = 0;
    uint32_t item_id_ = 0;
    
    std::shared_ptr<UIImage> item_icon_;
    std::shared_ptr<UILabel> quantity_text_;
    std::shared_ptr<UIPanel> quality_border_;
    std::shared_ptr<UIPanel> cooldown_overlay_;
    
    bool is_dragging_ = false;
    std::shared_ptr<UIImage> drag_visual_;
    
    std::function<void(int, uint32_t)> on_drag_start_;
    std::function<void(int, int)> on_drag_end_;
    std::function<void(int)> on_right_click_;
    
    // [SEQUENCE: MVP12-27] Set quality border color
    void SetQualityBorder(inventory::ItemQuality quality) {
        Color border_color;
        
        switch (quality) {
            case inventory::ItemQuality::POOR:
                border_color = {0.5f, 0.5f, 0.5f, 1.0f};  // Gray
                break;
            case inventory::ItemQuality::COMMON:
                border_color = {1.0f, 1.0f, 1.0f, 1.0f};  // White
                break;
            case inventory::ItemQuality::UNCOMMON:
                border_color = {0.2f, 1.0f, 0.2f, 1.0f};  // Green
                break;
            case inventory::ItemQuality::RARE:
                border_color = {0.2f, 0.4f, 1.0f, 1.0f};  // Blue
                break;
            case inventory::ItemQuality::EPIC:
                border_color = {0.8f, 0.2f, 1.0f, 1.0f};  // Purple
                break;
            case inventory::ItemQuality::LEGENDARY:
                border_color = {1.0f, 0.6f, 0.2f, 1.0f};  // Orange
                break;
            case inventory::ItemQuality::ARTIFACT:
                border_color = {1.0f, 0.8f, 0.4f, 1.0f};  // Gold
                break;
        }
        
        quality_border_->SetBorderColor(border_color);
    }
    
    // [SEQUENCE: MVP12-28] Show item tooltip
    void ShowItemTooltip() {
        // Would fetch item data and create detailed tooltip
        std::string tooltip = "Item: " + std::to_string(item_id_) + "\n";
        tooltip += "Click to use\nRight-click for options";
        
        UIManager::Instance().ShowTooltip(tooltip,
            GetWorldPosition().x, GetWorldPosition().y - 100);
    }
    
    uint32_t GetTexture() const {
        return item_icon_ ? item_icon_->GetTexture() : 0;
    }
};

// [SEQUENCE: MVP12-29] Inventory window UI
class InventoryWindow : public UIWindow {
public:
    InventoryWindow(const std::string& name) : UIWindow(name) {
        SetTitle("Inventory");
        SetSize({350, 450});
        
        // [SEQUENCE: MVP12-30] Create bag tabs
        CreateBagTabs();
        
        // [SEQUENCE: MVP12-31] Create inventory grid
        inventory_grid_ = std::make_shared<UIPanel>("InventoryGrid");
        inventory_grid_->SetPosition({10, 80});
        inventory_grid_->SetSize({330, 280});
        inventory_grid_->SetBackgroundColor({0.1f, 0.1f, 0.1f, 0.3f});
        AddChild(inventory_grid_);
        
        // [SEQUENCE: MVP12-32] Create item slots
        CreateItemSlots();
        
        // [SEQUENCE: MVP12-33] Create currency display
        CreateCurrencyDisplay();
        
        // [SEQUENCE: MVP12-34] Create sort button
        sort_button_ = std::make_shared<UIButton>("SortButton");
        sort_button_->SetText("Sort");
        sort_button_->SetSize({60, 25});
        sort_button_->SetPosition({280, 50});
        sort_button_->SetOnClick([this]() { SortInventory(); });
        AddChild(sort_button_);
    }
    
    // [SEQUENCE: MVP12-35] Update inventory display
    void UpdateInventory(const inventory::Inventory& inventory) {
        // Clear all slots first
        for (auto& slot : item_slots_) {
            slot->ClearSlot();
        }
        
        // Update with current items
        for (const auto& [slot_index, item] : inventory.items) {
            if (slot_index < item_slots_.size()) {
                item_slots_[slot_index]->SetItem(&item);
            }
        }
        
        // Update currency
        UpdateCurrency(inventory.currency);
    }
    
    // [SEQUENCE: MVP12-36] Set inventory callbacks
    void SetOnItemMove(std::function<void(int, int)> callback) {
        on_item_move_ = callback;
        
        // Set drag callbacks for all slots
        for (auto& slot : item_slots_) {
            slot->SetOnDragEnd(callback);
        }
    }
    
    void SetOnItemUse(std::function<void(int)> callback) {
        on_item_use_ = callback;
        
        // Set right-click callbacks
        for (auto& slot : item_slots_) {
            slot->SetOnRightClick(callback);
        }
    }
    
private:
    static constexpr int SLOTS_PER_ROW = 6;
    static constexpr int TOTAL_SLOTS = 30;  // 5 bags * 6 slots
    
    std::vector<std::shared_ptr<ItemSlot>> item_slots_;
    std::shared_ptr<UIPanel> inventory_grid_;
    std::vector<std::shared_ptr<UIButton>> bag_tabs_;
    std::shared_ptr<UIButton> sort_button_;
    
    // Currency display
    std::shared_ptr<UILabel> gold_label_;
    std::shared_ptr<UILabel> silver_label_;
    std::shared_ptr<UILabel> copper_label_;
    
    int current_bag_ = 0;
    
    std::function<void(int, int)> on_item_move_;
    std::function<void(int)> on_item_use_;
    
    // [SEQUENCE: MVP12-37] Create item slot grid
    void CreateItemSlots() {
        float slot_size = 52.0f;  // 50 + 2 spacing
        
        for (int i = 0; i < TOTAL_SLOTS; ++i) {
            auto slot = std::make_shared<ItemSlot>("Slot" + std::to_string(i));
            slot->SetSlotIndex(i);
            
            int row = i / SLOTS_PER_ROW;
            int col = i % SLOTS_PER_ROW;
            
            slot->SetPosition({col * slot_size, row * slot_size});
            
            // Initially hide slots from other bags
            if (i >= SLOTS_PER_ROW * 5) {  // First bag only
                slot->SetVisibility(Visibility::HIDDEN);
            }
            
            item_slots_.push_back(slot);
            inventory_grid_->AddChild(slot);
        }
    }
    
    // [SEQUENCE: MVP12-38] Create bag tab buttons
    void CreateBagTabs() {
        float tab_width = 60.0f;
        
        for (int i = 0; i < 5; ++i) {
            auto tab = std::make_shared<UIButton>("BagTab" + std::to_string(i));
            tab->SetText("Bag " + std::to_string(i + 1));
            tab->SetSize({tab_width, 25});
            tab->SetPosition({10 + i * (tab_width + 5), 50});
            
            int bag_index = i;
            tab->SetOnClick([this, bag_index]() { SwitchToBag(bag_index); });
            
            bag_tabs_.push_back(tab);
            AddChild(tab);
        }
        
        // Highlight first bag
        bag_tabs_[0]->SetButtonColors(
            {0.4f, 0.4f, 0.4f, 1.0f},  // Selected
            {0.5f, 0.5f, 0.5f, 1.0f},
            {0.3f, 0.3f, 0.3f, 1.0f},
            {0.2f, 0.2f, 0.2f, 0.5f}
        );
    }
    
    // [SEQUENCE: MVP12-39] Create currency display
    void CreateCurrencyDisplay() {
        // Gold icon and label
        auto gold_icon = std::make_shared<UIImage>("GoldIcon");
        gold_icon->SetSize({16, 16});
        gold_icon->SetPosition({10, 370});
        gold_icon->SetTint({1.0f, 0.85f, 0.0f, 1.0f});
        AddChild(gold_icon);
        
        gold_label_ = std::make_shared<UILabel>("GoldLabel");
        gold_label_->SetPosition({30, 370});
        gold_label_->SetSize({60, 16});
        gold_label_->SetText("0");
        AddChild(gold_label_);
        
        // Silver icon and label
        auto silver_icon = std::make_shared<UIImage>("SilverIcon");
        silver_icon->SetSize({16, 16});
        silver_icon->SetPosition({100, 370});
        silver_icon->SetTint({0.75f, 0.75f, 0.75f, 1.0f});
        AddChild(silver_icon);
        
        silver_label_ = std::make_shared<UILabel>("SilverLabel");
        silver_label_->SetPosition({120, 370});
        silver_label_->SetSize({60, 16});
        silver_label_->SetText("0");
        AddChild(silver_label_);
        
        // Copper icon and label
        auto copper_icon = std::make_shared<UIImage>("CopperIcon");
        copper_icon->SetSize({16, 16});
        copper_icon->SetPosition({190, 370});
        copper_icon->SetTint({0.72f, 0.45f, 0.2f, 1.0f});
        AddChild(copper_icon);
        
        copper_label_ = std::make_shared<UILabel>("CopperLabel");
        copper_label_->SetPosition({210, 370});
        copper_label_->SetSize({60, 16});
        copper_label_->SetText("0");
        AddChild(copper_label_);
    }
    
    // [SEQUENCE: MVP12-40] Update currency display
    void UpdateCurrency(const inventory::Currency& currency) {
        gold_label_->SetText(std::to_string(currency.gold));
        silver_label_->SetText(std::to_string(currency.silver));
        copper_label_->SetText(std::to_string(currency.copper));
    }
    
    // [SEQUENCE: MVP12-41] Switch active bag view
    void SwitchToBag(int bag_index) {
        current_bag_ = bag_index;
        
        // Update tab highlighting
        for (int i = 0; i < bag_tabs_.size(); ++i) {
            if (i == bag_index) {
                bag_tabs_[i]->SetButtonColors(
                    {0.4f, 0.4f, 0.4f, 1.0f},  // Selected
                    {0.5f, 0.5f, 0.5f, 1.0f},
                    {0.3f, 0.3f, 0.3f, 1.0f},
                    {0.2f, 0.2f, 0.2f, 0.5f}
                );
            } else {
                bag_tabs_[i]->SetButtonColors(
                    {0.2f, 0.2f, 0.2f, 0.8f},  // Normal
                    {0.3f, 0.3f, 0.3f, 0.8f},
                    {0.4f, 0.4f, 0.2f, 0.8f},
                    {0.1f, 0.1f, 0.1f, 0.5f}
                );
            }
        }
        
        // Show/hide appropriate slots
        int start_slot = bag_index * SLOTS_PER_ROW;
        int end_slot = start_slot + SLOTS_PER_ROW;
        
        for (int i = 0; i < item_slots_.size(); ++i) {
            if (i >= start_slot && i < end_slot) {
                item_slots_[i]->SetVisibility(Visibility::VISIBLE);
            } else {
                item_slots_[i]->SetVisibility(Visibility::HIDDEN);
            }
        }
    }
    
    // [SEQUENCE: MVP12-42] Sort inventory items
    void SortInventory() {
        spdlog::info("Sorting inventory");
        // Would trigger inventory sort on server
    }
};

// [SEQUENCE: MVP12-43] Equipment window UI
class EquipmentWindow : public UIWindow {
public:
    EquipmentWindow(const std::string& name) : UIWindow(name) {
        SetTitle("Character");
        SetSize({300, 400});
        
        // [SEQUENCE: MVP12-44] Create character model area
        model_area_ = std::make_shared<UIPanel>("ModelArea");
        model_area_->SetPosition({75, 50});
        model_area_->SetSize({150, 200});
        model_area_->SetBackgroundColor({0.1f, 0.1f, 0.1f, 0.5f});
        AddChild(model_area_);
        
        // [SEQUENCE: MVP12-45] Create equipment slots
        CreateEquipmentSlots();
        
        // [SEQUENCE: MVP12-46] Create stats display
        CreateStatsDisplay();
    }
    
    // [SEQUENCE: MVP12-47] Update equipment display
    void UpdateEquipment(const std::unordered_map<inventory::EquipmentSlot, inventory::Item>& equipment) {
        for (const auto& [slot_type, item] : equipment) {
            auto it = equipment_slots_.find(slot_type);
            if (it != equipment_slots_.end()) {
                it->second->SetItem(&item);
            }
        }
    }
    
    void UpdateStats(const CharacterStats& stats) {
        // Update stat labels
        stat_labels_["Strength"]->SetText("Strength: " + std::to_string(stats.strength));
        stat_labels_["Agility"]->SetText("Agility: " + std::to_string(stats.agility));
        stat_labels_["Intelligence"]->SetText("Intelligence: " + std::to_string(stats.intelligence));
        stat_labels_["Stamina"]->SetText("Stamina: " + std::to_string(stats.stamina));
        
        stat_labels_["Attack Power"]->SetText("Attack Power: " + std::to_string(stats.attack_power));
        stat_labels_["Spell Power"]->SetText("Spell Power: " + std::to_string(stats.spell_power));
        stat_labels_["Armor"]->SetText("Armor: " + std::to_string(stats.armor));
        stat_labels_["Resistance"]->SetText("Resistance: " + std::to_string(stats.resistance));
    }
    
private:
    std::shared_ptr<UIPanel> model_area_;
    std::unordered_map<inventory::EquipmentSlot, std::shared_ptr<ItemSlot>> equipment_slots_;
    std::unordered_map<std::string, std::shared_ptr<UILabel>> stat_labels_;
    
    // [SEQUENCE: MVP12-48] Create equipment slot layout
    void CreateEquipmentSlots() {
        // Head slot
        CreateEquipmentSlot(inventory::EquipmentSlot::HEAD, {125, 50});
        
        // Shoulders
        CreateEquipmentSlot(inventory::EquipmentSlot::SHOULDERS, {60, 80});
        
        // Chest
        CreateEquipmentSlot(inventory::EquipmentSlot::CHEST, {125, 110});
        
        // Hands
        CreateEquipmentSlot(inventory::EquipmentSlot::HANDS, {60, 140});
        
        // Belt
        CreateEquipmentSlot(inventory::EquipmentSlot::BELT, {125, 170});
        
        // Legs
        CreateEquipmentSlot(inventory::EquipmentSlot::LEGS, {125, 200});
        
        // Feet
        CreateEquipmentSlot(inventory::EquipmentSlot::FEET, {125, 230});
        
        // Main hand
        CreateEquipmentSlot(inventory::EquipmentSlot::MAIN_HAND, {60, 200});
        
        // Off hand
        CreateEquipmentSlot(inventory::EquipmentSlot::OFF_HAND, {190, 200});
        
        // Rings
        CreateEquipmentSlot(inventory::EquipmentSlot::RING1, {60, 230});
        CreateEquipmentSlot(inventory::EquipmentSlot::RING2, {190, 230});
        
        // Trinkets
        CreateEquipmentSlot(inventory::EquipmentSlot::TRINKET1, {10, 110});
        CreateEquipmentSlot(inventory::EquipmentSlot::TRINKET2, {10, 170});
    }
    
    void CreateEquipmentSlot(inventory::EquipmentSlot slot_type, const Vector2& position) {
        auto slot = std::make_shared<ItemSlot>("EquipSlot_" + std::to_string(static_cast<int>(slot_type)));
        slot->SetPosition(position);
        slot->SetSize({45, 45});
        
        equipment_slots_[slot_type] = slot;
        AddChild(slot);
    }
    
    // [SEQUENCE: MVP12-49] Create character stats display
    void CreateStatsDisplay() {
        float y_offset = 270;
        float line_height = 18;
        
        // Primary stats
        CreateStatLabel("Strength", {10, y_offset});
        CreateStatLabel("Agility", {10, y_offset + line_height});
        CreateStatLabel("Intelligence", {10, y_offset + line_height * 2});
        CreateStatLabel("Stamina", {10, y_offset + line_height * 3});
        
        // Secondary stats
        CreateStatLabel("Attack Power", {150, y_offset});
        CreateStatLabel("Spell Power", {150, y_offset + line_height});
        CreateStatLabel("Armor", {150, y_offset + line_height * 2});
        CreateStatLabel("Resistance", {150, y_offset + line_height * 3});
    }
    
    void CreateStatLabel(const std::string& stat_name, const Vector2& position) {
        auto label = std::make_shared<UILabel>(stat_name + "Label");
        label->SetPosition(position);
        label->SetSize({130, 16});
        label->SetFontSize(12.0f);
        label->SetText(stat_name + ": 0");
        
        stat_labels_[stat_name] = label;
        AddChild(label);
    }
    
    // Character stats structure
    struct CharacterStats {
        int strength = 0;
        int agility = 0;
        int intelligence = 0;
        int stamina = 0;
        int attack_power = 0;
        int spell_power = 0;
        int armor = 0;
        int resistance = 0;
    };
};

// [SEQUENCE: MVP12-50] Bank window UI
class BankWindow : public UIWindow {
public:
    BankWindow(const std::string& name) : UIWindow(name) {
        SetTitle("Bank");
        SetSize({400, 500});
        
        // [SEQUENCE: MVP12-51] Create bank tabs
        CreateBankTabs();
        
        // [SEQUENCE: MVP12-52] Create bank grid
        bank_grid_ = std::make_shared<UIPanel>("BankGrid");
        bank_grid_->SetPosition({10, 80});
        bank_grid_->SetSize({380, 350});
        bank_grid_->SetBackgroundColor({0.1f, 0.1f, 0.1f, 0.3f});
        AddChild(bank_grid_);
        
        // Create bank slots (more than inventory)
        CreateBankSlots();
        
        // [SEQUENCE: MVP12-53] Create deposit all button
        deposit_button_ = std::make_shared<UIButton>("DepositAll");
        deposit_button_->SetText("Deposit All");
        deposit_button_->SetSize({100, 25});
        deposit_button_->SetPosition({290, 440});
        deposit_button_->SetOnClick([this]() { DepositAll(); });
        AddChild(deposit_button_);
    }
    
    void UpdateBankContents(const std::vector<inventory::Item>& items) {
        // Clear all slots
        for (auto& slot : bank_slots_) {
            slot->ClearSlot();
        }
        
        // Update with bank items
        for (size_t i = 0; i < items.size() && i < bank_slots_.size(); ++i) {
            bank_slots_[i]->SetItem(&items[i]);
        }
    }
    
private:
    static constexpr int BANK_SLOTS_PER_ROW = 7;
    static constexpr int BANK_TABS = 8;
    static constexpr int SLOTS_PER_TAB = 98;  // 14 rows * 7 columns
    
    std::vector<std::shared_ptr<ItemSlot>> bank_slots_;
    std::shared_ptr<UIPanel> bank_grid_;
    std::vector<std::shared_ptr<UIButton>> bank_tabs_;
    std::shared_ptr<UIButton> deposit_button_;
    
    int current_tab_ = 0;
    
    // [SEQUENCE: MVP12-54] Create bank slot grid
    void CreateBankSlots() {
        float slot_size = 52.0f;
        
        for (int tab = 0; tab < BANK_TABS; ++tab) {
            for (int i = 0; i < SLOTS_PER_TAB; ++i) {
                auto slot = std::make_shared<ItemSlot>("BankSlot" + 
                    std::to_string(tab * SLOTS_PER_TAB + i));
                
                int row = i / BANK_SLOTS_PER_ROW;
                int col = i % BANK_SLOTS_PER_ROW;
                
                slot->SetPosition({col * slot_size, row * slot_size});
                slot->SetSlotIndex(tab * SLOTS_PER_TAB + i);
                
                // Initially hide all but first tab
                if (tab != 0) {
                    slot->SetVisibility(Visibility::HIDDEN);
                }
                
                bank_slots_.push_back(slot);
                bank_grid_->AddChild(slot);
            }
        }
    }
    
    void CreateBankTabs() {
        float tab_width = 45.0f;
        
        for (int i = 0; i < BANK_TABS; ++i) {
            auto tab = std::make_shared<UIButton>("BankTab" + std::to_string(i));
            tab->SetText(std::to_string(i + 1));
            tab->SetSize({tab_width, 25});
            tab->SetPosition({10 + i * (tab_width + 2), 50});
            
            int tab_index = i;
            tab->SetOnClick([this, tab_index]() { SwitchToTab(tab_index); });
            
            bank_tabs_.push_back(tab);
            AddChild(tab);
        }
        
        // Highlight first tab
        bank_tabs_[0]->SetButtonColors(
            {0.4f, 0.4f, 0.4f, 1.0f},
            {0.5f, 0.5f, 0.5f, 1.0f},
            {0.3f, 0.3f, 0.3f, 1.0f},
            {0.2f, 0.2f, 0.2f, 0.5f}
        );
    }
    
    void SwitchToTab(int tab_index) {
        current_tab_ = tab_index;
        
        // Update tab highlighting
        for (int i = 0; i < bank_tabs_.size(); ++i) {
            if (i == tab_index) {
                bank_tabs_[i]->SetButtonColors(
                    {0.4f, 0.4f, 0.4f, 1.0f},
                    {0.5f, 0.5f, 0.5f, 1.0f},
                    {0.3f, 0.3f, 0.3f, 1.0f},
                    {0.2f, 0.2f, 0.2f, 0.5f}
                );
            } else {
                bank_tabs_[i]->SetButtonColors(
                    {0.2f, 0.2f, 0.2f, 0.8f},
                    {0.3f, 0.3f, 0.3f, 0.8f},
                    {0.4f, 0.4f, 0.2f, 0.8f},
                    {0.1f, 0.1f, 0.1f, 0.5f}
                );
            }
        }
        
        // Show/hide appropriate slots
        int start_slot = tab_index * SLOTS_PER_TAB;
        int end_slot = start_slot + SLOTS_PER_TAB;
        
        for (int i = 0; i < bank_slots_.size(); ++i) {
            if (i >= start_slot && i < end_slot) {
                bank_slots_[i]->SetVisibility(Visibility::VISIBLE);
            } else {
                bank_slots_[i]->SetVisibility(Visibility::HIDDEN);
            }
        }
    }
    
    void DepositAll() {
        spdlog::info("Depositing all items to bank");
        // Would trigger deposit all operation
    }
};

// [SEQUENCE: MVP12-55] Vendor window UI
class VendorWindow : public UIWindow {
public:
    VendorWindow(const std::string& name) : UIWindow(name) {
        SetTitle("Vendor");
        SetSize({450, 500});
        
        // [SEQUENCE: MVP12-56] Create vendor inventory
        vendor_grid_ = std::make_shared<UIPanel>("VendorGrid");
        vendor_grid_->SetPosition({10, 50});
        vendor_grid_->SetSize({430, 300});
        vendor_grid_->SetBackgroundColor({0.1f, 0.1f, 0.1f, 0.3f});
        AddChild(vendor_grid_);
        
        CreateVendorSlots();
        
        // [SEQUENCE: MVP12-57] Create buyback tab
        buyback_button_ = std::make_shared<UIButton>("BuybackTab");
        buyback_button_->SetText("Buyback");
        buyback_button_->SetSize({80, 25});
        buyback_button_->SetPosition({360, 20});
        buyback_button_->SetOnClick([this]() { ShowBuyback(); });
        AddChild(buyback_button_);
        
        // [SEQUENCE: MVP12-58] Create repair buttons
        repair_button_ = std::make_shared<UIButton>("RepairAll");
        repair_button_->SetText("Repair All");
        repair_button_->SetSize({100, 25});
        repair_button_->SetPosition({10, 460});
        repair_button_->SetOnClick([this]() { RepairAll(); });
        AddChild(repair_button_);
        
        // [SEQUENCE: MVP12-59] Create sell area
        CreateSellArea();
    }
    
    void SetVendorItems(const std::vector<VendorItem>& items) {
        vendor_items_ = items;
        UpdateVendorDisplay();
    }
    
private:
    struct VendorItem {
        inventory::Item item;
        inventory::Currency price;
        int stock = -1;  // -1 = unlimited
    };
    
    std::vector<VendorItem> vendor_items_;
    std::vector<std::shared_ptr<ItemSlot>> vendor_slots_;
    std::shared_ptr<UIPanel> vendor_grid_;
    std::shared_ptr<UIPanel> sell_area_;
    std::shared_ptr<UIButton> buyback_button_;
    std::shared_ptr<UIButton> repair_button_;
    
    bool showing_buyback_ = false;
    
    void CreateVendorSlots() {
        float slot_size = 52.0f;
        int slots_per_row = 8;
        int total_slots = 40;
        
        for (int i = 0; i < total_slots; ++i) {
            auto slot = std::make_shared<ItemSlot>("VendorSlot" + std::to_string(i));
            
            int row = i / slots_per_row;
            int col = i % slots_per_row;
            
            slot->SetPosition({col * slot_size, row * slot_size});
            slot->SetSlotIndex(i);
            
            // Vendor slots have different click behavior
            slot->SetOnRightClick([this, i](int index) {
                PurchaseItem(index);
            });
            
            vendor_slots_.push_back(slot);
            vendor_grid_->AddChild(slot);
        }
    }
    
    void CreateSellArea() {
        // Create drop area for selling items
        sell_area_ = std::make_shared<UIPanel>("SellArea");
        sell_area_->SetPosition({10, 370});
        sell_area_->SetSize({430, 80});
        sell_area_->SetBackgroundColor({0.2f, 0.1f, 0.1f, 0.5f});
        sell_area_->SetBorderColor({0.8f, 0.4f, 0.4f, 1.0f});
        sell_area_->SetBorderWidth(2.0f);
        AddChild(sell_area_);
        
        auto sell_label = std::make_shared<UILabel>("SellLabel");
        sell_label->SetText("Drop items here to sell");
        sell_label->SetTextAlign(UILabel::TextAlign::CENTER);
        sell_label->SetPosition({0, 30});
        sell_label->SetSize({430, 20});
        sell_label->SetTextColor({0.8f, 0.8f, 0.8f, 1.0f});
        sell_area_->AddChild(sell_label);
    }
    
    void UpdateVendorDisplay() {
        // Clear all slots
        for (auto& slot : vendor_slots_) {
            slot->ClearSlot();
        }
        
        // Update with vendor items
        for (size_t i = 0; i < vendor_items_.size() && i < vendor_slots_.size(); ++i) {
            vendor_slots_[i]->SetItem(&vendor_items_[i].item);
            
            // Add price overlay or stock count
            if (vendor_items_[i].stock != -1 && vendor_items_[i].stock == 0) {
                // Mark as out of stock
                vendor_slots_[i]->SetEnabled(false);
            }
        }
    }
    
    void PurchaseItem(int index) {
        if (index < vendor_items_.size()) {
            spdlog::info("Purchasing item at index {}", index);
            // Would send purchase request to server
        }
    }
    
    void ShowBuyback() {
        showing_buyback_ = !showing_buyback_;
        spdlog::info("Toggling buyback view: {}", showing_buyback_);
        // Would switch between vendor items and buyback list
    }
    
    void RepairAll() {
        spdlog::info("Repairing all equipment");
        // Would send repair request to server
    }
};

// [SEQUENCE: MVP12-60] Inventory UI manager
class InventoryUIManager {
public:
    static InventoryUIManager& Instance() {
        static InventoryUIManager instance;
        return instance;
    }
    
    void Initialize() {
        auto root = UIManager::Instance().GetRoot();
        if (!root) return;
        
        // [SEQUENCE: MVP12-61] Create inventory window
        inventory_window_ = std::make_shared<InventoryWindow>("InventoryWindow");
        inventory_window_->SetPosition({100, 100});
        inventory_window_->SetVisibility(Visibility::HIDDEN);
        root->AddChild(inventory_window_);
        
        // [SEQUENCE: MVP12-62] Create equipment window
        equipment_window_ = std::make_shared<EquipmentWindow>("EquipmentWindow");
        equipment_window_->SetPosition({500, 100});
        equipment_window_->SetVisibility(Visibility::HIDDEN);
        root->AddChild(equipment_window_);
        
        // [SEQUENCE: MVP12-63] Create bank window
        bank_window_ = std::make_shared<BankWindow>("BankWindow");
        bank_window_->SetPosition({300, 50});
        bank_window_->SetVisibility(Visibility::HIDDEN);
        root->AddChild(bank_window_);
        
        // [SEQUENCE: MVP12-64] Create vendor window
        vendor_window_ = std::make_shared<VendorWindow>("VendorWindow");
        vendor_window_->SetPosition({250, 75});
        vendor_window_->SetVisibility(Visibility::HIDDEN);
        root->AddChild(vendor_window_);
    }
    
    // [SEQUENCE: MVP12-65] Window visibility control
    void ToggleInventory() {
        auto vis = inventory_window_->IsVisible() ? 
            Visibility::HIDDEN : Visibility::VISIBLE;
        inventory_window_->SetVisibility(vis);
    }
    
    void ToggleEquipment() {
        auto vis = equipment_window_->IsVisible() ? 
            Visibility::HIDDEN : Visibility::VISIBLE;
        equipment_window_->SetVisibility(vis);
    }
    
    void ShowBank() {
        bank_window_->SetVisibility(Visibility::VISIBLE);
    }
    
    void HideBank() {
        bank_window_->SetVisibility(Visibility::HIDDEN);
    }
    
    void ShowVendor() {
        vendor_window_->SetVisibility(Visibility::VISIBLE);
    }
    
    void HideVendor() {
        vendor_window_->SetVisibility(Visibility::HIDDEN);
    }
    
    // [SEQUENCE: MVP12-66] Update methods
    void UpdateInventory(const inventory::Inventory& inventory) {
        if (inventory_window_) {
            inventory_window_->UpdateInventory(inventory);
        }
    }
    
    void UpdateEquipment(const std::unordered_map<inventory::EquipmentSlot, inventory::Item>& equipment) {
        if (equipment_window_) {
            equipment_window_->UpdateEquipment(equipment);
        }
    }
    
    void UpdateBank(const std::vector<inventory::Item>& bank_items) {
        if (bank_window_) {
            bank_window_->UpdateBankContents(bank_items);
        }
    }
    
    // [SEQUENCE: MVP12-67] Set callbacks
    void SetOnItemMove(std::function<void(int, int)> callback) {
        if (inventory_window_) {
            inventory_window_->SetOnItemMove(callback);
        }
    }
    
    void SetOnItemUse(std::function<void(int)> callback) {
        if (inventory_window_) {
            inventory_window_->SetOnItemUse(callback);
        }
    }
    
private:
    InventoryUIManager() = default;
    
    std::shared_ptr<InventoryWindow> inventory_window_;
    std::shared_ptr<EquipmentWindow> equipment_window_;
    std::shared_ptr<BankWindow> bank_window_;
    std::shared_ptr<VendorWindow> vendor_window_;
};

} // namespace mmorpg::ui
