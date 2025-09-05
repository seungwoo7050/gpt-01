# Extension 05: Session Robustness (Heartbeat, Timeout, Replay Prevention)

## 1. Objective

This guide focuses on making client sessions more robust and secure. We will implement three key features:
1.  **Timeout Mechanism**: Automatically disconnect clients that have been silent for too long to prevent zombie sessions.
2.  **Heartbeat Protocol**: A keep-alive mechanism for connections that might be idle but should remain open.
3.  **Replay Attack Prevention**: A basic security measure to prevent malicious actors from re-using old login packets.

## 2. Analysis of Current Implementation

*   **No Timeouts**: The current `Session` class will keep a connection open indefinitely, even if the client has crashed or lost connectivity. This can lead to a resource leak as zombie sessions accumulate.
*   **No Heartbeat Logic**: While `HeartbeatRequest` and `HeartbeatResponse` are defined in `packet.proto`, there is no server-side logic in `PacketHandler` or `Session` to process them.
*   **No Replay Prevention**: The `LoginRequest` message contains no fields to prevent a captured packet from being replayed by an attacker to gain unauthorized access.

## 3. Proposed Implementation

### Step 3.1: Implement Session Timeout

We will add a `boost::asio::steady_timer` to the `Session` class. Any successful data read will reset the timer. If the timer expires, the session is disconnected.

**Modified File (`ecs-realm-server/src/network/session.h`):**
```cpp
// ... includes
#include <boost/asio/steady_timer.hpp> // [NEW]

class Session : public std::enable_shared_from_this<Session> {
public:
    // ... constructor, etc.

private:
    // ...
    void DoReadHeader();
    void DoReadBody(uint32_t body_size);

    // [NEW] Timeout handling
    void StartTimeoutTimer();
    void OnTimeout(const boost::system::error_code& ec);
    void ResetTimeout();

    // ... private members
    boost::asio::strand<boost::asio::any_io_executor> m_strand;
    boost::asio::steady_timer m_timeoutTimer; // [NEW]
    // ...
};
```

**Modified File (`ecs-realm-server/src/network/session.cpp`):**
```cpp
// In Session constructor
Session::Session(/*...*/) 
    : /*...*/,
      m_timeoutTimer(io_context) // [NEW] Initialize timer
      {}

void Session::DoHandshake() {
    // ... on successful handshake:
    if (!ec) {
        // ...
        self->DoReadHeader();
        self->StartTimeoutTimer(); // [NEW] Start the timer after connecting
    } else {
        // ...
    }
}

void Session::DoReadBody(uint32_t body_size) {
    // ...
    boost::asio::async_read(m_ssl_stream, /*...*/, 
        [self = shared_from_this()](/*...*/) {
            if (!ec) {
                self->ResetTimeout(); // [NEW] Reset timer on any activity
                self->ProcessPacket(std::move(self->m_readBuffer));
                self->DoReadHeader();
            } else { /*...*/ }
        });
}

// [NEW] Add timer implementation
void Session::StartTimeoutTimer() {
    m_timeoutTimer.expires_after(std::chrono::seconds(30));
    m_timeoutTimer.async_wait(
        boost::asio::bind_executor(m_strand,
            std::bind(&Session::OnTimeout, shared_from_this(), std::placeholders::_1)));
}

void Session::OnTimeout(const boost::system::error_code& ec) {
    if (ec == boost::asio::error::operation_aborted) {
        // Timer was cancelled, which is normal.
        return;
    }
    if (m_state == SessionState::Connected) {
        LOG_INFO("Session [{}] timed out. Disconnecting.", m_sessionId);
        Disconnect();
    }
}

void Session::ResetTimeout() {
    m_timeoutTimer.cancel();
    StartTimeoutTimer();
}

void Session::Disconnect() {
    if (m_state == SessionState::Disconnected) return;
    m_timeoutTimer.cancel(); // [NEW] Cancel timer on disconnect
    m_state = SessionState::Disconnected;
    // ... rest of disconnect logic
}
```

### Step 3.2: Implement Heartbeat Handler

Now, let's implement the handler for the existing `HeartbeatRequest` message. This handler will simply send a `HeartbeatResponse` back. The act of receiving the request is enough to reset the timeout timer.

