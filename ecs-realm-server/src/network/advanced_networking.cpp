#include "advanced_networking.h"
#include "../core/logger.h"
#include <spdlog/spdlog.h>
#include <zlib.h>
#include <algorithm>
#include <cmath>

namespace mmorpg::network {

// [SEQUENCE: MVP13-20] Advanced connection implementation
AdvancedConnection::AdvancedConnection(boost::asio::ip::tcp::socket socket)
    : Connection(std::move(socket)) {
    
    // Initialize priority queues
    for (auto& queue : priority_queues_) {
        queue = std::queue<QueuedPacket>();
    }
    
    spdlog::debug("[Network] Advanced connection created");
}

AdvancedConnection::~AdvancedConnection() {
    // Log final statistics
    spdlog::info("[Network] Connection closed - Stats: sent={} KB, recv={} KB, loss={}%",
                stats_.bytes_sent.load() / 1024,
                stats_.bytes_received.load() / 1024,
                stats_.packet_loss_rate.load());
}

// [SEQUENCE: MVP13-21] Priority-based packet sending
void AdvancedConnection::SendPacket(PacketPtr packet, PacketPriority priority,
                                   ReliabilityMode reliability) {
    if (!packet || !IsConnected()) {
        return;
    }
    
    // Create queued packet
    QueuedPacket queued;
    queued.packet = packet;
    queued.priority = priority;
    queued.reliability = reliability;
    queued.queued_time = std::chrono::steady_clock::now();
    
    // Add to appropriate queue
    priority_queues_[static_cast<int>(priority)].push(std::move(queued));
    
    // Process queue
    ProcessOutgoingQueue();
}

void AdvancedConnection::ProcessOutgoingQueue() {
    auto now = std::chrono::steady_clock::now();
    
    // Check bandwidth limit
    if (bandwidth_limit_ > 0) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - last_send_time_).count();
        
        if (elapsed >= 1000) {
            bytes_sent_this_second_ = 0;
            last_send_time_ = now;
        }
        
        if (bytes_sent_this_second_ >= bandwidth_limit_) {
            return;  // Bandwidth limit reached
        }
    }
    
    // Process queues by priority
    for (int p = 0; p < 5; ++p) {
        auto& queue = priority_queues_[p];
        
        while (!queue.empty() && ShouldSendPacket(queue.front())) {
            auto& queued = queue.front();
            auto packet = queued.packet;
            
            // Apply compression if enabled
            if (compression_enabled_) {
                CompressPacket(packet);
            }
            
            // Apply encryption if enabled
            if (encryption_enabled_) {
                EncryptPacket(packet);
            }
            
            // Send packet
            uint32_t packet_size = packet->GetSize();
            Connection::Send(packet);
            
            // Update statistics
            stats_.packets_sent++;
            stats_.bytes_sent += packet_size;
            bytes_sent_this_second_ += packet_size;
            
            queue.pop();
            
            // Check bandwidth again
            if (bandwidth_limit_ > 0 && bytes_sent_this_second_ >= bandwidth_limit_) {
                break;
            }
        }
    }
}

bool AdvancedConnection::ShouldSendPacket(const QueuedPacket& packet) {
    // Check age for timeout
    auto age = std::chrono::steady_clock::now() - packet.queued_time;
    if (age > std::chrono::seconds(5)) {
        return false;  // Too old, drop it
    }
    
    // QoS checks
    if (qos_level_ > 0) {
        // Higher QoS levels have stricter requirements
        // TODO: Implement QoS logic
    }
    
    return true;
}

void AdvancedConnection::CompressPacket(PacketPtr& packet) {
    auto data = packet->GetData();
    auto compressed = NetworkOptimization::CompressData(data);
    
    if (compressed.size() < data.size()) {
        // Update statistics
        stats_.uncompressed_bytes += data.size();
        stats_.compressed_bytes += compressed.size();
        stats_.compression_ratio = static_cast<float>(stats_.compressed_bytes) / 
                                  stats_.uncompressed_bytes;
        
        // Replace packet data
        packet->SetData(compressed);
        packet->SetFlag(PacketFlags::COMPRESSED);
    }
}

void AdvancedConnection::EncryptPacket(PacketPtr& packet) {
    // TODO: Implement encryption
    packet->SetFlag(PacketFlags::ENCRYPTED);
}

// [SEQUENCE: MVP13-22] Packet aggregator implementation
PacketAggregator::PacketAggregator(uint32_t max_size)
    : max_size_(max_size) {
}

