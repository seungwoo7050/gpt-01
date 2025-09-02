# DevHistory 13: Advanced Networking, Part 2

## 1. Advanced Networking System

### Header

```cpp
// [SEQUENCE: MVP13-1] Advanced networking features

// [SEQUENCE: MVP13-2] Network event listener
class INetworkEventListener {
public:
    virtual ~INetworkEventListener() = default;
    virtual void OnPacketSent(const Packet& packet, size_t bytes_sent) = 0;
    virtual void OnPacketReceived(const Packet& packet, size_t bytes_received) = 0;
    virtual void OnConnectionStatusChanged(uint64_t connection_id, ConnectionStatus status) = 0;
    virtual void OnNetworkError(uint64_t connection_id, const std::string& error_message) = 0;
};

// [SEQUENCE: MVP13-3] Network statistics
struct NetworkStats {
    std::atomic<uint64_t> packets_sent{0};
    std::atomic<uint64_t> packets_received{0};
    std::atomic<uint64_t> bytes_sent{0};
    std::atomic<uint64_t> bytes_received{0};
    std::atomic<uint32_t> current_connections{0};
    std::atomic<uint64_t> total_connections{0};
    std::atomic<uint32_t> connection_attempts{0};
    std::atomic<uint32_t> successful_connections{0};
    std::atomic<uint32_t> failed_connections{0};
    std::atomic<double> average_rtt_ms{0.0};
    std::atomic<double> packet_loss_rate{0.0};
    std::chrono::steady_clock::time_point last_updated;
};

// [SEQUENCE: MVP13-4] Packet fragmentation and reassembly
class PacketFragmenter {
public:
    // [SEQUENCE: MVP13-5] Fragmented packet
    struct FragmentedPacket {
        uint32_t packet_id;
        uint16_t fragment_index;
        uint16_t total_fragments;
        std::vector<uint8_t> data;
    };

    std::vector<Packet> Fragment(const Packet& packet, size_t max_fragment_size);
    std::optional<Packet> Reassemble(const FragmentedPacket& fragment);

private:
    struct ReassemblyBuffer {
        uint32_t total_fragments;
        std::vector<std::optional<std::vector<uint8_t>>> fragments;
        std::chrono::steady_clock::time_point creation_time;
    };

    std::unordered_map<uint32_t, ReassemblyBuffer> reassembly_buffers_;
    uint32_t next_packet_id_ = 0;
    static constexpr std::chrono::seconds REASSEMBLY_TIMEOUT = std::chrono::seconds(10);
};

// [SEQUENCE: MVP13-6] Congestion control algorithm
class ICongestionControl {
public:
    virtual ~ICongestionControl() = default;
    virtual void OnPacketSent(size_t bytes) = 0;
    virtual void OnAckReceived(std::chrono::microseconds rtt, size_t bytes_acked) = 0;
    virtual void OnPacketLoss() = 0;
    virtual double GetCongestionWindow() const = 0;
    virtual std::chrono::microseconds GetPacingInterval() const = 0;
};

// [SEQUENCE: MVP13-7] TCP BBR congestion control implementation
class TCPBBRCongestionControl : public ICongestionControl;

// [SEQUENCE: MVP13-8] CUBIC congestion control implementation
class CUBICCongestionControl : public ICongestionControl;

// [SEQUENCE: MVP13-9] Network channel for QoS
enum class NetworkChannel {
    RELIABLE_ORDERED,    // Important game state, RPCs
    UNRELIABLE_ORDERED,  // Player movement, transient state
    UNRELIABLE_UNORDERED // Voice chat, some effects
};

// [SEQUENCE: MVP13-10] Quality of Service (QoS) manager
class QoSManager {
public:
    void SetChannelPriority(NetworkChannel channel, uint8_t priority);
    void SetChannelRateLimit(NetworkChannel channel, size_t bytes_per_second);
    bool CanSend(NetworkChannel channel, size_t packet_size);

private:
    struct ChannelInfo {
        uint8_t priority = 0;
        size_t rate_limit = 0;
        size_t tokens = 0;
        std::chrono::steady_clock::time_point last_token_update;
    };
    std::unordered_map<NetworkChannel, ChannelInfo> channels_;
};

// [SEQUENCE: MVP13-11] Network condition simulator
class NetworkConditionSimulator {
public:
    void SetConfiguration(double latency_ms, double jitter_ms, double packet_loss_rate);
    bool ShouldProcessPacket();
    std::chrono::milliseconds GetAdditionalLatency();

private:
    double latency_ms_ = 0.0;
    double jitter_ms_ = 0.0;
    double packet_loss_rate_ = 0.0;
    std::default_random_engine random_engine_;
};

// [SEQUENCE: MVP13-12] Connection migration support
class ConnectionMigrator {
public:
    void InitiateMigration(uint64_t connection_id, const std::string& new_ip, uint16_t new_port);
    void OnMigrationRequest(const Packet& request_packet);
    void OnMigrationComplete(uint64_t connection_id);

private:
    enum class MigrationState { NONE, IN_PROGRESS, VALIDATING, DONE };
    struct MigrationInfo {
        MigrationState state;
        std::string new_ip;
        uint16_t new_port;
        std::chrono::steady_clock::time_point start_time;
    };
    std::unordered_map<uint64_t, MigrationInfo> migrations_;
};

// [SEQUENCE: MVP13-13] Dynamic MTU discovery
class MTUDiscovery {
public:
    void StartDiscovery(uint64_t connection_id);
    void OnReceivePacket(uint64_t connection_id, size_t packet_size);
    void OnPacketLoss(uint64_t connection_id);
    size_t GetCurrentMTU(uint64_t connection_id) const;

private:
    struct MTUInfo {
        size_t current_mtu = 1200; // Start with a safe default
        size_t max_mtu = 1500;
        size_t min_mtu = 576;
        // Probing state
    };
    std::unordered_map<uint64_t, MTUInfo> mtu_info_;
};

// [SEQUENCE: MVP13-14] Network diagnostics toolkit
class NetworkDiagnostics {
public:
    void LogEvent(const std::string& event_name, const std::string& details);
    void StartTrace(const std::string& trace_name);
    void EndTrace(const std::string& trace_name);
    void DumpReport(const std::string& file_path);

private:
    // Tracing and logging implementation
};

// [SEQUENCE: MVP13-15] Packet batching
class PacketBatcher {
public:
    void AddPacket(Packet&& packet, NetworkChannel channel);
    std::vector<std::vector<uint8_t>> FlushBatches();

private:
    std::unordered_map<NetworkChannel, std::vector<Packet>> batches_;
    size_t max_batch_size_ = 1200;
};

// [SEQUENCE: MVP13-16] Reliable UDP implementation (simplified)
class ReliableUDP {
public:
    void Send(const Packet& packet);
    std::optional<Packet> Receive();
    void Update();

private:
    uint32_t next_sequence_number_ = 0;
    uint32_t expected_sequence_number_ = 0;
    std::deque<Packet> send_buffer_;
    std::map<uint32_t, Packet> receive_buffer_;
    // ACK handling, retransmission logic
};

// [SEQUENCE: MVP13-17] Network topology awareness
class NetworkTopology {
public:
    enum class NodeType { CLIENT, EDGE_SERVER, RELAY, CORE_SERVER };
    struct NodeInfo {
        uint64_t node_id;
        NodeType type;
        std::string region;
        std::vector<uint64_t> connected_nodes;
    };

    void UpdateTopology(const std::vector<NodeInfo>& topology);
    std::vector<uint64_t> GetOptimalRoute(uint64_t from_node, uint64_t to_node);

private:
    std::unordered_map<uint64_t, NodeInfo> nodes_;
};

// [SEQUENCE: MVP13-18] Peer-to-peer (P2P) connection helper
class P2PManager {
public:
    bool InitiateP2P(uint64_t client1_id, uint64_t client2_id);
    void HandleNATTraversal(const Packet& packet);

private:
    // STUN/ICE/TURN logic
};

// [SEQUENCE: MVP13-19] Advanced network interface
class AdvancedNetworkInterface {
public:
    void RegisterEventListener(std::shared_ptr<INetworkEventListener> listener);
    void Send(uint64_t connection_id, const Packet& packet, NetworkChannel channel);
    NetworkStats GetStats() const;
    void SetCongestionControl(uint64_t connection_id, std::unique_ptr<ICongestionControl> algorithm);
    void SimulateNetworkConditions(double latency_ms, double jitter_ms, double packet_loss_rate);
};
```

