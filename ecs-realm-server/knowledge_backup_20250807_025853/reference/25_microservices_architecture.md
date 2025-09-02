# 13단계: 마이크로서비스 아키텍처 - 하나의 거대한 서버를 여러 전문가로 나누기
*모놀리식 서버를 작은 전문 서비스들로 나누어 확장성과 유지보수성 극대화하기*

> **🎯 목표**: 1000만 명 동접이 가능한 분산 마이크로서비스 게임 아키텍처 구축하기

---

## 📋 문서 정보

- **난이도**: 🔴 고급 (하지만 차근차근 따라하면 가능함!)
- **예상 학습 시간**: 8-10시간 (이론 + 실습)
- **필요 선수 지식**: 
  - ✅ [1-12단계](./01_advanced_cpp_features.md) 모든 내용 완료
  - ✅ "RESTful API"가 뭔지 대략 알고 있음
  - ✅ "분산 시스템"을 들어본 적 있음
  - ✅ Docker 기본 개념 이해
- **실습 환경**: C++17, Boost.Beast, Consul, Docker, Kubernetes
- **최종 결과물**: 독립적으로 확장 가능한 마이크로서비스 게임 플랫폼!

---

## 🤔 왜 거대한 서버 하나보다 작은 서버 여러 개가 좋을까?

### **모놀리식 서버에서 일어나는 문제들**

```
😰 하나의 거대한 서버에서 벌어지는 재앙들

🏢 회사 비유로 이해하기:
├── "모든 직원이 한 건물에서 모든 일을 처리" 🏢
├── CEO부터 청소부까지 한 사무실에... 😵
├── 회계팀 문제로 전체 회사 마비! 💀
├── 마케팅팀 확장하려면 전체 건물 이사? 🚚
└── 한 명 실수하면 모든 부서 영향... 😱

🎮 게임 서버로 번역하면:
├── 로그인, 게임로직, 길드, 채팅 모두 한 프로그램
├── 채팅 서버 문제로 전체 게임 다운
├── 길드전 많아지면 전체 서버 업그레이드
├── 한 기능 버그로 모든 플레이어 영향
└── 새 기능 추가하면 전체 서버 재시작
```

### **실제 모놀리식 서버의 끔찍한 사례**

```cpp
// 😰 모든 기능이 한 덩어리인 괴물 서버

class MonolithicGameServer {
public:
    // 💀 문제: 모든 기능이 하나의 프로세스에...
    AuthenticationManager auth_manager_;
    PlayerManager player_manager_;
    GuildManager guild_manager_;
    WorldManager world_manager_;
    ChatManager chat_manager_;
    ShopManager shop_manager_;
    DatabaseManager database_manager_;
    
    void Start() {
        // 😱 한 기능이라도 초기화 실패하면 전체 서버 죽음
        auth_manager_.Initialize();
        player_manager_.Initialize();
        guild_manager_.Initialize();  // 길드 DB 문제로 실패!
        // 전체 서버 시작 불가능... 😭
    }
    
    void HandleRequest(const Request& request) {
        // 😰 채팅 요청 폭증으로 전체 서버 과부하
        if (request.type == "chat") {
            chat_manager_.HandleChat(request);  // CPU 100% 사용
        }
        // 다른 모든 기능도 느려짐... 😵
    }
};

// 😰 실제 벌어지는 재앙들:
void DemoMonolithProblems() {
    MonolithicGameServer server;
    
    // 😱 문제 1: 확장성 재앙
    std::cout << "채팅 사용자 급증!" << std::endl;
    std::cout << "→ 채팅만 확장하려면? 전체 서버 10배 늘려야 함" << std::endl;
    std::cout << "→ 비용: 월 $10,000 → $100,000 😭" << std::endl;
    
    // 😱 문제 2: 배포 지옥
    std::cout << "\n새 채팅 기능 추가" << std::endl;
    std::cout << "→ 전체 서버 재시작 필요" << std::endl;
    std::cout << "→ 모든 플레이어 강제 로그아웃 💀" << std::endl;
    
    // 😱 문제 3: 장애 전파
    std::cout << "\n길드 기능에 버그 발생" << std::endl;
    std::cout << "→ 전체 서버 크래시" << std::endl;
    std::cout << "→ 로그인, 게임, 채팅 모두 마비 😵" << std::endl;
}
```

### **마이크로서비스의 놀라운 해결책 ✨**

```cpp
// ✨ 각자 전문분야가 있는 작은 서비스들

class MicroservicesArchitecture {
public:
    // 🎯 각 서비스가 독립적으로 동작!
    struct Services {
        AuthService auth_service;      // 포트 8001: 인증 전문
        PlayerService player_service;  // 포트 8002: 플레이어 관리 전문
        GuildService guild_service;    // 포트 8003: 길드 전문
        ChatService chat_service;      // 포트 8004: 채팅 전문
        WorldService world_service;    // 포트 8005: 게임월드 전문
    };
    
    void StartServices() {
        // 😍 각 서비스가 독립적으로 시작!
        std::vector<std::thread> service_threads;
        
        service_threads.emplace_back([]() {
            AuthService auth;
            auth.Start(8001);  // 인증 서비스 시작
        });
        
        service_threads.emplace_back([]() {
            ChatService chat;
            chat.Start(8004);  // 채팅 서비스 시작
        });
        
        // ✨ 길드 서비스 문제 있어도 다른 서비스는 정상 동작!
        try {
            GuildService guild;
            guild.Start(8003);
        } catch (const std::exception& e) {
            std::cout << "길드 서비스만 문제, 다른 서비스는 정상! ✨" << std::endl;
        }
    }
    
    void HandleGrowth() {
        // 🚀 채팅 사용자 급증? 채팅 서비스만 확장!
        for (int i = 0; i < 10; ++i) {
            StartChatService(8004 + i);  // 채팅 서비스 10개 실행
        }
        
        // 😍 다른 서비스는 그대로! 비용 10분의 1로 해결!
        std::cout << "채팅 서비스만 10배 확장 완료!" << std::endl;
        std::cout << "비용 절약: $90,000/월 💰" << std::endl;
    }
};

// 🎮 놀라운 개선 결과
void DemoMicroservices() {
    std::cout << "🏢 모놀리스 vs 🏗️ 마이크로서비스" << std::endl;
    std::cout << "────────────────────────────────" << std::endl;
    
    std::cout << "확장성:" << std::endl;
    std::cout << "모놀리스: 전체 서버 10배 확장 ($100,000/월) 😰" << std::endl;
    std::cout << "마이크로: 필요한 서비스만 확장 ($10,000/월) 🚀" << std::endl;
    
    std::cout << "\n배포:" << std::endl;
    std::cout << "모놀리스: 전체 서버 재시작 (모든 플레이어 로그아웃) 💀" << std::endl;
    std::cout << "마이크로: 해당 서비스만 재시작 (무중단 배포) ✨" << std::endl;
    
    std::cout << "\n장애 처리:" << std::endl;
    std::cout << "모놀리스: 한 기능 문제로 전체 서비스 다운 😭" << std::endl;
    std::cout << "마이크로: 해당 서비스만 문제, 다른 기능 정상 🛡️" << std::endl;
}
```

