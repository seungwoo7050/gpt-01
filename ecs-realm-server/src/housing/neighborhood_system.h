#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <unordered_map>
#include <set>
#include "player_housing.h"
#include "../world/world_types.h"
#include "../core/types.h"

namespace mmorpg::housing {

// [SEQUENCE: MVP14-26] Neighborhood types
enum class NeighborhoodType {
    RESIDENTIAL,    // 주거 지역
    COMMERCIAL,     // 상업 지역
    ARTISAN,        // 장인 지역
    NOBLE,          // 귀족 지역
    WATERFRONT,     // 해안 지역
    MOUNTAIN,       // 산악 지역
    MAGICAL,        // 마법 지역
    GUILD_DISTRICT  // 길드 지역
};

// [SEQUENCE: MVP14-27] Neighborhood features
struct NeighborhoodFeatures {
    bool has_market{false};         // 시장
    bool has_crafting_hub{false};   // 제작 허브
    bool has_guild_hall{false};     // 길드 홀
    bool has_park{false};           // 공원
    bool has_fountain{false};       // 분수대
    bool has_teleporter{false};     // 텔레포터
    bool has_bank{false};           // 은행
    bool has_mailbox{true};         // 우편함
    
    // Special features
    bool has_ocean_view{false};     // 바다 전망
    bool has_mountain_view{false};  // 산 전망
    bool has_special_npcs{false};   // 특수 NPC
    bool has_seasonal_events{false}; // 계절 이벤트
};

// [SEQUENCE: MVP14-28] Neighborhood layout
struct NeighborhoodLayout {
    struct Plot {
        uint32_t plot_id;
        Vector3 position;
        Vector3 dimensions;
        float rotation;
        
        HouseType allowed_type;
        bool is_occupied{false};
        uint64_t house_id{0};
        uint64_t owner_id{0};
        
        // Plot features
        bool has_garden_space{true};
        bool has_water_access{false};
        bool is_corner_lot{false};
        uint32_t max_floors{2};
    };
    
    std::vector<Plot> plots;
    
    // Roads and paths
    struct Road {
        std::vector<Vector3> path_points;
        float width{5.0f};
        bool is_main_road{true};
    };
    std::vector<Road> roads;
    
    // Common areas
    struct CommonArea {
        std::string name;
        BoundingBox bounds;
        std::string area_type;  // "park", "market", "plaza"
    };
    std::vector<CommonArea> common_areas;
};

// [SEQUENCE: MVP14-29] Neighborhood instance
class Neighborhood {
public:
    // [SEQUENCE: MVP14-30] Core properties
    struct Properties {
        uint32_t neighborhood_id;
        std::string name;
        NeighborhoodType type;
        uint32_t max_houses{50};
        uint32_t current_houses{0};
        
        // Location
        Vector3 center_position;
        float radius{500.0f};
        uint32_t world_zone_id;
        
        // Features
        NeighborhoodFeatures features;
        
        // Economics
        float property_tax_rate{0.05f};
        uint64_t average_property_value{0};
        uint32_t desirability_score{50};  // 0-100
    };
    
    Neighborhood(const Properties& props);
    
    // [SEQUENCE: MVP14-31] Plot management
    Plot* GetAvailablePlot(HouseType type) const;
    bool ReservePlot(uint32_t plot_id, uint64_t player_id);
    bool ReleasePlot(uint32_t plot_id);
    
    std::vector<Plot*> GetAvailablePlots() const;
    Plot* GetPlotByHouseId(uint64_t house_id);
    
    // Neighbor queries
    std::vector<uint64_t> GetNeighborHouses(uint64_t house_id, float radius) const;
    std::vector<uint64_t> GetAllHouses() const;
    
    // [SEQUENCE: MVP14-32] Community features
    void UpdateDesirability();
    float GetDesirabilityScore() const { return properties_.desirability_score; }
    
    void AddCommunityFeature(const std::string& feature_type, const Vector3& position);
    void RemoveCommunityFeature(const std::string& feature_type);
    
    // Events
    void StartSeasonalEvent(const std::string& event_type);
    void EndSeasonalEvent();
    bool IsEventActive() const { return !active_event_.empty(); }
    
    // Statistics
    struct Statistics {
        uint32_t total_residents{0};
        uint32_t active_residents_today{0};
        uint64_t total_property_value{0};
        float average_house_rating{0.0f};
        uint32_t community_events_held{0};
    };
    
    Statistics GetStatistics() const;
    
private:
    Properties properties_;
    NeighborhoodLayout layout_;
    
    std::string active_event_;
    std::chrono::system_clock::time_point event_end_time_;
    
    // Cached data
    mutable std::unordered_map<uint64_t, std::vector<uint64_t>> neighbor_cache_;
    mutable std::chrono::steady_clock::time_point cache_expiry_;
    
