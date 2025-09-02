#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <chrono>
#include <functional>
#include <queue>
#include <future>
#include <array>
#include <random>

namespace mmorpg::network {

// [SEQUENCE: MVP13-80] QUIC 프로토콜 핸들러
class QUICProtocolHandler {
public:
    // QUIC 연결 상태
    enum class ConnectionState {
        INITIAL,
        HANDSHAKE,
        ESTABLISHED,
        CLOSING,
        CLOSED,
        DRAINING
    };

    // QUIC 스트림 타입
    enum class StreamType {
        BIDIRECTIONAL_CLIENT = 0,
        BIDIRECTIONAL_SERVER = 1,
        UNIDIRECTIONAL_CLIENT = 2,
        UNIDIRECTIONAL_SERVER = 3
    };

    // QUIC 패킷 타입
    enum class PacketType {
        INITIAL = 0,
        ZERO_RTT = 1,
        HANDSHAKE = 2,
        RETRY = 3,
        VERSION_NEGOTIATION = 4,
        ONE_RTT = 5
    };

    struct QUICFrame {
        enum class Type {
            PADDING = 0x00,
            PING = 0x01,
            ACK = 0x02,
            RESET_STREAM = 0x04,
            STOP_SENDING = 0x05,
            CRYPTO = 0x06,
            NEW_TOKEN = 0x07,
            STREAM = 0x08,
            MAX_DATA = 0x10,
            MAX_STREAM_DATA = 0x11,
            MAX_STREAMS = 0x12,
            DATA_BLOCKED = 0x14,
            STREAM_DATA_BLOCKED = 0x15,
            STREAMS_BLOCKED = 0x16,
            NEW_CONNECTION_ID = 0x18,
            RETIRE_CONNECTION_ID = 0x19,
            PATH_CHALLENGE = 0x1a,
            PATH_RESPONSE = 0x1b,
            CONNECTION_CLOSE = 0x1c,
            HANDSHAKE_DONE = 0x1e
        };
        
        Type frame_type;
        std::vector<uint8_t> payload;
        uint64_t stream_id{0};
        uint64_t offset{0};
        bool fin{false};
        
        QUICFrame(Type type) : frame_type(type) {}
    };

    struct QUICPacket {
        PacketType packet_type;
        std::array<uint8_t, 8> connection_id;
        uint64_t packet_number;
        std::vector<QUICFrame> frames;
        std::vector<uint8_t> payload;
        std::chrono::steady_clock::time_point timestamp;
        bool is_ack_eliciting{false};
        
        QUICPacket(PacketType type) 
            : packet_type(type), timestamp(std::chrono::steady_clock::now()) {}
    };

    struct QUICConnection {
        std::array<uint8_t, 8> connection_id;
        std::string peer_address;
        uint16_t peer_port;
        ConnectionState state{ConnectionState::INITIAL};
        
        // 패킷 번호 관리
        std::atomic<uint64_t> next_packet_number{0};
        std::atomic<uint64_t> largest_acked_packet{0};
        std::unordered_map<uint64_t, std::chrono::steady_clock::time_point> sent_packets;
        
        // 스트림 관리
        std::unordered_map<uint64_t, std::shared_ptr<QUICStream>> streams;
        std::atomic<uint64_t> next_stream_id{0};
        
        // 흐름 제어
        std::atomic<uint64_t> max_data{1048576}; // 1MB
        std::atomic<uint64_t> data_sent{0};
        std::atomic<uint64_t> data_received{0};
        
        // 혼잡 제어
        std::atomic<uint32_t> congestion_window{10}; // 초기 윈도우 크기
        std::atomic<uint32_t> bytes_in_flight{0};
        std::chrono::steady_clock::time_point last_rtt_sample;
        std::atomic<double> smoothed_rtt_ms{100.0}; // 초기 RTT 100ms
        std::atomic<double> rtt_var_ms{50.0};
        
        // 암호화
        bool handshake_completed{false};
        std::vector<uint8_t> encryption_key;
        std::vector<uint8_t> decryption_key;
        
        std::chrono::steady_clock::time_point created_at;
        std::chrono::steady_clock::time_point last_activity;
        
        QUICConnection() {
            created_at = std::chrono::steady_clock::now();
            last_activity = created_at;
            GenerateConnectionId();
        }
        
        void GenerateConnectionId() {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<uint8_t> dist(0, 255);
            
            for (auto& byte : connection_id) {
                byte = dist(gen);
            }
        }
        