**💡 마이크로서비스의 핵심 개념:**
- **서비스 분해**: 큰 서버를 작은 전문 서비스들로 나누기
- **독립 배포**: 각 서비스를 독립적으로 업데이트 가능
- **기술 다양성**: 서비스마다 적합한 기술 선택 가능
- **장애 격리**: 한 서비스 문제가 다른 서비스에 영향 없음
- **팀 자율성**: 각 팀이 자신의 서비스를 완전히 소유

## 📚 우리 MMORPG 서버의 진화 과정

```
🏗️ 단계별 마이크로서비스 전환 로드맵

현재 (모놀리스): 하나의 거대한 서버
└── 모든 기능이 한 프로그램 안에
    ├── 관리하기 쉬움 (처음에는...)
    ├── 배포 단순함 (전체를 한 번에)
    └── 서버 3-5개까지만 확장 가능

1단계 (핵심 서비스 분리): 위험 없는 것부터
├── 인증 서비스 분리 (가장 안전함)
├── 플레이어 서비스 분리
├── 길드 서비스 분리
└── 서버 10-20개까지 확장 가능

2단계 (API 게이트웨이): 입구 통합 관리
├── API 게이트웨이 도입 (모든 요청의 관문)
├── 서비스 디스커버리 (서비스들이 서로 찾기)
├── 로드 밸런싱 (부하 분산)
└── 서버 50-100개까지 확장 가능

3단계 (고급 패턴): 엔터프라이즈급
├── 서킷 브레이커 (장애 격리)
├── 분산 추적 (문제 원인 찾기)
├── 이벤트 소싱 (모든 변경 기록)
└── 서버 1000개+ 확장 가능 (1000만 동접!)
```

---

## 🎯 Service Decomposition Strategy

### Domain-Driven Service Boundaries

**`src/microservices/service_boundaries.h` - Design:**
```cpp
// [SEQUENCE: 1] Service boundary definitions based on game domains
namespace GameServices {
    
    // [SEQUENCE: 2] Authentication & Authorization Service
    struct AuthService {
        static constexpr const char* SERVICE_NAME = "auth-service";
        static constexpr int DEFAULT_PORT = 8001;
        
        struct Responsibilities {
            // User authentication and JWT token management
            // Session validation and refresh
            // Password policies and security
            // OAuth integration (Steam, Discord, etc.)
        };
        
        struct APIs {
            // POST /auth/login
            // POST /auth/refresh
            // POST /auth/logout
            // GET /auth/validate
        };
    };
    
    // [SEQUENCE: 3] Player Management Service
    struct PlayerService {
        static constexpr const char* SERVICE_NAME = "player-service";
        static constexpr int DEFAULT_PORT = 8002;
        
        struct Responsibilities {
            // Player profile and character management
            // Inventory and equipment
            // Player statistics and progression
            // Friends and social connections
        };
        
        struct DataOwnership {
            // players table
            // character_stats table  
            // player_inventory table
            // player_friends table
        };
    };
    
    // [SEQUENCE: 4] Guild & Social Service
    struct GuildService {
        static constexpr const char* SERVICE_NAME = "guild-service";
        static constexpr int DEFAULT_PORT = 8003;
        
        struct Responsibilities {
            // Guild creation and management
            // Guild wars and large-scale PvP
            // Chat and messaging systems
            // Social features (parties, alliances)
        };
        
        struct EventsProduced {
            // guild.created
            // guild.war.started
            // guild.member.joined
            // chat.message.sent
        };
    };
    
    // [SEQUENCE: 5] World & Game Logic Service
    struct WorldService {
        static constexpr const char* SERVICE_NAME = "world-service";
        static constexpr int DEFAULT_PORT = 8004;
        
        struct Responsibilities {
            // Game world state management
            // Real-time combat and movement
            // NPC and monster AI
            // World events and spawning
        };
        
        struct PerformanceRequirements {
            static constexpr int MAX_LATENCY_MS = 50;
            static constexpr int TARGET_TPS = 60;  // Ticks per second
            static constexpr int MAX_CONCURRENT_PLAYERS = 2000;
        };
    };
    
    // [SEQUENCE: 6] Analytics & Metrics Service
    struct AnalyticsService {
        static constexpr const char* SERVICE_NAME = "analytics-service";
        static constexpr int DEFAULT_PORT = 8005;
        
        struct Responsibilities {
            // Player behavior analytics
            // Game balance metrics
            // Real-time monitoring dashboards
            // A/B testing framework
        };
    };
}
```

### Service Implementation Template

