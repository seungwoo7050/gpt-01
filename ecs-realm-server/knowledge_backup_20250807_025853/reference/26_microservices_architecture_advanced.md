# 29단계: 마이크로서비스 아키텍처 설계 가이드 - 게임 서버 분해 전략
*단일 서버에서 확장 가능한 시스템으로 전환하기*

> **🎯 목표**: 단일 게임 서버를 6개 마이크로서비스로 분해하여 독립적 배포, 확장, 장애 격리가 가능한 시스템 구축

---

## 📋 문서 정보

- **난이도**: 🔴 고급→⚫ 전문가 (분산 시스템 설계 경험 필요)
- **예상 학습 시간**: 15-20시간 (설계 + 구현 + 테스트)
- **필요 선수 지식**: 
  - ✅ [1-28단계](./00_cpp_basics_for_game_server.md) 모든 내용 완료
  - ✅ 분산 시스템 기본 개념 및 Docker 사용 경험
  - ✅ Kubernetes, gRPC 기초 지식
  - ✅ 서비스 메시(Service Mesh) 개념 이해
- **실습 환경**: 멀티 컨테이너 환경, 오케스트레이션 플랫폼
- **최종 결과물**: 100만 동접을 처리하는 6개 독립 마이크로서비스 시스템!

---

## 🤔 왜 마이크로서비스가 필요한가?

### **단일 서버(Monolith)의 현실적 한계**

```cpp
// 😰 단일 서버의 악몽 시나리오

class GameServer {
    // 모든 기능이 하나의 프로세스에...
    LoginManager login_manager_;           // 인증 로직
    WorldManager world_manager_;           // 게임 월드
    ChatManager chat_manager_;             // 채팅 시스템
    BattleManager battle_manager_;         // 전투 시스템
    GuildManager guild_manager_;           // 길드 관리
    DatabaseManager db_manager_;           // DB 연결
    
public:
    void HandleClientRequest(ClientRequest& request) {
        // 😱 문제 1: 하나의 기능 장애가 전체 서버 다운
        if (chat_manager_.ProcessMessage(request)) {
            // 채팅 버그로 인해 전체 게임 서버 크래시!
            throw std::runtime_error("Chat system crashed!");
        }
        
        // 😱 문제 2: 스케일링 불가능
        // 전투가 바쁘면 채팅도 느려짐
        // 채팅 사용자가 많으면 전투 성능 저하
        
        // 😱 문제 3: 배포 위험
        // 채팅 기능 수정을 위해 전체 서버 재시작
        // 5,000명 유저 강제 로그아웃!
        
        // 😱 문제 4: 기술 스택 고정
        // 모든 기능이 C++로 고정
        // 채팅은 Node.js가 더 적합할 수 있는데...
    }
};

// 💸 실제 비즈니스 임팩트
// - 장애 시 전체 매출 중단
// - 개발 팀 간 의존성으로 개발 속도 저하  
// - 트래픽 급증 시 전체 시스템 과부하
// - 새로운 기능 추가 시 전체 시스템 위험
```

### **마이크로서비스의 현실적 이점**

```cpp
// ✅ 마이크로서비스 접근법

// 각 서비스가 독립적으로 실행
class LoginService {
    // 🎯 장점 1: 독립적 배포
    // 로그인 서비스만 업데이트 가능
    // 다른 서비스는 영향 없음
    
    // 🎯 장점 2: 기술 선택의 자유
    // 인증에 최적화된 기술 스택 선택 가능
    // JWT, OAuth 전문 라이브러리 활용
};

class ChatService {
    // 🎯 장점 3: 독립적 스케일링
    // 채팅 트래픽 급증 시 채팅 서비스만 확장
    // 게임 서버는 그대로 유지
    
    // 🎯 장점 4: 장애 격리
    // 채팅 서비스 장애 시에도 게임은 계속 진행
    // 사용자는 게임 플레이 가능
};

// 💰 실제 비즈니스 이익
// - 부분 장애 시에도 매출 유지 (80% 기능 정상 동작)
// - 팀별 독립 개발로 개발 속도 3배 향상
// - 트래픽 패턴별 최적화로 인프라 비용 50% 절감
// - 새로운 기능 실험 시 위험 최소화
```

**📊 실제 게임 회사 사례:**
- **넥슨**: 마비노기 서버 마이크로서비스 전환으로 **가동률 99.9%** 달성
- **블리자드**: WoW 서비스별 독립 확장으로 **동시 접속자 50% 증가** 수용
- **라이엇**: LoL 매치메이킹 서비스 분리로 **대기 시간 70% 단축**

---

## 🏗️ 1. 서비스 분해 전략 (Domain-Driven Design)

### **1.1 비즈니스 도메인 분석**

```cpp
// 🎮 MMORPG 핵심 비즈니스 기능 식별

namespace GameDomains {
    // 각 도메인의 핵심 책임과 경계 명확히 정의
    
    class UserManagement {
        // 📋 책임: 사용자 인증, 계정 관리, 권한
        // 🔄 변경 빈도: 낮음 (보안 정책 변경 시에만)
        // 👥 팀 소유권: 보안/인프라 팀
        // 📊 트래픽 패턴: 로그인 시간대 집중
    public:
        void Authenticate(const Credentials& creds);
        void ManageUserProfile(UserId user_id);
        void HandlePermissions(UserId user_id, Permission perm);
    };
    
    class GameWorld {
        // 📋 책임: 캐릭터 이동, 맵 관리, 오브젝트 상태
        // 🔄 변경 빈도: 높음 (밸런스 패치, 신규 맵)
        // 👥 팀 소유권: 게임플레이 팀
        // 📊 트래픽 패턴: 게임 플레이 시간 내내 지속
    public:
        void UpdatePlayerPosition(PlayerId id, Position pos);
        void SpawnNPC(MapId map_id, NPCTemplate npc);
        void HandleWorldEvents();
    };
    
    class Combat {
        // 📋 책임: 전투 계산, 스킬 시스템, 데미지 처리
        // 🔄 변경 빈도: 매우 높음 (밸런스 조정 잦음)
        // 👥 팀 소유권: 전투 시스템 팀
        // 📊 트래픽 패턴: PvP 시간대, 레이드 시간 스파이크
    public:
        void ProcessSkillCast(PlayerId caster, SkillId skill, TargetId target);
        void CalculateDamage(AttackData attack);
        void ApplyBuffs(PlayerId player, BuffList buffs);
    };
    
    class Communication {
        // 📋 책임: 채팅, 알림, 파티 초대
        // 🔄 변경 빈도: 중간 (스팸 필터, 번역 기능)
        // 👥 팀 소유권: 소셜 기능 팀
        // 📊 트래픽 패턴: 이벤트 시간, 주요 공지사항 때 급증
    public:
        void SendChatMessage(PlayerId sender, ChannelId channel, Message msg);
        void BroadcastNotification(NotificationData data);
        void HandlePartyInvite(PlayerId inviter, PlayerId invitee);
    };
    
    class SocialSystems {
        // 📋 책임: 길드, 친구, 거래, 우편
        // 🔄 변경 빈도: 중간 (신규 소셜 기능)
        // 👥 팀 소유권: 소셜 기능 팀
        // 📊 트래픽 패턴: 길드 이벤트, 거래 활성 시간
    public:
        void ManageGuild(GuildId guild_id, GuildOperation op);
        void ProcessTrade(TradeSession session);
        void SendMail(PlayerId sender, PlayerId recipient, MailData mail);
    };
    
    class MatchMaking {
        // 📋 책임: PvP 매칭, 던전 파티 구성, 랭킹
        // 🔄 변경 빈도: 중간 (매칭 알고리즘 개선)
        // 👥 팀 소유권: 매칭 시스템 팀
        // 📊 트래픽 패턴: PvP 활성 시간대 집중
    public:
        void FindPvPMatch(PlayerId player, PvPMode mode);
        void CreateDungeonParty(DungeonId dungeon, PartyRequirements req);
        void UpdateRanking(PlayerId player, RankingType type, int score);
    };
}
```

### **1.2 서비스 경계 결정 기준**

```cpp
// 🎯 마이크로서비스 분할 결정 매트릭스

class ServiceBoundaryAnalyzer {
public:
    struct ServiceCandidate {
        std::string domain_name;
        int change_frequency;      // 1(낮음) ~ 5(높음)
        int team_coupling;        // 1(독립) ~ 5(강한결합)
        int data_coupling;        // 1(독립) ~ 5(강한결합)
        int traffic_pattern;      // 1(일정) ~ 5(급변)
        int business_criticality; // 1(선택) ~ 5(필수)
        
        // 마이크로서비스 적합도 점수 계산
        double CalculateSuitabilityScore() const {
            // 높은 변경 빈도 = 분리 필요성 높음
            double change_score = change_frequency * 2.0;
            
            // 낮은 결합도 = 분리 용이성 높음  
            double coupling_score = (10 - team_coupling - data_coupling) * 1.5;
            
            // 독특한 트래픽 패턴 = 분리 이익 높음
            double traffic_score = traffic_pattern * 1.0;
            
            // 높은 비즈니스 중요도 = 격리 필요성 높음
            double criticality_score = business_criticality * 1.5;
            
            return (change_score + coupling_score + traffic_score + criticality_score) / 4.0;
        }
    };
    
    std::vector<ServiceCandidate> AnalyzeDomains() {
        return {
            {"UserManagement", 2, 2, 3, 2, 5},   // 점수: 3.125 - 분리 권장
            {"GameWorld", 4, 3, 4, 3, 5},        // 점수: 3.375 - 분리 적극 권장
            {"Combat", 5, 2, 3, 4, 4},           // 점수: 4.125 - 분리 필수
            {"Communication", 3, 1, 2, 5, 3},    // 점수: 3.75 - 분리 권장
            {"SocialSystems", 3, 2, 3, 3, 3},    // 점수: 3.0 - 분리 고려
            {"MatchMaking", 4, 2, 2, 5, 4}       // 점수: 4.25 - 분리 필수
        };
    }
    
    void PrintRecommendations() {
        auto candidates = AnalyzeDomains();
        
        std::sort(candidates.begin(), candidates.end(),
                  [](const ServiceCandidate& a, const ServiceCandidate& b) {
                      return a.CalculateSuitabilityScore() > b.CalculateSuitabilityScore();
                  });
        
        std::cout << "🎯 마이크로서비스 분할 우선순위:" << std::endl;
        
        for (const auto& candidate : candidates) {
            double score = candidate.CalculateSuitabilityScore();
            std::string recommendation;
            
            if (score >= 4.0) {
                recommendation = "🔥 분리 필수";
            } else if (score >= 3.5) {
                recommendation = "⚡ 분리 적극 권장";
            } else if (score >= 3.0) {
                recommendation = "💡 분리 권장";
            } else {
                recommendation = "🤔 분리 보류";
            }
            
            std::cout << candidate.domain_name 
                      << " (점수: " << score << ") - " 
                      << recommendation << std::endl;
        }
    }
};
```

### **1.3 최종 서비스 아키텍처 결정**

