#pragma once

#include <memory>
#include <vector>
#include <queue>
#include <unordered_map>
#include <chrono>
#include <atomic>
#include <boost/asio.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include "../core/types.h"
#include "../core/singleton.h"
#include "packet.h"
#include "connection.h"

namespace mmorpg::network {

// [SEQUENCE: MVP13-1] Advanced packet types
enum class PacketPriority {
    CRITICAL,       // 게임 상태 필수 (전투, 이동)
    HIGH,           // 중요 업데이트 (인벤토리, 스탯)
    NORMAL,         // 일반 업데이트 (채팅, UI)
    LOW,            // 낮은 우선순위 (애니메이션, 이펙트)
    BULK            // 대량 데이터 (맵, 리소스)
};

enum class ReliabilityMode {
    UNRELIABLE,             // UDP, 손실 가능
    UNRELIABLE_SEQUENCED,   // UDP, 순서 보장
    RELIABLE,               // TCP 스타일
    RELIABLE_ORDERED,       // TCP + 순서 보장
    RELIABLE_SEQUENCED      // 최신 것만 보장
};

// [SEQUENCE: MVP13-2] Packet queue entry
struct QueuedPacket {
    PacketPtr packet;
    PacketPriority priority;
    ReliabilityMode reliability;
    std::chrono::steady_clock::time_point queued_time;
    uint32_t retry_count{0};
    uint32_t sequence_number{0};
};

// [SEQUENCE: MVP13-3] Network statistics
struct NetworkStats {
    // Bandwidth
    std::atomic<uint64_t> bytes_sent{0};
    std::atomic<uint64_t> bytes_received{0};
    std::atomic<uint64_t> packets_sent{0};
    std::atomic<uint64_t> packets_received{0};
    
    // Latency
    std::atomic<float> avg_latency_ms{0.0f};
    std::atomic<float> min_latency_ms{999.9f};
    std::atomic<float> max_latency_ms{0.0f};
    std::atomic<float> jitter_ms{0.0f};
    
    // Packet loss
    std::atomic<uint32_t> packets_lost{0};
    std::atomic<float> packet_loss_rate{0.0f};
    
    // Compression
    std::atomic<uint64_t> uncompressed_bytes{0};
    std::atomic<uint64_t> compressed_bytes{0};
    std::atomic<float> compression_ratio{0.0f};
};

// [SEQUENCE: MVP13-4] Advanced connection
class AdvancedConnection : public Connection {
public:
    AdvancedConnection(boost::asio::ip::tcp::socket socket);
    virtual ~AdvancedConnection();
    
    // [SEQUENCE: MVP13-5] Priority-based sending
    void SendPacket(PacketPtr packet, PacketPriority priority = PacketPriority::NORMAL,
                   ReliabilityMode reliability = ReliabilityMode::RELIABLE);
    
    // Bandwidth management
    void SetBandwidthLimit(uint32_t bytes_per_second);
    uint32_t GetBandwidthLimit() const { return bandwidth_limit_; }
    
    // Quality of Service
    void SetQoSLevel(uint8_t level);
    uint8_t GetQoSLevel() const { return qos_level_; }
    
    // Statistics
    NetworkStats GetStats() const { return stats_; }
    void ResetStats();
    
    // Advanced features
    void EnableCompression(bool enable) { compression_enabled_ = enable; }
    void EnableEncryption(bool enable) { encryption_enabled_ = enable; }
    void SetPacketAggregation(bool enable) { packet_aggregation_ = enable; }
    
private:
    // Priority queues
    std::array<std::queue<QueuedPacket>, 5> priority_queues_;
    
    // Bandwidth control
    uint32_t bandwidth_limit_{0};  // 0 = unlimited
    std::chrono::steady_clock::time_point last_send_time_;
    uint64_t bytes_sent_this_second_{0};
    
    // QoS
    uint8_t qos_level_{0};
    
    // Features
    bool compression_enabled_{true};
    bool encryption_enabled_{true};
    bool packet_aggregation_{true};
    
    // Statistics
    NetworkStats stats_;
    
    // Packet processing
    void ProcessOutgoingQueue();
    bool ShouldSendPacket(const QueuedPacket& packet);
    void CompressPacket(PacketPtr& packet);
    void EncryptPacket(PacketPtr& packet);
};

// [SEQUENCE: MVP13-6] Packet aggregator
class PacketAggregator {
public:
    PacketAggregator(uint32_t max_size = 1400);  // MTU safe
    
    // Add packet for aggregation
    bool AddPacket(PacketPtr packet);
    
    // Get aggregated packet
    PacketPtr GetAggregatedPacket();
    
    // Force flush
    PacketPtr Flush();
    
    // Check if should flush
    bool ShouldFlush() const;
    
private:
    std::vector<PacketPtr> pending_packets_;
    uint32_t current_size_{0};
    uint32_t max_size_;
    std::chrono::steady_clock::time_point first_packet_time_;
    
