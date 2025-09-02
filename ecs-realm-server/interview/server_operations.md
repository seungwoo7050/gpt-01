# 서버 운영 및 모니터링

## 1. 모니터링 시스템 구축

### 핵심 메트릭 모니터링

**시스템 메트릭**
```cpp
class SystemMonitor {
    struct Metrics {
        float cpuUsage;
        float memoryUsage;
        float diskIO;
        float networkBandwidth;
        int activeConnections;
        float averageLatency;
    };
    
    void CollectMetrics() {
        metrics.cpuUsage = GetCPUUsage();
        metrics.memoryUsage = GetMemoryUsage();
        metrics.activeConnections = connectionPool.GetActiveCount();
        
        // Prometheus에 전송
        prometheus::Gauge cpu_gauge{"server_cpu_usage", "CPU usage percentage"};
        cpu_gauge.Set(metrics.cpuUsage);
    }
};
```

**게임 메트릭**
```cpp
class GameMetricsCollector {
    void TrackGameMetrics() {
        // DAU/MAU
        TrackDailyActiveUsers();
        
        // 동시 접속자
        TrackConcurrentUsers();
        
        // 게임 플레이 메트릭
        TrackAverageSessionLength();
        TrackPlayerRetention();
        
        // 경제 지표
        TrackCurrencyFlow();
        TrackItemDistribution();
    }
};
```

### 로깅 시스템

**구조화된 로깅**
```cpp
class StructuredLogger {
    void LogPlayerAction(const PlayerAction& action) {
        json logEntry = {
            {"timestamp", GetTimestamp()},
            {"playerId", action.playerId},
            {"action", action.type},
            {"metadata", action.metadata},
            {"serverId", GetServerId()},
            {"sessionId", action.sessionId}
        };
        
        // ELK 스택으로 전송
        elasticsearchClient.Index("game-logs", logEntry);
    }
};
```

**로그 레벨 관리**
```cpp
enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

class Logger {
    LogLevel currentLevel = LogLevel::INFO;
    
    template<typename... Args>
    void Log(LogLevel level, const string& format, Args... args) {
        if (level < currentLevel) return;
        
        string message = fmt::format(format, args...);
        
        // 비동기 로깅
        logQueue.Push({level, message, GetTimestamp()});
    }
};
```

## 2. 무중단 배포 전략

### Blue-Green 배포
```bash
# 배포 스크립트
deploy_blue_green() {
    # 1. Green 환경에 새 버전 배포
    deploy_to_green $NEW_VERSION
    
    # 2. Health check
    if ! health_check_green; then
        rollback_green
        exit 1
    fi
    
    # 3. 트래픽 전환
    switch_traffic_to_green
    
    # 4. Blue 환경 업데이트
    update_blue_environment
}
```

### Rolling Update
```cpp
class RollingUpdateManager {
    void PerformUpdate(const vector<ServerNode>& nodes) {
        int batchSize = nodes.size() / 4;  // 25%씩 업데이트
        
        for (int i = 0; i < nodes.size(); i += batchSize) {
            // 1. 노드 제거
            for (int j = i; j < min(i + batchSize, nodes.size()); j++) {
                loadBalancer.RemoveNode(nodes[j]);
            }
            
            // 2. 업데이트
            UpdateNodes(nodes, i, min(i + batchSize, nodes.size()));
            
            // 3. 헬스체크 & 복구
            WaitForHealthy(nodes, i, min(i + batchSize, nodes.size()));
            
            // 4. 노드 추가
            for (int j = i; j < min(i + batchSize, nodes.size()); j++) {
                loadBalancer.AddNode(nodes[j]);
            }
            
            // 5. 안정화 대기
            this_thread::sleep_for(chrono::minutes(5));
        }
    }
};
```

## 3. 장애 대응

### 자동 장애 감지
```cpp
class FailureDetector {
    struct NodeHealth {
        int failureCount = 0;
        chrono::time_point<chrono::steady_clock> lastCheck;
        bool isHealthy = true;
    };
    
    map<string, NodeHealth> nodeHealthMap;
    
    void CheckNode(const string& nodeId) {
        if (!PingNode(nodeId)) {
            nodeHealthMap[nodeId].failureCount++;
            
            if (nodeHealthMap[nodeId].failureCount >= 3) {
                HandleNodeFailure(nodeId);
            }
        } else {
            nodeHealthMap[nodeId].failureCount = 0;
        }
    }
    
    void HandleNodeFailure(const string& nodeId) {
        // 1. 로드밸런서에서 제거
        loadBalancer.RemoveNode(nodeId);
        
        // 2. 알림 발송
        alertManager.SendAlert(AlertLevel::CRITICAL, 
            fmt::format("Node {} failed", nodeId));
        
        // 3. 자동 복구 시도
        recoveryManager.AttemptRecovery(nodeId);
    }
};
```

