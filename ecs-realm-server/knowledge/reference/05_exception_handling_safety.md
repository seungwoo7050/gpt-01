# C++ 예외 처리와 안전성 - 게임 서버의 안정성 보장
*robust한 게임 서버를 위한 예외 처리 완전 가이드*

> **🎯 목표**: C++ 예외 처리 메커니즘을 완전히 이해하고, 게임 서버의 안정성과 신뢰성을 보장하는 견고한 에러 처리 시스템 구축

---

## 📋 문서 정보

- **난이도**: 🟡 중급 → 🔴 고급
- **예상 학습 시간**: 5-7시간
- **필요 선수 지식**: 
  - ✅ C++ 기본 문법과 클래스
  - ✅ RAII 개념 이해
  - ✅ 스마트 포인터 사용 경험
- **실습 환경**: C++17 이상 (C++20 권장)

---

## 🤔 왜 예외 처리가 게임 서버에 중요할까?

### **게임 서버의 치명적인 문제들**

```cpp
// 😰 예외 처리 없는 위험한 코드들
class UnsafeGameServer {
public:
    void ProcessPlayerLogin(const std::string& username, const std::string& password) {
        // 위험 1: 네트워크 연결 실패 시 크래시
        DatabaseConnection* db = new DatabaseConnection("localhost");  // 예외 발생 가능
        
        // 위험 2: 메모리 부족 시 크래시
        std::vector<Player> players(1000000);  // bad_alloc 예외 가능
        
        // 위험 3: 잘못된 데이터로 크래시
        Player player = db->GetPlayer(username);  // 플레이어 없으면 예외
        
        // 위험 4: 파일 I/O 실패 시 크래시
        std::ifstream config("server.cfg");  // 파일 없으면 실패
        config >> server_port;  // 실패해도 계속 진행
        
        delete db;  // 위의 어떤 곳에서든 예외 발생하면 메모리 누수!
    }
    
    // 위험 5: 전체 서버 다운
    void HandlePacket(const PacketData& packet) {
        ProcessCombat(packet);     // 여기서 예외 → 서버 종료
        ProcessMovement(packet);   // 실행 안됨
        ProcessChat(packet);       // 실행 안됨
    }
};

// 현실에서 일어나는 문제들:
// 1. 하나의 플레이어 문제로 전체 서버가 다운됨
// 2. 메모리 누수로 서버가 점진적으로 불안정해짐  
// 3. 에러 원인을 파악할 수 없음
// 4. 복구 불가능한 상태가 됨
```

### **예외 처리로 해결된 안전한 코드**

```cpp
// ✨ 예외 안전한 게임 서버 코드
class SafeGameServer {
public:
    bool ProcessPlayerLogin(const std::string& username, const std::string& password) noexcept {
        try {
            // RAII로 자동 리소스 정리
            auto db = std::make_unique<DatabaseConnection>("localhost");
            
            // 메모리 할당 실패 대비
            std::vector<Player> players;
            players.reserve(1000000);  // 미리 용량 확보
            
            // 안전한 플레이어 조회
            auto player_opt = db->TryGetPlayer(username);
            if (!player_opt) {
                LogWarning("Player not found: {}", username);
                return false;
            }
            
            // 안전한 파일 처리
            if (auto config = LoadConfig("server.cfg")) {
                server_port = config->GetPort();
            }
            
            LogInfo("Player {} logged in successfully", username);
            return true;
            
        } catch (const DatabaseException& e) {
            LogError("Database error during login: {}", e.what());
            return false;
        } catch (const std::bad_alloc& e) {
            LogError("Memory allocation failed during login: {}", e.what());
            return false;
        } catch (const std::exception& e) {
            LogError("Unexpected error during login: {}", e.what());
            return false;
        }
        // RAII로 모든 리소스 자동 정리됨
    }
    
    void HandlePacket(const PacketData& packet) noexcept {
        // 각각의 처리가 독립적으로 안전함
        try {
            ProcessCombat(packet);
        } catch (const std::exception& e) {
            LogError("Combat processing failed: {}", e.what());
        }
        
        try {
            ProcessMovement(packet);  // 앞의 실패와 무관하게 실행
        } catch (const std::exception& e) {
            LogError("Movement processing failed: {}", e.what());
        }
        
        try {
            ProcessChat(packet);  // 독립적으로 처리
        } catch (const std::exception& e) {
            LogError("Chat processing failed: {}", e.what());
        }
    }
};

// ✅ 장점:
// 1. 한 플레이어 문제가 다른 플레이어에게 영향 없음
// 2. 메모리 누수 방지
// 3. 상세한 에러 로그로 디버깅 용이
// 4. 서버가 계속 운영됨
```

---

## 📚 1. C++ 예외 처리 기초

### **1.1 예외 처리 기본 문법**

```cpp
#include <stdexcept>
#include <iostream>
#include <memory>

// 🎮 게임 서버 기본 예외 처리 패턴

class GameServerBasics {
public:
    // 기본 try-catch 구조
    void BasicExceptionHandling() {
        try {
            // 위험한 작업들
            ProcessPlayerData();
            ConnectToDatabase();
            LoadGameWorld();
            
        } catch (const std::runtime_error& e) {
            // 런타임 에러 처리
            std::cerr << "Runtime error: " << e.what() << std::endl;
        } catch (const std::invalid_argument& e) {
            // 잘못된 인수 에러
            std::cerr << "Invalid argument: " << e.what() << std::endl;
        } catch (const std::exception& e) {
            // 모든 표준 예외의 베이스 클래스
            std::cerr << "Standard exception: " << e.what() << std::endl;
        } catch (...) {
            // 모든 예외 (표준이 아닌 것도 포함)
            std::cerr << "Unknown exception occurred!" << std::endl;
        }
    }
    
    // 함수에서 예외 던지기
    void ValidatePlayer(const std::string& username) {
        if (username.empty()) {
            throw std::invalid_argument("Username cannot be empty");
        }
        
        if (username.length() > 20) {
            throw std::invalid_argument("Username too long (max 20 characters)");
        }
        
        if (IsUsernameBanned(username)) {
            throw std::runtime_error("Username is banned");
        }
        
        // 모든 검증 통과
        std::cout << "Player validation successful: " << username << std::endl;
    }
    
    // 안전한 함수 호출 패턴
    bool SafePlayerValidation(const std::string& username) noexcept {
        try {
            ValidatePlayer(username);
            return true;
        } catch (const std::invalid_argument& e) {
            LogWarning("Validation failed: {}", e.what());
            return false;
        } catch (const std::runtime_error& e) {
            LogError("Runtime error during validation: {}", e.what());
            return false;
        }
    }

private:
    void ProcessPlayerData() { /* 구현 */ }
    void ConnectToDatabase() { /* 구현 */ }
    void LoadGameWorld() { /* 구현 */ }
    bool IsUsernameBanned(const std::string& username) { return false; }
    
    void LogWarning(const std::string& format, const std::string& message) {
        std::cout << "[WARNING] " << message << std::endl;
    }
    
    void LogError(const std::string& format, const std::string& message) {
        std::cout << "[ERROR] " << message << std::endl;
    }
};
```

### **1.2 표준 예외 클래스 체계**