    void InvalidateNeighborCache();
    void CalculateDesirability();
};

// [SEQUENCE: MVP14-33] Neighborhood management
class NeighborhoodManager {
public:
    static NeighborhoodManager& Instance() {
        static NeighborhoodManager instance;
        return instance;
    }
    
    // [SEQUENCE: MVP14-34] Neighborhood creation
    std::shared_ptr<Neighborhood> CreateNeighborhood(
        const std::string& name,
        NeighborhoodType type,
        const Vector3& location);
    
    void RegisterNeighborhood(std::shared_ptr<Neighborhood> neighborhood);
    std::shared_ptr<Neighborhood> GetNeighborhood(uint32_t neighborhood_id);
    
    // Search and discovery
    std::vector<std::shared_ptr<Neighborhood>> GetNeighborhoodsByType(
        NeighborhoodType type);
    
    std::shared_ptr<Neighborhood> FindBestNeighborhood(
        HouseType house_type,
        uint64_t budget);
    
    // [SEQUENCE: MVP14-35] Plot allocation
    struct PlotAllocation {
        uint32_t neighborhood_id;
        uint32_t plot_id;
        Plot* plot_info;
    };
    
    PlotAllocation AllocatePlot(uint64_t player_id, HouseType type,
                               NeighborhoodType preferred_type);
    
    bool TransferPlot(uint64_t from_player, uint64_t to_player,
                     uint32_t neighborhood_id, uint32_t plot_id);
    
    // Community management
    void UpdateAllNeighborhoods();
    void ProcessPropertyTaxes();
    void UpdateDesirabilityScores();
    
    // Analytics
    struct NeighborhoodAnalytics {
        std::unordered_map<NeighborhoodType, uint32_t> houses_by_type;
        std::unordered_map<uint32_t, float> occupancy_rates;
        uint64_t total_property_value;
        uint32_t total_neighborhoods;
    };
    
    NeighborhoodAnalytics GetAnalytics() const;
    
private:
    NeighborhoodManager() = default;
    
    std::unordered_map<uint32_t, std::shared_ptr<Neighborhood>> neighborhoods_;
    std::atomic<uint32_t> next_neighborhood_id_{1};
    
    // Spatial indexing for fast lookups
    std::unordered_map<uint32_t, std::vector<uint32_t>> zone_neighborhoods_;
};

// [SEQUENCE: MVP14-36] Community interaction system
class CommunityInteraction {
public:
    // Neighbor relationships
    enum class RelationshipType {
        STRANGER,       // 낯선 사람
        ACQUAINTANCE,   // 아는 사람
        NEIGHBOR,       // 이웃
        FRIEND,         // 친구
        BEST_FRIEND,    // 절친
        RIVAL           // 라이벌
    };
    
    struct NeighborRelation {
        uint64_t player_id_1;
        uint64_t player_id_2;
        RelationshipType type{RelationshipType::STRANGER};
        int32_t relationship_points{0};
        std::chrono::system_clock::time_point last_interaction;
    };
    
    // [SEQUENCE: MVP14-37] Interaction methods
    void RecordInteraction(uint64_t player1, uint64_t player2,
                          const std::string& interaction_type);
    
    RelationshipType GetRelationship(uint64_t player1, uint64_t player2) const;
    void UpdateRelationship(uint64_t player1, uint64_t player2,
                          int32_t points_change);
    
    // Community activities
    void OrganizeBlockParty(uint32_t neighborhood_id, uint64_t organizer_id);
    void JoinCommunityEvent(uint32_t event_id, uint64_t player_id);
    
    // Neighbor assistance
    void RequestHelp(uint64_t requester_id, const std::string& help_type);
    void OfferHelp(uint64_t helper_id, uint64_t requester_id);
    
    // Reputation system
    struct CommunityReputation {
        int32_t helpful_score{0};
        int32_t friendly_score{0};
        int32_t event_participation{0};
        int32_t decoration_score{0};
        
        int32_t GetTotalReputation() const {
            return helpful_score + friendly_score + 
                   event_participation + decoration_score;
        }
    };
    
    CommunityReputation GetReputation(uint64_t player_id) const;
    void UpdateReputation(uint64_t player_id, const std::string& action);
    
private:
    std::unordered_map<std::pair<uint64_t, uint64_t>, NeighborRelation,
                      pair_hash> relationships_;
    
    std::unordered_map<uint64_t, CommunityReputation> reputations_;
    
    struct pair_hash {
        template <class T1, class T2>
        std::size_t operator()(const std::pair<T1, T2>& p) const {
            auto h1 = std::hash<T1>{}(p.first);
            auto h2 = std::hash<T2>{}(p.second);
            return h1 ^ h2;
        }
    };
};

// [SEQUENCE: MVP14-38] Neighborhood services
class NeighborhoodServices {
public:
    struct ServicePoint {
        std::string service_type;  // "mailbox", "bank", "vendor"
        Vector3 position;
        uint32_t service_level{1};
        bool is_available{true};
        
