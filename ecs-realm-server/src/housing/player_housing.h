#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <unordered_map>
#include "../core/types.h"
#include "../spatial/collision_detection.h"

namespace mmorpg::housing {

// [SEQUENCE: 3268] House types and tiers
enum class HouseType {
    ROOM,           // 단일 방
    SMALL_HOUSE,    // 작은 집
    MEDIUM_HOUSE,   // 중간 집
    LARGE_HOUSE,    // 큰 집
    MANSION,        // 맨션
    GUILD_HALL     // 길드 홀
};

enum class HouseTier {
    BASIC,          // 기본
    STANDARD,       // 표준
    DELUXE,         // 디럭스
    PREMIUM,        // 프리미엄
    LUXURY         // 럭셔리
};

// [SEQUENCE: 3269] House location and plot
struct HousePlot {
    uint64_t plot_id;
    std::string zone_name;          // "residential_district_1"
    Vector3 position;
    float plot_size;                // Square meters
    
    bool is_available{true};
    uint64_t price{0};
    
    // Neighborhood info
    uint32_t district_id;
    uint32_t ward_number;
    uint32_t plot_number;
};

// [SEQUENCE: 3270] House configuration
struct HouseConfig {
    HouseType type;
    HouseTier tier;
    
    // Size limits
    uint32_t max_furniture_count{100};
    uint32_t max_storage_slots{50};
    uint32_t max_garden_items{20};
    uint32_t max_vendors{2};
    
    // Room configuration
    uint32_t num_rooms{1};
    uint32_t num_floors{1};
    float total_area{100.0f};       // Square meters
    
    // Features
    bool has_garden{false};
    bool has_balcony{false};
    bool has_basement{false};
    bool has_workshop{false};
    
    // Permissions
    uint32_t max_co_owners{0};
    uint32_t max_tenants{0};
    uint32_t max_guests{10};
};

// [SEQUENCE: 3271] House room definition
struct HouseRoom {
    uint32_t room_id;
    std::string room_name;          // "Living Room", "Bedroom"
    
    // Spatial info
    BoundingBox bounds;
    uint32_t floor_number{1};
    
    // Room properties
    float lighting_level{1.0f};     // 0.0 - 1.0
    uint32_t ambient_sound_id{0};
    uint32_t wallpaper_id{0};
    uint32_t flooring_id{0};
    
    // Furniture placement
    std::vector<uint64_t> furniture_ids;
    uint32_t furniture_limit{20};
};

// [SEQUENCE: 3272] Player house instance
class PlayerHouse {
public:
    PlayerHouse(uint64_t house_id, uint64_t owner_id, const HouseConfig& config);
    ~PlayerHouse() = default;
    
    // [SEQUENCE: 3273] House lifecycle
    bool Initialize(const HousePlot& plot);
    void Update(float delta_time);
    void Save();
    void Load();
    
    // [SEQUENCE: 3274] Ownership management
    bool TransferOwnership(uint64_t new_owner_id);
    bool AddCoOwner(uint64_t player_id);
    bool RemoveCoOwner(uint64_t player_id);
    bool IsOwner(uint64_t player_id) const;
    bool HasAccess(uint64_t player_id) const;
    
    // [SEQUENCE: 3275] Room management
    HouseRoom* GetRoom(uint32_t room_id);
    std::vector<HouseRoom*> GetAllRooms();
    bool AddRoom(const HouseRoom& room);
    bool RemoveRoom(uint32_t room_id);
    
    // [SEQUENCE: 3276] Furniture placement
    bool PlaceFurniture(uint64_t furniture_id, uint32_t room_id, 
                       const Vector3& position, float rotation);
    bool MoveFurniture(uint64_t furniture_id, const Vector3& new_position);
    bool RotateFurniture(uint64_t furniture_id, float rotation);
    bool RemoveFurniture(uint64_t furniture_id);
    
    // [SEQUENCE: 3277] House customization
    bool ChangeWallpaper(uint32_t room_id, uint32_t wallpaper_id);
    bool ChangeFlooring(uint32_t room_id, uint32_t flooring_id);
    bool ChangeLighting(uint32_t room_id, float level);
    bool SetAmbientSound(uint32_t room_id, uint32_t sound_id);
    
