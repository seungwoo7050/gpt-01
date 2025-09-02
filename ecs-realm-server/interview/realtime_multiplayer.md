# 실시간 멀티플레이어 구현

## 1. 네트워크 아키텍처

### 클라이언트-서버 모델

**권위 서버 (Authoritative Server) 구현:**
```cpp
class AuthoritativeGameServer {
    // 서버가 모든 게임 상태의 권한을 가짐
    class GameWorld {
        unordered_map<PlayerId, PlayerState> players;
        unordered_map<EntityId, GameObject> gameObjects;
        
        void ProcessPlayerInput(PlayerId playerId, const InputCommand& input) {
            // 1. 입력 검증
            if (!ValidateInput(playerId, input)) {
                SendInvalidInputResponse(playerId);
                return;
            }
            
            // 2. 서버에서 시뮬레이션
            auto& player = players[playerId];
            SimulatePlayerAction(player, input);
            
            // 3. 결과를 모든 클라이언트에 전파
            StateUpdate update;
            update.playerId = playerId;
            update.newState = player;
            update.timestamp = GetServerTime();
            update.inputSequence = input.sequence;
            
            BroadcastStateUpdate(update);
        }
        
        // 결정적 시뮬레이션
        void SimulatePlayerAction(PlayerState& player, const InputCommand& input) {
            switch (input.type) {
                case InputType::MOVE:
                    player.position += input.direction * player.speed * FIXED_TIMESTEP;
                    break;
                case InputType::ATTACK:
                    ProcessAttack(player, input.targetId);
                    break;
            }
        }
    };
};
```

### P2P vs 전용 서버

**하이브리드 아키텍처:**
```cpp
class HybridNetworkArchitecture {
    // 매치메이킹은 전용 서버
    class MatchmakingServer {
        void CreateMatch(const vector<PlayerId>& players) {
            // 호스트 선정 (가장 좋은 네트워크 조건)
            PlayerId host = SelectBestHost(players);
            
            // 매치 정보 생성
            MatchInfo match;
            match.hostId = host;
            match.players = players;
            match.sessionToken = GenerateSessionToken();
            
            // 모든 플레이어에게 매치 정보 전송
            for (auto playerId : players) {
                SendMatchInfo(playerId, match);
            }
        }
    };
    
    // 게임플레이는 P2P (호스트 마이그레이션 지원)
    class P2PGameSession {
        PlayerId currentHost;
        vector<PlayerId> peers;
        
        void MigrateHost() {
            // 현재 호스트 연결 끊김
            if (!IsPlayerConnected(currentHost)) {
                // 새 호스트 선정
                PlayerId newHost = SelectNewHost();
                
                // 상태 동기화
                GameState state = GatherStateFromPeers();
                
                // 새 호스트에게 권한 이전
                TransferHostAuthority(newHost, state);
                
                currentHost = newHost;
            }
        }
    };
};
```

## 2. 상태 동기화

### 스냅샷 보간

**클라이언트 측 보간:**
```cpp
class SnapshotInterpolation {
    struct Snapshot {
        uint32_t tick;
        float serverTime;
        unordered_map<EntityId, EntityState> entities;
    };
    
    circular_buffer<Snapshot> snapshotBuffer{10};
    
    void RenderInterpolatedState(float currentTime) {
        // 렌더링 시간 = 현재 시간 - 보간 지연
        float renderTime = currentTime - INTERPOLATION_DELAY;
        
        // 렌더링 시간을 감싸는 두 스냅샷 찾기
        Snapshot* before = nullptr;
        Snapshot* after = nullptr;
        
        for (size_t i = 1; i < snapshotBuffer.size(); i++) {
            if (snapshotBuffer[i-1].serverTime <= renderTime && 
                snapshotBuffer[i].serverTime > renderTime) {
                before = &snapshotBuffer[i-1];
                after = &snapshotBuffer[i];
                break;
            }
        }
        
        if (before && after) {
            // 선형 보간
            float t = (renderTime - before->serverTime) / 
                     (after->serverTime - before->serverTime);
            
            for (const auto& [id, beforeState] : before->entities) {
                if (after->entities.count(id)) {
                    const auto& afterState = after->entities[id];
                    
                    EntityState interpolated;
                    interpolated.position = Lerp(beforeState.position, afterState.position, t);
                    interpolated.rotation = Slerp(beforeState.rotation, afterState.rotation, t);
                    
                    RenderEntity(id, interpolated);
                }
            }
        }
    }
};
```

### 델타 압축

