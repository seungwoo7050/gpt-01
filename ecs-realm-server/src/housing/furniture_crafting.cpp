#include "furniture_crafting.h"
#include "../core/logger.h"
#include "../player/player.h"
#include <spdlog/spdlog.h>
#include <algorithm>
#include <random>

namespace mmorpg::housing {

// [SEQUENCE: MVP14-80] Furniture crafting station constructor
FurnitureCraftingStation::FurnitureCraftingStation(uint64_t station_id, 
                                                 const StationProperties& props)
    : station_id_(station_id)
    , properties_(props)
    , current_tier_(props.tier) {
}

// [SEQUENCE: MVP14-81] Check if station can craft recipe
bool FurnitureCraftingStation::CanCraftRecipe(const FurnitureRecipe& recipe) const {
    // Check category support
    auto it = std::find(properties_.supported_categories.begin(),
                       properties_.supported_categories.end(),
                       recipe.category);
    
    if (it == properties_.supported_categories.end()) {
        return false;
    }
    
    // Check if station meets recipe requirements
    if (recipe.requires_workshop && 
        properties_.type < StationType::MAGICAL_WORKSHOP) {
        return false;
    }
    
    // Check material quality support
    for (const auto& mat : recipe.materials) {
        if (mat.quality_min > properties_.max_material_quality) {
            return false;
        }
    }
    
    return true;
}

// [SEQUENCE: MVP14-82] Get crafting time modifier
float FurnitureCraftingStation::GetCraftingTimeModifier() const {
    return properties_.crafting_speed_bonus;
}

// [SEQUENCE: MVP14-83] Can upgrade station
bool FurnitureCraftingStation::CanUpgrade() const {
    return current_tier_ < 5;  // Max tier is 5
}

// [SEQUENCE: MVP14-84] Get upgrade requirements
std::vector<std::pair<uint32_t, uint32_t>> 
FurnitureCraftingStation::GetUpgradeRequirements() const {
    std::vector<std::pair<uint32_t, uint32_t>> requirements;
    
    // Base requirements scale with tier
    uint32_t base_cost = 1000 * current_tier_ * current_tier_;
    
    // Wood and metal for basic upgrades
    requirements.push_back({1001, 50 * current_tier_});  // Wood
    requirements.push_back({1002, 30 * current_tier_});  // Metal
    
    // Special materials for higher tiers
    if (current_tier_ >= 3) {
        requirements.push_back({1003, 10 * (current_tier_ - 2)});  // Crystal
    }
    
    return requirements;
}

// [SEQUENCE: MVP14-85] Upgrade station
void FurnitureCraftingStation::Upgrade() {
    if (CanUpgrade()) {
        current_tier_++;
        properties_.tier = current_tier_;
        
        // Improve bonuses
        properties_.crafting_speed_bonus *= 1.2f;
        properties_.success_rate_bonus += 0.05f;
        properties_.quality_chance_bonus += 0.1f;
        properties_.max_material_quality++;
        
        spdlog::info("[FURNITURE_CRAFTING] Station {} upgraded to tier {}",
                    station_id_, current_tier_);
    }
}

// [SEQUENCE: MVP14-86] Crafting session constructor
FurnitureCraftingSession::FurnitureCraftingSession(uint64_t session_id,
                                                 uint64_t player_id,
                                                 const FurnitureRecipe& recipe,
                                                 FurnitureCraftingStation* station)
    : session_id_(session_id)
    , player_id_(player_id)
    , recipe_(recipe)
    , station_(station) {
}

// [SEQUENCE: MVP14-87] Start crafting session
void FurnitureCraftingSession::Start() {
    if (state_ != SessionState::PREPARING) {
        return;
    }
    
    // Check materials
    if (!CheckMaterials()) {
        state_ = SessionState::FAILED;
        spdlog::warn("[FURNITURE_CRAFTING] Session {} failed: insufficient materials",
                    session_id_);
        return;
    }
    
    // Consume materials
    ConsumeMaterials();
    
    state_ = SessionState::CRAFTING;
    start_time_ = std::chrono::steady_clock::now();
    
    spdlog::info("[FURNITURE_CRAFTING] Session {} started for recipe {}",
                session_id_, recipe_.name);
}

// [SEQUENCE: MVP14-88] Update crafting session
void FurnitureCraftingSession::Update(float delta_time) {
    if (state_ != SessionState::CRAFTING && state_ != SessionState::FINISHING) {
        return;
    }
    
    // Calculate total crafting time
    float time_modifier = station_ ? station_->GetCraftingTimeModifier() : 1.0f;
    float total_time = recipe_.base_crafting_time_seconds / time_modifier;
    
    // Update progress
    auto elapsed = std::chrono::steady_clock::now() - start_time_;
    float elapsed_seconds = std::chrono::duration<float>(elapsed).count();
    
    progress_ = std::min(1.0f, elapsed_seconds / total_time);
    
    // Check completion
    if (progress_ >= 1.0f && state_ == SessionState::CRAFTING) {
        state_ = SessionState::FINISHING;
        
        // Roll for success
        if (RollSuccess()) {
            // Create result
            result_ = std::make_shared<CraftedFurnitureProperties>();
            result_->base_item_id = recipe_.result_item_id;
            result_->crafter_id = player_id_;
            result_->crafted_date = std::chrono::system_clock::now();
            result_->quality = DetermineQuality();
            
            // Apply quality modifiers
            switch (result_->quality) {
                case CraftedFurnitureProperties::Quality::POOR:
                    result_->durability_modifier = 0.7f;
                    result_->value_modifier = 0.5f;
                    break;
                case CraftedFurnitureProperties::Quality::GOOD:
                    result_->durability_modifier = 1.2f;
                    result_->value_modifier = 1.5f;
                    break;
                case CraftedFurnitureProperties::Quality::EXCELLENT:
                    result_->durability_modifier = 1.5f;
                    result_->value_modifier = 2.0f;
                    result_->bonus_functionality = 1;
                    break;
                case CraftedFurnitureProperties::Quality::MASTERWORK:
                    result_->durability_modifier = 2.0f;
                    result_->value_modifier = 5.0f;
                    result_->bonus_functionality = 2;
                    result_->special_effects.push_back("masterwork_glow");
                    break;
                case CraftedFurnitureProperties::Quality::LEGENDARY:
                    result_->durability_modifier = 3.0f;
                    result_->value_modifier = 10.0f;
                    result_->bonus_functionality = 3;
                    result_->special_effects.push_back("legendary_aura");
                    result_->special_effects.push_back("unique_appearance");
                    break;
                default:
                    break;
            }
            
            state_ = SessionState::COMPLETED;
            spdlog::info("[FURNITURE_CRAFTING] Session {} completed with {} quality",
                        session_id_, static_cast<int>(result_->quality));
        } else {
            state_ = SessionState::FAILED;
            spdlog::info("[FURNITURE_CRAFTING] Session {} failed", session_id_);
        }
    }
}

// [SEQUENCE: MVP14-89] Cancel crafting session
void FurnitureCraftingSession::Cancel() {
    if (state_ == SessionState::CRAFTING || state_ == SessionState::PREPARING) {
        state_ = SessionState::CANCELLED;
        // Could return partial materials here
        spdlog::info("[FURNITURE_CRAFTING] Session {} cancelled", session_id_);
    }
}

// [SEQUENCE: MVP14-90] Get crafting progress
float FurnitureCraftingSession::GetProgress() const {
    return progress_;
}

// [SEQUENCE: MVP14-91] Get remaining time
std::chrono::seconds FurnitureCraftingSession::GetRemainingTime() const {
    if (state_ != SessionState::CRAFTING) {
        return std::chrono::seconds(0);
    }
    
    float time_modifier = station_ ? station_->GetCraftingTimeModifier() : 1.0f;
    float total_time = recipe_.base_crafting_time_seconds / time_modifier;
    
    auto elapsed = std::chrono::steady_clock::now() - start_time_;
    float elapsed_seconds = std::chrono::duration<float>(elapsed).count();
    
    float remaining = std::max(0.0f, total_time - elapsed_seconds);
    return std::chrono::seconds(static_cast<int>(remaining));
}

// [SEQUENCE: MVP14-92] Determine crafted quality
CraftedFurnitureProperties::Quality FurnitureCraftingSession::DetermineQuality() const {
    // Get player skill (placeholder)
    uint32_t skill_level = 50;  // Would get from player
    
    // Calculate quality chances
    float station_bonus = station_ ? station_->GetQualityBonus() : 0.0f;
    float skill_bonus = skill_level * recipe_.quality_skill_modifier;
    float total_bonus = station_bonus + skill_bonus;
    
    // Roll for quality
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);
    float roll = dis(gen);
    
