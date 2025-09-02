# 23단계: 고급 보안 & 컴플라이언스 - 해커들로부터 게임을 지키는 최후의 방어선
*1억 달러 규모의 게임이 한 순간에 무너지지 않게 만드는 철벽 보안*

> **🎯 목표**: 상용 게임 서비스에 필요한 엔터프라이즈급 보안 시스템 구축 및 국제 규정 준수 체계 마스터

---

## 📋 문서 정보

- **난이도**: 🔴 최고급 (보안은 전쟁이다!)
- **예상 학습 시간**: 15-20시간 (보안은 절대 서두르면 안됨)
- **필요 선수 지식**: 
  - ✅ [1-22단계](./01_advanced_cpp_features.md) 모든 내용 완료
  - ✅ 네트워크 보안 기초
  - ✅ 암호학 기본 개념
  - ✅ 시스템 관리 경험
  - ✅ "보안 사고가 얼마나 무서운지" 아는 마음가짐
- **실습 환경**: Linux, 보안 도구, SIEM 시스템
- **최종 결과물**: 펜타곤급 보안 시스템!

---

## 🚨 왜 게임 서버 보안이 생존 문제인가?

### **실제 게임 보안 사고 사례**

```cpp
// 😱 2019년 A 게임사 해킹 사고
사용자 데이터: 1,200만 명 개인정보 유출
피해 규모: 배상금 300억원 + 서비스 중단 2주
원인: SQL Injection + 암호화 미적용

// 😱 2021년 B 게임사 DDoS 공격  
공격 규모: 500Gbps DDoS
서비스 마비: 72시간
매출 손실: 일 200억원 × 3일 = 600억원
원인: DDoS 방어 시스템 부재

// 😱 2022년 C 게임사 치팅 대란
치팅 사용자: 전체의 40%
게임 경제 붕괴: 가상 화폐 가치 1/100로 폭락  
사용자 이탈: 80% (6개월 내)
원인: 클라이언트 기반 검증
```

**💰 게임 보안 실패의 비용:**
- **직접 비용**: 배상금, 복구 비용, 법무 비용
- **간접 비용**: 신뢰도 하락, 사용자 이탈, 매출 감소
- **기회 비용**: 개발 리소스 전환, 신규 기능 지연

**⚖️ 법적 리스크:**
- **개인정보보호법**: 과징금 매출액의 3% (최대 300억원)
- **GDPR**: 과징금 연 매출의 4% (최대 2천만 유로)
- **SOX법**: 임원 개인 형사 처벌

---

## 🛡️ 1. Zero Trust 아키텍처 구현

### **1.1 Zero Trust 기본 원칙**

```cpp
// 전통적인 보안 모델 (경계 기반)
class TraditionalSecurity {
    // ❌ 문제점: 내부 네트워크는 신뢰
    bool IsSecure(User user, Resource resource) {
        if (user.IsInsideFirewall()) {
            return true;  // 내부 사용자는 무조건 신뢰
        }
        return CheckCredentials(user);
    }
};

// Zero Trust 모델 (절대 신뢰 없음)
class ZeroTrustSecurity {
    // ✅ 모든 접근을 검증
    bool IsSecure(User user, Resource resource, Context context) {
        // 1. 사용자 신원 확인
        if (!VerifyIdentity(user)) return false;
        
        // 2. 디바이스 상태 확인  
        if (!VerifyDevice(user.device)) return false;
        
        // 3. 네트워크 위치 무관하게 검증
        if (!CheckPermissions(user, resource)) return false;
        
        // 4. 실시간 위험 평가
        if (!AssessRisk(user, resource, context)) return false;
        
        // 5. 최소 권한 원칙
        return GrantMinimalAccess(user, resource);
    }
    
private:
    bool VerifyIdentity(const User& user) {
        // 다단계 인증 (MFA) 필수
        return user.HasValidMFA() && 
               user.BiometricVerified() &&
               user.TokenValid();
    }
    
    bool VerifyDevice(const Device& device) {
        // 디바이스 신뢰성 검증
        return device.IsManaged() &&
               device.HasLatestPatches() &&
               !device.IsCompromised();
    }
    
    RiskLevel AssessRisk(const User& user, const Resource& resource, const Context& context) {
        RiskLevel risk = RiskLevel::LOW;
        
        // 비정상적인 접근 패턴
        if (IsUnusualBehavior(user, context)) {
            risk = std::max(risk, RiskLevel::MEDIUM);
        }
        
        // 고위험 리소스 접근
        if (resource.IsCritical()) {
            risk = std::max(risk, RiskLevel::HIGH);
        }
        
        // 이상한 시간대/위치
        if (IsAnomalousAccess(user, context)) {
            risk = std::max(risk, RiskLevel::HIGH);
        }
        
        return risk;
    }
};
```

### **1.2 마이크로 세그멘테이션 구현**

```cpp
#include <unordered_set>
#include <string>

// 네트워크 마이크로 세그멘테이션
class NetworkMicroSegmentation {
private:
    struct SecurityZone {
        std::string name;
        SecurityLevel level;
        std::unordered_set<std::string> allowed_services;
        std::unordered_set<std::string> allowed_users;
        
        bool CanAccess(const std::string& service, const User& user) const {
            return allowed_services.count(service) > 0 &&
                   allowed_users.count(user.GetRole()) > 0;
        }
    };
    
    std::unordered_map<std::string, SecurityZone> zones_;
    
public:
    void InitializeGameServerZones() {
        // DMZ 존 (인터넷 접근 가능)
        zones_["dmz"] = {
            .name = "DMZ",
            .level = SecurityLevel::PUBLIC,
            .allowed_services = {"web_server", "load_balancer"},
            .allowed_users = {"public"}
        };
        
        // 게임 서버 존 (게임 로직 처리)
        zones_["game_server"] = {
            .name = "GameServer",
            .level = SecurityLevel::RESTRICTED,
            .allowed_services = {"game_logic", "session_manager"},
            .allowed_users = {"game_service", "monitoring"}
        };
        
        // 데이터베이스 존 (최고 보안)
        zones_["database"] = {
            .name = "Database",
            .level = SecurityLevel::CONFIDENTIAL,
            .allowed_services = {"mysql", "redis"},
            .allowed_users = {"db_admin", "game_service"}
        };
        
        // 관리 존 (운영자 접근)
        zones_["admin"] = {
            .name = "Admin",
            .level = SecurityLevel::TOP_SECRET,
            .allowed_services = {"monitoring", "logging", "backup"},
            .allowed_users = {"admin", "sre"}
        };
    }
    
    AccessDecision EvaluateAccess(const NetworkRequest& request) {
        auto source_zone = GetZone(request.source_ip);
        auto target_zone = GetZone(request.target_ip);
        
        // 기본적으로 거부
        AccessDecision decision = AccessDecision::DENY;
        
        // 존 간 통신 규칙 확인
        if (IsAllowedCommunication(source_zone, target_zone, request)) {
            decision = AccessDecision::ALLOW;
            
            // 추가 보안 검사
            if (RequiresDeepInspection(source_zone, target_zone)) {
                decision = PerformDeepPacketInspection(request);
            }
        }
        
        // 보안 로그 기록
        LogSecurityEvent(request, decision);
        
        return decision;
    }
    
private:
    bool IsAllowedCommunication(const SecurityZone& source, const SecurityZone& target, 
                               const NetworkRequest& request) {
        // 같은 존 내 통신은 허용
        if (source.name == target.name) {
            return true;
        }
        
        // 상위 보안 등급에서 하위로의 접근 차단
        if (source.level < target.level) {
            return false;
        }
        
        // 서비스별 접근 규칙 확인
        return target.CanAccess(request.service, request.user);
    }
    
    AccessDecision PerformDeepPacketInspection(const NetworkRequest& request) {
        // SQL Injection 패턴 검사
        if (ContainsSqlInjectionPattern(request.payload)) {
            AlertSecurityTeam("SQL Injection attempt detected", request);
            return AccessDecision::DENY;
        }
        
        // 악성 스크립트 패턴 검사
        if (ContainsMaliciousScript(request.payload)) {
            AlertSecurityTeam("Malicious script detected", request);
            return AccessDecision::DENY;
        }
        
        // 대용량 페이로드 검사 (DDoS 가능성)
        if (request.payload.size() > MAX_PAYLOAD_SIZE) {
            AlertSecurityTeam("Oversized payload detected", request);
            return AccessDecision::THROTTLE;
        }
        
        return AccessDecision::ALLOW;
    }
};
```

### **1.3 동적 액세스 제어**

```cpp
#include <chrono>
#include <algorithm>

// 실시간 위험 기반 액세스 제어
class DynamicAccessControl {
private:
    struct UserBehaviorProfile {
        std::vector<AccessPattern> normal_patterns;
        std::chrono::system_clock::time_point last_updated;
        double risk_score = 0.0;
        
        void UpdateProfile(const AccessAttempt& attempt) {
            // 기계학습 모델로 정상 패턴 업데이트
            UpdateNormalPatterns(attempt);
            
            // 위험 점수 재계산
            risk_score = CalculateRiskScore(attempt);
            
            last_updated = std::chrono::system_clock::now();
        }
        
    private:
        void UpdateNormalPatterns(const AccessAttempt& attempt) {
            // 시간대별 접근 패턴
            normal_patterns.push_back({
                .time_of_day = attempt.timestamp.hour(),
                .location = attempt.source_location,
                .device_type = attempt.device.type,
                .access_frequency = CalculateFrequency(attempt)
            });
            
            // 최근 30일 데이터만 유지
            auto cutoff = std::chrono::system_clock::now() - std::chrono::hours(24 * 30);
            normal_patterns.erase(
                std::remove_if(normal_patterns.begin(), normal_patterns.end(),
                    [cutoff](const AccessPattern& pattern) {
                        return pattern.timestamp < cutoff;
                    }),
                normal_patterns.end()
            );
        }
        
        double CalculateRiskScore(const AccessAttempt& attempt) {
            double score = 0.0;
            
            // 비정상적인 시간대 (평소와 다른 시간)
            if (!IsNormalTimeOfDay(attempt.timestamp)) {
                score += 0.3;
            }
            
            // 비정상적인 위치 (새로운 국가/도시)
            if (!IsKnownLocation(attempt.source_location)) {
                score += 0.4;
            }
            
            // 비정상적인 디바이스
            if (!IsKnownDevice(attempt.device)) {
                score += 0.5;
            }
            
            // 접근 빈도 이상 (너무 자주 또는 너무 드물게)
            double frequency_anomaly = CalculateFrequencyAnomaly(attempt);
            score += frequency_anomaly * 0.2;
            
            return std::min(score, 1.0);  // 최대 1.0으로 제한
        }
    };
    
    std::unordered_map<UserId, UserBehaviorProfile> user_profiles_;
    MLAnomalyDetector anomaly_detector_;
    
public:
    AccessDecision EvaluateAccess(const AccessAttempt& attempt) {
        auto& profile = user_profiles_[attempt.user_id];
        
        // 사용자 프로필 업데이트
        profile.UpdateProfile(attempt);
        
        // 위험 점수 기반 결정
        if (profile.risk_score < 0.3) {
            return AccessDecision::ALLOW;
        } else if (profile.risk_score < 0.7) {
            return AccessDecision::REQUIRE_MFA;
        } else {
            return AccessDecision::REQUIRE_ADMIN_APPROVAL;
        }
    }
    
    void HandleHighRiskAccess(const AccessAttempt& attempt) {
        // 즉시 보안팀에 알림
        SendSecurityAlert({
            .severity = AlertSeverity::HIGH,
            .user_id = attempt.user_id,
            .risk_score = user_profiles_[attempt.user_id].risk_score,
            .details = GenerateRiskDetails(attempt)
        });
        
        // 사용자 계정 임시 잠금
        TemporarilyLockAccount(attempt.user_id, std::chrono::minutes(15));
        
        // 추가 인증 요구
        RequireStepUpAuthentication(attempt.user_id);
    }
    
private:
    std::string GenerateRiskDetails(const AccessAttempt& attempt) {
        json risk_details;
        risk_details["unusual_time"] = !IsNormalTimeOfDay(attempt.timestamp);
        risk_details["new_location"] = !IsKnownLocation(attempt.source_location);
        risk_details["new_device"] = !IsKnownDevice(attempt.device);
        risk_details["ip_reputation"] = CheckIPReputation(attempt.source_ip);
        
        return risk_details.dump();
    }
};
```

