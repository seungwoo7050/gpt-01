# 실전 문제 해결 사례

## 1. 성능 문제 해결

### 케이스 1: 서버 렉 문제

**문제 상황:**
"동시접속 5000명 이상에서 서버 응답 시간이 1초 이상으로 증가"

**분석 과정:**
```cpp
// 1. 프로파일링으로 병목 지점 발견
class WorldManager {
    void BroadcastToNearbyPlayers(Player& source, Packet& packet) {
        // 문제 코드: O(n²) 복잡도
        for (auto& player : allPlayers) {
            float distance = CalculateDistance(source, player);
            if (distance < BROADCAST_RANGE) {
                player.SendPacket(packet);
            }
        }
    }
};
```

**해결 방법:**
```cpp
// 공간 분할 구조 도입
class SpatialGrid {
    static constexpr int CELL_SIZE = 100;
    unordered_map<int, vector<Player*>> grid;
    
    int GetCellIndex(const Vector3& pos) {
        int x = pos.x / CELL_SIZE;
        int z = pos.z / CELL_SIZE;
        return x * 1000 + z;  // 간단한 해싱
    }
    
    void BroadcastOptimized(Player& source, Packet& packet) {
        int sourceCell = GetCellIndex(source.position);
        
        // 주변 9개 셀만 확인
        for (int dx = -1; dx <= 1; dx++) {
            for (int dz = -1; dz <= 1; dz++) {
                int cellIndex = sourceCell + dx * 1000 + dz;
                
                for (auto& player : grid[cellIndex]) {
                    if (Distance(source, *player) < BROADCAST_RANGE) {
                        player->SendPacket(packet);
                    }
                }
            }
        }
    }
};
```

**결과:**
- 응답 시간: 1000ms → 50ms (95% 개선)
- CPU 사용률: 90% → 40%
- 동시접속 한계: 5000명 → 20000명

### 케이스 2: 메모리 누수

**문제 상황:**
"서버가 24시간 운영 후 메모리 사용량이 32GB 초과"

**분석 도구 활용:**
```cpp
// Valgrind로 누수 탐지
// valgrind --leak-check=full ./game_server

// 커스텀 메모리 추적
class MemoryTracker {
    struct AllocationInfo {
        size_t size;
        string file;
        int line;
        chrono::time_point<chrono::steady_clock> timestamp;
    };
    
    unordered_map<void*, AllocationInfo> allocations;
    
    void* TrackedAlloc(size_t size, const char* file, int line) {
        void* ptr = malloc(size);
        allocations[ptr] = {size, file, line, chrono::steady_clock::now()};
        return ptr;
    }
    
    void TrackedFree(void* ptr) {
        allocations.erase(ptr);
        free(ptr);
    }
    
    void ReportLeaks() {
        for (const auto& [ptr, info] : allocations) {
            cout << "Leak: " << info.size << " bytes at " 
                 << info.file << ":" << info.line << endl;
        }
    }
};

#define NEW(type) new (MemoryTracker::TrackedAlloc(sizeof(type), __FILE__, __LINE__)) type
#define DELETE(ptr) MemoryTracker::TrackedFree(ptr); delete ptr
```

**발견된 문제:**
```cpp
// 이벤트 핸들러에서 누수
class EventSystem {
    multimap<EventType, function<void(Event&)>> handlers;
    
    void Subscribe(EventType type, function<void(Event&)> handler) {
        handlers.emplace(type, handler);
        // 문제: Unsubscribe 없이 계속 누적
    }
};

// 해결: 자동 정리 메커니즘
class ScopedEventSubscription {
    EventSystem* system;
    EventType type;
    multimap<EventType, function<void(Event&)>>::iterator it;
    
public:
    ~ScopedEventSubscription() {
        if (system) {
            system->handlers.erase(it);
        }
    }
};
```

### 케이스 3: 데드락 문제

**문제 상황:**
"간헐적으로 서버가 멈추고 모든 스레드가 대기 상태"

**디버깅 과정:**
```cpp
// GDB로 스레드 상태 확인
// (gdb) info threads
// (gdb) thread apply all bt

// 데드락 탐지 코드
class DeadlockDetector {
    struct LockInfo {
        thread::id owner;
        string name;
        chrono::time_point<chrono::steady_clock> acquiredTime;
    };
    
    map<mutex*, LockInfo> lockMap;
    mutex mapMutex;
    
    void BeforeLock(mutex* m, const string& name) {
        lock_guard<mutex> guard(mapMutex);
        
        // 순환 대기 감지
        if (WouldCauseDeadlock(m)) {
            throw runtime_error("Potential deadlock detected!");
        }
    }
};
```