    // Determine quality based on roll + bonuses
    float chance = roll + total_bonus;
    
    if (chance >= 0.99f) {
        return CraftedFurnitureProperties::Quality::LEGENDARY;
    } else if (chance >= 0.95f) {
        return CraftedFurnitureProperties::Quality::MASTERWORK;
    } else if (chance >= 0.85f) {
        return CraftedFurnitureProperties::Quality::EXCELLENT;
    } else if (chance >= 0.65f) {
        return CraftedFurnitureProperties::Quality::GOOD;
    } else if (chance >= 0.30f) {
        return CraftedFurnitureProperties::Quality::NORMAL;
    } else {
        return CraftedFurnitureProperties::Quality::POOR;
    }
}

// [SEQUENCE: MVP14-93] Check materials availability
bool FurnitureCraftingSession::CheckMaterials() {
    // In real implementation, would check player inventory
    return true;
}

// [SEQUENCE: MVP14-94] Consume crafting materials
void FurnitureCraftingSession::ConsumeMaterials() {
    // In real implementation, would remove from player inventory
    spdlog::debug("[FURNITURE_CRAFTING] Consumed materials for recipe {}",
                 recipe_.name);
}

// [SEQUENCE: MVP14-95] Roll for crafting success
bool FurnitureCraftingSession::RollSuccess() {
    // Get player skill (placeholder)
    uint32_t skill_level = 50;
    
    // Calculate success rate
    float base_rate = recipe_.success_rate_base;
    float skill_modifier = FurnitureCraftingSkill::GetSuccessRateModifier(
        skill_level, recipe_.required_skill_level);
    float station_bonus = station_ ? station_->GetSuccessRateBonus() : 0.0f;
    
    float total_rate = std::min(1.0f, base_rate * skill_modifier + station_bonus);
    
    // Roll
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);
    
    return dis(gen) < total_rate;
}