### Production Considerations

*   **Data Management**: Network statistics (`NetworkStats`) and diagnostic reports (`NetworkDiagnostics`) must be collected and stored centrally. A time-series database (e.g., Prometheus, InfluxDB) is ideal for stats, allowing for monitoring and alerting on key metrics like packet loss, RTT, and connection churn. Diagnostic reports should be archived in a durable object store (like S3) for later analysis of network incidents.
*   **API/Admin Tools**: 
    *   An admin panel should allow real-time viewing of `NetworkStats`.
    *   Provide tools to dynamically adjust QoS settings (`QoSManager`) per-channel for the entire server or specific connections.
    *   Expose controls for the `NetworkConditionSimulator` in staging/dev environments to easily test game performance under adverse conditions.
    *   An endpoint to trigger a `NetworkDiagnostics` report dump for a specific server instance.
    *   Tools to manually inspect and clear `PacketFragmenter` reassembly buffers to diagnose potential memory leak issues.
*   **Performance/Scalability**: 
    *   The `PacketFragmenter`'s reassembly buffer can become a memory bottleneck if many fragmented packets are lost. A strict, memory-capped cleanup mechanism is crucial to prevent memory exhaustion.
    *   Congestion control algorithms (`TCPBBRCongestionControl`, `CUBICCongestionControl`) can be CPU-intensive. Their performance should be profiled under heavy load. Consider offloading this to a dedicated network thread.
    *   `PacketBatcher` is critical for reducing the number of system calls for sending packets, which improves throughput. The batch flush interval and size must be tuned based on server tick rate and traffic patterns.
    *   The `NetworkTopology` route calculation could be slow if the graph is large. It should be pre-calculated and cached, only updating when the topology changes.
