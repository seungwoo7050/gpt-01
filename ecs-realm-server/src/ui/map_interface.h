#pragma once

#include "ui_framework.h"
#include "../world/world_map.h"
#include "../core/math_utils.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <cmath>
#include <spdlog/spdlog.h>

namespace mmorpg::ui {

// [SEQUENCE: MVP12-100] Map interface system for navigation
// 맵 인터페이스 - 월드 네비게이션을 위한 UI 시스템

// [SEQUENCE: MVP12-101] Map icon types
enum class MapIconType {
    PLAYER,                 // 플레이어 위치
    PARTY_MEMBER,          // 파티원
    QUEST_AVAILABLE,       // 퀘스트 가능
    QUEST_COMPLETE,        // 퀘스트 완료
    VENDOR,                // 상인
    REPAIR,                // 수리 NPC
    MAILBOX,               // 우편함
    BANK,                  // 은행
    FLIGHT_MASTER,         // 비행 조련사
    INN,                   // 여관
    TRAINER,               // 직업 훈련사
    DUNGEON_ENTRANCE,      // 던전 입구
    RAID_ENTRANCE,         // 레이드 입구
    PVP_ZONE,              // PvP 지역
    RESOURCE_NODE,         // 자원 노드
    ENEMY,                 // 적 위치
    NEUTRAL,               // 중립 NPC
    FRIENDLY,              // 우호 NPC
    WAYPOINT,              // 웨이포인트
    CUSTOM                 // 커스텀 마커
};

// [SEQUENCE: MVP12-102] Map icon data
struct MapIcon {
    MapIconType type;
    Vector2 world_position;     // 월드 좌표
    std::string tooltip;        // 툴팁 텍스트
    Color tint_color;          // 아이콘 색상
    float scale = 1.0f;        // 아이콘 크기
    bool is_tracked = false;   // 추적 중인지
    uint32_t entity_id = 0;    // 연관된 엔티티 ID
    
    // [SEQUENCE: MVP12-103] Get icon texture based on type
    uint32_t GetTextureId() const {
        // Would return actual texture IDs
        switch (type) {
            case MapIconType::PLAYER: return 1001;
            case MapIconType::PARTY_MEMBER: return 1002;
            case MapIconType::QUEST_AVAILABLE: return 1003;
            case MapIconType::QUEST_COMPLETE: return 1004;
            case MapIconType::VENDOR: return 1005;
            default: return 1000;
        }
    }
};

// [SEQUENCE: MVP12-104] Minimap widget
class Minimap : public UIPanel {
public:
    Minimap(const std::string& name) : UIPanel(name) {
        SetSize({200, 200});
        SetBackgroundColor({0.0f, 0.0f, 0.0f, 0.8f});
        SetBorderColor({0.5f, 0.5f, 0.5f, 1.0f});
        SetBorderWidth(2.0f);
        
        // [SEQUENCE: MVP12-105] Create map texture display
        map_texture_ = std::make_shared<UIImage>("MapTexture");
        map_texture_->SetSize({196, 196});
        map_texture_->SetPosition({2, 2});
        AddChild(map_texture_);
        
        // [SEQUENCE: MVP12-106] Create player arrow
        player_arrow_ = std::make_shared<UIImage>("PlayerArrow");
        player_arrow_->SetSize({16, 16});
        player_arrow_->SetTint({1.0f, 1.0f, 0.0f, 1.0f});
        player_arrow_->SetPivot({0.5f, 0.5f});
        AddChild(player_arrow_);
        
        // [SEQUENCE: MVP12-107] Create zoom controls
        zoom_in_ = std::make_shared<UIButton>("ZoomIn");
        zoom_in_->SetText("+");
        zoom_in_->SetSize({20, 20});
        zoom_in_->SetPosition({175, 5});
        zoom_in_->SetOnClick([this]() { ZoomIn(); });
        AddChild(zoom_in_);
        
        zoom_out_ = std::make_shared<UIButton>("ZoomOut");
        zoom_out_->SetText("-");
        zoom_out_->SetSize({20, 20});
        zoom_out_->SetPosition({175, 30});
        zoom_out_->SetOnClick([this]() { ZoomOut(); });
        AddChild(zoom_out_);
        
        // [SEQUENCE: MVP12-108] Create coordinate display
        coord_label_ = std::make_shared<UILabel>("Coordinates");
        coord_label_->SetPosition({5, 180});
        coord_label_->SetSize({100, 16});
        coord_label_->SetFontSize(11.0f);
        coord_label_->SetTextColor({0.8f, 0.8f, 0.8f, 1.0f});
        AddChild(coord_label_);
        
        // Create zone name label
        zone_label_ = std::make_shared<UILabel>("ZoneName");
        zone_label_->SetPosition({5, 5});
        zone_label_->SetSize({165, 16});
        zone_label_->SetFontSize(12.0f);
        zone_label_->SetTextColor({1.0f, 1.0f, 0.8f, 1.0f});
        AddChild(zone_label_);
        
        // Create icon container
        icon_container_ = std::make_shared<UIPanel>("IconContainer");
        icon_container_->SetSize({196, 196});
        icon_container_->SetPosition({2, 2});
        icon_container_->SetBackgroundColor({0, 0, 0, 0});
        AddChild(icon_container_);
    }
    
