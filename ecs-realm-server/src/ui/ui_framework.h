#pragma once

#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <unordered_map>
#include <chrono>
#include <optional>
#include <spdlog/spdlog.h>

namespace mmorpg::ui {

// [SEQUENCE: 2363] UI framework for client-side interface
// UI „Ìl - t|t¸¸ x0˜t¤| \ Ü¤\

// [SEQUENCE: 2364] Basic UI types
struct Color {
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
    
    static Color White() { return {1.0f, 1.0f, 1.0f, 1.0f}; }
    static Color Black() { return {0.0f, 0.0f, 0.0f, 1.0f}; }
    static Color Red() { return {1.0f, 0.0f, 0.0f, 1.0f}; }
    static Color Green() { return {0.0f, 1.0f, 0.0f, 1.0f}; }
    static Color Blue() { return {0.0f, 0.0f, 1.0f, 1.0f}; }
    static Color Yellow() { return {1.0f, 1.0f, 0.0f, 1.0f}; }
};

struct Vector2 {
    float x = 0, y = 0;
    
    Vector2 operator+(const Vector2& other) const {
        return {x + other.x, y + other.y};
    }
    
    Vector2 operator-(const Vector2& other) const {
        return {x - other.x, y - other.y};
    }
    
    Vector2 operator*(float scalar) const {
        return {x * scalar, y * scalar};
    }
};

struct Rect {
    float x = 0, y = 0, width = 0, height = 0;
    
    bool Contains(const Vector2& point) const {
        return point.x >= x && point.x <= x + width &&
               point.y >= y && point.y <= y + height;
    }
    
    Vector2 GetCenter() const {
        return {x + width * 0.5f, y + height * 0.5f};
    }
};

// [SEQUENCE: 2365] UI element anchor types
enum class AnchorType {
    TOP_LEFT,           // ŒÁè
    TOP_CENTER,         // Áè Y
    TOP_RIGHT,          // °Áè
    MIDDLE_LEFT,        // Œ! Y
    CENTER,             // Y
    MIDDLE_RIGHT,       // °! Y
    BOTTOM_LEFT,        // ŒXè
    BOTTOM_CENTER,      // Xè Y
    BOTTOM_RIGHT        // °Xè
};

// [SEQUENCE: 2366] UI visibility states
enum class Visibility {
    VISIBLE,            // ô„
    HIDDEN,             // (@ (TÁ Hh)
    COLLAPSED           // •Œ (õÄ (À Hh)
};

// [SEQUENCE: 2367] UI element states
enum class UIState {
    NORMAL,             // |
    HOVERED,            // È°¤ $„
    PRESSED,            // ¼
    DISABLED,           // D\1T
    FOCUSED             // ìä¤
};

// [SEQUENCE: 2368] Base UI element class
class UIElement {
public:
    UIElement(const std::string& name) : name_(name) {}
    virtual ~UIElement() = default;
    
    // [SEQUENCE: 2369] Core update and render
    virtual void Update(float delta_time) {
        if (visibility_ == Visibility::VISIBLE) {
            OnUpdate(delta_time);
            
            // Update children
            for (auto& child : children_) {
                child->Update(delta_time);
            }
        }
    }
    
    virtual void Render() {
        if (visibility_ == Visibility::VISIBLE) {
            OnRender();
            
            // Render children
            for (auto& child : children_) {
                child->Render();
            }
        }
    }
    
    // [SEQUENCE: 2370] Event handling
    virtual bool HandleMouseMove(float x, float y) {
        if (!IsInteractable()) return false;
        
        Vector2 local = ScreenToLocal({x, y});
        bool was_hovered = is_hovered_;
        is_hovered_ = GetLocalBounds().Contains(local);
        
        if (is_hovered_ != was_hovered) {
            if (is_hovered_) {
                OnMouseEnter();
                state_ = UIState::HOVERED;
            } else {
                OnMouseLeave();
                state_ = UIState::NORMAL;
            }
        }
        
        // Propagate to children
        for (auto& child : children_) {
            if (child->HandleMouseMove(x, y)) {
                return true;
            }
        }
        
        return is_hovered_;
    }
    