```cpp
#include <stdexcept>
#include <system_error>

// 🔍 C++ 표준 예외 클래스 활용

class StandardExceptionUsage {
public:
    void DemonstrateStandardExceptions() {
        try {
            // 1. std::logic_error 계열 (프로그램 로직 오류)
            ThrowLogicErrors();
            
        } catch (const std::invalid_argument& e) {
            std::cout << "Invalid argument: " << e.what() << std::endl;
        } catch (const std::out_of_range& e) {
            std::cout << "Out of range: " << e.what() << std::endl;
        } catch (const std::logic_error& e) {
            std::cout << "Logic error: " << e.what() << std::endl;
        }
        
        try {
            // 2. std::runtime_error 계열 (런타임 환경 문제)
            ThrowRuntimeErrors();
            
        } catch (const std::bad_alloc& e) {
            std::cout << "Memory allocation failed: " << e.what() << std::endl;
        } catch (const std::system_error& e) {
            std::cout << "System error: " << e.what() 
                     << " (code: " << e.code() << ")" << std::endl;
        } catch (const std::runtime_error& e) {
            std::cout << "Runtime error: " << e.what() << std::endl;
        }
    }
    
private:
    void ThrowLogicErrors() {
        std::vector<int> vec{1, 2, 3};
        
        // invalid_argument 예제
        int negative_size = -5;
        if (negative_size < 0) {
            throw std::invalid_argument("Size cannot be negative");
        }
        
        // out_of_range 예제  
        try {
            int value = vec.at(10);  // 범위 초과
        } catch (const std::out_of_range&) {
            throw std::out_of_range("Vector index out of range");
        }
    }
    
    void ThrowRuntimeErrors() {
        // 파일 열기 실패 시뮬레이션
        throw std::runtime_error("Cannot open configuration file");
        
        // 메모리 부족 시뮬레이션 (실제로는 new가 던짐)
        // throw std::bad_alloc();
        
        // 시스템 에러 시뮬레이션
        throw std::system_error(std::make_error_code(std::errc::connection_refused), 
                               "Database connection failed");
    }
};
```

---

## 📚 2. 사용자 정의 예외 클래스

### **2.1 게임 서버 전용 예외 클래스**

```cpp
#include <stdexcept>
#include <string>
#include <chrono>

// 🎮 게임 서버 전용 예외 클래스 설계

// 베이스 게임 예외 클래스
class GameException : public std::runtime_error {
protected:
    std::string component_;
    std::chrono::system_clock::time_point timestamp_;
    
public:
    GameException(const std::string& component, const std::string& message)
        : std::runtime_error(message)
        , component_(component)
        , timestamp_(std::chrono::system_clock::now()) {
    }
    
    const std::string& GetComponent() const noexcept { return component_; }
    
    std::chrono::system_clock::time_point GetTimestamp() const noexcept { 
        return timestamp_; 
    }
    
    virtual std::string GetFullMessage() const {
        auto time_t = std::chrono::system_clock::to_time_t(timestamp_);
        return "[" + component_ + "] " + what() + 
               " (Time: " + std::ctime(&time_t) + ")";
    }
};

// 플레이어 관련 예외들
class PlayerException : public GameException {
public:
    PlayerException(const std::string& message) 
        : GameException("Player", message) {}
};

class PlayerNotFoundException : public PlayerException {
private:
    uint21_t player_id_;
    
public:
    PlayerNotFoundException(uint21_t player_id)
        : PlayerException("Player with ID " + std::to_string(player_id) + " not found")
        , player_id_(player_id) {}
    
    uint21_t GetPlayerId() const noexcept { return player_id_; }
};

class PlayerBannedException : public PlayerException {
private:
    std::string ban_reason_;
    std::chrono::system_clock::time_point ban_expires_;
    
public:
    PlayerBannedException(const std::string& username, 
                         const std::string& reason,
                         std::chrono::system_clock::time_point expires)
        : PlayerException("Player " + username + " is banned: " + reason)
        , ban_reason_(reason)
        , ban_expires_(expires) {}
    
    const std::string& GetBanReason() const noexcept { return ban_reason_; }
    std::chrono::system_clock::time_point GetBanExpiration() const noexcept { 
        return ban_expires_; 
    }
};

// 네트워크 관련 예외들
class NetworkException : public GameException {
public:
    NetworkException(const std::string& message) 
        : GameException("Network", message) {}
};

class ConnectionFailedException : public NetworkException {
private:
    std::string host_;
    int port_;
    
public:
    ConnectionFailedException(const std::string& host, int port)
        : NetworkException("Failed to connect to " + host + ":" + std::to_string(port))
        , host_(host), port_(port) {}
    
    const std::string& GetHost() const noexcept { return host_; }
    int GetPort() const noexcept { return port_; }
};

class PacketCorruptedException : public NetworkException {
private:
    size_t packet_size_;
    std::string packet_type_;
    
public:
    PacketCorruptedException(const std::string& packet_type, size_t size)
        : NetworkException("Corrupted " + packet_type + " packet (size: " + std::to_string(size) + ")")
        , packet_type_(packet_type), packet_size_(size) {}
    
    const std::string& GetPacketType() const noexcept { return packet_type_; }
    size_t GetPacketSize() const noexcept { return packet_size_; }
};

// 데이터베이스 관련 예외들
class DatabaseException : public GameException {
private:
    int error_code_;
    
public:
    DatabaseException(const std::string& message, int error_code = 0)
        : GameException("Database", message), error_code_(error_code) {}
    
    int GetErrorCode() const noexcept { return error_code_; }
};

class DatabaseConnectionLostException : public DatabaseException {
private:
    std::chrono::seconds downtime_;
    
public:
    DatabaseConnectionLostException(std::chrono::seconds downtime)
        : DatabaseException("Database connection lost")
        , downtime_(downtime) {}
    
    std::chrono::seconds GetDowntime() const noexcept { return downtime_; }
};
```

### **2.2 예외 계층 구조 활용**

```cpp
// 🎯 게임 시스템에서의 예외 활용 예제

class GameSystemExceptionDemo {
public:
    void PlayerManagementDemo() {
        try {
            // 플레이어 로그인 시도
            LoginPlayer(12345, "banned_user", "password123");
            
        } catch (const PlayerBannedException& e) {
            // 밴된 플레이어 특별 처리
            auto now = std::chrono::system_clock::now();
            if (e.GetBanExpiration() > now) {
                auto remaining = std::chrono::duration_cast<std::chrono::hours>(
                    e.GetBanExpiration() - now);
                
                SendMessageToClient("Ban expires in " + std::to_string(remaining.count()) + " hours");
                LogPlayerBanAttempt(e.GetFullMessage());
            }
            
        } catch (const PlayerNotFoundException& e) {
            // 존재하지 않는 플레이어
            SendMessageToClient("Invalid credentials");
            LogSuspiciousActivity("Login attempt with non-existent player ID: " + 
                                std::to_string(e.GetPlayerId()));
            
        } catch (const PlayerException& e) {
            // 기타 플레이어 관련 에러
            SendMessageToClient("Player error occurred");
            LogError(e.GetFullMessage());
            
        } catch (const GameException& e) {
            // 모든 게임 관련 에러
            LogError("Game system error in component {}: {}", 
                    e.GetComponent(), e.what());
        }
    }
    
    void NetworkHandlingDemo() {
        try {
            ProcessIncomingPacket();
            
        } catch (const PacketCorruptedException& e) {
            // 패킷 손상 특별 처리
            LogWarning("Corrupted {} packet detected (size: {})", 
                      e.GetPacketType(), e.GetPacketSize());
            
            // 클라이언트에게 재전송 요청
            RequestPacketResend(e.GetPacketType());
            
        } catch (const ConnectionFailedException& e) {
            // 연결 실패 시 재시도
            LogWarning("Connection failed to {}:{}", e.GetHost(), e.GetPort());
            
            // 백업 서버로 연결 시도
            TryConnectToBackupServer();
            
        } catch (const NetworkException& e) {
            // 기타 네트워크 에러
            LogError("Network error: {}", e.what());
            CloseConnection();
        }
    }
    
    void DatabaseOperationDemo() {
        try {
            SavePlayerData(GetCurrentPlayer());
            
        } catch (const DatabaseConnectionLostException& e) {
            // DB 연결 끊김 - 특별한 복구 로직
            LogCritical("Database connection lost for {} seconds", 
                       e.GetDowntime().count());
            
            // 메모리에 임시 저장
            CachePlayerDataInMemory(GetCurrentPlayer());
            
            // 연결 복구 시도
            ScheduleDatabaseReconnection();
            
        } catch (const DatabaseException& e) {
            // 기타 DB 에러
            if (e.GetErrorCode() == 1062) {  // Duplicate entry
                LogWarning("Duplicate data entry attempt: {}", e.what());
            } else {
                LogError("Database error (code {}): {}", e.GetErrorCode(), e.what());
            }
        }
    }

private:
    void LoginPlayer(uint21_t id, const std::string& username, const std::string& password) {
        if (username == "banned_user") {
            throw PlayerBannedException(username, "Cheating", 
                std::chrono::system_clock::now() + std::chrono::hours(24));
        }
        if (id == 12345) {
            throw PlayerNotFoundException(id);
        }
    }
    
    void ProcessIncomingPacket() {
        // 패킷 처리 로직
        throw PacketCorruptedException("LOGIN", 256);
    }
    
    void SavePlayerData(const std::string& player) {
        // DB 저장 로직
        throw DatabaseConnectionLostException(std::chrono::seconds(30));
    }
    
    // 헬퍼 메서드들
    void SendMessageToClient(const std::string& msg) { std::cout << "To client: " << msg << std::endl; }
    void LogPlayerBanAttempt(const std::string& msg) { std::cout << "[BAN] " << msg << std::endl; }
    void LogSuspiciousActivity(const std::string& msg) { std::cout << "[SUSPICIOUS] " << msg << std::endl; }
    void LogError(const std::string& msg) { std::cout << "[ERROR] " << msg << std::endl; }
    void LogWarning(const std::string& msg) { std::cout << "[WARNING] " << msg << std::endl; }
    void LogCritical(const std::string& msg) { std::cout << "[CRITICAL] " << msg << std::endl; }
    void RequestPacketResend(const std::string& type) { std::cout << "Requesting resend: " << type << std::endl; }
    void TryConnectToBackupServer() { std::cout << "Connecting to backup server..." << std::endl; }
    void CloseConnection() { std::cout << "Connection closed" << std::endl; }
    void CachePlayerDataInMemory(const std::string& player) { std::cout << "Cached: " << player << std::endl; }
    void ScheduleDatabaseReconnection() { std::cout << "DB reconnection scheduled" << std::endl; }
    std::string GetCurrentPlayer() { return "TestPlayer"; }
};
```

