# 3단계: 네트워크 프로그래밍 기초 - Boost.Asio 실전 가이드
*MMORPG Server Engine 프로젝트를 위한 완벽 가이드*

---

## 🎯 이 문서에서 배울 내용
- 실제 구현된 Boost.Asio TCP 서버 아키텍처
- 5,000명 동시 접속을 지원하는 IO Context Pool 설계
- Protocol Buffers 기반 패킷 시스템 구현 방법
- 세션 관리와 패킷 처리의 실전 기법

### 🎮 MMORPG Server Engine에서의 활용

```
🌐 네트워크 아키텍처 구현 현황
├── TCP Server: Boost.Asio 기반 비동기 서버 (TcpServer 클래스)
├── Session 관리: 5,000개 동시 세션 지원 (SessionManager)
├── Packet 시스템: Protocol Buffers 직렬화 (23개 패킷 타입)
└── IO Context Pool: 4개 스레드로 부하 분산
    ├── Accept Thread: 새 연결 수락
    ├── Worker Threads: 패킷 I/O 처리
    ├── Handler Thread: 게임 로직 처리
    └── Monitor Thread: 성능 메트릭 수집
```

---

## 🏗️ TCP 서버 아키텍처 실제 구현

### 서버 설정과 초기화

**`src/core/network/tcp_server.h`의 실제 구성:**
```cpp
// [SEQUENCE: 1] 실제 서버 설정 구조체
struct ServerConfig {
    std::string address = "0.0.0.0";      // 모든 인터페이스에서 수신
    uint16_t port = 8080;                  // Login Server: 8080, Game Server: 8081
    size_t worker_threads = 4;             // CPU 코어 수에 맞춤
    size_t max_connections = 5000;         // 동시 접속 제한
    size_t io_context_pool_size = 4;       // IO Context 풀 크기
    bool reuse_address = true;             // SO_REUSEADDR 활성화
    bool tcp_no_delay = true;              // 네이글 알고리즘 비활성화
    size_t accept_backlog = 100;           // Accept 대기열 크기
};
```

**실제 성능 고려사항:**
- **worker_threads = CPU 코어 수**: 컨텍스트 스위칭 최소화
- **tcp_no_delay = true**: 실시간 게임에서 레이턴시 최우선
- **accept_backlog = 100**: DDoS 방어와 정상 트래픽 균형

```
📱 클라이언트 (게임 앱)          🖥️ 서버 (게임 서버)
┌─────────────────────┐         ┌─────────────────────┐
│ - 화면 출력         │◄────────┤ - 게임 로직 처리    │
│ - 키보드/마우스 입력│         │ - 플레이어 데이터   │
│ - 그래픽/사운드     │────────►│ - 채팅 시스템       │
│ - 서버와 통신       │         │ - 모든 클라이언트   │
└─────────────────────┘         │   관리              │
                                └─────────────────────┘

💬 실제 대화 예시:
클라이언트: "안녕! 나는 '드래곤슬레이어'야, 게임에 입장하고 싶어!"
서버: "환영해! 너의 캐릭터 정보를 불러올게. 잠깐만 기다려."
클라이언트: "내가 (100, 200) 위치로 이동했어!"
서버: "알겠어! 다른 플레이어들에게도 네가 이동했다고 알려줄게."
```

**🎮 왜 네트워크가 중요한가:**
- **실시간 상호작용**: 여러 플레이어가 동시에 같은 게임 세계에서 플레이
- **데이터 동기화**: 모든 플레이어가 같은 게임 상태를 보도록 보장
- **공정한 게임**: 서버가 모든 게임 규칙을 관리하여 치팅 방지

---

## 🏠 1. 클라이언트-서버 모델 이해

### **1.1 우체국 비유로 이해하기**

```
🏠 집 (클라이언트)                    🏢 우체국 (서버)
┌─────────────────┐                 ┌─────────────────┐
│ 편지를 쓴다     │ ────편지────►   │ 편지를 받는다   │
│ (요청 메시지)   │                 │ (요청 처리)     │
│                 │ ◄────답장────   │ 답장을 보낸다   │
│ 답장을 받는다   │                 │ (응답 메시지)   │
└─────────────────┘                 └─────────────────┘

🎮 게임으로 번역하면:
집 = 게임 클라이언트 (당신의 게임 앱)
우체국 = 게임 서버 (회사의 서버 컴퓨터)
편지 = 네트워크 메시지 (이동, 공격, 채팅 등)
```

### **1.2 실제 게임에서의 동작**

```cpp
// 💬 이런 대화가 실제로 일어남

클라이언트 → 서버: {
    "type": "login",
    "username": "DragonSlayer",
    "password": "secret123"
}

서버 → 클라이언트: {
    "type": "login_response",
    "success": true,
    "player_id": 12345,
    "message": "환영합니다!"
}

클라이언트 → 서버: {
    "type": "move",
    "player_id": 12345,
    "x": 100.5,
    "y": 200.3
}

서버 → 모든 클라이언트: {
    "type": "player_moved",
    "player_id": 12345,
    "x": 100.5,
    "y": 200.3,
    "player_name": "DragonSlayer"
}
```

---

## 🚛 2. TCP vs UDP: 배송 방법 선택하기

### **2.1 TCP: 등기우편 (안전하고 순서대로)**

```
📮 TCP = 등기우편
✅ 장점:
- 편지가 반드시 도착함 (신뢰성)
- 보낸 순서대로 도착함 (순서 보장)
- 받았는지 확인 가능 (확인 응답)

❌ 단점:
- 조금 느림 (확인 과정 때문에)
- 오버헤드가 있음 (부가 정보가 많음)

🎮 게임에서 사용하는 경우:
- 로그인/로그아웃
- 채팅 메시지
- 아이템 거래
- 캐릭터 생성/삭제
- 중요한 게임 이벤트

💡 "절대 잃어버리면 안 되는 중요한 정보"에 사용
```