    // [SEQUENCE: MVP12-109] Update player position
    void UpdatePlayerPosition(float x, float y, float facing) {
        player_world_x_ = x;
        player_world_y_ = y;
        player_facing_ = facing;
        
        // Update coordinate display
        coord_label_->SetText(std::to_string(static_cast<int>(x)) + ", " + 
                             std::to_string(static_cast<int>(y)));
        
        // Center player arrow
        player_arrow_->SetPosition({GetSize().x * 0.5f, GetSize().y * 0.5f});
        player_arrow_->SetRotation(-facing * 180.0f / M_PI);  // Convert to degrees
        
        UpdateMapDisplay();
    }
    
    // [SEQUENCE: MVP12-110] Set current zone
    void SetZone(const std::string& zone_name, uint32_t zone_id) {
        current_zone_ = zone_name;
        zone_id_ = zone_id;
        zone_label_->SetText(zone_name);
        
        // Load zone minimap texture
        LoadZoneTexture(zone_id);
    }
    
    // [SEQUENCE: MVP12-111] Add map icon
    void AddMapIcon(const MapIcon& icon) {
        map_icons_[icon.entity_id] = icon;
        UpdateMapDisplay();
    }
    
    void RemoveMapIcon(uint32_t entity_id) {
        map_icons_.erase(entity_id);
        UpdateMapDisplay();
    }
    
    void UpdateMapIcon(uint32_t entity_id, const Vector2& new_position) {
        auto it = map_icons_.find(entity_id);
        if (it != map_icons_.end()) {
            it->second.world_position = new_position;
            UpdateMapDisplay();
        }
    }
    
    // [SEQUENCE: MVP12-112] Set tracking target
    void SetTracking(uint32_t entity_id, bool track) {
        auto it = map_icons_.find(entity_id);
        if (it != map_icons_.end()) {
            it->second.is_tracked = track;
            UpdateMapDisplay();
        }
    }
    
protected:
    void OnMouseDown() override {
        // Click on minimap to open world map
        if (on_minimap_click_) {
            on_minimap_click_();
        }
    }
    
    bool HandleMouseButton(int button, bool pressed, float x, float y) override {
        if (button == 1 && pressed) {  // Right click
            // Add waypoint at clicked position
            Vector2 local = ScreenToLocal({x, y});
            Vector2 world_pos = MinimapToWorld(local);
            
            if (on_waypoint_add_) {
                on_waypoint_add_(world_pos.x, world_pos.y);
            }
            return true;
        }
        
        return UIPanel::HandleMouseButton(button, pressed, x, y);
    }
    
private:
    float player_world_x_ = 0;
    float player_world_y_ = 0;
    float player_facing_ = 0;
    
    std::string current_zone_;
    uint32_t zone_id_ = 0;
    
    float zoom_level_ = 1.0f;
    static constexpr float min_zoom_ = 0.5f;
    static constexpr float max_zoom_ = 4.0f;
    static constexpr float zoom_step_ = 0.5f;
    
    std::shared_ptr<UIImage> map_texture_;
    std::shared_ptr<UIImage> player_arrow_;
    std::shared_ptr<UIButton> zoom_in_;
    std::shared_ptr<UIButton> zoom_out_;
    std::shared_ptr<UILabel> coord_label_;
    std::shared_ptr<UILabel> zone_label_;
    std::shared_ptr<UIPanel> icon_container_;
    