bool PacketAggregator::AddPacket(PacketPtr packet) {
    if (!packet) {
        return false;
    }
    
    uint32_t packet_size = packet->GetSize();
    
    // Check if packet fits
    if (current_size_ + packet_size + 4 > max_size_) {  // +4 for size header
        return false;
    }
    
    // Add packet
    pending_packets_.push_back(packet);
    current_size_ += packet_size + 4;
    
    // Record first packet time
    if (pending_packets_.size() == 1) {
        first_packet_time_ = std::chrono::steady_clock::now();
    }
    
    return true;
}

PacketPtr PacketAggregator::GetAggregatedPacket() {
    if (pending_packets_.empty()) {
        return nullptr;
    }
    
    // Create aggregated packet
    auto aggregated = std::make_shared<Packet>(PacketType::AGGREGATED);
    PacketBuilder builder(aggregated);
    
    // Write packet count
    builder.Write<uint16_t>(pending_packets_.size());
    
    // Write each packet
    for (const auto& packet : pending_packets_) {
        auto data = packet->GetData();
        builder.Write<uint32_t>(data.size());
        builder.WriteBytes(data.data(), data.size());
    }
    
    // Clear pending
    pending_packets_.clear();
    current_size_ = 0;
    
    return aggregated;
}

bool PacketAggregator::ShouldFlush() const {
    if (pending_packets_.empty()) {
        return false;
    }
    
    // Check time limit
    auto elapsed = std::chrono::steady_clock::now() - first_packet_time_;
    if (elapsed >= MAX_AGGREGATION_TIME) {
        return true;
    }
    
    // Check size limit (80% full)
    if (current_size_ >= max_size_ * 0.8f) {
        return true;
    }
    
    return false;
}

// [SEQUENCE: MVP13-23] Interest manager implementation
InterestManager::InterestLevel InterestManager::CalculateInterest(
    const Entity& observer, const Entity& target) {
    
    InterestLevel level;
    
    // Calculate distance
    float distance = Vector3::Distance(observer.GetPosition(), target.GetPosition());
    level.distance = distance;
    
    // Determine priority based on distance and entity type
    if (distance < 20.0f) {
        level.priority = 5;  // Very close
        level.update_rate_ms = 33;  // 30 FPS
    } else if (distance < 50.0f) {
        level.priority = 4;  // Close
        level.update_rate_ms = 66;  // 15 FPS
    } else if (distance < 100.0f) {
        level.priority = 3;  // Medium
        level.update_rate_ms = 100;  // 10 FPS
    } else if (distance < 150.0f) {
        level.priority = 2;  // Far
        level.update_rate_ms = 200;  // 5 FPS
    } else if (distance < max_view_distance_) {
        level.priority = 1;  // Very far
        level.update_rate_ms = 500;  // 2 FPS
    } else {
        level.priority = 0;  // Out of range
        level.update_rate_ms = 0;  // No updates
    }
    
    // Adjust for entity importance
    if (target.GetType() == EntityType::PLAYER) {
        level.priority = std::min<uint8_t>(level.priority + 1, 5);
    } else if (target.GetType() == EntityType::BOSS) {
        level.priority = std::min<uint8_t>(level.priority + 2, 5);
    }
    
    return level;
}

void InterestManager::UpdateInterestSets(uint64_t observer_id, const Vector3& position) {
    auto& interest_set = interest_sets_[observer_id];
    interest_set.entities.clear();
    
    // Get spatial hash for position
    uint32_t hash = GetSpatialHash(position);
    
    // Check neighboring cells
    std::vector<std::pair<uint64_t, float>> candidates;
    
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dz = -1; dz <= 1; ++dz) {
                Vector3 offset(dx * 50.0f, dy * 50.0f, dz * 50.0f);
                uint32_t neighbor_hash = GetSpatialHash(position + offset);
                
                auto it = spatial_hash_.find(neighbor_hash);
                if (it != spatial_hash_.end()) {
                    for (uint64_t entity_id : it->second) {
                        if (entity_id != observer_id) {
                            // Calculate distance for sorting
                            // TODO: Get actual entity position
                            float dist = 0.0f;  // Placeholder
                            candidates.emplace_back(entity_id, dist);
                        }
                    }
                }
            }
        }
    }
    
    // Sort by distance
    std::sort(candidates.begin(), candidates.end(),
              [](const auto& a, const auto& b) { return a.second < b.second; });
    
    // Take top N entities
    size_t count = std::min(candidates.size(), static_cast<size_t>(max_interest_set_));
    for (size_t i = 0; i < count; ++i) {
        interest_set.entities.push_back(candidates[i].first);
    }
    
    interest_set.last_update = std::chrono::steady_clock::now();
}

