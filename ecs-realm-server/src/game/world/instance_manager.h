#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <chrono>
#include "core/singleton.h"
#include "core/types.h"

namespace mmorpg::game::world {

// [SEQUENCE: MVP7-60] Instance difficulty levels
enum class InstanceDifficulty {
    NORMAL,
    HARD,
    HEROIC,
    MYTHIC,
    MYTHIC_PLUS
};

// [SEQUENCE: MVP7-61] Instance reset frequency
enum class InstanceResetFrequency {
    NEVER,
    ON_LEAVE,
    DAILY,
    WEEKLY,
    MONTHLY
};

// [SEQUENCE: MVP7-62] Instance state
enum class InstanceState {
    IDLE,
    IN_PROGRESS,
    COMPLETED,
    EXPIRED
};

// [SEQUENCE: MVP7-63] Instance configuration data
struct InstanceConfig {
    uint32_t map_id;
    std::string name;
    uint32_t min_players;
    uint32_t max_players;
    uint32_t min_level;
    InstanceResetFrequency reset_frequency;
    std::chrono::seconds time_limit;
};

// [SEQUENCE: MVP7-64] Live instance data
class Instance {
public:
    Instance(uint64_t guid, const InstanceConfig& config);

    uint64_t GetGuid() const { return guid_; }
    const InstanceConfig& GetConfig() const { return config_; }
    InstanceState GetState() const { return state_; }
    
    bool AddPlayer(core::ecs::EntityId player_id);
    bool RemovePlayer(core::ecs::EntityId player_id);
    bool IsFull() const;
    
    void Start();
    void Complete();
    void Reset();

private:
    uint64_t guid_;
    InstanceConfig config_;
    InstanceState state_ = InstanceState::IDLE;
    std::vector<core::ecs::EntityId> players_;
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point completion_time_;
};

// [SEQUENCE: MVP7-65] Instance manager
class InstanceManager : public Singleton<InstanceManager> {
public:
    // [SEQUENCE: MVP7-66] Create a new instance
    std::shared_ptr<Instance> CreateInstance(uint32_t map_id, InstanceDifficulty difficulty = InstanceDifficulty::NORMAL);
    
    // [SEQUENCE: MVP7-67] Find an instance by GUID
    std::shared_ptr<Instance> GetInstance(uint64_t guid);
    
    // [SEQUENCE: MVP7-68] Find or create an instance for a party
    std::shared_ptr<Instance> FindOrCreateInstanceForParty(uint32_t map_id, uint64_t party_id);
    
    // [SEQUENCE: MVP7-69] Update loop for all instances
    void Update(float delta_time);
    
    // [SEQUENCE: MVP7-70] Load instance configurations from data
    void LoadInstanceConfigs(const std::string& file_path);

private:
    friend class Singleton<InstanceManager>;
    InstanceManager() = default;

    std::unordered_map<uint64_t, std::shared_ptr<Instance>> instances_;
    std::unordered_map<uint32_t, InstanceConfig> instance_configs_;
    std::unordered_map<uint64_t, uint64_t> party_to_instance_map_;
    uint64_t next_guid_ = 1;
};

} // namespace mmorpg::game::world
