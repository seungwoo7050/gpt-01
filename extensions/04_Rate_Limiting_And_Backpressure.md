# Extension 04: Rate Limiting and Backpressure

## 1. Objective

To enhance server stability and prevent abuse, this guide details the implementation of two crucial mechanisms: **Rate Limiting** and **Backpressure**. We will implement a per-session Token Bucket algorithm to control the rate of incoming messages and a simple backpressure mechanism to prevent the server's outgoing write queue from becoming overloaded.

## 2. Analysis of Current Implementation

The current `Session` class processes incoming data as fast as it arrives. 

**Legacy Code (`session.cpp`):**
```cpp
void Session::DoReadBody(uint32_t body_size) {
    // ...
    boost::asio::async_read(m_ssl_stream, boost::asio::buffer(m_readBuffer),
        boost::asio::bind_executor(m_strand,
            [self = shared_from_this()](/*...*/) {
                if (!ec) {
                    // [PROBLEM]: ProcessPacket is called immediately without any rate check.
                    self->ProcessPacket(std::move(self->m_readBuffer));
                    self->DoReadHeader(); // Immediately listen for the next packet.
                } else {
                    self->HandleError(ec);
                }
            }));
}
```
This implementation is vulnerable:
1.  **No Rate Limiting**: A malicious or misbehaving client can send thousands of packets per second, overwhelming the `PacketHandler` and consuming excessive CPU and memory.
2.  **Unbounded Write Queue**: The `Send` method unconditionally pushes data to `m_writeQueue`. If a client reads slowly while the server sends data rapidly, this queue can grow indefinitely, leading to memory exhaustion.

## 3. Proposed Implementation

### Step 3.1: Implement a Token Bucket in `Session`

First, we will add a simple token bucket implementation to the `Session` header file.

**Modified File (`ecs-realm-server/src/network/session.h`):**
```cpp
#pragma once

#include <boost/asio.hpp>
// ...
#include <chrono> // [NEW]

// ...

class Session : public std::enable_shared_from_this<Session> {
public:
    // ...

private:
    // [NEW] Token Bucket for Rate Limiting
    class TokenBucket {
    public:
        TokenBucket(double tokens_per_sec, double bucket_size)
            : rate_(tokens_per_sec), capacity_(bucket_size), tokens_(bucket_size),
              last_update_(std::chrono::steady_clock::now()) {}

        bool Consume(double tokens = 1.0) {
            Refill();
            if (tokens_ >= tokens) {
                tokens_ -= tokens;
                return true;
            }
            return false;
        }

    private:
        void Refill() {
            auto now = std::chrono::steady_clock::now();
            std::chrono::duration<double> elapsed = now - last_update_;
            tokens_ = std::min(capacity_, tokens_ + elapsed.count() * rate_);
            last_update_ = now;
        }

        const double rate_;
        const double capacity_;
        double tokens_;
        std::chrono::steady_clock::time_point last_update_;
    };

    // ... (rest of private members)

    // [NEW] Add the token bucket to the session
    TokenBucket m_rateLimiter;

    // [NEW] Backpressure threshold
    static constexpr size_t WRITE_QUEUE_HIGH_WATER_MARK = 100;
};
```

In the constructor, initialize the `TokenBucket`. A good starting point is to allow a burst of 10 packets and an average rate of 5 packets per second.

**Modified File (`ecs-realm-server/src/network/session.cpp`):**
```cpp
// In Session constructor
Session::Session(tcp::socket socket, boost::asio::ssl::context& context, uint32_t session_id, std::shared_ptr<IPacketHandler> handler)
    : m_ssl_stream(std::move(socket), context),
      m_strand(boost::asio::make_strand(m_ssl_stream.get_executor())),
      m_state(SessionState::Connecting),
      m_sessionId(session_id),
      m_packetHandler(std::move(handler)),
      m_isAuthenticated(false),
      // [NEW] Initialize the rate limiter
      m_rateLimiter(5.0, 10.0) {}
```

### Step 3.2: Apply Rate Limiting to Packet Processing

Now, modify `DoReadBody` to check the token bucket before processing a packet.