uint32_t InterestManager::GetSpatialHash(const Vector3& position) const {
    // Simple spatial hashing
    const float cell_size = 50.0f;
    int x = static_cast<int>(position.x / cell_size);
    int y = static_cast<int>(position.y / cell_size);
    int z = static_cast<int>(position.z / cell_size);
    
    // Combine into hash
    return (x * 73856093) ^ (y * 19349663) ^ (z * 83492791);
}

// [SEQUENCE: MVP13-24] Delta compressor implementation
PacketPtr DeltaCompressor::CreateDelta(const StateSnapshot& old_state,
                                      const StateSnapshot& new_state) {
    
    auto delta_packet = std::make_shared<Packet>(PacketType::DELTA_UPDATE);
    PacketBuilder builder(delta_packet);
    
    // Write tick information
    builder.Write<uint32_t>(old_state.tick);
    builder.Write<uint32_t>(new_state.tick);
    
    // Count changed fields
    uint16_t changed_count = 0;
    auto count_pos = builder.GetPosition();
    builder.Write<uint16_t>(0);  // Placeholder
    
    // Compare states
    for (const auto& [key, new_value] : new_state.values) {
        auto old_it = old_state.values.find(key);
        
        bool changed = false;
        if (old_it == old_state.values.end()) {
            changed = true;  // New field
        } else {
            // Compare values (simplified)
            // TODO: Implement proper comparison for different types
            changed = true;  // Assume changed for now
        }
        
        if (changed) {
            // Write field name
            builder.WriteString(key);
            
            // Encode delta
            if (old_it != old_state.values.end()) {
                EncodeDelta(old_it->second, new_value, builder);
            } else {
                // Full value for new fields
                builder.Write<uint8_t>(0xFF);  // Special marker
                // TODO: Write full value
            }
            
            changed_count++;
        }
    }
    
    // Update count
    builder.WriteAt(count_pos, changed_count);
    
    return delta_packet;
}

void DeltaCompressor::EncodeDelta(const std::any& old_value, const std::any& new_value,
                                 PacketBuilder& builder) {
    // Type-specific delta encoding
    if (old_value.type() == typeid(float) && new_value.type() == typeid(float)) {
        float old_f = std::any_cast<float>(old_value);
        float new_f = std::any_cast<float>(new_value);
        float delta = new_f - old_f;
        
        // Quantize delta
        int16_t quantized = static_cast<int16_t>(delta * 100.0f);
        builder.Write<uint8_t>(1);  // Type: float delta
        builder.Write<int16_t>(quantized);
    }
    else if (old_value.type() == typeid(Vector3) && new_value.type() == typeid(Vector3)) {
        Vector3 old_v = std::any_cast<Vector3>(old_value);
        Vector3 new_v = std::any_cast<Vector3>(new_value);
        Vector3 delta = new_v - old_v;
        
        // Quantize delta
        builder.Write<uint8_t>(2);  // Type: vector delta
        builder.Write<int16_t>(static_cast<int16_t>(delta.x * 100.0f));
        builder.Write<int16_t>(static_cast<int16_t>(delta.y * 100.0f));
        builder.Write<int16_t>(static_cast<int16_t>(delta.z * 100.0f));
    }
    else {
        // Fallback: send full value
        builder.Write<uint8_t>(0);  // Type: full value
        // TODO: Serialize full value
    }
}

// [SEQUENCE: MVP13-25] Advanced network manager implementation
AdvancedNetworkManager::AdvancedNetworkManager()
    : interest_manager_(std::make_unique<InterestManager>())
    , delta_compressor_(std::make_unique<DeltaCompressor>()) {
    
    last_tick_time_ = std::chrono::steady_clock::now();
    
    spdlog::info("[Network] Advanced network manager initialized");
}

void AdvancedNetworkManager::RegisterConnection(
    std::shared_ptr<AdvancedConnection> connection) {
    
    if (!connection) {
        return;
    }
    
    uint64_t id = connection->GetId();
    
    {
        std::unique_lock lock(connections_mutex_);
        connections_[id] = connection;
    }
    
    // Apply default limits
    if (per_connection_limit_ > 0) {
        connection->SetBandwidthLimit(per_connection_limit_);
    }
    
    spdlog::debug("[Network] Registered connection {}", id);
}

