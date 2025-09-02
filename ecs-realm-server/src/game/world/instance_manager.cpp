#include "instance_manager.h"
#include <fstream>
#include <nlohmann/json.hpp>

namespace mmorpg::game::world {

using json = nlohmann::json;

// [SEQUENCE: MVP7-71] Instance implementation
Instance::Instance(uint64_t guid, const InstanceConfig& config)
    : guid_(guid), config_(config) {}

bool Instance::AddPlayer(core::ecs::EntityId player_id) {
    if (IsFull()) {
        return false;
    }
    players_.push_back(player_id);
    return true;
}

bool Instance::RemovePlayer(core::ecs::EntityId player_id) {
    auto it = std::remove(players_.begin(), players_.end(), player_id);
    if (it != players_.end()) {
        players_.erase(it, players_.end());
        return true;
    }
    return false;
}

bool Instance::IsFull() const {
    return players_.size() >= config_.max_players;
}

void Instance::Start() {
    if (state_ == InstanceState::IDLE) {
        state_ = InstanceState::IN_PROGRESS;
        start_time_ = std::chrono::steady_clock::now();
    }
}

void Instance::Complete() {
    if (state_ == InstanceState::IN_PROGRESS) {
        state_ = InstanceState::COMPLETED;
        completion_time_ = std::chrono::steady_clock::now();
    }
}

void Instance::Reset() {
    state_ = InstanceState::IDLE;
    players_.clear();
}

// [SEQUENCE: MVP7-72] InstanceManager implementation
std::shared_ptr<Instance> InstanceManager::CreateInstance(uint32_t map_id, InstanceDifficulty difficulty) {
    auto it = instance_configs_.find(map_id);
    if (it == instance_configs_.end()) {
        return nullptr; // No config for this map
    }
    
    uint64_t guid = next_guid_++;
    auto instance = std::make_shared<Instance>(guid, it->second);
    instances_[guid] = instance;
    
    return instance;
}

std::shared_ptr<Instance> InstanceManager::GetInstance(uint64_t guid) {
    auto it = instances_.find(guid);
    if (it != instances_.end()) {
        return it->second;
    }
    return nullptr;
}

std::shared_ptr<Instance> InstanceManager::FindOrCreateInstanceForParty(uint32_t map_id, uint64_t party_id) {
    auto it = party_to_instance_map_.find(party_id);
    if (it != party_to_instance_map_.end()) {
        return GetInstance(it->second);
    }
    
    auto new_instance = CreateInstance(map_id);
    if (new_instance) {
        party_to_instance_map_[party_id] = new_instance->GetGuid();
    }
    return new_instance;
}

void InstanceManager::Update(float delta_time) {
    // Handle instance logic, e.g., time limits, resets
    for (auto& pair : instances_) {
        auto& instance = pair.second;
        if (instance->GetState() == InstanceState::IN_PROGRESS) {
            // Check time limit, etc.
        }
    }
}

void InstanceManager::LoadInstanceConfigs(const std::string& file_path) {
    std::ifstream f(file_path);
    json data = json::parse(f);
    
    for (const auto& item : data["instances"]) {
        InstanceConfig config;
        config.map_id = item.at("map_id").get<uint32_t>();
        config.name = item.at("name").get<std::string>();
        config.min_players = item.at("min_players").get<uint32_t>();
        config.max_players = item.at("max_players").get<uint32_t>();
        config.min_level = item.at("min_level").get<uint32_t>();
        // ... load other fields ...
        
        instance_configs_[config.map_id] = config;
    }
}

} // namespace mmorpg::game::world