// [SEQUENCE: MVP14-96] Get required experience for level
uint32_t FurnitureCraftingSkill::GetRequiredExperience(uint32_t level) {
    // Exponential growth
    return 100 * level * level;
}

// [SEQUENCE: MVP14-97] Get success rate modifier
float FurnitureCraftingSkill::GetSuccessRateModifier(uint32_t skill_level,
                                                    uint32_t recipe_level) {
    int32_t level_diff = static_cast<int32_t>(skill_level) - recipe_level;
    
    if (level_diff >= 20) {
        return 1.5f;  // Master crafting easy recipe
    } else if (level_diff >= 10) {
        return 1.2f;
    } else if (level_diff >= 0) {
        return 1.0f;
    } else if (level_diff >= -10) {
        return 0.8f;
    } else if (level_diff >= -20) {
        return 0.5f;
    } else {
        return 0.2f;  // Very difficult
    }
}

// [SEQUENCE: MVP14-98] Get quality chance
float FurnitureCraftingSkill::GetQualityChance(uint32_t skill_level,
                                              uint32_t mastery_level) {
    float skill_bonus = skill_level * 0.005f;    // 0.5% per level
    float mastery_bonus = mastery_level * 0.01f; // 1% per mastery level
    
    return skill_bonus + mastery_bonus;
}

// [SEQUENCE: MVP14-99] Can learn recipe
bool FurnitureCraftingSkill::CanLearnRecipe(const SkillData& skill,
                                           const FurnitureRecipe& recipe) {
    // Check skill level requirement
    if (skill.level < recipe.required_skill_level) {
        return false;
    }
    
    // Check if already known
    auto it = std::find(skill.known_recipes.begin(),
                       skill.known_recipes.end(),
                       recipe.recipe_id);
    
    return it == skill.known_recipes.end();
}