    virtual bool HandleMouseButton(int button, bool pressed, float x, float y) {
        if (!IsInteractable()) return false;
        
        Vector2 local = ScreenToLocal({x, y});
        if (!GetLocalBounds().Contains(local)) return false;
        
        if (button == 0) {  // Left button
            if (pressed) {
                state_ = UIState::PRESSED;
                OnMouseDown();
            } else {
                state_ = UIState::HOVERED;
                OnMouseUp();
                OnClick();
            }
        }
        
        return true;
    }
    
    virtual bool HandleKeyboard(int key, bool pressed) {
        if (!has_focus_) return false;
        
        if (pressed) {
            OnKeyDown(key);
        } else {
            OnKeyUp(key);
        }
        
        return true;
    }
    
    // [SEQUENCE: 2371] Hierarchy management
    void AddChild(std::shared_ptr<UIElement> child) {
        child->parent_ = this;
        children_.push_back(child);
        OnChildAdded(child.get());
    }
    
    void RemoveChild(UIElement* child) {
        children_.erase(
            std::remove_if(children_.begin(), children_.end(),
                [child](const std::shared_ptr<UIElement>& elem) {
                    return elem.get() == child;
                }),
            children_.end()
        );
        OnChildRemoved(child);
    }
    
    // [SEQUENCE: 2372] Transform and layout
    void SetPosition(const Vector2& position) {
        position_ = position;
        UpdateTransform();
    }
    
    void SetSize(const Vector2& size) {
        size_ = size;
        UpdateTransform();
    }
    
    void SetAnchor(AnchorType anchor) {
        anchor_ = anchor;
        UpdateTransform();
    }
    
    void SetPivot(const Vector2& pivot) {
        pivot_ = pivot;
        UpdateTransform();
    }
    
    // [SEQUENCE: 2373] Properties
    void SetVisibility(Visibility visibility) {
        visibility_ = visibility;
    }
    
    void SetEnabled(bool enabled) {
        enabled_ = enabled;
        state_ = enabled ? UIState::NORMAL : UIState::DISABLED;
    }
    
    void SetAlpha(float alpha) {
        alpha_ = std::clamp(alpha, 0.0f, 1.0f);
    }
    
    // Getters
    const std::string& GetName() const { return name_; }
    UIElement* GetParent() const { return parent_; }
    const std::vector<std::shared_ptr<UIElement>>& GetChildren() const { return children_; }
    
    Vector2 GetPosition() const { return position_; }
    Vector2 GetSize() const { return size_; }
    Rect GetBounds() const { return {position_.x, position_.y, size_.x, size_.y}; }
    
    bool IsVisible() const { return visibility_ == Visibility::VISIBLE; }
    bool IsEnabled() const { return enabled_; }
    bool IsHovered() const { return is_hovered_; }
    bool HasFocus() const { return has_focus_; }
    
protected:
    // [SEQUENCE: 2374] Virtual methods for derived classes
    virtual void OnUpdate(float delta_time) {}
    virtual void OnRender() {}
    
    virtual void OnMouseEnter() {}
    virtual void OnMouseLeave() {}
    virtual void OnMouseDown() {}
    virtual void OnMouseUp() {}
    virtual void OnClick() {}
    
    virtual void OnKeyDown(int key) {}
    virtual void OnKeyUp(int key) {}
    
    virtual void OnChildAdded(UIElement* child) {}
    virtual void OnChildRemoved(UIElement* child) {}
    
    // [SEQUENCE: 2375] Transform helpers
    Vector2 ScreenToLocal(const Vector2& screen_pos) const {
        // Transform screen coordinates to local space
        return screen_pos - GetWorldPosition();
    }
    
    Vector2 LocalToScreen(const Vector2& local_pos) const {
        // Transform local coordinates to screen space
        return local_pos + GetWorldPosition();
    }
    
    Vector2 GetWorldPosition() const {
        Vector2 world_pos = position_;
        if (parent_) {
            world_pos = world_pos + parent_->GetWorldPosition();
        }
        return world_pos;
    }
    
    Rect GetLocalBounds() const {
        return {0, 0, size_.x, size_.y};
    }
    
    void UpdateTransform() {
        // Recalculate transform based on anchor and pivot
        // This would update the actual rendering position
    }
    