    // [SEQUENCE: 3278] Utilities and maintenance
    bool PayRent();
    bool PayTaxes();
    bool RepairDamage(float damage_amount);
    void CalculateUpkeep();
    
    // [SEQUENCE: 3279] Getters
    uint64_t GetHouseId() const { return house_id_; }
    uint64_t GetOwnerId() const { return owner_id_; }
    HouseType GetType() const { return config_.type; }
    HouseTier GetTier() const { return config_.tier; }
    const HousePlot& GetPlot() const { return plot_; }
    
    // [SEQUENCE: 3280] House statistics
    uint32_t GetFurnitureCount() const;
    uint32_t GetVisitorCount() const { return current_visitors_.size(); }
    float GetCondition() const { return condition_; }
    uint64_t GetValue() const;
    
private:
    uint64_t house_id_;
    uint64_t owner_id_;
    HouseConfig config_;
    HousePlot plot_;
    
    // Rooms
    std::unordered_map<uint32_t, HouseRoom> rooms_;
    uint32_t next_room_id_{1};
    
    // Co-owners and permissions
    std::vector<uint64_t> co_owners_;
    std::vector<uint64_t> tenants_;
    std::unordered_map<uint64_t, uint32_t> access_levels_;  // player_id -> level
    
    // Current state
    std::vector<uint64_t> current_visitors_;
    float condition_{100.0f};       // 0-100, degrades over time
    
    // Financial
    uint64_t monthly_rent_{0};
    uint64_t property_tax_{0};
    std::chrono::system_clock::time_point last_payment_;
    
    // Timestamps
    std::chrono::system_clock::time_point created_at_;
    std::chrono::system_clock::time_point last_visited_;
    std::chrono::system_clock::time_point last_modified_;
};

// [SEQUENCE: 3281] Housing system manager
class HousingSystem {
public:
    static HousingSystem& Instance() {
        static HousingSystem instance;
        return instance;
    }
    
    // [SEQUENCE: 3282] House creation and deletion
    std::shared_ptr<PlayerHouse> CreateHouse(uint64_t owner_id, 
                                            const HouseConfig& config,
                                            const HousePlot& plot);
    
    bool DeleteHouse(uint64_t house_id);
    bool AbandonHouse(uint64_t house_id);
    
    // [SEQUENCE: 3283] House access
    std::shared_ptr<PlayerHouse> GetHouse(uint64_t house_id);
    std::shared_ptr<PlayerHouse> GetHouseByOwner(uint64_t owner_id);
    std::vector<std::shared_ptr<PlayerHouse>> GetHousesInZone(const std::string& zone);
    
    // [SEQUENCE: 3284] Plot management
    std::vector<HousePlot> GetAvailablePlots(const std::string& zone = "");
    bool PurchasePlot(uint64_t player_id, uint64_t plot_id);
    bool ReleasePlot(uint64_t plot_id);
    
    // [SEQUENCE: 3285] House visiting
    bool EnterHouse(uint64_t player_id, uint64_t house_id);
    bool ExitHouse(uint64_t player_id, uint64_t house_id);
    std::vector<uint64_t> GetVisitors(uint64_t house_id);
    
    // [SEQUENCE: 3286] House search
    struct HouseSearchCriteria {
        std::optional<HouseType> type;
        std::optional<HouseTier> tier;
        std::optional<std::string> zone;
        std::optional<uint64_t> max_price;
        std::optional<uint32_t> min_rooms;
        bool available_only{true};
    };
    
    std::vector<std::shared_ptr<PlayerHouse>> SearchHouses(
        const HouseSearchCriteria& criteria);
    
    // [SEQUENCE: 3287] System management
    void Update(float delta_time);
    void ProcessMonthlyPayments();
    void CalculatePropertyValues();
    
    // [SEQUENCE: 3288] Statistics
    struct HousingStats {
        uint32_t total_houses;
        uint32_t occupied_plots;
        uint32_t available_plots;
        std::unordered_map<HouseType, uint32_t> houses_by_type;
        std::unordered_map<std::string, uint32_t> houses_by_zone;
        uint64_t total_property_value;
        uint64_t monthly_tax_revenue;
    };
    