---

## 📚 3. RAII와 예외 안전성

### **3.1 RAII (Resource Acquisition Is Initialization)**

```cpp
#include <memory>
#include <fstream>
#include <mutex>
#include <vector>

// 🛡️ RAII를 통한 예외 안전한 리소스 관리

class RAIIExampleInGameServer {
public:
    // ❌ 예외 불안전한 코드
    void UnsafeResourceManagement() {
        FILE* file = fopen("game_data.bin", "rb");
        int* large_buffer = new int[1000000];
        std::mutex* player_mutex = new std::mutex;
        
        // 여기서 예외 발생하면?
        ProcessGameData(file, large_buffer);  // 예외 가능
        
        // 리소스 정리가 실행되지 않을 수 있음!
        fclose(file);
        delete[] large_buffer;
        delete player_mutex;
    }
    
    // ✅ RAII로 안전한 리소스 관리
    void SafeResourceManagement() {
        // 1. 파일을 RAII로 관리
        auto file = std::unique_ptr<FILE, decltype(&fclose)>(
            fopen("game_data.bin", "rb"), &fclose);
        
        if (!file) {
            throw std::runtime_error("Cannot open game data file");
        }
        
        // 2. 메모리를 스마트 포인터로 관리
        auto large_buffer = std::make_unique<int[]>(1000000);
        
        // 3. 뮤텍스를 스택 객체로 관리
        std::mutex player_mutex;
        
        try {
            // 여기서 예외가 발생해도 모든 리소스가 자동으로 정리됨
            ProcessGameData(file.get(), large_buffer.get());
        } catch (const std::exception& e) {
            // 예외 발생 시에도 RAII에 의해 모든 리소스가 안전하게 해제됨
            LogError("Failed to process game data: {}", e.what());
            throw;  // 예외를 다시 던져서 상위에서 처리할 수 있게 함
        }
        // 함수 종료 시 모든 리소스 자동 해제
    }
    
    // RAII를 활용한 데이터베이스 연결 관리
    class DatabaseConnection {
    private:
        void* connection_handle_;  // 실제로는 DB 라이브러리의 핸들
        bool is_connected_;
        
    public:
        DatabaseConnection(const std::string& connection_string) 
            : connection_handle_(nullptr), is_connected_(false) {
            // 데이터베이스 연결
            connection_handle_ = ConnectToDatabase(connection_string);
            if (!connection_handle_) {
                throw DatabaseException("Failed to connect to database");
            }
            is_connected_ = true;
            LogInfo("Database connected successfully");
        }
        
        ~DatabaseConnection() {
            if (is_connected_ && connection_handle_) {
                DisconnectFromDatabase(connection_handle_);
                LogInfo("Database disconnected");
            }
        }
        
        // 복사 금지 (리소스는 하나만 소유)
        DatabaseConnection(const DatabaseConnection&) = delete;
        DatabaseConnection& operator=(const DatabaseConnection&) = delete;
        
        // 이동 허용
        DatabaseConnection(DatabaseConnection&& other) noexcept
            : connection_handle_(other.connection_handle_)
            , is_connected_(other.is_connected_) {
            other.connection_handle_ = nullptr;
            other.is_connected_ = false;
        }
        
        void ExecuteQuery(const std::string& query) {
            if (!is_connected_) {
                throw DatabaseException("Not connected to database");
            }
            
            // 쿼리 실행 (예외 발생 가능)
            if (!DoExecuteQuery(connection_handle_, query)) {
                throw DatabaseException("Query execution failed: " + query);
            }
        }
        
    private:
        void* ConnectToDatabase(const std::string& conn_str) { 
            // 실제 DB 연결 로직
            return reinterpret_cast<void*>(0x12345); // 더미
        }
        
        void DisconnectFromDatabase(void* handle) {
            // 실제 DB 연결 해제 로직
        }
        
        bool DoExecuteQuery(void* handle, const std::string& query) {
            // 실제 쿼리 실행 로직
            return true;
        }
        
        void LogInfo(const std::string& msg) {
            std::cout << "[INFO] " << msg << std::endl;
        }
    };
    
    void UseDatabaseConnection() {
        try {
            DatabaseConnection db("mysql://localhost:3306/gamedb");
            
            db.ExecuteQuery("SELECT * FROM players");
            db.ExecuteQuery("UPDATE players SET last_login = NOW()");
            
            // 여기서 예외가 발생해도 DatabaseConnection 소멸자에서 자동으로 연결 해제
            ProcessComplexQuery();
            
        } catch (const DatabaseException& e) {
            LogError("Database operation failed: {}", e.what());
        }
        // DatabaseConnection 객체가 스코프를 벗어나면서 자동으로 소멸자 호출됨
    }

private:
    void ProcessGameData(FILE* file, int* buffer) {
        // 게임 데이터 처리 (예외 발생 가능)
        if (rand() % 2) {
            throw std::runtime_error("Random processing error");
        }
    }
    
    void ProcessComplexQuery() {
        // 복잡한 쿼리 처리
        throw DatabaseException("Complex query failed");
    }
    
    void LogError(const std::string& format, const std::string& msg) {
        std::cout << "[ERROR] " << msg << std::endl;
    }
};
```

### **3.2 예외 안전성 수준 (Exception Safety Levels)**