    bool IsInteractable() const {
        return visibility_ == Visibility::VISIBLE && enabled_;
    }
    
protected:
    std::string name_;
    UIElement* parent_ = nullptr;
    std::vector<std::shared_ptr<UIElement>> children_;
    
    // Transform
    Vector2 position_;
    Vector2 size_{100, 100};
    Vector2 pivot_{0.5f, 0.5f};
    AnchorType anchor_ = AnchorType::TOP_LEFT;
    float rotation_ = 0.0f;
    Vector2 scale_{1.0f, 1.0f};
    
    // State
    Visibility visibility_ = Visibility::VISIBLE;
    bool enabled_ = true;
    float alpha_ = 1.0f;
    UIState state_ = UIState::NORMAL;
    
    // Interaction
    bool is_hovered_ = false;
    bool has_focus_ = false;
};

// [SEQUENCE: 2376] UI Panel - Container element
class UIPanel : public UIElement {
public:
    UIPanel(const std::string& name) : UIElement(name) {}
    
    void SetBackgroundColor(const Color& color) {
        background_color_ = color;
    }
    
    void SetBorderColor(const Color& color) {
        border_color_ = color;
    }
    
    void SetBorderWidth(float width) {
        border_width_ = width;
    }
    
    void SetPadding(float left, float top, float right, float bottom) {
        padding_ = {left, top, right, bottom};
    }
    
protected:
    void OnRender() override {
        // Render background
        if (background_color_.a > 0) {
            RenderFilledRect(GetBounds(), background_color_);
        }
        
        // Render border
        if (border_width_ > 0 && border_color_.a > 0) {
            RenderRectOutline(GetBounds(), border_color_, border_width_);
        }
    }
    
private:
    Color background_color_{0, 0, 0, 0.8f};
    Color border_color_{1, 1, 1, 0.5f};
    float border_width_ = 0;
    struct { float left, top, right, bottom; } padding_{5, 5, 5, 5};
    
    // Placeholder render functions
    void RenderFilledRect(const Rect& rect, const Color& color) {}
    void RenderRectOutline(const Rect& rect, const Color& color, float width) {}
};

// [SEQUENCE: 2377] UI Button
class UIButton : public UIElement {
public:
    UIButton(const std::string& name) : UIElement(name) {}
    
    void SetText(const std::string& text) {
        text_ = text;
    }
    
    void SetTextColor(const Color& color) {
        text_color_ = color;
    }
    
    void SetButtonColors(const Color& normal, const Color& hover, 
                        const Color& pressed, const Color& disabled) {
        color_normal_ = normal;
        color_hover_ = hover;
        color_pressed_ = pressed;
        color_disabled_ = disabled;
    }
    
    // [SEQUENCE: 2378] Click callback
    void SetOnClick(std::function<void()> callback) {
        on_click_ = callback;
    }
    
protected:
    void OnRender() override {
        Color current_color;
        
        switch (state_) {
            case UIState::NORMAL:
                current_color = color_normal_;
                break;
            case UIState::HOVERED:
                current_color = color_hover_;
                break;
            case UIState::PRESSED:
                current_color = color_pressed_;
                break;
            case UIState::DISABLED:
                current_color = color_disabled_;
                break;
            default:
                current_color = color_normal_;
                break;
        }
        
        // Render button background
        RenderFilledRect(GetBounds(), current_color);
        
        // Render text
        if (!text_.empty()) {
            RenderTextCentered(text_, GetBounds().GetCenter(), text_color_);
        }
    }
    
    void OnClick() override {
        if (on_click_) {
            on_click_();
        }
    }
    
private:
    std::string text_;
    Color text_color_ = Color::White();
    
    Color color_normal_{0.2f, 0.2f, 0.2f, 1.0f};
    Color color_hover_{0.3f, 0.3f, 0.3f, 1.0f};
    Color color_pressed_{0.1f, 0.1f, 0.1f, 1.0f};
    Color color_disabled_{0.1f, 0.1f, 0.1f, 0.5f};
    
    std::function<void()> on_click_;
    
