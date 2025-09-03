#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <unordered_map>
#include "decoration_system.h"
#include "../crafting/crafting_system.h"
#include "../core/types.h"

namespace mmorpg::housing {

// [SEQUENCE: 3397] Furniture crafting categories
enum class FurnitureCraftingCategory {
    BASIC_FURNITURE,    // 기본 가구
    ADVANCED_FURNITURE, // 고급 가구
    DECORATIVE_ITEMS,   // 장식품
    LIGHTING_FIXTURES,  // 조명 기구
    STORAGE_FURNITURE,  // 수납 가구
    GARDEN_FURNITURE,   // 정원 가구
    SPECIAL_FURNITURE   // 특수 가구
};

// [SEQUENCE: 3398] Crafting material types
enum class FurnitureMaterial {
    WOOD,           // 목재
    METAL,          // 금속
    FABRIC,         // 직물
    STONE,          // 석재
    GLASS,          // 유리
    CRYSTAL,        // 수정
    LEATHER,        // 가죽
    SPECIAL_ALLOY,  // 특수 합금
    MAGICAL_ESSENCE // 마법 정수
};

// [SEQUENCE: 3399] Furniture recipe definition
struct FurnitureRecipe {
    uint32_t recipe_id;
    std::string name;
    uint32_t result_item_id;  // Decoration item ID
    
    FurnitureCraftingCategory category;
    uint32_t required_skill_level;
    
    // Required materials
    struct MaterialRequirement {
        FurnitureMaterial material;
        uint32_t quantity;
        uint32_t quality_min{0};  // Minimum material quality
    };
    std::vector<MaterialRequirement> materials;
    
    // Required items (components)
    std::vector<std::pair<uint32_t, uint32_t>> required_items;  // item_id, quantity
    
    // Required tools
    std::vector<uint32_t> required_tools;
    
    // Crafting properties
    uint32_t base_crafting_time_seconds{60};
    float success_rate_base{1.0f};
    uint32_t experience_reward{10};
    
    // Quality modifiers
    bool can_produce_high_quality{true};
    float quality_skill_modifier{0.01f};  // Per skill level
    
    // Special requirements
    bool requires_workshop{false};
    std::string special_requirement;  // e.g., "Near forge", "During full moon"
};

// [SEQUENCE: 3400] Crafted furniture properties
struct CraftedFurnitureProperties {
    uint32_t base_item_id;
    
    // Quality affects appearance and value
    enum class Quality {
        POOR,           // 조악함
        NORMAL,         // 보통
        GOOD,           // 좋음
        EXCELLENT,      // 훌륭함
        MASTERWORK,     // 걸작
        LEGENDARY       // 전설
    } quality{Quality::NORMAL};
    
    // Crafted by info
    uint64_t crafter_id;
    std::string crafter_name;
    std::chrono::system_clock::time_point crafted_date;
    
    // Custom properties
    std::string custom_name;        // Player-given name
    std::string custom_description;
    
    // Bonuses based on quality
    float durability_modifier{1.0f};
    float value_modifier{1.0f};
    uint32_t bonus_functionality{0};  // Extra storage slots, etc.
    
    // Special effects (for high quality)
    std::vector<std::string> special_effects;
};

// [SEQUENCE: 3401] Furniture crafting station
class FurnitureCraftingStation {
public:
    enum class StationType {
        BASIC_WORKBENCH,    // 기본 작업대
        CARPENTRY_BENCH,    // 목공 작업대
        FORGE,              // 대장간
        LOOM,               // 베틀
        JEWELERS_BENCH,     // 보석 세공대
        MAGICAL_WORKSHOP,   // 마법 작업장
        MASTER_WORKSHOP     // 마스터 작업장
    };
    
    // [SEQUENCE: 3402] Station properties
    struct StationProperties {
        StationType type;
        std::string name;
        uint32_t tier{1};  // 1-5
        
        // Capabilities
        std::vector<FurnitureCraftingCategory> supported_categories;
        uint32_t max_material_quality{3};
        
        // Bonuses
        float crafting_speed_bonus{1.0f};
        float success_rate_bonus{0.0f};
        float quality_chance_bonus{0.0f};
        
        // Requirements
        uint32_t required_house_tier{1};
        bool requires_power_source{false};
    };
    
    FurnitureCraftingStation(uint64_t station_id, const StationProperties& props);
    
    // [SEQUENCE: 3403] Station usage
    bool CanCraftRecipe(const FurnitureRecipe& recipe) const;
    float GetCraftingTimeModifier() const;
    float GetSuccessRateBonus() const;
    float GetQualityBonus() const;
    
    // Upgrade system
    bool CanUpgrade() const;
    std::vector<std::pair<uint32_t, uint32_t>> GetUpgradeRequirements() const;
    void Upgrade();
    
private:
    uint64_t station_id_;
    StationProperties properties_;
    uint32_t current_tier_{1};
};

// [SEQUENCE: 3404] Furniture crafting session
class FurnitureCraftingSession {
public:
    enum class SessionState {
        PREPARING,      // 준비 중
        CRAFTING,       // 제작 중
        FINISHING,      // 마무리 중
        COMPLETED,      // 완료
        FAILED,         // 실패
        CANCELLED       // 취소됨
    };
    