**Base Service Framework:**
```cpp
// [SEQUENCE: 7] Common microservice base class
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

template<typename ServiceConfig>
class MicroserviceBase {
protected:
    boost::asio::io_context io_context_;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    
    // Service discovery and health
    std::unique_ptr<ServiceDiscoveryClient> discovery_client_;
    std::unique_ptr<HealthChecker> health_checker_;
    
    // Configuration and metrics
    ServiceConfig config_;
    std::shared_ptr<PrometheusMetrics> metrics_;
    
public:
    MicroserviceBase(const ServiceConfig& config) : config_(config) {
        // [SEQUENCE: 8] Initialize common components
        metrics_ = std::make_shared<PrometheusMetrics>(config_.service_name);
        discovery_client_ = std::make_unique<ServiceDiscoveryClient>(config_.consul_address);
        health_checker_ = std::make_unique<HealthChecker>();
        
        // [SEQUENCE: 9] Set up HTTP server
        acceptor_ = std::make_unique<boost::asio::ip::tcp::acceptor>(
            io_context_, 
            boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), config_.port)
        );
    }
    
    // [SEQUENCE: 10] Service lifecycle management
    virtual void Start() {
        // Register with service discovery
        ServiceRegistration registration{
            .service_name = config_.service_name,
            .service_id = GenerateServiceId(),
            .address = GetLocalIP(),
            .port = config_.port,
            .health_check_url = fmt::format("http://{}:{}/health", GetLocalIP(), config_.port),
            .tags = {"game-server", "microservice"}
        };
        
        discovery_client_->RegisterService(registration);
        
        // Start health check endpoint
        StartHealthCheckEndpoint();
        
        // Start accepting HTTP requests
        StartAcceptLoop();
        
        spdlog::info("{} started on port {}", config_.service_name, config_.port);
    }
    
    virtual void Stop() {
        // Deregister from service discovery
        discovery_client_->DeregisterService();
        
        // Graceful shutdown
        acceptor_->close();
        io_context_.stop();
        
        spdlog::info("{} stopped", config_.service_name);
    }
    
    // [SEQUENCE: 11] HTTP request routing (must be implemented by derived classes)
    virtual nlohmann::json HandleRequest(const std::string& method, 
                                       const std::string& path,
                                       const nlohmann::json& body) = 0;
    
private:
    // [SEQUENCE: 12] HTTP request handling
    void StartAcceptLoop() {
        auto session = std::make_shared<HttpSession>(io_context_, *this);
        acceptor_->async_accept(session->GetSocket(),
            [this, session](boost::system::error_code ec) {
                if (!ec) {
                    session->Start();
                }
                StartAcceptLoop();  // Continue accepting
            });
    }
    
    void StartHealthCheckEndpoint() {
        // [SEQUENCE: 13] Health check implementation
        health_checker_->RegisterCheck("database", [this]() {
            return CheckDatabaseConnection();
        });
        
        health_checker_->RegisterCheck("memory", [this]() {
            return GetMemoryUsage() < config_.max_memory_mb;
        });
        
        health_checker_->RegisterCheck("cpu", [this]() {
            return GetCPUUsage() < config_.max_cpu_percent;
        });
    }
};
```

---

## 🚪 API Gateway Implementation

### Centralized Request Routing

**`src/microservices/api_gateway/gateway_server.h` - Implementation:**
```cpp
// [SEQUENCE: 14] API Gateway for microservices routing
#include <boost/beast.hpp>
#include <boost/asio.hpp>
#include <unordered_map>

class APIGateway {
private:
    boost::asio::io_context io_context_;
    boost::beast::http::response<boost::beast::http::string_body> response_;
    
    // Service discovery and load balancing
    std::unique_ptr<ServiceDiscoveryClient> discovery_client_;
    std::unique_ptr<LoadBalancer> load_balancer_;
    
    // Rate limiting and authentication
    std::unique_ptr<RateLimiter> rate_limiter_;
    std::unique_ptr<JWTValidator> jwt_validator_;
    
    // Route configuration
    struct Route {
        std::string path_pattern;
        std::string target_service;
        std::vector<std::string> methods;
        bool requires_auth;
        int rate_limit_per_minute;
    };
    
    std::vector<Route> routes_;
    
public:
    APIGateway(const GatewayConfig& config) {
        // [SEQUENCE: 15] Initialize gateway components
        discovery_client_ = std::make_unique<ServiceDiscoveryClient>(config.consul_address);
        load_balancer_ = std::make_unique<RoundRobinLoadBalancer>();
        rate_limiter_ = std::make_unique<TokenBucketRateLimiter>();
        jwt_validator_ = std::make_unique<JWTValidator>(config.jwt_secret);
        
        // [SEQUENCE: 16] Configure routing rules
        ConfigureRoutes();
    }
    
    // [SEQUENCE: 17] Main request handling pipeline
    boost::beast::http::response<boost::beast::http::string_body> 
    HandleRequest(boost::beast::http::request<boost::beast::http::string_body> request) {
        
        std::string client_ip = ExtractClientIP(request);
        std::string path = std::string(request.target());
        std::string method = std::string(request.method_string());
        
        try {
            // [SEQUENCE: 18] Authentication middleware
            if (!AuthenticateRequest(request)) {
                return CreateErrorResponse(401, "Unauthorized");
            }
            
            // [SEQUENCE: 19] Rate limiting middleware
            if (!rate_limiter_->AllowRequest(client_ip)) {
                return CreateErrorResponse(429, "Too Many Requests");
            }
            
            // [SEQUENCE: 20] Route matching and service discovery
            auto route = FindMatchingRoute(path, method);
            if (!route) {
                return CreateErrorResponse(404, "Route Not Found");
            }
            
            auto service_instances = discovery_client_->GetHealthyInstances(route->target_service);
            if (service_instances.empty()) {
                return CreateErrorResponse(503, "Service Unavailable");
            }
            
            // [SEQUENCE: 21] Load balancing
            auto target_instance = load_balancer_->SelectInstance(service_instances);
            
            // [SEQUENCE: 22] Forward request to microservice
            auto response = ForwardRequest(request, target_instance);
            
            // [SEQUENCE: 23] Response middleware (logging, metrics)
            LogRequest(client_ip, path, method, response.result_int());
            metrics_->IncrementCounter("requests_total", {
                {"service", route->target_service},
                {"status", std::to_string(response.result_int())}
            });
            
            return response;
            
        } catch (const std::exception& e) {
            spdlog::error("Gateway error: {}", e.what());
            return CreateErrorResponse(500, "Internal Server Error");
        }
    }
    
private:
    // [SEQUENCE: 24] Route configuration for game services
    void ConfigureRoutes() {
        routes_ = {
            // Authentication routes
            {"/api/auth/.*", "auth-service", {"POST", "GET"}, false, 100},
            
            // Player management routes
            {"/api/players/.*", "player-service", {"GET", "POST", "PUT"}, true, 1000},
            
            // Guild operations
            {"/api/guilds/.*", "guild-service", {"GET", "POST", "PUT", "DELETE"}, true, 500},
            
            // World and game state
            {"/api/world/.*", "world-service", {"GET", "POST"}, true, 2000},
            
            // Real-time game actions (WebSocket upgrade)
            {"/ws/game", "world-service", {"GET"}, true, 10000},
            
            // Analytics and metrics
            {"/api/analytics/.*", "analytics-service", {"GET"}, true, 100}
        };
    }
    
    // [SEQUENCE: 25] JWT authentication middleware
    bool AuthenticateRequest(const boost::beast::http::request<boost::beast::http::string_body>& request) {
        std::string path = std::string(request.target());
        
        // [SEQUENCE: 26] Skip auth for public endpoints
        if (path.starts_with("/api/auth/login") || 
            path.starts_with("/api/auth/register") ||
            path == "/health") {
            return true;
        }
        
        // [SEQUENCE: 27] Extract and validate JWT token
        auto auth_header = request.find("Authorization");
        if (auth_header == request.end()) {
            return false;
        }
        
        std::string auth_value = std::string(auth_header->value());
        if (!auth_value.starts_with("Bearer ")) {
            return false;
        }
        
        std::string token = auth_value.substr(7);  // Remove "Bearer "
        return jwt_validator_->ValidateToken(token);
    }
    
    // [SEQUENCE: 28] Service-to-service communication
    boost::beast::http::response<boost::beast::http::string_body> 
    ForwardRequest(const boost::beast::http::request<boost::beast::http::string_body>& request,
                  const ServiceInstance& target) {
        
        try {
            // [SEQUENCE: 29] Create HTTP client connection
            boost::asio::ip::tcp::resolver resolver(io_context_);
            auto const results = resolver.resolve(target.address, std::to_string(target.port));
            
            boost::beast::tcp_stream stream(io_context_);
            stream.connect(results);
            
            // [SEQUENCE: 30] Forward the request
            boost::beast::http::write(stream, request);
            
            // [SEQUENCE: 31] Read the response
            boost::beast::flat_buffer buffer;
            boost::beast::http::response<boost::beast::http::string_body> response;
            boost::beast::http::read(stream, buffer, response);
            
            // [SEQUENCE: 32] Add gateway headers
            response.set("X-Gateway-Service", target.service_name);
            response.set("X-Gateway-Instance", target.instance_id);
            
            stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both);
            
            return response;
            
        } catch (const std::exception& e) {
            spdlog::error("Request forwarding error: {}", e.what());
            return CreateErrorResponse(502, "Bad Gateway");
        }
    }
};
```