    // Placeholder render functions
    void RenderFilledRect(const Rect& rect, const Color& color) {}
    void RenderTextCentered(const std::string& text, const Vector2& pos, const Color& color) {}
};

// [SEQUENCE: 2379] UI Label - Text display
class UILabel : public UIElement {
public:
    UILabel(const std::string& name) : UIElement(name) {}
    
    void SetText(const std::string& text) {
        text_ = text;
    }
    
    void SetTextColor(const Color& color) {
        text_color_ = color;
    }
    
    void SetFontSize(float size) {
        font_size_ = size;
    }
    
    enum class TextAlign {
        LEFT,
        CENTER,
        RIGHT
    };
    
    void SetTextAlign(TextAlign align) {
        text_align_ = align;
    }
    
protected:
    void OnRender() override {
        if (!text_.empty()) {
            Vector2 text_pos = GetWorldPosition();
            
            switch (text_align_) {
                case TextAlign::CENTER:
                    text_pos.x += size_.x * 0.5f;
                    break;
                case TextAlign::RIGHT:
                    text_pos.x += size_.x;
                    break;
                default:
                    break;
            }
            
            RenderText(text_, text_pos, text_color_, font_size_);
        }
    }
    
private:
    std::string text_;
    Color text_color_ = Color::White();
    float font_size_ = 14.0f;
    TextAlign text_align_ = TextAlign::LEFT;
    
    void RenderText(const std::string& text, const Vector2& pos, 
                   const Color& color, float size) {}
};

// [SEQUENCE: 2380] UI Image
class UIImage : public UIElement {
public:
    UIImage(const std::string& name) : UIElement(name) {}
    
    void SetTexture(uint32_t texture_id) {
        texture_id_ = texture_id;
    }
    
    void SetTint(const Color& tint) {
        tint_ = tint;
    }
    
    enum class ScaleMode {
        STRETCH,            // ˜¬0
        FIT,                // D(  ÀXp Þ”0
        FILL,               // D(  ÀXp D°0
        TILE                // À|Á
    };
    
    void SetScaleMode(ScaleMode mode) {
        scale_mode_ = mode;
    }
    
protected:
    void OnRender() override {
        if (texture_id_ != 0) {
            RenderTexture(texture_id_, GetBounds(), tint_, scale_mode_);
        }
    }
    
private:
    uint32_t texture_id_ = 0;
    Color tint_ = Color::White();
    ScaleMode scale_mode_ = ScaleMode::STRETCH;
    
    void RenderTexture(uint32_t texture, const Rect& rect, 
                      const Color& tint, ScaleMode mode) {}
};

// [SEQUENCE: 2381] UI Progress Bar
class UIProgressBar : public UIElement {
public:
    UIProgressBar(const std::string& name) : UIElement(name) {}
    
    void SetValue(float value) {
        value_ = std::clamp(value, 0.0f, 1.0f);
    }
    
    void SetColors(const Color& background, const Color& fill) {
        background_color_ = background;
        fill_color_ = fill;
    }
    
    void SetShowText(bool show) {
        show_text_ = show;
    }
    
    enum class FillDirection {
        LEFT_TO_RIGHT,
        RIGHT_TO_LEFT,
        BOTTOM_TO_TOP,
        TOP_TO_BOTTOM
    };
    
    void SetFillDirection(FillDirection direction) {
        fill_direction_ = direction;
    }
    
protected:
    void OnRender() override {
        // Render background
        RenderFilledRect(GetBounds(), background_color_);
        
        // Calculate fill rect
        Rect fill_rect = GetBounds();
        
        switch (fill_direction_) {
            case FillDirection::LEFT_TO_RIGHT:
                fill_rect.width *= value_;
                break;
            case FillDirection::RIGHT_TO_LEFT:
                fill_rect.x += fill_rect.width * (1.0f - value_);
                fill_rect.width *= value_;
                break;
            case FillDirection::BOTTOM_TO_TOP:
                fill_rect.y += fill_rect.height * (1.0f - value_);
                fill_rect.height *= value_;
                break;
            case FillDirection::TOP_TO_BOTTOM:
                fill_rect.height *= value_;
                break;
        }
        
        // Render fill
        RenderFilledRect(fill_rect, fill_color_);
        
        // Render text
        if (show_text_) {
            std::string text = std::to_string(static_cast<int>(value_ * 100)) + "%";
            RenderTextCentered(text, GetBounds().GetCenter(), Color::White());
        }
    }
    
private:
    float value_ = 0.5f;
    Color background_color_{0.1f, 0.1f, 0.1f, 1.0f};
    Color fill_color_{0.2f, 0.8f, 0.2f, 1.0f};
    bool show_text_ = true;
    FillDirection fill_direction_ = FillDirection::LEFT_TO_RIGHT;
    