*   **Security**: 
    *   The `ConnectionMigrator` introduces a potential attack vector. Migration requests must be strictly authenticated and validated to prevent connection hijacking. The process should involve a cryptographic challenge-response exchange.
    *   The `P2PManager` must implement robust NAT traversal security (ICE) to prevent IP address spoofing and other P2P-related attacks. All P2P traffic should ideally be encrypted.
    *   The `NetworkConditionSimulator` must be completely disabled in production builds to prevent accidental or malicious activation.

---

## 2. Client Prediction System

### Header

```cpp
// [SEQUENCE: MVP13-28] Client-side prediction system

// [SEQUENCE: MVP13-29] Player input structure
struct PlayerInput;

// [SEQUENCE: MVP13-30] Predicted state of an entity
struct PredictedState;

// [SEQUENCE: MVP13-31] Authoritative state from server
struct AuthoritativeState;

// [SEQUENCE: MVP13-32] Client prediction core class
class ClientPrediction;

// [SEQUENCE: MVP13-33] State interpolator for smooth rendering
class StateInterpolator;

// [SEQUENCE: MVP13-34] Prediction utilities
namespace PredictionUtils;

// [SEQUENCE: MVP13-35] Ability prediction
class AbilityPredictor;

// [SEQUENCE: MVP13-36] Ability prediction result
struct PredictionResult;

// [SEQUENCE: MVP13-37] Movement predictor
class MovementPredictor;

// [SEQUENCE: MVP13-38] Client-side prediction handler
class ClientPredictionHandler;

// [SEQUENCE: MVP13-39] Prediction utilities
namespace PredictionUtils;

// [SEQUENCE: MVP13-40] Ability prediction
class AbilityPredictor;

// [SEQUENCE: MVP13-41] Ability prediction result
struct PredictionResult;

// [SEQUENCE: MVP13-42] Movement predictor
class MovementPredictor;
```

### Production Considerations

*   **Data Management**: Server needs to store a short history of client inputs and resulting authoritative states to validate incoming client actions and perform reconciliation. This data should be stored in a fast, in-memory cache (like Redis) or a ring buffer on the game server itself, with a clear time-to-live (TTL) to prevent memory growth.
*   **API/Admin Tools**:
    *   A debug command to view a player's prediction state: input buffer, state history, and reconciliation statistics (`ClientPrediction::PredictionStats`).
    *   Tools to visualize prediction errors, showing the server's authoritative path vs. the client's predicted path.
    *   Ability to toggle different interpolation modes (`StateInterpolator::InterpolationMode`) for a client to debug visual artifacts.
    *   An endpoint to inspect the `AbilityPredictor`'s cooldown and ability data to ensure client and server are in sync.
*   **Performance/Scalability**:
    *   The server-side reconciliation process, which involves replaying client inputs, can be CPU-intensive. This process must be highly optimized and potentially rate-limited to prevent a single misbehaving client from overloading a server core.
    *   The size of the state history (`state_history_`) and input buffers (`input_buffer_`) must be strictly capped to prevent unbounded memory usage per player.
    *   Input compression (`PredictionUtils::CompressInput`) is vital for reducing bandwidth, but the CPU cost of compression/decompression must be monitored.
