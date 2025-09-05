# Extension 06: Reliable UDP and Client-Side Interpolation

## 1. Objective

This guide addresses a core challenge in real-time multiplayer games: making unreliable UDP communication suitable for critical data like player movement. We will implement a basic reliable UDP system by leveraging the existing `sequence` number in `PacketHeader`. The goal is to detect out-of-order and lost packets. As an advanced topic, we will also describe a client-side interpolation strategy to smooth out character movement.

## 2. Analysis of Current Implementation

*   **Existing Sequence Field**: `proto/packet.proto` already defines a `sequence` field in `PacketHeader`. This is the perfect foundation, but it is currently unused for UDP packets.
*   **No Sequence Tracking**: The `UdpPacketHandler` does not inspect the sequence number. It processes handshake packets and ignores everything else. There is no mechanism to track which packets have been received or to detect gaps.
*   **No Interpolation**: Since the client-side is not implemented here, there is naturally no interpolation logic, which is crucial for hiding network jitter and packet loss from the player.

## 3. Proposed Implementation

### Step 3.1: Sending Sequenced UDP Packets

The first step is to ensure that every outgoing UDP packet has a unique, incrementing sequence number. This logic should reside within the `Session` class, which maintains the state for each client.

**Modified File (`ecs-realm-server/src/network/session.h`):**
```cpp
// ... includes

class Session : public std::enable_shared_from_this<Session> {
public:
    // ...
    // [MODIFIED] Add a method for sending sequenced UDP packets
    void SendUdp(const google::protobuf::Message& message);

private:
    // ...
    // [NEW] Sequence numbers for reliable UDP
    std::atomic<uint64_t> m_nextUdpOutSequence{0};
    std::atomic<uint64_t> m_lastUdpInSequence{0};
};
```

**Modified File (`ecs-realm-server/src/network/session.cpp`):**
```cpp
// This is a conceptual implementation. A real implementation would need a UDP socket from the UdpServer.
// For this guide, we assume a SendUdp method exists on the UdpServer that takes a buffer and endpoint.

// This method would be called from game systems to send updates.
void Session::SendUdp(const google::protobuf::Message& message) {
    if (!m_udp_endpoint.has_value()) {
        return; // No UDP endpoint for this session yet
    }

    // [NEW] Use the existing PacketSerializer but assign a sequence number
    uint64_t sequence_num = m_nextUdpOutSequence++;
    auto buffer = PacketSerializer::Serialize(message, sequence_num);
    if (buffer.empty()) return;

    // Assume UdpServer has a method to send data
    // UdpServer::Instance().Send(buffer, *m_udp_endpoint);
}
```
*Note: You would also need to modify `PacketSerializer::Serialize` to accept an optional sequence number and place it in the header.*

### Step 3.2: Processing Sequenced Packets

Next, we modify `UdpPacketHandler` to deserialize the packet, check the sequence number, and then pass the validated message to a new, higher-level handler.

**Modified File (`ecs-realm-server/src/network/udp_packet_handler.cpp`):**
```cpp
void UdpPacketHandler::Handle(std::shared_ptr<Session> session, const boost::asio::ip::udp::endpoint& endpoint, const std::vector<std::byte>& buffer, size_t size) {
    // ... (handshake logic remains the same)

    // If it's not a handshake, it must be a gameplay packet.
    if (!session) {
        return; // Ignore packets from unknown sources
    }

    // [MODIFIED] Deserialize the generic packet to access the header
    auto packet = network::PacketSerializer::Deserialize(buffer.data(), size);
    if (!packet) {
        LOG_WARN("Failed to deserialize UDP packet from session {}", session->GetSessionId());
        return;
    }

    // [NEW] Sequence Check
    uint64_t received_sequence = packet->header().sequence();
    if (session->IsNewerUdpSequence(received_sequence)) {
        session->UpdateLastUdpInSequence(received_sequence);
        
        // TODO: The packet is valid. Now, deserialize the payload and pass it to a game logic handler.
        // e.g., GameplayUdpHandler::Handle(session, packet->payload());
    } else {
        // Packet is old or a duplicate, discard it.
        LOG_DEBUG("Discarding out-of-order UDP packet from session {}. Last seen: {}, Received: {}", 
                  session->GetSessionId(), session->GetLastUdpInSequence(), received_sequence);
    }
}
```

To support this, we need to add the sequence tracking methods to the `Session` class.