    void RenderFilledRect(const Rect& rect, const Color& color) {}
    void RenderTextCentered(const std::string& text, const Vector2& pos, const Color& color) {}
};

// [SEQUENCE: 2382] UI Window - Draggable container
class UIWindow : public UIPanel {
public:
    UIWindow(const std::string& name) : UIPanel(name) {
        // Create title bar
        title_bar_ = std::make_shared<UIPanel>("TitleBar");
        title_bar_->SetSize({size_.x, title_bar_height_});
        AddChild(title_bar_);
        
        // Create title label
        title_label_ = std::make_shared<UILabel>("Title");
        title_label_->SetText(name);
        title_bar_->AddChild(title_label_);
        
        // Create close button
        close_button_ = std::make_shared<UIButton>("CloseButton");
        close_button_->SetText("X");
        close_button_->SetSize({title_bar_height_, title_bar_height_});
        close_button_->SetPosition({size_.x - title_bar_height_, 0});
        close_button_->SetOnClick([this]() { Close(); });
        title_bar_->AddChild(close_button_);
    }
    
    void SetTitle(const std::string& title) {
        title_label_->SetText(title);
    }
    
    void SetDraggable(bool draggable) {
        draggable_ = draggable;
    }
    
    void SetResizable(bool resizable) {
        resizable_ = resizable;
    }
    
    void Close() {
        SetVisibility(Visibility::HIDDEN);
        if (on_close_) {
            on_close_();
        }
    }
    
    void SetOnClose(std::function<void()> callback) {
        on_close_ = callback;
    }
    
protected:
    bool HandleMouseButton(int button, bool pressed, float x, float y) override {
        if (button == 0 && draggable_) {
            Vector2 local = ScreenToLocal({x, y});
            
            if (pressed && title_bar_->GetBounds().Contains(local)) {
                is_dragging_ = true;
                drag_offset_ = local;
                return true;
            } else if (!pressed) {
                is_dragging_ = false;
            }
        }
        
        return UIPanel::HandleMouseButton(button, pressed, x, y);
    }
    
    bool HandleMouseMove(float x, float y) override {
        if (is_dragging_) {
            Vector2 new_pos = {x, y};
            new_pos = new_pos - drag_offset_;
            SetPosition(new_pos);
            return true;
        }
        
        return UIPanel::HandleMouseMove(x, y);
    }
    
private:
    float title_bar_height_ = 30.0f;
    bool draggable_ = true;
    bool resizable_ = false;
    bool is_dragging_ = false;
    Vector2 drag_offset_;
    
    std::shared_ptr<UIPanel> title_bar_;
    std::shared_ptr<UILabel> title_label_;
    std::shared_ptr<UIButton> close_button_;
    
    std::function<void()> on_close_;
};

// [SEQUENCE: 2383] UI Manager - Manages all UI elements
class UIManager {
public:
    static UIManager& Instance() {
        static UIManager instance;
        return instance;
    }
    
    // [SEQUENCE: 2384] Root management
    void SetRoot(std::shared_ptr<UIElement> root) {
        root_ = root;
    }
    
    std::shared_ptr<UIElement> GetRoot() const {
        return root_;
    }
    
    // [SEQUENCE: 2385] Update and render
    void Update(float delta_time) {
        if (root_) {
            root_->Update(delta_time);
        }
        
        // Update tooltips, animations, etc.
        UpdateTooltip(delta_time);
    }
    
    void Render() {
        if (root_) {
            root_->Render();
        }
        
        // Render tooltip on top
        RenderTooltip();
    }
    
    // [SEQUENCE: 2386] Event handling
    bool HandleMouseMove(float x, float y) {
        mouse_x_ = x;
        mouse_y_ = y;
        
        if (root_) {
            return root_->HandleMouseMove(x, y);
        }
        
        return false;
    }
    