**Modified File (`ecs-realm-server/src/network/session.cpp`):**
```cpp
void Session::DoReadBody(uint32_t body_size) {
    // ... (same as before)
    boost::asio::async_read(m_ssl_stream, boost::asio::buffer(m_readBuffer),
        boost::asio::bind_executor(m_strand,
            [self = shared_from_this()](const boost::system::error_code& ec, [[maybe_unused]] std::size_t length) {
                if (!ec) {
                    // [MODIFIED] Apply rate limiting
                    if (self->m_rateLimiter.Consume()) {
                        self->ProcessPacket(std::move(self->m_readBuffer));
                    } else {
                        // Not enough tokens. Log and disconnect the spamming client.
                        LOG_WARN("Session [{}] exceeded rate limit. Disconnecting.", self->m_sessionId);
                        self->Disconnect();
                        return; // Stop processing
                    }
                    self->DoReadHeader(); // Listen for the next packet
                } else {
                    self->HandleError(ec);
                }
            }));
}
```

### Step 3.3: Implement Backpressure on the Write Queue

Modify the `Send` method to check the size of the write queue before adding more data. This is a simple form of backpressure.

**Modified File (`ecs-realm-server/src/network/session.cpp`):**
```cpp
void Session::Send(const google::protobuf::Message& message) {
    auto buffer = PacketSerializer::Serialize(message);
    if (buffer.empty()) return;

    boost::asio::post(m_strand, [this, buffer = std::move(buffer)]() {
        // [MODIFIED] Backpressure check
        if (m_writeQueue.size() > WRITE_QUEUE_HIGH_WATER_MARK) {
            LOG_WARN("Session [{}] write queue is full (>{}) Dropping packet.", m_sessionId, WRITE_QUEUE_HIGH_WATER_MARK);
            // Optionally, send a "ServerBusy" message to the client if this happens often.
            return;
        }

        bool write_in_progress = !m_writeQueue.empty();
        m_writeQueue.push_back(std::move(buffer));
        if (!write_in_progress) {
            DoWrite();
        }
    });
}
```

## 4. Rationale for Changes

*   **Token Bucket Algorithm**: This is a classic, stateless, and efficient algorithm for rate limiting. It allows for short bursts of traffic (up to the bucket capacity) while enforcing an average rate over time, which is ideal for network applications.
*   **Per-Session Limiting**: The rate limiter is a member of the `Session` class, meaning each connected client gets their own bucket. This isolates bad actors without affecting well-behaved clients.
*   **Fail-Fast**: When a client exceeds the rate limit, we disconnect them immediately. This is a simple and effective way to protect the server. A more lenient approach could involve dropping packets for a short period before disconnecting.
*   **Backpressure**: Checking the write queue size (`m_writeQueue.size()`) before enqueuing more data is a simple but effective backpressure mechanism. It prevents a slow client from causing the server to allocate unbounded amounts of memory. The `WRITE_QUEUE_HIGH_WATER_MARK` acts as a safety valve.

## 5. Testing Guide

It is crucial to test these mechanisms. You can add a new test file `tests/unit/test_rate_limiting.cpp`.

1.  **Testing the Token Bucket**: Write a standalone test to verify the `TokenBucket` logic.

    ```cpp
    #include <gtest/gtest.h>
    #include "network/session.h" // Include session.h to access the internal class
    #include <thread>
    #include <chrono>

    // NOTE: You may need to make the TokenBucket class public within session.h for this test
    // or use friend class declaration.
    TEST(TokenBucketTest, RateLimiting) {
        Session::TokenBucket bucket(10.0, 10.0); // 10 tokens/sec, 10 capacity

        // Consume initial burst
        for (int i = 0; i < 10; ++i) {
            EXPECT_TRUE(bucket.Consume());
        }
        EXPECT_FALSE(bucket.Consume()); // Bucket is now empty

        // Wait for half a second to refill 5 tokens
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        for (int i = 0; i < 5; ++i) {
            EXPECT_TRUE(bucket.Consume());
        }
        EXPECT_FALSE(bucket.Consume()); // Bucket is empty again
    }
    ```

2.  **Testing Session Backpressure**: This is harder to unit test and is better suited for an integration test. The approach would be:
    *   Create a mock client that connects to the server but never reads data from its socket.
    *   Have the server send a rapid stream of messages to this client.
    *   Monitor the server logs for the `"write queue is full... Dropping packet."` warning.
    *   Observe the server's memory usage to confirm it does not grow uncontrollably.