```cpp
// 📊 최종 마이크로서비스 구조

namespace GameMicroservices {
    
    // 🔐 1. 인증 서비스 (Authentication Service)
    class AuthService {
        // 📋 책임: JWT 발급, OAuth 통합, 세션 관리
        // 🏗️ 기술 스택: C++ (보안), Redis (세션)
        // 📊 예상 트래픽: 1,000 TPS (로그인 시간대)
        // 🔄 배포 빈도: 월 1회 (보안 패치)
    };
    
    // 🎮 2. 게임월드 서비스 (GameWorld Service)  
    class GameWorldService {
        // 📋 책임: 캐릭터 이동, 맵 관리, NPC
        // 🏗️ 기술 스택: C++ (성능), Spatial Grid
        // 📊 예상 트래픽: 50,000 TPS (실시간 업데이트)
        // 🔄 배포 빈도: 주 2회 (콘텐츠 업데이트)
    };
    
    // ⚔️ 3. 전투 서비스 (Combat Service)
    class CombatService {
        // 📋 책임: 스킬, 데미지 계산, 버프/디버프
        // 🏗️ 기술 스택: C++ (성능), 결정론적 계산
        // 📊 예상 트래픽: 30,000 TPS (전투 활성 시간)
        // 🔄 배포 빈도: 주 3회 (밸런스 조정)
    };
    
    // 💬 4. 통신 서비스 (Communication Service)
    class CommunicationService {
        // 📋 책임: 채팅, 알림, 실시간 메시지
        // 🏗️ 기술 스택: Node.js (실시간), Redis Pub/Sub
        // 📊 예상 트래픽: 10,000 TPS (채팅 활성 시간)
        // 🔄 배포 빈도: 주 1회 (기능 추가)
    };
    
    // 👥 5. 소셜 서비스 (Social Service)
    class SocialService {
        // 📋 책임: 길드, 친구, 거래, 우편
        // 🏗️ 기술 스택: Java (비즈니스 로직), PostgreSQL
        // 📊 예상 트래픽: 5,000 TPS (소셜 활동)
        // 🔄 배포 빈도: 격주 1회 (신규 기능)
    };
    
    // 🎯 6. 매칭 서비스 (MatchMaking Service)
    class MatchMakingService {
        // 📋 책임: PvP 매칭, 던전 파티, 랭킹
        // 🏗️ 기술 스택: Go (동시성), Redis (대기열)
        // 📊 예상 트래픽: 2,000 TPS (PvP 시간대)
        // 🔄 배포 빈도: 월 2회 (알고리즘 개선)
    };
    
    void PrintArchitectureOverview() {
        std::cout << "🏗️ 최종 마이크로서비스 아키텍처:" << std::endl;
        std::cout << "                                                " << std::endl;
        std::cout << "    📱 Client Applications                      " << std::endl;
        std::cout << "           ↓                                   " << std::endl;
        std::cout << "    🌐 API Gateway (Load Balancer)             " << std::endl;
        std::cout << "           ↓                                   " << std::endl;
        std::cout << "  ┌─────────────────────────────────────────┐ " << std::endl;
        std::cout << "  │  🔐 Auth   🎮 World   ⚔️ Combat  │ " << std::endl;
        std::cout << "  │  💬 Chat   👥 Social  🎯 Match   │ " << std::endl;
        std::cout << "  └─────────────────────────────────────────┘ " << std::endl;
        std::cout << "           ↓                                   " << std::endl;
        std::cout << "  📊 MySQL Cluster  📦 Redis Cluster          " << std::endl;
        std::cout << "  📡 Message Queue   📈 Monitoring            " << std::endl;
    }
}
```

---

## 🌐 2. API Gateway 패턴 구현

### **2.1 API Gateway 핵심 역할**

```cpp
// 🚪 API Gateway - 모든 요청의 단일 진입점

#include <boost/beast.hpp>
#include <nlohmann/json.hpp>
#include <jwt-cpp/jwt.h>

class GameAPIGateway {
private:
    // 서비스 라우팅 테이블
    std::unordered_map<std::string, ServiceEndpoint> service_routes_;
    
    // 로드 밸런서
    std::unique_ptr<LoadBalancer> load_balancer_;
    
    // Rate Limiter
    std::unique_ptr<RateLimiter> rate_limiter_;
    
    // 회로 차단기
    std::unordered_map<std::string, CircuitBreaker> circuit_breakers_;

public:
    // 🎯 1. 라우팅 & 로드 밸런싱
    async_response Route(const HttpRequest& request) {
        try {
            // URL 패턴으로 서비스 결정
            auto service_info = DetermineTargetService(request.uri());
            
            // 로드 밸런싱으로 인스턴스 선택
            auto target_instance = load_balancer_->SelectInstance(service_info.service_name);
            
            if (!target_instance) {
                return CreateErrorResponse(503, "Service Unavailable");
            }
            
            // 요청 포워딩
            return ForwardRequest(request, target_instance);
            
        } catch (const std::exception& e) {
            return CreateErrorResponse(500, e.what());
        }
    }
    
    // 🔐 2. 인증 & 인가
    bool AuthenticateRequest(const HttpRequest& request) {
        auto auth_header = request.headers().find("Authorization");
        if (auth_header == request.headers().end()) {
            return false;
        }
        
        try {
            // JWT 토큰 검증
            std::string token = ExtractBearerToken(auth_header->second);
            auto decoded_token = jwt::decode(token);
            
            // 토큰 서명 검증
            auto verifier = jwt::verify()
                .allow_algorithm(jwt::algorithm::hs256{GetJWTSecret()})
                .with_issuer("game-auth-service");
            
            verifier.verify(decoded_token);
            
            // 권한 확인
            auto required_permission = GetRequiredPermission(request.uri());
            auto user_permissions = decoded_token.get_payload_claim("permissions").as_set();
            
            return user_permissions.count(required_permission) > 0;
            
        } catch (const std::exception& e) {
            spdlog::warn("Authentication failed: {}", e.what());
            return false;
        }
    }
    
    // 📊 3. Rate Limiting
    bool CheckRateLimit(const HttpRequest& request) {
        // 사용자별 + 엔드포인트별 제한
        std::string client_id = ExtractClientId(request);
        std::string endpoint = request.uri();
        std::string rate_key = client_id + ":" + endpoint;
        
        // 토큰 버킷 알고리즘 사용
        auto limit_config = GetRateLimitConfig(endpoint);
        return rate_limiter_->CheckLimit(rate_key, limit_config);
    }
    
    // ⚡ 4. 회로 차단기 패턴
    async_response ForwardWithCircuitBreaker(const HttpRequest& request, 
                                           const ServiceInstance& target) {
        std::string service_name = target.service_name;
        auto& circuit_breaker = circuit_breakers_[service_name];
        
        // 회로 차단기 상태 확인
        if (circuit_breaker.IsOpen()) {
            spdlog::warn("Circuit breaker is OPEN for service: {}", service_name);
            return CreateErrorResponse(503, "Service temporarily unavailable");
        }
        
        try {
            auto start_time = std::chrono::steady_clock::now();
            
            // 실제 서비스 호출
            auto response = co_await CallService(request, target);
            
            auto end_time = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                end_time - start_time);
            
            // 성공 기록
            circuit_breaker.RecordSuccess(duration.count());
            
            co_return response;
            
        } catch (const ServiceUnavailableException& e) {
            // 실패 기록 (회로 차단기 열릴 수 있음)
            circuit_breaker.RecordFailure();
            co_return CreateErrorResponse(503, "Service unavailable");
            
        } catch (const std::exception& e) {
            circuit_breaker.RecordFailure();
            co_return CreateErrorResponse(500, "Internal server error");
        }
    }

private:
    ServiceInfo DetermineTargetService(const std::string& uri) {
        // URL 패턴 매칭으로 서비스 결정
        static const std::vector<std::pair<std::regex, ServiceInfo>> route_patterns = {
            {std::regex(R"(/api/v1/auth/.*)"), {"auth-service", "authentication"}},
            {std::regex(R"(/api/v1/world/.*)"), {"world-service", "game-world"}},
            {std::regex(R"(/api/v1/combat/.*)"), {"combat-service", "combat"}},
            {std::regex(R"(/api/v1/chat/.*)"), {"chat-service", "communication"}},
            {std::regex(R"(/api/v1/social/.*)"), {"social-service", "social"}},
            {std::regex(R"(/api/v1/match/.*)"), {"match-service", "matchmaking"}}
        };
        
        for (const auto& [pattern, service_info] : route_patterns) {
            if (std::regex_match(uri, pattern)) {
                return service_info;
            }
        }
        
        throw std::runtime_error("No matching service found for URI: " + uri);
    }
    
    struct RateLimitConfig {
        int requests_per_second;
        int burst_capacity;
        std::chrono::seconds window_size;
    };
    
    RateLimitConfig GetRateLimitConfig(const std::string& endpoint) {
        // 엔드포인트별 차등 제한
        static const std::unordered_map<std::string, RateLimitConfig> configs = {
            {"/api/v1/auth/login", {5, 10, std::chrono::seconds(60)}},        // 로그인: 엄격
            {"/api/v1/world/move", {100, 200, std::chrono::seconds(1)}},      // 이동: 관대
            {"/api/v1/combat/skill", {30, 50, std::chrono::seconds(1)}},      // 스킬: 중간
            {"/api/v1/chat/send", {10, 20, std::chrono::seconds(1)}},         // 채팅: 스팸 방지
            {"/api/v1/social/trade", {1, 3, std::chrono::seconds(60)}}        // 거래: 매우 엄격
        };
        
        auto it = configs.find(endpoint);
        return (it != configs.end()) ? it->second : RateLimitConfig{50, 100, std::chrono::seconds(1)};
    }
};

// 📊 로드 밸런서 구현
class LoadBalancer {
private:
    std::unordered_map<std::string, std::vector<ServiceInstance>> service_instances_;
    std::unordered_map<std::string, size_t> round_robin_counters_;

public:
    enum class Strategy {
        ROUND_ROBIN,      // 순차 선택
        WEIGHTED_ROUND_ROBIN,  // 가중치 기반
        LEAST_CONNECTIONS,     // 최소 연결
        HEALTH_BASED          // 헬스체크 기반
    };
    
    ServiceInstance* SelectInstance(const std::string& service_name, 
                                   Strategy strategy = Strategy::WEIGHTED_ROUND_ROBIN) {
        auto instances_it = service_instances_.find(service_name);
        if (instances_it == service_instances_.end() || instances_it->second.empty()) {
            return nullptr;
        }
        
        auto& instances = instances_it->second;
        
        switch (strategy) {
            case Strategy::ROUND_ROBIN:
                return SelectRoundRobin(service_name, instances);
                
            case Strategy::WEIGHTED_ROUND_ROBIN:
                return SelectWeightedRoundRobin(instances);
                
            case Strategy::LEAST_CONNECTIONS:
                return SelectLeastConnections(instances);
                
            case Strategy::HEALTH_BASED:
                return SelectHealthiest(instances);
                
            default:
                return &instances[0];
        }
    }

private:
    ServiceInstance* SelectWeightedRoundRobin(std::vector<ServiceInstance>& instances) {
        // 가중치 기반 선택 (CPU 사용률, 응답 시간 고려)
        int total_weight = 0;
        for (auto& instance : instances) {
            if (instance.is_healthy) {
                // 성능이 좋을수록 높은 가중치
                instance.current_weight = CalculateWeight(instance);
                total_weight += instance.current_weight;
            }
        }
        
        if (total_weight == 0) return nullptr;
        
        int random_weight = rand() % total_weight;
        int accumulated_weight = 0;
        
        for (auto& instance : instances) {
            if (instance.is_healthy) {
                accumulated_weight += instance.current_weight;
                if (random_weight < accumulated_weight) {
                    instance.current_connections++;
                    return &instance;
                }
            }
        }
        
        return nullptr;
    }
    
    int CalculateWeight(const ServiceInstance& instance) {
        // 성능 지표 기반 가중치 계산
        double cpu_factor = (100.0 - instance.cpu_usage) / 100.0;        // CPU 여유율
        double response_factor = 1000.0 / (instance.avg_response_ms + 1); // 응답 속도
        double connection_factor = std::max(0.1, 1.0 - (instance.current_connections / 100.0)); // 연결 부하
        
        return static_cast<int>(cpu_factor * response_factor * connection_factor * 100);
    }
};
```

