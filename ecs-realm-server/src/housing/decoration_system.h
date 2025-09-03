#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include "player_housing.h"
#include "../core/types.h"

namespace mmorpg::housing {

// [SEQUENCE: 3331] Decoration categories
enum class DecorationCategory {
    FURNITURE,      // 가구
    LIGHTING,       // 조명
    WALL_DECOR,     // 벽 장식
    FLOOR_DECOR,    // 바닥 장식
    CEILING_DECOR,  // 천장 장식
    WINDOW_DECOR,   // 창문 장식
    GARDEN_DECOR,   // 정원 장식
    SEASONAL,       // 계절 장식
    SPECIAL        // 특별 장식
};

// [SEQUENCE: 3332] Decoration placement rules
enum class PlacementRule {
    FLOOR_ONLY,     // 바닥에만
    WALL_ONLY,      // 벽에만
    CEILING_ONLY,   // 천장에만
    SURFACE_REQUIRED, // 표면 필요
    STACKABLE,      // 쌓기 가능
    OUTDOOR_ONLY,   // 야외만
    INDOOR_ONLY,    // 실내만
    NO_OVERLAP,     // 겹침 불가
    ROTATABLE      // 회전 가능
};

// [SEQUENCE: 3333] Decoration item definition
struct DecorationItem {
    uint32_t item_id;
    std::string name;
    std::string description;
    
    DecorationCategory category;
    std::vector<PlacementRule> placement_rules;
    
    // Visual properties
    std::string model_path;
    std::string texture_path;
    std::vector<std::string> material_variants;
    
    // Physical properties
    BoundingBox bounds;
    float weight{1.0f};
    bool is_interactive{false};
    
    // Size and scaling
    Vector3 default_scale{1.0f, 1.0f, 1.0f};
    Vector3 min_scale{0.5f, 0.5f, 0.5f};
    Vector3 max_scale{2.0f, 2.0f, 2.0f};
    
    // Special properties
    bool emits_light{false};
    float light_radius{0.0f};
    Color light_color{1.0f, 1.0f, 1.0f, 1.0f};
    
    bool has_animation{false};
    std::string animation_name;
    
    bool has_particle_effect{false};
    std::string particle_effect_name;
};

// [SEQUENCE: 3334] Placed decoration instance
class PlacedDecoration {
public:
    PlacedDecoration(uint64_t instance_id, const DecorationItem& item);
    
    // [SEQUENCE: 3335] Decoration properties
    uint64_t GetInstanceId() const { return instance_id_; }
    uint32_t GetItemId() const { return item_.item_id; }
    const DecorationItem& GetItem() const { return item_; }
    
    // [SEQUENCE: 3336] Transform management
    void SetPosition(const Vector3& position) { position_ = position; }
    void SetRotation(const Quaternion& rotation) { rotation_ = rotation; }
    void SetScale(const Vector3& scale);
    
    Vector3 GetPosition() const { return position_; }
    Quaternion GetRotation() const { return rotation_; }
    Vector3 GetScale() const { return scale_; }
    
    // [SEQUENCE: 3337] Visual customization
    void SetMaterialVariant(uint32_t variant_index);
    void SetTint(const Color& tint) { tint_color_ = tint; }
    void SetEmissiveIntensity(float intensity);
    
    // [SEQUENCE: 3338] Interaction
    void SetInteractionEnabled(bool enabled) { interaction_enabled_ = enabled; }
    void SetCustomData(const std::string& key, const std::string& value);
    std::string GetCustomData(const std::string& key) const;
    
    // [SEQUENCE: 3339] State management
    void SetVisible(bool visible) { is_visible_ = visible; }
    bool IsVisible() const { return is_visible_; }
    
    BoundingBox GetWorldBounds() const;
    bool CheckCollision(const PlacedDecoration& other) const;
    
private:
    uint64_t instance_id_;
    DecorationItem item_;
    
    // Transform
    Vector3 position_{0, 0, 0};
    Quaternion rotation_{0, 0, 0, 1};
    Vector3 scale_{1, 1, 1};
    
    // Visual state
    uint32_t material_variant_{0};
    Color tint_color_{1, 1, 1, 1};
    float emissive_intensity_{1.0f};
    
    // State
    bool is_visible_{true};
    bool interaction_enabled_{false};
    
    // Custom properties
    std::unordered_map<std::string, std::string> custom_data_;
};

// [SEQUENCE: 3340] Decoration placement validator
class DecorationPlacementValidator {
public:
    struct ValidationResult {
        bool is_valid{false};
        std::vector<std::string> errors;
        std::vector<std::string> warnings;
    };
    