### Circuit Breaker Pattern

**Resilient Service Communication:**
```cpp
// [SEQUENCE: 33] Circuit breaker for service resilience
class CircuitBreaker {
private:
    enum class State {
        CLOSED,    // Normal operation
        OPEN,      // Failing fast
        HALF_OPEN  // Testing recovery
    };
    
    State state_ = State::CLOSED;
    int failure_count_ = 0;
    int success_count_ = 0;
    std::chrono::steady_clock::time_point last_failure_time_;
    
    // Configuration
    int failure_threshold_ = 5;
    int success_threshold_ = 3;
    std::chrono::seconds timeout_ = std::chrono::seconds(60);
    
public:
    // [SEQUENCE: 34] Execute request with circuit breaker protection
    template<typename Func>
    auto Execute(Func&& func) -> decltype(func()) {
        switch (state_) {
            case State::OPEN:
                // [SEQUENCE: 35] Check if we should attempt recovery
                if (std::chrono::steady_clock::now() - last_failure_time_ > timeout_) {
                    state_ = State::HALF_OPEN;
                    success_count_ = 0;
                    spdlog::info("Circuit breaker transitioning to HALF_OPEN");
                } else {
                    throw CircuitBreakerOpenException("Circuit breaker is OPEN");
                }
                break;
                
            case State::HALF_OPEN:
                // [SEQUENCE: 36] Limited requests in half-open state
                break;
                
            case State::CLOSED:
                // [SEQUENCE: 37] Normal operation
                break;
        }
        
        try {
            auto result = func();
            OnSuccess();
            return result;
            
        } catch (const std::exception& e) {
            OnFailure();
            throw;
        }
    }
    
private:
    void OnSuccess() {
        failure_count_ = 0;
        
        if (state_ == State::HALF_OPEN) {
            success_count_++;
            if (success_count_ >= success_threshold_) {
                state_ = State::CLOSED;
                spdlog::info("Circuit breaker transitioned to CLOSED");
            }
        }
    }
    
    void OnFailure() {
        failure_count_++;
        last_failure_time_ = std::chrono::steady_clock::now();
        
        if (state_ == State::HALF_OPEN) {
            state_ = State::OPEN;
            spdlog::warn("Circuit breaker transitioned to OPEN from HALF_OPEN");
        } else if (failure_count_ >= failure_threshold_) {
            state_ = State::OPEN;
            spdlog::warn("Circuit breaker transitioned to OPEN due to failures");
        }
    }
};
```

---

## 🔍 Service Discovery & Configuration

### Consul Integration

