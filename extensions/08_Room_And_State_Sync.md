# Extension 08: Room Spin-up and State Synchronization

## 1. Objective

This guide details the architectural changes required to support multiple, independent game sessions, hereafter called "Rooms." When a match is made, we need to dynamically create an isolated world for the matched players. We will also implement the fundamental state synchronization mechanisms: sending a full **snapshot** of the room state to a joining player, and broadcasting periodic **delta** updates to all players in the room.

## 2. Analysis of Current Implementation

The current server architecture is monolithic, with a single, global game world.

*   **`OptimizedWorld`**: This class, which represents the game world, is designed as a single instance. It directly owns the ECS managers.
*   **`main.cpp`**: The main server loop creates and updates a single instance of game systems like `PvpManager`. There is no concept of multiple, concurrent game loops for different groups of players.

To support rooms, we must refactor the core architecture to allow `OptimizedWorld` and its associated systems to be instantiated multiple times.

## 3. Proposed Implementation

### Step 3.1: Create `Room` and `RoomManager` Classes

First, we introduce the concept of a `Room`, which encapsulates an `OptimizedWorld` instance. The `RoomManager` will be a singleton responsible for creating, managing, and destroying these rooms.

**New File (`ecs-realm-server/src/game/room/room.h`):**
```cpp
#pragma once

#include "core/ecs/optimized/optimized_world.h"
#include "network/session.h"
#include <string>
#include <vector>
#include <memory>

namespace mmorpg::game {

class Room {
public:
    Room(std::string id, std::vector<std::shared_ptr<network::Session>> players);
    ~Room();

    void Update(float delta_time);
    void AddPlayer(std::shared_ptr<network::Session> player);
    void RemovePlayer(uint32_t session_id);

    const std::string& GetId() const { return m_id; }

private:
    void Broadcast(const google::protobuf::Message& message);
    void SendSnapshot(std::shared_ptr<network::Session> player);

    std::string m_id;
    core::ecs::optimized::OptimizedWorld m_world;
    std::vector<std::shared_ptr<network::Session>> m_players;
    // TODO: Add mutex for thread-safe player list modification
};

}
```

**New File (`ecs-realm-server/src/game/room/room_manager.h`):**
```cpp
#pragma once

#include "room.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace mmorpg::network { class Session; } // Forward declaration

namespace mmorpg::game {

class RoomManager {
public:
    static RoomManager& Instance();

    std::shared_ptr<Room> CreateRoom(std::vector<std::shared_ptr<network::Session>> players);
    void RemoveRoom(const std::string& room_id);
    void Update(float delta_time);

private:
    RoomManager() = default;
    std::unordered_map<std::string, std::shared_ptr<Room>> m_rooms;
    // TODO: Add mutex for thread-safe room map modification
};

}
```

### Step 3.2: Integrate `RoomManager` into the Server

Modify the main loop to update the `RoomManager` instead of individual game systems. The `MatchmakingManager` will now create rooms via the `RoomManager`.

**Modified File (`ecs-realm-server/src/server/game/main.cpp`):**
```diff
// ... includes
#include "game/room/room_manager.h"

// ...

int main(...) {
    // ... (initialization)

    auto last_time = std::chrono::high_resolution_clock::now();
    while (!io_context.stopped()) {
        auto current_time = std::chrono::high_resolution_clock::now();
        float delta_time = std::chrono::duration<float>(current_time - last_time).count();
        last_time = current_time;

-       pvp_manager->Update(delta_time);
+       // [MODIFIED] Update all active rooms
+       mmorpg::game::RoomManager::Instance().Update(delta_time);
+       // Matchmaking logic would also be updated here
+       mmorpg::game::MatchmakingManager::Instance().Update(delta_time);

        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }

    // ... (shutdown)
}
```

**Modified File (`ecs-realm-server/src/game/matchmaking/matchmaking_manager.cpp`):**
```diff
#include "game/room/room_manager.h"

// ... in TryFormMatch after a match is found

    if (queue.size() >= 2) {
        // ... (pop players from queue)

-       // Match found! Notify players.
-       LOG_INFO("...");
-       proto::MatchFoundNotification notification;
-       // ... (fill notification)
-       player1.session->Send(notification);
-       player2.session->Send(notification);
+
+       // [MODIFIED] Create a room for the matched players
+       auto room = RoomManager::Instance().CreateRoom({player1.session, player2.session});
+       LOG_INFO("Created room {} for players {} and {}", room->GetId(), /*...*/);
    }
```

### Step 3.3: Implement State Synchronization

This is the core logic for syncing the game state. We need new protobuf messages for this.