    std::unordered_map<uint32_t, MapIcon> map_icons_;
    std::vector<std::shared_ptr<UIImage>> icon_images_;
    
    std::function<void()> on_minimap_click_;
    std::function<void(float, float)> on_waypoint_add_;
    
    // [SEQUENCE: MVP12-113] Update minimap display
    void UpdateMapDisplay() {
        // Clear existing icons
        for (auto& icon_image : icon_images_) {
            icon_container_->RemoveChild(icon_image.get());
        }
        icon_images_.clear();
        
        // Calculate visible range
        float visible_range = 100.0f / zoom_level_;  // World units
        
        // Add visible icons
        for (const auto& [id, icon] : map_icons_) {
            float dx = icon.world_position.x - player_world_x_;
            float dy = icon.world_position.y - player_world_y_;
            float distance = std::sqrt(dx * dx + dy * dy);
            
            if (distance <= visible_range) {
                // Convert world position to minimap position
                Vector2 minimap_pos = WorldToMinimap(icon.world_position);
                
                auto icon_image = std::make_shared<UIImage>("Icon_" + std::to_string(id));
                icon_image->SetTexture(icon.GetTextureId());
                icon_image->SetSize({12 * icon.scale, 12 * icon.scale});
                icon_image->SetPosition(minimap_pos);
                icon_image->SetTint(icon.tint_color);
                icon_image->SetPivot({0.5f, 0.5f});
                
                icon_images_.push_back(icon_image);
                icon_container_->AddChild(icon_image);
                
                // Add tracking arrow if tracked
                if (icon.is_tracked) {
                    AddTrackingArrow(minimap_pos);
                }
            }
        }
    }
    
    // [SEQUENCE: MVP12-114] Convert world coordinates to minimap
    Vector2 WorldToMinimap(const Vector2& world_pos) {
        float dx = world_pos.x - player_world_x_;
        float dy = world_pos.y - player_world_y_;
        
        // Scale to minimap size
        float scale = (GetSize().x * 0.5f) / (100.0f / zoom_level_);
        
        // Rotate based on player facing (minimap rotates with player)
        float cos_f = std::cos(player_facing_);
        float sin_f = std::sin(player_facing_);
        float rotated_x = dx * cos_f - dy * sin_f;
        float rotated_y = dx * sin_f + dy * cos_f;
        
        // Center on minimap
        float minimap_x = GetSize().x * 0.5f + rotated_x * scale;
        float minimap_y = GetSize().y * 0.5f - rotated_y * scale;  // Flip Y
        
        return {minimap_x, minimap_y};
    }
    
    Vector2 MinimapToWorld(const Vector2& minimap_pos) {
        // Inverse of WorldToMinimap
        float dx = minimap_pos.x - GetSize().x * 0.5f;
        float dy = GetSize().y * 0.5f - minimap_pos.y;  // Flip Y
        
        float scale = (100.0f / zoom_level_) / (GetSize().x * 0.5f);
        dx *= scale;
        dy *= scale;
        
        // Rotate back based on player facing
        float cos_f = std::cos(-player_facing_);
        float sin_f = std::sin(-player_facing_);
        float world_x = player_world_x_ + dx * cos_f - dy * sin_f;
        float world_y = player_world_y_ + dx * sin_f + dy * cos_f;
        
        return {world_x, world_y};
    }
    
    void ZoomIn() {
        zoom_level_ = std::min(max_zoom_, zoom_level_ + zoom_step_);
        UpdateMapDisplay();
    }
    
    void ZoomOut() {
        zoom_level_ = std::max(min_zoom_, zoom_level_ - zoom_step_);
        UpdateMapDisplay();
    }
    
    void LoadZoneTexture(uint32_t zone_id) {
        // Would load the appropriate minimap texture for the zone
        map_texture_->SetTexture(2000 + zone_id);  // Placeholder
    }
    
    void AddTrackingArrow(const Vector2& target_pos) {
        // Would add directional arrow pointing to tracked target
    }
    
public:
    void SetOnMinimapClick(std::function<void()> callback) {
        on_minimap_click_ = callback;
    }
    
