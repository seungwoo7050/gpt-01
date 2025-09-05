# Extension 07: Matchmaking Service Integration

## 1. Objective

This guide details the creation of a foundational matchmaking service. The goal is to allow players to queue up for a specific game mode, have the server form a match based on simple criteria, and notify the matched players. This is a core feature for most session-based multiplayer games.

## 2. Analysis of Current Implementation

The project currently has no concept of matchmaking.
*   **`packet.proto`**: Lacks any messages for joining a queue or being notified of a match.
*   **`PacketHandler`**: Has no handlers for matchmaking-related logic.
*   **No Queueing System**: There is no central manager or system responsible for maintaining a queue of players waiting for a game.

This feature will be built from scratch.

## 3. Proposed Implementation

### Step 3.1: Define Matchmaking Protobuf Messages

First, we need to define the data structures for matchmaking communication in `packet.proto`.

**Modified File (`ecs-realm-server/proto/packet.proto`):**
```diff
// ... existing enum PacketType
    PACKET_DUEL_DECLINE_REQUEST = 3009;

    // [NEW] Matchmaking packets (4000-4099)
    PACKET_JOIN_MATCHMAKING_REQUEST = 4000;
    PACKET_JOIN_MATCHMAKING_RESPONSE = 4001;
    PACKET_LEAVE_MATCHMAKING_REQUEST = 4002;
    PACKET_LEAVE_MATCHMAKING_RESPONSE = 4003;
    PACKET_MATCH_FOUND_NOTIFICATION = 4004;
}

// ... existing messages

// [NEW] Matchmaking Messages
message JoinMatchmakingRequest {
    int32 game_mode = 1; // e.g., 1 for 1v1 Arena, 2 for 5v5 Battleground
}

message JoinMatchmakingResponse {
    bool success = 1;
    int32 estimated_wait_time_sec = 2;
}

message LeaveMatchmakingRequest {}

message LeaveMatchmakingResponse {
    bool success = 1;
}

message MatchFoundNotification {
    string room_id = 1;
    string server_ip = 2;
    int32 server_port = 3;
    repeated uint64 player_ids = 4;
}
```
*(Remember to re-run the protobuf compiler after this change.)*

### Step 3.2: Create the `MatchmakingManager`

This new class will be the heart of the matchmaking system. It will manage queues and run the matching logic. For simplicity, we will make it a singleton that is updated periodically by the main server loop.

**New File (`ecs-realm-server/src/game/matchmaking/matchmaking_manager.h`):**
```cpp
#pragma once

#include <cstdint>
#include <vector>
#include <deque>
#include <unordered_map>
#include <memory>

namespace mmorpg::network { class Session; } // Forward declaration

namespace mmorpg::game {

struct QueuedPlayer {
    std::shared_ptr<network::Session> session;
    int32_t game_mode;
    uint32_t skill_rating; // e.g., MMR or Elo
    // Add other relevant data like latency, etc.
};

class MatchmakingManager {
public:
    static MatchmakingManager& Instance();

    void Enqueue(std::shared_ptr<network::Session> session, int32_t game_mode);
    void Dequeue(std::shared_ptr<network::Session> session);
    void Update(float delta_time);

private:
    MatchmakingManager() = default;
    ~MatchmakingManager() = default;

    void TryFormMatch(int32_t game_mode);

    // Map from game_mode to a queue of players
    std::unordered_map<int32_t, std::deque<QueuedPlayer>> m_queues;
};

}
```