    // [SEQUENCE: 3405] Session management
    FurnitureCraftingSession(uint64_t session_id, 
                           uint64_t player_id,
                           const FurnitureRecipe& recipe,
                           FurnitureCraftingStation* station);
    
    void Start();
    void Update(float delta_time);
    void Cancel();
    
    // Progress tracking
    float GetProgress() const;
    std::chrono::seconds GetRemainingTime() const;
    SessionState GetState() const { return state_; }
    
    // Quality determination
    CraftedFurnitureProperties::Quality DetermineQuality() const;
    
    // Result
    std::shared_ptr<CraftedFurnitureProperties> GetResult() const { return result_; }
    
private:
    uint64_t session_id_;
    uint64_t player_id_;
    FurnitureRecipe recipe_;
    FurnitureCraftingStation* station_;
    
    SessionState state_{SessionState::PREPARING};
    float progress_{0.0f};
    std::chrono::steady_clock::time_point start_time_;
    
    std::shared_ptr<CraftedFurnitureProperties> result_;
    
    // Helper methods
    bool CheckMaterials();
    void ConsumeMaterials();
    bool RollSuccess();
};

// [SEQUENCE: 3406] Furniture crafting skill
class FurnitureCraftingSkill {
public:
    struct SkillData {
        uint32_t level{1};
        uint32_t experience{0};
        
        // Specializations
        std::unordered_map<FurnitureCraftingCategory, uint32_t> category_mastery;
        
        // Unlocked recipes
        std::vector<uint32_t> known_recipes;
        
        // Statistics
        uint32_t items_crafted{0};
        uint32_t masterworks_created{0};
        uint32_t failures{0};
    };
    
    // [SEQUENCE: 3407] Skill progression
    static uint32_t GetRequiredExperience(uint32_t level);
    static float GetSuccessRateModifier(uint32_t skill_level, uint32_t recipe_level);
    static float GetQualityChance(uint32_t skill_level, uint32_t mastery_level);
    
    // Recipe learning
    static bool CanLearnRecipe(const SkillData& skill, const FurnitureRecipe& recipe);
    static void LearnRecipe(SkillData& skill, uint32_t recipe_id);
    
    // Experience gain
    static void GainExperience(SkillData& skill, uint32_t amount,
                             FurnitureCraftingCategory category);
};

// [SEQUENCE: 3408] Furniture recipe manager
class FurnitureRecipeManager {
public:
    static FurnitureRecipeManager& Instance() {
        static FurnitureRecipeManager instance;
        return instance;
    }
    
    // [SEQUENCE: 3409] Recipe management
    void RegisterRecipe(const FurnitureRecipe& recipe);
    FurnitureRecipe* GetRecipe(uint32_t recipe_id);
    
    std::vector<FurnitureRecipe> GetRecipesByCategory(
        FurnitureCraftingCategory category);
    
    std::vector<FurnitureRecipe> GetAvailableRecipes(
        const FurnitureCraftingSkill::SkillData& skill);
    
    // Discovery system
    std::vector<FurnitureRecipe> GetDiscoverableRecipes(
        uint32_t skill_level);
    
private:
    FurnitureRecipeManager() = default;
    
    std::unordered_map<uint32_t, FurnitureRecipe> recipes_;
    std::unordered_map<FurnitureCraftingCategory, std::vector<uint32_t>> recipes_by_category_;
};

// [SEQUENCE: 3410] Furniture crafting manager
class FurnitureCraftingManager {
public:
    static FurnitureCraftingManager& Instance() {
        static FurnitureCraftingManager instance;
        return instance;
    }
    
    // [SEQUENCE: 3411] Crafting operations
    std::shared_ptr<FurnitureCraftingSession> StartCrafting(
        uint64_t player_id,
        uint32_t recipe_id,
        FurnitureCraftingStation* station);
    
    void UpdateSessions(float delta_time);
    
    std::shared_ptr<FurnitureCraftingSession> GetSession(uint64_t session_id);
    std::vector<std::shared_ptr<FurnitureCraftingSession>> GetPlayerSessions(
        uint64_t player_id);
    
    // [SEQUENCE: 3412] Material management
    bool CheckMaterialAvailability(uint64_t player_id,
                                  const FurnitureRecipe& recipe);
    
    std::vector<FurnitureMaterial> GetRequiredMaterials(
        const FurnitureRecipe& recipe);
    
    // Statistics
    struct CraftingStats {
        uint32_t total_items_crafted;
        uint32_t items_by_quality[6];  // Poor to Legendary
        std::unordered_map<uint32_t, uint32_t> popular_recipes;
        uint64_t total_materials_used;
    };
    
    CraftingStats GetGlobalStats() const;
    
private:
    FurnitureCraftingManager() = default;
    