**`src/microservices/service_discovery/consul_client.h` - Implementation:**
```cpp
// [SEQUENCE: 38] Consul service discovery client
#include <curl/curl.h>
#include <nlohmann/json.hpp>

class ConsulServiceDiscovery {
private:
    std::string consul_address_;
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl_;
    
    // Service health monitoring
    std::thread health_monitor_thread_;
    std::atomic<bool> should_stop_{false};
    
public:
    ConsulServiceDiscovery(const std::string& consul_address) 
        : consul_address_(consul_address),
          curl_(curl_easy_init(), &curl_easy_cleanup) {
        
        if (!curl_) {
            throw std::runtime_error("Failed to initialize CURL");
        }
        
        // [SEQUENCE: 39] Start health monitoring thread
        health_monitor_thread_ = std::thread(&ConsulServiceDiscovery::HealthMonitorLoop, this);
    }
    
    ~ConsulServiceDiscovery() {
        should_stop_ = true;
        if (health_monitor_thread_.joinable()) {
            health_monitor_thread_.join();
        }
    }
    
    // [SEQUENCE: 40] Register service with Consul
    bool RegisterService(const ServiceRegistration& registration) {
        nlohmann::json service_json = {
            {"ID", registration.service_id},
            {"Name", registration.service_name},
            {"Address", registration.address},
            {"Port", registration.port},
            {"Tags", registration.tags},
            {"Check", {
                {"HTTP", registration.health_check_url},
                {"Interval", "10s"},
                {"Timeout", "3s"},
                {"DeregisterCriticalServiceAfter", "30s"}
            }}
        };
        
        std::string url = fmt::format("{}/v1/agent/service/register", consul_address_);
        std::string response;
        
        // [SEQUENCE: 41] HTTP PUT request to Consul
        curl_easy_setopt(curl_.get(), CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl_.get(), CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl_.get(), CURLOPT_POSTFIELDS, service_json.dump().c_str());
        curl_easy_setopt(curl_.get(), CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl_.get(), CURLOPT_WRITEDATA, &response);
        
        CURLcode res = curl_easy_perform(curl_.get());
        
        if (res == CURLE_OK) {
            long response_code;
            curl_easy_getinfo(curl_.get(), CURLINFO_RESPONSE_CODE, &response_code);
            
            if (response_code == 200) {
                spdlog::info("Service {} registered with Consul", registration.service_name);
                return true;
            }
        }
        
        spdlog::error("Failed to register service with Consul: {}", curl_easy_strerror(res));
        return false;
    }
    
    // [SEQUENCE: 42] Discover healthy service instances
    std::vector<ServiceInstance> GetHealthyInstances(const std::string& service_name) {
        std::string url = fmt::format("{}/v1/health/service/{}?passing=true", 
                                     consul_address_, service_name);
        std::string response;
        
        curl_easy_setopt(curl_.get(), CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl_.get(), CURLOPT_HTTPGET, 1L);
        curl_easy_setopt(curl_.get(), CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl_.get(), CURLOPT_WRITEDATA, &response);
        
        CURLcode res = curl_easy_perform(curl_.get());
        std::vector<ServiceInstance> instances;
        
        if (res == CURLE_OK) {
            try {
                // [SEQUENCE: 43] Parse Consul response
                nlohmann::json consul_response = nlohmann::json::parse(response);
                
                for (const auto& entry : consul_response) {
                    if (entry.contains("Service")) {
                        const auto& service = entry["Service"];
                        
                        ServiceInstance instance;
                        instance.service_name = service["Service"];
                        instance.instance_id = service["ID"];
                        instance.address = service["Address"];
                        instance.port = service["Port"];
                        instance.tags = service["Tags"];
                        
                        instances.push_back(instance);
                    }
                }
                
            } catch (const nlohmann::json::exception& e) {
                spdlog::error("Failed to parse Consul response: {}", e.what());
            }
        }
        
        return instances;
    }
    
    // [SEQUENCE: 44] Configuration management
    std::optional<std::string> GetConfigValue(const std::string& key) {
        std::string url = fmt::format("{}/v1/kv/{}", consul_address_, key);
        std::string response;
        
        curl_easy_setopt(curl_.get(), CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl_.get(), CURLOPT_HTTPGET, 1L);
        curl_easy_setopt(curl_.get(), CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl_.get(), CURLOPT_WRITEDATA, &response);
        
        CURLcode res = curl_easy_perform(curl_.get());
        
        if (res == CURLE_OK) {
            try {
                nlohmann::json kv_response = nlohmann::json::parse(response);
                if (!kv_response.empty()) {
                    std::string encoded_value = kv_response[0]["Value"];
                    // Base64 decode the value (Consul stores values as base64)
                    return Base64Decode(encoded_value);
                }
            } catch (const nlohmann::json::exception& e) {
                spdlog::error("Failed to parse KV response: {}", e.what());
            }
        }
        
        return std::nullopt;
    }
    
private:
    // [SEQUENCE: 45] Health monitoring background thread
    void HealthMonitorLoop() {
        while (!should_stop_) {
            try {
                // Monitor service health and update local cache
                RefreshServiceCache();
                
                std::this_thread::sleep_for(std::chrono::seconds(30));
                
            } catch (const std::exception& e) {
                spdlog::error("Health monitor error: {}", e.what());
            }
        }
    }
    
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* response) {
        size_t total_size = size * nmemb;
        response->append(static_cast<char*>(contents), total_size);
        return total_size;
    }
};
```

---

## 📊 Inter-Service Communication Patterns

### Event-Driven Communication

**Service Event Bus Implementation:**
```cpp
// [SEQUENCE: 46] Event-driven inter-service communication
class ServiceEventBus {
private:
    std::shared_ptr<RabbitMQManager> message_queue_;
    std::unordered_map<std::string, std::vector<EventHandler>> event_handlers_;
    
public:
    // [SEQUENCE: 47] Publish domain events
    template<typename Event>
    bool PublishEvent(const Event& event) {
        nlohmann::json event_json = {
            {"event_type", Event::EVENT_TYPE},
            {"event_id", GenerateEventId()},
            {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()},
            {"service_id", GetServiceId()},
            {"data", event.ToJson()}
        };
        
        std::string routing_key = fmt::format("events.{}.{}", 
                                            GetServiceName(), Event::EVENT_TYPE);
        
        return message_queue_->Publish("service.events", routing_key, event_json.dump());
    }
    
    // [SEQUENCE: 48] Subscribe to events from other services
    template<typename Event>
    void SubscribeToEvent(std::function<void(const Event&)> handler) {
        std::string routing_key = fmt::format("events.*.{}", Event::EVENT_TYPE);
        
        message_queue_->Subscribe("service.events", routing_key, 
            [handler](const std::string& message) {
                try {
                    nlohmann::json event_json = nlohmann::json::parse(message);
                    Event event = Event::FromJson(event_json["data"]);
                    handler(event);
                    
                } catch (const std::exception& e) {
                    spdlog::error("Event handling error: {}", e.what());
                }
            });
    }
};

// [SEQUENCE: 49] Example domain events
struct PlayerLevelUpEvent {
    static constexpr const char* EVENT_TYPE = "player.level_up";
    
    uint64_t player_id;
    int old_level;
    int new_level;
    std::chrono::system_clock::time_point timestamp;
    
    nlohmann::json ToJson() const {
        return {
            {"player_id", player_id},
            {"old_level", old_level},
            {"new_level", new_level},
            {"timestamp", timestamp.time_since_epoch().count()}
        };
    }
    
    static PlayerLevelUpEvent FromJson(const nlohmann::json& json) {
        PlayerLevelUpEvent event;
        event.player_id = json["player_id"];
        event.old_level = json["old_level"];
        event.new_level = json["new_level"];
        event.timestamp = std::chrono::system_clock::time_point(
            std::chrono::nanoseconds(json["timestamp"]));
        return event;
    }
};

struct GuildWarStartedEvent {
    static constexpr const char* EVENT_TYPE = "guild.war_started";
    
    uint32_t guild_1_id;
    uint32_t guild_2_id;
    std::string war_zone;
    std::chrono::system_clock::time_point start_time;
    
    nlohmann::json ToJson() const {
        return {
            {"guild_1_id", guild_1_id},
            {"guild_2_id", guild_2_id},
            {"war_zone", war_zone},
            {"start_time", start_time.time_since_epoch().count()}
        };
    }
};
```