    bool HandleMouseButton(int button, bool pressed, float x, float y) {
        if (root_) {
            return root_->HandleMouseButton(button, pressed, x, y);
        }
        
        return false;
    }
    
    bool HandleKeyboard(int key, bool pressed) {
        if (focused_element_) {
            return focused_element_->HandleKeyboard(key, pressed);
        }
        
        return false;
    }
    
    // [SEQUENCE: 2387] Focus management
    void SetFocusedElement(UIElement* element) {
        if (focused_element_) {
            focused_element_->has_focus_ = false;
        }
        
        focused_element_ = element;
        
        if (focused_element_) {
            focused_element_->has_focus_ = true;
        }
    }
    
    // [SEQUENCE: 2388] Tooltip system
    void ShowTooltip(const std::string& text, float x, float y) {
        tooltip_text_ = text;
        tooltip_x_ = x;
        tooltip_y_ = y;
        tooltip_visible_ = true;
    }
    
    void HideTooltip() {
        tooltip_visible_ = false;
    }
    
    // [SEQUENCE: 2389] Screen management
    void SetScreenSize(float width, float height) {
        screen_width_ = width;
        screen_height_ = height;
    }
    
    Vector2 GetScreenSize() const {
        return {screen_width_, screen_height_};
    }
    
private:
    UIManager() = default;
    
    std::shared_ptr<UIElement> root_;
    UIElement* focused_element_ = nullptr;
    
    float screen_width_ = 1920.0f;
    float screen_height_ = 1080.0f;
    float mouse_x_ = 0, mouse_y_ = 0;
    
    // Tooltip
    bool tooltip_visible_ = false;
    std::string tooltip_text_;
    float tooltip_x_ = 0, tooltip_y_ = 0;
    float tooltip_delay_ = 0.5f;
    float tooltip_timer_ = 0.0f;
    
    void UpdateTooltip(float delta_time) {
        if (tooltip_visible_) {
            tooltip_timer_ += delta_time;
        } else {
            tooltip_timer_ = 0.0f;
        }
    }
    
    void RenderTooltip() {
        if (tooltip_visible_ && tooltip_timer_ >= tooltip_delay_) {
            // Render tooltip background and text
            // This would interface with the rendering system
        }
    }
};

// [SEQUENCE: 2390] Layout system base
class UILayout {
public:
    virtual ~UILayout() = default;
    virtual void ArrangeChildren(UIElement* parent) = 0;
};

// [SEQUENCE: 2391] Horizontal layout
class HorizontalLayout : public UILayout {
public:
    HorizontalLayout(float spacing = 5.0f) : spacing_(spacing) {}
    
    void ArrangeChildren(UIElement* parent) override {
        float current_x = 0;
        
        for (const auto& child : parent->GetChildren()) {
            child->SetPosition({current_x, 0});
            current_x += child->GetSize().x + spacing_;
        }
    }
    
private:
    float spacing_;
};

// [SEQUENCE: 2392] Vertical layout
class VerticalLayout : public UILayout {
public:
    VerticalLayout(float spacing = 5.0f) : spacing_(spacing) {}
    
    void ArrangeChildren(UIElement* parent) override {
        float current_y = 0;
        
        for (const auto& child : parent->GetChildren()) {
            child->SetPosition({0, current_y});
            current_y += child->GetSize().y + spacing_;
        }
    }
    
private:
    float spacing_;
};

// [SEQUENCE: 2393] Grid layout
class GridLayout : public UILayout {
public:
    GridLayout(int columns, float spacing = 5.0f) 
        : columns_(columns), spacing_(spacing) {}
    
    void ArrangeChildren(UIElement* parent) override {
        int index = 0;
        
        for (const auto& child : parent->GetChildren()) {
            int row = index / columns_;
            int col = index % columns_;
            
            float x = col * (child->GetSize().x + spacing_);
            float y = row * (child->GetSize().y + spacing_);
            
            child->SetPosition({x, y});
            index++;
        }
    }
    
private:
    int columns_;
    float spacing_;
};

} // namespace mmorpg::ui