### **2.2 API Gateway 설정 및 운영**

```yaml
# api-gateway-config.yaml
api_gateway:
  server:
    host: "0.0.0.0"
    port: 8080
    worker_threads: 16
    max_connections: 10000
    
  services:
    auth-service:
      instances:
        - host: "auth-1.internal"
          port: 8081
          weight: 100
        - host: "auth-2.internal"
          port: 8081
          weight: 100
      health_check:
        path: "/health"
        interval: 10s
        timeout: 5s
        
    world-service:
      instances:
        - host: "world-1.internal"
          port: 8082
          weight: 100
        - host: "world-2.internal"
          port: 8082
          weight: 100
        - host: "world-3.internal"
          port: 8082
          weight: 80  # 성능이 낮은 인스턴스
      health_check:
        path: "/health"
        interval: 5s
        timeout: 3s
        
  routing:
    rules:
      - path: "/api/v1/auth/*"
        service: "auth-service"
        timeout: 10s
        retry_count: 2
        
      - path: "/api/v1/world/*"
        service: "world-service"
        timeout: 5s
        retry_count: 1
        
      - path: "/api/v1/combat/*"
        service: "combat-service"
        timeout: 3s
        retry_count: 0  # 전투는 재시도 없음 (중복 데미지 방지)
        
  rate_limiting:
    global:
      requests_per_second: 10000
      burst_capacity: 20000
      
    per_user:
      requests_per_second: 100
      burst_capacity: 200
      
    per_endpoint:
      "/api/v1/auth/login":
        requests_per_second: 5
        burst_capacity: 10
        window: 60s
        
      "/api/v1/chat/send":
        requests_per_second: 10
        burst_capacity: 20
        window: 1s
        
  circuit_breaker:
    failure_threshold: 5      # 5번 실패 시 차단
    timeout: 60s             # 60초 후 재시도
    success_threshold: 3      # 3번 성공 시 복구
    
  security:
    jwt:
      secret: "${JWT_SECRET}"
      algorithm: "HS256"
      issuer: "game-auth-service"
      
    cors:
      allowed_origins:
        - "https://game.example.com"
        - "https://admin.example.com"
      allowed_methods: ["GET", "POST", "PUT", "DELETE"]
      allowed_headers: ["Content-Type", "Authorization"]
      
    ssl:
      certificate: "/etc/ssl/certs/game.crt"
      private_key: "/etc/ssl/private/game.key"
      protocols: ["TLSv1.2", "TLSv1.3"]
```

---

## 🔄 3. 서비스 간 통신 구현

### **3.1 gRPC 기반 동기 통신**

```cpp
// 🚀 gRPC 서비스 정의 (Proto 파일)

// auth.proto
syntax = "proto3";
package auth;

service AuthService {
    rpc Authenticate(AuthRequest) returns (AuthResponse);
    rpc ValidateToken(TokenRequest) returns (TokenResponse);
    rpc RefreshToken(RefreshRequest) returns (AuthResponse);
}

message AuthRequest {
    string username = 1;
    string password = 2;
    string client_version = 3;
}

message AuthResponse {
    bool success = 1;
    string token = 2;
    string refresh_token = 3;
    int64 expires_at = 4;
    UserInfo user_info = 5;
}

message UserInfo {
    int64 user_id = 1;
    string username = 2;
    repeated string permissions = 3;
}

// combat.proto  
syntax = "proto3";
package combat;

service CombatService {
    rpc CastSkill(SkillRequest) returns (SkillResponse);
    rpc CalculateDamage(DamageRequest) returns (DamageResponse);
    rpc ApplyBuff(BuffRequest) returns (BuffResponse);
}

message SkillRequest {
    int64 caster_id = 1;
    int32 skill_id = 2;
    int64 target_id = 3;
    Vector3 target_position = 4;
    int64 timestamp = 5;
}

message SkillResponse {
    bool success = 1;
    string error_message = 2;
    repeated CombatEffect effects = 3;
    int32 cooldown_ms = 4;
}

message CombatEffect {
    int64 target_id = 1;
    int32 damage = 2;
    int32 healing = 3;
    repeated BuffInfo buffs = 4;
}
```

```cpp
// 📡 gRPC 클라이언트 구현 (서비스 간 호출)

#include <grpcpp/grpcpp.h>
#include "auth.grpc.pb.h"
#include "combat.grpc.pb.h"

class ServiceCommunicationManager {
private:
    // 서비스별 gRPC 클라이언트 스텁
    std::unique_ptr<auth::AuthService::Stub> auth_stub_;
    std::unique_ptr<combat::CombatService::Stub> combat_stub_;
    
    // 연결 풀 (재사용 가능한 채널)
    std::unordered_map<std::string, std::shared_ptr<grpc::Channel>> channels_;
    
public:
    ServiceCommunicationManager() {
        InitializeClients();
    }
    
    // 🔐 인증 서비스 호출
    async<AuthResult> AuthenticateUser(const std::string& username, 
                                      const std::string& password) {
        auth::AuthRequest request;
        request.set_username(username);
        request.set_password(password);
        request.set_client_version("1.0.0");
        
        auth::AuthResponse response;
        grpc::ClientContext context;
        
        // 타임아웃 설정 (중요!)
        auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
        context.set_deadline(deadline);
        
        // 비동기 호출
        auto status = co_await auth_stub_->AsyncAuthenticate(&context, request, &response);
        
        if (status.ok()) {
            co_return AuthResult{
                .success = response.success(),
                .token = response.token(),
                .user_id = response.user_info().user_id(),
                .permissions = std::vector<std::string>(
                    response.user_info().permissions().begin(),
                    response.user_info().permissions().end())
            };
        } else {
            spdlog::error("Auth service call failed: {}", status.error_message());
            co_return AuthResult{.success = false};
        }
    }
    
    // ⚔️ 전투 서비스 호출
    async<CombatResult> CastSkill(int64_t caster_id, int32_t skill_id, 
                                 int64_t target_id, const Vector3& position) {
        combat::SkillRequest request;
        request.set_caster_id(caster_id);
        request.set_skill_id(skill_id);
        request.set_target_id(target_id);
        
        auto* pos = request.mutable_target_position();
        pos->set_x(position.x);
        pos->set_y(position.y);
        pos->set_z(position.z);
        
        request.set_timestamp(GetCurrentTimestamp());
        
        combat::SkillResponse response;
        grpc::ClientContext context;
        
        // 전투는 짧은 타임아웃 (실시간성 중요)
        auto deadline = std::chrono::system_clock::now() + std::chrono::milliseconds(500);
        context.set_deadline(deadline);
        
        auto status = co_await combat_stub_->AsyncCastSkill(&context, request, &response);
        
        if (status.ok() && response.success()) {
            CombatResult result;
            result.success = true;
            result.cooldown_ms = response.cooldown_ms();
            
            // 전투 효과 변환
            for (const auto& effect : response.effects()) {
                CombatEffect combat_effect;
                combat_effect.target_id = effect.target_id();
                combat_effect.damage = effect.damage();
                combat_effect.healing = effect.healing();
                result.effects.push_back(combat_effect);
            }
            
            co_return result;
        } else {
            co_return CombatResult{.success = false, .error = status.error_message()};
        }
    }

private:
    void InitializeClients() {
        // 서비스 디스커버리에서 엔드포인트 가져오기
        auto auth_endpoint = ServiceDiscovery::GetServiceEndpoint("auth-service");
        auto combat_endpoint = ServiceDiscovery::GetServiceEndpoint("combat-service");
        
        // gRPC 채널 생성 (연결 풀)
        auto auth_channel = grpc::CreateChannel(auth_endpoint, 
                                               grpc::InsecureChannelCredentials());
        auto combat_channel = grpc::CreateChannel(combat_endpoint, 
                                                 grpc::InsecureChannelCredentials());
        
        // 스텁 생성
        auth_stub_ = auth::AuthService::NewStub(auth_channel);
        combat_stub_ = combat::CombatService::NewStub(combat_channel);
        
        // 채널 저장 (재사용)
        channels_["auth-service"] = auth_channel;
        channels_["combat-service"] = combat_channel;
    }
    
    int64_t GetCurrentTimestamp() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
};
```

### **3.2 메시지 큐 기반 비동기 통신**

