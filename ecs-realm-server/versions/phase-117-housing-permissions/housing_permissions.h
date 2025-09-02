#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <unordered_map>
#include <set>
#include "player_housing.h"
#include "../core/types.h"

namespace mmorpg::housing {

// [SEQUENCE: 3510] Permission levels
enum class HousingPermissionLevel {
    NO_ACCESS = 0,      // 접근 불가
    VISITOR = 1,        // 방문자
    FRIEND = 2,         // 친구
    DECORATOR = 3,      // 장식가
    ROOMMATE = 4,       // 룸메이트
    MANAGER = 5,        // 관리자
    CO_OWNER = 6,       // 공동 소유자
    OWNER = 7           // 소유자
};

// [SEQUENCE: 3511] Permission flags
enum class PermissionFlag : uint32_t {
    ENTER_HOUSE = 1 << 0,           // 집 진입
    USE_FURNITURE = 1 << 1,         // 가구 사용
    ACCESS_STORAGE = 1 << 2,        // 저장소 접근
    PLACE_DECORATION = 1 << 3,      // 장식 배치
    REMOVE_DECORATION = 1 << 4,     // 장식 제거
    MODIFY_ROOM = 1 << 5,           // 방 수정
    INVITE_GUESTS = 1 << 6,         // 손님 초대
    MANAGE_PERMISSIONS = 1 << 7,    // 권한 관리
    ACCESS_PRIVATE_ROOMS = 1 << 8,  // 개인 방 접근
    USE_CRAFTING_STATIONS = 1 << 9, // 제작대 사용
    HARVEST_GARDEN = 1 << 10,       // 정원 수확
    FEED_PETS = 1 << 11,            // 펫 먹이주기
    COLLECT_MAIL = 1 << 12,         // 우편 수집
    PAY_RENT = 1 << 13,             // 임대료 지불
    SELL_HOUSE = 1 << 14            // 집 판매
};

// [SEQUENCE: 3512] Permission set
struct PermissionSet {
    HousingPermissionLevel level;
    uint32_t flags{0};
    
    // Time restrictions
    bool has_time_restriction{false};
    std::chrono::system_clock::time_point access_start;
    std::chrono::system_clock::time_point access_end;
    
    // Room restrictions
    bool has_room_restriction{false};
    std::vector<uint32_t> allowed_rooms;
    
    // Custom notes
    std::string permission_note;
    
    // Helper methods
    bool HasFlag(PermissionFlag flag) const {
        return (flags & static_cast<uint32_t>(flag)) != 0;
    }
    
    void SetFlag(PermissionFlag flag, bool value) {
        if (value) {
            flags |= static_cast<uint32_t>(flag);
        } else {
            flags &= ~static_cast<uint32_t>(flag);
        }
    }
};

// [SEQUENCE: 3513] House access control
class HouseAccessControl {
public:
    // [SEQUENCE: 3514] Permission management
    void GrantPermission(uint64_t player_id, const PermissionSet& permissions);
    void RevokePermission(uint64_t player_id);
    void UpdatePermission(uint64_t player_id, const PermissionSet& permissions);
    
    // Permission queries
    bool HasAccess(uint64_t player_id) const;
    bool CanPerformAction(uint64_t player_id, PermissionFlag action) const;
    PermissionSet* GetPermissions(uint64_t player_id);
    
    // Bulk operations
    void GrantGroupPermission(const std::vector<uint64_t>& player_ids,
                            const PermissionSet& permissions);
    void RevokeAllPermissions();
    
    // [SEQUENCE: 3515] Access lists
    std::vector<std::pair<uint64_t, PermissionSet>> GetAllPermissions() const;
    std::vector<uint64_t> GetPlayersWithLevel(HousingPermissionLevel level) const;
    std::vector<uint64_t> GetPlayersWithFlag(PermissionFlag flag) const;
    
    // Ban list
    void BanPlayer(uint64_t player_id, const std::string& reason);
    void UnbanPlayer(uint64_t player_id);
    bool IsBanned(uint64_t player_id) const;
    