    static constexpr auto MAX_AGGREGATION_TIME = std::chrono::milliseconds(10);
};

// [SEQUENCE: MVP13-7] Interest management
class InterestManager {
public:
    // [SEQUENCE: MVP13-8] Interest calculation
    struct InterestLevel {
        float distance;
        uint8_t priority;
        uint32_t update_rate_ms;
    };
    
    // Calculate interest between entities
    InterestLevel CalculateInterest(const Entity& observer, const Entity& target);
    
    // Update interest sets
    void UpdateInterestSets(uint64_t observer_id, const Vector3& position);
    std::vector<uint64_t> GetInterestedEntities(uint64_t observer_id) const;
    
    // Configuration
    void SetMaxViewDistance(float distance) { max_view_distance_ = distance; }
    void SetMaxInterestSet(uint32_t size) { max_interest_set_ = size; }
    
private:
    struct InterestSet {
        std::vector<uint64_t> entities;
        std::chrono::steady_clock::time_point last_update;
    };
    
    std::unordered_map<uint64_t, InterestSet> interest_sets_;
    float max_view_distance_{200.0f};
    uint32_t max_interest_set_{100};
    
    // Spatial indexing for fast lookups
    std::unordered_map<uint32_t, std::vector<uint64_t>> spatial_hash_;
    
    uint32_t GetSpatialHash(const Vector3& position) const;
};

// [SEQUENCE: MVP13-9] Delta compression
class DeltaCompressor {
public:
    // [SEQUENCE: MVP13-10] State snapshot
    struct StateSnapshot {
        uint32_t tick;
        std::unordered_map<std::string, std::any> values;
        std::chrono::steady_clock::time_point timestamp;
    };
    
    // Create delta between states
    PacketPtr CreateDelta(const StateSnapshot& old_state,
                         const StateSnapshot& new_state);
    
    // Apply delta to state
    void ApplyDelta(StateSnapshot& state, const PacketPtr& delta);
    
    // Store baseline
    void StoreBaseline(uint64_t entity_id, const StateSnapshot& snapshot);
    StateSnapshot* GetBaseline(uint64_t entity_id);
    
private:
    std::unordered_map<uint64_t, StateSnapshot> baselines_;
    
    // Delta encoding helpers
    void EncodeDelta(const std::any& old_value, const std::any& new_value,
                    PacketBuilder& builder);
    std::any DecodeDelta(const std::any& old_value, PacketReader& reader);
};

// [SEQUENCE: MVP13-11] Advanced network manager
class AdvancedNetworkManager : public Singleton<AdvancedNetworkManager> {
public:
    // [SEQUENCE: MVP13-12] Connection management
    void RegisterConnection(std::shared_ptr<AdvancedConnection> connection);
    void UnregisterConnection(uint64_t connection_id);
    std::shared_ptr<AdvancedConnection> GetConnection(uint64_t connection_id);
    
    // [SEQUENCE: MVP13-13] Broadcast optimization
    void BroadcastPacket(PacketPtr packet, const Vector3& origin,
                        float radius = -1.0f,
                        PacketPriority priority = PacketPriority::NORMAL);
    
    void MulticastPacket(PacketPtr packet,
                        const std::vector<uint64_t>& recipients,
                        PacketPriority priority = PacketPriority::NORMAL);
    
    // [SEQUENCE: MVP13-14] Network optimization
    void EnablePacketAggregation(bool enable) { packet_aggregation_enabled_ = enable; }
    void EnableDeltaCompression(bool enable) { delta_compression_enabled_ = enable; }
    void EnableInterestManagement(bool enable) { interest_management_enabled_ = enable; }
    
    // [SEQUENCE: MVP13-15] Bandwidth management
    void SetGlobalBandwidthLimit(uint64_t bytes_per_second);
    void SetPerConnectionLimit(uint32_t bytes_per_second);
    uint64_t GetCurrentBandwidthUsage() const;
    
    // [SEQUENCE: MVP13-16] Lag compensation
    void SetServerTickRate(uint32_t ticks_per_second);
    uint32_t GetServerTick() const { return current_tick_; }
    void RecordPlayerInput(uint64_t player_id, uint32_t tick,
                          const PlayerInput& input);
    
    // Statistics
    struct GlobalNetworkStats {
        uint64_t total_connections{0};
        uint64_t active_connections{0};
        uint64_t total_bandwidth_used{0};
        float average_latency{0.0f};
        float average_packet_loss{0.0f};
        uint64_t packets_aggregated{0};
        uint64_t delta_compressions{0};
    };
    
    GlobalNetworkStats GetGlobalStats() const;
    
private:
    friend class Singleton<AdvancedNetworkManager>;
    AdvancedNetworkManager();
    
    // Connections
    std::unordered_map<uint64_t, std::shared_ptr<AdvancedConnection>> connections_;
    mutable std::shared_mutex connections_mutex_;
    