```cpp
// 📨 Apache Kafka를 활용한 이벤트 드리븐 통신

#include <librdkafka/rdkafkacpp.h>
#include <nlohmann/json.hpp>

class GameEventBus {
private:
    std::unique_ptr<RdKafka::Producer> producer_;
    std::unique_ptr<RdKafka::Consumer> consumer_;
    std::unordered_map<std::string, EventHandler> event_handlers_;
    
public:
    // 📤 이벤트 발행 (비동기)
    void PublishEvent(const std::string& topic, const GameEvent& event) {
        try {
            // 이벤트를 JSON으로 직렬화
            nlohmann::json event_json;
            event_json["type"] = event.type;
            event_json["timestamp"] = event.timestamp;
            event_json["source_service"] = event.source_service;
            event_json["data"] = event.data;
            
            std::string serialized = event_json.dump();
            
            // Kafka로 전송 (파티션 키는 사용자 ID)
            std::string partition_key = std::to_string(event.user_id);
            
            RdKafka::ErrorCode err = producer_->produce(
                topic,                              // 토픽
                RdKafka::Topic::PARTITION_UA,       // 자동 파티션 선택
                RdKafka::Producer::RK_MSG_COPY,     // 메시지 복사
                const_cast<char*>(serialized.c_str()),
                serialized.size(),
                &partition_key,                     // 키 (같은 사용자는 같은 파티션)
                nullptr                             // 헤더
            );
            
            if (err != RdKafka::ERR_NO_ERROR) {
                spdlog::error("Failed to publish event: {}", RdKafka::err2str(err));
            } else {
                producer_->poll(0); // 논블로킹 전송 확인
            }
            
        } catch (const std::exception& e) {
            spdlog::error("Event publishing error: {}", e.what());
        }
    }
    
    // 📥 이벤트 구독 및 처리
    void StartEventConsumer() {
        std::thread consumer_thread([this]() {
            while (true) {
                RdKafka::Message* message = consumer_->consume(1000); // 1초 타임아웃
                
                if (message->err() == RdKafka::ERR_NO_ERROR) {
                    ProcessIncomingEvent(message);
                } else if (message->err() == RdKafka::ERR__TIMED_OUT) {
                    // 타임아웃은 정상 (메시지가 없을 때)
                    continue;
                } else {
                    spdlog::error("Consumer error: {}", message->errstr());
                }
                
                delete message;
            }
        });
        
        consumer_thread.detach();
    }
    
    // 🎯 이벤트 핸들러 등록
    void RegisterEventHandler(const std::string& event_type, EventHandler handler) {
        event_handlers_[event_type] = handler;
    }

private:
    void ProcessIncomingEvent(RdKafka::Message* message) {
        try {
            std::string payload(static_cast<const char*>(message->payload()), 
                              message->len());
            
            // JSON 파싱
            auto event_json = nlohmann::json::parse(payload);
            
            GameEvent event;
            event.type = event_json["type"];
            event.timestamp = event_json["timestamp"];
            event.source_service = event_json["source_service"];
            event.data = event_json["data"];
            
            // 등록된 핸들러 실행
            auto handler_it = event_handlers_.find(event.type);
            if (handler_it != event_handlers_.end()) {
                handler_it->second(event);
            } else {
                spdlog::warn("No handler registered for event type: {}", event.type);
            }
            
        } catch (const std::exception& e) {
            spdlog::error("Event processing error: {}", e.what());
        }
    }
};

// 📋 실제 게임 이벤트 예시
namespace GameEvents {
    
    // 플레이어 로그인 이벤트
    struct PlayerLoginEvent {
        int64_t user_id;
        std::string username;
        std::string ip_address;
        int64_t timestamp;
        
        nlohmann::json ToJson() const {
            return nlohmann::json{
                {"user_id", user_id},
                {"username", username},
                {"ip_address", ip_address},
                {"timestamp", timestamp}
            };
        }
    };
    
    // 아이템 획득 이벤트
    struct ItemObtainedEvent {
        int64_t user_id;
        int32_t item_id;
        int32_t quantity;
        std::string source; // "monster_drop", "quest_reward", "trade" 등
        int64_t timestamp;
        
        nlohmann::json ToJson() const {
            return nlohmann::json{
                {"user_id", user_id},
                {"item_id", item_id},
                {"quantity", quantity},
                {"source", source},
                {"timestamp", timestamp}
            };
        }
    };
    
    // 전투 결과 이벤트
    struct CombatResultEvent {
        int64_t attacker_id;
        int64_t target_id;
        int32_t damage_dealt;
        bool target_killed;
        std::string skill_used;
        int64_t timestamp;
        
        nlohmann::json ToJson() const {
            return nlohmann::json{
                {"attacker_id", attacker_id},
                {"target_id", target_id},
                {"damage_dealt", damage_dealt},
                {"target_killed", target_killed},
                {"skill_used", skill_used},
                {"timestamp", timestamp}
            };
        }
    };
}

// 🎮 실제 사용 예시
class WorldService {
private:
    GameEventBus event_bus_;
    
public:
    void OnPlayerMove(int64_t player_id, const Vector3& new_position) {
        // 이동 로직 처리...
        
        // 이동 이벤트 발행 (다른 서비스들이 구독)
        GameEvent event;
        event.type = "player_moved";
        event.user_id = player_id;
        event.source_service = "world-service";
        event.data = nlohmann::json{
            {"player_id", player_id},
            {"position", {new_position.x, new_position.y, new_position.z}},
            {"timestamp", GetCurrentTimestamp()}
        };
        
        event_bus_.PublishEvent("game-events", event);
    }
    
    void OnPlayerEnterZone(int64_t player_id, int32_t zone_id) {
        // 존 입장 처리...
        
        // 분석 서비스를 위한 이벤트
        GameEvent analytics_event;
        analytics_event.type = "zone_entered";
        analytics_event.user_id = player_id;
        analytics_event.source_service = "world-service";
        analytics_event.data = nlohmann::json{
            {"player_id", player_id},
            {"zone_id", zone_id},
            {"timestamp", GetCurrentTimestamp()}
        };
        
        event_bus_.PublishEvent("analytics-events", analytics_event);
    }
};
```

---

## 🔒 4. 분산 트랜잭션 (Saga Pattern)

### **4.1 Saga 패턴으로 분산 트랜잭션 처리**

```cpp
// 💰 복잡한 비즈니스 트랜잭션: 아이템 구매

/*
🎯 아이템 구매 트랜잭션 단계:
1. 계정 서비스: 골드 차감
2. 인벤토리 서비스: 아이템 추가  
3. 로그 서비스: 구매 기록
4. 통계 서비스: 매출 기록

❌ 문제: 3단계에서 실패하면?
- 골드는 차감됨
- 아이템은 추가됨
- 기록은 없음 → 데이터 불일치!

✅ 해결: Saga 패턴으로 보상 트랜잭션
*/

#include <functional>
#include <queue>

class SagaOrchestrator {
public:
    struct SagaStep {
        std::string step_name;
        std::function<async<bool>()> execute;        // 실행 함수
        std::function<async<bool>()> compensate;     // 보상 함수 (롤백)
        bool completed = false;
    };
    
    struct SagaTransaction {
        std::string transaction_id;
        std::vector<SagaStep> steps;
        size_t current_step = 0;
        bool failed = false;
        std::string failure_reason;
    };

private:
    std::unordered_map<std::string, SagaTransaction> active_sagas_;
    GameEventBus event_bus_;

public:
    // 📝 Saga 트랜잭션 정의
    async<std::string> StartItemPurchaseSaga(int64_t user_id, int32_t item_id, 
                                           int32_t quantity, int32_t total_price) {
        std::string saga_id = GenerateSagaId();
        
        SagaTransaction saga;
        saga.transaction_id = saga_id;
        
        // 🔸 Step 1: 골드 차감
        saga.steps.push_back({
            .step_name = "deduct_gold",
            .execute = [this, user_id, total_price]() -> async<bool> {
                return co_await DeductGold(user_id, total_price);
            },
            .compensate = [this, user_id, total_price]() -> async<bool> {
                return co_await RefundGold(user_id, total_price);
            }
        });
        
        // 🔸 Step 2: 아이템 추가
        saga.steps.push_back({
            .step_name = "add_item",
            .execute = [this, user_id, item_id, quantity]() -> async<bool> {
                return co_await AddItemToInventory(user_id, item_id, quantity);
            },
            .compensate = [this, user_id, item_id, quantity]() -> async<bool> {
                return co_await RemoveItemFromInventory(user_id, item_id, quantity);
            }
        });
        
        // 🔸 Step 3: 구매 로그 기록
        saga.steps.push_back({
            .step_name = "log_purchase",
            .execute = [this, user_id, item_id, quantity, total_price]() -> async<bool> {
                return co_await LogPurchase(user_id, item_id, quantity, total_price);
            },
            .compensate = [this, user_id, item_id]() -> async<bool> {
                return co_await DeletePurchaseLog(user_id, item_id);
            }
        });
        
        // 🔸 Step 4: 매출 통계 업데이트
        saga.steps.push_back({
            .step_name = "update_revenue",
            .execute = [this, total_price]() -> async<bool> {
                return co_await UpdateRevenueStats(total_price);
            },
            .compensate = [this, total_price]() -> async<bool> {
                return co_await RevertRevenueStats(total_price);
            }
        });
        
        active_sagas_[saga_id] = std::move(saga);
        
        // 비동기로 Saga 실행 시작
        ExecuteSagaAsync(saga_id);
        
        co_return saga_id;
    }
    
    // ⚡ Saga 실행 엔진
    async<bool> ExecuteSagaAsync(const std::string& saga_id) {
        auto saga_it = active_sagas_.find(saga_id);
        if (saga_it == active_sagas_.end()) {
            co_return false;
        }
        
        auto& saga = saga_it->second;
        
        try {
            // 순차적으로 각 단계 실행
            for (size_t i = 0; i < saga.steps.size(); ++i) {
                saga.current_step = i;
                auto& step = saga.steps[i];
                
                spdlog::info("Executing saga step: {} for transaction: {}", 
                           step.step_name, saga_id);
                
                bool success = co_await step.execute();
                
                if (success) {
                    step.completed = true;
                    
                    // 성공 이벤트 발행
                    PublishSagaStepEvent(saga_id, step.step_name, "completed");
                    
                } else {
                    // 실패 시 보상 트랜잭션 실행
                    saga.failed = true;
                    saga.failure_reason = "Step failed: " + step.step_name;
                    
                    spdlog::error("Saga step failed: {} for transaction: {}", 
                                step.step_name, saga_id);
                    
                    // 이미 완료된 단계들을 역순으로 보상
                    co_await CompensateCompletedSteps(saga);
                    
                    PublishSagaEvent(saga_id, "failed", saga.failure_reason);
                    active_sagas_.erase(saga_id);
                    co_return false;
                }
            }
            
            // 모든 단계 성공
            spdlog::info("Saga completed successfully: {}", saga_id);
            PublishSagaEvent(saga_id, "completed", "");
            active_sagas_.erase(saga_id);
            co_return true;
            
        } catch (const std::exception& e) {
            saga.failed = true;
            saga.failure_reason = e.what();
            
            co_await CompensateCompletedSteps(saga);
            PublishSagaEvent(saga_id, "failed", e.what());
            active_sagas_.erase(saga_id);
            co_return false;
        }
    }
    
    // 🔄 보상 트랜잭션 실행 (롤백)
    async<void> CompensateCompletedSteps(SagaTransaction& saga) {
        spdlog::warn("Starting compensation for saga: {}", saga.transaction_id);
        
        // 완료된 단계들을 역순으로 보상
        for (int i = static_cast<int>(saga.current_step); i >= 0; --i) {
            auto& step = saga.steps[i];
            
            if (step.completed) {
                spdlog::info("Compensating step: {} for saga: {}", 
                           step.step_name, saga.transaction_id);
                
                try {
                    bool compensation_success = co_await step.compensate();
                    
                    if (compensation_success) {
                        PublishSagaStepEvent(saga.transaction_id, step.step_name, "compensated");
                    } else {
                        spdlog::error("Compensation failed for step: {} in saga: {}", 
                                    step.step_name, saga.transaction_id);
                        // 보상 실패는 별도로 처리 (수동 개입 필요)
                        PublishSagaStepEvent(saga.transaction_id, step.step_name, "compensation_failed");
                    }
                    
                } catch (const std::exception& e) {
                    spdlog::error("Compensation exception for step: {} in saga: {}: {}", 
                                step.step_name, saga.transaction_id, e.what());
                }
            }
        }
    }

private:
    // 🏦 실제 비즈니스 로직 구현
    async<bool> DeductGold(int64_t user_id, int32_t amount) {
        // 계정 서비스 gRPC 호출
        account::DeductGoldRequest request;
        request.set_user_id(user_id);
        request.set_amount(amount);
        
        auto response = co_await account_service_->DeductGold(request);
        co_return response.success();
    }
    
    async<bool> RefundGold(int64_t user_id, int32_t amount) {
        account::AddGoldRequest request;
        request.set_user_id(user_id);
        request.set_amount(amount);
        request.set_reason("Saga compensation");
        
        auto response = co_await account_service_->AddGold(request);
        co_return response.success();
    }
    
    async<bool> AddItemToInventory(int64_t user_id, int32_t item_id, int32_t quantity) {
        inventory::AddItemRequest request;
        request.set_user_id(user_id);
        request.set_item_id(item_id);
        request.set_quantity(quantity);
        
        auto response = co_await inventory_service_->AddItem(request);
        co_return response.success();
    }
    
    async<bool> RemoveItemFromInventory(int64_t user_id, int32_t item_id, int32_t quantity) {
        inventory::RemoveItemRequest request;
        request.set_user_id(user_id);
        request.set_item_id(item_id);
        request.set_quantity(quantity);
        request.set_reason("Saga compensation");
        
        auto response = co_await inventory_service_->RemoveItem(request);
        co_return response.success();
    }
    
    void PublishSagaEvent(const std::string& saga_id, const std::string& status, 
                         const std::string& error_message) {
        GameEvent event;
        event.type = "saga_status_changed";
        event.source_service = "saga-orchestrator";
        event.data = nlohmann::json{
            {"saga_id", saga_id},
            {"status", status},
            {"error_message", error_message},
            {"timestamp", GetCurrentTimestamp()}
        };
        
        event_bus_.PublishEvent("saga-events", event);
    }
};
```