    // Guest management
    void AddGuest(uint64_t player_id, std::chrono::hours duration);
    void RemoveGuest(uint64_t player_id);
    bool IsGuest(uint64_t player_id) const;
    
private:
    uint64_t house_id_;
    uint64_t owner_id_;
    
    std::unordered_map<uint64_t, PermissionSet> permissions_;
    std::set<uint64_t> banned_players_;
    
    struct BanInfo {
        std::string reason;
        std::chrono::system_clock::time_point ban_date;
    };
    std::unordered_map<uint64_t, BanInfo> ban_details_;
    
    struct GuestInfo {
        std::chrono::system_clock::time_point expiry;
    };
    std::unordered_map<uint64_t, GuestInfo> guests_;
    
    // Helper methods
    void CleanupExpiredGuests();
    bool ValidateTimeRestriction(const PermissionSet& perms) const;
};

// [SEQUENCE: 3516] Permission templates
class PermissionTemplates {
public:
    // Predefined templates
    static PermissionSet GetVisitorTemplate();
    static PermissionSet GetFriendTemplate();
    static PermissionSet GetRoommateTemplate();
    static PermissionSet GetDecoratorTemplate();
    static PermissionSet GetManagerTemplate();
    
    // [SEQUENCE: 3517] Custom templates
    struct Template {
        std::string name;
        std::string description;
        PermissionSet permissions;
        bool is_public{false};
    };
    
    void CreateTemplate(const std::string& name, const Template& tmpl);
    void DeleteTemplate(const std::string& name);
    Template* GetTemplate(const std::string& name);
    
    std::vector<std::string> GetAvailableTemplates() const;
    
private:
    std::unordered_map<std::string, Template> custom_templates_;
};

// [SEQUENCE: 3518] Permission group system
class PermissionGroups {
public:
    struct Group {
        std::string name;
        std::vector<uint64_t> members;
        PermissionSet permissions;
        uint64_t created_by;
        std::chrono::system_clock::time_point created_date;
    };
    
    // [SEQUENCE: 3519] Group management
    uint32_t CreateGroup(const std::string& name, uint64_t creator_id);
    void DeleteGroup(uint32_t group_id);
    void RenameGroup(uint32_t group_id, const std::string& new_name);
    
    // Member management
    void AddMember(uint32_t group_id, uint64_t player_id);
    void RemoveMember(uint32_t group_id, uint64_t player_id);
    void SetGroupPermissions(uint32_t group_id, const PermissionSet& permissions);
    
    // Queries
    Group* GetGroup(uint32_t group_id);
    std::vector<uint32_t> GetPlayerGroups(uint64_t player_id) const;
    std::vector<Group> GetAllGroups() const;
    
private:
    std::unordered_map<uint32_t, Group> groups_;
    std::atomic<uint32_t> next_group_id_{1};
};

// [SEQUENCE: 3520] House sharing system
class HouseSharingSystem {
public:
    enum class SharingType {
        PRIVATE,        // 개인 전용
        FRIENDS_ONLY,   // 친구만
        GUILD_ONLY,     // 길드원만
        PUBLIC,         // 공개
        TICKETED       // 티켓 필요
    };
    
    struct SharingSettings {
        SharingType type{SharingType::PRIVATE};
        bool requires_approval{false};
        uint64_t visitor_fee{0};
        uint32_t max_visitors{10};
        
        // Showcase mode
        bool showcase_mode{false};
        std::string showcase_title;
        std::string showcase_description;
        std::vector<std::string> showcase_tags;
        
        // Rating system
        bool allow_ratings{true};
        float average_rating{0.0f};
        uint32_t total_ratings{0};
    };
    
    // [SEQUENCE: 3521] Sharing management
    void UpdateSharingSettings(uint64_t house_id, const SharingSettings& settings);
    SharingSettings GetSharingSettings(uint64_t house_id) const;
    