        bool IsExpired(std::chrono::seconds timeout = std::chrono::seconds(30)) const {
            auto now = std::chrono::steady_clock::now();
            return (now - last_activity) > timeout;
        }
    };

    struct QUICStream {
        uint64_t stream_id;
        StreamType type;
        std::queue<std::vector<uint8_t>> send_buffer;
        std::vector<uint8_t> receive_buffer;
        
        std::atomic<uint64_t> max_stream_data{65536}; // 64KB
        std::atomic<uint64_t> bytes_sent{0};
        std::atomic<uint64_t> bytes_received{0};
        std::atomic<uint64_t> next_send_offset{0};
        std::atomic<uint64_t> next_receive_offset{0};
        
        bool send_closed{false};
        bool receive_closed{false};
        
        std::function<void(const std::vector<uint8_t>&)> data_callback;
        std::mutex stream_mutex;
        
        QUICStream(uint64_t id, StreamType t) : stream_id(id), type(t) {}
        
        bool IsBidirectional() const {
            return type == StreamType::BIDIRECTIONAL_CLIENT || type == StreamType::BIDIRECTIONAL_SERVER;
        }
        
        bool IsClientInitiated() const {
            return type == StreamType::BIDIRECTIONAL_CLIENT || type == StreamType::UNIDIRECTIONAL_CLIENT;
        }
    };

    struct QUICConfig {
        uint32_t initial_max_data{1048576}; // 1MB
        uint32_t initial_max_stream_data{65536}; // 64KB
        uint32_t initial_max_streams_bidi{100};
        uint32_t initial_max_streams_uni{100};
        std::chrono::milliseconds idle_timeout{30000}; // 30초
        std::chrono::milliseconds max_ack_delay{25}; // 25ms
        uint32_t max_packet_size{1200}; // 바이트
        bool enable_migration{true};
        bool enable_0rtt{false};
    };

    explicit QUICProtocolHandler(const QUICConfig& config = {})
        : config_(config), is_running_(false) {
        InitializeDefaults();
    }

    ~QUICProtocolHandler() {
        Shutdown();
    }

    // [SEQUENCE: MVP13-81] 연결 관리
    std::shared_ptr<QUICConnection> CreateConnection(const std::string& peer_address, uint16_t peer_port) {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        
        auto connection = std::make_shared<QUICConnection>();
        connection->peer_address = peer_address;
        connection->peer_port = peer_port;
        connection->state = ConnectionState::INITIAL;
        
        std::string conn_key = GetConnectionKey(connection->connection_id);
        connections_[conn_key] = connection;
        
        // 초기 핸드셰이크 시작
        InitiateHandshake(connection);
        
        return connection;
    }

    bool CloseConnection(const std::array<uint8_t, 8>& connection_id, uint64_t error_code = 0) {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        
        std::string conn_key = GetConnectionKey(connection_id);
        auto it = connections_.find(conn_key);
        
        if (it != connections_.end()) {
            auto& connection = it->second;
            connection->state = ConnectionState::CLOSING;
            
            // CONNECTION_CLOSE 프레임 전송
            SendConnectionClose(connection, error_code);
            
            return true;
        }
        
        return false;
    }

    // [SEQUENCE: MVP13-82] 스트림 관리
    std::shared_ptr<QUICStream> CreateStream(const std::array<uint8_t, 8>& connection_id, 
                                           StreamType type = StreamType::BIDIRECTIONAL_CLIENT) {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        
        std::string conn_key = GetConnectionKey(connection_id);
        auto conn_it = connections_.find(conn_key);
        
        if (conn_it == connections_.end() || conn_it->second->state != ConnectionState::ESTABLISHED) {
            return nullptr;
        }
        
        auto& connection = conn_it->second;
        uint64_t stream_id = connection->next_stream_id.fetch_add(4); // QUIC 스트림 ID는 4씩 증가
        
        auto stream = std::make_shared<QUICStream>(stream_id, type);
        connection->streams[stream_id] = stream;
        
        return stream;
    }