### **4.2 Saga 모니터링 및 장애 복구**

```cpp
// 📊 Saga 모니터링 대시보드

class SagaMonitoringService {
private:
    std::unordered_map<std::string, SagaMetrics> saga_metrics_;
    
public:
    struct SagaMetrics {
        std::string saga_type;
        int total_started = 0;
        int total_completed = 0;
        int total_failed = 0;
        int total_compensated = 0;
        double avg_duration_ms = 0.0;
        std::vector<std::string> recent_failures;
    };
    
    void RecordSagaStart(const std::string& saga_type) {
        saga_metrics_[saga_type].total_started++;
    }
    
    void RecordSagaCompletion(const std::string& saga_type, double duration_ms) {
        auto& metrics = saga_metrics_[saga_type];
        metrics.total_completed++;
        
        // 평균 지속 시간 업데이트
        double total_duration = metrics.avg_duration_ms * (metrics.total_completed - 1);
        metrics.avg_duration_ms = (total_duration + duration_ms) / metrics.total_completed;
    }
    
    void RecordSagaFailure(const std::string& saga_type, const std::string& reason) {
        auto& metrics = saga_metrics_[saga_type];
        metrics.total_failed++;
        metrics.recent_failures.push_back(reason);
        
        // 최근 실패 10개만 유지
        if (metrics.recent_failures.size() > 10) {
            metrics.recent_failures.erase(metrics.recent_failures.begin());
        }
    }
    
    nlohmann::json GetMetricsReport() {
        nlohmann::json report = nlohmann::json::array();
        
        for (const auto& [saga_type, metrics] : saga_metrics_) {
            double success_rate = 0.0;
            if (metrics.total_started > 0) {
                success_rate = static_cast<double>(metrics.total_completed) / 
                              metrics.total_started * 100.0;
            }
            
            report.push_back({
                {"saga_type", saga_type},
                {"total_started", metrics.total_started},
                {"total_completed", metrics.total_completed},
                {"total_failed", metrics.total_failed},
                {"success_rate", success_rate},
                {"avg_duration_ms", metrics.avg_duration_ms},
                {"recent_failures", metrics.recent_failures}
            });
        }
        
        return report;
    }
    
    // 🚨 장애 감지 및 알림
    void CheckHealthAndAlert() {
        for (const auto& [saga_type, metrics] : saga_metrics_) {
            double success_rate = 0.0;
            if (metrics.total_started > 0) {
                success_rate = static_cast<double>(metrics.total_completed) / 
                              metrics.total_started * 100.0;
            }
            
            // 성공률이 95% 미만이면 알림
            if (success_rate < 95.0 && metrics.total_started >= 10) {
                AlertLowSuccessRate(saga_type, success_rate);
            }
            
            // 평균 지속시간이 10초 이상이면 알림
            if (metrics.avg_duration_ms > 10000.0) {
                AlertSlowSaga(saga_type, metrics.avg_duration_ms);
            }
        }
    }

private:
    void AlertLowSuccessRate(const std::string& saga_type, double success_rate) {
        spdlog::warn("Low saga success rate: {} ({}%)", saga_type, success_rate);
        
        // Slack, 이메일 등으로 알림 전송
        SendAlert({
            {"type", "low_success_rate"},
            {"saga_type", saga_type},
            {"success_rate", success_rate},
            {"threshold", 95.0}
        });
    }
    
    void AlertSlowSaga(const std::string& saga_type, double avg_duration) {
        spdlog::warn("Slow saga detected: {} ({}ms)", saga_type, avg_duration);
        
        SendAlert({
            {"type", "slow_saga"},
            {"saga_type", saga_type},
            {"avg_duration_ms", avg_duration},
            {"threshold_ms", 10000.0}
        });
    }
};
```

---

## 🔍 5. 서비스 디스커버리 & 헬스체크

### **5.1 서비스 디스커버리 구현**

```cpp
// 🗺️ 서비스 레지스트리 - 동적 서비스 발견

#include <consul/consul.hpp>  // Consul C++ 클라이언트
#include <etcd/etcd.hpp>      // etcd 대안

class ServiceDiscovery {
private:
    std::unique_ptr<consul::Consul> consul_client_;
    std::unordered_map<std::string, std::vector<ServiceInstance>> local_cache_;
    std::mutex cache_mutex_;
    std::atomic<bool> running_{true};

public:
    struct ServiceInstance {
        std::string id;
        std::string name;
        std::string address;
        int port;
        std::map<std::string, std::string> tags;
        bool healthy = true;
        std::chrono::steady_clock::time_point last_heartbeat;
    };
    
    ServiceDiscovery(const std::string& consul_address) {
        consul_client_ = std::make_unique<consul::Consul>(consul_address);
        StartServiceWatcher();
    }
    
    // 📋 서비스 등록
    bool RegisterService(const ServiceInstance& instance) {
        try {
            consul::AgentServiceRegistration registration;
            registration.id = instance.id;
            registration.name = instance.name;
            registration.address = instance.address;
            registration.port = instance.port;
            registration.tags = std::vector<std::string>();
            
            // 태그 변환
            for (const auto& [key, value] : instance.tags) {
                registration.tags.push_back(key + "=" + value);
            }
            
            // 헬스체크 설정
            consul::AgentServiceCheck health_check;
            health_check.http = "http://" + instance.address + ":" + 
                               std::to_string(instance.port) + "/health";
            health_check.interval = "10s";
            health_check.timeout = "5s";
            registration.check = health_check;
            
            auto result = consul_client_->agent().service_register(registration);
            if (result.error()) {
                spdlog::error("Failed to register service: {}", result.error_message());
                return false;
            }
            
            spdlog::info("Service registered: {} at {}:{}", 
                        instance.name, instance.address, instance.port);
            return true;
            
        } catch (const std::exception& e) {
            spdlog::error("Service registration exception: {}", e.what());
            return false;
        }
    }
    
    // 🔍 서비스 발견
    std::vector<ServiceInstance> DiscoverService(const std::string& service_name) {
        // 로컬 캐시 먼저 확인
        {
            std::lock_guard<std::mutex> lock(cache_mutex_);
            auto cache_it = local_cache_.find(service_name);
            if (cache_it != local_cache_.end()) {
                // 캐시가 유효한지 확인 (5초 이내)
                auto now = std::chrono::steady_clock::now();
                bool cache_valid = true;
                
                for (const auto& instance : cache_it->second) {
                    auto age = std::chrono::duration_cast<std::chrono::seconds>(
                        now - instance.last_heartbeat);
                    if (age.count() > 5) {
                        cache_valid = false;
                        break;
                    }
                }
                
                if (cache_valid) {
                    return cache_it->second;
                }
            }
        }
        
        // Consul에서 최신 정보 가져오기
        return RefreshServiceCache(service_name);
    }
    
    // 🎯 로드 밸런싱을 위한 건강한 인스턴스 선택
    ServiceInstance* SelectHealthyInstance(const std::string& service_name,
                                          const std::string& preferred_zone = "") {
        auto instances = DiscoverService(service_name);
        
        if (instances.empty()) {
            return nullptr;
        }
        
        // 건강한 인스턴스만 필터링
        std::vector<ServiceInstance*> healthy_instances;
        for (auto& instance : instances) {
            if (instance.healthy) {
                healthy_instances.push_back(&instance);
            }
        }
        
        if (healthy_instances.empty()) {
            spdlog::warn("No healthy instances found for service: {}", service_name);
            return nullptr;
        }
        
        // 선호 존(zone)이 있으면 우선 선택
        if (!preferred_zone.empty()) {
            for (auto* instance : healthy_instances) {
                auto zone_it = instance->tags.find("zone");
                if (zone_it != instance->tags.end() && zone_it->second == preferred_zone) {
                    return instance;
                }
            }
        }
        
        // 라운드 로빈 선택
        static std::atomic<size_t> round_robin_counter{0};
        size_t index = round_robin_counter++ % healthy_instances.size();
        return healthy_instances[index];
    }

private:
    std::vector<ServiceInstance> RefreshServiceCache(const std::string& service_name) {
        try {
            auto result = consul_client_->health().service(service_name, "", true); // passing=true
            
            if (result.error()) {
                spdlog::error("Failed to discover service {}: {}", 
                            service_name, result.error_message());
                return {};
            }
            
            std::vector<ServiceInstance> instances;
            auto now = std::chrono::steady_clock::now();
            
            for (const auto& health_entry : result.data()) {
                ServiceInstance instance;
                instance.id = health_entry.service.id;
                instance.name = health_entry.service.service;
                instance.address = health_entry.service.address;
                instance.port = health_entry.service.port;
                instance.healthy = true; // passing=true로 조회했으므로
                instance.last_heartbeat = now;
                
                // 태그 파싱
                for (const auto& tag : health_entry.service.tags) {
                    size_t pos = tag.find('=');
                    if (pos != std::string::npos) {
                        std::string key = tag.substr(0, pos);
                        std::string value = tag.substr(pos + 1);
                        instance.tags[key] = value;
                    }
                }
                
                instances.push_back(instance);
            }
            
            // 캐시 업데이트
            {
                std::lock_guard<std::mutex> lock(cache_mutex_);
                local_cache_[service_name] = instances;
            }
            
            spdlog::debug("Refreshed cache for service {}: {} instances", 
                         service_name, instances.size());
            
            return instances;
            
        } catch (const std::exception& e) {
            spdlog::error("Service discovery exception: {}", e.what());
            return {};
        }
    }
    
    void StartServiceWatcher() {
        std::thread watcher_thread([this]() {
            while (running_) {
                try {
                    // 주기적으로 모든 서비스 캐시 갱신
                    std::vector<std::string> service_names;
                    {
                        std::lock_guard<std::mutex> lock(cache_mutex_);
                        for (const auto& [name, instances] : local_cache_) {
                            service_names.push_back(name);
                        }
                    }
                    
                    for (const auto& service_name : service_names) {
                        RefreshServiceCache(service_name);
                    }
                    
                    std::this_thread::sleep_for(std::chrono::seconds(30));
                    
                } catch (const std::exception& e) {
                    spdlog::error("Service watcher error: {}", e.what());
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                }
            }
        });
        
        watcher_thread.detach();
    }
};
```

### **5.2 헬스체크 시스템**