    void SetOnWaypointAdd(std::function<void(float, float)> callback) {
        on_waypoint_add_ = callback;
    }
};

// [SEQUENCE: MVP12-115] World map window
class WorldMapWindow : public UIWindow {
public:
    WorldMapWindow(const std::string& name) : UIWindow(name) {
        SetTitle("World Map");
        SetSize({800, 600});
        
        // [SEQUENCE: MVP12-116] Create map viewport
        map_viewport_ = std::make_shared<UIPanel>("MapViewport");
        map_viewport_->SetPosition({10, 40});
        map_viewport_->SetSize({580, 500});
        map_viewport_->SetBackgroundColor({0.1f, 0.1f, 0.1f, 1.0f});
        AddChild(map_viewport_);
        
        // Create map image
        map_image_ = std::make_shared<UIImage>("MapImage");
        map_image_->SetSize({2000, 2000});  // Large world map
        map_image_->SetPosition({0, 0});
        map_viewport_->AddChild(map_image_);
        
        // [SEQUENCE: MVP12-117] Create continent selector
        CreateContinentTabs();
        
        // [SEQUENCE: MVP12-118] Create zoom slider
        zoom_slider_ = std::make_shared<UIProgressBar>("ZoomSlider");
        zoom_slider_->SetPosition({600, 100});
        zoom_slider_->SetSize({180, 20});
        zoom_slider_->SetValue(0.5f);  // Default zoom
        AddChild(zoom_slider_);
        
        zoom_label_ = std::make_shared<UILabel>("ZoomLabel");
        zoom_label_->SetText("Zoom: 100%");
        zoom_label_->SetPosition({600, 80});
        zoom_label_->SetSize({180, 16});
        AddChild(zoom_label_);
        
        // [SEQUENCE: MVP12-119] Create legend
        CreateLegend();
        
        // [SEQUENCE: MVP12-120] Create search box
        search_box_ = std::make_shared<UIPanel>("SearchBox");
        search_box_->SetPosition({600, 400});
        search_box_->SetSize({180, 25});
        search_box_->SetBackgroundColor({0.2f, 0.2f, 0.2f, 0.9f});
        AddChild(search_box_);
        
        search_label_ = std::make_shared<UILabel>("SearchLabel");
        search_label_->SetText("Search location...");
        search_label_->SetPosition({5, 3});
        search_label_->SetSize({170, 19});
        search_label_->SetFontSize(12.0f);
        search_box_->AddChild(search_label_);
        
        // Player position marker
        player_marker_ = std::make_shared<UIImage>("PlayerMarker");
        player_marker_->SetSize({20, 20});
        player_marker_->SetTint({1.0f, 1.0f, 0.0f, 1.0f});
        player_marker_->SetPivot({0.5f, 0.5f});
        map_viewport_->AddChild(player_marker_);
    }
    
    // [SEQUENCE: MVP12-121] Set current continent
    void SetContinent(int continent_id) {
        current_continent_ = continent_id;
        
        // Load continent map
        LoadContinentMap(continent_id);
        
        // Update tab highlighting
        UpdateContinentTabs();
        
        // Reset zoom and position
        ResetView();
    }
    
    // [SEQUENCE: MVP12-122] Update player position on world map
    void UpdatePlayerPosition(float world_x, float world_y) {
        player_world_x_ = world_x;
        player_world_y_ = world_y;
        
        // Convert to map coordinates
        Vector2 map_pos = WorldToMapCoordinates({world_x, world_y});
        player_marker_->SetPosition(map_pos);
    }
    
    // [SEQUENCE: MVP12-123] Add point of interest
    void AddPointOfInterest(const std::string& name, float x, float y, 
                           MapIconType type) {
        PointOfInterest poi;
        poi.name = name;
        poi.world_position = {x, y};
        poi.type = type;
        
        points_of_interest_.push_back(poi);
        CreatePOIMarker(poi);
    }
    
protected:
    bool HandleMouseButton(int button, bool pressed, float x, float y) override {
        Vector2 local = ScreenToLocal({x, y});
        
        if (map_viewport_->GetBounds().Contains(local)) {
            if (button == 0 && pressed) {  // Left click
                // Start dragging map
                is_dragging_map_ = true;
                drag_start_ = local;
                map_start_pos_ = map_image_->GetPosition();
                return true;
            } else if (button == 0 && !pressed) {
                is_dragging_map_ = false;
            } else if (button == 1 && pressed) {  // Right click
                // Add waypoint
                Vector2 map_local = local - map_viewport_->GetPosition();
                Vector2 world_pos = MapToWorldCoordinates(map_local);
                AddWaypoint(world_pos);
                return true;
            }
        }
        
        return UIWindow::HandleMouseButton(button, pressed, x, y);
    }
    