    bool SendData(const std::array<uint8_t, 8>& connection_id, uint64_t stream_id, 
                 const std::vector<uint8_t>& data, bool fin = false) {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        
        std::string conn_key = GetConnectionKey(connection_id);
        auto conn_it = connections_.find(conn_key);
        
        if (conn_it == connections_.end()) {
            return false;
        }
        
        auto& connection = conn_it->second;
        auto stream_it = connection->streams.find(stream_id);
        
        if (stream_it == connection->streams.end()) {
            return false;
        }
        
        auto& stream = stream_it->second;
        
        // 흐름 제어 확인
        if (stream->bytes_sent.load() + data.size() > stream->max_stream_data.load()) {
            return false; // 스트림 레벨 흐름 제어 위반
        }
        
        if (connection->data_sent.load() + data.size() > connection->max_data.load()) {
            return false; // 연결 레벨 흐름 제어 위반
        }
        
        // STREAM 프레임 생성 및 전송
        QUICFrame stream_frame(QUICFrame::Type::STREAM);
        stream_frame.stream_id = stream_id;
        stream_frame.offset = stream->next_send_offset.load();
        stream_frame.payload = data;
        stream_frame.fin = fin;
        
        bool success = SendFrame(connection, stream_frame);
        
        if (success) {
            stream->bytes_sent.fetch_add(data.size());
            stream->next_send_offset.fetch_add(data.size());
            connection->data_sent.fetch_add(data.size());
            
            if (fin) {
                stream->send_closed = true;
            }
        }
        
        return success;
    }

    // [SEQUENCE: MVP13-83] 패킷 처리
    bool ProcessIncomingPacket(const std::vector<uint8_t>& packet_data, 
                              const std::string& source_address, uint16_t source_port) {
        if (packet_data.size() < 12) { // 최소 QUIC 패킷 크기
            return false;
        }
        
        auto packet = ParsePacket(packet_data);
        if (!packet) {
            return false;
        }
        
        std::string conn_key = GetConnectionKey(packet->connection_id);
        
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto conn_it = connections_.find(conn_key);
        
        if (conn_it == connections_.end()) {
            // 새 연결인 경우
            if (packet->packet_type == PacketType::INITIAL) {
                auto connection = std::make_shared<QUICConnection>();
                connection->connection_id = packet->connection_id;
                connection->peer_address = source_address;
                connection->peer_port = source_port;
                connection->state = ConnectionState::HANDSHAKE;
                
                connections_[conn_key] = connection;
                return ProcessPacketFrames(connection, packet);
            }
            return false;
        }
        
        auto& connection = conn_it->second;
        connection->last_activity = std::chrono::steady_clock::now();
        
        // 패킷 번호 검증
        if (packet->packet_number <= connection->largest_acked_packet.load()) {
            // 중복 패킷 무시
            return true;
        }
        
        // RTT 계산
        UpdateRTT(connection, packet->packet_number);
        
        return ProcessPacketFrames(connection, packet);
    }

    // [SEQUENCE: MVP13-84] 혼잡 제어 및 흐름 제어
    void UpdateCongestionWindow(std::shared_ptr<QUICConnection> connection, bool packet_lost) {
        auto& cwnd = connection->congestion_window;
        
        if (packet_lost) {
            // 패킷 손실 시 혼잡 윈도우 절반으로 줄임
            cwnd.store(std::max(2u, cwnd.load() / 2));
        } else {
            // 성공적인 ACK 수신 시 윈도우 증가 (Slow Start 또는 Congestion Avoidance)
            if (cwnd.load() < 64) { // Slow Start 임계값
                cwnd.fetch_add(1); // Slow Start: 지수적 증가
            } else {
                // Congestion Avoidance: 선형 증가
                if (std::rand() % cwnd.load() == 0) {
                    cwnd.fetch_add(1);
                }
            }
        }
        
        // 최대 윈도우 크기 제한
        if (cwnd.load() > 1000) {
            cwnd.store(1000);
        }
    }

    // [SEQUENCE: MVP13-85] 0-RTT 지원
    bool Send0RTTData(const std::array<uint8_t, 8>& connection_id, 
                     const std::vector<uint8_t>& application_data) {
        if (!config_.enable_0rtt) {
            return false;
        }
        
        std::lock_guard<std::mutex> lock(connections_mutex_);
        
        std::string conn_key = GetConnectionKey(connection_id);
        auto conn_it = connections_.find(conn_key);
        
        if (conn_it == connections_.end()) {
            return false;
        }
        
        auto& connection = conn_it->second;
        
        // 0-RTT는 이전에 연결된 적이 있는 서버에만 가능
        if (connection->encryption_key.empty()) {
            return false;
        }
        
        // 0-RTT 패킷 생성
        QUICPacket packet(PacketType::ZERO_RTT);
        packet.connection_id = connection_id;
        packet.packet_number = connection->next_packet_number.fetch_add(1);
        
        QUICFrame app_frame(QUICFrame::Type::STREAM);
        app_frame.payload = application_data;
        packet.frames.push_back(app_frame);
        
        return SendPacket(connection, packet);
    }