### **2.2 UDP: 일반우편 (빠르지만 불확실)**

```
📪 UDP = 일반우편
✅ 장점:
- 매우 빠름 (확인 과정 없음)
- 오버헤드 적음 (부가 정보 최소)
- 실시간 통신에 적합

❌ 단점:
- 편지가 안 도착할 수 있음 (신뢰성 낮음)
- 순서가 바뀔 수 있음
- 받았는지 모름

🎮 게임에서 사용하는 경우:
- 플레이어 위치 업데이트 (초당 30번)
- 실시간 전투 액션
- 음성 채팅
- 게임 상태 동기화

💡 "조금 잃어버려도 괜찮지만 빨라야 하는 정보"에 사용
```

### **2.3 실제 게임에서의 선택 기준**

```cpp
// 🎮 실제 게임 서버에서의 프로토콜 선택

class GameNetworkManager {
public:
    // TCP를 사용하는 기능들
    void SendLoginRequest(const std::string& username, const std::string& password) {
        // TCP: 로그인은 반드시 성공해야 함
        TCPMessage login_msg;
        login_msg.type = "login";
        login_msg.data = {{"username", username}, {"password", password}};
        tcp_client.Send(login_msg);
    }
    
    void SendChatMessage(const std::string& message) {
        // TCP: 채팅 메시지는 반드시 전달되어야 함
        TCPMessage chat_msg;
        chat_msg.type = "chat";
        chat_msg.data = {{"message", message}};
        tcp_client.Send(chat_msg);
    }
    
    // UDP를 사용하는 기능들
    void SendPositionUpdate(float x, float y) {
        // UDP: 위치는 계속 업데이트되므로 하나 정도 잃어버려도 괜찮음
        UDPMessage pos_msg;
        pos_msg.type = "position";
        pos_msg.data = {{"x", x}, {"y", y}};
        udp_client.Send(pos_msg);  // 빠른 전송이 중요!
    }
    
    void SendAttackAction(int target_id, int skill_id) {
        // 🤔 이건 어떨까? TCP vs UDP?
        // 정답: TCP! 공격은 반드시 서버에 전달되어야 함
        TCPMessage attack_msg;
        attack_msg.type = "attack";
        attack_msg.data = {{"target", target_id}, {"skill", skill_id}};
        tcp_client.Send(attack_msg);
    }
};
```

---

## 🔌 3. 소켓 프로그래밍 기초

### **3.1 소켓이 뭔가요?**

```
🔌 소켓 = 네트워크 전화기

집 전화기 (클라이언트 소켓)     교환기 (서버 소켓)
┌─────────────────┐           ┌─────────────────┐
│ 📞 전화 걸기    │ ────선────│ 📞 전화 받기    │
│ (connect)       │           │ (accept)        │
│ 대화하기        │ ◄────────►│ 대화하기        │
│ (send/recv)     │           │ (send/recv)     │
│ 전화 끊기       │           │ 전화 끊기       │
│ (close)         │           │ (close)         │
└─────────────────┘           └─────────────────┘

💡 소켓 = 네트워크 통신을 위한 창구 (인터페이스)
```

### **3.2 TCP 소켓 기본 개념**

```cpp
#include <iostream>
#include <string>

// 🔍 소켓의 기본 개념을 의사코드로 이해하기

// 📞 서버 측 (전화 받는 쪽)
class TCPServer {
public:
    void Start(int port) {
        // 1. 전화번호 등록 (포트 번호 바인딩)
        server_socket = CreateSocket();
        Bind(server_socket, port);  // 8080번으로 전화 받겠다!
        
        // 2. 전화 대기 상태로 전환
        Listen(server_socket);
        std::cout << "서버가 " << port << "번 포트에서 대기 중..." << std::endl;
        
        // 3. 클라이언트의 연결 요청 기다리기
        while (true) {
            Socket client_socket = Accept(server_socket);  // 전화 받기
            std::cout << "새로운 클라이언트 연결됨!" << std::endl;
            
            // 4. 클라이언트와 대화하기
            HandleClient(client_socket);
        }
    }
    
private:
    void HandleClient(Socket client_socket) {
        std::string message = Receive(client_socket);  // 클라이언트 메시지 받기
        std::cout << "클라이언트: " << message << std::endl;
        
        std::string response = "안녕하세요! 메시지 잘 받았습니다.";
        Send(client_socket, response);  // 응답 보내기
        
        Close(client_socket);  // 연결 종료
    }
};

// 📱 클라이언트 측 (전화 거는 쪽)
class TCPClient {
public:
    void ConnectToServer(const std::string& server_address, int port) {
        // 1. 전화기 준비
        client_socket = CreateSocket();
        
        // 2. 서버에 전화 걸기
        Connect(client_socket, server_address, port);
        std::cout << "서버에 연결됨: " << server_address << ":" << port << std::endl;
        
        // 3. 메시지 보내기
        std::string message = "안녕하세요! 클라이언트입니다.";
        Send(client_socket, message);
        
        // 4. 서버 응답 받기
        std::string response = Receive(client_socket);
        std::cout << "서버 응답: " << response << std::endl;
        
        // 5. 연결 종료
        Close(client_socket);
    }
};
```

### **3.3 실제 C++ 소켓 코드 (기본)**