    bool HandleMouseMove(float x, float y) override {
        if (is_dragging_map_) {
            Vector2 local = ScreenToLocal({x, y});
            Vector2 delta = local - drag_start_;
            
            // Update map position
            Vector2 new_pos = map_start_pos_ + delta;
            
            // Clamp to viewport bounds
            ClampMapPosition(new_pos);
            map_image_->SetPosition(new_pos);
            
            return true;
        }
        
        return UIWindow::HandleMouseMove(x, y);
    }
    
private:
    struct PointOfInterest {
        std::string name;
        Vector2 world_position;
        MapIconType type;
    };
    
    int current_continent_ = 0;
    float player_world_x_ = 0;
    float player_world_y_ = 0;
    
    float current_zoom_ = 1.0f;
    bool is_dragging_map_ = false;
    Vector2 drag_start_;
    Vector2 map_start_pos_;
    
    std::shared_ptr<UIPanel> map_viewport_;
    std::shared_ptr<UIImage> map_image_;
    std::shared_ptr<UIProgressBar> zoom_slider_;
    std::shared_ptr<UILabel> zoom_label_;
    std::shared_ptr<UIPanel> search_box_;
    std::shared_ptr<UILabel> search_label_;
    std::shared_ptr<UIImage> player_marker_;
    
    std::vector<std::shared_ptr<UIButton>> continent_tabs_;
    std::vector<std::shared_ptr<UIPanel>> legend_items_;
    std::vector<PointOfInterest> points_of_interest_;
    std::vector<std::shared_ptr<UIImage>> poi_markers_;
    std::vector<std::shared_ptr<UIImage>> waypoint_markers_;
    
    // [SEQUENCE: MVP12-124] Create continent selection tabs
    void CreateContinentTabs() {
        std::vector<std::string> continent_names = {
            "Eastern Kingdoms",
            "Kalimdor", 
            "Northrend",
            "Pandaria"
        };
        
        float tab_width = 140.0f;
        float x_offset = 10.0f;
        
        for (int i = 0; i < continent_names.size(); ++i) {
            auto tab = std::make_shared<UIButton>("ContinentTab_" + std::to_string(i));
            tab->SetText(continent_names[i]);
            tab->SetSize({tab_width, 25});
            tab->SetPosition({x_offset + i * (tab_width + 5), 10});
            
            int continent_id = i;
            tab->SetOnClick([this, continent_id]() { SetContinent(continent_id); });
            
            continent_tabs_.push_back(tab);
            AddChild(tab);
        }
    }
    
    // [SEQUENCE: MVP12-125] Create map legend
    void CreateLegend() {
        auto legend_panel = std::make_shared<UIPanel>("Legend");
        legend_panel->SetPosition({600, 200});
        legend_panel->SetSize({180, 180});
        legend_panel->SetBackgroundColor({0.1f, 0.1f, 0.1f, 0.8f});
        AddChild(legend_panel);
        
        auto legend_title = std::make_shared<UILabel>("LegendTitle");
        legend_title->SetText("Legend");
        legend_title->SetPosition({5, 5});
        legend_title->SetSize({170, 16});
        legend_title->SetFontSize(14.0f);
        legend_panel->AddChild(legend_title);
        
        // Add legend items
        struct LegendItem {
            std::string label;
            Color color;
            MapIconType icon;
        };
        
        std::vector<LegendItem> items = {
            {"Quest Available", {1.0f, 1.0f, 0.0f, 1.0f}, MapIconType::QUEST_AVAILABLE},
            {"Vendor", {0.7f, 0.7f, 0.7f, 1.0f}, MapIconType::VENDOR},
            {"Flight Path", {0.5f, 1.0f, 0.5f, 1.0f}, MapIconType::FLIGHT_MASTER},
            {"Dungeon", {1.0f, 0.5f, 0.0f, 1.0f}, MapIconType::DUNGEON_ENTRANCE},
            {"Your Position", {1.0f, 1.0f, 0.0f, 1.0f}, MapIconType::PLAYER}
        };
        
        float y_offset = 25.0f;
        for (const auto& item : items) {
            auto icon = std::make_shared<UIImage>("LegendIcon");
            icon->SetSize({16, 16});
            icon->SetPosition({5, y_offset});
            icon->SetTint(item.color);
            legend_panel->AddChild(icon);
            
            auto label = std::make_shared<UILabel>("LegendLabel");
            label->SetText(item.label);
            label->SetPosition({25, y_offset});
            label->SetSize({150, 16});
            label->SetFontSize(11.0f);
            legend_panel->AddChild(label);
            
            y_offset += 20.0f;
        }
    }
    