    // [SEQUENCE: MVP13-86] 연결 마이그레이션
    bool MigrateConnection(const std::array<uint8_t, 8>& connection_id,
                          const std::string& new_address, uint16_t new_port) {
        if (!config_.enable_migration) {
            return false;
        }
        
        std::lock_guard<std::mutex> lock(connections_mutex_);
        
        std::string conn_key = GetConnectionKey(connection_id);
        auto conn_it = connections_.find(conn_key);
        
        if (conn_it == connections_.end()) {
            return false;
        }
        
        auto& connection = conn_it->second;
        
        // PATH_CHALLENGE 프레임 전송으로 새 경로 검증
        QUICFrame challenge_frame(QUICFrame::Type::PATH_CHALLENGE);
        challenge_frame.payload.resize(8);
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint8_t> dist(0, 255);
        
        for (auto& byte : challenge_frame.payload) {
            byte = dist(gen);
        }
        
        bool success = SendFrame(connection, challenge_frame);
        
        if (success) {
            // 성공 시 연결 정보 업데이트
            connection->peer_address = new_address;
            connection->peer_port = new_port;
        }
        
        return success;
    }

    // [SEQUENCE: MVP13-87] 통계 및 모니터링
    struct QUICStats {
        uint64_t total_connections{0};
        uint64_t active_connections{0};
        uint64_t total_streams{0};
        uint64_t active_streams{0};
        
        uint64_t packets_sent{0};
        uint64_t packets_received{0};
        uint64_t packets_lost{0};
        uint64_t bytes_sent{0};
        uint64_t bytes_received{0};
        
        double average_rtt_ms{0.0};
        double packet_loss_rate{0.0};
        uint32_t average_congestion_window{0};
        
        uint64_t zero_rtt_attempts{0};
        uint64_t zero_rtt_successes{0};
        uint64_t connection_migrations{0};
        
        std::chrono::steady_clock::time_point last_updated;
    };

    QUICStats GetStatistics() {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        
        QUICStats stats;
        stats.last_updated = std::chrono::steady_clock::now();
        stats.total_connections = total_connections_.load();
        
        double total_rtt = 0.0;
        uint32_t total_cwnd = 0;
        
        for (const auto& [conn_key, connection] : connections_) {
            if (connection->state == ConnectionState::ESTABLISHED) {
                stats.active_connections++;
                total_rtt += connection->smoothed_rtt_ms.load();
                total_cwnd += connection->congestion_window.load();
                
                stats.active_streams += connection->streams.size();
                stats.bytes_sent += connection->data_sent.load();
                stats.bytes_received += connection->data_received.load();
            }
        }
        
        if (stats.active_connections > 0) {
            stats.average_rtt_ms = total_rtt / stats.active_connections;
            stats.average_congestion_window = total_cwnd / stats.active_connections;
        }
        
        stats.packets_sent = packets_sent_.load();
        stats.packets_received = packets_received_.load();
        stats.packets_lost = packets_lost_.load();
        
        uint64_t total_packets = stats.packets_sent + stats.packets_received;
        if (total_packets > 0) {
            stats.packet_loss_rate = static_cast<double>(stats.packets_lost) / total_packets;
        }
        
        stats.zero_rtt_attempts = zero_rtt_attempts_.load();
        stats.zero_rtt_successes = zero_rtt_successes_.load();
        stats.connection_migrations = connection_migrations_.load();
        
        return stats;
    }