```cpp
// 🛡️ 예외 안전성의 3가지 레벨

class ExceptionSafetyLevels {
public:
    // 1. 기본 보장 (Basic Guarantee)
    // - 예외 발생 시 객체는 유효한 상태 유지
    // - 메모리 누수 없음
    class BasicSafetyExample {
    private:
        std::vector<Player> players_;
        std::unique_ptr<GameWorld> world_;
        
    public:
        void AddPlayer(const Player& player) {
            // 기본 보장: 예외 발생해도 객체 상태는 유효함
            try {
                players_.push_back(player);  // 예외 가능
                world_->RegisterPlayer(player);  // 예외 가능
            } catch (...) {
                // players_는 일관된 상태이지만, 
                // world_와 동기화되지 않을 수 있음
                throw;
            }
        }
    };
    
    // 2. 강한 보장 (Strong Guarantee) - 권장!
    // - 예외 발생 시 호출 전 상태로 완전히 복원
    // - "commit or rollback" 동작
    class StrongSafetyExample {
    private:
        std::vector<Player> players_;
        std::unique_ptr<GameWorld> world_;
        
    public:
        void AddPlayer(const Player& player) {
            // 강한 보장: 예외 발생 시 완전히 이전 상태로 복원
            
            // 1단계: 모든 변경사항을 임시로 준비
            std::vector<Player> temp_players = players_;  // 복사본 생성
            temp_players.push_back(player);
            
            auto temp_world = std::make_unique<GameWorld>(*world_);
            temp_world->RegisterPlayer(player);  // 예외 발생 가능
            
            // 2단계: 모든 준비가 완료되면 한 번에 커밋
            // 이 시점부터는 예외가 발생하지 않아야 함
            players_ = std::move(temp_players);  // noexcept
            world_ = std::move(temp_world);      // noexcept
        }
        
        // 더 효율적인 강한 보장 구현
        void AddPlayerEfficient(const Player& player) {
            // Copy-and-swap idiom 사용
            StrongSafetyExample temp = *this;  // 현재 상태 복사
            
            temp.players_.push_back(player);
            temp.world_->RegisterPlayer(player);  // 예외 가능
            
            // 모든 작업이 성공하면 swap (noexcept)
            swap(*this, temp);
        }
        
        friend void swap(StrongSafetyExample& a, StrongSafetyExample& b) noexcept {
            using std::swap;
            swap(a.players_, b.players_);
            swap(a.world_, b.world_);
        }
    };
    
    // 3. 예외 없음 보장 (No-throw Guarantee)
    // - 절대 예외를 던지지 않음
    class NoThrowExample {
    private:
        int player_count_;
        bool is_server_running_;
        
    public:
        // 예외를 던지지 않는 함수들
        int GetPlayerCount() const noexcept {
            return player_count_;
        }
        
        bool IsServerRunning() const noexcept {
            return is_server_running_;
        }
        
        void SetPlayerCount(int count) noexcept {
            player_count_ = (count >= 0) ? count : 0;  // 방어적 프로그래밍
        }
        
        // 스왑 함수는 보통 noexcept
        void swap(NoThrowExample& other) noexcept {
            std::swap(player_count_, other.player_count_);
            std::swap(is_server_running_, other.is_server_running_);
        }
    };
    
    // 실전 예제: 게임 서버의 플레이어 관리 시스템
    class PlayerManager {
    private:
        std::unordered_map<uint21_t, std::unique_ptr<Player>> players_;
        std::mutex players_mutex_;
        size_t max_players_;
        
    public:
        PlayerManager(size_t max_players) : max_players_(max_players) {}
        
        // 강한 예외 안전성을 가진 플레이어 추가
        bool AddPlayer(std::unique_ptr<Player> player) {
            if (!player) {
                return false;  // null 포인터는 조용히 실패
            }
            
            std::lock_guard<std::mutex> lock(players_mutex_);
            
            if (players_.size() >= max_players_) {
                return false;  // 용량 초과는 조용히 실패
            }
            
            uint21_t player_id = player->GetId();
            
            // 이미 존재하는 플레이어인지 확인
            if (players_.find(player_id) != players_.end()) {
                return false;  // 중복은 조용히 실패
            }
            
            try {
                // 강한 보장: 성공하거나 완전히 실패
                auto result = players_.emplace(player_id, std::move(player));
                
                if (result.second) {
                    // 성공 시 추가 작업들
                    NotifyPlayerAdded(player_id);  // 예외 가능
                    UpdateServerStats();           // 예외 가능
                    return true;
                } else {
                    return false;
                }
                
            } catch (const std::exception& e) {
                // 예외 발생 시 맵은 이미 이전 상태로 복원됨
                LogError("Failed to add player {}: {}", player_id, e.what());
                return false;
            }
        }
        
        // 예외 없음 보장을 가진 플레이어 조회
        Player* GetPlayer(uint21_t player_id) noexcept {
            std::lock_guard<std::mutex> lock(players_mutex_);
            
            auto it = players_.find(player_id);
            return (it != players_.end()) ? it->second.get() : nullptr;
        }
        
        // 예외 없음 보장을 가진 통계 조회
        size_t GetPlayerCount() const noexcept {
            std::lock_guard<std::mutex> lock(players_mutex_);
            return players_.size();
        }
        
    private:
        void NotifyPlayerAdded(uint21_t player_id) {
            // 다른 시스템에 플레이어 추가 알림 (예외 가능)
        }
        
        void UpdateServerStats() {
            // 서버 통계 업데이트 (예외 가능)
        }
        
        void LogError(const std::string& format, uint21_t id, const std::string& msg) {
            std::cout << "[ERROR] Player " << id << ": " << msg << std::endl;
        }
    };
};
```

---

## 📚 4. 네트워크와 비동기 처리에서의 예외 안전성

### **4.1 Boost.Asio와 예외 처리**