    // [SEQUENCE: 3341] Validation methods
    static ValidationResult ValidatePlacement(
        const DecorationItem& item,
        const Vector3& position,
        const HouseRoom& room,
        const std::vector<PlacedDecoration*>& existing_decorations);
    
    static bool CheckSurfaceRequirement(
        const DecorationItem& item,
        const Vector3& position,
        const HouseRoom& room);
    
    static bool CheckOverlap(
        const BoundingBox& bounds,
        const std::vector<PlacedDecoration*>& existing_decorations);
    
    static bool CheckPlacementRules(
        const DecorationItem& item,
        const Vector3& position,
        const HouseRoom& room);
    
private:
    static bool IsOnFloor(const Vector3& position, const HouseRoom& room);
    static bool IsOnWall(const Vector3& position, const HouseRoom& room);
    static bool IsOnCeiling(const Vector3& position, const HouseRoom& room);
};

// [SEQUENCE: 3342] Decoration theme system
class DecorationTheme {
public:
    struct ThemeData {
        std::string name;
        std::string description;
        
        // Color palette
        std::vector<Color> primary_colors;
        std::vector<Color> accent_colors;
        
        // Recommended items
        std::vector<uint32_t> furniture_ids;
        std::vector<uint32_t> lighting_ids;
        std::vector<uint32_t> decor_ids;
        
        // Material presets
        std::unordered_map<uint32_t, uint32_t> item_material_variants;
        
        // Lighting presets
        float ambient_light_level{0.5f};
        Color ambient_light_color{1, 1, 1, 1};
    };
    
    // [SEQUENCE: 3343] Theme management
    static std::vector<ThemeData> GetAvailableThemes();
    static ThemeData GetTheme(const std::string& theme_name);
    static void ApplyTheme(PlayerHouse& house, const std::string& theme_name);
    
private:
    static std::unordered_map<std::string, ThemeData> themes_;
};

// [SEQUENCE: 3344] Decoration preset system
class DecorationPreset {
public:
    struct PresetData {
        std::string name;
        std::string description;
        HouseType target_house_type;
        
        struct PresetItem {
            uint32_t item_id;
            Vector3 position;
            Quaternion rotation;
            Vector3 scale;
            uint32_t material_variant;
            Color tint;
        };
        
        std::unordered_map<uint32_t, std::vector<PresetItem>> room_decorations;
    };
    
    // [SEQUENCE: 3345] Preset application
    static std::vector<PresetData> GetPresetsForHouseType(HouseType type);
    static bool ApplyPreset(PlayerHouse& house, const std::string& preset_name);
    static PresetData CreateCustomPreset(const PlayerHouse& house, 
                                       const std::string& preset_name);
};

// [SEQUENCE: 3346] Decoration effects system
class DecorationEffects {
public:
    // [SEQUENCE: 3347] Light effects
    struct LightEffect {
        Color color;
        float intensity;
        float radius;
        bool flicker{false};
        float flicker_rate{1.0f};
        bool cast_shadows{true};
    };
    
    // [SEQUENCE: 3348] Particle effects
    struct ParticleEffect {
        std::string effect_name;
        Vector3 emission_offset;
        float emission_rate;
        float lifetime;
        bool loop{true};
    };
    
    // [SEQUENCE: 3349] Sound effects
    struct SoundEffect {
        std::string sound_name;
        float volume{1.0f};
        float radius{10.0f};
        bool loop{false};
        bool ambient{true};
    };
    
    // [SEQUENCE: 3350] Animation effects
    struct AnimationEffect {
        std::string animation_name;
        float speed{1.0f};
        bool loop{true};
        bool auto_play{true};
    };
    
    static void ApplyLightEffect(PlacedDecoration& decoration, 
                                const LightEffect& effect);
    static void ApplyParticleEffect(PlacedDecoration& decoration,
                                  const ParticleEffect& effect);
    static void ApplySoundEffect(PlacedDecoration& decoration,
                               const SoundEffect& effect);
    static void ApplyAnimationEffect(PlacedDecoration& decoration,
                                   const AnimationEffect& effect);
};

// [SEQUENCE: 3351] Decoration catalog
class DecorationCatalog {
public:
    static DecorationCatalog& Instance() {
        static DecorationCatalog instance;
        return instance;
    }
    
    // [SEQUENCE: 3352] Catalog management
    void RegisterItem(const DecorationItem& item);
    DecorationItem* GetItem(uint32_t item_id);
    
    std::vector<DecorationItem> GetItemsByCategory(DecorationCategory category);
    std::vector<DecorationItem> SearchItems(const std::string& query);
    
    // [SEQUENCE: 3353] Filtering
    struct FilterCriteria {
        std::optional<DecorationCategory> category;
        std::optional<std::vector<PlacementRule>> required_rules;
        std::optional<float> max_size;
        std::optional<bool> interactive_only;
        std::optional<bool> light_emitting_only;
    };
    