**Modified File (`ecs-realm-server/proto/packet.proto`):**
```diff
enum PacketType {
    // ...
    PACKET_MATCH_FOUND_NOTIFICATION = 4004;
+   PACKET_ROOM_STATE_SNAPSHOT = 4005;
+   PACKET_ROOM_STATE_DELTA = 4006;
}

// ...

// [NEW] State Synchronization Messages
message Vector3 { float x=1; float y=2; float z=3; }

message ComponentData {
    uint32 component_type_id = 1;
    bytes component_payload = 2; // Serialized component
}

message EntityData {
    uint64 entity_id = 1;
    repeated ComponentData components = 2;
}

message RoomStateSnapshot {
    repeated EntityData entities = 1;
}

message EntityDelta {
    enum UpdateType { CREATED = 0; UPDATED = 1; DESTROYED = 2; }
    UpdateType update_type = 1;
    EntityData entity_data = 2;
}

message RoomStateDelta {
    repeated EntityDelta deltas = 1;
}
```

Now, implement the logic in the `Room` class.

**Modified File (`ecs-realm-server/src/game/room/room.cpp` - Conceptual):**
```cpp
// In Room::AddPlayer
void Room::AddPlayer(std::shared_ptr<network::Session> player) {
    m_players.push_back(player);
    // When a new player joins, send them the complete current state
    SendSnapshot(player);
}

// Send a complete snapshot of the world to one player
void Room::SendSnapshot(std::shared_ptr<network::Session> player) {
    proto::RoomStateSnapshot snapshot;
    // This requires a way to iterate all entities and their components
    for (auto entity : m_world.GetAllEntities()) { 
        proto::EntityData* entity_data = snapshot.add_entities();
        entity_data->set_entity_id(entity);
        // This requires a way to get all components for an entity as raw data
        for (auto component_pair : m_world.GetAllComponents(entity)) {
            proto::ComponentData* comp_data = entity_data->add_components();
            comp_data->set_component_type_id(component_pair.id);
            comp_data->set_component_payload(component_pair.data); // Serialized
        }
    }
    player->Send(snapshot);
}

// In Room::Update
void Room::Update(float delta_time) {
    // 1. Update the world simulation
    m_world.Update(delta_time);

    // 2. Collect state changes (deltas)
    proto::RoomStateDelta delta_update;
    // This requires the ECS to track changes since the last frame
    auto changes = m_world.GetAndClearChanges(); 
    for (const auto& change : changes) {
        proto::EntityDelta* entity_delta = delta_update.add_deltas();
        entity_delta->set_update_type(change.type);
        // ... serialize the changed entity/components into entity_delta->mutable_entity_data()
    }

    // 3. Broadcast deltas to all players in the room
    if (delta_update.deltas_size() > 0) {
        Broadcast(delta_update);
    }
}

void Room::Broadcast(const google::protobuf::Message& message) {
    for (const auto& player : m_players) {
        player->Send(message);
    }
}
```

## 4. Rationale for Changes

*   **Encapsulation**: The `Room` class encapsulates a single game world (`OptimizedWorld`) and the players within it. This is a clean, object-oriented way to manage concurrent game sessions.
*   **Manager Pattern**: The `RoomManager` acts as a factory and registry for all active rooms, providing a single point of control for the main server loop.
*   **Snapshot on Join**: Sending a full snapshot is the simplest way to get a new client up to speed. While potentially large, it's a one-time cost per player and guarantees a consistent state.
*   **Delta Updates**: Broadcasting only the *changes* since the last frame is critical for performance. This minimizes bandwidth usage and client-side processing, allowing the game to scale to more players and more frequent updates.
*   **ECS Modifications**: This design requires significant new functionality in the ECS. The `OptimizedWorld` would need methods like `GetAllEntities()`, `GetAllComponents(entity)`, and most importantly, a change-tracking mechanism (`GetAndClearChanges()`) to produce the deltas. This is a non-trivial but necessary enhancement for a networked game.

## 5. Testing Guide

1.  **Unit Test `RoomManager`**: 
    *   Test that `CreateRoom` correctly returns a new room and adds it to the internal map.
    *   Test that `Update` correctly calls the `Update` method on all managed rooms.
    *   Test that `RemoveRoom` correctly destroys a room.
2.  **Integration Test State Sync**:
    *   Create a test with two mock clients that are put into a room.
    *   **Snapshot Test**: Verify that when the second client joins, it receives a `RoomStateSnapshot` containing the entities created for the first client.
    *   **Delta Test**: After both clients are in, create a new entity in the room's world. Call `Update` on the room. Verify that *both* clients receive a `RoomStateDelta` message containing a `CREATED` delta for the new entity.