    void UpdateContinentTabs() {
        for (int i = 0; i < continent_tabs_.size(); ++i) {
            if (i == current_continent_) {
                continent_tabs_[i]->SetButtonColors(
                    {0.4f, 0.4f, 0.4f, 1.0f},
                    {0.5f, 0.5f, 0.5f, 1.0f},
                    {0.3f, 0.3f, 0.3f, 1.0f},
                    {0.2f, 0.2f, 0.2f, 0.5f}
                );
            } else {
                continent_tabs_[i]->SetButtonColors(
                    {0.2f, 0.2f, 0.2f, 0.8f},
                    {0.3f, 0.3f, 0.3f, 0.8f},
                    {0.4f, 0.4f, 0.2f, 0.8f},
                    {0.1f, 0.1f, 0.1f, 0.5f}
                );
            }
        }
    }
    
    void LoadContinentMap(int continent_id) {
        // Would load the appropriate continent texture
        map_image_->SetTexture(3000 + continent_id);
    }
    
    void ResetView() {
        current_zoom_ = 1.0f;
        map_image_->SetPosition({0, 0});
        zoom_slider_->SetValue(0.5f);
        UpdateZoomLabel();
    }
    
    void UpdateZoomLabel() {
        int zoom_percent = static_cast<int>(current_zoom_ * 100);
        zoom_label_->SetText("Zoom: " + std::to_string(zoom_percent) + "%");
    }
    
    Vector2 WorldToMapCoordinates(const Vector2& world_pos) {
        // Convert world coordinates to map image coordinates
        // This would use actual world-to-map transformation data
        float map_scale = 0.1f;  // Example: 1 world unit = 0.1 map pixels
        return {world_pos.x * map_scale, world_pos.y * map_scale};
    }
    
    Vector2 MapToWorldCoordinates(const Vector2& map_pos) {
        // Inverse of WorldToMapCoordinates
        float map_scale = 0.1f;
        return {map_pos.x / map_scale, map_pos.y / map_scale};
    }
    
    void ClampMapPosition(Vector2& pos) {
        // Prevent map from being dragged outside viewport
        float max_x = 0;
        float min_x = map_viewport_->GetSize().x - map_image_->GetSize().x * current_zoom_;
        float max_y = 0;
        float min_y = map_viewport_->GetSize().y - map_image_->GetSize().y * current_zoom_;
        
        pos.x = std::clamp(pos.x, min_x, max_x);
        pos.y = std::clamp(pos.y, min_y, max_y);
    }
    
    void CreatePOIMarker(const PointOfInterest& poi) {
        auto marker = std::make_shared<UIImage>("POI_" + poi.name);
        marker->SetSize({16, 16});
        marker->SetPivot({0.5f, 0.5f});
        
        Vector2 map_pos = WorldToMapCoordinates(poi.world_position);
        marker->SetPosition(map_pos);
        
        // Set color based on type
        switch (poi.type) {
            case MapIconType::QUEST_AVAILABLE:
                marker->SetTint({1.0f, 1.0f, 0.0f, 1.0f});
                break;
            case MapIconType::VENDOR:
                marker->SetTint({0.7f, 0.7f, 0.7f, 1.0f});
                break;
            default:
                marker->SetTint({1.0f, 1.0f, 1.0f, 1.0f});
                break;
        }
        
        poi_markers_.push_back(marker);
        map_viewport_->AddChild(marker);
    }
    