// [SEQUENCE: MVP14-100] Learn recipe
void FurnitureCraftingSkill::LearnRecipe(SkillData& skill, uint32_t recipe_id) {
    skill.known_recipes.push_back(recipe_id);
    spdlog::info("[FURNITURE_CRAFTING] Learned recipe {}", recipe_id);
}

// [SEQUENCE: MVP14-101] Gain crafting experience
void FurnitureCraftingSkill::GainExperience(SkillData& skill, uint32_t amount,
                                           FurnitureCraftingCategory category) {
    skill.experience += amount;
    
    // Check for level up
    while (skill.experience >= GetRequiredExperience(skill.level + 1)) {
        skill.experience -= GetRequiredExperience(skill.level + 1);
        skill.level++;
        
        spdlog::info("[FURNITURE_CRAFTING] Skill leveled up to {}", skill.level);
    }
    
    // Increase category mastery
    skill.category_mastery[category]++;
}

// [SEQUENCE: MVP14-102] Register furniture recipe
void FurnitureRecipeManager::RegisterRecipe(const FurnitureRecipe& recipe) {
    recipes_[recipe.recipe_id] = recipe;
    recipes_by_category_[recipe.category].push_back(recipe.recipe_id);
    
    spdlog::debug("[FURNITURE_CRAFTING] Registered recipe: {}", recipe.name);
}

// [SEQUENCE: MVP14-103] Get recipe by ID
FurnitureRecipe* FurnitureRecipeManager::GetRecipe(uint32_t recipe_id) {
    auto it = recipes_.find(recipe_id);
    return it != recipes_.end() ? &it->second : nullptr;
}

// [SEQUENCE: MVP14-104] Get recipes by category
std::vector<FurnitureRecipe> FurnitureRecipeManager::GetRecipesByCategory(
    FurnitureCraftingCategory category) {
    
    std::vector<FurnitureRecipe> result;
    
    auto it = recipes_by_category_.find(category);
    if (it != recipes_by_category_.end()) {
        for (uint32_t recipe_id : it->second) {
            if (auto* recipe = GetRecipe(recipe_id)) {
                result.push_back(*recipe);
            }
        }
    }
    
    return result;
}

// [SEQUENCE: MVP14-105] Get available recipes for skill
std::vector<FurnitureRecipe> FurnitureRecipeManager::GetAvailableRecipes(
    const FurnitureCraftingSkill::SkillData& skill) {
    
    std::vector<FurnitureRecipe> result;
    
    for (uint32_t recipe_id : skill.known_recipes) {
        if (auto* recipe = GetRecipe(recipe_id)) {
            result.push_back(*recipe);
        }
    }
    
    return result;
}

// [SEQUENCE: MVP14-106] Start furniture crafting
std::shared_ptr<FurnitureCraftingSession> FurnitureCraftingManager::StartCrafting(
    uint64_t player_id,
    uint32_t recipe_id,
    FurnitureCraftingStation* station) {
    
    // Get recipe
    auto* recipe = FurnitureRecipeManager::Instance().GetRecipe(recipe_id);
    if (!recipe) {
        spdlog::warn("[FURNITURE_CRAFTING] Recipe {} not found", recipe_id);
        return nullptr;
    }
    
    // Check station compatibility
    if (station && !station->CanCraftRecipe(*recipe)) {
        spdlog::warn("[FURNITURE_CRAFTING] Station cannot craft recipe {}", 
                    recipe->name);
        return nullptr;
    }
    
    // Create session
    uint64_t session_id = next_session_id_++;
    auto session = std::make_shared<FurnitureCraftingSession>(
        session_id, player_id, *recipe, station);
    
    // Store session
    active_sessions_[session_id] = session;
    player_sessions_[player_id].push_back(session_id);
    
    // Start crafting
    session->Start();
    
    return session;
}