```cpp
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define closesocket close
#endif

// 🎮 간단한 게임 서버 클래스
class SimpleGameServer {
private:
    SOCKET server_socket;
    std::vector<SOCKET> client_sockets;
    bool running;

public:
    SimpleGameServer() : running(false) {
#ifdef _WIN32
        // 윈도우에서는 WinSock 초기화 필요
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    }
    
    ~SimpleGameServer() {
        Stop();
#ifdef _WIN32
        WSACleanup();
#endif
    }
    
    bool Start(int port) {
        // 1. 소켓 생성
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket == INVALID_SOCKET) {
            std::cerr << "소켓 생성 실패!" << std::endl;
            return false;
        }
        
        // 2. 서버 주소 설정
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;           // IPv4 사용
        server_addr.sin_addr.s_addr = INADDR_ANY;   // 모든 IP에서 접속 허용
        server_addr.sin_port = htons(port);         // 포트 번호 (네트워크 바이트 순서로 변환)
        
        // 3. 소켓과 주소 바인딩
        if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "바인딩 실패!" << std::endl;
            return false;
        }
        
        // 4. 클라이언트 연결 대기 상태로 전환
        if (listen(server_socket, 5) < 0) {  // 최대 5개 대기열
            std::cerr << "리슨 실패!" << std::endl;
            return false;
        }
        
        running = true;
        std::cout << "🎮 게임 서버가 포트 " << port << "에서 시작됨!" << std::endl;
        
        // 5. 클라이언트 연결 처리 스레드 시작
        std::thread accept_thread(&SimpleGameServer::AcceptClients, this);
        accept_thread.detach();
        
        return true;
    }
    
    void Stop() {
        running = false;
        
        // 모든 클라이언트 연결 종료
        for (SOCKET client : client_sockets) {
            closesocket(client);
        }
        client_sockets.clear();
        
        // 서버 소켓 종료
        if (server_socket != INVALID_SOCKET) {
            closesocket(server_socket);
        }
        
        std::cout << "🛑 게임 서버 종료됨" << std::endl;
    }
    
    void BroadcastMessage(const std::string& message) {
        // 모든 클라이언트에게 메시지 전송
        std::string full_msg = message + "\n";
        
        for (auto it = client_sockets.begin(); it != client_sockets.end(); ) {
            SOCKET client = *it;
            int result = send(client, full_msg.c_str(), full_msg.length(), 0);
            
            if (result <= 0) {
                // 클라이언트 연결이 끊어진 경우
                std::cout << "클라이언트 연결 해제됨" << std::endl;
                closesocket(client);
                it = client_sockets.erase(it);
            } else {
                ++it;
            }
        }
    }

private:
    void AcceptClients() {
        while (running) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            
            // 새로운 클라이언트 연결 대기
            SOCKET client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_len);
            
            if (client_socket != INVALID_SOCKET) {
                client_sockets.push_back(client_socket);
                std::cout << "👋 새로운 플레이어 입장! (총 " << client_sockets.size() << "명)" << std::endl;
                
                // 클라이언트 처리 스레드 시작
                std::thread client_thread(&SimpleGameServer::HandleClient, this, client_socket);
                client_thread.detach();
                
                // 다른 플레이어들에게 알림
                BroadcastMessage("새로운 플레이어가 입장했습니다!");
            }
        }
    }
    
    void HandleClient(SOCKET client_socket) {
        char buffer[1024];
        
        while (running) {
            // 클라이언트로부터 메시지 받기
            int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            
            if (bytes_received <= 0) {
                // 클라이언트 연결 해제됨
                break;
            }
            
            buffer[bytes_received] = '\0';  // 문자열 끝 표시
            std::string message(buffer);
            
            std::cout << "📩 받은 메시지: " << message << std::endl;
            
            // 🎮 간단한 게임 명령어 처리
            if (message.find("MOVE") == 0) {
                BroadcastMessage("플레이어가 이동했습니다: " + message);
            } else if (message.find("CHAT") == 0) {
                std::string chat_content = message.substr(5);  // "CHAT " 제거
                BroadcastMessage("💬 " + chat_content);
            } else if (message.find("ATTACK") == 0) {
                BroadcastMessage("⚔️ 플레이어가 공격했습니다!");
            } else {
                // 기본 응답
                std::string response = "서버: 메시지 '" + message + "'를 받았습니다.";
                send(client_socket, response.c_str(), response.length(), 0);
            }
        }
    }
};

// 🎮 테스트용 메인 함수
int main() {
    SimpleGameServer server;
    
    if (!server.Start(8080)) {
        std::cerr << "서버 시작 실패!" << std::endl;
        return -1;
    }
    
    std::cout << "🎮 게임 서버 실행 중... (종료하려면 Enter)" << std::endl;
    std::cout << "클라이언트는 텔넷이나 netcat으로 테스트 가능:" << std::endl;
    std::cout << "  telnet localhost 8080" << std::endl;
    std::cout << "  또는 nc localhost 8080" << std::endl;
    
    // 간단한 명령어 처리
    std::string input;
    while (std::getline(std::cin, input)) {
        if (input == "quit" || input == "exit") {
            break;
        } else if (input.find("say") == 0) {
            std::string message = input.substr(4);  // "say " 제거
            server.BroadcastMessage("📢 서버 공지: " + message);
        } else {
            std::cout << "명령어: say <메시지>, quit" << std::endl;
        }
    }
    
    return 0;
}
```

---

## 📦 4. 직렬화/역직렬화 (데이터 포장하기)

### **4.1 직렬화가 뭔가요?**