**효율적인 상태 전송:**
```cpp
class DeltaCompression {
    struct DeltaFrame {
        uint32_t baseFrame;
        uint32_t currentFrame;
        vector<EntityDelta> deltas;
    };
    
    struct EntityDelta {
        EntityId id;
        bitset<32> changedFields;
        
        optional<Vector3> position;
        optional<Quaternion> rotation;
        optional<float> health;
        // ... 기타 필드
    };
    
    DeltaFrame CreateDelta(const GameState& previous, const GameState& current) {
        DeltaFrame delta;
        delta.baseFrame = previous.frame;
        delta.currentFrame = current.frame;
        
        // 변경된 엔티티만 포함
        for (const auto& [id, entity] : current.entities) {
            if (!previous.entities.count(id)) {
                // 새 엔티티
                EntityDelta d;
                d.id = id;
                d.changedFields.set();  // 모든 필드 변경
                d.position = entity.position;
                d.rotation = entity.rotation;
                d.health = entity.health;
                delta.deltas.push_back(d);
            } else {
                const auto& prev = previous.entities.at(id);
                EntityDelta d;
                d.id = id;
                
                // 변경된 필드만 설정
                if (entity.position != prev.position) {
                    d.changedFields.set(0);
                    d.position = entity.position;
                }
                if (entity.rotation != prev.rotation) {
                    d.changedFields.set(1);
                    d.rotation = entity.rotation;
                }
                if (entity.health != prev.health) {
                    d.changedFields.set(2);
                    d.health = entity.health;
                }
                
                if (d.changedFields.any()) {
                    delta.deltas.push_back(d);
                }
            }
        }
        
        return delta;
    }
    
    // 비트 패킹으로 추가 압축
    vector<uint8_t> SerializeDelta(const DeltaFrame& delta) {
        BitWriter writer;
        
        writer.WriteUInt(delta.baseFrame, 16);
        writer.WriteUInt(delta.currentFrame, 16);
        writer.WriteUInt(delta.deltas.size(), 10);
        
        for (const auto& d : delta.deltas) {
            writer.WriteUInt(d.id, 16);
            writer.WriteBits(d.changedFields);
            
            if (d.position) {
                // 위치는 고정소수점으로
                writer.WriteInt(d.position->x * 100, 16);
                writer.WriteInt(d.position->y * 100, 16);
                writer.WriteInt(d.position->z * 100, 16);
            }
            
            if (d.rotation) {
                // 쿼터니언 압축 (최대 성분 제외)
                WriteCompressedQuaternion(writer, *d.rotation);
            }
            
            if (d.health) {
                writer.WriteUInt(*d.health, 8);
            }
        }
        
        return writer.GetData();
    }
};
```

## 3. 입력 처리

### 클라이언트 예측

**예측 및 롤백:**
```cpp
class ClientPrediction {
    struct InputBuffer {
        uint32_t sequence;
        InputCommand command;
        PlayerState resultState;
    };
    
    circular_buffer<InputBuffer> unacknowledgedInputs{100};
    PlayerState localPlayerState;
    PlayerState lastServerState;
    
    void ProcessLocalInput(const InputCommand& input) {
        // 1. 즉시 로컬 시뮬레이션
        localPlayerState = SimulateInput(localPlayerState, input);
        
        // 2. 서버로 전송
        input.sequence = nextSequence++;
        SendInputToServer(input);
        
        // 3. 미확인 입력 버퍼에 저장
        unacknowledgedInputs.push_back({input.sequence, input, localPlayerState});
    }
    
    void ReceiveServerUpdate(const ServerUpdate& update) {
        lastServerState = update.playerState;
        uint32_t lastAcknowledged = update.lastProcessedInput;
        
        // 1. 확인된 입력 제거
        while (!unacknowledgedInputs.empty() && 
               unacknowledgedInputs.front().sequence <= lastAcknowledged) {
            unacknowledgedInputs.pop_front();
        }
        
        // 2. 서버 상태에서 시작하여 미확인 입력 재적용
        PlayerState correctedState = lastServerState;
        
        for (const auto& input : unacknowledgedInputs) {
            correctedState = SimulateInput(correctedState, input.command);
        }
        
        // 3. 부드러운 오차 보정
        Vector3 error = correctedState.position - localPlayerState.position;
        
        if (error.Length() > SNAP_THRESHOLD) {
            // 오차가 크면 즉시 보정
            localPlayerState = correctedState;
        } else {
            // 작은 오차는 부드럽게
            localPlayerState.position += error * CORRECTION_RATE;
        }
    }
};
```

### 입력 버퍼링