// [SEQUENCE: MVP14-107] Update all crafting sessions
void FurnitureCraftingManager::UpdateSessions(float delta_time) {
    std::vector<uint64_t> completed_sessions;
    
    for (auto& [session_id, session] : active_sessions_) {
        session->Update(delta_time);
        
        if (session->GetState() == FurnitureCraftingSession::SessionState::COMPLETED ||
            session->GetState() == FurnitureCraftingSession::SessionState::FAILED ||
            session->GetState() == FurnitureCraftingSession::SessionState::CANCELLED) {
            
            completed_sessions.push_back(session_id);
            
            // Update statistics
            if (session->GetState() == FurnitureCraftingSession::SessionState::COMPLETED) {
                global_stats_.total_items_crafted++;
                
                if (auto result = session->GetResult()) {
                    global_stats_.items_by_quality[static_cast<int>(result->quality)]++;
                }
            }
        }
    }
    
    // Remove completed sessions
    for (uint64_t session_id : completed_sessions) {
        auto it = active_sessions_.find(session_id);
        if (it != active_sessions_.end()) {
            uint64_t player_id = it->second->GetPlayerId();
            
            // Remove from player sessions
            auto& player_list = player_sessions_[player_id];
            player_list.erase(
                std::remove(player_list.begin(), player_list.end(), session_id),
                player_list.end()
            );
            
            active_sessions_.erase(it);
        }
    }
}

// [SEQUENCE: MVP14-108] Apply furniture effect
void SpecialFurnitureEffects::ApplyFurnitureEffect(Player& player,
                                                  const FurnitureEffect& effect) {
    switch (effect.type) {
        case EffectType::RESTING_BONUS:
            // Increase HP/MP regeneration
            player.AddBuff("furniture_resting", effect.magnitude, effect.duration);
            break;
            
        case EffectType::CRAFTING_SPEED:
            // Boost crafting speed
            player.AddBuff("furniture_crafting_speed", effect.magnitude, effect.duration);
            break;
            
        case EffectType::EXPERIENCE_BOOST:
            // Increase experience gain
            player.AddBuff("furniture_exp_boost", effect.magnitude, effect.duration);
            break;
            
        default:
            break;
    }
    
    spdlog::debug("[FURNITURE_CRAFTING] Applied furniture effect {} to player {}",
                 static_cast<int>(effect.type), player.GetId());
}

// [SEQUENCE: MVP14-109] Get effects for quality
std::vector<SpecialFurnitureEffects::FurnitureEffect> 
SpecialFurnitureEffects::GetEffectsForQuality(CraftedFurnitureProperties::Quality quality) {
    
    std::vector<FurnitureEffect> effects;
    
    switch (quality) {
        case CraftedFurnitureProperties::Quality::EXCELLENT:
            effects.push_back({
                .type = EffectType::COMFORT_ZONE,
                .magnitude = 0.1f,
                .radius = 5.0f
            });
            break;
            
        case CraftedFurnitureProperties::Quality::MASTERWORK:
            effects.push_back({
                .type = EffectType::RESTING_BONUS,
                .magnitude = 0.2f,
                .radius = 5.0f
            });
            effects.push_back({
                .type = EffectType::AMBIENT_LIGHTING,
                .magnitude = 1.0f,
                .radius = 10.0f
            });
            break;
            
        case CraftedFurnitureProperties::Quality::LEGENDARY:
            effects.push_back({
                .type = EffectType::EXPERIENCE_BOOST,
                .magnitude = 0.1f,
                .radius = 10.0f
            });
            effects.push_back({
                .type = EffectType::MAGICAL_AURA,
                .magnitude = 1.0f,
                .radius = 15.0f
            });
            effects.push_back({
                .type = EffectType::CRAFTING_SPEED,
                .magnitude = 0.3f,
                .radius = 8.0f
            });
            break;
            
        default:
            break;
    }
    
    return effects;
}

// [SEQUENCE: MVP14-110] Attempt recipe discovery
bool RecipeDiscoverySystem::AttemptDiscovery(uint64_t player_id,
                                           DiscoveryMethod method,
                                           const std::vector<FurnitureMaterial>& materials) {
    // Get possible discoveries
    auto possible = GetPossibleDiscoveries(50, materials);  // Placeholder skill level
    
    if (possible.empty()) {
        return false;
    }
    
    // Roll for discovery
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);
    
    float discovery_chance = 0.1f;  // Base 10% chance
    
    // Method modifiers
    switch (method) {
        case DiscoveryMethod::EXPERIMENTATION:
            discovery_chance *= 1.5f;
            break;
        case DiscoveryMethod::INSPIRATION:
            discovery_chance *= 2.0f;
            break;
        case DiscoveryMethod::MASTER_TEACHING:
            discovery_chance = 1.0f;  // Guaranteed
            break;
        default:
            break;
    }
    
    if (dis(gen) < discovery_chance) {
        // Select random recipe from possible
        std::uniform_int_distribution<> recipe_dis(0, possible.size() - 1);
        uint32_t recipe_id = possible[recipe_dis(gen)];
        
        UnlockRecipe(player_id, recipe_id, method);
        return true;
    }
    
    return false;
}