```cpp
#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <memory>
#include <iostream>

using boost::asio::ip::tcp;

// 🌐 네트워크 프로그래밍에서의 안전한 예외 처리

class SafeNetworkHandler {
private:
    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    
public:
    SafeNetworkHandler(boost::asio::io_context& ioc, int port)
        : io_context_(ioc)
        , acceptor_(ioc, tcp::endpoint(tcp::v4(), port)) {
    }
    
    // 예외 안전한 서버 시작
    void StartServer() noexcept {
        try {
            StartAccept();
            LogInfo("Server started successfully on port {}", acceptor_.local_endpoint().port());
        } catch (const std::exception& e) {
            LogError("Failed to start server: {}", e.what());
        }
    }
    
private:
    void StartAccept() {
        auto new_session = std::make_shared<Session>(io_context_);
        
        // 비동기 accept - 예외 안전한 방식
        acceptor_.async_accept(new_session->GetSocket(),
            [this, new_session](boost::system::error_code ec) {
                HandleAccept(new_session, ec);
            });
    }
    
    void HandleAccept(std::shared_ptr<Session> session, boost::system::error_code ec) {
        if (!ec) {
            // 연결 성공 - 세션 시작
            try {
                session->Start();
                LogInfo("New client connected from {}", 
                       session->GetSocket().remote_endpoint().address().to_string());
            } catch (const std::exception& e) {
                LogError("Failed to start session: {}", e.what());
                // 세션은 shared_ptr이므로 자동으로 정리됨
            }
        } else {
            LogWarning("Accept failed: {}", ec.message());
        }
        
        // 다음 연결을 위해 다시 accept 시작
        try {
            StartAccept();
        } catch (const std::exception& e) {
            LogCritical("Failed to restart accept: {}", e.what());
            // 서버 재시작 또는 종료 로직
        }
    }
    
public:
    // 예외 안전한 세션 클래스
    class Session : public std::enable_shared_from_this<Session> {
    private:
        tcp::socket socket_;
        std::array<char, 1024> buffer_;
        
    public:
        Session(boost::asio::io_context& ioc) : socket_(ioc) {}
        
        tcp::socket& GetSocket() { return socket_; }
        
        void Start() {
            try {
                StartRead();
            } catch (const std::exception& e) {
                LogError("Session start failed: {}", e.what());
                // shared_ptr이므로 자동 정리됨
            }
        }
        
    private:
        void StartRead() {
            auto self = shared_from_this();  // 생명주기 보장
            
            socket_.async_read_some(
                boost::asio::buffer(buffer_),
                [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
                    HandleRead(ec, bytes_transferred);
                });
        }
        
        void HandleRead(boost::system::error_code ec, std::size_t bytes_transferred) {
            if (!ec) {
                try {
                    // 패킷 처리
                    ProcessPacket(buffer_.data(), bytes_transferred);
                    
                    // 응답 전송
                    SendResponse("OK");
                    
                    // 다음 읽기 시작
                    StartRead();
                    
                } catch (const PacketCorruptedException& e) {
                    LogWarning("Corrupted packet from client: {}", e.what());
                    SendErrorResponse("PACKET_ERROR");
                    StartRead();  // 연결 유지하면서 계속 읽기
                    
                } catch (const std::exception& e) {
                    LogError("Packet processing failed: {}", e.what());
                    CloseConnection();
                }
            } else if (ec == boost::asio::error::eof) {
                // 클라이언트가 정상적으로 연결 종료
                LogInfo("Client disconnected normally");
                CloseConnection();
            } else {
                // 기타 에러
                LogError("Read error: {}", ec.message());
                CloseConnection();
            }
        }
        
        void SendResponse(const std::string& response) {
            auto self = shared_from_this();
            auto buffer = std::make_shared<std::string>(response);
            
            boost::asio::async_write(socket_, 
                boost::asio::buffer(*buffer),
                [this, self, buffer](boost::system::error_code ec, std::size_t /*bytes*/) {
                    if (ec) {
                        LogError("Write failed: {}", ec.message());
                        CloseConnection();
                    }
                });
        }
        
        void SendErrorResponse(const std::string& error_code) {
            SendResponse("ERROR:" + error_code);
        }
        
        void ProcessPacket(const char* data, size_t size) {
            // 패킷 유효성 검사
            if (size < 4) {
                throw PacketCorruptedException("UNKNOWN", size);
            }
            
            // 패킷 타입 확인
            std::string packet_type(data, 4);
            
            if (packet_type == "PING") {
                // PING 처리
            } else if (packet_type == "MOVE") {
                // 이동 처리
                ProcessMovePacket(data + 4, size - 4);
            } else if (packet_type == "CHAT") {
                // 채팅 처리
                ProcessChatPacket(data + 4, size - 4);
            } else {
                throw PacketCorruptedException(packet_type, size);
            }
        }
        
        void ProcessMovePacket(const char* data, size_t size) {
            if (size < 12) {  // x, y, z 좌표 (각 4바이트)
                throw PacketCorruptedException("MOVE", size);
            }
            
            // 좌표 파싱 및 유효성 검사
            float x, y, z;
            std::memcpy(&x, data, 4);
            std::memcpy(&y, data + 4, 4);
            std::memcpy(&z, data + 8, 4);
            
            // 좌표 범위 검사
            if (std::abs(x) > 10000 || std::abs(y) > 10000 || std::abs(z) > 10000) {
                throw std::invalid_argument("Invalid coordinates");
            }
            
            // 이동 처리 로직
            LogInfo("Player moved to ({}, {}, {})", x, y, z);
        }
        
        void ProcessChatPacket(const char* data, size_t size) {
            if (size == 0 || size > 500) {  // 채팅 메시지 길이 제한
                throw PacketCorruptedException("CHAT", size);
            }
            
            std::string message(data, size);
            
            // 메시지 유효성 검사
            if (message.find('\0') != std::string::npos) {
                throw PacketCorruptedException("CHAT", size);
            }
            
            // 채팅 처리 로직
            LogInfo("Chat message: {}", message);
        }
        
        void CloseConnection() noexcept {
            try {
                if (socket_.is_open()) {
                    socket_.close();
                    LogInfo("Connection closed");
                }
            } catch (const std::exception& e) {
                LogError("Error closing connection: {}", e.what());
            }
        }
    };

private:
    void LogInfo(const std::string& format, auto... args) {
        std::cout << "[INFO] ";
        printf(format.c_str(), args...);
        std::cout << std::endl;
    }
    
    void LogWarning(const std::string& format, auto... args) {
        std::cout << "[WARNING] ";
        printf(format.c_str(), args...);
        std::cout << std::endl;
    }
    
    void LogError(const std::string& format, auto... args) {
        std::cout << "[ERROR] ";
        printf(format.c_str(), args...);
        std::cout << std::endl;
    }
    
    void LogCritical(const std::string& format, auto... args) {
        std::cout << "[CRITICAL] ";
        printf(format.c_str(), args...);
        std::cout << std::endl;
    }
};
```

### **4.2 예외와 async/await 패턴 (C++20 코루틴)**

```cpp
#include <coroutine>
#include <exception>
#include <optional>

// 🚀 C++20 코루틴에서의 예외 안전한 처리

template<typename T>
class Task {
public:
    struct promise_type {
        std::exception_ptr exception_ = nullptr;
        T value_;
        
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        
        std::suspend_never initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        
        void return_value(T value) {
            value_ = std::move(value);
        }
        
        void unhandled_exception() {
            exception_ = std::current_exception();
        }
    };
    
    std::coroutine_handle<promise_type> handle_;
    
    explicit Task(std::coroutine_handle<promise_type> h) : handle_(h) {}
    
    ~Task() {
        if (handle_) {
            handle_.destroy();
        }
    }
    
    // 이동만 허용
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    
    Task(Task&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    Task& operator=(Task&& other) noexcept {
        if (this != &other) {
            if (handle_) {
                handle_.destroy();
            }
            handle_ = std::exchange(other.handle_, {});
        }
        return *this;
    }
    
    T get() {
        if (!handle_) {
            throw std::runtime_error("Invalid task handle");
        }
        
        if (handle_.promise().exception_) {
            std::rethrow_exception(handle_.promise().exception_);
        }
        
        return std::move(handle_.promise().value_);
    }
    
    bool is_ready() const noexcept {
        return handle_ && handle_.done();
    }
};

// 예외 안전한 게임 서버 코루틴 예제들
class CoroutineGameServer {
public:
    // 안전한 플레이어 로그인 처리
    Task<bool> LoginPlayerAsync(const std::string& username, const std::string& password) {
        try {
            // 1단계: 데이터베이스에서 플레이어 정보 조회
            auto player_info = co_await QueryPlayerFromDatabaseAsync(username);
            if (!player_info) {
                LogWarning("Player not found: {}", username);
                co_return false;
            }
            
            // 2단계: 비밀번호 검증
            bool password_valid = co_await ValidatePasswordAsync(password, player_info->password_hash);
            if (!password_valid) {
                LogWarning("Invalid password for player: {}", username);
                co_return false;
            }
            
            // 3단계: 플레이어 상태 확인
            if (player_info->is_banned) {
                LogWarning("Banned player login attempt: {}", username);
                co_return false;
            }
            
            // 4단계: 게임 월드에 플레이어 추가
            bool added = co_await AddPlayerToWorldAsync(*player_info);
            if (!added) {
                LogError("Failed to add player to world: {}", username);
                co_return false;
            }
            
            // 5단계: 로그인 성공 처리
            co_await UpdateLastLoginAsync(player_info->id);
            LogInfo("Player {} logged in successfully", username);
            co_return true;
            
        } catch (const DatabaseException& e) {
            LogError("Database error during login for {}: {}", username, e.what());
            co_return false;
        } catch (const NetworkException& e) {
            LogError("Network error during login for {}: {}", username, e.what());
            co_return false;
        } catch (const std::exception& e) {
            LogError("Unexpected error during login for {}: {}", username, e.what());
            co_return false;
        }
    }
    
    // 안전한 게임 데이터 저장
    Task<bool> SaveGameDataAsync(uint21_t player_id) {
        try {
            // 1단계: 현재 플레이어 상태 수집
            auto player_data = co_await CollectPlayerDataAsync(player_id);
            if (!player_data) {
                LogWarning("Player {} not found for saving", player_id);
                co_return false;
            }
            
            // 2단계: 데이터 유효성 검사
            if (!ValidatePlayerData(*player_data)) {
                LogError("Invalid player data for {}", player_id);
                co_return false;
            }
            
            // 3단계: 트랜잭션으로 데이터 저장
            bool saved = co_await SaveToDatabase(*player_data);
            if (!saved) {
                LogError("Failed to save player data for {}", player_id);
                co_return false;
            }
            
            // 4단계: 백업 데이터 생성
            co_await CreateBackupAsync(*player_data);  // 실패해도 괜찮음
            
            LogInfo("Player {} data saved successfully", player_id);
            co_return true;
            
        } catch (const DatabaseException& e) {
            LogError("Database error while saving player {}: {}", player_id, e.what());
            // 데이터 손실 방지를 위한 응급 로컬 저장
            co_await EmergencyLocalSaveAsync(player_id);
            co_return false;
        } catch (const std::exception& e) {
            LogError("Unexpected error while saving player {}: {}", player_id, e.what());
            co_return false;
        }
    }
    
    // 여러 플레이어의 동시 처리 (예외 안전)
    Task<std::vector<bool>> ProcessMultiplePlayersAsync(const std::vector<uint21_t>& player_ids) {
        std::vector<bool> results;
        results.reserve(player_ids.size());
        
        for (uint21_t player_id : player_ids) {
            try {
                bool result = co_await SaveGameDataAsync(player_id);
                results.push_back(result);
            } catch (const std::exception& e) {
                LogError("Failed to process player {}: {}", player_id, e.what());
                results.push_back(false);  // 개별 실패가 전체에 영향 없음
            }
        }
        
        co_return results;
    }

private:
    struct PlayerInfo {
        uint21_t id;
        std::string username;
        std::string password_hash;
        bool is_banned;
        int level;
        // 기타 필드들...
    };
    
    struct PlayerData {
        uint21_t id;
        float position_x, position_y, position_z;
        int level;
        int experience;
        std::vector<int> inventory;
        // 기타 게임 데이터...
    };
    
    // 보조 코루틴 함수들
    Task<std::optional<PlayerInfo>> QueryPlayerFromDatabaseAsync(const std::string& username) {
        // 실제 데이터베이스 비동기 쿼리
        // 예외 발생 가능
        try {
            // DB 쿼리 시뮬레이션
            if (username == "testuser") {
                PlayerInfo info{1, "testuser", "hash123", false, 10};
                co_return info;
            }
            co_return std::nullopt;
        } catch (const std::exception& e) {
            LogError("Database query failed for {}: {}", username, e.what());
            throw DatabaseException("Player query failed");
        }
    }
    
    Task<bool> ValidatePasswordAsync(const std::string& password, const std::string& hash) {
        // 비동기 비밀번호 해싱 및 검증
        co_return password == "test123";  // 단순화된 검증
    }
    
    Task<bool> AddPlayerToWorldAsync(const PlayerInfo& player) {
        // 게임 월드에 플레이어 추가
        co_return true;  // 단순화
    }
    
    Task<void> UpdateLastLoginAsync(uint21_t player_id) {
        // 마지막 로그인 시간 업데이트
        co_return;
    }
    
    Task<std::optional<PlayerData>> CollectPlayerDataAsync(uint21_t player_id) {
        // 플레이어 현재 상태 수집
        PlayerData data{player_id, 100.0f, 200.0f, 0.0f, 10, 1500, {1, 2, 3}};
        co_return data;
    }
    
    bool ValidatePlayerData(const PlayerData& data) {
        // 데이터 유효성 검사
        return data.level > 0 && data.experience >= 0;
    }
    
    Task<bool> SaveToDatabase(const PlayerData& data) {
        // 데이터베이스에 저장
        co_return true;  // 단순화
    }
    
    Task<void> CreateBackupAsync(const PlayerData& data) {
        // 백업 생성 (실패해도 괜찮음)
        co_return;
    }
    
    Task<void> EmergencyLocalSaveAsync(uint21_t player_id) {
        // 응급 로컬 저장
        LogInfo("Emergency local save for player {}", player_id);
        co_return;
    }
    
    void LogInfo(const std::string& format, auto... args) {
        std::cout << "[INFO] ";
        printf(format.c_str(), args...);
        std::cout << std::endl;
    }
    
    void LogWarning(const std::string& format, auto... args) {
        std::cout << "[WARNING] ";
        printf(format.c_str(), args...);
        std::cout << std::endl;
    }
    
    void LogError(const std::string& format, auto... args) {
        std::cout << "[ERROR] ";
        printf(format.c_str(), args...);
        std::cout << std::endl;
    }
};
```