### Circuit Breaker 패턴
```cpp
class CircuitBreaker {
    enum State { CLOSED, OPEN, HALF_OPEN };
    
    State state = CLOSED;
    int failureCount = 0;
    int failureThreshold = 5;
    chrono::duration<int> timeout{30};
    chrono::time_point<chrono::steady_clock> lastFailureTime;
    
    template<typename Func>
    auto Call(Func func) -> decltype(func()) {
        if (state == OPEN) {
            if (chrono::steady_clock::now() - lastFailureTime > timeout) {
                state = HALF_OPEN;
            } else {
                throw CircuitOpenException();
            }
        }
        
        try {
            auto result = func();
            
            if (state == HALF_OPEN) {
                state = CLOSED;
                failureCount = 0;
            }
            
            return result;
        } catch (...) {
            RecordFailure();
            throw;
        }
    }
    
    void RecordFailure() {
        failureCount++;
        lastFailureTime = chrono::steady_clock::now();
        
        if (failureCount >= failureThreshold) {
            state = OPEN;
        }
    }
};
```

## 4. 성능 모니터링

### APM (Application Performance Monitoring)
```cpp
class APMCollector {
    void TraceTransaction(const string& transactionName) {
        auto span = tracer->StartSpan(transactionName);
        
        // 트랜잭션 추적
        span->SetTag("user.id", getCurrentUserId());
        span->SetTag("server.id", getServerId());
        
        // 성능 데이터 수집
        auto start = chrono::high_resolution_clock::now();
        
        // 실제 작업 수행
        ProcessTransaction();
        
        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
        
        span->SetTag("duration.ms", duration.count());
        span->Finish();
    }
};
```

### 프로파일링
```cpp
class PerformanceProfiler {
    struct ProfileData {
        string functionName;
        uint64_t callCount;
        chrono::nanoseconds totalTime;
        chrono::nanoseconds maxTime;
    };
    
    map<string, ProfileData> profileMap;
    
    class ScopedTimer {
        string name;
        chrono::time_point<chrono::high_resolution_clock> start;
        PerformanceProfiler* profiler;
        
    public:
        ScopedTimer(const string& n, PerformanceProfiler* p) 
            : name(n), profiler(p) {
            start = chrono::high_resolution_clock::now();
        }
        
        ~ScopedTimer() {
            auto end = chrono::high_resolution_clock::now();
            auto duration = end - start;
            profiler->Record(name, duration);
        }
    };
    
    void Record(const string& name, chrono::nanoseconds duration) {
        profileMap[name].callCount++;
        profileMap[name].totalTime += duration;
        profileMap[name].maxTime = max(profileMap[name].maxTime, duration);
    }
};

#define PROFILE(name) PerformanceProfiler::ScopedTimer _timer(name, &profiler)
```

## 5. 백업 및 복구

### 데이터 백업 전략
```cpp
class BackupManager {
    void PerformBackup() {
        // 1. 스냅샷 생성
        CreateDatabaseSnapshot();
        
        // 2. 증분 백업
        BackupTransactionLogs();
        
        // 3. 원격 저장소 동기화
        SyncToRemoteStorage();
        
        // 4. 백업 검증
        VerifyBackupIntegrity();
    }
    
    void RestoreFromBackup(const string& backupId) {
        // 1. 서비스 중단
        StopGameServers();
        
        // 2. 백업 복원
        RestoreDatabase(backupId);
        
        // 3. 일관성 검증
        VerifyDataConsistency();
        
        // 4. 서비스 재개
        StartGameServers();
    }
};
```

## 6. 스케일링 전략

### 자동 스케일링
```cpp
class AutoScaler {
    struct ScalingPolicy {
        float cpuThresholdUp = 70.0f;
        float cpuThresholdDown = 30.0f;
        int minInstances = 2;
        int maxInstances = 20;
        chrono::seconds cooldownPeriod{300};
    };
    
    void EvaluateScaling() {
        auto metrics = GetAverageMetrics();
        
        if (metrics.cpuUsage > policy.cpuThresholdUp) {
            ScaleUp();
        } else if (metrics.cpuUsage < policy.cpuThresholdDown) {
            ScaleDown();
        }
    }
    
    void ScaleUp() {
        if (currentInstances >= policy.maxInstances) return;
        if (IsInCooldown()) return;
        
        int newInstances = min(currentInstances * 2, policy.maxInstances);
        LaunchInstances(newInstances - currentInstances);
        
        lastScalingTime = chrono::steady_clock::now();
    }
};
```

## 면접 대비 체크포인트

1. **실제 장애 경험**: 어떤 장애를 겪었고 어떻게 해결했는지
2. **모니터링 도구**: Prometheus, Grafana, ELK 스택 경험
3. **CI/CD 파이프라인**: Jenkins, GitLab CI, GitHub Actions
4. **컨테이너화**: Docker, Kubernetes 운영 경험
5. **클라우드 서비스**: AWS, GCP, Azure 활용 경험