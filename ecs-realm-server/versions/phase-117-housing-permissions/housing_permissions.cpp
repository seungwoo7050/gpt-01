#include "housing_permissions.h"
#include "../core/logger.h"
#include "../player/player.h"
#include <spdlog/spdlog.h>
#include <algorithm>

namespace mmorpg::housing {

// [SEQUENCE: 3526] Grant permission to player
void HouseAccessControl::GrantPermission(uint64_t player_id,
                                       const PermissionSet& permissions) {
    // Owner cannot have permissions changed
    if (player_id == owner_id_) {
        spdlog::warn("[HOUSING_PERMISSIONS] Cannot change owner permissions");
        return;
    }
    
    // Check if player is banned
    if (IsBanned(player_id)) {
        spdlog::warn("[HOUSING_PERMISSIONS] Cannot grant permissions to banned player {}",
                    player_id);
        return;
    }
    
    permissions_[player_id] = permissions;
    
    spdlog::info("[HOUSING_PERMISSIONS] Granted level {} permissions to player {} for house {}",
                static_cast<int>(permissions.level), player_id, house_id_);
}

// [SEQUENCE: 3527] Revoke player permissions
void HouseAccessControl::RevokePermission(uint64_t player_id) {
    if (player_id == owner_id_) {
        spdlog::warn("[HOUSING_PERMISSIONS] Cannot revoke owner permissions");
        return;
    }
    
    auto it = permissions_.find(player_id);
    if (it != permissions_.end()) {
        permissions_.erase(it);
        spdlog::info("[HOUSING_PERMISSIONS] Revoked permissions for player {} from house {}",
                    player_id, house_id_);
    }
}

// [SEQUENCE: 3528] Update existing permissions
void HouseAccessControl::UpdatePermission(uint64_t player_id,
                                        const PermissionSet& permissions) {
    auto it = permissions_.find(player_id);
    if (it != permissions_.end()) {
        it->second = permissions;
        spdlog::debug("[HOUSING_PERMISSIONS] Updated permissions for player {} in house {}",
                     player_id, house_id_);
    } else {
        GrantPermission(player_id, permissions);
    }
}

// [SEQUENCE: 3529] Check if player has access
bool HouseAccessControl::HasAccess(uint64_t player_id) const {
    // Owner always has access
    if (player_id == owner_id_) {
        return true;
    }
    
    // Check ban list
    if (IsBanned(player_id)) {
        return false;
    }
    
    // Check permissions
    auto it = permissions_.find(player_id);
    if (it != permissions_.end()) {
        // Check time restrictions
        if (it->second.has_time_restriction &&
            !ValidateTimeRestriction(it->second)) {
            return false;
        }
        return true;
    }
    
    // Check guest list
    return IsGuest(player_id);
}

// [SEQUENCE: 3530] Check if player can perform action
bool HouseAccessControl::CanPerformAction(uint64_t player_id,
                                        PermissionFlag action) const {
    // Owner can do everything
    if (player_id == owner_id_) {
        return true;
    }
    
    if (!HasAccess(player_id)) {
        return false;
    }
    
    auto it = permissions_.find(player_id);
    if (it != permissions_.end()) {
        return it->second.HasFlag(action);
    }
    
    // Guests have minimal permissions
    if (IsGuest(player_id)) {
        return action == PermissionFlag::ENTER_HOUSE ||
               action == PermissionFlag::USE_FURNITURE;
    }
    
    return false;
}

// [SEQUENCE: 3531] Get player permissions
PermissionSet* HouseAccessControl::GetPermissions(uint64_t player_id) {
    auto it = permissions_.find(player_id);
    return it != permissions_.end() ? &it->second : nullptr;
}

// [SEQUENCE: 3532] Grant permissions to group
void HouseAccessControl::GrantGroupPermission(const std::vector<uint64_t>& player_ids,
                                            const PermissionSet& permissions) {
    for (uint64_t player_id : player_ids) {
        GrantPermission(player_id, permissions);
    }
    
    spdlog::info("[HOUSING_PERMISSIONS] Granted permissions to {} players for house {}",
                player_ids.size(), house_id_);
}

// [SEQUENCE: 3533] Revoke all permissions
void HouseAccessControl::RevokeAllPermissions() {
    size_t count = permissions_.size();
    permissions_.clear();
    guests_.clear();
    
    spdlog::info("[HOUSING_PERMISSIONS] Revoked all {} permissions for house {}",
                count, house_id_);
}

// [SEQUENCE: 3534] Get all permissions
std::vector<std::pair<uint64_t, PermissionSet>> 
HouseAccessControl::GetAllPermissions() const {
    std::vector<std::pair<uint64_t, PermissionSet>> result;
    result.reserve(permissions_.size());
    
    for (const auto& [player_id, perms] : permissions_) {
        result.push_back({player_id, perms});
    }
    
    return result;
}

// [SEQUENCE: 3535] Get players with specific level
std::vector<uint64_t> HouseAccessControl::GetPlayersWithLevel(
    HousingPermissionLevel level) const {
    
    std::vector<uint64_t> result;
    
    for (const auto& [player_id, perms] : permissions_) {
        if (perms.level == level) {
            result.push_back(player_id);
        }
    }
    
    return result;
}

// [SEQUENCE: 3536] Ban player from house
void HouseAccessControl::BanPlayer(uint64_t player_id, const std::string& reason) {
    if (player_id == owner_id_) {
        spdlog::warn("[HOUSING_PERMISSIONS] Cannot ban house owner");
        return;
    }
    
    banned_players_.insert(player_id);
    ban_details_[player_id] = {
        .reason = reason,
        .ban_date = std::chrono::system_clock::now()
    };
    
    // Remove any existing permissions
    RevokePermission(player_id);
    RemoveGuest(player_id);
    
    spdlog::info("[HOUSING_PERMISSIONS] Banned player {} from house {}: {}",
                player_id, house_id_, reason);
}

// [SEQUENCE: 3537] Check if player is banned
bool HouseAccessControl::IsBanned(uint64_t player_id) const {
    return banned_players_.find(player_id) != banned_players_.end();
}

// [SEQUENCE: 3538] Add temporary guest
void HouseAccessControl::AddGuest(uint64_t player_id, std::chrono::hours duration) {
    if (IsBanned(player_id)) {
        spdlog::warn("[HOUSING_PERMISSIONS] Cannot add banned player as guest");
        return;
    }
    
    guests_[player_id] = {
        .expiry = std::chrono::system_clock::now() + duration
    };
    
    spdlog::debug("[HOUSING_PERMISSIONS] Added guest {} to house {} for {} hours",
                 player_id, house_id_, duration.count());
}

// [SEQUENCE: 3539] Check if player is guest
bool HouseAccessControl::IsGuest(uint64_t player_id) const {
    auto it = guests_.find(player_id);
    if (it != guests_.end()) {
        return std::chrono::system_clock::now() < it->second.expiry;
    }
    return false;
}

// [SEQUENCE: 3540] Cleanup expired guests
void HouseAccessControl::CleanupExpiredGuests() {
    auto now = std::chrono::system_clock::now();
    
    for (auto it = guests_.begin(); it != guests_.end(); ) {
        if (now >= it->second.expiry) {
            it = guests_.erase(it);
        } else {
            ++it;
        }
    }
}

// [SEQUENCE: 3541] Validate time restrictions
bool HouseAccessControl::ValidateTimeRestriction(const PermissionSet& perms) const {
    if (!perms.has_time_restriction) {
        return true;
    }
    
    auto now = std::chrono::system_clock::now();
    return now >= perms.access_start && now <= perms.access_end;
}

// [SEQUENCE: 3542] Get visitor permission template
PermissionSet PermissionTemplates::GetVisitorTemplate() {
    PermissionSet perms;
    perms.level = HousingPermissionLevel::VISITOR;
    perms.SetFlag(PermissionFlag::ENTER_HOUSE, true);
    perms.SetFlag(PermissionFlag::USE_FURNITURE, true);
    return perms;
}

// [SEQUENCE: 3543] Get friend permission template
PermissionSet PermissionTemplates::GetFriendTemplate() {
    PermissionSet perms = GetVisitorTemplate();
    perms.level = HousingPermissionLevel::FRIEND;
    perms.SetFlag(PermissionFlag::ACCESS_STORAGE, true);
    perms.SetFlag(PermissionFlag::USE_CRAFTING_STATIONS, true);
    perms.SetFlag(PermissionFlag::HARVEST_GARDEN, true);
    perms.SetFlag(PermissionFlag::FEED_PETS, true);
    return perms;
}

// [SEQUENCE: 3544] Get roommate permission template
PermissionSet PermissionTemplates::GetRoommateTemplate() {
    PermissionSet perms = GetFriendTemplate();
    perms.level = HousingPermissionLevel::ROOMMATE;
    perms.SetFlag(PermissionFlag::PLACE_DECORATION, true);
    perms.SetFlag(PermissionFlag::REMOVE_DECORATION, true);
    perms.SetFlag(PermissionFlag::ACCESS_PRIVATE_ROOMS, true);
    perms.SetFlag(PermissionFlag::INVITE_GUESTS, true);
    perms.SetFlag(PermissionFlag::COLLECT_MAIL, true);
    return perms;
}

// [SEQUENCE: 3545] Create permission group
uint32_t PermissionGroups::CreateGroup(const std::string& name, uint64_t creator_id) {
    uint32_t group_id = next_group_id_++;
    
    groups_[group_id] = {
        .name = name,
        .members = {},
        .permissions = PermissionSet(),
        .created_by = creator_id,
        .created_date = std::chrono::system_clock::now()
    };
    
    spdlog::info("[HOUSING_PERMISSIONS] Created permission group '{}' with ID {}",
                name, group_id);
    
    return group_id;
}

// [SEQUENCE: 3546] Add member to group
void PermissionGroups::AddMember(uint32_t group_id, uint64_t player_id) {
    auto it = groups_.find(group_id);
    if (it == groups_.end()) {
        return;
    }
    
    auto& members = it->second.members;
    if (std::find(members.begin(), members.end(), player_id) == members.end()) {
        members.push_back(player_id);
        
        spdlog::debug("[HOUSING_PERMISSIONS] Added player {} to group {}",
                     player_id, group_id);
    }
}

// [SEQUENCE: 3547] Update sharing settings
void HouseSharingSystem::UpdateSharingSettings(uint64_t house_id,
                                              const SharingSettings& settings) {
    sharing_settings_[house_id] = settings;
    
    spdlog::info("[HOUSING_PERMISSIONS] Updated sharing settings for house {} to {}",
                house_id, static_cast<int>(settings.type));
}

// [SEQUENCE: 3548] Record house visit
void HouseSharingSystem::RecordVisit(uint64_t house_id, uint64_t visitor_id) {
    visit_history_[house_id].push_back({
        .visitor_id = visitor_id,
        .visit_time = std::chrono::system_clock::now()
    });
    
    // Keep only recent visits (last 1000)
    auto& visits = visit_history_[house_id];
    if (visits.size() > 1000) {
        visits.erase(visits.begin(), visits.begin() + (visits.size() - 1000));
    }
}

// [SEQUENCE: 3549] Get visitor count
uint32_t HouseSharingSystem::GetVisitorCount(uint64_t house_id,
                                            std::chrono::hours period) const {
    auto it = visit_history_.find(house_id);
    if (it == visit_history_.end()) {
        return 0;
    }
    
    auto cutoff = std::chrono::system_clock::now() - period;
    uint32_t count = 0;
    
    for (const auto& visit : it->second) {
        if (visit.visit_time >= cutoff) {
            count++;
        }
    }
    
    return count;
}

// [SEQUENCE: 3550] Rate house
void HouseSharingSystem::RateHouse(uint64_t house_id, uint64_t rater_id,
                                  uint8_t rating) {
    if (rating > 5) {
        rating = 5;
    }
    
    ratings_[house_id][rater_id] = rating;
    
    // Update average rating
    auto& settings = sharing_settings_[house_id];
    if (settings.allow_ratings) {
        uint32_t total = 0;
        for (const auto& [id, rate] : ratings_[house_id]) {
            total += rate;
        }
        
        settings.total_ratings = ratings_[house_id].size();
        settings.average_rating = static_cast<float>(total) / settings.total_ratings;
    }
    
    spdlog::debug("[HOUSING_PERMISSIONS] Player {} rated house {} with {} stars",
                 rater_id, house_id, rating);
}

// [SEQUENCE: 3551] Get house access control
HouseAccessControl* HousingPermissionManager::GetHouseAccess(uint64_t house_id) {
    auto it = house_access_.find(house_id);
    return it != house_access_.end() ? it->second.get() : nullptr;
}

// [SEQUENCE: 3552] Create house access control
void HousingPermissionManager::CreateHouseAccess(uint64_t house_id, uint64_t owner_id) {
    auto access = std::make_unique<HouseAccessControl>();
    access->house_id_ = house_id;
    access->owner_id_ = owner_id;
    
    house_access_[house_id] = std::move(access);
    
    spdlog::info("[HOUSING_PERMISSIONS] Created access control for house {} owned by {}",
                house_id, owner_id);
}

// [SEQUENCE: 3553] Validate access
bool HousingPermissionManager::ValidateAccess(uint64_t house_id, uint64_t player_id,
                                            PermissionFlag required_action) {
    auto* access = GetHouseAccess(house_id);
    if (!access) {
        return false;
    }
    
    return access->CanPerformAction(player_id, required_action);
}

// [SEQUENCE: 3554] Apply template to house
void HousingPermissionManager::ApplyTemplateToHouse(uint64_t house_id,
                                                  const std::string& template_name,
                                                  const std::vector<uint64_t>& player_ids) {
    auto* access = GetHouseAccess(house_id);
    if (!access) {
        return;
    }
    
    auto* tmpl = templates_.GetTemplate(template_name);
    if (!tmpl) {
        return;
    }
    
    for (uint64_t player_id : player_ids) {
        access->GrantPermission(player_id, tmpl->permissions);
    }
    
    spdlog::info("[HOUSING_PERMISSIONS] Applied template '{}' to {} players in house {}",
                template_name, player_ids.size(), house_id);
}

// [SEQUENCE: 3555] Lockdown house
void HousingPermissionManager::LockdownHouse(uint64_t house_id) {
    auto* access = GetHouseAccess(house_id);
    if (!access) {
        return;
    }
    
    // Cache current permissions
    lockdown_cache_[house_id] = access->GetAllPermissions();
    
    // Revoke all permissions
    access->RevokeAllPermissions();
    
    spdlog::warn("[HOUSING_PERMISSIONS] House {} is now in lockdown mode", house_id);
}

// [SEQUENCE: 3556] Permission utility functions
namespace PermissionUtils {

bool IsHigherLevel(HousingPermissionLevel a, HousingPermissionLevel b) {
    return static_cast<int>(a) > static_cast<int>(b);
}

HousingPermissionLevel GetMinimumLevelForFlag(PermissionFlag flag) {
    switch (flag) {
        case PermissionFlag::ENTER_HOUSE:
        case PermissionFlag::USE_FURNITURE:
            return HousingPermissionLevel::VISITOR;
            
        case PermissionFlag::ACCESS_STORAGE:
        case PermissionFlag::USE_CRAFTING_STATIONS:
        case PermissionFlag::HARVEST_GARDEN:
        case PermissionFlag::FEED_PETS:
            return HousingPermissionLevel::FRIEND;
            
        case PermissionFlag::PLACE_DECORATION:
        case PermissionFlag::REMOVE_DECORATION:
            return HousingPermissionLevel::DECORATOR;
            
        case PermissionFlag::ACCESS_PRIVATE_ROOMS:
        case PermissionFlag::INVITE_GUESTS:
        case PermissionFlag::COLLECT_MAIL:
            return HousingPermissionLevel::ROOMMATE;
            
        case PermissionFlag::MODIFY_ROOM:
        case PermissionFlag::MANAGE_PERMISSIONS:
        case PermissionFlag::PAY_RENT:
            return HousingPermissionLevel::MANAGER;
            
        case PermissionFlag::SELL_HOUSE:
            return HousingPermissionLevel::CO_OWNER;
            
        default:
            return HousingPermissionLevel::OWNER;
    }
}

uint32_t GetDefaultFlags(HousingPermissionLevel level) {
    uint32_t flags = 0;
    
    // Accumulate permissions for each level
    if (level >= HousingPermissionLevel::VISITOR) {
        flags |= static_cast<uint32_t>(PermissionFlag::ENTER_HOUSE);
        flags |= static_cast<uint32_t>(PermissionFlag::USE_FURNITURE);
    }
    
    if (level >= HousingPermissionLevel::FRIEND) {
        flags |= static_cast<uint32_t>(PermissionFlag::ACCESS_STORAGE);
        flags |= static_cast<uint32_t>(PermissionFlag::USE_CRAFTING_STATIONS);
        flags |= static_cast<uint32_t>(PermissionFlag::HARVEST_GARDEN);
        flags |= static_cast<uint32_t>(PermissionFlag::FEED_PETS);
    }
    
    if (level >= HousingPermissionLevel::DECORATOR) {
        flags |= static_cast<uint32_t>(PermissionFlag::PLACE_DECORATION);
        flags |= static_cast<uint32_t>(PermissionFlag::REMOVE_DECORATION);
    }
    
    if (level >= HousingPermissionLevel::ROOMMATE) {
        flags |= static_cast<uint32_t>(PermissionFlag::ACCESS_PRIVATE_ROOMS);
        flags |= static_cast<uint32_t>(PermissionFlag::INVITE_GUESTS);
        flags |= static_cast<uint32_t>(PermissionFlag::COLLECT_MAIL);
    }
    
    if (level >= HousingPermissionLevel::MANAGER) {
        flags |= static_cast<uint32_t>(PermissionFlag::MODIFY_ROOM);
        flags |= static_cast<uint32_t>(PermissionFlag::MANAGE_PERMISSIONS);
        flags |= static_cast<uint32_t>(PermissionFlag::PAY_RENT);
    }
    
    if (level >= HousingPermissionLevel::CO_OWNER) {
        flags |= static_cast<uint32_t>(PermissionFlag::SELL_HOUSE);
    }
    
    return flags;
}

std::string PermissionLevelToString(HousingPermissionLevel level) {
    switch (level) {
        case HousingPermissionLevel::NO_ACCESS: return "No Access";
        case HousingPermissionLevel::VISITOR: return "Visitor";
        case HousingPermissionLevel::FRIEND: return "Friend";
        case HousingPermissionLevel::DECORATOR: return "Decorator";
        case HousingPermissionLevel::ROOMMATE: return "Roommate";
        case HousingPermissionLevel::MANAGER: return "Manager";
        case HousingPermissionLevel::CO_OWNER: return "Co-Owner";
        case HousingPermissionLevel::OWNER: return "Owner";
        default: return "Unknown";
    }
}

bool ValidatePermissionSet(const PermissionSet& perms) {
    // Check if flags match minimum level requirements
    for (uint32_t i = 0; i < 15; ++i) {
        PermissionFlag flag = static_cast<PermissionFlag>(1 << i);
        if (perms.HasFlag(flag)) {
            auto min_level = GetMinimumLevelForFlag(flag);
            if (perms.level < min_level) {
                return false;
            }
        }
    }
    
    // Validate time restrictions
    if (perms.has_time_restriction) {
        if (perms.access_start >= perms.access_end) {
            return false;
        }
    }
    
    return true;
}

} // namespace PermissionUtils

} // namespace mmorpg::housing