---

## 📚 5. 성능과 예외 처리의 균형

### **5.1 예외 처리 성능 최적화**

```cpp
#include <chrono>
#include <optional>
#include <expected>  // C++23

// ⚡ 성능을 고려한 예외 처리 전략

class PerformanceOptimizedExceptionHandling {
public:
    // 성능 측정을 위한 헬퍼
    class Timer {
        std::chrono::high_resolution_clock::time_point start_;
    public:
        Timer() : start_(std::chrono::high_resolution_clock::now()) {}
        
        double ElapsedMs() const {
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start_);
            return duration.count() / 1000.0;
        }
    };
    
    // 1. 예외 vs 에러 코드 성능 비교
    enum class ErrorCode {
        SUCCESS,
        INVALID_PLAYER_ID,
        PLAYER_NOT_FOUND,
        DATABASE_ERROR,
        NETWORK_ERROR
    };
    
    // ❌ 예외 기반 (정상 경로에서는 빠름, 예외 발생 시 느림)
    int ProcessPlayerWithExceptions(uint21_t player_id) {
        if (player_id == 0) {
            throw std::invalid_argument("Invalid player ID");
        }
        
        if (player_id > 1000000) {
            throw PlayerNotFoundException(player_id);
        }
        
        if (player_id % 1000 == 0) {
            throw DatabaseException("Random database error");
        }
        
        return player_id * 2;  // 정상 처리
    }
    
    // ✅ 에러 코드 기반 (안정적인 성능)
    std::pair<ErrorCode, int> ProcessPlayerWithErrorCodes(uint21_t player_id) noexcept {
        if (player_id == 0) {
            return {ErrorCode::INVALID_PLAYER_ID, 0};
        }
        
        if (player_id > 1000000) {
            return {ErrorCode::PLAYER_NOT_FOUND, 0};
        }
        
        if (player_id % 1000 == 0) {
            return {ErrorCode::DATABASE_ERROR, 0};
        }
        
        return {ErrorCode::SUCCESS, static_cast<int>(player_id * 2)};
    }
    
    // ✅ std::optional 기반 (C++17)
    std::optional<int> ProcessPlayerWithOptional(uint21_t player_id) noexcept {
        if (player_id == 0 || player_id > 1000000 || player_id % 1000 == 0) {
            return std::nullopt;
        }
        
        return player_id * 2;
    }
    
    // ✅ std::expected 기반 (C++23)
    /*
    std::expected<int, ErrorCode> ProcessPlayerWithExpected(uint21_t player_id) noexcept {
        if (player_id == 0) {
            return std::unexpected(ErrorCode::INVALID_PLAYER_ID);
        }
        
        if (player_id > 1000000) {
            return std::unexpected(ErrorCode::PLAYER_NOT_FOUND);
        }
        
        if (player_id % 1000 == 0) {
            return std::unexpected(ErrorCode::DATABASE_ERROR);
        }
        
        return player_id * 2;
    }
    */
    
    void PerformanceComparison() {
        const int iterations = 100000;
        const std::vector<uint21_t> test_ids = {1, 500, 1000, 2000, 500000, 1000000};
        
        // 예외 기반 성능 측정
        {
            Timer timer;
            int success_count = 0;
            int exception_count = 0;
            
            for (int i = 0; i < iterations; ++i) {
                for (uint21_t id : test_ids) {
                    try {
                        int result = ProcessPlayerWithExceptions(id);
                        success_count++;
                    } catch (const std::exception&) {
                        exception_count++;
                    }
                }
            }
            
            std::cout << "Exceptions - Time: " << timer.ElapsedMs() << "ms, "
                     << "Success: " << success_count << ", Exceptions: " << exception_count << std::endl;
        }
        
        // 에러 코드 기반 성능 측정
        {
            Timer timer;
            int success_count = 0;
            int error_count = 0;
            
            for (int i = 0; i < iterations; ++i) {
                for (uint21_t id : test_ids) {
                    auto [error_code, result] = ProcessPlayerWithErrorCodes(id);
                    if (error_code == ErrorCode::SUCCESS) {
                        success_count++;
                    } else {
                        error_count++;
                    }
                }
            }
            
            std::cout << "Error codes - Time: " << timer.ElapsedMs() << "ms, "
                     << "Success: " << success_count << ", Errors: " << error_count << std::endl;
        }
        
        // std::optional 기반 성능 측정
        {
            Timer timer;
            int success_count = 0;
            int error_count = 0;
            
            for (int i = 0; i < iterations; ++i) {
                for (uint21_t id : test_ids) {
                    auto result = ProcessPlayerWithOptional(id);
                    if (result) {
                        success_count++;
                    } else {
                        error_count++;
                    }
                }
            }
            
            std::cout << "std::optional - Time: " << timer.ElapsedMs() << "ms, "
                     << "Success: " << success_count << ", Errors: " << error_count << std::endl;
        }
    }
    
    // 2. 조건부 예외 처리 (성능 최적화)
    class ConditionalExceptionHandling {
    private:
        bool debug_mode_ = false;
        bool strict_validation_ = false;
        
    public:
        void SetDebugMode(bool enabled) noexcept { debug_mode_ = enabled; }
        void SetStrictValidation(bool enabled) noexcept { strict_validation_ = enabled; }
        
        // 성능 크리티컬한 경로에서는 예외를 최소화
        bool ProcessPlayerFast(uint21_t player_id, int& result) noexcept {
            // 빠른 경로: 기본적인 검증만
            if (player_id == 0) {
                return false;  // 에러 코드 반환
            }
            
            result = player_id * 2;
            return true;
        }
        
        // 디버그 모드나 엄격한 검증이 필요할 때만 예외 사용
        int ProcessPlayerWithValidation(uint21_t player_id) {
            if (strict_validation_) {
                // 엄격한 검증 모드: 예외 사용
                ValidatePlayerIdStrict(player_id);
                ValidatePlayerExistence(player_id);
                ValidatePlayerPermissions(player_id);
            } else {
                // 빠른 모드: 기본 검증만
                if (player_id == 0) {
                    if (debug_mode_) {
                        throw std::invalid_argument("Invalid player ID: 0");
                    }
                    return 0;  // 조용히 실패
                }
            }
            
            return player_id * 2;
        }
        
    private:
        void ValidatePlayerIdStrict(uint21_t player_id) {
            if (player_id == 0) {
                throw std::invalid_argument("Player ID cannot be 0");
            }
            if (player_id > 1000000) {
                throw std::out_of_range("Player ID too large");
            }
        }
        
        void ValidatePlayerExistence(uint21_t player_id) {
            if (player_id % 1000 == 0) {
                throw PlayerNotFoundException(player_id);
            }
        }
        
        void ValidatePlayerPermissions(uint21_t player_id) {
            if (player_id % 100 == 0) {
                throw std::runtime_error("Player access denied");
            }
        }
    };
    
    // 3. 예외 처리 비용 최소화 기법
    class ExceptionOptimization {
    public:
        // 예외 발생 가능성을 미리 체크하여 비용 절약
        bool SafeProcessPlayer(uint21_t player_id, int& result) noexcept {
            // 1단계: 빠른 사전 검사
            if (!IsPlayerIdValid(player_id)) {
                return false;
            }
            
            // 2단계: 예외가 발생할 수 있는 작업을 안전하게 래핑
            try {
                result = DoProcessPlayer(player_id);
                return true;
            } catch (const std::exception& e) {
                // 예외 발생 시 로그 기록 (선택적)
                if (ShouldLogException()) {
                    LogException(e.what());
                }
                return false;
            }
        }
        
        // 예외 정보를 미리 수집하여 throw 비용 절약
        void ThrowDetailedException(uint21_t player_id, const std::string& operation) {
            // 예외 정보를 미리 준비 (한 번만 계산)
            static thread_local std::string error_prefix = "Player operation failed: ";
            
            std::string error_message = error_prefix + operation + 
                                      " (Player ID: " + std::to_string(player_id) + ")";
            
            throw std::runtime_error(std::move(error_message));
        }
        
        // 예외 객체 재사용으로 할당 비용 절약
        class ExceptionPool {
        private:
            static thread_local std::runtime_error cached_exception_;
            
        public:
            static void ThrowCachedException(const std::string& message) {
                // 기존 예외 객체를 재사용하여 할당 비용 절약
                // (주의: 멀티스레드 환경에서는 thread_local 사용)
                cached_exception_ = std::runtime_error(message);
                throw cached_exception_;
            }
        };
        
    private:
        bool IsPlayerIdValid(uint21_t player_id) const noexcept {
            return player_id > 0 && player_id <= 1000000;
        }
        
        int DoProcessPlayer(uint21_t player_id) {
            if (player_id % 1000 == 0) {
                throw std::runtime_error("Simulated processing error");
            }
            return player_id * 2;
        }
        
        bool ShouldLogException() const noexcept {
            // 로깅 레벨이나 설정에 따라 결정
            return true;
        }
        
        void LogException(const std::string& message) noexcept {
            std::cout << "[EXCEPTION] " << message << std::endl;
        }
    };
};

// thread_local 정의
thread_local std::runtime_error PerformanceOptimizedExceptionHandling::ExceptionOptimization::ExceptionPool::cached_exception_("");
```