**네트워크 지터 대응:**
```cpp
class InputBuffering {
    struct TimedInput {
        InputCommand command;
        float timestamp;
        uint32_t frame;
    };
    
    class JitterBuffer {
        static constexpr float TARGET_DELAY = 100.0f;  // 100ms
        
        priority_queue<TimedInput, vector<TimedInput>, 
                      greater<TimedInput>> buffer;
        float adaptiveDelay = TARGET_DELAY;
        
        void AddInput(const TimedInput& input) {
            buffer.push(input);
            
            // 적응형 지연 조정
            UpdateAdaptiveDelay();
        }
        
        optional<InputCommand> GetInput(float currentTime) {
            float processTime = currentTime - adaptiveDelay;
            
            if (!buffer.empty() && buffer.top().timestamp <= processTime) {
                auto input = buffer.top().command;
                buffer.pop();
                return input;
            }
            
            return nullopt;
        }
        
        void UpdateAdaptiveDelay() {
            // 버퍼 상태에 따라 지연 조정
            size_t bufferSize = buffer.size();
            
            if (bufferSize > 10) {
                // 버퍼가 쌓이면 지연 증가
                adaptiveDelay = min(adaptiveDelay * 1.1f, 200.0f);
            } else if (bufferSize < 2) {
                // 버퍼가 비면 지연 감소
                adaptiveDelay = max(adaptiveDelay * 0.9f, 50.0f);
            }
        }
    };
};
```

## 4. 대역폭 최적화

### 관심 영역 관리 (Area of Interest)

**동적 AOI 시스템:**
```cpp
class AreaOfInterestManager {
    struct AOICell {
        set<EntityId> entities;
        set<PlayerId> observers;
    };
    
    unordered_map<CellId, AOICell> grid;
    
    void UpdatePlayerAOI(Player& player) {
        // 플레이어 시야 범위 계산
        float viewDistance = GetViewDistance(player);
        
        // 관심 셀 계산
        set<CellId> newCells = GetCellsInRadius(player.position, viewDistance);
        set<CellId> oldCells = player.observedCells;
        
        // 새로 들어온 셀
        set<CellId> enterCells;
        set_difference(newCells.begin(), newCells.end(),
                      oldCells.begin(), oldCells.end(),
                      inserter(enterCells, enterCells.begin()));
        
        // 나간 셀
        set<CellId> exitCells;
        set_difference(oldCells.begin(), oldCells.end(),
                      newCells.begin(), newCells.end(),
                      inserter(exitCells, exitCells.begin()));
        
        // 엔티티 업데이트 전송
        for (CellId cell : enterCells) {
            for (EntityId entity : grid[cell].entities) {
                SendEntityEnter(player.id, entity);
            }
            grid[cell].observers.insert(player.id);
        }
        
        for (CellId cell : exitCells) {
            for (EntityId entity : grid[cell].entities) {
                SendEntityExit(player.id, entity);
            }
            grid[cell].observers.erase(player.id);
        }
        
        player.observedCells = newCells;
    }
    
    // 우선순위 기반 업데이트
    void PrioritizeUpdates(Player& player, vector<EntityUpdate>& updates) {
        sort(updates.begin(), updates.end(), [&](const auto& a, const auto& b) {
            float distA = Distance(player.position, a.position);
            float distB = Distance(player.position, b.position);
            
            // 가까운 엔티티 우선
            if (abs(distA - distB) > 10.0f) {
                return distA < distB;
            }
            
            // 중요도 고려
            return a.importance > b.importance;
        });
        
        // 대역폭 제한
        size_t maxUpdates = GetMaxUpdatesPerFrame(player.bandwidth);
        if (updates.size() > maxUpdates) {
            updates.resize(maxUpdates);
        }
    }
};
```

### 프로토콜 최적화