    void StartProtocolHandler() {
        if (is_running_) return;
        
        is_running_ = true;
        
        // 패킷 전송 스레드
        packet_sender_thread_ = std::thread([this]() {
            while (is_running_) {
                ProcessOutgoingPackets();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
        
        // 연결 정리 스레드
        cleanup_thread_ = std::thread([this]() {
            while (is_running_) {
                CleanupExpiredConnections();
                std::this_thread::sleep_for(std::chrono::seconds(10));
            }
        });
    }

    void Shutdown() {
        is_running_ = false;
        
        if (packet_sender_thread_.joinable()) {
            packet_sender_thread_.join();
        }
        
        if (cleanup_thread_.joinable()) {
            cleanup_thread_.join();
        }
        
        // 모든 연결 종료
        std::lock_guard<std::mutex> lock(connections_mutex_);
        for (auto& [conn_key, connection] : connections_) {
            connection->state = ConnectionState::CLOSED;
        }
        connections_.clear();
    }

private:
    QUICConfig config_;
    std::atomic<bool> is_running_;
    
    // 연결 관리
    std::unordered_map<std::string, std::shared_ptr<QUICConnection>> connections_;
    std::mutex connections_mutex_;
    
    // 전송 대기열
    std::queue<std::pair<std::shared_ptr<QUICConnection>, QUICPacket>> outgoing_packets_;
    std::mutex outgoing_mutex_;
    
    // 통계
    std::atomic<uint64_t> total_connections_{0};
    std::atomic<uint64_t> packets_sent_{0};
    std::atomic<uint64_t> packets_received_{0};
    std::atomic<uint64_t> packets_lost_{0};
    std::atomic<uint64_t> zero_rtt_attempts_{0};
    std::atomic<uint64_t> zero_rtt_successes_{0};
    std::atomic<uint64_t> connection_migrations_{0};
    
    // 백그라운드 스레드
    std::thread packet_sender_thread_;
    std::thread cleanup_thread_;

    // [SEQUENCE: MVP13-88] 내부 구현 함수들
    void InitializeDefaults() {
        // 기본 설정 초기화
    }

    std::string GetConnectionKey(const std::array<uint8_t, 8>& connection_id) {
        std::string key;
        key.reserve(16);
        for (const auto& byte : connection_id) {
            key += std::to_string(byte);
        }
        return key;
    }

    std::unique_ptr<QUICPacket> ParsePacket(const std::vector<uint8_t>& data) {
        if (data.size() < 12) return nullptr;
        
        // 간단한 패킷 파싱 (실제로는 더 복잡)
        auto packet = std::make_unique<QUICPacket>(PacketType::ONE_RTT);
        
        // 연결 ID 추출 (예시)
        std::copy(data.begin(), data.begin() + 8, packet->connection_id.begin());
        
        // 패킷 번호 추출 (단순화)
        packet->packet_number = 0;
        for (size_t i = 8; i < 12 && i < data.size(); ++i) {
            packet->packet_number = (packet->packet_number << 8) | data[i];
        }
        
        packets_received_.fetch_add(1);
        return packet;
    }

    bool ProcessPacketFrames(std::shared_ptr<QUICConnection> connection, 
                           std::unique_ptr<QUICPacket>& packet) {
        for (const auto& frame : packet->frames) {
            switch (frame.frame_type) {
                case QUICFrame::Type::STREAM:
                    ProcessStreamFrame(connection, frame);
                    break;
                case QUICFrame::Type::ACK:
                    ProcessAckFrame(connection, frame);
                    break;
                case QUICFrame::Type::CONNECTION_CLOSE:
                    connection->state = ConnectionState::DRAINING;
                    break;
                case QUICFrame::Type::CRYPTO:
                    ProcessCryptoFrame(connection, frame);
                    break;
                default:
                    // 다른 프레임 타입 처리
                    break;
            }
        }
        
        return true;
    }

    void ProcessStreamFrame(std::shared_ptr<QUICConnection> connection, const QUICFrame& frame) {
        auto stream_it = connection->streams.find(frame.stream_id);
        if (stream_it == connection->streams.end()) {
            // 새 스트림 생성
            auto stream = std::make_shared<QUICStream>(frame.stream_id, StreamType::BIDIRECTIONAL_CLIENT);
            connection->streams[frame.stream_id] = stream;
            stream_it = connection->streams.find(frame.stream_id);
        }
        
        auto& stream = stream_it->second;
        
        // 데이터를 수신 버퍼에 추가
        stream->receive_buffer.insert(stream->receive_buffer.end(), 
                                    frame.payload.begin(), frame.payload.end());
        
        stream->bytes_received.fetch_add(frame.payload.size());
        connection->data_received.fetch_add(frame.payload.size());
        
        if (frame.fin) {
            stream->receive_closed = true;
        }
        
        // 콜백 호출
        if (stream->data_callback) {
            stream->data_callback(frame.payload);
        }
    }

    void ProcessAckFrame(std::shared_ptr<QUICConnection> connection, const QUICFrame& frame) {
        // ACK 프레임 처리 (패킷 손실 감지, RTT 업데이트 등)
        // 단순화된 구현
        UpdateCongestionWindow(connection, false);
    }

    void ProcessCryptoFrame(std::shared_ptr<QUICConnection> connection, const QUICFrame& frame) {
        // 암호화 핸드셰이크 처리
        if (!connection->handshake_completed) {
            // 핸드셰이크 완료 시뮬레이션
            connection->handshake_completed = true;
            connection->state = ConnectionState::ESTABLISHED;
            
            // 가짜 암호화 키 생성
            connection->encryption_key.resize(32);
            connection->decryption_key.resize(32);
        }
    }

    void InitiateHandshake(std::shared_ptr<QUICConnection> connection) {
        QUICPacket initial_packet(PacketType::INITIAL);
        initial_packet.connection_id = connection->connection_id;
        initial_packet.packet_number = connection->next_packet_number.fetch_add(1);
        
        QUICFrame crypto_frame(QUICFrame::Type::CRYPTO);
        crypto_frame.payload = std::vector<uint8_t>(32, 0x00); // 가짜 핸드셰이크 데이터
        
        initial_packet.frames.push_back(crypto_frame);
        
        SendPacket(connection, initial_packet);
    }

    void SendConnectionClose(std::shared_ptr<QUICConnection> connection, uint64_t error_code) {
        QUICFrame close_frame(QUICFrame::Type::CONNECTION_CLOSE);
        close_frame.payload.resize(8);
        
        // 에러 코드를 페이로드에 저장
        for (int i = 0; i < 8; ++i) {
            close_frame.payload[i] = (error_code >> (i * 8)) & 0xFF;
        }
        
        SendFrame(connection, close_frame);
    }

    bool SendFrame(std::shared_ptr<QUICConnection> connection, const QUICFrame& frame) {
        QUICPacket packet(PacketType::ONE_RTT);
        packet.connection_id = connection->connection_id;
        packet.packet_number = connection->next_packet_number.fetch_add(1);
        packet.frames.push_back(frame);
        
        return SendPacket(connection, packet);
    }

    bool SendPacket(std::shared_ptr<QUICConnection> connection, const QUICPacket& packet) {
        // 혼잡 제어 확인
        if (connection->bytes_in_flight.load() >= connection->congestion_window.load() * config_.max_packet_size) {
            return false; // 혼잡 윈도우 초과
        }
        
        std::lock_guard<std::mutex> lock(outgoing_mutex_);
        outgoing_packets_.emplace(connection, packet);
        
        connection->bytes_in_flight.fetch_add(packet.payload.size());
        connection->sent_packets[packet.packet_number] = std::chrono::steady_clock::now();
        
        return true;
    }

    void UpdateRTT(std::shared_ptr<QUICConnection> connection, uint64_t acked_packet_number) {
        auto sent_it = connection->sent_packets.find(acked_packet_number);
        if (sent_it == connection->sent_packets.end()) {
            return;
        }
        
        auto now = std::chrono::steady_clock::now();
        auto rtt = std::chrono::duration_cast<std::chrono::microseconds>(now - sent_it->second).count() / 1000.0;
        
        // SRTT 및 RTT 변동 업데이트 (RFC 6298)
        double alpha = 0.125;
        double beta = 0.25;
        
        double current_srtt = connection->smoothed_rtt_ms.load();
        double current_rttvar = connection->rtt_var_ms.load();
        
        if (current_srtt == 0) {
            // 첫 RTT 측정
            connection->smoothed_rtt_ms.store(rtt);
            connection->rtt_var_ms.store(rtt / 2);
        } else {
            double rttvar = current_rttvar * (1 - beta) + beta * std::abs(current_srtt - rtt);
            double srtt = current_srtt * (1 - alpha) + alpha * rtt;
            
            connection->smoothed_rtt_ms.store(srtt);
            connection->rtt_var_ms.store(rttvar);
        }
        
        connection->sent_packets.erase(sent_it);
    }

    void ProcessOutgoingPackets() {
        std::lock_guard<std::mutex> lock(outgoing_mutex_);
        
        while (!outgoing_packets_.empty()) {
            auto& [connection, packet] = outgoing_packets_.front();
            
            // 실제 네트워크 전송 시뮬레이션
            packets_sent_.fetch_add(1);
            
            outgoing_packets_.pop();
        }
    }

    void CleanupExpiredConnections() {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        
        for (auto it = connections_.begin(); it != connections_.end();) {
            if (it->second->IsExpired(config_.idle_timeout)) {
                it = connections_.erase(it);
            } else {
                ++it;
            }
        }
    }
};

} // namespace mmorpg::network