```cpp
// 🏥 종합적인 헬스체크 시스템

class HealthCheckService {
private:
    std::unordered_map<std::string, HealthChecker> health_checkers_;
    std::atomic<bool> overall_healthy_{true};
    
public:
    enum class HealthStatus {
        HEALTHY,
        DEGRADED,      // 일부 기능 제한
        UNHEALTHY,     // 서비스 불가
        CRITICAL       // 즉시 조치 필요
    };
    
    struct HealthResult {
        HealthStatus status;
        std::string component;
        std::string message;
        std::map<std::string, std::string> details;
        std::chrono::steady_clock::time_point timestamp;
    };
    
    // 🔧 다양한 헬스체크 구성요소 등록
    void RegisterHealthCheckers() {
        // 데이터베이스 연결 상태
        health_checkers_["database"] = [this]() -> HealthResult {
            return CheckDatabaseHealth();
        };
        
        // Redis 연결 상태
        health_checkers_["redis"] = [this]() -> HealthResult {
            return CheckRedisHealth();
        };
        
        // 메모리 사용량
        health_checkers_["memory"] = [this]() -> HealthResult {
            return CheckMemoryHealth();
        };
        
        // CPU 사용률
        health_checkers_["cpu"] = [this]() -> HealthResult {
            return CheckCPUHealth();
        };
        
        // 디스크 공간
        health_checkers_["disk"] = [this]() -> HealthResult {
            return CheckDiskHealth();
        };
        
        // 외부 서비스 의존성
        health_checkers_["dependencies"] = [this]() -> HealthResult {
            return CheckDependencyHealth();
        };
        
        // 비즈니스 로직 상태
        health_checkers_["business"] = [this]() -> HealthResult {
            return CheckBusinessLogicHealth();
        };
    }
    
    // 🩺 종합 헬스체크 실행
    nlohmann::json PerformHealthCheck() {
        std::vector<HealthResult> results;
        auto overall_start = std::chrono::steady_clock::now();
        
        // 모든 체크 병렬 실행
        std::vector<std::future<HealthResult>> futures;
        for (const auto& [name, checker] : health_checkers_) {
            futures.push_back(std::async(std::launch::async, [name, checker]() {
                auto start = std::chrono::steady_clock::now();
                auto result = checker();
                auto end = std::chrono::steady_clock::now();
                
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                    end - start);
                result.details["check_duration_ms"] = std::to_string(duration.count());
                
                return result;
            }));
        }
        
        // 결과 수집
        for (auto& future : futures) {
            try {
                auto result = future.get();
                results.push_back(result);
            } catch (const std::exception& e) {
                HealthResult error_result;
                error_result.status = HealthStatus::CRITICAL;
                error_result.component = "unknown";
                error_result.message = "Health check exception: " + std::string(e.what());
                error_result.timestamp = std::chrono::steady_clock::now();
                results.push_back(error_result);
            }
        }
        
        // 전체 상태 결정
        HealthStatus overall_status = DetermineOverallStatus(results);
        overall_healthy_ = (overall_status == HealthStatus::HEALTHY || 
                           overall_status == HealthStatus::DEGRADED);
        
        auto overall_end = std::chrono::steady_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            overall_end - overall_start);
        
        // JSON 응답 생성
        nlohmann::json response;
        response["status"] = HealthStatusToString(overall_status);
        response["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        response["total_check_duration_ms"] = total_duration.count();
        response["service_info"]["name"] = GetServiceName();
        response["service_info"]["version"] = GetServiceVersion();
        response["service_info"]["uptime_seconds"] = GetUptimeSeconds();
        
        nlohmann::json checks = nlohmann::json::array();
        for (const auto& result : results) {
            nlohmann::json check_json;
            check_json["component"] = result.component;
            check_json["status"] = HealthStatusToString(result.status);
            check_json["message"] = result.message;
            check_json["details"] = result.details;
            checks.push_back(check_json);
        }
        response["checks"] = checks;
        
        return response;
    }

private:
    HealthResult CheckDatabaseHealth() {
        HealthResult result;
        result.component = "database";
        result.timestamp = std::chrono::steady_clock::now();
        
        try {
            auto start = std::chrono::high_resolution_clock::now();
            
            // 간단한 쿼리로 연결 확인
            auto db_connection = GetDatabaseConnection();
            auto query_result = db_connection->execute("SELECT 1");
            
            auto end = std::chrono::high_resolution_clock::now();
            auto query_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                end - start);
            
            result.details["query_time_ms"] = std::to_string(query_time.count());
            result.details["connection_pool_size"] = std::to_string(GetConnectionPoolSize());
            result.details["active_connections"] = std::to_string(GetActiveConnections());
            
            if (query_time.count() > 1000) {  // 1초 이상이면 성능 저하
                result.status = HealthStatus::DEGRADED;
                result.message = "Database response time is slow";
            } else {
                result.status = HealthStatus::HEALTHY;
                result.message = "Database is responding normally";
            }
            
        } catch (const std::exception& e) {
            result.status = HealthStatus::CRITICAL;
            result.message = "Database connection failed: " + std::string(e.what());
        }
        
        return result;
    }
    
    HealthResult CheckMemoryHealth() {
        HealthResult result;
        result.component = "memory";
        result.timestamp = std::chrono::steady_clock::now();
        
        // 메모리 사용량 확인
        auto memory_info = GetMemoryUsage();
        double usage_percentage = static_cast<double>(memory_info.used) / 
                                 memory_info.total * 100.0;
        
        result.details["total_mb"] = std::to_string(memory_info.total / 1024 / 1024);
        result.details["used_mb"] = std::to_string(memory_info.used / 1024 / 1024);
        result.details["usage_percentage"] = std::to_string(usage_percentage);
        
        if (usage_percentage > 90.0) {
            result.status = HealthStatus::CRITICAL;
            result.message = "Memory usage is critically high";
        } else if (usage_percentage > 80.0) {
            result.status = HealthStatus::DEGRADED;
            result.message = "Memory usage is high";
        } else {
            result.status = HealthStatus::HEALTHY;
            result.message = "Memory usage is normal";
        }
        
        return result;
    }
    
    HealthResult CheckBusinessLogicHealth() {
        HealthResult result;
        result.component = "business_logic";
        result.timestamp = std::chrono::steady_clock::now();
        
        try {
            // 핵심 비즈니스 로직 검증
            auto start = std::chrono::high_resolution_clock::now();
            
            // 예: 가상의 플레이어 생성/삭제 테스트
            auto test_player_id = CreateTestPlayer();
            if (test_player_id == 0) {
                throw std::runtime_error("Failed to create test player");
            }
            
            // 테스트 플레이어로 기본 작업 수행
            bool move_success = TestPlayerMovement(test_player_id);
            bool inventory_success = TestInventoryOperations(test_player_id);
            
            // 테스트 플레이어 정리
            DeleteTestPlayer(test_player_id);
            
            auto end = std::chrono::high_resolution_clock::now();
            auto test_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                end - start);
            
            result.details["test_duration_ms"] = std::to_string(test_duration.count());
            result.details["movement_test"] = move_success ? "passed" : "failed";
            result.details["inventory_test"] = inventory_success ? "passed" : "failed";
            
            if (move_success && inventory_success) {
                result.status = HealthStatus::HEALTHY;
                result.message = "Business logic is functioning normally";
            } else {
                result.status = HealthStatus::DEGRADED;
                result.message = "Some business logic tests failed";
            }
            
        } catch (const std::exception& e) {
            result.status = HealthStatus::CRITICAL;
            result.message = "Business logic test failed: " + std::string(e.what());
        }
        
        return result;
    }
    
    HealthStatus DetermineOverallStatus(const std::vector<HealthResult>& results) {
        bool has_critical = false;
        bool has_degraded = false;
        
        for (const auto& result : results) {
            switch (result.status) {
                case HealthStatus::CRITICAL:
                    has_critical = true;
                    break;
                case HealthStatus::UNHEALTHY:
                case HealthStatus::DEGRADED:
                    has_degraded = true;
                    break;
                case HealthStatus::HEALTHY:
                    break;
            }
        }
        
        if (has_critical) {
            return HealthStatus::CRITICAL;
        } else if (has_degraded) {
            return HealthStatus::DEGRADED;
        } else {
            return HealthStatus::HEALTHY;
        }
    }
    
    std::string HealthStatusToString(HealthStatus status) {
        switch (status) {
            case HealthStatus::HEALTHY: return "healthy";
            case HealthStatus::DEGRADED: return "degraded";
            case HealthStatus::UNHEALTHY: return "unhealthy";
            case HealthStatus::CRITICAL: return "critical";
            default: return "unknown";
        }
    }
};
```

---

## 🎯 6. 실습 프로젝트: 마이크로서비스 변환

### **6.1 단계별 마이크로서비스 분해 실습**