// [SEQUENCE: MVP14-111] Get possible discoveries
std::vector<uint32_t> RecipeDiscoverySystem::GetPossibleDiscoveries(
    uint32_t skill_level,
    const std::vector<FurnitureMaterial>& materials) {
    
    std::vector<uint32_t> possible;
    
    // Check all recipes
    auto& manager = FurnitureRecipeManager::Instance();
    
    // In real implementation, would check which recipes match materials
    // and are discoverable at this skill level
    
    return possible;
}

// [SEQUENCE: MVP14-112] Unlock discovered recipe
void RecipeDiscoverySystem::UnlockRecipe(uint64_t player_id, uint32_t recipe_id,
                                       DiscoveryMethod method) {
    // In real implementation, would add to player's known recipes
    
    spdlog::info("[FURNITURE_CRAFTING] Player {} discovered recipe {} via {}",
                player_id, recipe_id, static_cast<int>(method));
}

// [SEQUENCE: MVP14-113] Furniture crafting utilities
namespace FurnitureCraftingUtils {

uint64_t CalculateMaterialValue(FurnitureMaterial material, 
                              uint32_t quality,
                              uint32_t quantity) {
    // Base values by material
    uint64_t base_value = 10;
    
    switch (material) {
        case FurnitureMaterial::WOOD:
            base_value = 10;
            break;
        case FurnitureMaterial::METAL:
            base_value = 25;
            break;
        case FurnitureMaterial::CRYSTAL:
            base_value = 100;
            break;
        case FurnitureMaterial::MAGICAL_ESSENCE:
            base_value = 500;
            break;
        default:
            break;
    }
    
    // Quality multiplier
    float quality_mult = 1.0f + (quality - 1) * 0.5f;
    
    return static_cast<uint64_t>(base_value * quality_mult * quantity);
}

std::chrono::seconds CalculateCraftingTime(const FurnitureRecipe& recipe,
                                          uint32_t skill_level,
                                          float station_bonus) {
    float base_time = recipe.base_crafting_time_seconds;
    
    // Skill reduces time (up to 50% reduction)
    float skill_reduction = std::min(0.5f, skill_level * 0.005f);
    base_time *= (1.0f - skill_reduction);
    
    // Apply station bonus
    base_time /= station_bonus;
    
    return std::chrono::seconds(static_cast<int>(base_time));
}

CraftedFurnitureProperties::Quality RollQuality(uint32_t skill_level,
                                              uint32_t recipe_difficulty,
                                              float bonus_chance) {
    // Calculate chances
    float skill_bonus = FurnitureCraftingSkill::GetQualityChance(skill_level, 0);
    float total_bonus = skill_bonus + bonus_chance;
    
    // Difficulty penalty
    if (recipe_difficulty > skill_level) {
        total_bonus *= 0.5f;
    }
    
    // Roll
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, 1.0);
    float roll = dis(gen) + total_bonus;
    
    // Determine quality
    if (roll >= 0.99f) {
        return CraftedFurnitureProperties::Quality::LEGENDARY;
    } else if (roll >= 0.95f) {
        return CraftedFurnitureProperties::Quality::MASTERWORK;
    } else if (roll >= 0.85f) {
        return CraftedFurnitureProperties::Quality::EXCELLENT;
    } else if (roll >= 0.65f) {
        return CraftedFurnitureProperties::Quality::GOOD;
    } else if (roll >= 0.30f) {
        return CraftedFurnitureProperties::Quality::NORMAL;
    } else {
        return CraftedFurnitureProperties::Quality::POOR;
    }
}

} // namespace FurnitureCraftingUtils

} // namespace mmorpg::housing