    HousingStats GetStatistics() const;
    
private:
    HousingSystem() = default;
    
    // Houses
    std::unordered_map<uint64_t, std::shared_ptr<PlayerHouse>> houses_;
    std::unordered_map<uint64_t, uint64_t> owner_to_house_;  // owner_id -> house_id
    
    // Plots
    std::unordered_map<uint64_t, HousePlot> all_plots_;
    std::unordered_map<uint64_t, uint64_t> plot_to_house_;   // plot_id -> house_id
    
    // ID generation
    std::atomic<uint64_t> next_house_id_{1};
    std::atomic<uint64_t> next_plot_id_{1};
    
    mutable std::mutex houses_mutex_;
    mutable std::mutex plots_mutex_;
};

// [SEQUENCE: 3289] House templates
class HouseTemplate {
public:
    struct TemplateData {
        std::string name;
        HouseType type;
        HouseTier tier;
        HouseConfig config;
        std::vector<HouseRoom> default_rooms;
        uint64_t base_price;
        uint32_t required_level;
    };
    
    static TemplateData GetTemplate(const std::string& template_name);
    static std::vector<std::string> GetAvailableTemplates();
    
private:
    static std::unordered_map<std::string, TemplateData> templates_;
};

// [SEQUENCE: 3290] House upgrade system
class HouseUpgradeSystem {
public:
    struct UpgradeOption {
        std::string name;
        std::string description;
        
        // Requirements
        uint64_t cost;
        std::vector<std::pair<uint32_t, uint32_t>> required_items;  // item_id, count
        uint32_t required_level;
        
        // Effects
        std::function<void(PlayerHouse&)> apply_upgrade;
    };
    
    // [SEQUENCE: 3291] Upgrade management
    static std::vector<UpgradeOption> GetAvailableUpgrades(const PlayerHouse& house);
    static bool ApplyUpgrade(PlayerHouse& house, const std::string& upgrade_name);
    
    // [SEQUENCE: 3292] Tier upgrades
    static bool UpgradeTier(PlayerHouse& house);
    static uint64_t GetTierUpgradeCost(HouseTier current_tier);
    
    // [SEQUENCE: 3293] Room expansions
    static bool AddRoomExpansion(PlayerHouse& house, const std::string& room_type);
    static bool AddFloor(PlayerHouse& house);
    static bool AddBasement(PlayerHouse& house);
};

// [SEQUENCE: 3294] House visitor management
class HouseVisitorManager {
public:
    // [SEQUENCE: 3295] Visitor tracking
    struct VisitorInfo {
        uint64_t player_id;
        std::string player_name;
        std::chrono::system_clock::time_point entry_time;
        std::chrono::seconds duration;
        uint32_t visit_count;
    };
    
    static void RecordVisit(uint64_t house_id, uint64_t visitor_id);
    static std::vector<VisitorInfo> GetRecentVisitors(uint64_t house_id, 
                                                     std::chrono::hours duration);
    static uint32_t GetTotalVisitCount(uint64_t house_id);
    
private:
    static std::unordered_map<uint64_t, std::vector<VisitorInfo>> visitor_history_;
};

// [SEQUENCE: 3296] House utility functions
namespace HouseUtils {
    // Configuration helpers
    HouseConfig CreateDefaultConfig(HouseType type, HouseTier tier);
    uint32_t CalculateMaxFurniture(HouseType type, HouseTier tier);
    uint64_t CalculateBasePrice(HouseType type, HouseTier tier);
    
    // Validation
    bool ValidateFurniturePlacement(const HouseRoom& room, 
                                   const Vector3& position,
                                   const BoundingBox& furniture_bounds);
    
    bool ValidateRoomExpansion(const PlayerHouse& house,
                              const HouseRoom& new_room);
    
    // Cost calculations
    uint64_t CalculateMonthlyRent(const PlayerHouse& house);
    uint64_t CalculatePropertyTax(const PlayerHouse& house);
    uint64_t CalculateUpkeepCost(const PlayerHouse& house);
}

} // namespace mmorpg::housing