```
🎁 직렬화 = 택배 포장하기

보내고 싶은 물건 (C++ 객체)     📦 택배 상자 (네트워크 데이터)
┌─────────────────────┐         ┌─────────────────────┐
│ struct Player {     │         │ 01001001 01000001   │
│   string name;      │ ──포장──► │ 01101100 01101001   │
│   int level;        │         │ 01100011 01100101   │
│   float x, y;       │         │ ...                 │
│ }                   │         │ (바이너리 데이터)   │
└─────────────────────┘         └─────────────────────┘

받은 택배 (네트워크 데이터)      원래 물건 (C++ 객체)
┌─────────────────────┐         ┌─────────────────────┐
│ 01001001 01000001   │         │ Player alice {      │
│ 01101100 01101001   │ ──언패킹─► │   name: "Alice",    │
│ 01100011 01100101   │         │   level: 25,        │
│ ...                 │         │   x: 100.0, y: 200.0│
│ (바이너리 데이터)   │         │ }                   │
└─────────────────────┘         └─────────────────────┘
```

### **4.2 JSON 직렬화 (인간이 읽기 쉬운 방식)**

```cpp
#include <iostream>
#include <string>
#include <sstream>

// 🎮 플레이어 데이터 구조체
struct Player {
    std::string name;
    int level;
    float x, y;
    int health;
    
    // 🔄 JSON으로 직렬화
    std::string ToJSON() const {
        std::ostringstream oss;
        oss << "{"
            << "\"name\":\"" << name << "\","
            << "\"level\":" << level << ","
            << "\"x\":" << x << ","
            << "\"y\":" << y << ","
            << "\"health\":" << health
            << "}";
        return oss.str();
    }
    
    // 🔄 JSON에서 역직렬화 (간단한 버전)
    static Player FromJSON(const std::string& json) {
        Player player;
        
        // 💡 실제로는 JSON 라이브러리를 사용하지만, 여기서는 간단한 파싱
        // 실제 프로젝트에서는 nlohmann/json 같은 라이브러리 사용 권장
        
        size_t name_start = json.find("\"name\":\"") + 8;
        size_t name_end = json.find("\"", name_start);
        player.name = json.substr(name_start, name_end - name_start);
        
        size_t level_start = json.find("\"level\":") + 8;
        size_t level_end = json.find(",", level_start);
        player.level = std::stoi(json.substr(level_start, level_end - level_start));
        
        // x, y, health 파싱 (생략... 실제로는 JSON 라이브러리 사용)
        player.x = 100.0f;  // 예시 값
        player.y = 200.0f;
        player.health = 1000;
        
        return player;
    }
};

// 🎮 게임 메시지 클래스
class GameMessage {
public:
    enum Type {
        LOGIN,
        LOGOUT,
        MOVE,
        ATTACK,
        CHAT
    };
    
    Type type;
    std::string sender;
    std::string data;
    
    std::string ToJSON() const {
        std::ostringstream oss;
        oss << "{"
            << "\"type\":\"" << TypeToString(type) << "\","
            << "\"sender\":\"" << sender << "\","
            << "\"data\":\"" << data << "\""
            << "}";
        return oss.str();
    }

private:
    std::string TypeToString(Type t) const {
        switch (t) {
            case LOGIN: return "login";
            case LOGOUT: return "logout";
            case MOVE: return "move";
            case ATTACK: return "attack";
            case CHAT: return "chat";
            default: return "unknown";
        }
    }
};

// 🎮 실제 사용 예시
int main() {
    // 플레이어 객체 생성
    Player alice{"Alice", 25, 150.5f, 200.3f, 1000};
    
    // JSON으로 직렬화
    std::string json_data = alice.ToJSON();
    std::cout << "📦 직렬화된 데이터:" << std::endl;
    std::cout << json_data << std::endl;
    
    // 이 데이터를 네트워크로 전송한다고 가정...
    std::cout << "\n🌐 네트워크 전송 중..." << std::endl;
    
    // 받은 쪽에서 역직렬화
    Player received_alice = Player::FromJSON(json_data);
    std::cout << "\n📬 역직렬화된 데이터:" << std::endl;
    std::cout << "이름: " << received_alice.name << std::endl;
    std::cout << "레벨: " << received_alice.level << std::endl;
    
    // 게임 메시지 예시
    std::cout << "\n💬 게임 메시지 예시:" << std::endl;
    
    GameMessage move_msg;
    move_msg.type = GameMessage::MOVE;
    move_msg.sender = "Alice";
    move_msg.data = "x:150.5,y:200.3";
    
    std::cout << move_msg.ToJSON() << std::endl;
    
    GameMessage chat_msg;
    chat_msg.type = GameMessage::CHAT;
    chat_msg.sender = "Alice";
    chat_msg.data = "안녕하세요!";
    
    std::cout << chat_msg.ToJSON() << std::endl;
    
    return 0;
}
```

### **4.3 바이너리 직렬화 (효율적인 방식)**