*   **Security**:
    *   The server must perform rigorous validation on all client inputs. The `MovementPredictor` on the server side acts as a sanity check against speed hacking and teleportation. Any significant deviation from the server's predicted movement should be flagged and potentially trigger a disconnect.
    *   The `AbilityPredictor` on the server must be the source of truth for cooldowns and mana costs. Client predictions are for responsiveness only; the server must re-validate every ability cast request against its own state to prevent cheating.

---

## 3. Lag Compensation System

### Header

```cpp
// [SEQUENCE: MVP13-51] World snapshot for time rewind
struct WorldSnapshot;

// [SEQUENCE: MVP13-52] Hit validation result
struct HitValidation;

// [SEQUENCE: MVP13-53] Lag compensation system
class LagCompensation : public Singleton<LagCompensation>;

// [SEQUENCE: MVP13-54] Snapshot management

// [SEQUENCE: MVP13-55] Hit validation

// [SEQUENCE: MVP13-56] Movement validation

// [SEQUENCE: MVP13-57] Rewind context for temporary state
class RewindContext;

// [SEQUENCE: MVP13-58] Hit registration system
class HitRegistration;

// [SEQUENCE: MVP13-59] Hit request from client
struct HitRequest;

// [SEQUENCE: MVP13-60] Interpolation utilities
namespace InterpolationUtils;

// [SEQUENCE: MVP13-61] Favor the shooter settings
struct FavorTheShooterSettings;

// [SEQUENCE: MVP13-62] Advanced lag compensation manager
class AdvancedLagCompensation;

// [SEQUENCE: MVP13-63] Predictive lag compensation
struct PredictedHit;

// [SEQUENCE: MVP13-64] Rollback networking
class RollbackNetworking;

// [SEQUENCE: MVP13-65] Rollback state
struct RollbackState;

// [SEQUENCE: MVP13-66] Lag compensation utilities
namespace LagCompensationUtils;
```

### Production Considerations

*   **Data Management**: `WorldSnapshot` storage is extremely memory-intensive. The number of snapshots (`MAX_SNAPSHOTS`) and the data stored within each must be carefully managed. Storing only essential data (ID, position, hitbox, state) is critical. Snapshots should be stored in memory on the game server instance where they are generated and never serialized to disk or a database in real-time.
*   **API/Admin Tools**:
    *   A debug command to visualize a lag compensation event. This would draw the rewound hitboxes, the raycast, and the server's view of the world at the time of the shot.
    *   Admin panel to tweak `FavorTheShooterSettings` in real-time to find a good balance for the live player base.
    *   An interface to view `LagCompensationStats` to monitor the frequency of rejected hits and the reasons, which can help identify cheating or high-latency players.
    *   Tools to inspect the `RollbackNetworking` state history for a given player to debug desynchronization issues.
*   **Performance/Scalability**:
    *   The process of rewinding time and performing raycasts against historical world states (`RewindContext::PerformRaycast`) is the primary performance bottleneck. The number of entities and the complexity of their hitboxes directly impact CPU usage. Optimizing the spatial partitioning of entities within snapshots is crucial.
    *   The `snapshots_` deque must have a hard memory limit to prevent a server from running out of memory under load or due to a bug.
    *   `RollbackNetworking` is even more performance-intensive than standard lag compensation, as it involves resimulating entire frames. This approach is typically only suitable for small-scale encounters (e.g., 1v1 fights) and may not scale to large-scale battles without significant optimization.
*   **Security**:
    *   The server must be the ultimate authority. Client-side hit requests (`HitRequest`) are just that—requests. The server must perform its own complete validation using lag compensation.
    *   Timestamps from clients (`HitRequest::timestamp`) cannot be fully trusted. The server should use its own latency measurements and sanity checks (`LagCompensationUtils::IsTimestampValid`) to prevent clients from manipulating time to their advantage.
    *   Movement validation (`LagCompensation::ValidateMovement`) is a critical anti-cheat measure. It must be robust enough to detect and prevent teleportation and speed hacks by comparing client movement against the server's physics simulation capabilities.

---

## 4. QUIC Protocol Handler

### Header