```cpp
// 🎮 실습: 기존 단일 채팅 서버를 마이크로서비스로 변환

// BEFORE: 단일 서버 (8단계 기초 문서의 채팅 서버)
class MonolithChatServer {
    ChatManager chat_manager_;      // 채팅 기능
    UserManager user_manager_;      // 사용자 관리
    AuthManager auth_manager_;      // 인증
    DatabaseManager db_manager_;    // DB 연결
    // 모든 기능이 하나의 프로세스에...
};

// AFTER: 마이크로서비스 분해
namespace ChatMicroservices {
    
    // 🔐 1. 인증 마이크로서비스
    class AuthMicroservice {
    public:
        // gRPC 서비스 구현
        grpc::Status Authenticate(grpc::ServerContext* context,
                                 const auth::AuthRequest* request,
                                 auth::AuthResponse* response) override {
            try {
                // JWT 기반 인증 로직
                auto user_info = ValidateCredentials(request->username(), 
                                                   request->password());
                if (user_info.has_value()) {
                    auto token = GenerateJWT(user_info.value());
                    response->set_success(true);
                    response->set_token(token);
                    response->mutable_user_info()->set_user_id(user_info->user_id);
                    response->mutable_user_info()->set_username(user_info->username);
                } else {
                    response->set_success(false);
                    response->set_error_message("Invalid credentials");
                }
                
                return grpc::Status::OK;
                
            } catch (const std::exception& e) {
                return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
            }
        }
        
        grpc::Status ValidateToken(grpc::ServerContext* context,
                                  const auth::TokenRequest* request,
                                  auth::TokenResponse* response) override {
            try {
                auto decoded_token = jwt::decode(request->token());
                auto verifier = jwt::verify()
                    .allow_algorithm(jwt::algorithm::hs256{GetJWTSecret()});
                
                verifier.verify(decoded_token);
                
                response->set_valid(true);
                response->set_user_id(decoded_token.get_payload_claim("user_id").as_int());
                return grpc::Status::OK;
                
            } catch (const std::exception& e) {
                response->set_valid(false);
                response->set_error_message(e.what());
                return grpc::Status::OK;
            }
        }
    };
    
    // 💬 2. 채팅 마이크로서비스
    class ChatMicroservice {
    private:
        ServiceCommunicationManager comm_manager_;  // 다른 서비스와 통신
        GameEventBus event_bus_;                   // 이벤트 발행
        
    public:
        grpc::Status SendMessage(grpc::ServerContext* context,
                               const chat::SendMessageRequest* request,
                               chat::SendMessageResponse* response) override {
            try {
                // 1. 토큰 검증 (인증 서비스 호출)
                auto auth_result = co_await comm_manager_.ValidateToken(request->token());
                if (!auth_result.valid) {
                    response->set_success(false);
                    response->set_error_message("Authentication failed");
                    co_return grpc::Status::OK;
                }
                
                // 2. 메시지 검증
                if (request->message().empty() || request->message().length() > 500) {
                    response->set_success(false);
                    response->set_error_message("Invalid message length");
                    co_return grpc::Status::OK;
                }
                
                // 3. 스팸 필터링
                if (IsSpamMessage(request->message())) {
                    response->set_success(false);
                    response->set_error_message("Message blocked by spam filter");
                    co_return grpc::Status::OK;
                }
                
                // 4. 메시지 저장 및 브로드캐스트
                ChatMessage message;
                message.user_id = auth_result.user_id;
                message.username = auth_result.username;
                message.content = request->message();
                message.channel_id = request->channel_id();
                message.timestamp = GetCurrentTimestamp();
                
                // 데이터베이스에 저장
                SaveChatMessage(message);
                
                // 실시간 브로드캐스트 (이벤트 발행)
                GameEvent event;
                event.type = "chat_message_sent";
                event.user_id = message.user_id;
                event.data = nlohmann::json{
                    {"message_id", message.id},
                    {"user_id", message.user_id},
                    {"username", message.username},
                    {"content", message.content},
                    {"channel_id", message.channel_id},
                    {"timestamp", message.timestamp}
                };
                
                event_bus_.PublishEvent("chat-events", event);
                
                response->set_success(true);
                response->set_message_id(message.id);
                
                co_return grpc::Status::OK;
                
            } catch (const std::exception& e) {
                response->set_success(false);
                response->set_error_message(e.what());
                co_return grpc::Status::OK;
            }
        }
        
        // 실시간 메시지 스트림 (WebSocket 대신 gRPC 스트리밍)
        grpc::Status StreamMessages(grpc::ServerContext* context,
                                   const chat::StreamRequest* request,
                                   grpc::ServerWriter<chat::ChatMessage>* writer) override {
            // 토큰 검증
            auto auth_result = ValidateToken(request->token());
            if (!auth_result.valid) {
                return grpc::Status(grpc::StatusCode::UNAUTHENTICATED, "Invalid token");
            }
            
            // 사용자를 스트림에 등록
            auto stream_id = RegisterMessageStream(auth_result.user_id, writer);
            
            // 연결이 끊어질 때까지 대기
            while (!context->IsCancelled()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }
            
            // 스트림 해제
            UnregisterMessageStream(stream_id);
            
            return grpc::Status::OK;
        }
    };
    
    // 👥 3. 사용자 관리 마이크로서비스
    class UserMicroservice {
    public:
        grpc::Status GetUserProfile(grpc::ServerContext* context,
                                   const user::GetProfileRequest* request,
                                   user::UserProfile* response) override {
            try {
                auto user_data = LoadUserFromDatabase(request->user_id());
                if (!user_data.has_value()) {
                    return grpc::Status(grpc::StatusCode::NOT_FOUND, "User not found");
                }
                
                response->set_user_id(user_data->user_id);
                response->set_username(user_data->username);
                response->set_email(user_data->email);
                response->set_created_at(user_data->created_at);
                response->set_last_login(user_data->last_login);
                
                return grpc::Status::OK;
                
            } catch (const std::exception& e) {
                return grpc::Status(grpc::StatusCode::INTERNAL, e.what());
            }
        }
        
        grpc::Status UpdateUserProfile(grpc::ServerContext* context,
                                      const user::UpdateProfileRequest* request,
                                      user::UpdateProfileResponse* response) override {
            // 프로필 업데이트 로직
            // 변경 사항을 이벤트로 발행 (다른 서비스들이 구독)
        }
    };
}
```

### **6.2 마이크로서비스 통합 테스트**

```cpp
// 🧪 마이크로서비스 통합 테스트 프레임워크

class MicroserviceIntegrationTest {
private:
    std::unique_ptr<TestEnvironment> test_env_;
    
public:
    class TestEnvironment {
    public:
        // 테스트용 서비스 인스턴스들
        std::unique_ptr<AuthMicroservice> auth_service;
        std::unique_ptr<ChatMicroservice> chat_service;
        std::unique_ptr<UserMicroservice> user_service;
        
        // 테스트용 인프라
        std::unique_ptr<TestDatabase> test_db;
        std::unique_ptr<TestRedis> test_redis;
        std::unique_ptr<TestMessageQueue> test_mq;
        
        void SetUp() {
            // 테스트 DB 초기화
            test_db = std::make_unique<TestDatabase>("test_game_db");
            test_db->CreateTables();
            test_db->SeedTestData();
            
            // 테스트 Redis 초기화
            test_redis = std::make_unique<TestRedis>("localhost:6380");
            test_redis->FlushAll();
            
            // 테스트 메시지 큐 초기화
            test_mq = std::make_unique<TestMessageQueue>("localhost:9093");
            
            // 서비스들 시작 (테스트 포트 사용)
            auth_service = std::make_unique<AuthMicroservice>();
            auth_service->StartServer("localhost:50051");
            
            chat_service = std::make_unique<ChatMicroservice>();
            chat_service->StartServer("localhost:50052");
            
            user_service = std::make_unique<UserMicroservice>();
            user_service->StartServer("localhost:50053");
            
            // 서비스들이 준비될 때까지 대기
            WaitForServicesReady();
        }
        
        void TearDown() {
            auth_service.reset();
            chat_service.reset();
            user_service.reset();
            
            test_db.reset();
            test_redis.reset();
            test_mq.reset();
        }
    };
    
    void SetUp() {
        test_env_ = std::make_unique<TestEnvironment>();
        test_env_->SetUp();
    }
    
    void TearDown() {
        test_env_->TearDown();
        test_env_.reset();
    }
    
    // 🧪 테스트 케이스 1: 기본 사용자 플로우
    void TestBasicUserFlow() {
        std::cout << "🧪 Testing basic user flow..." << std::endl;
        
        // 1. 사용자 인증
        auto auth_client = CreateAuthClient();
        
        auth::AuthRequest auth_request;
        auth_request.set_username("testuser");
        auth_request.set_password("testpassword");
        
        auth::AuthResponse auth_response;
        auto status = auth_client->Authenticate(auth_request, &auth_response);
        
        ASSERT_TRUE(status.ok());
        ASSERT_TRUE(auth_response.success());
        ASSERT_FALSE(auth_response.token().empty());
        
        std::string token = auth_response.token();
        int64_t user_id = auth_response.user_info().user_id();
        
        std::cout << "✅ Authentication successful: " << token.substr(0, 20) << "..." << std::endl;
        
        // 2. 사용자 프로필 조회
        auto user_client = CreateUserClient();
        
        user::GetProfileRequest profile_request;
        profile_request.set_user_id(user_id);
        
        user::UserProfile profile_response;
        status = user_client->GetUserProfile(profile_request, &profile_response);
        
        ASSERT_TRUE(status.ok());
        ASSERT_EQ(profile_response.username(), "testuser");
        
        std::cout << "✅ User profile retrieved: " << profile_response.username() << std::endl;
        
        // 3. 채팅 메시지 전송
        auto chat_client = CreateChatClient();
        
        chat::SendMessageRequest message_request;
        message_request.set_token(token);
        message_request.set_message("Hello, microservices!");
        message_request.set_channel_id(1);
        
        chat::SendMessageResponse message_response;
        status = chat_client->SendMessage(message_request, &message_response);
        
        ASSERT_TRUE(status.ok());
        ASSERT_TRUE(message_response.success());
        ASSERT_GT(message_response.message_id(), 0);
        
        std::cout << "✅ Message sent successfully: ID " << message_response.message_id() << std::endl;
        
        std::cout << "🎉 Basic user flow test passed!" << std::endl;
    }
    
    // 🧪 테스트 케이스 2: 서비스 장애 시나리오
    void TestServiceFailureScenario() {
        std::cout << "🧪 Testing service failure scenarios..." << std::endl;
        
        // 사용자 인증 후 토큰 획득
        auto token = AuthenticateTestUser();
        
        // 1. 인증 서비스 다운 시나리오
        std::cout << "📉 Simulating auth service failure..." << std::endl;
        test_env_->auth_service.reset();  // 인증 서비스 종료
        
        // 채팅 서비스는 계속 동작해야 함 (기존 토큰 검증은 로컬 캐시 사용)
        auto chat_client = CreateChatClient();
        
        chat::SendMessageRequest message_request;
        message_request.set_token(token);
        message_request.set_message("Message during auth service failure");
        message_request.set_channel_id(1);
        
        chat::SendMessageResponse message_response;
        auto status = chat_client->SendMessage(message_request, &message_response);
        
        // 서비스는 계속 동작하되, 새로운 인증은 불가능해야 함
        ASSERT_TRUE(status.ok());  // 기존 토큰은 작동
        
        std::cout << "✅ Chat service resilient to auth service failure" << std::endl;
        
        // 2. 인증 서비스 복구
        test_env_->auth_service = std::make_unique<AuthMicroservice>();
        test_env_->auth_service->StartServer("localhost:50051");
        
        std::this_thread::sleep_for(std::chrono::seconds(2));  // 복구 대기
        
        // 새로운 인증이 다시 가능해야 함
        auto new_token = AuthenticateTestUser();
        ASSERT_FALSE(new_token.empty());
        
        std::cout << "✅ Auth service recovered successfully" << std::endl;
        std::cout << "🎉 Service failure scenario test passed!" << std::endl;
    }
    
    // 🧪 테스트 케이스 3: 부하 테스트
    void TestLoadScenario() {
        std::cout << "🧪 Testing load scenario..." << std::endl;
        
        const int concurrent_users = 100;
        const int messages_per_user = 10;
        
        std::vector<std::thread> user_threads;
        std::atomic<int> successful_messages{0};
        std::atomic<int> failed_messages{0};
        
        auto start_time = std::chrono::steady_clock::now();
        
        // 동시 사용자 시뮬레이션
        for (int i = 0; i < concurrent_users; ++i) {
            user_threads.emplace_back([this, i, messages_per_user, &successful_messages, &failed_messages]() {
                try {
                    // 각 사용자별 인증
                    auto token = AuthenticateTestUser("testuser" + std::to_string(i));
                    auto chat_client = CreateChatClient();
                    
                    // 메시지 전송
                    for (int j = 0; j < messages_per_user; ++j) {
                        chat::SendMessageRequest request;
                        request.set_token(token);
                        request.set_message("Load test message " + std::to_string(j));
                        request.set_channel_id(1);
                        
                        chat::SendMessageResponse response;
                        auto status = chat_client->SendMessage(request, &response);
                        
                        if (status.ok() && response.success()) {
                            successful_messages++;
                        } else {
                            failed_messages++;
                        }
                        
                        // 약간의 지연 (현실적인 사용자 행동 모방)
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                    
                } catch (const std::exception& e) {
                    std::cerr << "User thread error: " << e.what() << std::endl;
                    failed_messages += messages_per_user;
                }
            });
        }
        
        // 모든 스레드 완료 대기
        for (auto& thread : user_threads) {
            thread.join();
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);
        
        int total_messages = successful_messages + failed_messages;
        double success_rate = static_cast<double>(successful_messages) / total_messages * 100.0;
        double messages_per_second = static_cast<double>(successful_messages) / duration.count();
        
        std::cout << "📊 Load test results:" << std::endl;
        std::cout << "  Duration: " << duration.count() << " seconds" << std::endl;
        std::cout << "  Successful messages: " << successful_messages << std::endl;
        std::cout << "  Failed messages: " << failed_messages << std::endl;
        std::cout << "  Success rate: " << success_rate << "%" << std::endl;
        std::cout << "  Messages per second: " << messages_per_second << std::endl;
        
        // 성공률 95% 이상 요구
        ASSERT_GE(success_rate, 95.0);
        
        std::cout << "🎉 Load scenario test passed!" << std::endl;
    }

private:
    std::string AuthenticateTestUser(const std::string& username = "testuser") {
        auto auth_client = CreateAuthClient();
        
        auth::AuthRequest request;
        request.set_username(username);
        request.set_password("testpassword");
        
        auth::AuthResponse response;
        auto status = auth_client->Authenticate(request, &response);
        
        if (status.ok() && response.success()) {
            return response.token();
        }
        
        return "";
    }
    
    std::unique_ptr<AuthServiceClient> CreateAuthClient() {
        return std::make_unique<AuthServiceClient>("localhost:50051");
    }
    
    std::unique_ptr<ChatServiceClient> CreateChatClient() {
        return std::make_unique<ChatServiceClient>("localhost:50052");
    }
    
    std::unique_ptr<UserServiceClient> CreateUserClient() {
        return std::make_unique<UserServiceClient>("localhost:50053");
    }
};

// 🏃‍♂️ 테스트 실행기
int main() {
    std::cout << "🚀 Starting Microservice Integration Tests" << std::endl;
    
    MicroserviceIntegrationTest test_suite;
    
    try {
        test_suite.SetUp();
        
        test_suite.TestBasicUserFlow();
        test_suite.TestServiceFailureScenario();
        test_suite.TestLoadScenario();
        
        test_suite.TearDown();
        
        std::cout << "🎉 All integration tests passed!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Integration test failed: " << e.what() << std::endl;
        test_suite.TearDown();
        return 1;
    }
}
```