```cpp
#include <iostream>
#include <vector>
#include <cstring>

// 🎮 바이너리 직렬화 유틸리티
class BinarySerializer {
private:
    std::vector<uint8_t> buffer;
    size_t read_pos = 0;

public:
    // 🔢 기본 타입 직렬화
    void WriteInt(int value) {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
        for (size_t i = 0; i < sizeof(int); ++i) {
            buffer.push_back(bytes[i]);
        }
    }
    
    void WriteFloat(float value) {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&value);
        for (size_t i = 0; i < sizeof(float); ++i) {
            buffer.push_back(bytes[i]);
        }
    }
    
    void WriteString(const std::string& str) {
        // 문자열 길이 먼저 저장
        WriteInt(static_cast<int>(str.length()));
        
        // 문자열 데이터 저장
        for (char c : str) {
            buffer.push_back(static_cast<uint8_t>(c));
        }
    }
    
    // 🔍 역직렬화
    int ReadInt() {
        if (read_pos + sizeof(int) > buffer.size()) {
            throw std::runtime_error("버퍼 오버플로우!");
        }
        
        int value;
        memcpy(&value, &buffer[read_pos], sizeof(int));
        read_pos += sizeof(int);
        return value;
    }
    
    float ReadFloat() {
        if (read_pos + sizeof(float) > buffer.size()) {
            throw std::runtime_error("버퍼 오버플로우!");
        }
        
        float value;
        memcpy(&value, &buffer[read_pos], sizeof(float));
        read_pos += sizeof(float);
        return value;
    }
    
    std::string ReadString() {
        int length = ReadInt();
        
        if (read_pos + length > buffer.size()) {
            throw std::runtime_error("버퍼 오버플로우!");
        }
        
        std::string str(reinterpret_cast<const char*>(&buffer[read_pos]), length);
        read_pos += length;
        return str;
    }
    
    // 📦 데이터 접근
    const std::vector<uint8_t>& GetBuffer() const { return buffer; }
    size_t GetSize() const { return buffer.size(); }
    
    void SetBuffer(const std::vector<uint8_t>& data) {
        buffer = data;
        read_pos = 0;
    }
    
    void Clear() {
        buffer.clear();
        read_pos = 0;
    }
};

// 🎮 직렬화 가능한 플레이어 클래스
class SerializablePlayer {
public:
    std::string name;
    int level;
    float x, y;
    int health;
    
    SerializablePlayer() = default;
    SerializablePlayer(const std::string& n, int l, float px, float py, int h)
        : name(n), level(l), x(px), y(py), health(h) {}
    
    // 🔄 바이너리 직렬화
    std::vector<uint8_t> Serialize() const {
        BinarySerializer serializer;
        
        serializer.WriteString(name);
        serializer.WriteInt(level);
        serializer.WriteFloat(x);
        serializer.WriteFloat(y);
        serializer.WriteInt(health);
        
        return serializer.GetBuffer();
    }
    
    // 🔄 바이너리 역직렬화
    static SerializablePlayer Deserialize(const std::vector<uint8_t>& data) {
        BinarySerializer serializer;
        serializer.SetBuffer(data);
        
        SerializablePlayer player;
        player.name = serializer.ReadString();
        player.level = serializer.ReadInt();
        player.x = serializer.ReadFloat();
        player.y = serializer.ReadFloat();
        player.health = serializer.ReadInt();
        
        return player;
    }
    
    void Print() const {
        std::cout << "플레이어: " << name 
                  << " (레벨 " << level << ")"
                  << " 위치: (" << x << ", " << y << ")"
                  << " 체력: " << health << std::endl;
    }
};

// 🎮 성능 비교 테스트
int main() {
    SerializablePlayer original("DragonSlayer", 25, 150.5f, 200.3f, 1000);
    
    std::cout << "=== 원본 데이터 ===" << std::endl;
    original.Print();
    
    // 바이너리 직렬화
    auto binary_data = original.Serialize();
    std::cout << "\n📦 바이너리 직렬화:" << std::endl;
    std::cout << "크기: " << binary_data.size() << " 바이트" << std::endl;
    
    // JSON 직렬화 (비교용)
    std::string json_data = "{"
        "\"name\":\"DragonSlayer\","
        "\"level\":25,"
        "\"x\":150.5,"
        "\"y\":200.3,"
        "\"health\":1000"
        "}";
    std::cout << "JSON 크기: " << json_data.length() << " 바이트" << std::endl;
    
    // 바이너리 역직렬화
    SerializablePlayer deserialized = SerializablePlayer::Deserialize(binary_data);
    
    std::cout << "\n📬 역직렬화된 데이터:" << std::endl;
    deserialized.Print();
    
    // 정확성 검증
    bool is_correct = (original.name == deserialized.name &&
                      original.level == deserialized.level &&
                      std::abs(original.x - deserialized.x) < 0.001f &&
                      std::abs(original.y - deserialized.y) < 0.001f &&
                      original.health == deserialized.health);
    
    std::cout << "\n✅ 직렬화/역직렬화 " << (is_correct ? "성공!" : "실패!") << std::endl;
    
    // 💡 크기 비교 결과
    std::cout << "\n📊 크기 비교:" << std::endl;
    std::cout << "바이너리: " << binary_data.size() << " 바이트 (더 효율적!)" << std::endl;
    std::cout << "JSON: " << json_data.length() << " 바이트 (더 읽기 쉬움!)" << std::endl;
    
    return 0;
}
```

---

## 🎯 5. 실전 연습: 채팅 서버 만들기