**Modified File (`ecs-realm-server/src/network/session.h`):**
```cpp
class Session : public std::enable_shared_from_this<Session> {
public:
    // ...
    // [NEW] Methods for sequence checking
    bool IsNewerUdpSequence(uint64_t sequence) const {
        return sequence > m_lastUdpInSequence;
    }
    void UpdateLastUdpInSequence(uint64_t sequence) {
        m_lastUdpInSequence = sequence;
    }
    uint64_t GetLastUdpInSequence() const {
        return m_lastUdpInSequence;
    }
private:
    // ...
};
```

### Step 3.3: Client-Side Interpolation Strategy (Conceptual)

Implementing the actual client is outside the scope, but the strategy is critical. A client should not immediately snap a character to the position in a newly received `MovementUpdate`. Instead, it should perform interpolation.

**Client-Side Pseudo-code:**

```cpp
class RemotePlayer {
    // Buffer of recent, valid movement updates (timestamp, position)
    std::deque<MovementUpdate> movement_buffer;
    
    // The time the server is currently at (calculated from updates)
    double server_time = 0.0;
    // The local time we want to render at (always slightly behind server_time)
    double render_time = 0.0;

    void OnReceiveMovementUpdate(MovementUpdate update) {
        // Discard old packets using sequence numbers (as on server)
        if (IsNewer(update.sequence)) {
            movement_buffer.push_back(update);
            // Keep buffer from growing too large
            if (movement_buffer.size() > 10) {
                movement_buffer.pop_front();
            }
        }
    }

    void Update(float local_delta_time) {
        // The goal is to find two server updates that bracket our desired render_time
        if (movement_buffer.size() < 2) return; // Not enough data to interpolate

        // Advance our render clock smoothly
        render_time += local_delta_time;

        MovementUpdate& from = movement_buffer[0];
        MovementUpdate& to = movement_buffer[1];

        // Find the correct two updates in the buffer to interpolate between
        while (movement_buffer.size() >= 2 && render_time > to.timestamp) {
            movement_buffer.pop_front();
            from = movement_buffer[0];
            to = movement_buffer[1];
        }

        // Calculate interpolation factor (alpha)
        double total_time_diff = to.timestamp - from.timestamp;
        double render_time_diff = render_time - from.timestamp;
        float alpha = (total_time_diff > 0) ? (render_time_diff / total_time_diff) : 1.0f;
        alpha = std::clamp(alpha, 0.0f, 1.0f);

        // Linearly interpolate the position
        Vector3 interpolated_position = Vector3::Lerp(from.position, to.position, alpha);
        
        // Set the visual model's position to interpolated_position
        SetRenderedPosition(interpolated_position);
    }
};
```

## 4. Rationale for Changes

*   **Simple Sequence Check**: Simply accepting any packet with a higher sequence number than the last seen is the most basic form of reliable UDP. It effectively discards duplicates and out-of-order packets, which is a significant improvement.
*   **Stateful Sessions**: The sequence number state (`m_lastUdpInSequence`) must be stored per-session, as each client has its own independent stream of packets.
*   **Client-Side Authority on Rendering**: The server sends the truth (where players *are* at a specific time), but the client is responsible for making it look smooth. Interpolation is the key technique for this. It introduces a tiny amount of latency (the `render_time` is always slightly behind the latest server update) in exchange for perfectly smooth motion, hiding the effects of network jitter.

## 5. Testing Guide

1.  **Unit Test Sequence Logic**: Create a unit test for the `Session` class's sequence methods.
    ```cpp
    #include <gtest/gtest.h>
    #include "network/session.h"

    TEST(ReliableUdpTest, SequenceNumber) {
        // You will need a mock Session object for this
        MockSession session;
        
        EXPECT_TRUE(session.IsNewerUdpSequence(1));
        session.UpdateLastUdpInSequence(1);

        EXPECT_FALSE(session.IsNewerUdpSequence(0)); // Old packet
        EXPECT_FALSE(session.IsNewerUdpSequence(1)); // Duplicate packet
        EXPECT_TRUE(session.IsNewerUdpSequence(2));  // New packet
    }
    ```
2.  **Integration Test with Packet Loss**: This requires a more advanced test setup. You can create a proxy between the client and server that can be configured to randomly drop a certain percentage of UDP packets. When running the `load_test_client` through this proxy, you can monitor server logs to ensure that the "Discarding out-of-order" messages appear and that the server remains stable.