void AdvancedNetworkManager::BroadcastPacket(PacketPtr packet, const Vector3& origin,
                                            float radius, PacketPriority priority) {
    if (!packet) {
        return;
    }
    
    std::shared_lock lock(connections_mutex_);
    
    for (const auto& [id, connection] : connections_) {
        if (!connection || !connection->IsConnected()) {
            continue;
        }
        
        // Check radius if specified
        if (radius > 0) {
            // TODO: Get player position for connection
            // For now, send to all
        }
        
        // Send with priority
        connection->SendPacket(packet, priority);
    }
}

void AdvancedNetworkManager::ProcessNetworkTick() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - last_tick_time_).count();
    
    if (elapsed >= 1000 / tick_rate_) {
        current_tick_++;
        last_tick_time_ = now;
        
        // Update bandwidth tracking
        UpdateBandwidthTracking();
        
        // Cleanup old input history
        CleanupOldInputHistory();
    }
}

void AdvancedNetworkManager::UpdateBandwidthTracking() {
    uint64_t total_usage = 0;
    
    std::shared_lock lock(connections_mutex_);
    for (const auto& [id, connection] : connections_) {
        if (connection) {
            auto stats = connection->GetStats();
            total_usage += stats.bytes_sent.load();
        }
    }
    
    current_bandwidth_usage_ = total_usage;
}

void AdvancedNetworkManager::CleanupOldInputHistory() {
    const uint32_t max_history_ticks = tick_rate_ * 2;  // 2 seconds
    
    for (auto& [player_id, history] : input_history_) {
        // Remove old entries
        history.erase(
            std::remove_if(history.begin(), history.end(),
                          [this, max_history_ticks](const InputRecord& record) {
                              return current_tick_ - record.tick > max_history_ticks;
                          }),
            history.end());
    }
}

// [SEQUENCE: MVP13-26] Network optimization utilities
namespace NetworkOptimization {

std::vector<uint8_t> CompressData(const std::vector<uint8_t>& data) {
    if (data.empty()) {
        return data;
    }
    
    // Use zlib compression
    uLongf compressed_size = compressBound(data.size());
    std::vector<uint8_t> compressed(compressed_size);
    
    int result = compress2(compressed.data(), &compressed_size,
                          data.data(), data.size(),
                          Z_BEST_SPEED);  // Fast compression
    
    if (result == Z_OK) {
        compressed.resize(compressed_size);
        return compressed;
    }
    
    // Compression failed, return original
    return data;
}

std::vector<uint8_t> DecompressData(const std::vector<uint8_t>& compressed) {
    if (compressed.empty()) {
        return compressed;
    }
    
    // Estimate decompressed size (could be stored in header)
    uLongf decompressed_size = compressed.size() * 10;  // Guess 10x
    std::vector<uint8_t> decompressed(decompressed_size);
    
    int result = uncompress(decompressed.data(), &decompressed_size,
                           compressed.data(), compressed.size());
    
    if (result == Z_OK) {
        decompressed.resize(decompressed_size);
        return decompressed;
    }
    
    // Decompression failed
    return {};
}

void BitPacker::WriteBits(uint32_t value, uint8_t num_bits) {
    // Ensure we have enough space
    while (buffer_.size() * 8 < bit_position_ + num_bits) {
        buffer_.push_back(0);
    }
    
    // Write bits
    for (uint8_t i = 0; i < num_bits; ++i) {
        uint32_t byte_index = bit_position_ / 8;
        uint8_t bit_index = bit_position_ % 8;
        
        if (value & (1 << i)) {
            buffer_[byte_index] |= (1 << bit_index);
        }
        
        bit_position_++;
    }
}

void BitPacker::WriteFloat(float value, float min, float max, uint8_t num_bits) {
    // Normalize to 0-1
    float normalized = (value - min) / (max - min);
    normalized = std::clamp(normalized, 0.0f, 1.0f);
    
    // Quantize
    uint32_t max_value = (1 << num_bits) - 1;
    uint32_t quantized = static_cast<uint32_t>(normalized * max_value + 0.5f);
    
    WriteBits(quantized, num_bits);
}

void BitPacker::WriteVector3(const Vector3& vec, float min, float max,
                            uint8_t bits_per_component) {
    WriteFloat(vec.x, min, max, bits_per_component);
    WriteFloat(vec.y, min, max, bits_per_component);
    WriteFloat(vec.z, min, max, bits_per_component);
}

PacketPriority CalculatePriority(const Packet& packet, const NetworkStats& stats) {
    // Base priority on packet type
    PacketPriority priority = PacketPriority::NORMAL;
    
    switch (packet.GetType()) {
        case PacketType::POSITION_UPDATE:
        case PacketType::COMBAT_ACTION:
            priority = PacketPriority::CRITICAL;
            break;
            
        case PacketType::INVENTORY_UPDATE:
        case PacketType::STAT_UPDATE:
            priority = PacketPriority::HIGH;
            break;
            
        case PacketType::CHAT_MESSAGE:
        case PacketType::UI_UPDATE:
            priority = PacketPriority::NORMAL;
            break;
            
        case PacketType::ANIMATION:
        case PacketType::EFFECT:
            priority = PacketPriority::LOW;
            break;
            
        default:
            break;
    }
    
    // Adjust based on network conditions
    if (stats.packet_loss_rate > 0.05f) {  // > 5% loss
        // Increase priority for critical packets
        if (priority == PacketPriority::CRITICAL) {
            // Keep critical
        } else if (priority == PacketPriority::HIGH) {
            priority = PacketPriority::CRITICAL;
        }
    }
    
    return priority;
}

QualitySettings AdaptQualityToNetwork(const NetworkStats& stats) {
    QualitySettings settings;
    
    // Default high quality
    settings.update_rate = 30;
    settings.position_precision = 16;
    settings.enable_compression = true;
    settings.enable_aggregation = true;
    
    // Adapt based on latency
    if (stats.avg_latency_ms > 150.0f) {
        settings.update_rate = 20;
        settings.position_precision = 12;
    } else if (stats.avg_latency_ms > 250.0f) {
        settings.update_rate = 15;
        settings.position_precision = 10;
    }
    
    // Adapt based on packet loss
    if (stats.packet_loss_rate > 0.05f) {
        settings.enable_aggregation = false;  // Don't risk losing multiple packets
    }
    
    // Adapt based on bandwidth
    if (stats.bytes_sent + stats.bytes_received < 10000) {  // < 10KB/s
        settings.enable_compression = false;  // CPU cost not worth it
    }
    
    return settings;
}

} // namespace NetworkOptimization