```cpp
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <map>
#include <sstream>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define closesocket close
#endif

// 🎮 플레이어 정보
struct PlayerInfo {
    SOCKET socket;
    std::string name;
    std::string room;
    bool is_authenticated;
    
    PlayerInfo() : socket(INVALID_SOCKET), is_authenticated(false) {}
    PlayerInfo(SOCKET s) : socket(s), is_authenticated(false) {}
};

// 🎮 게임 채팅 서버
class GameChatServer {
private:
    SOCKET server_socket;
    std::map<SOCKET, PlayerInfo> players;
    std::mutex players_mutex;
    bool running;

public:
    GameChatServer() : running(false) {
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    }
    
    ~GameChatServer() {
        Stop();
#ifdef _WIN32
        WSACleanup();
#endif
    }
    
    bool Start(int port) {
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket == INVALID_SOCKET) {
            std::cerr << "❌ 소켓 생성 실패!" << std::endl;
            return false;
        }
        
        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);
        
        if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "❌ 바인딩 실패!" << std::endl;
            return false;
        }
        
        if (listen(server_socket, 10) < 0) {
            std::cerr << "❌ 리슨 실패!" << std::endl;
            return false;
        }
        
        running = true;
        std::cout << "💬 채팅 서버가 포트 " << port << "에서 시작됨!" << std::endl;
        std::cout << "📋 사용 가능한 명령어:" << std::endl;
        std::cout << "  /login <이름>    - 로그인" << std::endl;
        std::cout << "  /join <방이름>   - 방 입장" << std::endl;
        std::cout << "  /leave          - 방 떠나기" << std::endl;
        std::cout << "  /list           - 온라인 플레이어 목록" << std::endl;
        std::cout << "  /rooms          - 방 목록" << std::endl;
        std::cout << "  <메시지>        - 채팅" << std::endl;
        
        std::thread accept_thread(&GameChatServer::AcceptClients, this);
        accept_thread.detach();
        
        return true;
    }
    
    void Stop() {
        running = false;
        
        std::lock_guard<std::mutex> lock(players_mutex);
        for (auto& pair : players) {
            closesocket(pair.first);
        }
        players.clear();
        
        if (server_socket != INVALID_SOCKET) {
            closesocket(server_socket);
        }
        
        std::cout << "🛑 채팅 서버 종료됨" << std::endl;
    }

private:
    void AcceptClients() {
        while (running) {
            sockaddr_in client_addr{};
            socklen_t client_len = sizeof(client_addr);
            
            SOCKET client_socket = accept(server_socket, (sockaddr*)&client_addr, &client_len);
            
            if (client_socket != INVALID_SOCKET) {
                {
                    std::lock_guard<std::mutex> lock(players_mutex);
                    players[client_socket] = PlayerInfo(client_socket);
                }
                
                std::cout << "🔗 새로운 연결: " << inet_ntoa(client_addr.sin_addr) << std::endl;
                
                // 환영 메시지
                std::string welcome = "📢 게임 채팅 서버에 오신 것을 환영합니다!\n"
                                    "💡 먼저 '/login <이름>'으로 로그인해주세요.\n";
                send(client_socket, welcome.c_str(), welcome.length(), 0);
                
                std::thread client_thread(&GameChatServer::HandleClient, this, client_socket);
                client_thread.detach();
            }
        }
    }
    
    void HandleClient(SOCKET client_socket) {
        char buffer[1024];
        
        while (running) {
            int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            
            if (bytes_received <= 0) {
                break;  // 클라이언트 연결 해제
            }
            
            buffer[bytes_received] = '\0';
            std::string message(buffer);
            
            // 줄바꿈 문자 제거
            if (!message.empty() && message.back() == '\n') {
                message.pop_back();
            }
            if (!message.empty() && message.back() == '\r') {
                message.pop_back();
            }
            
            ProcessCommand(client_socket, message);
        }
        
        // 클라이언트 연결 해제 처리
        DisconnectClient(client_socket);
    }
    
    void ProcessCommand(SOCKET client_socket, const std::string& message) {
        if (message.empty()) return;
        
        std::lock_guard<std::mutex> lock(players_mutex);
        auto& player = players[client_socket];
        
        if (message[0] == '/') {
            // 명령어 처리
            std::istringstream iss(message);
            std::string command;
            iss >> command;
            
            if (command == "/login") {
                std::string name;
                iss >> name;
                
                if (name.empty()) {
                    SendToClient(client_socket, "❌ 사용법: /login <이름>\n");
                    return;
                }
                
                // 이름 중복 확인
                for (const auto& pair : players) {
                    if (pair.second.name == name && pair.first != client_socket) {
                        SendToClient(client_socket, "❌ 이미 사용 중인 이름입니다.\n");
                        return;
                    }
                }
                
                player.name = name;
                player.is_authenticated = true;
                
                SendToClient(client_socket, "✅ 로그인 성공! 환영합니다, " + name + "님!\n");
                std::cout << "👤 " << name << " 로그인" << std::endl;
                
            } else if (command == "/join") {
                if (!player.is_authenticated) {
                    SendToClient(client_socket, "❌ 먼저 로그인해주세요.\n");
                    return;
                }
                
                std::string room;
                iss >> room;
                
                if (room.empty()) {
                    SendToClient(client_socket, "❌ 사용법: /join <방이름>\n");
                    return;
                }
                
                std::string old_room = player.room;
                player.room = room;
                
                if (!old_room.empty()) {
                    BroadcastToRoom(old_room, "📤 " + player.name + "님이 방을 떠났습니다.\n", client_socket);
                }
                
                BroadcastToRoom(room, "📥 " + player.name + "님이 방에 입장했습니다.\n", client_socket);
                SendToClient(client_socket, "🚪 '" + room + "' 방에 입장했습니다.\n");
                
                std::cout << "🚪 " << player.name << " → " << room << " 방 입장" << std::endl;
                
            } else if (command == "/leave") {
                if (!player.is_authenticated || player.room.empty()) {
                    SendToClient(client_socket, "❌ 현재 방에 있지 않습니다.\n");
                    return;
                }
                
                BroadcastToRoom(player.room, "📤 " + player.name + "님이 방을 떠났습니다.\n", client_socket);
                SendToClient(client_socket, "🚪 '" + player.room + "' 방에서 나왔습니다.\n");
                
                std::cout << "🚪 " << player.name << " ← " << player.room << " 방 퇴장" << std::endl;
                player.room.clear();
                
            } else if (command == "/list") {
                std::string player_list = "👥 온라인 플레이어:\n";
                for (const auto& pair : players) {
                    if (pair.second.is_authenticated) {
                        player_list += "  - " + pair.second.name;
                        if (!pair.second.room.empty()) {
                            player_list += " (방: " + pair.second.room + ")";
                        }
                        player_list += "\n";
                    }
                }
                SendToClient(client_socket, player_list);
                
            } else if (command == "/rooms") {
                std::map<std::string, int> room_counts;
                for (const auto& pair : players) {
                    if (!pair.second.room.empty()) {
                        room_counts[pair.second.room]++;
                    }
                }
                
                std::string room_list = "🏠 방 목록:\n";
                for (const auto& room_pair : room_counts) {
                    room_list += "  - " + room_pair.first + " (" + std::to_string(room_pair.second) + "명)\n";
                }
                
                if (room_counts.empty()) {
                    room_list = "🏠 현재 생성된 방이 없습니다.\n";
                }
                
                SendToClient(client_socket, room_list);
                
            } else {
                SendToClient(client_socket, "❌ 알 수 없는 명령어입니다. 사용 가능한 명령어:\n"
                                          "  /login, /join, /leave, /list, /rooms\n");
            }
            
        } else {
            // 일반 채팅 메시지
            if (!player.is_authenticated) {
                SendToClient(client_socket, "❌ 먼저 로그인해주세요.\n");
                return;
            }
            
            if (player.room.empty()) {
                SendToClient(client_socket, "❌ 방에 입장한 후 채팅할 수 있습니다.\n");
                return;
            }
            
            std::string chat_message = "💬 " + player.name + ": " + message + "\n";
            BroadcastToRoom(player.room, chat_message, INVALID_SOCKET);  // 모든 사람에게 전송
            
            std::cout << "[" << player.room << "] " << player.name << ": " << message << std::endl;
        }
    }
    
    void SendToClient(SOCKET client_socket, const std::string& message) {
        send(client_socket, message.c_str(), message.length(), 0);
    }
    
    void BroadcastToRoom(const std::string& room, const std::string& message, SOCKET except_socket) {
        for (const auto& pair : players) {
            if (pair.second.room == room && pair.first != except_socket) {
                SendToClient(pair.first, message);
            }
        }
    }
    
    void DisconnectClient(SOCKET client_socket) {
        std::lock_guard<std::mutex> lock(players_mutex);
        
        auto it = players.find(client_socket);
        if (it != players.end()) {
            const auto& player = it->second;
            
            if (player.is_authenticated) {
                std::cout << "👋 " << player.name << " 연결 해제" << std::endl;
                
                if (!player.room.empty()) {
                    BroadcastToRoom(player.room, "📤 " + player.name + "님이 게임을 종료했습니다.\n", client_socket);
                }
            }
            
            players.erase(it);
        }
        
        closesocket(client_socket);
    }
};

// 🎮 테스트용 메인 함수
int main() {
    GameChatServer server;
    
    if (!server.Start(8080)) {
        return -1;
    }
    
    std::cout << "\n🎮 채팅 서버 실행 중..." << std::endl;
    std::cout << "📱 텔넷으로 테스트: telnet localhost 8080" << std::endl;
    std::cout << "🛑 종료하려면 'quit' 입력" << std::endl;
    
    std::string input;
    while (std::getline(std::cin, input)) {
        if (input == "quit" || input == "exit") {
            break;
        }
    }
    
    return 0;
}
```