    // Visitor tracking
    void RecordVisit(uint64_t house_id, uint64_t visitor_id);
    uint32_t GetVisitorCount(uint64_t house_id, std::chrono::hours period) const;
    std::vector<uint64_t> GetRecentVisitors(uint64_t house_id, uint32_t count) const;
    
    // Rating system
    void RateHouse(uint64_t house_id, uint64_t rater_id, uint8_t rating);
    float GetAverageRating(uint64_t house_id) const;
    
    // Showcase features
    std::vector<uint64_t> SearchShowcases(const std::string& query) const;
    std::vector<uint64_t> GetTopRatedHouses(uint32_t count) const;
    std::vector<uint64_t> GetMostVisitedHouses(uint32_t count) const;
    
private:
    std::unordered_map<uint64_t, SharingSettings> sharing_settings_;
    
    struct VisitRecord {
        uint64_t visitor_id;
        std::chrono::system_clock::time_point visit_time;
    };
    std::unordered_map<uint64_t, std::vector<VisitRecord>> visit_history_;
    
    std::unordered_map<uint64_t, std::unordered_map<uint64_t, uint8_t>> ratings_;
};

// [SEQUENCE: 3522] Permission manager
class HousingPermissionManager {
public:
    static HousingPermissionManager& Instance() {
        static HousingPermissionManager instance;
        return instance;
    }
    
    // [SEQUENCE: 3523] House access control
    HouseAccessControl* GetHouseAccess(uint64_t house_id);
    void CreateHouseAccess(uint64_t house_id, uint64_t owner_id);
    void DeleteHouseAccess(uint64_t house_id);
    
    // Permission validation
    bool ValidateAccess(uint64_t house_id, uint64_t player_id,
                       PermissionFlag required_action);
    
    // Group system
    PermissionGroups& GetGroupSystem() { return group_system_; }
    
    // Sharing system
    HouseSharingSystem& GetSharingSystem() { return sharing_system_; }
    
    // Template system
    PermissionTemplates& GetTemplates() { return templates_; }
    
    // [SEQUENCE: 3524] Bulk operations
    void ApplyTemplateToHouse(uint64_t house_id, const std::string& template_name,
                            const std::vector<uint64_t>& player_ids);
    
    void ApplyGroupToHouse(uint64_t house_id, uint32_t group_id);
    
    // Security
    void LockdownHouse(uint64_t house_id);  // Revoke all permissions
    void UnlockHouse(uint64_t house_id);    // Restore previous permissions
    
    // Analytics
    struct AccessStats {
        uint32_t total_visitors;
        uint32_t unique_visitors;
        uint32_t permission_changes;
        std::unordered_map<HousingPermissionLevel, uint32_t> visitors_by_level;
    };
    
    AccessStats GetHouseStats(uint64_t house_id, std::chrono::hours period) const;
    
private:
    HousingPermissionManager() = default;
    
    std::unordered_map<uint64_t, std::unique_ptr<HouseAccessControl>> house_access_;
    PermissionGroups group_system_;
    HouseSharingSystem sharing_system_;
    PermissionTemplates templates_;
    
    // Lockdown cache
    std::unordered_map<uint64_t, std::vector<std::pair<uint64_t, PermissionSet>>> 
        lockdown_cache_;
};

// [SEQUENCE: 3525] Permission utilities
namespace PermissionUtils {
    // Permission level checks
    bool IsHigherLevel(HousingPermissionLevel a, HousingPermissionLevel b);
    HousingPermissionLevel GetMinimumLevelForFlag(PermissionFlag flag);
    
    // Permission merging
    PermissionSet MergePermissions(const PermissionSet& a, const PermissionSet& b);
    PermissionSet IntersectPermissions(const PermissionSet& a, const PermissionSet& b);
    
    // Default permissions by level
    uint32_t GetDefaultFlags(HousingPermissionLevel level);
    
    // Permission strings
    std::string PermissionLevelToString(HousingPermissionLevel level);
    std::string PermissionFlagToString(PermissionFlag flag);
    
    // Validation
    bool ValidatePermissionSet(const PermissionSet& perms);
}

} // namespace mmorpg::housing