---

## 🔍 2. SIEM (Security Information and Event Management) 시스템

### **2.1 실시간 보안 이벤트 수집**

```cpp
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <queue>
#include <mutex>

// 통합 보안 이벤트 관리 시스템
class GameServerSIEM {
private:
    struct SecurityEvent {
        EventType type;
        SecurityLevel severity;
        std::chrono::system_clock::time_point timestamp;
        std::string source;
        std::string target;
        std::string description;
        nlohmann::json metadata;
        
        std::string ToJson() const {
            nlohmann::json j;
            j["type"] = EventTypeToString(type);
            j["severity"] = SeverityToString(severity);
            j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                timestamp.time_since_epoch()).count();
            j["source"] = source;
            j["target"] = target;
            j["description"] = description;
            j["metadata"] = metadata;
            return j.dump();
        }
    };
    
    // 이벤트 큐 (스레드 안전)
    std::queue<SecurityEvent> event_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
    // 실시간 분석 엔진
    std::unique_ptr<RealTimeAnalyzer> analyzer_;
    
    // 외부 SIEM 연동 (Splunk, ELK 등)
    std::vector<std::unique_ptr<SIEMConnector>> external_siems_;
    
public:
    GameServerSIEM() {
        // 실시간 분석 엔진 초기화
        analyzer_ = std::make_unique<RealTimeAnalyzer>();
        
        // 외부 SIEM 시스템 연결
        InitializeExternalSIEMs();
        
        // 이벤트 처리 워커 스레드 시작
        StartEventProcessors();
    }
    
    // 게임 서버 특화 보안 이벤트 로깅
    void LogPlayerSecurityEvent(PlayerId player_id, const std::string& event_type, 
                               const nlohmann::json& details) {
        SecurityEvent event;
        event.type = EventType::PLAYER_SECURITY;
        event.severity = DetermineSeverity(event_type, details);
        event.timestamp = std::chrono::system_clock::now();
        event.source = "game_server";
        event.target = std::to_string(player_id);
        event.description = event_type;
        event.metadata = details;
        
        QueueEvent(std::move(event));
    }
    
    void LogCheatDetection(PlayerId player_id, CheatType cheat_type, 
                          const std::string& evidence) {
        nlohmann::json metadata;
        metadata["cheat_type"] = CheatTypeToString(cheat_type);
        metadata["evidence"] = evidence;
        metadata["player_level"] = GetPlayerLevel(player_id);
        metadata["play_time"] = GetPlayerPlayTime(player_id);
        metadata["previous_violations"] = GetPreviousViolations(player_id);
        
        SecurityEvent event;
        event.type = EventType::CHEAT_DETECTION;
        event.severity = SecurityLevel::HIGH;
        event.timestamp = std::chrono::system_clock::now();
        event.source = "anti_cheat_system";
        event.target = std::to_string(player_id);
        event.description = fmt::format("Cheat detected: {}", CheatTypeToString(cheat_type));
        event.metadata = metadata;
        
        QueueEvent(std::move(event));
        
        // 즉시 대응 조치
        TakeImmediateAction(player_id, cheat_type);
    }
    
    void LogNetworkAnomaly(const std::string& source_ip, 
                          const std::string& anomaly_type,
                          const NetworkMetrics& metrics) {
        nlohmann::json metadata;
        metadata["source_ip"] = source_ip;
        metadata["packets_per_second"] = metrics.packets_per_second;
        metadata["bytes_per_second"] = metrics.bytes_per_second;
        metadata["connection_count"] = metrics.connection_count;
        metadata["geolocation"] = GetIPGeolocation(source_ip);
        metadata["ip_reputation"] = CheckIPReputation(source_ip);
        
        SecurityEvent event;
        event.type = EventType::NETWORK_ANOMALY;
        event.severity = DetermineNetworkAnomalySeverity(metrics);
        event.timestamp = std::chrono::system_clock::now();
        event.source = source_ip;
        event.target = "game_server";
        event.description = fmt::format("Network anomaly: {}", anomaly_type);
        event.metadata = metadata;
        
        QueueEvent(std::move(event));
    }
    
private:
    void QueueEvent(SecurityEvent event) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            event_queue_.push(std::move(event));
        }
        queue_cv_.notify_one();
    }
    
    void StartEventProcessors() {
        // 이벤트 처리 워커 스레드들
        for (int i = 0; i < std::thread::hardware_concurrency(); ++i) {
            std::thread([this]() {
                EventProcessor();
            }).detach();
        }
        
        // 실시간 분석 스레드
        std::thread([this]() {
            RealTimeAnalysis();
        }).detach();
        
        // 알럿 처리 스레드
        std::thread([this]() {
            AlertProcessor();
        }).detach();
    }
    
    void EventProcessor() {
        while (true) {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] { return !event_queue_.empty(); });
            
            if (event_queue_.empty()) continue;
            
            SecurityEvent event = std::move(event_queue_.front());
            event_queue_.pop();
            lock.unlock();
            
            // 이벤트 처리
            ProcessSecurityEvent(event);
        }
    }
    
    void ProcessSecurityEvent(const SecurityEvent& event) {
        // 1. 로컬 저장
        StoreEventLocally(event);
        
        // 2. 실시간 분석에 전달
        analyzer_->AnalyzeEvent(event);
        
        // 3. 외부 SIEM으로 전송
        for (auto& siem : external_siems_) {
            siem->SendEvent(event);
        }
        
        // 4. 심각도별 즉시 대응
        if (event.severity >= SecurityLevel::HIGH) {
            TriggerImmediateResponse(event);
        }
    }
    
    void RealTimeAnalysis() {
        CorrelationEngine correlation_engine;
        
        while (true) {
            // 최근 이벤트들을 상관 분석
            auto recent_events = GetRecentEvents(std::chrono::minutes(5));
            auto correlations = correlation_engine.FindCorrelations(recent_events);
            
            for (const auto& correlation : correlations) {
                if (correlation.threat_level >= ThreatLevel::MEDIUM) {
                    GenerateSecurityAlert(correlation);
                }
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
    }
};

// 상관 분석 엔진 (복합 공격 탐지)
class CorrelationEngine {
public:
    std::vector<SecurityCorrelation> FindCorrelations(const std::vector<SecurityEvent>& events) {
        std::vector<SecurityCorrelation> correlations;
        
        // 1. DDoS 공격 패턴 탐지
        auto ddos_correlation = DetectDDoSPattern(events);
        if (ddos_correlation.has_value()) {
            correlations.push_back(ddos_correlation.value());
        }
        
        // 2. 계정 탈취 시도 탐지
        auto account_takeover = DetectAccountTakeoverPattern(events);
        if (account_takeover.has_value()) {
            correlations.push_back(account_takeover.value());
        }
        
        // 3. 내부자 위협 탐지
        auto insider_threat = DetectInsiderThreatPattern(events);
        if (insider_threat.has_value()) {
            correlations.push_back(insider_threat.value());
        }
        
        // 4. APT (Advanced Persistent Threat) 탐지
        auto apt_correlation = DetectAPTPattern(events);
        if (apt_correlation.has_value()) {
            correlations.push_back(apt_correlation.value());
        }
        
        return correlations;
    }
    
private:
    std::optional<SecurityCorrelation> DetectDDoSPattern(const std::vector<SecurityEvent>& events) {
        // 동일 IP에서 대량 요청
        std::unordered_map<std::string, int> ip_counts;
        
        for (const auto& event : events) {
            if (event.type == EventType::NETWORK_ANOMALY) {
                ip_counts[event.source]++;
            }
        }
        
        for (const auto& [ip, count] : ip_counts) {
            if (count > 100) {  // 5분간 100건 이상
                SecurityCorrelation correlation;
                correlation.threat_type = ThreatType::DDOS_ATTACK;
                correlation.threat_level = ThreatLevel::HIGH;
                correlation.source_ips = {ip};
                correlation.confidence = 0.9;
                correlation.description = fmt::format("DDoS attack detected from {}: {} requests in 5 minutes", ip, count);
                
                return correlation;
            }
        }
        
        return std::nullopt;
    }
    
    std::optional<SecurityCorrelation> DetectAccountTakeoverPattern(const std::vector<SecurityEvent>& events) {
        // 계정별 로그인 실패 -> 성공 패턴
        std::unordered_map<PlayerId, std::vector<SecurityEvent>> player_events;
        
        for (const auto& event : events) {
            if (event.type == EventType::AUTHENTICATION) {
                PlayerId player_id = std::stoull(event.target);
                player_events[player_id].push_back(event);
            }
        }
        
        for (const auto& [player_id, events] : player_events) {
            int failed_attempts = 0;
            bool successful_login = false;
            std::string last_failure_ip;
            std::string success_ip;
            
            for (const auto& event : events) {
                if (event.metadata["result"] == "failed") {
                    failed_attempts++;
                    last_failure_ip = event.source;
                } else if (event.metadata["result"] == "success") {
                    successful_login = true;
                    success_ip = event.source;
                }
            }
            
            // 5회 이상 실패 후 다른 IP에서 성공
            if (failed_attempts >= 5 && successful_login && last_failure_ip != success_ip) {
                SecurityCorrelation correlation;
                correlation.threat_type = ThreatType::ACCOUNT_TAKEOVER;
                correlation.threat_level = ThreatLevel::HIGH;
                correlation.source_ips = {last_failure_ip, success_ip};
                correlation.confidence = 0.8;
                correlation.description = fmt::format("Possible account takeover for player {}: {} failed attempts followed by success from different IP", player_id, failed_attempts);
                
                return correlation;
            }
        }
        
        return std::nullopt;
    }
};
```

### **2.2 실시간 위협 탐지 및 대응**