    void AddWaypoint(const Vector2& world_pos) {
        auto waypoint = std::make_shared<UIImage>("Waypoint");
        waypoint->SetSize({24, 24});
        waypoint->SetPivot({0.5f, 0.5f});
        waypoint->SetTint({0.0f, 1.0f, 1.0f, 1.0f});
        
        Vector2 map_pos = WorldToMapCoordinates(world_pos);
        waypoint->SetPosition(map_pos);
        
        waypoint_markers_.push_back(waypoint);
        map_viewport_->AddChild(waypoint);
        
        spdlog::info("Added waypoint at world position: {}, {}", 
                    world_pos.x, world_pos.y);
    }
};

// [SEQUENCE: MVP12-126] Quest tracker window
class QuestTracker : public UIPanel {
public:
    QuestTracker(const std::string& name) : UIPanel(name) {
        SetSize({250, 300});
        SetBackgroundColor({0.0f, 0.0f, 0.0f, 0.7f});
        
        // [SEQUENCE: MVP12-127] Create title
        auto title = std::make_shared<UILabel>("Title");
        title->SetText("Quest Tracker");
        title->SetPosition({5, 5});
        title->SetSize({240, 20});
        title->SetFontSize(14.0f);
        title->SetTextColor({1.0f, 1.0f, 0.8f, 1.0f});
        AddChild(title);
        
        // Create quest list container
        quest_container_ = std::make_shared<UIPanel>("QuestContainer");
        quest_container_->SetPosition({5, 30});
        quest_container_->SetSize({240, 265});
        quest_container_->SetBackgroundColor({0, 0, 0, 0});
        AddChild(quest_container_);
    }
    
    // [SEQUENCE: MVP12-128] Add quest to tracker
    void AddQuest(uint32_t quest_id, const std::string& quest_name,
                  const std::vector<std::string>& objectives) {
        
        auto quest_panel = std::make_shared<UIPanel>("Quest_" + std::to_string(quest_id));
        quest_panel->SetSize({235, 60});  // Base height
        quest_panel->SetBackgroundColor({0.1f, 0.1f, 0.1f, 0.5f});
        
        // Quest name
        auto name_label = std::make_shared<UILabel>("QuestName");
        name_label->SetText(quest_name);
        name_label->SetPosition({5, 3});
        name_label->SetSize({225, 16});
        name_label->SetFontSize(12.0f);
        name_label->SetTextColor({1.0f, 0.8f, 0.4f, 1.0f});
        quest_panel->AddChild(name_label);
        
        // Objectives
        float y_offset = 20.0f;
        for (const auto& objective : objectives) {
            auto obj_label = std::make_shared<UILabel>("Objective");
            obj_label->SetText("- " + objective);
            obj_label->SetPosition({10, y_offset});
            obj_label->SetSize({220, 16});
            obj_label->SetFontSize(11.0f);
            obj_label->SetTextColor({0.8f, 0.8f, 0.8f, 1.0f});
            quest_panel->AddChild(obj_label);
            
            y_offset += 16.0f;
        }
        
        quest_panel->SetSize({235, y_offset + 5});
        
        // Position based on existing quests
        float quest_y = 0;
        for (const auto& [id, panel] : tracked_quests_) {
            quest_y += panel->GetSize().y + 5;
        }
        quest_panel->SetPosition({0, quest_y});
        
        tracked_quests_[quest_id] = quest_panel;
        quest_container_->AddChild(quest_panel);
    }
    
    void RemoveQuest(uint32_t quest_id) {
        auto it = tracked_quests_.find(quest_id);
        if (it != tracked_quests_.end()) {
            quest_container_->RemoveChild(it->second.get());
            tracked_quests_.erase(it);
            
            // Reposition remaining quests
            RepositionQuests();
        }
    }
    
    void UpdateObjective(uint32_t quest_id, int objective_index, 
                        const std::string& new_text, bool completed) {
        auto it = tracked_quests_.find(quest_id);
        if (it != tracked_quests_.end()) {
            // Update objective text and color
            // Would find the specific objective label and update it
        }
    }
    
private:
    std::shared_ptr<UIPanel> quest_container_;
    std::unordered_map<uint32_t, std::shared_ptr<UIPanel>> tracked_quests_;
    