    std::unordered_map<uint64_t, std::shared_ptr<FurnitureCraftingSession>> active_sessions_;
    std::unordered_map<uint64_t, std::vector<uint64_t>> player_sessions_;  // player_id -> session_ids
    
    std::atomic<uint64_t> next_session_id_{1};
    
    CraftingStats global_stats_;
};

// [SEQUENCE: 3413] Material gathering integration
class FurnitureMaterialGathering {
public:
    struct GatheringNode {
        FurnitureMaterial material_type;
        Vector3 position;
        uint32_t quality_level{1};
        uint32_t remaining_resources{100};
        std::chrono::steady_clock::time_point respawn_time;
    };
    
    // [SEQUENCE: 3414] Gathering methods
    static bool CanGatherFrom(const GatheringNode& node, uint32_t skill_level);
    static uint32_t GatherMaterial(GatheringNode& node, uint32_t skill_level);
    
    // Material processing
    static bool CanProcessMaterial(FurnitureMaterial material, uint32_t quality);
    static uint32_t ProcessMaterial(FurnitureMaterial material, 
                                  uint32_t raw_quality,
                                  uint32_t processing_skill);
};

// [SEQUENCE: 3415] Special furniture effects
class SpecialFurnitureEffects {
public:
    // Effect types
    enum class EffectType {
        RESTING_BONUS,      // 휴식 보너스
        CRAFTING_SPEED,     // 제작 속도 증가
        STORAGE_EXPANSION,  // 저장 공간 확장
        AMBIENT_LIGHTING,   // 주변 조명
        MAGICAL_AURA,       // 마법 오라
        COMFORT_ZONE,       // 편안함 지역
        EXPERIENCE_BOOST    // 경험치 부스트
    };
    
    struct FurnitureEffect {
        EffectType type;
        float magnitude;
        float radius{0.0f};  // 0 for personal effects
        std::chrono::seconds duration{0};  // 0 for permanent
        
        // Conditions
        bool requires_interaction{false};
        std::string activation_condition;
    };
    
    // [SEQUENCE: 3416] Apply effects
    static void ApplyFurnitureEffect(Player& player,
                                   const FurnitureEffect& effect);
    
    static std::vector<FurnitureEffect> GetEffectsForQuality(
        CraftedFurnitureProperties::Quality quality);
};

// [SEQUENCE: 3417] Furniture customization
class FurnitureCustomization {
public:
    struct CustomizationOption {
        std::string name;
        enum Type {
            COLOR_TINT,
            MATERIAL_FINISH,
            ENGRAVING,
            INLAY,
            ENCHANTMENT
        } type;
        
        std::vector<std::pair<uint32_t, uint32_t>> required_materials;
        uint32_t required_skill_level;
        uint64_t cost;
    };
    
    // [SEQUENCE: 3418] Customization methods
    static bool CanCustomize(const CraftedFurnitureProperties& furniture,
                           const CustomizationOption& option);
    
    static void ApplyCustomization(CraftedFurnitureProperties& furniture,
                                 const CustomizationOption& option,
                                 const std::string& custom_value);
    
    static std::vector<CustomizationOption> GetAvailableOptions(
        uint32_t item_id,
        uint32_t crafter_skill);
};

// [SEQUENCE: 3419] Recipe discovery system
class RecipeDiscoverySystem {
public:
    enum class DiscoveryMethod {
        EXPERIMENTATION,    // 실험
        INSPIRATION,        // 영감
        BLUEPRINT_STUDY,    // 설계도 연구
        MASTER_TEACHING,    // 마스터 교육
        ACHIEVEMENT_REWARD  // 업적 보상
    };
    
    // [SEQUENCE: 3420] Discovery mechanics
    static bool AttemptDiscovery(uint64_t player_id,
                               DiscoveryMethod method,
                               const std::vector<FurnitureMaterial>& materials);
    
    static std::vector<uint32_t> GetPossibleDiscoveries(
        uint32_t skill_level,
        const std::vector<FurnitureMaterial>& materials);
    
    static void UnlockRecipe(uint64_t player_id, uint32_t recipe_id,
                           DiscoveryMethod method);
};

// [SEQUENCE: 3421] Furniture crafting utilities
namespace FurnitureCraftingUtils {
    // Material value calculations
    uint64_t CalculateMaterialValue(FurnitureMaterial material, 
                                  uint32_t quality,
                                  uint32_t quantity);
    
    // Time calculations
    std::chrono::seconds CalculateCraftingTime(const FurnitureRecipe& recipe,
                                              uint32_t skill_level,
                                              float station_bonus);
    
    // Quality determination
    CraftedFurnitureProperties::Quality RollQuality(uint32_t skill_level,
                                                  uint32_t recipe_difficulty,
                                                  float bonus_chance);
    
    // Recipe validation
    bool ValidateRecipeRequirements(const FurnitureRecipe& recipe,
                                   const Player& player,
                                   const FurnitureCraftingStation* station);
}

} // namespace mmorpg::housing