// [SEQUENCE: MVP13-27] Reliable UDP implementation
ReliableUDP::ReliableUDP(boost::asio::io_context& io_context, uint16_t port)
    : socket_(io_context, boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), port)) {
    
    StartReceive();
}

void ReliableUDP::SendReliable(const boost::asio::ip::udp::endpoint& endpoint,
                               const std::vector<uint8_t>& data,
                               uint32_t sequence_number) {
    // Store pending packet
    PendingPacket pending;
    pending.data = data;
    pending.endpoint = endpoint;
    pending.send_time = std::chrono::steady_clock::now();
    pending_packets_[sequence_number] = pending;
    
    // Send packet
    socket_.async_send_to(
        boost::asio::buffer(data), endpoint,
        [this, sequence_number](boost::system::error_code ec, std::size_t) {
            if (!ec) {
                packets_sent_++;
            }
        });
    
    // Schedule retransmission
    // TODO: Implement timer-based retransmission
}

void ReliableUDP::ProcessAck(uint32_t sequence_number) {
    auto it = pending_packets_.find(sequence_number);
    if (it != pending_packets_.end()) {
        // Calculate RTT
        auto rtt = std::chrono::steady_clock::now() - it->second.send_time;
        float rtt_ms = std::chrono::duration<float, std::milli>(rtt).count();
        UpdateRTT(rtt_ms);
        
        // Remove from pending
        pending_packets_.erase(it);
    }
}

void ReliableUDP::UpdateRTT(float rtt) {
    rtt_samples_.push_back(rtt);
    if (rtt_samples_.size() > MAX_RTT_SAMPLES) {
        rtt_samples_.pop_front();
    }
}

float ReliableUDP::GetAverageRTT() const {
    if (rtt_samples_.empty()) {
        return 0.0f;
    }
    
    float sum = 0.0f;
    for (float rtt : rtt_samples_) {
        sum += rtt;
    }
    
    return sum / rtt_samples_.size();
}

float ReliableUDP::GetPacketLoss() const {
    if (packets_sent_ == 0) {
        return 0.0f;
    }
    
    return static_cast<float>(packets_lost_) / packets_sent_;
}

} // namespace mmorpg::network