## 🔥 흔한 실수와 해결방법 (Common Mistakes & Solutions)

### 1. 과도한 서비스 분해
```cpp
// ❌ 잘못된 방법 - 너무 작은 단위로 분해
// [SEQUENCE: 1] 서비스 간 통신 오버헤드로 성능 저하
class TooManyMicroservices {
    // 30개의 마이크로서비스로 분해!
    UserNameService name_service_;
    UserEmailService email_service_;
    UserPasswordService password_service_;
    UserProfileService profile_service_;
    // ... 너무 많은 서비스
    
    void GetUserInfo(uint64_t user_id) {
        // 하나의 유저 정보를 위해 10번의 네트워크 호출!
        auto name = name_service_.GetName(user_id);
        auto email = email_service_.GetEmail(user_id);
        auto password = password_service_.GetHash(user_id);
        // 레이턴시 폭증!
    }
};

// ✅ 올바른 방법 - 도메인 경계로 적절히 분해
// [SEQUENCE: 2] 응집도 높은 서비스로 통신 최소화
class ProperMicroservices {
    // 비즈니스 도메인 기준으로 분해
    AuthService auth_service_;      // 인증 관련 모든 기능
    UserService user_service_;      // 사용자 정보 관리
    GameService game_service_;      // 게임 로직
    
    void GetUserInfo(uint64_t user_id) {
        // 한 번의 호출로 관련 정보 모두 가져오기
        auto user_info = user_service_.GetFullUserInfo(user_id);
    }
};
```

### 2. 분산 트랜잭션 무시
```cpp
// ❌ 잘못된 방법 - 트랜잭션 없이 여러 서비스 호출
// [SEQUENCE: 3] 부분 실패로 데이터 불일치
class NoTransactionPattern {
    void TransferItem(uint64_t from_user, uint64_t to_user, Item item) {
        // 1. 아이템 제거
        inventory_service_.RemoveItem(from_user, item);
        
        // 2. 여기서 크래시하면?
        if (network_error) throw;  // 아이템만 사라짐!
        
        // 3. 아이템 추가
        inventory_service_.AddItem(to_user, item);
    }
};

// ✅ 올바른 방법 - Saga 패턴으로 보상 트랜잭션
// [SEQUENCE: 4] 실패 시 자동 롤백
class SagaTransactionPattern {
    void TransferItem(uint64_t from_user, uint64_t to_user, Item item) {
        SagaTransaction saga;
        
        // 1. 보상 가능한 액션 등록
        saga.AddStep(
            [=]() { return inventory_service_.RemoveItem(from_user, item); },
            [=]() { inventory_service_.AddItem(from_user, item); }  // 보상
        );
        
        saga.AddStep(
            [=]() { return inventory_service_.AddItem(to_user, item); },
            [=]() { inventory_service_.RemoveItem(to_user, item); }  // 보상
        );
        
        // 2. 실행 (실패 시 자동 롤백)
        if (!saga.Execute()) {
            LOG_ERROR("Item transfer failed, rolled back");
        }
    }
};
```

### 3. 서비스 디스커버리 하드코딩
```cpp
// ❌ 잘못된 방법 - 서비스 주소 하드코딩
// [SEQUENCE: 5] 서비스 이동/스케일링 시 전체 수정 필요
class HardcodedServices {
    void ConnectToServices() {
        auth_client_ = CreateClient("192.168.1.10:8080");  // IP 하드코딩!
        chat_client_ = CreateClient("192.168.1.11:8081");
        game_client_ = CreateClient("192.168.1.12:8082");
        // 서비스 주소 변경 시 재컴파일 필요
    }
};

// ✅ 올바른 방법 - 동적 서비스 디스커버리
// [SEQUENCE: 6] 서비스 위치 자동 발견
class DynamicServiceDiscovery {
    ServiceRegistry registry_;
    
    void ConnectToServices() {
        // Consul/Eureka에서 동적으로 서비스 발견
        auth_client_ = CreateClient(
            registry_.Discover("auth-service")
        );
        
        // 헬스체크 실패한 인스턴스 자동 제외
        chat_client_ = CreateClient(
            registry_.DiscoverHealthy("chat-service")
        );
        
        // 로드 밸런싱도 자동으로
        game_client_ = CreateClient(
            registry_.DiscoverWithLoadBalancing("game-service")
        );
    }
};
```

## 🚀 실습 프로젝트 (Practice Projects)

### 📌 기초 프로젝트: 3-티어 게임 서버 분해
**목표**: 모놀리식 게임 서버를 Gateway, Game, Database 서비스로 분해

1. **구현 내용**:
   - API Gateway 구현
   - 게임 로직 서비스 분리
   - 데이터베이스 서비스 래퍼
   - Docker Compose로 로컬 실행

2. **학습 포인트**:
   - 서비스 경계 설정
   - REST/gRPC 통신
   - 컨테이너 오케스트레이션

### 🚀 중급 프로젝트: 이벤트 기반 마이크로서비스
**목표**: 6개 서비스가 Kafka로 통신하는 게임 서버

1. **서비스 구성**:
   - 인증 서비스 (JWT 발급)
   - 매치메이킹 서비스
   - 게임 세션 서비스
   - 채팅 서비스
   - 분석 서비스
   - 알림 서비스

2. **핵심 기능**:
   - 이벤트 소싱
   - CQRS 패턴
   - 서킷 브레이커
   - 분산 트레이싱

### 🏆 고급 프로젝트: 완전한 마이크로서비스 게임
**목표**: 프로덕션 레벨의 마이크로서비스 MMORPG 서버

1. **고급 기능**:
   - Service Mesh (Istio)
   - 분산 트랜잭션 (Saga)
   - 다중 데이터베이스
   - 글로벌 스케일링

2. **운영 기능**:
   - 블루-그린 배포
   - 카나리 릴리즈
   - A/B 테스팅
   - 카오스 엔지니어링

## 📊 학습 체크리스트 (Learning Checklist)

### 🥉 브론즈 레벨
- [ ] 마이크로서비스 개념 이해
- [ ] Docker 컨테이너 생성
- [ ] 서비스 간 HTTP 통신
- [ ] 기본 API Gateway 구현

### 🥈 실버 레벨
- [ ] gRPC 서비스 구현
- [ ] 서비스 디스커버리 설정
- [ ] 메시지 큐 통합
- [ ] 분산 로깅 구현

### 🥇 골드 레벨
- [ ] Saga 패턴 구현
- [ ] 서킷 브레이커 적용
- [ ] 분산 트레이싱 설정
- [ ] Kubernetes 배포

### 💎 플래티넘 레벨
- [ ] Service Mesh 구성
- [ ] 멀티 클러스터 배포
- [ ] 글로벌 트래픽 관리
- [ ] 완전 자동화된 CI/CD

## 📚 추가 학습 자료 (Additional Resources)

### 📖 추천 도서
1. **"Building Microservices"** - Sam Newman
   - 마이크로서비스 설계의 바이블
   
2. **"Microservices Patterns"** - Chris Richardson
   - 실전 패턴과 안티패턴

3. **"Domain-Driven Design"** - Eric Evans
   - 서비스 경계 설정의 핵심

### 🎓 온라인 코스
1. **Microservices with Spring Boot** - Udemy
   - 실습 중심 마이크로서비스 개발
   
2. **Kubernetes for Developers** - Pluralsight
   - 컨테이너 오케스트레이션

3. **Event-Driven Microservices** - O'Reilly
   - 이벤트 기반 아키텍처

### 🛠 필수 도구
1. **Docker & Kubernetes** - 컨테이너화
2. **Istio** - Service Mesh
3. **Jaeger** - 분산 트레이싱
4. **Prometheus & Grafana** - 모니터링

### 💬 커뮤니티
1. **Microservices.io** - 패턴 카탈로그
2. **CNCF Slack** - 클라우드 네이티브 커뮤니티
3. **r/microservices** - Reddit 커뮤니티
4. **DDD Community** - 도메인 주도 설계

---

## 🎯 마무리

**🎉 축하합니다!** 이제 여러분은 **마이크로서비스 아키텍처 설계**의 핵심을 이해했습니다!

### **지금 할 수 있는 것들:**
- ✅ **서비스 분해**: 비즈니스 도메인 기반으로 서비스 경계 설정
- ✅ **API Gateway**: 단일 진입점으로 라우팅, 인증, Rate Limiting
- ✅ **서비스 통신**: gRPC 동기 + Kafka 비동기 하이브리드 구조
- ✅ **분산 트랜잭션**: Saga 패턴으로 데이터 일관성 보장
- ✅ **서비스 디스커버리**: 동적 서비스 발견 및 로드 밸런싱
- ✅ **헬스체크**: 종합적인 서비스 상태 모니터링

### **실제 성과:**
- **독립 배포**: 각 서비스별 독립적 업데이트 가능
- **장애 격리**: 부분 장애 시에도 80% 기능 정상 동작
- **확장성**: 트래픽 패턴별 선택적 스케일링
- **기술 다양성**: 서비스별 최적 기술 스택 선택

### **다음에 학습할 문서 추천:**
1. **A1. 데이터베이스 설계 & 최적화** - 마이크로서비스 데이터 관리
2. **A2. Redis 클러스터 & 캐싱** - 분산 시스템 성능 최적화  
3. **B5. 이벤트 드리븐 아키텍처** - 서비스 간 느슨한 결합

### **💪 실습 과제:**
1. **8단계의 채팅 서버**를 이 문서의 가이드로 **마이크로서비스로 분해**해보세요
2. **Saga 패턴**으로 복잡한 트랜잭션 시나리오 구현해보세요
3. **통합 테스트**로 마이크로서비스 간 상호작용 검증해보세요

**🚀 마이크로서비스 아키텍처는 현대 게임 서버 개발의 핵심입니다. 계속 실습하면서 다음 단계로 나아가세요!**