### **5.2 실시간 게임에서의 예외 처리 전략**

```cpp
// 🎮 실시간 게임 서버에서의 예외 처리 전략

class RealtimeGameExceptionStrategy {
private:
    // 성능 모니터링
    struct PerformanceMetrics {
        std::atomic<uint21_t> exception_count{0};
        std::atomic<uint21_t> total_operations{0};
        std::chrono::steady_clock::time_point last_reset;
        
        PerformanceMetrics() : last_reset(std::chrono::steady_clock::now()) {}
        
        double GetExceptionRate() const noexcept {
            uint21_t ops = total_operations.load();
            return ops > 0 ? static_cast<double>(exception_count.load()) / ops : 0.0;
        }
        
        void RecordOperation() noexcept { total_operations++; }
        void RecordException() noexcept { exception_count++; }
        
        void Reset() noexcept {
            exception_count = 0;
            total_operations = 0;
            last_reset = std::chrono::steady_clock::now();
        }
    };
    
    mutable PerformanceMetrics metrics_;
    
public:
    // 1. 틱 기반 처리에서의 예외 안전성
    void ProcessGameTick(float delta_time) noexcept {
        try {
            // 각 시스템을 독립적으로 처리하여 하나의 실패가 전체에 영향 없도록
            ProcessPhysicsSystem(delta_time);
        } catch (const std::exception& e) {
            LogError("Physics system error: {}", e.what());
            metrics_.RecordException();
        }
        
        try {
            ProcessCombatSystem(delta_time);
        } catch (const std::exception& e) {
            LogError("Combat system error: {}", e.what());
            metrics_.RecordException();
        }
        
        try {
            ProcessAISystem(delta_time);
        } catch (const std::exception& e) {
            LogError("AI system error: {}", e.what());
            metrics_.RecordException();
        }
        
        try {
            ProcessNetworkSystem(delta_time);
        } catch (const std::exception& e) {
            LogError("Network system error: {}", e.what());
            metrics_.RecordException();
        }
        
        metrics_.RecordOperation();
        
        // 예외 발생률 모니터링
        if (metrics_.GetExceptionRate() > 0.01) {  // 1% 초과 시 경고
            LogWarning("High exception rate detected: {:.2f}%", 
                      metrics_.GetExceptionRate() * 100);
        }
    }
    
    // 2. 플레이어별 독립적 처리
    void ProcessPlayers(const std::vector<Player>& players) noexcept {
        for (const auto& player : players) {
            try {
                ProcessSinglePlayer(player);
            } catch (const PlayerException& e) {
                // 플레이어 관련 예외는 해당 플레이어만 영향
                LogWarning("Player {} error: {}", player.GetId(), e.what());
                NotifyPlayerError(player.GetId(), e.what());
                metrics_.RecordException();
            } catch (const std::exception& e) {
                // 기타 예외도 해당 플레이어만 건너뛰기
                LogError("Unexpected error for player {}: {}", player.GetId(), e.what());
                metrics_.RecordException();
            }
            
            metrics_.RecordOperation();
        }
    }
    
    // 3. 우선순위 기반 에러 처리
    enum class ErrorPriority {
        IGNORE,     // 무시하고 계속
        LOG_ONLY,   // 로그만 기록
        RECOVER,    // 복구 시도
        CRITICAL    // 즉시 안전 종료
    };
    
    ErrorPriority ClassifyError(const std::exception& e) const noexcept {
        // 예외 타입과 메시지를 기반으로 우선순위 결정
        const std::string msg = e.what();
        
        if (dynamic_cast<const std::bad_alloc*>(&e)) {
            return ErrorPriority::CRITICAL;  // 메모리 부족은 치명적
        }
        
        if (dynamic_cast<const DatabaseException*>(&e)) {
            return ErrorPriority::RECOVER;   // DB 오류는 복구 시도
        }
        
        if (dynamic_cast<const NetworkException*>(&e)) {
            return ErrorPriority::LOG_ONLY;  // 네트워크 오류는 일시적일 수 있음
        }
        
        if (msg.find("validation") != std::string::npos) {
            return ErrorPriority::IGNORE;    // 검증 오류는 무시
        }
        
        return ErrorPriority::LOG_ONLY;  // 기본값
    }
    
    void HandleErrorWithPriority(const std::exception& e) noexcept {
        ErrorPriority priority = ClassifyError(e);
        
        switch (priority) {
        case ErrorPriority::IGNORE:
            // 조용히 무시
            break;
            
        case ErrorPriority::LOG_ONLY:
            LogWarning("Non-critical error: {}", e.what());
            break;
            
        case ErrorPriority::RECOVER:
            LogError("Recoverable error: {}", e.what());
            AttemptRecovery(e);
            break;
            
        case ErrorPriority::CRITICAL:
            LogCritical("Critical error: {}", e.what());
            InitiateGracefulShutdown(e);
            break;
        }
    }
    
    // 4. 복구 메커니즘
    void AttemptRecovery(const std::exception& e) noexcept {
        try {
            if (dynamic_cast<const DatabaseException*>(&e)) {
                RecoverFromDatabaseError();
            } else if (dynamic_cast<const NetworkException*>(&e)) {
                RecoverFromNetworkError();
            }
        } catch (const std::exception& recovery_error) {
            LogError("Recovery failed: {}", recovery_error.what());
        }
    }
    
    void RecoverFromDatabaseError() {
        // DB 연결 재설정 시도
        LogInfo("Attempting database recovery...");
        // 실제 복구 로직
    }
    
    void RecoverFromNetworkError() {
        // 네트워크 연결 재설정 시도
        LogInfo("Attempting network recovery...");
        // 실제 복구 로직
    }
    
    // 5. 안전한 종료
    void InitiateGracefulShutdown(const std::exception& e) noexcept {
        LogCritical("Initiating graceful shutdown due to: {}", e.what());
        
        try {
            // 모든 플레이어에게 알림
            NotifyAllPlayersShutdown();
            
            // 중요한 데이터 저장
            SaveCriticalGameData();
            
            // 연결 종료
            CloseAllConnections();
            
        } catch (const std::exception& shutdown_error) {
            LogCritical("Error during shutdown: {}", shutdown_error.what());
        }
        
        // 강제 종료
        std::exit(1);
    }

private:
    // 게임 시스템들
    void ProcessPhysicsSystem(float delta_time) {
        // 물리 시스템 처리 (예외 발생 가능)
        if (rand() % 10000 == 0) {
            throw std::runtime_error("Physics calculation overflow");
        }
    }
    
    void ProcessCombatSystem(float delta_time) {
        // 전투 시스템 처리
        if (rand() % 5000 == 0) {
            throw std::runtime_error("Combat validation failed");
        }
    }
    
    void ProcessAISystem(float delta_time) {
        // AI 시스템 처리
        if (rand() % 8000 == 0) {
            throw std::runtime_error("AI pathfinding error");
        }
    }
    
    void ProcessNetworkSystem(float delta_time) {
        // 네트워크 시스템 처리
        if (rand() % 3000 == 0) {
            throw NetworkException("Packet processing failed");
        }
    }
    
    void ProcessSinglePlayer(const Player& player) {
        // 개별 플레이어 처리
        if (player.GetId() % 1000 == 0) {
            throw PlayerException("Player data corruption");
        }
    }
    
    void NotifyPlayerError(uint21_t player_id, const std::string& error) noexcept {
        std::cout << "Notifying player " << player_id << " of error: " << error << std::endl;
    }
    
    void NotifyAllPlayersShutdown() noexcept {
        std::cout << "Broadcasting shutdown notification to all players" << std::endl;
    }
    
    void SaveCriticalGameData() noexcept {
        std::cout << "Saving critical game data..." << std::endl;
    }
    
    void CloseAllConnections() noexcept {
        std::cout << "Closing all network connections..." << std::endl;
    }
    
    void LogInfo(const std::string& format, auto... args) const noexcept {
        std::cout << "[INFO] ";
        printf(format.c_str(), args...);
        std::cout << std::endl;
    }
    
    void LogWarning(const std::string& format, auto... args) const noexcept {
        std::cout << "[WARNING] ";
        printf(format.c_str(), args...);
        std::cout << std::endl;
    }
    
    void LogError(const std::string& format, auto... args) const noexcept {
        std::cout << "[ERROR] ";
        printf(format.c_str(), args...);
        std::cout << std::endl;
    }
    
    void LogCritical(const std::string& format, auto... args) const noexcept {
        std::cout << "[CRITICAL] ";
        printf(format.c_str(), args...);
        std::cout << std::endl;
    }
};

// 더미 클래스들
class Player {
    uint21_t id_;
public:
    Player(uint21_t id) : id_(id) {}
    uint21_t GetId() const { return id_; }
};
```