**Modified File (`ecs-realm-server/src/network/packet_handler.cpp` or wherever handlers are registered):**
```cpp
// In the function where you register your packet handlers

#include "proto/packet.pb.h"

void RegisterGamePacketHandlers(PacketHandler& packetHandler) {
    // ... other handlers

    // [NEW] Register Heartbeat handler
    packetHandler.RegisterHandler(mmorpg::proto::HeartbeatRequest::descriptor(), 
        [](std::shared_ptr<Session> session, const google::protobuf::Message& msg) {
            // The timeout is already reset just by receiving the packet.
            // We just need to send a response.
            mmorpg::proto::HeartbeatResponse res;
            res.set_timestamp(std::chrono::system_clock::now().time_since_epoch().count());
            session->Send(res);
        });
}
```

### Step 3.3: Add Replay Attack Prevention

First, update the `LoginRequest` message definition.

**Modified File (`ecs-realm-server/proto/packet.proto`):**
```diff
// In LoginRequest message definition
message LoginRequest {
    string username = 1;
    string password_hash = 2;
+   uint64 timestamp_ms = 3; // [NEW] Client-side timestamp in milliseconds
+   string nonce = 4;        // [NEW] A unique random string for this request
}
```
*(Remember to re-run the protobuf compiler after this change.)*

Next, create a simple cache to store recently seen nonces and check it in the `LoginRequest` handler.

**Modified File (`ecs-realm-server/src/network/packet_handler.cpp` or wherever the login handler is):**
```cpp
#include <chrono>
#include <unordered_set>
#include <mutex>

// [NEW] A simple, global cache for nonces. In a real distributed system, this would need to be in Redis or a similar shared cache.
std::unordered_set<std::string> g_recentNonces;
std::mutex g_nonceMutex;

void HandleLoginRequest(std::shared_ptr<Session> session, const google::protobuf::Message& msg) {
    const auto& req = static_cast<const mmorpg::proto::LoginRequest&>(msg);

    // [NEW] Replay attack checks
    // 1. Timestamp check: Reject requests that are too old (e.g., > 5 seconds)
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    if (now_ms - req.timestamp_ms() > 5000) {
        LOG_WARN("Session [{}] sent LoginRequest with old timestamp. Rejecting.", session->GetSessionId());
        // Optionally send a failure response
        session->Disconnect();
        return;
    }

    // 2. Nonce check: Reject if we've seen this nonce recently
    {
        std::lock_guard<std::mutex> lock(g_nonceMutex);
        if (g_recentNonces.count(req.nonce())) {
            LOG_WARN("Session [{}] sent LoginRequest with duplicate nonce. Rejecting.", session->GetSessionId());
            session->Disconnect();
            return;
        }
        // TODO: Add a mechanism to periodically clean up old nonces from the set.
        g_recentNonces.insert(req.nonce());
    }

    // ... (rest of the original login logic)
}
```

## 4. Rationale for Changes

*   **`steady_timer` for Timeouts**: We use `boost::asio::steady_timer` because it is monotonic and not affected by changes to the system clock, making it ideal for measuring durations.
*   **Implicit Timeout Reset**: Resetting the timer upon any packet arrival is an efficient way to handle timeouts. It means any valid communication, including heartbeats, keeps the session alive.
*   **Stateless Heartbeats**: The heartbeat implementation is stateless on the server. It doesn't need to track anything; it just responds. This is simple and scalable.
*   **Timestamp + Nonce**: Using both a timestamp and a nonce provides two layers of protection. The timestamp prevents very old packets from being replayed, while the nonce prevents the immediate replay of a recently captured packet. For a production system, the nonce cache would need to be a shared, time-aware data store like Redis with TTLs.

## 5. Testing Guide

1.  **Timeout Test**: 
    *   Create a test where a client connects but sends no data.
    *   In your test code, wait for slightly longer than the timeout duration (e.g., 31 seconds).
    *   Assert that the server has disconnected the client. You can check this by attempting to write to the socket and expecting a "broken pipe" or similar error.
2.  **Heartbeat Test**:
    *   Create a client that connects and does nothing but send a `HeartbeatRequest` every 15 seconds.
    *   Let the test run for a minute (longer than the 30-second timeout).
    *   Assert that the client remains connected and receives a `HeartbeatResponse` for each request.
3.  **Replay Attack Test**:
    *   Create a valid `LoginRequest` protobuf message.
    *   Send it to the server and confirm the login succeeds.
    *   Immediately send the *exact same* `LoginRequest` message again.
    *   Assert that the second attempt is rejected and the session is disconnected.