---

## 🔧 Distributed Transaction Management

### Saga Pattern Implementation

**`src/microservices/transactions/saga_orchestrator.h` - Implementation:**
```cpp
// [SEQUENCE: 50] Saga pattern for distributed transactions
class SagaOrchestrator {
public:
    struct SagaStep {
        std::string service_name;
        std::string operation;
        nlohmann::json parameters;
        std::string compensation_operation;  // Rollback operation
        bool completed = false;
    };
    
    struct SagaTransaction {
        std::string saga_id;
        std::vector<SagaStep> steps;
        size_t current_step = 0;
        bool failed = false;
        std::string failure_reason;
    };
    
private:
    std::unordered_map<std::string, SagaTransaction> active_sagas_;
    std::shared_ptr<ServiceEventBus> event_bus_;
    std::mutex sagas_mutex_;
    
public:
    // [SEQUENCE: 51] Start a distributed transaction saga
    std::string StartSaga(const std::vector<SagaStep>& steps) {
        std::string saga_id = GenerateSagaId();
        
        SagaTransaction saga;
        saga.saga_id = saga_id;
        saga.steps = steps;
        
        {
            std::lock_guard<std::mutex> lock(sagas_mutex_);
            active_sagas_[saga_id] = saga;
        }
        
        // [SEQUENCE: 52] Execute first step
        ExecuteNextStep(saga_id);
        
        return saga_id;
    }
    
    // [SEQUENCE: 53] Handle step completion events
    void OnStepCompleted(const std::string& saga_id, bool success, const std::string& error = "") {
        std::lock_guard<std::mutex> lock(sagas_mutex_);
        
        auto it = active_sagas_.find(saga_id);
        if (it == active_sagas_.end()) {
            return;
        }
        
        SagaTransaction& saga = it->second;
        
        if (success) {
            // [SEQUENCE: 54] Mark current step as completed
            saga.steps[saga.current_step].completed = true;
            saga.current_step++;
            
            // [SEQUENCE: 55] Check if saga is complete
            if (saga.current_step >= saga.steps.size()) {
                spdlog::info("Saga {} completed successfully", saga_id);
                active_sagas_.erase(it);
                
                // Publish saga completion event
                PublishSagaCompletedEvent(saga_id);
            } else {
                // Execute next step
                ExecuteNextStep(saga_id);
            }
            
        } else {
            // [SEQUENCE: 56] Step failed, start compensation
            saga.failed = true;
            saga.failure_reason = error;
            
            spdlog::warn("Saga {} step {} failed: {}", saga_id, saga.current_step, error);
            StartCompensation(saga_id);
        }
    }
    
private:
    // [SEQUENCE: 57] Execute the next step in the saga
    void ExecuteNextStep(const std::string& saga_id) {
        auto it = active_sagas_.find(saga_id);
        if (it == active_sagas_.end()) {
            return;
        }
        
        const SagaTransaction& saga = it->second;
        const SagaStep& step = saga.steps[saga.current_step];
        
        // [SEQUENCE: 58] Send operation request to target service
        nlohmann::json request = {
            {"saga_id", saga_id},
            {"operation", step.operation},
            {"parameters", step.parameters}
        };
        
        std::string routing_key = fmt::format("saga.{}.request", step.service_name);
        event_bus_->Publish("saga.operations", routing_key, request.dump());
        
        spdlog::info("Saga {} executing step {}: {} on {}", 
                    saga_id, saga.current_step, step.operation, step.service_name);
    }
    
    // [SEQUENCE: 59] Execute compensation (rollback) operations
    void StartCompensation(const std::string& saga_id) {
        auto it = active_sagas_.find(saga_id);
        if (it == active_sagas_.end()) {
            return;
        }
        
        SagaTransaction& saga = it->second;
        
        // [SEQUENCE: 60] Compensate completed steps in reverse order
        for (int i = saga.current_step - 1; i >= 0; i--) {
            const SagaStep& step = saga.steps[i];
            
            if (step.completed && !step.compensation_operation.empty()) {
                nlohmann::json compensation_request = {
                    {"saga_id", saga_id},
                    {"operation", step.compensation_operation},
                    {"parameters", step.parameters}
                };
                
                std::string routing_key = fmt::format("saga.{}.compensation", step.service_name);
                event_bus_->Publish("saga.operations", routing_key, compensation_request.dump());
                
                spdlog::info("Saga {} compensating step {}: {} on {}", 
                            saga_id, i, step.compensation_operation, step.service_name);
            }
        }
        
        // Remove failed saga
        active_sagas_.erase(it);
        PublishSagaFailedEvent(saga_id);
    }
};

// [SEQUENCE: 61] Example: Player guild transfer saga
class PlayerGuildTransferSaga {
public:
    static std::vector<SagaOrchestrator::SagaStep> CreateTransferSteps(
        uint64_t player_id, uint32_t from_guild_id, uint32_t to_guild_id) {
        
        return {
            // Step 1: Validate player can leave current guild
            {
                .service_name = "guild-service",
                .operation = "validate_leave",
                .parameters = {{"player_id", player_id}, {"guild_id", from_guild_id}},
                .compensation_operation = ""  // No compensation needed for validation
            },
            
            // Step 2: Validate player can join target guild
            {
                .service_name = "guild-service", 
                .operation = "validate_join",
                .parameters = {{"player_id", player_id}, {"guild_id", to_guild_id}},
                .compensation_operation = ""
            },
            
            // Step 3: Remove player from current guild
            {
                .service_name = "guild-service",
                .operation = "remove_member",
                .parameters = {{"player_id", player_id}, {"guild_id", from_guild_id}},
                .compensation_operation = "add_member"  // Re-add if later steps fail
            },
            
            // Step 4: Add player to new guild
            {
                .service_name = "guild-service",
                .operation = "add_member", 
                .parameters = {{"player_id", player_id}, {"guild_id", to_guild_id}},
                .compensation_operation = "remove_member"
            },
            
            // Step 5: Update player record
            {
                .service_name = "player-service",
                .operation = "update_guild",
                .parameters = {{"player_id", player_id}, {"guild_id", to_guild_id}},
                .compensation_operation = "update_guild"  // Restore old guild ID
            }
        };
    }
};
```

---

## 📊 Performance Monitoring & Observability