---

## 📝 정리 및 다음 단계

### **학습한 내용 정리**

✅ **예외 처리 기초**
- try-catch 기본 문법과 예외 계층 구조
- 표준 예외 클래스들의 활용
- throw와 rethrow 메커니즘

✅ **사용자 정의 예외**
- 게임 서버 특화 예외 클래스 설계
- 예외 계층 구조와 상속 활용
- 예외 정보 수집과 디버깅

✅ **RAII와 예외 안전성**
- 자동 리소스 관리와 예외 안전성
- 3가지 예외 안전성 수준
- Copy-and-swap idiom

✅ **네트워크와 비동기 처리**
- Boost.Asio에서의 예외 처리
- 코루틴과 예외의 조합
- 비동기 환경에서의 안전성

✅ **성능 최적화**
- 예외 vs 에러 코드 성능 비교
- 실시간 게임에서의 예외 처리 전략
- 우선순위 기반 에러 처리

### **다음 학습 권장사항**

1. **[STL 알고리즘 완전 정복](./32_stl_algorithms_comprehensive.md)** 🔥
   - 모든 STL 알고리즘과 예외 안전성
   - 성능 최적화 기법

2. **[Move 의미론과 Perfect Forwarding](./20_move_semantics_perfect_forwarding.md)** 🔥
   - 예외 안전한 이동 의미론
   - 완벽한 전달과 성능 최적화

3. **[빌드 시스템 고급](./21_build_systems_advanced.md)** 🔥
   - CMake 고급 패턴
   - 테스트 프레임워크 통합

### **실습 과제**

🎯 **초급 과제**: 안전한 플레이어 관리 시스템
- RAII를 활용한 리소스 관리
- 기본적인 예외 처리 구현

🎯 **중급 과제**: 예외 안전한 네트워크 시스템
- Boost.Asio와 예외 처리 통합
- 연결 복구 메커니즘 구현

🎯 **고급 과제**: 실시간 게임 서버 예외 처리 시스템
- 우선순위 기반 에러 분류
- 성능 모니터링과 자동 복구
- 안전한 서버 종료 메커니즘

---

**💡 핵심 포인트**: 예외 처리는 단순히 에러를 잡는 것이 아니라, 프로그램의 안정성과 신뢰성을 보장하는 핵심 메커니즘입니다. 게임 서버에서는 한 플레이어의 문제가 전체 서버에 영향을 주지 않도록 하는 방어적 프로그래밍이 필수입니다.