---

## 📚 6. 네트워크 성능 최적화 기초

### **6.1 네트워크 지연(Latency) 이해**

```
🌐 네트워크 지연의 현실

서울 → 서울 (같은 도시): 1-5ms
서울 → 부산 (국내): 10-20ms  
서울 → 도쿄 (근거리 해외): 30-50ms
서울 → 미국 서부: 150-200ms
서울 → 유럽: 250-300ms

🎮 게임에서의 임팩트:
- 30ms 이하: 매우 좋음 (전혀 느껴지지 않음)
- 50ms 이하: 좋음 (거의 느껴지지 않음)
- 100ms 이하: 보통 (약간 느껴짐)
- 150ms 이상: 나쁨 (명확히 느껴짐)
- 300ms 이상: 매우 나쁨 (게임하기 어려움)
```

### **6.2 패킷 크기 최적화**

```cpp
#include <iostream>
#include <vector>
#include <chrono>

// ❌ 비효율적인 방식: 매번 개별 전송
class InefficientNetworking {
public:
    void SendPlayerPositions(const std::vector<Player>& players) {
        for (const auto& player : players) {
            // 각 플레이어마다 개별 패킷 전송 (비효율적!)
            std::string message = "MOVE " + player.name + " " + 
                                std::to_string(player.x) + " " + 
                                std::to_string(player.y);
            // SendPacket(message);  // 100명이면 100번의 패킷 전송!
        }
    }
};

// ✅ 효율적인 방식: 배치 전송
class EfficientNetworking {
public:
    struct BatchedMovement {
        struct PlayerMovement {
            uint32_t player_id;
            float x, y;
        };
        
        std::vector<PlayerMovement> movements;
        
        std::vector<uint8_t> Serialize() const {
            std::vector<uint8_t> data;
            
            // 플레이어 수 저장 (4바이트)
            uint32_t count = static_cast<uint32_t>(movements.size());
            data.insert(data.end(), reinterpret_cast<const uint8_t*>(&count), 
                       reinterpret_cast<const uint8_t*>(&count) + sizeof(count));
            
            // 각 플레이어 데이터 저장
            for (const auto& movement : movements) {
                data.insert(data.end(), reinterpret_cast<const uint8_t*>(&movement), 
                           reinterpret_cast<const uint8_t*>(&movement) + sizeof(movement));
            }
            
            return data;
        }
    };
    
    void SendPlayerPositions(const std::vector<Player>& players) {
        BatchedMovement batch;
        
        for (const auto& player : players) {
            batch.movements.push_back({
                static_cast<uint32_t>(std::hash<std::string>{}(player.name)),
                player.x, 
                player.y
            });
        }
        
        auto data = batch.Serialize();
        // SendPacket(data);  // 100명이어도 1번의 패킷 전송!
        
        std::cout << "📦 배치 전송: " << players.size() << "명의 위치를 " 
                  << data.size() << " 바이트로 전송" << std::endl;
    }
};

// 🎮 성능 비교 시뮬레이션
int main() {
    // 테스트용 플레이어 데이터
    std::vector<Player> players;
    for (int i = 0; i < 100; ++i) {
        players.emplace_back("Player" + std::to_string(i), 1, 
                           i * 10.0f, i * 5.0f, 1000);
    }
    
    std::cout << "🎮 네트워크 성능 비교 (100명의 플레이어)" << std::endl;
    
    // 비효율적인 방식
    auto start = std::chrono::high_resolution_clock::now();
    InefficientNetworking inefficient;
    for (int i = 0; i < 1000; ++i) {  // 1000번 반복
        inefficient.SendPlayerPositions(players);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto inefficient_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // 효율적인 방식
    start = std::chrono::high_resolution_clock::now();
    EfficientNetworking efficient;
    for (int i = 0; i < 1000; ++i) {  // 1000번 반복
        efficient.SendPlayerPositions(players);
    }
    end = std::chrono::high_resolution_clock::now();
    auto efficient_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "\n📊 성능 비교 결과:" << std::endl;
    std::cout << "❌ 개별 전송: " << inefficient_time.count() << " μs" << std::endl;
    std::cout << "✅ 배치 전송: " << efficient_time.count() << " μs" << std::endl;
    std::cout << "🚀 성능 향상: " << (inefficient_time.count() / efficient_time.count()) << "배" << std::endl;
    
    return 0;
}
```