    void RepositionQuests() {
        float y_offset = 0;
        for (auto& [id, panel] : tracked_quests_) {
            panel->SetPosition({0, y_offset});
            y_offset += panel->GetSize().y + 5;
        }
    }
};

// [SEQUENCE: MVP12-129] Map UI manager
class MapUIManager {
public:
    static MapUIManager& Instance() {
        static MapUIManager instance;
        return instance;
    }
    
    void Initialize() {
        auto root = UIManager::Instance().GetRoot();
        if (!root) return;
        
        // [SEQUENCE: MVP12-130] Create minimap
        minimap_ = std::make_shared<Minimap>("Minimap");
        minimap_->SetPosition({-220, 20});
        minimap_->SetAnchor(AnchorType::TOP_RIGHT);
        root->AddChild(minimap_);
        
        // [SEQUENCE: MVP12-131] Create world map
        world_map_ = std::make_shared<WorldMapWindow>("WorldMap");
        world_map_->SetPosition({100, 50});
        world_map_->SetVisibility(Visibility::HIDDEN);
        root->AddChild(world_map_);
        
        // [SEQUENCE: MVP12-132] Create quest tracker
        quest_tracker_ = std::make_shared<QuestTracker>("QuestTracker");
        quest_tracker_->SetPosition({-270, 250});
        quest_tracker_->SetAnchor(AnchorType::TOP_RIGHT);
        root->AddChild(quest_tracker_);
        
        // Set minimap click to open world map
        minimap_->SetOnMinimapClick([this]() { ToggleWorldMap(); });
    }
    
    // [SEQUENCE: MVP12-133] Update player position
    void UpdatePlayerPosition(float x, float y, float facing) {
        if (minimap_) {
            minimap_->UpdatePlayerPosition(x, y, facing);
        }
        
        if (world_map_ && world_map_->IsVisible()) {
            world_map_->UpdatePlayerPosition(x, y);
        }
    }
    
    void SetZone(const std::string& zone_name, uint32_t zone_id) {
        if (minimap_) {
            minimap_->SetZone(zone_name, zone_id);
        }
    }
    
    // Minimap icon management
    void AddMinimapIcon(const MapIcon& icon) {
        if (minimap_) {
            minimap_->AddMapIcon(icon);
        }
    }
    
    void RemoveMinimapIcon(uint32_t entity_id) {
        if (minimap_) {
            minimap_->RemoveMapIcon(entity_id);
        }
    }
    
    void UpdateMinimapIcon(uint32_t entity_id, const Vector2& position) {
        if (minimap_) {
            minimap_->UpdateMapIcon(entity_id, position);
        }
    }
    
    // World map management
    void ToggleWorldMap() {
        if (world_map_) {
            auto vis = world_map_->IsVisible() ? 
                Visibility::HIDDEN : Visibility::VISIBLE;
            world_map_->SetVisibility(vis);
        }
    }
    
    void ShowWorldMap() {
        if (world_map_) {
            world_map_->SetVisibility(Visibility::VISIBLE);
        }
    }
    
    void HideWorldMap() {
        if (world_map_) {
            world_map_->SetVisibility(Visibility::HIDDEN);
        }
    }
    
    // Quest tracking
    void TrackQuest(uint32_t quest_id, const std::string& quest_name,
                   const std::vector<std::string>& objectives) {
        if (quest_tracker_) {
            quest_tracker_->AddQuest(quest_id, quest_name, objectives);
        }
    }
    
    void UntrackQuest(uint32_t quest_id) {
        if (quest_tracker_) {
            quest_tracker_->RemoveQuest(quest_id);
        }
    }
    
    void UpdateQuestObjective(uint32_t quest_id, int objective_index,
                             const std::string& new_text, bool completed) {
        if (quest_tracker_) {
            quest_tracker_->UpdateObjective(quest_id, objective_index, 
                                          new_text, completed);
        }
    }
    
private:
    MapUIManager() = default;
    
    std::shared_ptr<Minimap> minimap_;
    std::shared_ptr<WorldMapWindow> world_map_;
    std::shared_ptr<QuestTracker> quest_tracker_;
};

} // namespace mmorpg::ui