### Distributed Tracing

**OpenTelemetry Integration:**
```cpp
// [SEQUENCE: 62] Distributed tracing for microservices
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/exporters/jaeger/jaeger_exporter.h>

class DistributedTracing {
private:
    std::shared_ptr<opentelemetry::trace::Tracer> tracer_;
    
public:
    DistributedTracing(const std::string& service_name) {
        // [SEQUENCE: 63] Initialize OpenTelemetry with Jaeger
        auto exporter = std::make_unique<opentelemetry::exporter::jaeger::JaegerExporter>(
            opentelemetry::exporter::jaeger::JaegerExporterOptions{
                .endpoint = "http://jaeger:14268/api/traces"
            }
        );
        
        auto processor = std::make_shared<opentelemetry::sdk::trace::SimpleSpanProcessor>(
            std::move(exporter)
        );
        
        auto provider = std::make_shared<opentelemetry::sdk::trace::TracerProvider>(processor);
        opentelemetry::trace::Provider::SetTracerProvider(provider);
        
        tracer_ = provider->GetTracer(service_name);
    }
    
    // [SEQUENCE: 64] Trace inter-service calls
    template<typename Func>
    auto TraceServiceCall(const std::string& operation_name, 
                         const std::string& target_service,
                         Func&& func) -> decltype(func()) {
        
        auto span = tracer_->StartSpan(operation_name);
        span->SetAttribute("service.name", target_service);
        span->SetAttribute("span.kind", "client");
        
        try {
            auto result = func();
            span->SetStatus(opentelemetry::trace::StatusCode::kOk);
            return result;
            
        } catch (const std::exception& e) {
            span->SetStatus(opentelemetry::trace::StatusCode::kError, e.what());
            span->SetAttribute("error", true);
            span->SetAttribute("error.message", e.what());
            throw;
            
        } finally {
            span->End();
        }
    }
};
```

---

## 📈 Migration Strategy

### Strangler Fig Pattern

**Gradual Migration from Monolith:**
```cpp
// [SEQUENCE: 65] Strangler fig pattern for gradual migration
class MigrationRouter {
private:
    enum class RoutingStrategy {
        MONOLITH_ONLY,      // Route to legacy monolith
        MICROSERVICE_ONLY,  // Route to new microservice
        CANARY,            // Route percentage to microservice
        FEATURE_FLAG       // Route based on feature flags
    };
    
    struct RoutingRule {
        std::string path_pattern;
        RoutingStrategy strategy;
        int microservice_percentage = 0;  // For canary releases
        std::string feature_flag;         // For feature flag routing
    };
    
    std::vector<RoutingRule> routing_rules_;
    std::shared_ptr<FeatureFlagService> feature_flags_;
    
public:
    // [SEQUENCE: 66] Migration phases
    void ConfigureMigrationPhase(MigrationPhase phase) {
        switch (phase) {
            case MigrationPhase::PHASE_1_AUTH_EXTRACTION:
                routing_rules_ = {
                    {"/api/auth/.*", RoutingStrategy::MICROSERVICE_ONLY},
                    {"/api/.*", RoutingStrategy::MONOLITH_ONLY}
                };
                break;
                
            case MigrationPhase::PHASE_2_PLAYER_SERVICE:
                routing_rules_ = {
                    {"/api/auth/.*", RoutingStrategy::MICROSERVICE_ONLY},
                    {"/api/players/.*", RoutingStrategy::CANARY, .microservice_percentage = 10},
                    {"/api/.*", RoutingStrategy::MONOLITH_ONLY}
                };
                break;
                
            case MigrationPhase::PHASE_3_GUILD_SERVICE:
                routing_rules_ = {
                    {"/api/auth/.*", RoutingStrategy::MICROSERVICE_ONLY},
                    {"/api/players/.*", RoutingStrategy::MICROSERVICE_ONLY},
                    {"/api/guilds/.*", RoutingStrategy::FEATURE_FLAG, .feature_flag = "enable_guild_service"},
                    {"/api/.*", RoutingStrategy::MONOLITH_ONLY}
                };
                break;
        }
    }
};
```

---

## 🎯 Best Practices Summary

### 1. Service Boundaries
```cpp
// ✅ Good: Clear domain boundaries
class PlayerService {  // Owns player data and operations
    void UpdatePlayerStats();
    void ManageInventory();
};

class GuildService {   // Owns guild data and operations  
    void ManageGuildMembers();
    void HandleGuildWars();
};

// ❌ Avoid: Mixed responsibilities
class GameService {    // Too broad, unclear boundaries
    void UpdatePlayer();
    void HandleGuild();
    void ProcessWorld();  // Should be separate services
};
```

### 2. Data Consistency
```cpp
// ✅ Good: Eventual consistency with compensation
void TransferPlayer(uint64_t player_id, uint32_t to_guild_id) {
    auto saga_steps = PlayerGuildTransferSaga::CreateTransferSteps(player_id, to_guild_id);
    saga_orchestrator_->StartSaga(saga_steps);  // Handles rollback if needed
}

// ❌ Avoid: Distributed ACID transactions
void TransferPlayerBad(uint64_t player_id, uint32_t to_guild_id) {
    // This will be slow and fragile across services
    auto transaction = distributed_transaction_manager_->BeginTransaction();
    // ... multiple service calls in single transaction
}
```

### 3. Communication Patterns
```cpp
// ✅ Good: Async events for non-critical operations
void OnPlayerLevelUp(uint64_t player_id, int new_level) {
    // Synchronous: Update player record
    player_repository_->UpdateLevel(player_id, new_level);
    
    // Asynchronous: Notify other services
    event_bus_->PublishEvent(PlayerLevelUpEvent{player_id, new_level});
}

// ❌ Avoid: Synchronous calls for non-critical operations
void OnPlayerLevelUpBad(uint64_t player_id, int new_level) {
    player_repository_->UpdateLevel(player_id, new_level);
    achievement_service_->CheckAchievements(player_id);  // Blocking call
    analytics_service_->RecordLevelUp(player_id);        // Blocking call
    // If any service is down, entire operation fails
}
```

---

## 🔗 Integration Points

This guide builds upon:
- **message_queue_systems.md**: Event-driven communication between services
- **nosql_integration_guide.md**: Database per service pattern
- **advanced_cpp_features.md**: Modern C++ patterns for service implementation