---

## 🎯 7. 다음 단계를 위한 학습 가이드

### **7.1 필수 연습 과제**

1. **소켓 프로그래밍 기초**
   ```cpp
   // ✅ 해보세요: Echo 서버 구현
   // 클라이언트가 보낸 메시지를 그대로 다시 보내는 서버
   // TCP와 UDP 버전 모두 구현해보기
   ```

2. **프로토콜 설계**
   ```cpp
   // ✅ 해보세요: 간단한 게임 프로토콜 설계
   // 로그인, 이동, 공격, 채팅 메시지를 JSON으로 정의
   // 클라이언트-서버 간 메시지 교환 시나리오 작성
   ```

3. **직렬화 연습**
   ```cpp
   // ✅ 해보세요: 게임 상태 직렬화
   // 플레이어 인벤토리, 스킬, 퀘스트 진행도를 
   // JSON과 바이너리 방식으로 직렬화하고 성능 비교
   ```

### **7.2 실전 프로젝트 아이디어**

1. **간단한 멀티플레이어 게임**
   - 실시간 위치 동기화
   - 기본적인 채팅 시스템
   - 플레이어 입장/퇴장 처리

2. **네트워크 성능 측정 도구**
   - 레이턴시 측정
   - 패킷 손실률 계산
   - 처리량(Throughput) 측정

3. **프로토콜 버퍼 학습**
   - Google Protocol Buffers 사용
   - 스키마 정의 및 코드 생성
   - 성능 비교 (JSON vs ProtoBuf)

### **7.3 추천 학습 순서**

1. **1주차**: TCP/UDP 소켓 프로그래밍 기초, Echo 서버 구현
2. **2주차**: JSON 직렬화, 간단한 채팅 서버 구현
3. **3주차**: 바이너리 직렬화, 성능 최적화 기법 학습
4. **4주차**: 실전 프로젝트 - 간단한 멀티플레이어 게임 구현

### **7.4 유용한 도구 및 라이브러리**

```cpp
// 📚 추천 라이브러리

// 1. JSON 처리
#include <nlohmann/json.hpp>  // 가장 인기 있는 C++ JSON 라이브러리

// 2. 네트워크 라이브러리
#include <boost/asio.hpp>     // 고성능 비동기 네트워킹

// 3. 직렬화
#include <cereal/archives/binary.hpp>  // 직렬화 라이브러리
#include <google/protobuf/message.h>   // Google Protocol Buffers

// 4. 로깅
#include <spdlog/spdlog.h>    // 고성능 로깅 라이브러리
```

### **7.5 네트워크 디버깅 도구**

```bash
# 🔧 유용한 네트워크 도구들

# 포트 스캔 및 연결 테스트
telnet localhost 8080
nc localhost 8080

# 네트워크 트래픽 모니터링
wireshark                    # GUI 패킷 분석 도구
tcpdump -i any port 8080     # 커맨드라인 패킷 캡처

# 성능 테스트
iperf3 -s                    # 처리량 측정 서버
iperf3 -c <server_ip>        # 처리량 측정 클라이언트
```

---

## 🎯 마무리

이제 여러분은 **네트워크 프로그래밍의 기초**를 이해하고 실제 코드로 구현할 수 있습니다!

**✅ 지금까지 배운 것들:**
- 클라이언트-서버 모델의 동작 원리
- TCP vs UDP의 차이점과 게임에서의 선택 기준
- 소켓 프로그래밍 기본 개념과 실제 구현
- 직렬화/역직렬화 방법 (JSON, 바이너리)
- 네트워크 성능 최적화 기초

**🚀 다음 3단계에서 배울 내용:**
- **멀티스레딩 기초**: 여러 플레이어를 동시에 처리하는 방법
- **동기화 문제**: 경쟁 상태(Race Condition)와 해결책
- **비동기 프로그래밍**: Non-blocking I/O와 이벤트 루프

**💪 지금 할 수 있는 것들:**
- ✅ 간단한 TCP/UDP 서버 구현
- ✅ 클라이언트-서버 프로토콜 설계
- ✅ 게임 데이터 직렬화/역직렬화
- ✅ 기본적인 채팅 서버 구현

**계속해서 다음 단계로 진행할 준비가 되셨나요?** 🎮