    // Managers
    std::unique_ptr<InterestManager> interest_manager_;
    std::unique_ptr<DeltaCompressor> delta_compressor_;
    
    // Optimization flags
    std::atomic<bool> packet_aggregation_enabled_{true};
    std::atomic<bool> delta_compression_enabled_{true};
    std::atomic<bool> interest_management_enabled_{true};
    
    // Bandwidth control
    std::atomic<uint64_t> global_bandwidth_limit_{0};
    std::atomic<uint32_t> per_connection_limit_{0};
    std::atomic<uint64_t> current_bandwidth_usage_{0};
    
    // Tick management
    std::atomic<uint32_t> current_tick_{0};
    uint32_t tick_rate_{30};
    std::chrono::steady_clock::time_point last_tick_time_;
    
    // Input history for lag compensation
    struct InputRecord {
        uint32_t tick;
        PlayerInput input;
        std::chrono::steady_clock::time_point timestamp;
    };
    std::unordered_map<uint64_t, std::vector<InputRecord>> input_history_;
    
    // Background processing
    void ProcessNetworkTick();
    void UpdateBandwidthTracking();
    void CleanupOldInputHistory();
};

// [SEQUENCE: MVP13-17] Packet batching
class PacketBatcher {
public:
    PacketBatcher(uint32_t batch_size = 10, 
                 std::chrono::milliseconds timeout = std::chrono::milliseconds(5));
    
    // Add packet to batch
    void AddPacket(uint64_t connection_id, PacketPtr packet);
    
    // Get ready batches
    std::vector<std::pair<uint64_t, std::vector<PacketPtr>>> GetReadyBatches();
    
    // Force flush all batches
    void FlushAll();
    
private:
    struct Batch {
        std::vector<PacketPtr> packets;
        std::chrono::steady_clock::time_point first_packet_time;
    };
    
    std::unordered_map<uint64_t, Batch> batches_;
    uint32_t batch_size_;
    std::chrono::milliseconds timeout_;
};

// [SEQUENCE: MVP13-18] Network optimization utilities
namespace NetworkOptimization {
    // Packet compression
    std::vector<uint8_t> CompressData(const std::vector<uint8_t>& data);
    std::vector<uint8_t> DecompressData(const std::vector<uint8_t>& compressed);
    
    // Bit packing
    class BitPacker {
    public:
        void WriteBits(uint32_t value, uint8_t num_bits);
        void WriteFloat(float value, float min, float max, uint8_t num_bits);
        void WriteVector3(const Vector3& vec, float min, float max, uint8_t bits_per_component);
        
        uint32_t ReadBits(uint8_t num_bits);
        float ReadFloat(float min, float max, uint8_t num_bits);
        Vector3 ReadVector3(float min, float max, uint8_t bits_per_component);
        
        std::vector<uint8_t> GetData() const;
        
    private:
        std::vector<uint8_t> buffer_;
        uint32_t bit_position_{0};
    };
    
    // Priority calculation
    PacketPriority CalculatePriority(const Packet& packet,
                                    const NetworkStats& stats);
    
    // Adaptive quality
    struct QualitySettings {
        uint32_t update_rate;
        uint8_t position_precision;
        bool enable_compression;
        bool enable_aggregation;
    };
    
    QualitySettings AdaptQualityToNetwork(const NetworkStats& stats);
}

// [SEQUENCE: MVP14-829] Reliable UDP implementation
class ReliableUDP {
public:
    ReliableUDP(boost::asio::io_context& io_context, uint16_t port);
    
    // Send with reliability
    void SendReliable(const boost::asio::ip::udp::endpoint& endpoint,
                     const std::vector<uint8_t>& data,
                     uint32_t sequence_number);
    
    // Receive handling
    void StartReceive();
    
    // ACK management
    void SendAck(const boost::asio::ip::udp::endpoint& endpoint,
                uint32_t sequence_number);
    void ProcessAck(uint32_t sequence_number);
    
    // Statistics
    float GetPacketLoss() const;
    float GetAverageRTT() const;
    
private:
    boost::asio::ip::udp::socket socket_;
    
    struct PendingPacket {
        std::vector<uint8_t> data;
        boost::asio::ip::udp::endpoint endpoint;
        std::chrono::steady_clock::time_point send_time;
        uint32_t retry_count{0};
    };
    
    std::unordered_map<uint32_t, PendingPacket> pending_packets_;
    std::unordered_set<uint32_t> received_sequences_;
    
    // RTT tracking
    std::deque<float> rtt_samples_;
    static constexpr size_t MAX_RTT_SAMPLES = 100;
    
    // Packet loss tracking
    uint32_t packets_sent_{0};
    uint32_t packets_lost_{0};
    
    void RetransmitPacket(uint32_t sequence_number);
    void UpdateRTT(float rtt);
};

} // namespace mmorpg::network