**발견된 문제:**
```cpp
// 잘못된 락 순서
void TransferItem(Player& from, Player& to, Item& item) {
    lock_guard<mutex> lock1(from.mutex);  // A→B 순서
    lock_guard<mutex> lock2(to.mutex);
    // 동시에 다른 스레드에서 B→A 순서로 락 시도
}

// 해결: 일관된 락 순서
void TransferItemSafe(Player& from, Player& to, Item& item) {
    // ID 기반으로 항상 같은 순서
    if (from.id < to.id) {
        lock_guard<mutex> lock1(from.mutex);
        lock_guard<mutex> lock2(to.mutex);
        DoTransfer();
    } else {
        lock_guard<mutex> lock1(to.mutex);
        lock_guard<mutex> lock2(from.mutex);
        DoTransfer();
    }
}
```

## 2. 네트워크 문제 해결

### 케이스 4: 패킷 로스 대응

**문제 상황:**
"해외 유저들이 스킬 사용 시 간헐적으로 무시됨"

**분석:**
```cpp
// 패킷 로스 측정
class NetworkAnalyzer {
    struct PacketInfo {
        uint32_t sequenceNumber;
        chrono::time_point<chrono::steady_clock> sentTime;
        bool acknowledged = false;
    };
    
    map<uint32_t, PacketInfo> sentPackets;
    
    float GetPacketLossRate() {
        int total = sentPackets.size();
        int lost = count_if(sentPackets.begin(), sentPackets.end(),
            [](const auto& p) { return !p.second.acknowledged; });
        return (float)lost / total * 100;
    }
};
```

**해결 방안:**
```cpp
// 1. 중요 패킷 재전송
class ReliableUDP {
    void SendImportantPacket(Packet& packet) {
        packet.sequenceNumber = nextSeq++;
        packet.timestamp = chrono::steady_clock::now();
        
        // 전송 및 재전송 큐에 추가
        SendPacket(packet);
        retransmissionQueue[packet.sequenceNumber] = packet;
        
        // 재전송 타이머 설정
        SetTimer(packet.sequenceNumber, 100ms);
    }
    
    void OnAck(uint32_t sequenceNumber) {
        retransmissionQueue.erase(sequenceNumber);
        CancelTimer(sequenceNumber);
    }
    
    void OnTimeout(uint32_t sequenceNumber) {
        if (retransmissionQueue.count(sequenceNumber)) {
            auto& packet = retransmissionQueue[sequenceNumber];
            if (packet.retryCount < MAX_RETRIES) {
                packet.retryCount++;
                SendPacket(packet);
                SetTimer(sequenceNumber, 200ms);  // 백오프
            }
        }
    }
};

// 2. 상태 동기화 개선
class StateReconciliation {
    void ReconcileClientState(Player& player, ClientState& clientState) {
        // 클라이언트 예측과 서버 상태 차이 확인
        if (Distance(player.serverPos, clientState.predictedPos) > THRESHOLD) {
            // 부드러운 보정
            CorrectionPacket correction;
            correction.serverPos = player.serverPos;
            correction.serverVelocity = player.velocity;
            correction.timestamp = GetServerTime();
            
            player.SendReliable(correction);
        }
    }
};
```

### 케이스 5: DDoS 공격 대응

**문제 상황:**
"갑작스럽게 연결 요청이 폭증하여 정상 유저 접속 불가"

**즉각 대응:**
```cpp
class DDoSProtection {
    // 1. Rate Limiting
    class RateLimiter {
        unordered_map<string, deque<chrono::time_point<chrono::steady_clock>>> requestHistory;
        
        bool AllowRequest(const string& ip) {
            auto now = chrono::steady_clock::now();
            auto& history = requestHistory[ip];
            
            // 오래된 기록 제거
            while (!history.empty() && now - history.front() > 1min) {
                history.pop_front();
            }
            
            // 분당 요청 제한
            if (history.size() >= 60) {
                return false;
            }
            
            history.push_back(now);
            return true;
        }
    };
    
    // 2. SYN Flood 방어
    class SynCookieValidator {
        uint32_t GenerateCookie(const sockaddr_in& addr) {
            // 시간 기반 쿠키 생성
            auto timestamp = chrono::steady_clock::now().time_since_epoch().count();
            return Hash(addr.sin_addr.s_addr, addr.sin_port, timestamp, serverSecret);
        }
        
        bool ValidateCookie(const sockaddr_in& addr, uint32_t cookie) {
            // 최근 2분 내 생성된 쿠키만 유효
            for (int i = 0; i < 4; i++) {
                auto timestamp = chrono::steady_clock::now().time_since_epoch().count() - i * 30;
                if (cookie == Hash(addr.sin_addr.s_addr, addr.sin_port, timestamp, serverSecret)) {
                    return true;
                }
            }
            return false;
        }
    };
    
    // 3. 행동 패턴 분석
    class BehaviorAnalyzer {
        bool IsLikelyBot(const ConnectionInfo& info) {
            // 비정상적 패턴 감지
            if (info.packetsPerSecond > 100) return true;
            if (info.uniquePacketTypes < 2) return true;
            if (info.connectionDuration < 100ms) return true;
            
            return false;
        }
    };
};
```