```cpp
// [SEQUENCE: MVP13-80] QUIC 프로토콜 핸들러
class QUICProtocolHandler;

// [SEQUENCE: MVP13-81] 연결 관리

// [SEQUENCE: MVP13-82] 스트림 관리

// [SEQUENCE: MVP13-83] 패킷 처리

// [SEQUENCE: MVP13-84] 혼잡 제어 및 흐름 제어

// [SEQUENCE: MVP13-85] 0-RTT 지원

// [SEQUENCE: MVP13-86] 연결 마이그레이션

// [SEQUENCE: MVP13-87] 통계 및 모니터링

// [SEQUENCE: MVP13-88] 내부 구현 함수들
```

### Production Considerations

*   **Data Management**: `QUICStats` should be exported to a centralized monitoring system (e.g., Prometheus) to track the health of the transport layer across the entire fleet. Tracking metrics like `packet_loss_rate`, `average_rtt_ms`, and `connection_migrations` is vital for network health assessment.
*   **API/Admin Tools**:
    *   An admin interface to view live `QUICStats` for any server instance.
    *   A command to inspect the state of a specific `QUICConnection`, including its congestion window, RTT, and active streams.
    *   Tools to dynamically adjust `QUICConfig` settings, such as idle timeout and max packet size, without a server restart.
    *   Ability to manually trigger and test `MigrateConnection` in a staging environment.
*   **Performance/Scalability**:
    *   The cryptographic operations (handshake, packet encryption/decryption) are the most CPU-intensive part of QUIC. Using hardware acceleration (AES-NI) is essential for performance. The choice of crypto library can have a significant impact.
    *   Managing thousands of concurrent connections and their states (congestion windows, RTT, streams) requires efficient memory management. The `QUICConnection` and `QUICStream` objects must be as lightweight as possible.
    *   The background threads (`packet_sender_thread_`, `cleanup_thread_`) must be carefully designed to avoid lock contention, especially on the `connections_mutex_`.
*   **Security**:
    *   The implementation must be robust against amplification attacks, especially during the handshake process. The server should not send more data than it has received before the client's address is validated.
    *   Connection migration (`MigrateConnection`) requires strong validation (PATH_CHALLENGE/RESPONSE) to prevent an attacker from hijacking a valid connection.
    *   0-RTT data, if enabled, is vulnerable to replay attacks. The application layer protocol built on top of QUIC must implement its own replay protection mechanism for 0-RTT data.

---

## 5. Global Load Balancer

### Header

```cpp
// [SEQUENCE: MVP13-89] 글로벌 로드 밸런서
class GlobalLoadBalancer;

// [SEQUENCE: MVP13-90] 서버 노드 관리

// [SEQUENCE: MVP13-91] 클라이언트 요청 라우팅

// [SEQUENCE: MVP13-92] 지역별 서버 스케일링

// [SEQUENCE: MVP13-93] 로드 밸런서 통계

// [SEQUENCE: MVP13-94] 로드 밸런싱 전략 구현

// [SEQUENCE: MVP13-95] 개별 로드 밸런싱 전략들

// [SEQUENCE: MVP13-96] 유틸리티 함수들
```

### Production Considerations

*   **Data Management**: The load balancer's state (server lists, client locations) is critical infrastructure. This state should be persisted in a highly available database (like Consul, etcd, or a replicated Redis cluster) so that the load balancer can be stateless and restarted without losing routing information.
*   **API/Admin Tools**:
    *   A dashboard to visualize the `LoadBalancerStats`, showing a world map with server locations, their current load, and client connection distribution.
    *   An interface to manually register, unregister, or place a `ServerNode` into maintenance mode.
    *   Controls to adjust `LoadBalancerConfig` parameters, such as changing the primary routing strategy or tweaking scaling thresholds, on the fly.
    *   A tool to query the routing decision for a specific client ID or IP address to debug routing issues.
*   **Performance/Scalability**:
    *   The load balancer itself can become a single point of failure. It must be designed for high availability, running multiple instances with a failover mechanism.
    *   The `RouteClient` function is on the critical path for every new player connection. It must be extremely fast. GeoIP lookups (`EstimateClientLocation`) should be performed against a local, in-memory database, not a slow external service.
    *   The health checking mechanism (`PerformHealthChecks`) can generate significant network traffic if the number of servers is large. Health checks should be efficient and potentially distributed across multiple load balancer instances.
*   **Security**:
    *   The API endpoints for managing the load balancer (registering servers, updating metrics) must be strongly authenticated and authorized to prevent malicious actors from adding rogue servers or taking legitimate ones out of rotation.
    *   The GeoIP database needs to be updated regularly but also vetted to ensure its integrity.
    *   The load balancer must be protected against DDoS attacks, as it is a primary entry point to the entire game infrastructure.