```cpp
#include <machine_learning/threat_detector.h>

// AI 기반 실시간 위협 탐지 시스템
class AIThreatDetector {
private:
    // 머신러닝 모델들
    std::unique_ptr<AnomalyDetectionModel> anomaly_model_;
    std::unique_ptr<CheatDetectionModel> cheat_model_;
    std::unique_ptr<AttackPredictionModel> attack_model_;
    
    // 실시간 데이터 스트림
    RealTimeDataStream<PlayerBehavior> player_stream_;
    RealTimeDataStream<NetworkTraffic> network_stream_;
    RealTimeDataStream<SystemMetrics> system_stream_;
    
public:
    AIThreatDetector() {
        InitializeMLModels();
        StartRealTimeDetection();
    }
    
    void InitializeMLModels() {
        // 이상 탐지 모델 (Isolation Forest)
        anomaly_model_ = std::make_unique<IsolationForestModel>();
        anomaly_model_->LoadPretrainedModel("models/anomaly_detection.model");
        
        // 치팅 탐지 모델 (Deep Learning)
        cheat_model_ = std::make_unique<DNNCheatDetector>();
        cheat_model_->LoadPretrainedModel("models/cheat_detection.model");
        
        // 공격 예측 모델 (Time Series)
        attack_model_ = std::make_unique<LSTMAttackPredictor>();
        attack_model_->LoadPretrainedModel("models/attack_prediction.model");
    }
    
    void StartRealTimeDetection() {
        // 플레이어 행동 분석 스레드
        std::thread([this]() {
            PlayerBehaviorAnalysis();
        }).detach();
        
        // 네트워크 트래픽 분석 스레드
        std::thread([this]() {
            NetworkTrafficAnalysis();
        }).detach();
        
        // 시스템 메트릭 분석 스레드
        std::thread([this]() {
            SystemMetricsAnalysis();
        }).detach();
    }
    
private:
    void PlayerBehaviorAnalysis() {
        while (true) {
            auto behavior_batch = player_stream_.GetBatch(100);  // 100개씩 배치 처리
            
            for (const auto& behavior : behavior_batch) {
                // 치팅 탐지
                auto cheat_result = cheat_model_->Predict(behavior);
                if (cheat_result.probability > 0.8) {
                    HandleCheatDetection(behavior.player_id, cheat_result);
                }
                
                // 이상 행동 탐지
                auto anomaly_score = anomaly_model_->GetAnomalyScore(behavior);
                if (anomaly_score > 0.9) {
                    HandleAnomalousBehavior(behavior.player_id, anomaly_score);
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    void NetworkTrafficAnalysis() {
        while (true) {
            auto traffic_batch = network_stream_.GetBatch(1000);
            
            // 트래픽 패턴 분석
            auto traffic_features = ExtractTrafficFeatures(traffic_batch);
            
            // DDoS 공격 예측
            auto attack_prediction = attack_model_->PredictAttack(traffic_features);
            if (attack_prediction.probability > 0.7) {
                HandlePotentialAttack(attack_prediction);
            }
            
            // 악성 트래픽 탐지
            for (const auto& packet : traffic_batch) {
                if (IsMaliciousPacket(packet)) {
                    HandleMaliciousTraffic(packet);
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    
    void HandleCheatDetection(PlayerId player_id, const CheatPrediction& prediction) {
        // 즉시 조치
        switch (prediction.cheat_type) {
            case CheatType::SPEED_HACK:
                // 즉시 이동 속도 제한
                EnforceSpeedLimit(player_id);
                break;
                
            case CheatType::DAMAGE_HACK:
                // 데미지 계산을 서버에서만 수행
                EnableServerOnlyDamage(player_id);
                break;
                
            case CheatType::TELEPORT_HACK:
                // 위치 동기화 강화
                EnforcePositionValidation(player_id);
                break;
                
            case CheatType::ITEM_DUPLICATION:
                // 인벤토리 롤백
                RollbackInventory(player_id);
                break;
        }
        
        // 보안팀에 알림
        SendCheatAlert({
            .player_id = player_id,
            .cheat_type = prediction.cheat_type,
            .confidence = prediction.probability,
            .evidence = prediction.evidence,
            .action_taken = "automatic_mitigation"
        });
        
        // 플레이어 신뢰도 점수 업데이트
        UpdatePlayerTrustScore(player_id, -50);  // 50점 감점
    }
    
    void HandlePotentialAttack(const AttackPrediction& prediction) {
        if (prediction.attack_type == AttackType::DDOS) {
            // DDoS 방어 조치 활성화
            ActivateDDoSProtection(prediction.source_ips);
            
            // Rate limiting 강화
            EnhanceRateLimiting(prediction.target_services);
            
            // CDN에 알림 (트래픽 분산)
            NotifyCDN("ddos_mitigation", prediction.metadata);
        }
        
        // 예측 정확도 향상을 위한 피드백 루프
        attack_model_->UpdateWithRealTimeData(prediction);
    }
    
    // 고급 네트워크 포렌식
    void PerformNetworkForensics(const SecurityEvent& event) {
        ForensicsReport report;
        report.event_id = event.GetId();
        report.timestamp = event.timestamp;
        
        // 패킷 캡처 분석
        auto packet_capture = CapturePackets(event.source, std::chrono::minutes(5));
        report.packet_analysis = AnalyzePackets(packet_capture);
        
        // DNS 질의 분석
        auto dns_queries = GetDNSQueries(event.source, std::chrono::hours(1));
        report.dns_analysis = AnalyzeDNSQueries(dns_queries);
        
        // 연결 패턴 분석
        auto connections = GetConnectionHistory(event.source, std::chrono::hours(24));
        report.connection_analysis = AnalyzeConnections(connections);
        
        // 지리적 분석
        report.geolocation_analysis = PerformGeolocationAnalysis(event.source);
        
        // 보고서 저장 및 전송
        StoreForensicsReport(report);
        NotifySecurityTeam(report);
    }
};
```

---

## 🛡️ 3. 침입 탐지 시스템 (IDS/IPS)

### **3.1 네트워크 기반 침입 탐지**

```cpp
#include <pcap.h>
#include <regex>

// 고성능 네트워크 침입 탐지 시스템
class NetworkIDS {
private:
    // 공격 시그니처 데이터베이스
    struct AttackSignature {
        std::string name;
        std::regex pattern;
        AttackSeverity severity;
        std::string description;
        std::function<void(const NetworkPacket&)> response_action;
    };
    
    std::vector<AttackSignature> signatures_;
    
    // 실시간 패킷 캡처
    pcap_t* capture_handle_;
    std::thread capture_thread_;
    
    // 패킷 분석 워커 풀
    ThreadPool<NetworkPacket> analysis_pool_;
    
    // 통계 및 성능 메트릭
    std::atomic<uint64_t> packets_processed_{0};
    std::atomic<uint64_t> threats_detected_{0};
    std::atomic<uint64_t> false_positives_{0};
    
public:
    NetworkIDS() : analysis_pool_(std::thread::hardware_concurrency()) {
        InitializeSignatures();
        InitializePacketCapture();
        StartDetection();
    }
    
    void InitializeSignatures() {
        // SQL Injection 탐지
        signatures_.push_back({
            .name = "SQL Injection",
            .pattern = std::regex(R"((\bUNION\b|\bSELECT\b|\bINSERT\b|\bDELETE\b|\bDROP\b).*(\bFROM\b|\bWHERE\b))", 
                                std::regex_constants::icase),
            .severity = AttackSeverity::HIGH,
            .description = "Potential SQL injection attack detected",
            .response_action = [this](const NetworkPacket& packet) {
                BlockSourceIP(packet.source_ip, std::chrono::hours(1));
                LogSecurityEvent("SQL_INJECTION_BLOCKED", packet);
            }
        });
        
        // XSS 공격 탐지
        signatures_.push_back({
            .name = "Cross-Site Scripting",
            .pattern = std::regex(R"(<script[^>]*>.*?</script>|javascript:|on\w+\s*=)", 
                                std::regex_constants::icase),
            .severity = AttackSeverity::MEDIUM,
            .description = "Potential XSS attack detected",
            .response_action = [this](const NetworkPacket& packet) {
                SanitizeRequest(packet);
                LogSecurityEvent("XSS_SANITIZED", packet);
            }
        });
        
        // Command Injection 탐지
        signatures_.push_back({
            .name = "Command Injection",
            .pattern = std::regex(R"(\b(ls|cat|pwd|whoami|id|uname|wget|curl|nc|netcat)\b|[;&|`$])", 
                                std::regex_constants::icase),
            .severity = AttackSeverity::HIGH,
            .description = "Potential command injection detected",
            .response_action = [this](const NetworkPacket& packet) {
                BlockSourceIP(packet.source_ip, std::chrono::hours(24));
                AlertSecurityTeam("COMMAND_INJECTION", packet);
            }
        });
        
        // 게임 프로토콜 특화 공격 탐지
        signatures_.push_back({
            .name = "Game Protocol Abuse",
            .pattern = std::regex(R"(packet_frequency_exceeded|invalid_sequence|protocol_violation)"),
            .severity = AttackSeverity::MEDIUM,
            .description = "Game protocol abuse detected",
            .response_action = [this](const NetworkPacket& packet) {
                ThrottleConnection(packet.source_ip);
                LogGameSecurityEvent("PROTOCOL_ABUSE", packet);
            }
        });
    }
    
    void InitializePacketCapture() {
        char errbuf[PCAP_ERRBUF_SIZE];
        
        // 네트워크 인터페이스에서 패킷 캡처 시작
        capture_handle_ = pcap_open_live("eth0", BUFSIZ, 1, 1000, errbuf);
        if (capture_handle_ == nullptr) {
            throw std::runtime_error(fmt::format("Failed to open capture device: {}", errbuf));
        }
        
        // 필터 설정 (게임 서버 포트만)
        struct bpf_program filter;
        if (pcap_compile(capture_handle_, &filter, "port 8080 or port 8081", 0, PCAP_NETMASK_UNKNOWN) == -1) {
            throw std::runtime_error("Failed to compile packet filter");
        }
        
        if (pcap_setfilter(capture_handle_, &filter) == -1) {
            throw std::runtime_error("Failed to set packet filter");
        }
    }
    
    void StartDetection() {
        capture_thread_ = std::thread([this]() {
            PacketCaptureLoop();
        });
        
        // 성능 모니터링 스레드
        std::thread([this]() {
            PerformanceMonitoring();
        }).detach();
    }
    