**Next Implementation Steps:**
1. Extract Authentication Service first (lowest risk)
2. Implement API Gateway with basic routing
3. Add Circuit Breakers for resilience
4. Gradually migrate other services using Strangler Fig pattern

---

*💡 Key Insight: Microservices aren't just about breaking up code - they're about organizational scalability. Each service should be owned by a small team (2-8 people) and have clear business boundaries. Start with the monolith, identify natural service boundaries, and extract services incrementally rather than attempting a big-bang rewrite.*

---

## 🔥 흔한 실수와 해결방법

### 1. 너무 작은 서비스 분할

#### [SEQUENCE: 1] 모든 기능을 마이크로서비스로
```cpp
// ❌ 잘못된 예: 너무 세분화된 서비스
class HealthPotionService { // 단일 기능 서비스
    void UseHealthPotion(uint64_t player_id);
};

class ManaPotionService {   // 또 다른 단일 기능
    void UseManaPotion(uint64_t player_id);
};
// 관리 복잡도 폭발!

// ✅ 올바른 예: 도메인 경계로 분할
class InventoryService {    // 인벤토리 관련 모든 기능
    void UseItem(uint64_t player_id, uint32_t item_id);
    void AddItem(uint64_t player_id, uint32_t item_id, int quantity);
    void RemoveItem(uint64_t player_id, uint32_t item_id, int quantity);
};
```

### 2. 분산 트랜잭션 오용

#### [SEQUENCE: 2] 모든 작업에 분산 트랜잭션 적용
```cpp
// ❌ 잘못된 예: 단순 조회에도 트랜잭션
void GetPlayerInfo(uint64_t player_id) {
    auto transaction = StartDistributedTransaction();
    auto player = player_service->GetPlayer(player_id);
    auto guild = guild_service->GetGuild(player.guild_id);
    transaction->Commit();  // 불필요한 오버헤드
}

// ✅ 올바른 예: 필요한 경우에만 Saga 패턴
void GetPlayerInfo(uint64_t player_id) {
    // 단순 조회는 비동기로
    auto player = player_service->GetPlayer(player_id);
    auto guild = guild_service->GetGuild(player.guild_id);
}

void TransferGuildLeadership(uint64_t from_id, uint64_t to_id) {
    // 중요한 작업은 Saga로
    saga_orchestrator->StartSaga(GuildLeadershipTransferSaga::Create(from_id, to_id));
}
```

### 3. 서비스 간 동기 호출 남용

#### [SEQUENCE: 3] 모든 통신을 동기 HTTP로
```cpp
// ❌ 잘못된 예: 체인 동기 호출
void ProcessPlayerAction(uint64_t player_id) {
    auto player = player_service->GetPlayer(player_id);  // HTTP 동기
    auto stats = stats_service->GetStats(player_id);    // HTTP 동기
    auto items = inventory_service->GetItems(player_id); // HTTP 동기
    // 총 300ms+ 레이턴시!
}

// ✅ 올바른 예: 비동기 + 이벤트 기반
void ProcessPlayerAction(uint64_t player_id) {
    // 필수 데이터만 동기
    auto player = player_service->GetPlayer(player_id);
    
    // 나머지는 이벤트로
    event_bus->PublishEvent(PlayerActionEvent{player_id, action});
    // 다른 서비스가 비동기로 처리
}
```

## 실습 프로젝트

### 프로젝트 1: 인증 서비스 추출 (기초)
**목표**: 모놀리스에서 인증 로직 분리

**구현 사항**:
- JWT 기반 인증
- 세션 관리
- 사용자 등록/로그인
- API Gateway 통합

**학습 포인트**:
- 서비스 경계 설정
- RESTful API 설계
- 상태비저장 설계

### 프로젝트 2: API Gateway 구현 (중급)
**목표**: 마이크로서비스 진입점 구축

**구현 사항**:
- 요청 라우팅
- 로드 밸런싱
- 인증/인가
- 속도 제한

**학습 포인트**:
- Boost.Beast HTTP
- 서비스 디스커버리
- Circuit Breaker

### 프로젝트 3: 분산 트랜잭션 (고급)
**목표**: Saga 패턴 구현

**구현 사항**:
- Saga Orchestrator
- 보상 트랜잭션
- 이벤트 소싱
- 상태 추적

**학습 포인트**:
- 분산 트랜잭션 이론
- 최종 일관성
- 이벤트 기반 보상

## 학습 체크리스트

### 기초 레벨 ✅
- [ ] 모놀리스 vs 마이크로서비스
- [ ] RESTful API 설계 원칙
- [ ] 서비스 경계 정의
- [ ] API Gateway 개념

### 중급 레벨 📚
- [ ] 서비스 디스커버리 (Consul)
- [ ] 로드 밸런싱 전략
- [ ] Circuit Breaker 패턴
- [ ] 비동기 통신

### 고급 레벨 🚀
- [ ] Saga 패턴
- [ ] CQRS 패턴
- [ ] 이벤트 소싱
- [ ] 분산 추적

### 전문가 레벨 🏆
- [ ] Service Mesh (Istio)
- [ ] 카오스 엔지니어링
- [ ] Multi-tenancy
- [ ] Edge Computing

## 추가 학습 자료

### 추천 도서
- "Building Microservices" - Sam Newman
- "Microservices Patterns" - Chris Richardson
- "Release It!" - Michael Nygard
- "Domain-Driven Design" - Eric Evans

### 온라인 리소스
- [Microservices.io](https://microservices.io/)
- [Martin Fowler's Microservices](https://martinfowler.com/articles/microservices.html)
- [12 Factor App](https://12factor.net/)
- [CNCF Cloud Native Trail Map](https://github.com/cncf/trailmap)

### 실습 도구
- Docker & Docker Compose
- Kubernetes (Minikube/K3s)
- Consul/etcd (서비스 디스커버리)
- Jaeger (분산 추적)

### 테스트 도구
```bash
# 서비스 테스트
curl -X GET http://localhost:8080/api/players/123

# 로드 테스트
ab -n 10000 -c 100 http://localhost:8080/api/health

# 회로 차단기 테스트
for i in {1..20}; do curl -X GET http://localhost:8080/api/fail; done
```

---

*🚀 마이크로서비스는 은탄환이 아닙니다. 모놀리스가 적합한 경우도 많습니다. 팀의 크기, 서비스의 복잡도, 독립적인 배포 필요성을 고려하여 아키텍처를 선택하세요. 시작은 단순하게, 필요에 따라 진화시키세요!*