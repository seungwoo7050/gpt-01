#pragma once

#include <cstdint>
#include <chrono>

namespace mmorpg::game::components {

// [SEQUENCE: 1] Network synchronization component
struct NetworkComponent {
    uint64_t owner_session_id = 0;  // Session that owns this entity
    uint64_t owner_player_id = 0;   // Player ID
    
    // Sync state
    bool needs_full_update = true;   // Send all component data
    bool needs_position_update = false;
    bool needs_health_update = false;
    bool needs_removal = false;
    
    // Network optimization
    std::chrono::steady_clock::time_point last_sync_time;
    uint32_t sync_priority = 1;     // Higher = more important
    float sync_distance = 100.0f;   // Max distance for sync
    
    // Prediction/interpolation
    uint32_t last_acknowledged_input = 0;
    float interpolation_buffer = 0.1f; // 100ms buffer
    
    // [SEQUENCE: 2] Mark for update
    void MarkDirty() {
        needs_full_update = true;
    }
    
    void MarkPositionDirty() {
        needs_position_update = true;
    }
    
    void MarkHealthDirty() {
        needs_health_update = true;
    }
    
    // [SEQUENCE: 3] Check if owned by session
    bool IsOwnedBy(uint64_t session_id) const {
        return owner_session_id == session_id;
    }
    
    // [SEQUENCE: 4] Check if needs any update
    bool NeedsUpdate() const {
        return needs_full_update || 
               needs_position_update || 
               needs_health_update ||
               needs_removal;
    }
};

} // namespace mmorpg::game::components