**효율적인 패킷 구조:**
```cpp
class NetworkProtocol {
    // 메시지 타입별 최적화
    enum class MessageType : uint8_t {
        POSITION_UPDATE = 0,
        FULL_STATE = 1,
        INPUT = 2,
        ACTION = 3,
        CHAT = 4
    };
    
    // 고빈도 메시지 최적화
    struct PositionUpdate {
        uint16_t entityId;
        
        // 고정소수점 좌표 (0.01 정밀도)
        int16_t x;  // -327.68 ~ 327.67
        int16_t y;
        int16_t z;
        
        // 압축된 회전 (2바이트)
        uint16_t rotation;  // yaw만 전송, 360도를 65536으로 매핑
        
        // 속도 (선택적)
        optional<int8_t> velocityX;
        optional<int8_t> velocityY;
    };  // 최소 8바이트, 최대 10바이트
    
    // 비트 플래그로 선택적 필드 표시
    struct CompressedUpdate {
        uint16_t entityId;
        uint8_t flags;  // 어떤 필드가 포함되었는지
        
        void Serialize(BitWriter& writer) {
            writer.WriteUInt(entityId, 16);
            writer.WriteUInt(flags, 8);
            
            if (flags & FLAG_POSITION) {
                writer.WriteInt(x, 16);
                writer.WriteInt(y, 16);
                writer.WriteInt(z, 16);
            }
            
            if (flags & FLAG_ROTATION) {
                writer.WriteUInt(rotation, 16);
            }
            
            if (flags & FLAG_VELOCITY) {
                writer.WriteInt(velocityX, 8);
                writer.WriteInt(velocityY, 8);
            }
        }
    };
    
    // 메시지 배칭
    class MessageBatcher {
        vector<Message> pendingMessages;
        chrono::steady_clock::time_point lastFlush;
        
        void AddMessage(const Message& msg) {
            pendingMessages.push_back(msg);
            
            // 조건부 전송
            if (ShouldFlush()) {
                Flush();
            }
        }
        
        bool ShouldFlush() {
            // 크기 제한
            if (GetTotalSize() > MAX_PACKET_SIZE) return true;
            
            // 시간 제한
            auto now = chrono::steady_clock::now();
            if (now - lastFlush > MAX_BATCH_DELAY) return true;
            
            // 중요 메시지
            if (HasHighPriorityMessage()) return true;
            
            return false;
        }
        
        void Flush() {
            if (pendingMessages.empty()) return;
            
            // 단일 패킷으로 합치기
            Packet packet;
            packet.WriteUInt(pendingMessages.size(), 8);
            
            for (const auto& msg : pendingMessages) {
                packet.WriteUInt(msg.type, 4);
                packet.WriteData(msg.data);
            }
            
            Send(packet);
            pendingMessages.clear();
            lastFlush = chrono::steady_clock::now();
        }
    };
};
```

## 5. 신뢰성과 순서 보장

### 신뢰성 있는 UDP

**선택적 신뢰성:**
```cpp
class ReliableUDP {
    enum class ReliabilityType {
        UNRELIABLE,           // 손실 허용
        RELIABLE,             // 재전송 보장
        RELIABLE_ORDERED,     // 순서 보장
        RELIABLE_SEQUENCED    // 최신 것만
    };
    
    struct ReliablePacket {
        uint32_t sequence;
        uint32_t ack;
        uint32_t ackBits;  // 최근 32개 패킷 수신 여부
        
        ReliabilityType reliability;
        uint8_t channel;    // 채널별 순서 보장
        
        vector<uint8_t> payload;
        
        // 재전송 관리
        int sendCount = 0;
        chrono::steady_clock::time_point lastSendTime;
    };
    
    class Connection {
        uint32_t localSequence = 0;
        uint32_t remoteSequence = 0;
        
        map<uint32_t, ReliablePacket> sentPackets;
        map<uint32_t, ReliablePacket> receivedPackets;
        
        void SendPacket(const vector<uint8_t>& data, ReliabilityType reliability) {
            ReliablePacket packet;
            packet.sequence = localSequence++;
            packet.ack = remoteSequence;
            packet.ackBits = GenerateAckBits();
            packet.reliability = reliability;
            packet.payload = data;
            
            if (reliability != ReliabilityType::UNRELIABLE) {
                sentPackets[packet.sequence] = packet;
            }
            
            TransmitPacket(packet);
        }
        
        void ProcessAcks(uint32_t ack, uint32_t ackBits) {
            // 최신 ACK 처리
            if (sentPackets.count(ack)) {
                sentPackets.erase(ack);
            }
            
            // 비트마스크로 이전 32개 처리
            for (int i = 0; i < 32; i++) {
                if (ackBits & (1 << i)) {
                    uint32_t seq = ack - i - 1;
                    sentPackets.erase(seq);
                }
            }
        }
        
        void RetransmitReliablePackets() {
            auto now = chrono::steady_clock::now();
            
            for (auto& [seq, packet] : sentPackets) {
                auto elapsed = now - packet.lastSendTime;
                
                if (elapsed > CalculateRTO()) {
                    packet.sendCount++;
                    packet.lastSendTime = now;
                    
                    if (packet.sendCount > MAX_RETRIES) {
                        // 연결 끊김 처리
                        HandleConnectionLost();
                    } else {
                        TransmitPacket(packet);
                    }
                }
            }
        }
    };
};
```

## 면접 대비 포인트

1. **네트워크 기초**: TCP/UDP 차이, 레이턴시/대역폭 트레이드오프
2. **동기화 방식**: State sync, Lockstep, Rollback의 장단점
3. **최적화 기법**: 압축, 보간, 예측의 구현과 효과
4. **실전 경험**: 실제 겪은 네트워크 이슈와 해결 방법
5. **확장성**: 동시접속자 증가에 따른 아키텍처 변화