private:
    void PacketCaptureLoop() {
        struct pcap_pkthdr header;
        const u_char* packet_data;
        
        while ((packet_data = pcap_next(capture_handle_, &header)) != nullptr) {
            // 패킷 파싱
            NetworkPacket packet = ParsePacket(packet_data, header.len);
            
            // 워커 풀에 분석 작업 추가
            analysis_pool_.Submit([this, packet]() {
                AnalyzePacket(packet);
            });
            
            packets_processed_++;
        }
    }
    
    void AnalyzePacket(const NetworkPacket& packet) {
        // 1. 기본 패킷 검증
        if (!IsValidPacket(packet)) {
            HandleMalformedPacket(packet);
            return;
        }
        
        // 2. IP 평판 확인
        auto ip_reputation = CheckIPReputation(packet.source_ip);
        if (ip_reputation.is_malicious) {
            HandleMaliciousIP(packet, ip_reputation);
            return;
        }
        
        // 3. Rate limiting 확인
        if (IsRateLimitExceeded(packet.source_ip)) {
            HandleRateLimitViolation(packet);
            return;
        }
        
        // 4. 페이로드 분석
        AnalyzePayload(packet);
        
        // 5. 행동 패턴 분석
        AnalyzeBehaviorPattern(packet);
    }
    
    void AnalyzePayload(const NetworkPacket& packet) {
        std::string payload = packet.GetPayloadAsString();
        
        // 모든 시그니처와 매칭
        for (const auto& signature : signatures_) {
            std::smatch match;
            if (std::regex_search(payload, match, signature.pattern)) {
                // 위협 탐지!
                ThreatDetected threat;
                threat.signature_name = signature.name;
                threat.severity = signature.severity;
                threat.source_ip = packet.source_ip;
                threat.timestamp = std::chrono::system_clock::now();
                threat.evidence = match.str();
                threat.packet_info = packet.GetSummary();
                
                // 대응 조치 실행
                signature.response_action(packet);
                
                // 위협 로깅
                LogThreatDetection(threat);
                
                threats_detected_++;
                
                // 심각한 위협인 경우 즉시 알림
                if (signature.severity >= AttackSeverity::HIGH) {
                    SendImmediateAlert(threat);
                }
            }
        }
    }
    
    void AnalyzeBehaviorPattern(const NetworkPacket& packet) {
        // 연결 패턴 분석
        auto connection_pattern = connection_analyzer_.AnalyzeConnection(packet);
        
        if (connection_pattern.is_suspicious) {
            HandleSuspiciousConnection(packet, connection_pattern);
        }
        
        // 시간적 패턴 분석
        auto temporal_pattern = temporal_analyzer_.AnalyzeTemporalPattern(packet);
        
        if (temporal_pattern.is_anomalous) {
            HandleTemporalAnomaly(packet, temporal_pattern);
        }
        
        // 지리적 패턴 분석
        auto geo_pattern = geo_analyzer_.AnalyzeGeographicPattern(packet);
        
        if (geo_pattern.is_suspicious) {
            HandleGeographicAnomaly(packet, geo_pattern);
        }
    }
    
    // 고급 이상 탐지 (머신러닝 기반)
    void PerformAdvancedAnomalyDetection(const NetworkPacket& packet) {
        // 패킷 특성 벡터 생성
        FeatureVector features = ExtractPacketFeatures(packet);
        
        // 이상 점수 계산
        double anomaly_score = anomaly_detector_.GetAnomalyScore(features);
        
        if (anomaly_score > ANOMALY_THRESHOLD) {
            HandleNetworkAnomaly(packet, anomaly_score);
        }
        
        // 모델 업데이트 (온라인 학습)
        if (ShouldUpdateModel(packet)) {
            anomaly_detector_.UpdateModel(features);
        }
    }
    
    void HandleMaliciousIP(const NetworkPacket& packet, const IPReputation& reputation) {
        // IP 차단
        auto block_duration = CalculateBlockDuration(reputation.threat_score);
        BlockSourceIP(packet.source_ip, block_duration);
        
        // 보안 이벤트 로깅
        SecurityEvent event;
        event.type = EventType::MALICIOUS_IP_BLOCKED;
        event.severity = SecurityLevel::HIGH;
        event.source = packet.source_ip;
        event.description = fmt::format("Malicious IP blocked: threat score {}", reputation.threat_score);
        event.metadata["reputation"] = reputation.ToJson();
        
        LogSecurityEvent(event);
    }
    
    void PerformanceMonitoring() {
        while (true) {
            auto stats = GetPerformanceStats();
            
            spdlog::info("IDS Performance - Packets/sec: {}, Threats detected: {}, CPU usage: {}%",
                        stats.packets_per_second, threats_detected_.load(), stats.cpu_usage);
            
            // 성능 저하 시 자동 조정
            if (stats.cpu_usage > 80.0) {
                analysis_pool_.ReduceThreads();
                spdlog::warn("High CPU usage detected, reducing analysis threads");
            } else if (stats.cpu_usage < 40.0 && stats.queue_size > 1000) {
                analysis_pool_.IncreaseThreads();
                spdlog::info("Low CPU usage with high queue, increasing analysis threads");
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
    }
};
```

### **3.2 호스트 기반 침입 탐지**

```cpp
// 호스트 시스템 무결성 모니터링
class HostIDS {
private:
    // 파일 무결성 모니터링
    FileIntegrityMonitor fim_;
    
    // 프로세스 모니터링
    ProcessMonitor process_monitor_;
    
    // 네트워크 연결 모니터링
    NetworkConnectionMonitor network_monitor_;
    
    // 로그 파일 모니터링
    LogFileMonitor log_monitor_;
    
public:
    HostIDS() {
        InitializeMonitoring();
        StartMonitoring();
    }
    
    void InitializeMonitoring() {
        // 중요 파일들 모니터링 대상 추가
        fim_.AddCriticalFiles({
            "/etc/passwd",
            "/etc/shadow", 
            "/etc/hosts",
            "/opt/gameserver/config/",
            "/opt/gameserver/bin/",
            "/var/log/gameserver/"
        });
        
        // 허용된 프로세스 목록 설정
        process_monitor_.SetAllowedProcesses({
            "gameserver",
            "mysql",
            "redis-server",
            "nginx"
        });
        
        // 네트워크 연결 화이트리스트
        network_monitor_.SetAllowedConnections({
            {"database", "3306"},
            {"redis", "6379"},
            {"external_api", "443"}
        });
    }
    
    void StartMonitoring() {
        // 파일 시스템 감시
        std::thread([this]() {
            FileSystemWatcher();
        }).detach();
        
        // 프로세스 감시
        std::thread([this]() {
            ProcessWatcher();
        }).detach();
        
        // 네트워크 연결 감시
        std::thread([this]() {
            NetworkWatcher();
        }).detach();
        
        // 로그 파일 감시
        std::thread([this]() {
            LogWatcher();
        }).detach();
    }
    
private:
    void FileSystemWatcher() {
        while (true) {
            auto changes = fim_.CheckForChanges();
            
            for (const auto& change : changes) {
                HandleFileSystemChange(change);
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    
    void ProcessWatcher() {
        while (true) {
            auto running_processes = process_monitor_.GetRunningProcesses();
            
            for (const auto& process : running_processes) {
                if (!process_monitor_.IsAllowedProcess(process)) {
                    HandleUnauthorizedProcess(process);
                }
                
                // CPU/메모리 사용량 이상 감지
                if (process.cpu_usage > 90.0 || process.memory_usage > 8192) { // 8GB
                    HandleResourceAnomalyProcess(process);
                }
            }
            
            std::this_thread::sleep_for(std::chrono::seconds(10));
        }
    }
    
    void HandleFileSystemChange(const FileSystemChange& change) {
        SecurityEvent event;
        event.type = EventType::FILE_SYSTEM_CHANGE;
        event.timestamp = std::chrono::system_clock::now();
        event.source = "host_ids";
        event.target = change.file_path;
        
        switch (change.type) {
            case ChangeType::MODIFIED:
                event.severity = SecurityLevel::MEDIUM;
                event.description = fmt::format("Critical file modified: {}", change.file_path);
                
                // 백업에서 파일 복원 시도
                if (IsCriticalSystemFile(change.file_path)) {
                    RestoreFromBackup(change.file_path);
                    event.metadata["action_taken"] = "restored_from_backup";
                }
                break;
                
            case ChangeType::DELETED:
                event.severity = SecurityLevel::HIGH;
                event.description = fmt::format("Critical file deleted: {}", change.file_path);
                
                // 즉시 복원
                RestoreFromBackup(change.file_path);
                event.metadata["action_taken"] = "immediately_restored";
                break;
                
            case ChangeType::CREATED:
                event.severity = SecurityLevel::LOW;
                event.description = fmt::format("New file created: {}", change.file_path);
                
                // 새 파일 스캔
                ScanNewFile(change.file_path);
                break;
        }
        
        LogSecurityEvent(event);
        
        // 심각한 변경사항은 즉시 알림
        if (event.severity >= SecurityLevel::HIGH) {
            SendImmediateAlert(event);
        }
    }
    
    void HandleUnauthorizedProcess(const ProcessInfo& process) {
        SecurityEvent event;
        event.type = EventType::UNAUTHORIZED_PROCESS;
        event.severity = SecurityLevel::HIGH;
        event.timestamp = std::chrono::system_clock::now();
        event.source = "host_ids";
        event.description = fmt::format("Unauthorized process detected: {} (PID: {})", 
                                       process.name, process.pid);
        
        event.metadata["process_name"] = process.name;
        event.metadata["process_id"] = process.pid;
        event.metadata["command_line"] = process.command_line;
        event.metadata["parent_process"] = process.parent_name;
        event.metadata["user"] = process.user;
        
        // 즉시 대응 조치
        if (IsHighRiskProcess(process)) {
            // 프로세스 강제 종료
            KillProcess(process.pid);
            event.metadata["action_taken"] = "process_terminated";
            
            // 실행 파일 격리
            QuarantineExecutable(process.executable_path);
            event.metadata["executable_quarantined"] = true;
        } else {
            // 프로세스 일시 중지하고 분석
            SuspendProcess(process.pid);
            AnalyzeProcess(process);
            event.metadata["action_taken"] = "process_suspended_for_analysis";
        }
        
        LogSecurityEvent(event);
        SendImmediateAlert(event);
    }
};
```

---

## 📋 4. 컴플라이언스 및 개인정보보호

### **4.1 GDPR 준수 시스템**

```cpp
#include <nlohmann/json.hpp>
#include <openssl/evp.h>

// GDPR(일반 데이터 보호 규정) 준수 시스템
class GDPRComplianceSystem {
private:
    // 개인정보 분류 및 태깅
    enum class DataCategory {
        PERSONAL_IDENTIFIABLE,     // 이름, 이메일, 전화번호
        SENSITIVE_PERSONAL,        // 생체 정보, 건강 정보
        BEHAVIORAL,               // 게임 플레이 패턴, 선호도
        TECHNICAL,                // IP 주소, 세션 ID
        FINANCIAL                 // 결제 정보, 구매 내역
    };
    
    struct PersonalDataItem {
        std::string data_id;
        UserId user_id;
        DataCategory category;
        std::string purpose;      // 수집 목적
        std::chrono::system_clock::time_point collected_at;
        std::chrono::system_clock::time_point expires_at;
        ConsentStatus consent_status;
        std::string legal_basis;  // 처리 법적 근거
        bool is_encrypted;
        std::vector<std::string> shared_with;  // 제3자 공유 내역
    };
    
    // 개인정보 인벤토리
    std::unordered_map<std::string, PersonalDataItem> data_inventory_;
    std::mutex inventory_mutex_;
    
    // 암호화 관리
    EncryptionManager encryption_manager_;
    
    // 동의 관리
    ConsentManager consent_manager_;
    
    // 감사 로그
    AuditLogger audit_logger_;
    
public:
    GDPRComplianceSystem() {
        InitializeDataCategories();
        InitializeEncryption();
        StartComplianceMonitoring();
    }
    
    // 개인정보 수집 시 GDPR 준수 확인
    bool CollectPersonalData(const PersonalDataRequest& request) {
        // 1. 동의 확인
        if (!consent_manager_.HasValidConsent(request.user_id, request.data_category)) {
            audit_logger_.LogComplianceEvent({
                .event_type = "DATA_COLLECTION_DENIED",
                .user_id = request.user_id,
                .reason = "NO_VALID_CONSENT",
                .data_category = DataCategoryToString(request.data_category)
            });
            return false;
        }
        
        // 2. 목적 제한 원칙 확인
        if (!IsValidPurpose(request.purpose, request.data_category)) {
            audit_logger_.LogComplianceEvent({
                .event_type = "DATA_COLLECTION_DENIED",
                .user_id = request.user_id,
                .reason = "INVALID_PURPOSE",
                .purpose = request.purpose
            });
            return false;
        }
        
        // 3. 데이터 최소화 원칙 확인
        if (!IsMinimalDataCollection(request)) {
            audit_logger_.LogComplianceEvent({
                .event_type = "DATA_COLLECTION_DENIED", 
                .user_id = request.user_id,
                .reason = "DATA_MINIMIZATION_VIOLATION"
            });
            return false;
        }
        
        // 4. 개인정보 등록 및 암호화
        auto data_item = RegisterPersonalData(request);
        if (RequiresEncryption(request.data_category)) {
            EncryptPersonalData(data_item);
        }
        
        audit_logger_.LogComplianceEvent({
            .event_type = "DATA_COLLECTED",
            .user_id = request.user_id,
            .data_id = data_item.data_id,
            .legal_basis = data_item.legal_basis
        });
        
        return true;
    }
    
    // 개인정보 삭제 권리 (Right to Erasure)
    bool ProcessErasureRequest(UserId user_id, const ErasureRequest& request) {
        std::lock_guard<std::mutex> lock(inventory_mutex_);
        
        std::vector<std::string> items_to_delete;
        
        // 사용자의 모든 개인정보 찾기
        for (const auto& [data_id, item] : data_inventory_) {
            if (item.user_id == user_id) {
                // 삭제 가능 여부 확인
                if (CanBeDeleted(item, request)) {
                    items_to_delete.push_back(data_id);
                }
            }
        }
        
        // 데이터 삭제 실행
        for (const auto& data_id : items_to_delete) {
            DeletePersonalDataItem(data_id);
        }
        
        // 삭제 증명서 생성
        auto certificate = GenerateDeletionCertificate(user_id, items_to_delete);
        
        audit_logger_.LogComplianceEvent({
            .event_type = "DATA_ERASED",
            .user_id = user_id,
            .items_deleted = static_cast<int>(items_to_delete.size()),
            .certificate_id = certificate.id
        });
        
        return !items_to_delete.empty();
    }
    
    // 개인정보 이동 권리 (Right to Data Portability)
    std::string ExportUserData(UserId user_id, const DataExportRequest& request) {
        std::lock_guard<std::mutex> lock(inventory_mutex_);
        
        nlohmann::json export_data;
        export_data["user_id"] = user_id;
        export_data["export_timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        export_data["format_version"] = "1.0";
        
        // 게임 프로필 데이터
        auto profile_data = GetUserProfileData(user_id);
        export_data["profile"] = profile_data;
        
        // 게임 진행 데이터
        auto game_data = GetUserGameData(user_id);
        export_data["game_progress"] = game_data;
        
        // 구매 내역 (비식별화)
        auto purchase_data = GetUserPurchaseHistory(user_id);
        export_data["purchases"] = AnonymizePurchaseData(purchase_data);
        
        // 소셜 데이터
        auto social_data = GetUserSocialData(user_id);
        export_data["social"] = social_data;
        
        // 기술적 데이터 (최근 30일)
        auto technical_data = GetUserTechnicalData(user_id, std::chrono::days(30));
        export_data["technical"] = technical_data;
        
        // 내보내기 증명서 생성
        auto certificate = GenerateExportCertificate(user_id, export_data);
        export_data["certificate"] = certificate;
        
        audit_logger_.LogComplianceEvent({
            .event_type = "DATA_EXPORTED",
            .user_id = user_id,
            .certificate_id = certificate["id"],
            .data_size = export_data.dump().size()
        });
        
        return export_data.dump(2);  // 예쁘게 포맷팅
    }
    
    // 개인정보 처리 목적 및 보존 기간 관리
    void ManageDataRetention() {
        auto now = std::chrono::system_clock::now();
        std::vector<std::string> expired_items;
        
        {
            std::lock_guard<std::mutex> lock(inventory_mutex_);
            
            for (const auto& [data_id, item] : data_inventory_) {
                if (now > item.expires_at) {
                    expired_items.push_back(data_id);
                }
            }
        }
        
        // 만료된 데이터 자동 삭제
        for (const auto& data_id : expired_items) {
            DeletePersonalDataItem(data_id);
            
            audit_logger_.LogComplianceEvent({
                .event_type = "DATA_AUTO_DELETED",
                .data_id = data_id,
                .reason = "RETENTION_PERIOD_EXPIRED"
            });
        }
        
        spdlog::info("Data retention cleanup: {} items deleted", expired_items.size());
    }
    
private:
    bool CanBeDeleted(const PersonalDataItem& item, const ErasureRequest& request) {
        // 법적 의무가 있는 데이터는 삭제 불가
        if (HasLegalObligation(item)) {
            return false;
        }
        
        // 진행 중인 계약 관련 데이터는 삭제 불가
        if (IsRequiredForContract(item)) {
            return false;
        }
        
        // 정당한 이익이 우선하는 경우
        if (HasOverridingLegitimateInterest(item)) {
            return false;
        }
        
        return true;
    }
    
    void EncryptPersonalData(PersonalDataItem& item) {
        if (item.is_encrypted) return;
        
        // AES-256-GCM으로 암호화
        auto encrypted_data = encryption_manager_.Encrypt(item.data_id);
        
        // 원본 데이터 삭제 후 암호화된 데이터로 교체
        ReplaceWithEncryptedData(item.data_id, encrypted_data);
        
        item.is_encrypted = true;
        
        audit_logger_.LogComplianceEvent({
            .event_type = "DATA_ENCRYPTED",
            .data_id = item.data_id,
            .encryption_algorithm = "AES-256-GCM"
        });
    }
    
    nlohmann::json AnonymizePurchaseData(const nlohmann::json& purchase_data) {
        nlohmann::json anonymized = purchase_data;
        
        // 개인 식별 정보 제거
        anonymized.erase("payment_method");
        anonymized.erase("billing_address");
        anonymized.erase("credit_card_info");
        
        // 금액 구간화 (정확한 금액 대신 범위로)
        for (auto& purchase : anonymized["items"]) {
            double amount = purchase["amount"];
            purchase["amount_range"] = GetAmountRange(amount);
            purchase.erase("amount");
        }
        
        return anonymized;
    }
};
```

### **4.2 SOC 2 준수 모니터링**

```cpp
// SOC 2 (Service Organization Control 2) 준수 시스템
class SOC2ComplianceMonitor {
private:
    // SOC 2 Trust Service Criteria
    enum class TrustCriteria {
        SECURITY,              // 보안
        AVAILABILITY,          // 가용성
        PROCESSING_INTEGRITY,  // 처리 무결성
        CONFIDENTIALITY,      // 기밀성
        PRIVACY               // 개인정보보호
    };
    
    struct ComplianceControl {
        std::string control_id;
        TrustCriteria criteria;
        std::string description;
        std::function<bool()> test_procedure;
        std::chrono::duration<int> test_frequency;
        std::chrono::system_clock::time_point last_tested;
        bool is_effective;
    };
    
    std::vector<ComplianceControl> controls_;
    ComplianceReporter reporter_;
    
public:
    SOC2ComplianceMonitor() {
        InitializeControls();
        StartContinuousMonitoring();
    }
    
    void InitializeControls() {
        // 보안 통제
        controls_.push_back({
            .control_id = "SEC-001",
            .criteria = TrustCriteria::SECURITY,
            .description = "Multi-factor authentication for administrative access",
            .test_procedure = [this]() { return TestMFAImplementation(); },
            .test_frequency = std::chrono::hours(24),
            .last_tested = std::chrono::system_clock::time_point{},
            .is_effective = false
        });
        
        controls_.push_back({
            .control_id = "SEC-002",
            .criteria = TrustCriteria::SECURITY,
            .description = "Network traffic encryption",
            .test_procedure = [this]() { return TestNetworkEncryption(); },
            .test_frequency = std::chrono::hours(8),
            .last_tested = std::chrono::system_clock::time_point{},
            .is_effective = false
        });
        
        // 가용성 통제
        controls_.push_back({
            .control_id = "AVL-001",
            .criteria = TrustCriteria::AVAILABILITY,
            .description = "System uptime monitoring and alerting",
            .test_procedure = [this]() { return TestUptimeMonitoring(); },
            .test_frequency = std::chrono::hours(1),
            .last_tested = std::chrono::system_clock::time_point{},
            .is_effective = false
        });
        
        controls_.push_back({
            .control_id = "AVL-002",
            .criteria = TrustCriteria::AVAILABILITY,
            .description = "Backup and recovery procedures",
            .test_procedure = [this]() { return TestBackupRecovery(); },
            .test_frequency = std::chrono::hours(24),
            .last_tested = std::chrono::system_clock::time_point{},
            .is_effective = false
        });
        
        // 처리 무결성 통제
        controls_.push_back({
            .control_id = "PI-001",
            .criteria = TrustCriteria::PROCESSING_INTEGRITY,
            .description = "Transaction processing completeness and accuracy",
            .test_procedure = [this]() { return TestTransactionIntegrity(); },
            .test_frequency = std::chrono::hours(4),
            .last_tested = std::chrono::system_clock::time_point{},
            .is_effective = false
        });
        
        // 기밀성 통제
        controls_.push_back({
            .control_id = "CON-001",
            .criteria = TrustCriteria::CONFIDENTIALITY,
            .description = "Data encryption at rest",
            .test_procedure = [this]() { return TestDataEncryptionAtRest(); },
            .test_frequency = std::chrono::hours(12),
            .last_tested = std::chrono::system_clock::time_point{},
            .is_effective = false
        });
        
        // 개인정보보호 통제
        controls_.push_back({
            .control_id = "PRI-001",
            .criteria = TrustCriteria::PRIVACY,
            .description = "Personal data access controls",
            .test_procedure = [this]() { return TestPersonalDataAccess(); },
            .test_frequency = std::chrono::hours(6),
            .last_tested = std::chrono::system_clock::time_point{},
            .is_effective = false
        });
    }
    
    void StartContinuousMonitoring() {
        std::thread([this]() {
            ContinuousComplianceTesting();
        }).detach();
        
        std::thread([this]() {
            GenerateComplianceReports();
        }).detach();
    }
    
private:
    void ContinuousComplianceTesting() {
        while (true) {
            auto now = std::chrono::system_clock::now();
            
            for (auto& control : controls_) {
                if (now - control.last_tested >= control.test_frequency) {
                    // 통제 테스트 실행
                    bool test_result = control.test_procedure();
                    
                    control.last_tested = now;
                    control.is_effective = test_result;
                    
                    // 테스트 결과 로깅
                    LogControlTest({
                        .control_id = control.control_id,
                        .test_timestamp = now,
                        .result = test_result ? "PASS" : "FAIL",
                        .criteria = control.criteria
                    });
                    
                    // 실패한 통제에 대한 즉시 알림
                    if (!test_result) {
                        HandleControlFailure(control);
                    }
                }
            }
            
            std::this_thread::sleep_for(std::chrono::minutes(10));
        }
    }
    
    bool TestMFAImplementation() {
        // 관리자 계정의 MFA 활성화 여부 확인
        auto admin_accounts = GetAdminAccounts();
        
        for (const auto& account : admin_accounts) {
            if (!account.HasMFAEnabled()) {
                spdlog::error("Admin account {} does not have MFA enabled", account.username);
                return false;
            }
            
            // 최근 MFA 사용 여부 확인
            auto recent_logins = GetRecentLogins(account.user_id, std::chrono::hours(24));
            for (const auto& login : recent_logins) {
                if (!login.used_mfa) {
                    spdlog::error("Admin login without MFA detected for {}", account.username);
                    return false;
                }
            }
        }
        
        return true;
    }
    
    bool TestNetworkEncryption() {
        // 모든 네트워크 연결의 암호화 상태 확인
        auto network_connections = GetActiveNetworkConnections();
        
        for (const auto& connection : network_connections) {
            if (!connection.is_encrypted) {
                spdlog::error("Unencrypted connection detected: {} -> {}", 
                             connection.source, connection.destination);
                return false;
            }
            
            // TLS 버전 확인
            if (connection.tls_version < TLS_VERSION_1_2) {
                spdlog::error("Outdated TLS version {} in use", connection.tls_version);
                return false;
            }
        }
        
        return true;
    }
    
    bool TestUptimeMonitoring() {
        // 시스템 가동률 확인
        auto uptime_stats = GetUptimeStatistics(std::chrono::hours(24));
        
        if (uptime_stats.availability_percentage < 99.9) {
            spdlog::error("System availability {} below required threshold", 
                         uptime_stats.availability_percentage);
            return false;
        }
        
        // 모니터링 시스템 자체의 상태 확인
        if (!IsMonitoringSystemHealthy()) {
            spdlog::error("Monitoring system is not functioning properly");
            return false;
        }
        
        return true;
    }
    
    bool TestBackupRecovery() {
        // 자동 백업 실행 여부 확인
        auto latest_backup = GetLatestBackup();
        auto backup_age = std::chrono::system_clock::now() - latest_backup.timestamp;
        
        if (backup_age > std::chrono::hours(24)) {
            spdlog::error("Latest backup is {} hours old, exceeding 24 hour threshold", 
                         std::chrono::duration_cast<std::chrono::hours>(backup_age).count());
            return false;
        }
        
        // 백업 무결성 검증
        if (!VerifyBackupIntegrity(latest_backup)) {
            spdlog::error("Backup integrity verification failed");
            return false;
        }
        
        // 복구 테스트 (주 1회)
        if (ShouldPerformRecoveryTest()) {
            return PerformRecoveryTest();
        }
        
        return true;
    }
    
    bool TestTransactionIntegrity() {
        // 게임 내 거래 무결성 확인
        auto recent_transactions = GetRecentTransactions(std::chrono::hours(1));
        
        for (const auto& transaction : recent_transactions) {
            // 거래 완전성 확인
            if (!VerifyTransactionCompleteness(transaction)) {
                spdlog::error("Incomplete transaction detected: {}", transaction.id);
                return false;
            }
            
            // 거래 정확성 확인
            if (!VerifyTransactionAccuracy(transaction)) {
                spdlog::error("Transaction accuracy check failed: {}", transaction.id);
                return false;
            }
            
            // 이중 지불 방지 확인
            if (HasDuplicateTransaction(transaction)) {
                spdlog::error("Duplicate transaction detected: {}", transaction.id);
                return false;
            }
        }
        
        return true;
    }
    
    void HandleControlFailure(const ComplianceControl& control) {
        // 즉시 보안팀에 알림
        SendComplianceAlert({
            .severity = AlertSeverity::HIGH,
            .control_id = control.control_id,
            .criteria = control.criteria,
            .description = control.description,
            .failure_time = std::chrono::system_clock::now()
        });
        
        // 자동 수정 조치 시도
        AttemptAutoRemediation(control);
        
        // 컴플라이언스 대시보드 업데이트
        UpdateComplianceDashboard(control.control_id, false);
    }
    
    void GenerateComplianceReports() {
        while (true) {
            // 매주 컴플라이언스 보고서 생성
            std::this_thread::sleep_for(std::chrono::hours(24 * 7));
            
            auto report = GenerateWeeklyComplianceReport();
            SaveComplianceReport(report);
            
            // 외부 감사인에게 보고서 전송
            if (IsExternalAuditPeriod()) {
                SendReportToAuditors(report);
            }
        }
    }
    
    ComplianceReport GenerateWeeklyComplianceReport() {
        ComplianceReport report;
        report.report_period_start = std::chrono::system_clock::now() - std::chrono::hours(24 * 7);
        report.report_period_end = std::chrono::system_clock::now();
        
        // 각 Trust Service Criteria별 준수 현황
        for (auto criteria : {TrustCriteria::SECURITY, TrustCriteria::AVAILABILITY, 
                              TrustCriteria::PROCESSING_INTEGRITY, TrustCriteria::CONFIDENTIALITY, 
                              TrustCriteria::PRIVACY}) {
            
            auto criteria_controls = GetControlsByCriteria(criteria);
            auto effectiveness_rate = CalculateEffectivenessRate(criteria_controls);
            
            report.criteria_effectiveness[criteria] = effectiveness_rate;
        }
        
        // 예외사항 및 개선 계획
        report.exceptions = GetRecentControlFailures();
        report.remediation_plans = GetRemediationPlans();
        
        return report;
    }
};
```

---

## 🚨 5. 자동화된 보안 대응 시스템

### **5.1 SOAR (Security Orchestration, Automation and Response)**

```cpp
// 보안 오케스트레이션 및 자동 대응 시스템
class SecuritySOAR {
private:
    struct SecurityPlaybook {
        std::string playbook_id;
        ThreatType threat_type;
        std::vector<AutomatedAction> actions;
        std::chrono::duration<int> max_execution_time;
        Priority priority;
    };
    
    struct AutomatedAction {
        std::string action_id;
        ActionType type;
        std::map<std::string, std::string> parameters;
        std::function<bool(const SecurityIncident&)> condition;
        std::function<ActionResult(const SecurityIncident&)> execute;
    };
    
    std::unordered_map<ThreatType, SecurityPlaybook> playbooks_;
    IncidentQueue incident_queue_;
    ResponseOrchestrator orchestrator_;
    
public:
    SecuritySOAR() {
        InitializePlaybooks();
        StartResponseEngine();
    }
    
    void InitializePlaybooks() {
        // DDoS 공격 대응 플레이북
        SecurityPlaybook ddos_playbook;
        ddos_playbook.playbook_id = "DDOS-RESPONSE-001";
        ddos_playbook.threat_type = ThreatType::DDOS_ATTACK;
        ddos_playbook.priority = Priority::CRITICAL;
        ddos_playbook.max_execution_time = std::chrono::minutes(5);
        
        ddos_playbook.actions = {
            {
                .action_id = "DDOS-01",
                .type = ActionType::NETWORK_BLOCKING,
                .parameters = {{"block_duration", "3600"}, {"rule_type", "ip_based"}},
                .condition = [](const SecurityIncident& incident) {
                    return incident.metadata.contains("source_ips") && 
                           incident.severity >= SecurityLevel::HIGH;
                },
                .execute = [this](const SecurityIncident& incident) {
                    return BlockMaliciousIPs(incident);
                }
            },
            {
                .action_id = "DDOS-02", 
                .type = ActionType::TRAFFIC_SHAPING,
                .parameters = {{"rate_limit", "1000"}, {"burst_size", "100"}},
                .condition = [](const SecurityIncident& incident) {
                    return incident.GetMetric("packets_per_second") > 50000;
                },
                .execute = [this](const SecurityIncident& incident) {
                    return ApplyTrafficShaping(incident);
                }
            },
            {
                .action_id = "DDOS-03",
                .type = ActionType::CDN_ACTIVATION,
                .parameters = {{"protection_level", "aggressive"}, {"challenge_rate", "high"}},
                .condition = [](const SecurityIncident& incident) {
                    return incident.GetDuration() > std::chrono::minutes(2);
                },
                .execute = [this](const SecurityIncident& incident) {
                    return ActivateCDNProtection(incident);
                }
            },
            {
                .action_id = "DDOS-04",
                .type = ActionType::NOTIFICATION,
                .parameters = {{"recipients", "security-team,ops-team"}, {"severity", "critical"}},
                .condition = [](const SecurityIncident& incident) { return true; },
                .execute = [this](const SecurityIncident& incident) {
                    return NotifySecurityTeam(incident);
                }
            }
        };
        
        playbooks_[ThreatType::DDOS_ATTACK] = ddos_playbook;
        
        // 치팅 탐지 대응 플레이북
        SecurityPlaybook cheat_playbook;
        cheat_playbook.playbook_id = "CHEAT-RESPONSE-001";
        cheat_playbook.threat_type = ThreatType::CHEAT_DETECTION;
        cheat_playbook.priority = Priority::HIGH;
        cheat_playbook.max_execution_time = std::chrono::minutes(2);
        
        cheat_playbook.actions = {
            {
                .action_id = "CHEAT-01",
                .type = ActionType::PLAYER_ISOLATION,
                .parameters = {{"isolation_type", "sandbox"}, {"duration", "pending_investigation"}},
                .condition = [](const SecurityIncident& incident) {
                    return incident.confidence_score > 0.9;
                },
                .execute = [this](const SecurityIncident& incident) {
                    return IsolatePlayer(incident);
                }
            },
            {
                .action_id = "CHEAT-02",
                .type = ActionType::DATA_COLLECTION,
                .parameters = {{"collect_duration", "300"}, {"data_types", "movement,combat,inventory"}},
                .condition = [](const SecurityIncident& incident) { return true; },
                .execute = [this](const SecurityIncident& incident) {
                    return CollectPlayerData(incident);
                }
            },
            {
                .action_id = "CHEAT-03",
                .type = ActionType::ACCOUNT_FLAGGING,
                .parameters = {{"flag_type", "cheat_suspected"}, {"review_required", "true"}},
                .condition = [](const SecurityIncident& incident) { return true; },
                .execute = [this](const SecurityIncident& incident) {
                    return FlagPlayerAccount(incident);
                }
            }
        };
        
        playbooks_[ThreatType::CHEAT_DETECTION] = cheat_playbook;
        
        // 계정 탈취 대응 플레이북
        SecurityPlaybook account_takeover_playbook;
        account_takeover_playbook.playbook_id = "ATO-RESPONSE-001";
        account_takeover_playbook.threat_type = ThreatType::ACCOUNT_TAKEOVER;
        account_takeover_playbook.priority = Priority::HIGH;
        account_takeover_playbook.max_execution_time = std::chrono::minutes(3);
        
        account_takeover_playbook.actions = {
            {
                .action_id = "ATO-01",
                .type = ActionType::ACCOUNT_LOCKDOWN,
                .parameters = {{"lock_type", "temporary"}, {"duration", "1800"}},
                .condition = [](const SecurityIncident& incident) { return true; },
                .execute = [this](const SecurityIncident& incident) {
                    return LockdownAccount(incident);
                }
            },
            {
                .action_id = "ATO-02",
                .type = ActionType::AUTHENTICATION_CHALLENGE,
                .parameters = {{"challenge_type", "email_verification"}, {"expires_in", "3600"}},
                .condition = [](const SecurityIncident& incident) { return true; },
                .execute = [this](const SecurityIncident& incident) {
                    return SendAuthenticationChallenge(incident);
                }
            },
            {
                .action_id = "ATO-03",
                .type = ActionType::SESSION_TERMINATION,
                .parameters = {{"scope", "all_sessions"}, {"force", "true"}},
                .condition = [](const SecurityIncident& incident) { return true; },
                .execute = [this](const SecurityIncident& incident) {
                    return TerminateAllSessions(incident);
                }
            }
        };
        
        playbooks_[ThreatType::ACCOUNT_TAKEOVER] = account_takeover_playbook;
    }
    
    void ProcessSecurityIncident(const SecurityIncident& incident) {
        // 적절한 플레이북 선택
        auto playbook_it = playbooks_.find(incident.threat_type);
        if (playbook_it == playbooks_.end()) {
            spdlog::warn("No playbook found for threat type: {}", 
                        ThreatTypeToString(incident.threat_type));
            HandleUnknownThreat(incident);
            return;
        }
        
        const auto& playbook = playbook_it->second;
        
        // 응답 실행
        ExecutePlaybook(playbook, incident);
    }
    
private:
    void ExecutePlaybook(const SecurityPlaybook& playbook, const SecurityIncident& incident) {
        PlaybookExecution execution;
        execution.playbook_id = playbook.playbook_id;
        execution.incident_id = incident.incident_id;
        execution.start_time = std::chrono::system_clock::now();
        
        spdlog::info("Executing security playbook {} for incident {}", 
                    playbook.playbook_id, incident.incident_id);
        
        // 병렬로 실행 가능한 액션들 식별
        auto action_groups = GroupActionsByDependency(playbook.actions);
        
        for (const auto& action_group : action_groups) {
            // 각 그룹 내 액션들을 병렬 실행
            std::vector<std::future<ActionResult>> action_futures;
            
            for (const auto& action : action_group) {
                if (action.condition(incident)) {
                    auto future = std::async(std::launch::async, [&action, &incident]() {
                        return action.execute(incident);
                    });
                    action_futures.push_back(std::move(future));
                }
            }
            
            // 모든 액션 완료 대기
            for (auto& future : action_futures) {
                try {
                    auto result = future.get();
                    execution.action_results.push_back(result);
                    
                    if (!result.success) {
                        spdlog::error("Action {} failed: {}", result.action_id, result.error_message);
                    }
                } catch (const std::exception& e) {
                    spdlog::error("Exception in action execution: {}", e.what());
                }
            }
        }
        
        execution.end_time = std::chrono::system_clock::now();
        execution.total_duration = execution.end_time - execution.start_time;
        
        // 실행 결과 로깅
        LogPlaybookExecution(execution);
        
        // 효과성 평가
        EvaluatePlaybookEffectiveness(playbook, incident, execution);
    }
    
    ActionResult BlockMaliciousIPs(const SecurityIncident& incident) {
        ActionResult result;
        result.action_id = "DDOS-01";
        result.start_time = std::chrono::system_clock::now();
        
        try {
            auto source_ips = incident.GetSourceIPs();
            
            for (const auto& ip : source_ips) {
                // 방화벽 규칙 추가
                if (!firewall_manager_.BlockIP(ip, std::chrono::hours(1))) {
                    result.success = false;
                    result.error_message += fmt::format("Failed to block IP: {}; ", ip);
                    continue;
                }
                
                // CDN에도 차단 요청
                cdn_manager_.BlockIP(ip);
                
                // 상위 ISP에 알림 (심각한 경우)
                if (incident.severity >= SecurityLevel::CRITICAL) {
                    NotifyISP(ip, incident);
                }
            }
            
            result.success = result.error_message.empty();
            result.metadata["blocked_ips"] = nlohmann::json(source_ips);
            
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = e.what();
        }
        
        result.end_time = std::chrono::system_clock::now();
        return result;
    }
    
    ActionResult IsolatePlayer(const SecurityIncident& incident) {
        ActionResult result;
        result.action_id = "CHEAT-01";
        result.start_time = std::chrono::system_clock::now();
        
        try {
            auto player_id = incident.GetPlayerID();
            
            // 플레이어를 샌드박스 서버로 이동
            if (!game_server_.MovePlayerToSandbox(player_id)) {
                result.success = false;
                result.error_message = "Failed to move player to sandbox";
                return result;
            }
            
            // 다른 플레이어와의 상호작용 차단
            game_server_.DisablePlayerInteractions(player_id);
            
            // 경제 활동 제한
            game_server_.FreezePlayerEconomy(player_id);
            
            result.success = true;
            result.metadata["isolation_server"] = game_server_.GetSandboxServerID();
            result.metadata["player_id"] = player_id;
            
        } catch (const std::exception& e) {
            result.success = false;
            result.error_message = e.what();
        }
        
        result.end_time = std::chrono::system_clock::now();
        return result;
    }
    
    void EvaluatePlaybookEffectiveness(const SecurityPlaybook& playbook, 
                                     const SecurityIncident& incident, 
                                     const PlaybookExecution& execution) {
        EffectivenessMetrics metrics;
        
        // 응답 시간 평가
        metrics.response_time = execution.total_duration;
        metrics.meets_sla = execution.total_duration <= playbook.max_execution_time;
        
        // 위협 완화 여부 확인
        auto post_incident_metrics = GetPostIncidentMetrics(incident, std::chrono::minutes(10));
        metrics.threat_mitigated = post_incident_metrics.threat_level <= ThreatLevel::LOW;
        
        // 비즈니스 영향 평가
        metrics.business_impact = CalculateBusinessImpact(incident, execution);
        
        // 플레이북 개선 제안 생성
        if (!metrics.meets_sla || !metrics.threat_mitigated) {
            GeneratePlaybookImprovementRecommendations(playbook, metrics);
        }
        
        // 성과 데이터베이스에 저장 (머신러닝 학습용)
        StoreEffectivenessMetrics(playbook.playbook_id, incident.incident_id, metrics);
    }
};
```

---

## 📊 6. 보안 메트릭 및 KPI 대시보드

### **6.1 실시간 보안 대시보드**

```cpp
// 실시간 보안 메트릭 수집 및 대시보드
class SecurityMetricsDashboard {
private:
    struct SecurityKPI {
        std::string metric_name;
        double current_value;
        double target_value;
        double threshold_warning;
        double threshold_critical;
        TrendDirection trend;
        std::chrono::system_clock::time_point last_updated;
    };
    
    std::unordered_map<std::string, SecurityKPI> kpis_;
    MetricsCollector collector_;
    DashboardRenderer renderer_;
    AlertManager alert_manager_;
    
public:
    SecurityMetricsDashboard() {
        InitializeKPIs();
        StartMetricsCollection();
        StartDashboardUpdates();
    }
    
    void InitializeKPIs() {
        // 보안 사건 대응 시간 (MTTR - Mean Time To Response)
        kpis_["mttr"] = {
            .metric_name = "Mean Time To Response",
            .current_value = 0.0,
            .target_value = 300.0,     // 5분 목표
            .threshold_warning = 600.0, // 10분 경고
            .threshold_critical = 1800.0, // 30분 위험
            .trend = TrendDirection::STABLE,
            .last_updated = std::chrono::system_clock::now()
        };
        
        // 보안 사건 해결 시간 (MTTR - Mean Time To Resolution)
        kpis_["mttres"] = {
            .metric_name = "Mean Time To Resolution",
            .current_value = 0.0,
            .target_value = 3600.0,    // 1시간 목표
            .threshold_warning = 7200.0, // 2시간 경고
            .threshold_critical = 14400.0, // 4시간 위험
            .trend = TrendDirection::STABLE,
            .last_updated = std::chrono::system_clock::now()
        };
        
        // 보안 사건 발생률
        kpis_["incident_rate"] = {
            .metric_name = "Security Incidents per Hour",
            .current_value = 0.0,
            .target_value = 5.0,       // 시간당 5건 이하
            .threshold_warning = 10.0,  // 시간당 10건 경고
            .threshold_critical = 20.0, // 시간당 20건 위험
            .trend = TrendDirection::STABLE,
            .last_updated = std::chrono::system_clock::now()
        };
        
        // 치팅 탐지 정확도
        kpis_["cheat_detection_accuracy"] = {
            .metric_name = "Cheat Detection Accuracy (%)",
            .current_value = 0.0,
            .target_value = 95.0,      // 95% 목표
            .threshold_warning = 90.0,  // 90% 이하 경고
            .threshold_critical = 85.0, // 85% 이하 위험
            .trend = TrendDirection::STABLE,
            .last_updated = std::chrono::system_clock::now()
        };
        
        // 거짓 양성률 (False Positive Rate)
        kpis_["false_positive_rate"] = {
            .metric_name = "False Positive Rate (%)",
            .current_value = 0.0,
            .target_value = 2.0,       // 2% 이하 목표
            .threshold_warning = 5.0,   // 5% 이상 경고
            .threshold_critical = 10.0, // 10% 이상 위험
            .trend = TrendDirection::STABLE,
            .last_updated = std::chrono::system_clock::now()
        };
        
        // 취약점 패치 시간
        kpis_["vulnerability_patch_time"] = {
            .metric_name = "Mean Vulnerability Patch Time (hours)",
            .current_value = 0.0,
            .target_value = 24.0,      // 24시간 목표
            .threshold_warning = 72.0,  // 3일 경고
            .threshold_critical = 168.0, // 1주 위험
            .trend = TrendDirection::STABLE,
            .last_updated = std::chrono::system_clock::now()
        };
        
        // 컴플라이언스 준수율
        kpis_["compliance_score"] = {
            .metric_name = "Compliance Score (%)",
            .current_value = 0.0,
            .target_value = 98.0,      // 98% 목표
            .threshold_warning = 95.0,  // 95% 이하 경고
            .threshold_critical = 90.0, // 90% 이하 위험
            .trend = TrendDirection::STABLE,
            .last_updated = std::chrono::system_clock::now()
        };
    }
    
    void StartMetricsCollection() {
        // 실시간 메트릭 수집 스레드
        std::thread([this]() {
            CollectRealTimeMetrics();
        }).detach();
        
        // 주기적 메트릭 계산 스레드
        std::thread([this]() {
            CalculatePeriodicMetrics();
        }).detach();
    }
    
    void StartDashboardUpdates() {
        // 대시보드 업데이트 스레드
        std::thread([this]() {
            UpdateDashboard();
        }).detach();
        
        // 알럿 모니터링 스레드
        std::thread([this]() {
            MonitorAlertThresholds();
        }).detach();
    }
    
private:
    void CollectRealTimeMetrics() {
        while (true) {
            auto now = std::chrono::system_clock::now();
            
            // 보안 사건 발생률 계산
            auto recent_incidents = GetRecentIncidents(std::chrono::hours(1));
            UpdateKPI("incident_rate", static_cast<double>(recent_incidents.size()));
            
            // 활성 위협 수 계산
            auto active_threats = GetActiveThreats();
            UpdateKPI("active_threats", static_cast<double>(active_threats.size()));
            
            // 시스템 보안 상태 점수 계산
            auto security_score = CalculateOverallSecurityScore();
            UpdateKPI("security_score", security_score);
            
            std::this_thread::sleep_for(std::chrono::minutes(1));
        }
    }
    
    void CalculatePeriodicMetrics() {
        while (true) {
            // 24시간마다 계산하는 메트릭들
            std::this_thread::sleep_for(std::chrono::hours(1));
            
            auto now = std::chrono::system_clock::now();
            auto day_ago = now - std::chrono::hours(24);
            
            // MTTR 계산 (평균 대응 시간)
            auto incidents = GetIncidentsInRange(day_ago, now);
            if (!incidents.empty()) {
                double total_response_time = 0.0;
                double total_resolution_time = 0.0;
                int valid_incidents = 0;
                
                for (const auto& incident : incidents) {
                    if (incident.response_time.has_value() && incident.resolution_time.has_value()) {
                        total_response_time += std::chrono::duration<double>(
                            incident.response_time.value()).count();
                        total_resolution_time += std::chrono::duration<double>(
                            incident.resolution_time.value()).count();
                        valid_incidents++;
                    }
                }
                
                if (valid_incidents > 0) {
                    UpdateKPI("mttr", total_response_time / valid_incidents);
                    UpdateKPI("mttres", total_resolution_time / valid_incidents);
                }
            }
            
            // 치팅 탐지 정확도 계산
            auto cheat_detections = GetCheatDetections(day_ago, now);
            if (!cheat_detections.empty()) {
                int true_positives = 0;
                int false_positives = 0;
                
                for (const auto& detection : cheat_detections) {
                    if (detection.verified_result.has_value()) {
                        if (detection.verified_result.value()) {
                            true_positives++;
                        } else {
                            false_positives++;
                        }
                    }
                }
                
                int total_detections = true_positives + false_positives;
                if (total_detections > 0) {
                    double accuracy = (double)true_positives / total_detections * 100.0;
                    double fp_rate = (double)false_positives / total_detections * 100.0;
                    
                    UpdateKPI("cheat_detection_accuracy", accuracy);
                    UpdateKPI("false_positive_rate", fp_rate);
                }
            }
            
            // 컴플라이언스 점수 계산
            auto compliance_score = CalculateComplianceScore();
            UpdateKPI("compliance_score", compliance_score);
        }
    }
    
    void UpdateKPI(const std::string& metric_name, double new_value) {
        auto it = kpis_.find(metric_name);
        if (it != kpis_.end()) {
            auto& kpi = it->second;
            
            // 트렌드 계산
            if (new_value > kpi.current_value) {
                kpi.trend = TrendDirection::INCREASING;
            } else if (new_value < kpi.current_value) {
                kpi.trend = TrendDirection::DECREASING;
            } else {
                kpi.trend = TrendDirection::STABLE;
            }
            
            kpi.current_value = new_value;
            kpi.last_updated = std::chrono::system_clock::now();
            
            // 임계값 확인
            CheckThresholds(kpi);
        }
    }
    
    void CheckThresholds(const SecurityKPI& kpi) {
        if (kpi.current_value >= kpi.threshold_critical) {
            alert_manager_.SendAlert({
                .severity = AlertSeverity::CRITICAL,
                .metric_name = kpi.metric_name,
                .current_value = kpi.current_value,
                .threshold = kpi.threshold_critical,
                .message = fmt::format("{} has reached critical threshold: {} >= {}", 
                                     kpi.metric_name, kpi.current_value, kpi.threshold_critical)
            });
        } else if (kpi.current_value >= kpi.threshold_warning) {
            alert_manager_.SendAlert({
                .severity = AlertSeverity::WARNING,
                .metric_name = kpi.metric_name,
                .current_value = kpi.current_value,
                .threshold = kpi.threshold_warning,
                .message = fmt::format("{} has reached warning threshold: {} >= {}", 
                                     kpi.metric_name, kpi.current_value, kpi.threshold_warning)
            });
        }
    }
    
    void UpdateDashboard() {
        while (true) {
            // 대시보드 HTML 생성
            auto dashboard_html = GenerateDashboardHTML();
            
            // 웹 대시보드 업데이트
            web_server_.UpdateDashboard(dashboard_html);
            
            // Grafana 메트릭 전송
            SendMetricsToGrafana();
            
            // Slack/Teams 요약 전송 (매 시간)
            static int update_count = 0;
            if (++update_count % 60 == 0) {  // 60번째 업데이트마다 (1시간)
                SendHourlySecuritySummary();
            }
            
            std::this_thread::sleep_for(std::chrono::minutes(1));
        }
    }
    
    std::string GenerateDashboardHTML() {
        nlohmann::json dashboard_data;
        dashboard_data["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        // KPI 데이터 추가
        for (const auto& [name, kpi] : kpis_) {
            nlohmann::json kpi_data;
            kpi_data["name"] = kpi.metric_name;
            kpi_data["current_value"] = kpi.current_value;
            kpi_data["target_value"] = kpi.target_value;
            kpi_data["status"] = GetKPIStatus(kpi);
            kpi_data["trend"] = TrendDirectionToString(kpi.trend);
            
            dashboard_data["kpis"][name] = kpi_data;
        }
        
        // 최근 보안 이벤트
        auto recent_events = GetRecentSecurityEvents(10);
        for (const auto& event : recent_events) {
            nlohmann::json event_data;
            event_data["type"] = EventTypeToString(event.type);
            event_data["severity"] = SeverityToString(event.severity);
            event_data["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                event.timestamp.time_since_epoch()).count();
            event_data["description"] = event.description;
            
            dashboard_data["recent_events"].push_back(event_data);
        }
        
        // 위협 맵 (지리적 분포)
        auto threat_map_data = GenerateThreatMapData();
        dashboard_data["threat_map"] = threat_map_data;
        
        // HTML 템플릿에 데이터 삽입
        return renderer_.RenderDashboard(dashboard_data);
    }
    
    double CalculateOverallSecurityScore() {
        double score = 100.0;  // 기본 점수
        
        // 각 KPI의 목표 달성도를 종합하여 점수 계산
        for (const auto& [name, kpi] : kpis_) {
            double achievement_rate = std::min(kpi.current_value / kpi.target_value, 1.0);
            double weight = GetKPIWeight(name);
            
            // 목표 달성률에 따른 점수 조정
            if (achievement_rate < 0.8) {  // 80% 미만 달성
                score -= (1.0 - achievement_rate) * weight * 20.0;
            }
        }
        
        // 최근 보안 사건 수에 따른 점수 조정
        auto recent_incidents = GetRecentIncidents(std::chrono::hours(24));
        if (recent_incidents.size() > 10) {
            score -= (recent_incidents.size() - 10) * 2.0;  // 10건 초과 시 건당 2점 감점
        }
        
        return std::max(score, 0.0);  // 최소 0점
    }
    
    void SendHourlySecuritySummary() {
        auto now = std::chrono::system_clock::now();
        auto hour_ago = now - std::chrono::hours(1);
        
        SecuritySummary summary;
        summary.period_start = hour_ago;
        summary.period_end = now;
        summary.total_incidents = GetIncidentsInRange(hour_ago, now).size();
        summary.critical_incidents = GetCriticalIncidentsInRange(hour_ago, now).size();
        summary.threats_mitigated = GetMitigatedThreatsInRange(hour_ago, now).size();
        summary.overall_security_score = CalculateOverallSecurityScore();
        
        // Slack/Teams 메시지 전송
        std::string message = fmt::format(
            "🛡️ **시간별 보안 요약** ({})\n"
            "• 전체 보안 점수: {:.1f}/100\n"
            "• 보안 사건: {}건 (위험: {}건)\n"
            "• 완화된 위협: {}건\n"
            "• 평균 대응 시간: {:.1f}분\n",
            FormatTimeRange(hour_ago, now),
            summary.overall_security_score,
            summary.total_incidents,
            summary.critical_incidents,
            summary.threats_mitigated,
            kpis_["mttr"].current_value / 60.0
        );
        
        notification_service_.SendToSlack(message);
        notification_service_.SendToTeams(message);
    }
};
```

---

## 🎯 마무리

**🎉 축하합니다!** 이제 여러분은 **엔터프라이즈급 게임 서버 보안 시스템**을 마스터했습니다!

### **🛡️ 지금 구축할 수 있는 것들:**
- ✅ **Zero Trust 아키텍처**: 절대 신뢰하지 않는 보안 모델
- ✅ **SIEM 시스템**: 실시간 위협 탐지 및 분석
- ✅ **IDS/IPS**: 네트워크와 호스트 기반 침입 차단
- ✅ **GDPR/SOC2 준수**: 국제 규정 완벽 대응
- ✅ **자동화된 대응**: SOAR로 즉시 위협 차단
- ✅ **실시간 모니터링**: 보안 KPI 대시보드

### **💰 실제 비즈니스 가치:**
- **보안 사고 예방**: 연간 수백억원 손실 방지
- **규정 준수**: GDPR/개인정보보호법 위반 시 최대 300억원 과징금 회피
- **신뢰도 향상**: 사용자 신뢰 확보로 매출 증대
- **운영 효율성**: 자동화로 보안팀 업무 80% 절약

### **🚀 다음 단계 추천:**
1. **이 문서의 보안 시스템 구현** (4-6주)
2. **실제 게임 서버에 적용** 및 테스트
3. **보안 인증 취득** (ISO 27001, SOC 2)
4. **포트폴리오 완성** 후 **CISO/보안 아키텍트** 포지션 지원

### **💡 핵심 성취:**
- **엔터프라이즈 보안 전문가** 수준의 역량
- **글로벌 게임사** 보안 담당자 자격
- **연봉 1억+** 보안 아키텍트/CISO 경쟁력

**🔐 보안은 게임 서비스의 생명선입니다. 이 지식으로 여러분의 게임 서버를 철통같이 지켜보세요!**

---

## 🔥 흔한 실수와 해결방법 (Common Mistakes & Solutions)

### 1. 클라이언트 신뢰로 인한 보안 취약점
```cpp
// [SEQUENCE: 1] ❌ 잘못된 예: 클라이언트가 보낸 데이터 무조건 신뢰
class BadGameServer {
    void OnPlayerAttack(PlayerID attacker, PlayerID target, int damage) {
        // 클라이언트가 보낸 데미지 그대로 적용 - 치팅 가능!
        players[target].health -= damage;
        
        // 클라이언트가 보낸 위치 그대로 신뢰
        players[attacker].position = client_reported_position;
    }
};

// ✅ 올바른 예: 서버 권위 방식
class SecureGameServer {
    void OnPlayerAttack(PlayerID attacker, PlayerID target) {
        // 서버에서 모든 것을 검증하고 계산
        if (!CanAttack(attacker, target)) return;
        
        float distance = CalculateDistance(attacker, target);
        if (distance > GetWeaponRange(attacker)) return;
        
        // 서버가 데미지 계산
        int damage = CalculateDamage(attacker, target);
        ApplyDamage(target, damage);
        
        // 위치는 서버가 관리
        UpdatePosition(attacker, server_calculated_position);
    }
};
```

### 2. 약한 암호화와 키 관리
```cpp
// [SEQUENCE: 2] ❌ 잘못된 예: 하드코딩된 키와 약한 암호화
class BadEncryption {
    const std::string SECRET_KEY = "MySecretKey123";  // 소스코드에 노출!
    
    std::string Encrypt(const std::string& data) {
        // XOR 암호화 - 너무 약함!
        std::string encrypted = data;
        for (size_t i = 0; i < data.size(); i++) {
            encrypted[i] ^= SECRET_KEY[i % SECRET_KEY.size()];
        }
        return encrypted;
    }
};

// ✅ 올바른 예: 강력한 암호화와 안전한 키 관리
class SecureEncryption {
    SecureString encryption_key;  // 메모리 보호된 키
    
    SecureEncryption() {
        // HSM이나 키 관리 서비스에서 키 로드
        encryption_key = KeyManagementService::GetKey("game_encryption_key");
    }
    
    std::vector<uint8_t> Encrypt(const std::vector<uint8_t>& data) {
        // AES-256-GCM 사용
        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        
        // IV 생성
        std::vector<uint8_t> iv(16);
        RAND_bytes(iv.data(), iv.size());
        
        // 암호화
        EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, 
                          encryption_key.data(), iv.data());
        
        // ... 암호화 로직 ...
        
        EVP_CIPHER_CTX_free(ctx);
        return encrypted_data;
    }
};
```

### 3. 불충분한 로깅과 모니터링
```cpp
// [SEQUENCE: 3] ❌ 잘못된 예: 기본적인 로깅만 수행
class BadLogging {
    void LogEvent(const std::string& message) {
        // 단순 텍스트 로그 - 분석 불가능
        std::cout << "[" << GetTimestamp() << "] " << message << std::endl;
    }
};

// ✅ 올바른 예: 구조화된 보안 로깅
class SecureLogging {
    void LogSecurityEvent(const SecurityEvent& event) {
        nlohmann::json log_entry;
        log_entry["timestamp"] = std::chrono::system_clock::now();
        log_entry["event_id"] = GenerateUUID();
        log_entry["event_type"] = event.type;
        log_entry["severity"] = event.severity;
        log_entry["user_id"] = event.user_id;
        log_entry["ip_address"] = event.ip_address;
        log_entry["session_id"] = event.session_id;
        log_entry["details"] = event.details;
        
        // 무결성을 위한 해시
        log_entry["hash"] = CalculateHMAC(log_entry.dump());
        
        // SIEM으로 전송
        siem_client.Send(log_entry);
        
        // 로컬 저장 (암호화)
        secure_logger.Log(log_entry);
        
        // 실시간 알림
        if (event.severity >= Severity::HIGH) {
            alert_service.Notify(log_entry);
        }
    }
};
```

---

## 🚀 실습 프로젝트 (Practice Projects)

### 📚 기초 프로젝트: 게임 보안 감사 시스템
**목표**: 기본적인 보안 이벤트 로깅 및 분석

```cpp
// 구현 요구사항:
// 1. 로그인 이상 탐지
// 2. 게임 내 치팅 감지
// 3. DDoS 기본 방어
// 4. 보안 대시보드
// 5. 일일 보안 리포트
// 6. 기본 SIEM 연동
```

### 🎮 중급 프로젝트: Zero Trust 게임 서버
**목표**: 완전한 Zero Trust 아키텍처 구현

```cpp
// 핵심 기능:
// 1. mTLS 인증
// 2. 동적 권한 관리
// 3. 세션별 암호화
// 4. 행동 기반 인증
// 5. 마이크로 세그멘테이션
// 6. 실시간 위험 평가
```

### 🏆 고급 프로젝트: 엔터프라이즈 보안 플랫폼
**목표**: 대규모 게임 서비스용 통합 보안 시스템

```cpp
// 구현 내용:
// 1. 멀티 게임 보안 통합
// 2. AI 기반 위협 탐지
// 3. 자동화된 인시던트 대응
// 4. 컴플라이언스 자동화
// 5. 보안 오케스트레이션
// 6. 글로벌 위협 인텔리전스
// 7. 24/7 SOC 대시보드
```

---

## 📊 학습 체크리스트 (Learning Checklist)

### 🥉 브론즈 레벨
- [ ] 기본 암호화 구현
- [ ] 간단한 방화벽 규칙
- [ ] 로그 수집 시스템
- [ ] 기초 인증 시스템

### 🥈 실버 레벨
- [ ] PKI 인프라 구축
- [ ] IDS/IPS 구현
- [ ] SIEM 시스템 연동
- [ ] 보안 감사 도구

### 🥇 골드 레벨
- [ ] Zero Trust 구현
- [ ] 고급 위협 탐지
- [ ] 인시던트 대응 자동화
- [ ] 컴플라이언스 관리

### 💎 플래티넘 레벨
- [ ] AI 보안 시스템
- [ ] 글로벌 보안 아키텍처
- [ ] 위협 인텔리전스 플랫폼
- [ ] 보안 거버넌스 체계

---

## 📚 추가 학습 자료 (Additional Resources)

### 필독서
- 📖 "Security Engineering" - Ross Anderson
- 📖 "Applied Cryptography" - Bruce Schneier
- 📖 "The Web Application Hacker's Handbook" - Dafydd Stuttard

### 보안 인증
- 🏅 CISSP (Certified Information Systems Security Professional)
- 🏅 CEH (Certified Ethical Hacker)
- 🏅 OSCP (Offensive Security Certified Professional)
- 🏅 ISO 27001 Lead Auditor

### 오픈소스 도구
- 🔧 OWASP ZAP: 웹 애플리케이션 보안 스캐너
- 🔧 Metasploit: 침투 테스트 프레임워크
- 🔧 Wireshark: 네트워크 프로토콜 분석
- 🔧 ELK Stack: 로그 수집 및 분석

### 보안 커뮤니티
- 🌐 OWASP (Open Web Application Security Project)
- 🌐 SANS Institute
- 🌐 DefCon / BlackHat
- 🌐 한국인터넷진흥원 (KISA)

**다음 문서도 궁금하시면 언제든 말씀해 주세요!** 🛡️✨