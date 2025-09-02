# 게임 서버 아키텍처 패턴

## 1. 서버 아키텍처 패턴

### Monolithic vs Microservices

**Monolithic 아키텍처**
```
장점:
- 개발과 디버깅이 간단
- 레이턴시가 낮음
- 트랜잭션 처리 용이

단점:
- 확장성 제한
- 부분 장애가 전체 영향
- 배포 리스크 높음

적합한 경우:
- 소규모 게임
- 빠른 프로토타이핑
- 실시간 액션 게임
```

**Microservices 아키텍처**
```
장점:
- 독립적 확장 가능
- 기술 스택 자유도
- 장애 격리

단점:
- 네트워크 오버헤드
- 복잡한 운영
- 분산 트랜잭션 어려움

적합한 경우:
- 대규모 MMO
- 다양한 게임 모드
- 글로벌 서비스
```

### 서버 구성 패턴

**1. 월드 서버 분산**
```cpp
class WorldServer {
    int serverId;
    Region managedRegion;
    vector<Player*> players;
    
    void HandlePlayerTransfer(Player* player, int targetServerId) {
        // 서버 간 플레이어 이동
        SerializePlayerData(player);
        SendToServer(targetServerId, player);
        RemovePlayer(player);
    }
};
```

**2. 채널/룸 기반 서버**
```cpp
class GameRoom {
    int roomId;
    int maxPlayers;
    GameMode mode;
    vector<Player*> players;
    
    void ProcessGameLogic() {
        // 룸 단위 게임 로직
        UpdatePlayers();
        CheckCollisions();
        BroadcastState();
    }
};
```

## 2. 실시간 동기화 아키텍처

### State Synchronization
```cpp
class StateSyncManager {
    void BroadcastWorldState() {
        WorldSnapshot snapshot = CaptureWorldState();
        
        for (auto& player : players) {
            // 관심 영역 필터링
            auto relevantData = FilterByAOI(snapshot, player);
            
            // 델타 압축
            auto delta = ComputeDelta(player->lastSnapshot, relevantData);
            
            // 전송
            SendToPlayer(player, delta);
        }
    }
};
```

### Lockstep Synchronization
```cpp
class LockstepManager {
    int currentFrame;
    map<int, vector<Command>> frameCommands;
    
    void ProcessFrame() {
        // 모든 클라이언트의 입력 대기
        WaitForAllInputs(currentFrame);
        
        // 동시에 처리
        for (auto& cmd : frameCommands[currentFrame]) {
            ExecuteCommand(cmd);
        }
        
        currentFrame++;
    }
};
```

## 3. 데이터 아키텍처

### Event Sourcing Pattern
```cpp
class EventStore {
    struct GameEvent {
        uint64_t timestamp;
        EventType type;
        json payload;
    };
    
    vector<GameEvent> events;
    
    void ApplyEvent(const GameEvent& event) {
        events.push_back(event);
        
        // 이벤트 핸들러 실행
        switch(event.type) {
            case PLAYER_MOVED:
                HandlePlayerMove(event.payload);
                break;
            case ITEM_USED:
                HandleItemUse(event.payload);
                break;
        }
    }
    
    GameState RebuildState(uint64_t timestamp) {
        GameState state;
        for (const auto& event : events) {
            if (event.timestamp <= timestamp) {
                state.Apply(event);
            }
        }
        return state;
    }
};
```

### CQRS Pattern
```cpp
// Command Model
class CommandHandler {
    void HandleAttackCommand(AttackCommand cmd) {
        // 검증
        if (!ValidateAttack(cmd)) return;
        
        // 상태 변경
        ApplyDamage(cmd.targetId, cmd.damage);
        
        // 이벤트 발행
        PublishEvent(AttackEvent{cmd});
    }
};

// Query Model
class QueryHandler {
    PlayerStats GetPlayerStats(int playerId) {
        // 읽기 최적화된 모델에서 조회
        return readDB.GetPlayerStats(playerId);
    }
};
```

## 4. 확장성 패턴

### Sharding Strategy
```cpp
class ShardManager {
    int GetShardId(uint64_t playerId) {
        // 일관된 해싱
        return ConsistentHash(playerId) % totalShards;
    }
    
    void RouteRequest(Request req) {
        int shardId = GetShardId(req.playerId);
        shardServers[shardId]->Handle(req);
    }
};
```

### Actor Model
```cpp
class PlayerActor : public Actor {
    PlayerState state;
    
    void Receive(Message msg) override {
        switch(msg.type) {
            case MOVE:
                state.position = msg.position;
                break;
            case ATTACK:
                auto target = ActorSystem::GetActor(msg.targetId);
                target->Send(TakeDamageMessage{msg.damage});
                break;
        }
    }
};
```

## 5. 성능 최적화 아키텍처

### Object Pooling
```cpp
template<typename T>
class ObjectPool {
    stack<unique_ptr<T>> available;
    vector<unique_ptr<T>> all;
    
    T* Acquire() {
        if (available.empty()) {
            Expand();
        }
        
        auto obj = available.top().release();
        available.pop();
        return obj;
    }
    
    void Release(T* obj) {
        obj->Reset();
        available.push(unique_ptr<T>(obj));
    }
};
```

### Spatial Indexing
```cpp
class QuadTree {
    struct Node {
        Rect bounds;
        vector<GameObject*> objects;
        unique_ptr<Node> children[4];
    };
    
    void Insert(GameObject* obj) {
        InsertRecursive(root.get(), obj);
    }
    
    vector<GameObject*> Query(const Rect& area) {
        vector<GameObject*> result;
        QueryRecursive(root.get(), area, result);
        return result;
    }
};
```

## 6. 실제 사례 분석

### 리그 오브 레전드 서버 구조
- Deterministic Lockstep
- 지역별 서버 클러스터
- 게임 서버와 플랫폼 서버 분리

### 포트나이트 서버 구조
- AWS 기반 자동 스케일링
- 100명 동시 접속 배틀로얄
- 틱 레이트 최적화 (30Hz)

### 월드 오브 워크래프트 서버 구조
- 월드 샤딩
- 레이어링 시스템
- 크로스 렐름 기술

## 면접 대비 포인트

1. **각 패턴의 장단점을 명확히 이해**
2. **실제 적용 경험이나 사례 준비**
3. **트레이드오프 설명 능력**
4. **확장성과 성능의 균형점**
5. **최신 기술 트렌드 파악**