## 3. 데이터베이스 문제 해결

### 케이스 6: 쿼리 성능 저하

**문제 상황:**
"랭킹 조회 시 응답 시간이 5초 이상"

**쿼리 분석:**
```sql
-- 문제 쿼리
SELECT player_id, name, score,
       RANK() OVER (ORDER BY score DESC) as rank
FROM players
WHERE last_login > NOW() - INTERVAL 30 DAY
ORDER BY score DESC
LIMIT 100;

-- EXPLAIN 결과: Full table scan
```

**최적화:**
```sql
-- 1. 인덱스 추가
CREATE INDEX idx_score_login ON players(last_login, score DESC);

-- 2. 구체화된 뷰 사용
CREATE MATERIALIZED VIEW player_rankings AS
SELECT player_id, name, score,
       RANK() OVER (ORDER BY score DESC) as rank
FROM players
WHERE last_login > NOW() - INTERVAL 30 DAY;

-- 3. 캐싱 전략
```

```cpp
class RankingCache {
    struct RankingEntry {
        int playerId;
        string name;
        int score;
        int rank;
    };
    
    vector<RankingEntry> topPlayers;
    chrono::time_point<chrono::steady_clock> lastUpdate;
    
    vector<RankingEntry> GetTopPlayers() {
        auto now = chrono::steady_clock::now();
        
        // 5분마다 갱신
        if (now - lastUpdate > 5min) {
            UpdateRankings();
            lastUpdate = now;
        }
        
        return topPlayers;
    }
    
    void UpdateRankings() {
        // 비동기 업데이트
        async(launch::async, [this]() {
            auto newRankings = db.Query("SELECT * FROM player_rankings LIMIT 100");
            lock_guard<mutex> lock(rankingMutex);
            topPlayers = move(newRankings);
        });
    }
};
```

## 4. 동시성 문제 해결

### 케이스 7: Race Condition

**문제 상황:**
"아이템 복사 버그 - 동시에 거래하면 아이템이 복제됨"

**버그 재현:**
```cpp
// 문제 코드
void TradeItems(Player& player1, Player& player2) {
    // Race condition 발생 가능
    if (player1.HasItem(itemId)) {
        player1.RemoveItem(itemId);
        player2.AddItem(itemId);
    }
}
```

**해결:**
```cpp
class TransactionManager {
    // 트랜잭션 기반 처리
    void ExecuteTrade(Player& player1, Player& player2, 
                     const vector<Item>& items1, const vector<Item>& items2) {
        // 2-Phase Commit
        Transaction txn;
        
        // Phase 1: Prepare
        if (!player1.LockItems(items1) || !player2.LockItems(items2)) {
            return;  // 실패
        }
        
        // Phase 2: Commit
        try {
            txn.Begin();
            
            for (auto& item : items1) {
                txn.Execute([&]() {
                    player1.RemoveItem(item);
                    player2.AddItem(item);
                });
            }
            
            for (auto& item : items2) {
                txn.Execute([&]() {
                    player2.RemoveItem(item);
                    player1.AddItem(item);
                });
            }
            
            txn.Commit();
        } catch (...) {
            txn.Rollback();
            throw;
        } finally {
            player1.UnlockItems();
            player2.UnlockItems();
        }
    }
};
```

## 면접 대비 포인트

1. **체계적 접근**: 문제 재현 → 분석 → 해결 → 검증
2. **도구 활용**: 프로파일러, 디버거, 모니터링 툴
3. **근본 원인 분석**: 증상이 아닌 원인 해결
4. **성능 지표**: 개선 전후 정량적 비교
5. **재발 방지**: 자동화된 테스트, 모니터링 추가