        // Usage stats
        uint32_t daily_uses{0};
        std::chrono::system_clock::time_point last_maintenance;
    };
    
    // [SEQUENCE: MVP14-39] Service management
    void AddService(uint32_t neighborhood_id, const ServicePoint& service);
    void RemoveService(uint32_t neighborhood_id, const std::string& service_type);
    void UpgradeService(uint32_t neighborhood_id, const std::string& service_type);
    
    // Service discovery
    ServicePoint* FindNearestService(const Vector3& position,
                                   const std::string& service_type);
    
    std::vector<ServicePoint> GetNeighborhoodServices(
        uint32_t neighborhood_id) const;
    
    // Maintenance
    void PerformMaintenance(uint32_t neighborhood_id);
    float GetServiceQuality(uint32_t neighborhood_id) const;
    
    // Special services
    void EnableSeasonalDecorations(uint32_t neighborhood_id);
    void SetupCommunityGarden(uint32_t neighborhood_id, const Vector3& location);
    void InstallSecuritySystem(uint32_t neighborhood_id);
    
private:
    std::unordered_map<uint32_t, std::vector<ServicePoint>> services_;
    
    void UpdateServiceAvailability();
};

// [SEQUENCE: MVP14-40] Neighborhood events
class NeighborhoodEvents {
public:
    struct CommunityEvent {
        uint32_t event_id;
        std::string name;
        std::string description;
        uint32_t neighborhood_id;
        
        std::chrono::system_clock::time_point start_time;
        std::chrono::system_clock::time_point end_time;
        
        Vector3 event_location;
        uint32_t max_participants{50};
        std::vector<uint64_t> participants;
        
        // Rewards
        struct EventReward {
            std::string type;
            uint32_t amount;
        };
        std::vector<EventReward> rewards;
        
        // Requirements
        uint32_t min_reputation{0};
        uint32_t entry_fee{0};
    };
    
    // [SEQUENCE: MVP14-41] Event management
    uint32_t CreateEvent(const CommunityEvent& event);
    void CancelEvent(uint32_t event_id);
    void StartEvent(uint32_t event_id);
    void EndEvent(uint32_t event_id);
    
    // Participation
    bool RegisterForEvent(uint32_t event_id, uint64_t player_id);
    void UnregisterFromEvent(uint32_t event_id, uint64_t player_id);
    bool CheckInToEvent(uint32_t event_id, uint64_t player_id);
    
    // Event types
    void StartBlockParty(uint32_t neighborhood_id);
    void StartGardenContest(uint32_t neighborhood_id);
    void StartDecoratingContest(uint32_t neighborhood_id);
    void StartCommunityMarket(uint32_t neighborhood_id);
    
    // Seasonal events
    void StartSpringFestival(uint32_t neighborhood_id);
    void StartSummerBBQ(uint32_t neighborhood_id);
    void StartHarvestFestival(uint32_t neighborhood_id);
    void StartWinterCelebration(uint32_t neighborhood_id);
    
    // Event results
    struct EventResults {
        uint32_t total_participants;
        std::vector<std::pair<uint64_t, uint32_t>> contest_rankings;
        uint64_t community_points_earned;
    };
    
    EventResults GetEventResults(uint32_t event_id) const;
    
private:
    std::unordered_map<uint32_t, CommunityEvent> events_;
    std::atomic<uint32_t> next_event_id_{1};
    
    // Active events by neighborhood
    std::unordered_map<uint32_t, std::vector<uint32_t>> neighborhood_events_;
    
    void DistributeRewards(uint32_t event_id);
};

// [SEQUENCE: MVP14-42] Neighborhood utilities
namespace NeighborhoodUtils {
    // Distance calculations
    float GetDistanceBetweenHouses(uint64_t house1_id, uint64_t house2_id);
    std::vector<uint64_t> GetHousesInRadius(const Vector3& center, float radius);
    
    // Neighborhood scoring
    float CalculatePropertyValue(const Neighborhood::Plot& plot,
                               const Neighborhood& neighborhood);
    
    float CalculateDesirability(const NeighborhoodFeatures& features,
                              uint32_t house_count,
                              float average_property_value);
    
    // Layout generation
    NeighborhoodLayout GenerateSuburbanLayout(uint32_t plot_count);
    NeighborhoodLayout GenerateUrbanLayout(uint32_t plot_count);
    NeighborhoodLayout GenerateRuralLayout(uint32_t plot_count);
    
    // Pathfinding
    std::vector<Vector3> FindPathBetweenPlots(const NeighborhoodLayout& layout,
                                            uint32_t from_plot,
                                            uint32_t to_plot);
}

} // namespace mmorpg::housing