    std::vector<DecorationItem> FilterItems(const FilterCriteria& criteria);
    
private:
    DecorationCatalog() = default;
    
    std::unordered_map<uint32_t, DecorationItem> items_;
    std::unordered_map<DecorationCategory, std::vector<uint32_t>> items_by_category_;
};

// [SEQUENCE: 3354] Decoration manager for houses
class HouseDecorationManager {
public:
    HouseDecorationManager(PlayerHouse& house);
    
    // [SEQUENCE: 3355] Decoration placement
    std::shared_ptr<PlacedDecoration> PlaceDecoration(
        uint32_t item_id,
        uint32_t room_id,
        const Vector3& position,
        const Quaternion& rotation = {0, 0, 0, 1});
    
    bool RemoveDecoration(uint64_t instance_id);
    bool MoveDecoration(uint64_t instance_id, const Vector3& new_position);
    bool RotateDecoration(uint64_t instance_id, const Quaternion& rotation);
    
    // [SEQUENCE: 3356] Decoration queries
    std::shared_ptr<PlacedDecoration> GetDecoration(uint64_t instance_id);
    std::vector<std::shared_ptr<PlacedDecoration>> GetRoomDecorations(uint32_t room_id);
    std::vector<std::shared_ptr<PlacedDecoration>> GetAllDecorations();
    
    // [SEQUENCE: 3357] Bulk operations
    void ClearRoom(uint32_t room_id);
    void ClearAllDecorations();
    bool SaveLayout(const std::string& layout_name);
    bool LoadLayout(const std::string& layout_name);
    
    // [SEQUENCE: 3358] Statistics
    struct DecorationStats {
        uint32_t total_decorations;
        std::unordered_map<DecorationCategory, uint32_t> by_category;
        uint32_t interactive_count;
        uint32_t light_sources;
        float total_value;
    };
    
    DecorationStats GetStatistics() const;
    
private:
    PlayerHouse& house_;
    std::unordered_map<uint64_t, std::shared_ptr<PlacedDecoration>> decorations_;
    std::unordered_map<uint32_t, std::vector<uint64_t>> decorations_by_room_;
    
    std::atomic<uint64_t> next_instance_id_{1};
    
    // Saved layouts
    std::unordered_map<std::string, std::vector<PlacedDecoration>> saved_layouts_;
};

// [SEQUENCE: 3359] Seasonal decoration manager
class SeasonalDecorationManager {
public:
    enum class Season {
        SPRING,
        SUMMER,
        AUTUMN,
        WINTER,
        SPECIAL_EVENT
    };
    
    // [SEQUENCE: 3360] Seasonal items
    static std::vector<DecorationItem> GetSeasonalItems(Season season);
    static void EnableSeasonalItems(Season season);
    static void DisableSeasonalItems(Season previous_season);
    
    // Auto-decoration for seasons
    static void AutoDecorateForSeason(PlayerHouse& house, Season season);
};

// [SEQUENCE: 3361] Decoration interaction system
class DecorationInteractionHandler {
public:
    using InteractionCallback = std::function<void(Player&, PlacedDecoration&)>;
    
    // [SEQUENCE: 3362] Register interactions
    static void RegisterInteraction(uint32_t item_id, 
                                  const std::string& action_name,
                                  InteractionCallback callback);
    
    static std::vector<std::string> GetAvailableInteractions(uint32_t item_id);
    static bool ExecuteInteraction(Player& player,
                                 PlacedDecoration& decoration,
                                 const std::string& action_name);
    
private:
    static std::unordered_map<uint32_t, 
        std::unordered_map<std::string, InteractionCallback>> interactions_;
};

// [SEQUENCE: 3363] Decoration utility functions
namespace DecorationUtils {
    // Validation helpers
    bool IsValidPosition(const Vector3& position, const BoundingBox& room_bounds);
    bool HasSufficientSpace(const BoundingBox& item_bounds,
                          const Vector3& position,
                          const std::vector<PlacedDecoration*>& nearby);
    
    // Placement helpers
    Vector3 SnapToGrid(const Vector3& position, float grid_size = 0.1f);
    Vector3 SnapToSurface(const Vector3& position, const HouseRoom& room);
    Quaternion AlignToWall(const Vector3& position, const HouseRoom& room);
    
    // Visual helpers
    Color BlendColors(const Color& a, const Color& b, float t);
    float CalculateLightAttenuation(float distance, float radius);
    
    // Value calculation
    uint64_t CalculateDecorationValue(const DecorationItem& item,
                                     float condition = 1.0f);
}

} // namespace mmorpg::housing