**New File (`ecs-realm-server/src/game/matchmaking/matchmaking_manager.cpp`):**
```cpp
#include "matchmaking_manager.h"
#include "network/session.h"
#include "proto/packet.pb.h"

namespace mmorpg::game {

MatchmakingManager& MatchmakingManager::Instance() {
    static MatchmakingManager instance;
    return instance;
}

void MatchmakingManager::Enqueue(std::shared_ptr<network::Session> session, int32_t game_mode) {
    // TODO: Prevent player from queueing if already in a queue or in a game
    QueuedPlayer player{session, game_mode, 1200}; // Assume default skill rating
    m_queues[game_mode].push_back(player);
    // Respond to client
    proto::JoinMatchmakingResponse res;
    res.set_success(true);
    res.set_estimated_wait_time_sec(60);
    session->Send(res);
}

void MatchmakingManager::Dequeue(std::shared_ptr<network::Session> session) {
    // TODO: Implement logic to find and remove a player from all queues
}

void MatchmakingManager::Update(float /*delta_time*/) {
    // This method should be called periodically from the main game loop
    for (auto const& [game_mode, queue] : m_queues) {
        TryFormMatch(game_mode);
    }
}

void MatchmakingManager::TryFormMatch(int32_t game_mode) {
    auto& queue = m_queues[game_mode];

    // Simple example: form a 2-player match
    if (queue.size() >= 2) {
        QueuedPlayer player1 = queue.front();
        queue.pop_front();
        QueuedPlayer player2 = queue.front();
        queue.pop_front();

        // Match found! Notify players.
        LOG_INFO("Match found for game mode {} between players {} and {}", 
                 game_mode, player1.session->GetPlayerId(), player2.session->GetPlayerId());

        proto::MatchFoundNotification notification;
        notification.set_room_id("room_123"); // This would be dynamically generated
        notification.set_server_ip("127.0.0.1");
        notification.set_server_port(9001);
        notification.add_player_ids(player1.session->GetPlayerId());
        notification.add_player_ids(player2.session->GetPlayerId());

        player1.session->Send(notification);
        player2.session->Send(notification);

        // TODO: Transition these players' state to "In-Game"
    }
}

}
```

### Step 3.3: Integrate with the Server

1.  **Update Game Loop**: Call `MatchmakingManager::Instance().Update(delta_time);` from your main server game loop.
2.  **Register Packet Handler**: Register a handler for the `JoinMatchmakingRequest`.

**Modified File (`ecs-realm-server/src/network/packet_handler.cpp` or where handlers are registered):**
```cpp
#include "game/matchmaking/matchmaking_manager.h"

void RegisterGamePacketHandlers(PacketHandler& packetHandler) {
    // ... other handlers

    // [NEW] Register Matchmaking handler
    packetHandler.RegisterHandler(mmorpg::proto::JoinMatchmakingRequest::descriptor(), 
        [](std::shared_ptr<Session> session, const google::protobuf::Message& msg) {
            const auto& req = static_cast<const mmorpg::proto::JoinMatchmakingRequest&>(msg);
            game::MatchmakingManager::Instance().Enqueue(session, req.game_mode());
        });
    
    // TODO: Register handler for LeaveMatchmakingRequest
}
```
3.  **Update Build System**: Add the new `matchmaking_manager.cpp` to your `add_library(mmorpg_game ...)` section in `CMakeLists.txt`.

## 4. Rationale for Changes

*   **Singleton Manager**: A singleton `MatchmakingManager` provides a simple, globally accessible entry point for managing queues. For a distributed server, this manager would need to be a separate microservice that communicates with game servers, likely using a message bus like RabbitMQ or Redis Pub/Sub.
*   **Periodic Updates**: Running the matching logic in a periodic `Update` call decouples it from the client requests. This is a standard pattern that allows for more complex, non-blocking matching logic to be executed at a fixed interval.
*   **Simple Logic First**: The initial matching logic is a simple "first-come, first-served" for 2 players. This provides a solid foundation that can be expanded with more complex rules (skill-based, role-based, etc.) later without changing the overall architecture.

## 5. Testing Guide

1.  **Unit Test the Manager**: Create a `test_matchmaking_manager.cpp` file.
    *   Test Case 1: Enqueue one player. Call `Update`. Assert that no match is formed.
    *   Test Case 2: Enqueue two players. Call `Update`. Assert that a match is formed and that both players receive a `MatchFoundNotification`. You will need mock `Session` objects that can capture the messages they are sent.
    *   Test Case 3: Enqueue three players. Call `Update`. Assert that a 2-player match is formed and one player remains in the queue.
2.  **Integration Test**: 
    *   Use two instances of the `load_test_client` (or modify it to support this scenario).
    *   Have both clients connect and send a `JoinMatchmakingRequest`.
    *   Verify that both clients receive a `MatchFoundNotification`.
    *   Check server logs to confirm the "Match found" message appears.
