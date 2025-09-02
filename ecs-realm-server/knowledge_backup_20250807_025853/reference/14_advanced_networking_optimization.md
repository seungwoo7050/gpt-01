# 17단계: 고급 네트워킹 최적화 - 초당 50만 패킷을 처리하는 괴물 서버 만들기
*일반 웹서버를 뛰어넘어 실시간 게임의 극한 성능 요구사항을 만족시키는 네트워킹 기술*

> **🎯 목표**: 5,000명이 동시에 60fps로 플레이해도 10ms 이하 지연시간을 유지하는 초고성능 네트워크 엔진 구축하기

---

## 📋 문서 정보

- **난이도**: ⚫ 전문가 (네트워킹의 최고 수준!)
- **예상 학습 시간**: 8-10시간 (이론 + 최적화 실습)
- **필요 선수 지식**: 
  - ✅ [1-16단계](./01_advanced_cpp_features.md) 모든 내용 완료
  - ✅ C++ 네트워크 프로그래밍 경험
  - ✅ 시스템 프로그래밍 기초
  - ✅ "epoll", "IOCP" 같은 용어를 들어본 적 있음
- **실습 환경**: Linux, 고성능 네트워크 카드
- **최종 결과물**: 초당 50만 패킷 처리하는 전문가급 네트워크 엔진!

---

## 🤔 왜 웹서버 수준의 네트워킹으로는 게임을 만들 수 없을까?

### **일반 웹서버 vs 게임 서버의 차이**

```cpp
// 😴 일반 웹서버 (HTTP)
Request  → [처리] → Response (끝)
- 연결 수명: 밀리초~초 단위
- 패킷 빈도: 요청당 1-10개
- 지연 허용: 100-1000ms
- 처리량: 1,000-10,000 RPS

// 🔥 게임 서버 (실시간)
Connection → [지속적 상태 동기화] → Always Connected
- 연결 수명: 시간 단위
- 패킷 빈도: 초당 30-60개
- 지연 허용: 16-50ms
- 처리량: 100,000-1,000,000 PPS (Packets Per Second)
```

### **성능 차이가 게임에 미치는 영향**

```cpp
// ❌ 일반적인 네트워킹 (Boost.Asio 기본 사용)
void HandleReceive(const boost::system::error_code& error, size_t bytes) {
    if (!error) {
        ProcessPacket(buffer_.data(), bytes);  // 패킷 하나씩 처리
        StartReceive();                        // 다음 패킷 대기
    }
}
// 성능: ~5,000 PPS, 지연 100ms+

// ✅ 최적화된 게임 서버 네트워킹
void HandleReceiveBatch(const boost::system::error_code& error, size_t bytes) {
    if (!error) {
        // 배치 처리로 시스템 콜 오버헤드 감소
        auto packets = ExtractPacketsBatch(buffer_.data(), bytes);
        ProcessPacketsBatch(packets);          // 여러 패킷 한번에 처리
        
        // 즉시 다음 배치 대기 (컨텍스트 스위칭 최소화)
        StartReceiveBatch();
    }
}
// 성능: ~500,000 PPS, 지연 10ms 이하
```

**실제 게임에서의 차이:**
- **5,000 PPS**: 100명 동시 플레이 (5fps 업데이트)
- **50,000 PPS**: 1,000명 동시 플레이 (30fps 업데이트)  
- **500,000 PPS**: 5,000명 동시 플레이 (60fps 업데이트) ← **목표**

---

## ⚡ 1. epoll/IOCP 고성능 I/O 마스터

### **1.1 기본 개념: 왜 select()는 느릴까?**

```cpp
// 🐌 전통적인 select() 방식 (O(n) 스케일링)
void OldSchoolNetworking() {
    fd_set read_fds;
    int max_fd = 0;
    
    while (true) {
        FD_ZERO(&read_fds);
        
        // 모든 소켓을 매번 등록 (O(n))
        for (int i = 0; i < client_sockets.size(); ++i) {
            FD_SET(client_sockets[i], &read_fds);
            max_fd = std::max(max_fd, client_sockets[i]);
        }
        
        // 커널이 모든 소켓을 검사 (O(n))
        int activity = select(max_fd + 1, &read_fds, nullptr, nullptr, nullptr);
        
        // 어떤 소켓이 활성화되었는지 다시 검사 (O(n))
        for (int i = 0; i <= max_fd; ++i) {
            if (FD_ISSET(i, &read_fds)) {
                HandleSocket(i);
            }
        }
    }
}
// 성능: 1,000개 소켓에서 급격히 성능 저하
```

### **1.2 Linux epoll - 이벤트 기반 고성능 I/O**

```cpp
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <iostream>
#include <vector>
#include <unordered_map>

class EpollGameServer {
private:
    int epoll_fd_;
    int listen_fd_;
    
    struct ClientSession {
        int socket_fd;
        std::vector<uint8_t> read_buffer;
        std::vector<uint8_t> write_buffer;
        size_t bytes_read = 0;
        bool write_pending = false;
        
        ClientSession(int fd) : socket_fd(fd) {
            read_buffer.resize(8192);
            write_buffer.reserve(8192);
        }
    };
    
    std::unordered_map<int, std::unique_ptr<ClientSession>> clients_;
    
public:
    EpollGameServer(int port) {
        // epoll 인스턴스 생성
        epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
        if (epoll_fd_ == -1) {
            throw std::runtime_error("epoll_create1 failed");
        }
        
        // 리스닝 소켓 생성
        listen_fd_ = CreateListenSocket(port);
        AddToEpoll(listen_fd_, EPOLLIN);
        
        std::cout << "🚀 epoll 게임 서버 시작 (포트: " << port << ")" << std::endl;
    }
    
    ~EpollGameServer() {
        close(epoll_fd_);
        close(listen_fd_);
    }
    
    void Run() {
        const int MAX_EVENTS = 1024;  // 한 번에 처리할 최대 이벤트 수
        struct epoll_event events[MAX_EVENTS];
        
        while (true) {
            // ✅ epoll_wait: O(1) 복잡도로 준비된 이벤트 가져오기
            int ready_count = epoll_wait(epoll_fd_, events, MAX_EVENTS, -1);
            
            if (ready_count == -1) {
                if (errno == EINTR) continue;  // 시그널 인터럽트
                throw std::runtime_error("epoll_wait failed");
            }
            
            // 준비된 이벤트들만 처리 (O(ready_count))
            for (int i = 0; i < ready_count; ++i) {
                const auto& event = events[i];
                int fd = event.data.fd;
                
                if (fd == listen_fd_) {
                    // 새로운 클라이언트 연결
                    HandleNewConnection();
                } else {
                    // 기존 클라이언트 이벤트
                    HandleClientEvent(fd, event.events);
                }
            }
            
            // 성능 모니터링
            if (ready_count > 0) {
                MonitorPerformance(ready_count);
            }
        }
    }

private:
    int CreateListenSocket(int port) {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd == -1) {
            throw std::runtime_error("socket creation failed");
        }
        
        // 소켓 옵션 최적화
        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
        
        // 논블로킹 모드 설정
        SetNonBlocking(fd);
        
        // TCP_NODELAY 설정 (Nagle 알고리즘 비활성화)
        setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
        
        // 바인드 및 리슨
        struct sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        
        if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
            close(fd);
            throw std::runtime_error("bind failed");
        }
        
        if (listen(fd, SOMAXCONN) == -1) {
            close(fd);
            throw std::runtime_error("listen failed");
        }
        
        return fd;
    }
    
    void SetNonBlocking(int fd) {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags == -1) {
            throw std::runtime_error("fcntl F_GETFL failed");
        }
        
        if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
            throw std::runtime_error("fcntl F_SETFL failed");
        }
    }
    
    void AddToEpoll(int fd, uint32_t events) {
        struct epoll_event event{};
        event.events = events;
        event.data.fd = fd;
        
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event) == -1) {
            throw std::runtime_error("epoll_ctl ADD failed");
        }
    }
    
    void ModifyEpoll(int fd, uint32_t events) {
        struct epoll_event event{};
        event.events = events;
        event.data.fd = fd;
        
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &event) == -1) {
            throw std::runtime_error("epoll_ctl MOD failed");
        }
    }
    
    void HandleNewConnection() {
        while (true) {
            struct sockaddr_in client_addr{};
            socklen_t addr_len = sizeof(client_addr);
            
            int client_fd = accept4(listen_fd_, 
                                   (struct sockaddr*)&client_addr, 
                                   &addr_len, 
                                   SOCK_NONBLOCK | SOCK_CLOEXEC);
            
            if (client_fd == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;  // 더 이상 대기 중인 연결 없음
                }
                std::cerr << "accept4 failed: " << strerror(errno) << std::endl;
                break;
            }
            
            // TCP 소켓 최적화
            int opt = 1;
            setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &opt, sizeof(opt));
            
            // 클라이언트 세션 생성
            auto session = std::make_unique<ClientSession>(client_fd);
            clients_[client_fd] = std::move(session);
            
            // epoll에 읽기 이벤트 등록
            AddToEpoll(client_fd, EPOLLIN | EPOLLET);  // Edge Triggered
            
            std::cout << "🔗 새 클라이언트 연결: " << client_fd 
                      << " (총 " << clients_.size() << "명)" << std::endl;
        }
    }
    
    void HandleClientEvent(int fd, uint32_t events) {
        auto it = clients_.find(fd);
        if (it == clients_.end()) {
            return;
        }
        
        ClientSession* session = it->second.get();
        
        if (events & EPOLLERR || events & EPOLLHUP) {
            // 연결 오류 또는 종료
            DisconnectClient(fd);
            return;
        }
        
        if (events & EPOLLIN) {
            // 읽기 가능
            HandleRead(session);
        }
        
        if (events & EPOLLOUT) {
            // 쓰기 가능
            HandleWrite(session);
        }
    }
    
    void HandleRead(ClientSession* session) {
        int fd = session->socket_fd;
        
        while (true) {
            ssize_t bytes = read(fd, 
                               session->read_buffer.data() + session->bytes_read,
                               session->read_buffer.size() - session->bytes_read);
            
            if (bytes > 0) {
                session->bytes_read += bytes;
                
                // 완전한 패킷이 있는지 확인하고 처리
                ProcessReceivedData(session);
                
            } else if (bytes == 0) {
                // 클라이언트가 연결 종료
                DisconnectClient(fd);
                break;
                
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 더 이상 읽을 데이터 없음 (정상)
                    break;
                } else {
                    // 읽기 오류
                    std::cerr << "read error: " << strerror(errno) << std::endl;
                    DisconnectClient(fd);
                    break;
                }
            }
        }
    }
    
    void HandleWrite(ClientSession* session) {
        int fd = session->socket_fd;
        
        if (session->write_buffer.empty()) {
            // 쓸 데이터가 없으면 EPOLLOUT 제거
            ModifyEpoll(fd, EPOLLIN | EPOLLET);
            session->write_pending = false;
            return;
        }
        
        ssize_t bytes = write(fd, 
                            session->write_buffer.data(), 
                            session->write_buffer.size());
        
        if (bytes > 0) {
            session->write_buffer.erase(session->write_buffer.begin(), 
                                      session->write_buffer.begin() + bytes);
            
            if (session->write_buffer.empty()) {
                // 모든 데이터 전송 완료
                ModifyEpoll(fd, EPOLLIN | EPOLLET);
                session->write_pending = false;
            }
            
        } else if (bytes == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                std::cerr << "write error: " << strerror(errno) << std::endl;
                DisconnectClient(fd);
            }
        }
    }
    
    void ProcessReceivedData(ClientSession* session) {
        // 패킷 헤더 크기 (4바이트 길이 + 2바이트 타입)
        const size_t HEADER_SIZE = 6;
        
        while (session->bytes_read >= HEADER_SIZE) {
            // 패킷 길이 추출 (Little Endian)
            uint32_t packet_length;
            memcpy(&packet_length, session->read_buffer.data(), 4);
            
            // 전체 패킷이 도착했는지 확인
            if (session->bytes_read < packet_length + 4) {
                break;  // 아직 완전한 패킷이 아님
            }
            
            // 패킷 타입 추출
            uint16_t packet_type;
            memcpy(&packet_type, session->read_buffer.data() + 4, 2);
            
            // 패킷 데이터 추출
            std::vector<uint8_t> packet_data(
                session->read_buffer.begin() + HEADER_SIZE,
                session->read_buffer.begin() + packet_length + 4
            );
            
            // 패킷 처리
            ProcessGamePacket(session, packet_type, packet_data);
            
            // 처리된 패킷 제거
            size_t total_packet_size = packet_length + 4;
            session->read_buffer.erase(
                session->read_buffer.begin(),
                session->read_buffer.begin() + total_packet_size
            );
            session->bytes_read -= total_packet_size;
        }
    }
    
    void ProcessGamePacket(ClientSession* session, uint16_t type, 
                          const std::vector<uint8_t>& data) {
        // 실제 게임 패킷 처리 로직
        switch (type) {
            case 1: // 플레이어 이동
                HandlePlayerMovement(session, data);
                break;
            case 2: // 채팅 메시지
                HandleChatMessage(session, data);
                break;
            default:
                std::cout << "알 수 없는 패킷 타입: " << type << std::endl;
                break;
        }
    }
    
    void SendToClient(ClientSession* session, const std::vector<uint8_t>& data) {
        if (!session->write_pending) {
            // 바로 전송 시도
            ssize_t bytes = write(session->socket_fd, data.data(), data.size());
            
            if (bytes == static_cast<ssize_t>(data.size())) {
                // 모든 데이터 전송 완료
                return;
            } else if (bytes > 0) {
                // 일부만 전송됨 - 나머지를 버퍼에 저장
                session->write_buffer.assign(data.begin() + bytes, data.end());
            } else {
                // 전송 실패 - 전체를 버퍼에 저장
                session->write_buffer = data;
            }
            
            // EPOLLOUT 이벤트 등록
            ModifyEpoll(session->socket_fd, EPOLLIN | EPOLLOUT | EPOLLET);
            session->write_pending = true;
            
        } else {
            // 이미 대기 중인 데이터가 있음 - 버퍼에 추가
            session->write_buffer.insert(session->write_buffer.end(), 
                                       data.begin(), data.end());
        }
    }
    
    void DisconnectClient(int fd) {
        auto it = clients_.find(fd);
        if (it != clients_.end()) {
            epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr);
            close(fd);
            clients_.erase(it);
            
            std::cout << "👋 클라이언트 연결 종료: " << fd 
                      << " (남은 클라이언트: " << clients_.size() << "명)" << std::endl;
        }
    }
    
    void HandlePlayerMovement(ClientSession* session, const std::vector<uint8_t>& data) {
        // 플레이어 이동 처리 예시
        if (data.size() >= 12) {  // x, y, z 좌표 (4바이트씩)
            float x, y, z;
            memcpy(&x, data.data(), 4);
            memcpy(&y, data.data() + 4, 4);
            memcpy(&z, data.data() + 8, 4);
            
            std::cout << "플레이어 이동: (" << x << ", " << y << ", " << z << ")" << std::endl;
            
            // 다른 클라이언트들에게 브로드캐스트
            BroadcastMovement(session, x, y, z);
        }
    }
    
    void HandleChatMessage(ClientSession* session, const std::vector<uint8_t>& data) {
        std::string message(data.begin(), data.end());
        std::cout << "채팅 메시지: " << message << std::endl;
        
        // 모든 클라이언트에게 브로드캐스트
        BroadcastChat(session, message);
    }
    
    void BroadcastMovement(ClientSession* sender, float x, float y, float z) {
        // 이동 패킷 생성 (타입 1, 12바이트 데이터)
        std::vector<uint8_t> packet;
        
        // 패킷 길이 (4바이트)
        uint32_t length = 14;  // 2바이트 타입 + 12바이트 좌표
        packet.insert(packet.end(), 
                     reinterpret_cast<uint8_t*>(&length),
                     reinterpret_cast<uint8_t*>(&length) + 4);
        
        // 패킷 타입 (2바이트)
        uint16_t type = 1;
        packet.insert(packet.end(),
                     reinterpret_cast<uint8_t*>(&type),
                     reinterpret_cast<uint8_t*>(&type) + 2);
        
        // 좌표 데이터 (12바이트)
        packet.insert(packet.end(),
                     reinterpret_cast<uint8_t*>(&x),
                     reinterpret_cast<uint8_t*>(&x) + 4);
        packet.insert(packet.end(),
                     reinterpret_cast<uint8_t*>(&y),
                     reinterpret_cast<uint8_t*>(&y) + 4);
        packet.insert(packet.end(),
                     reinterpret_cast<uint8_t*>(&z),
                     reinterpret_cast<uint8_t*>(&z) + 4);
        
        // 모든 클라이언트에게 전송 (발신자 제외)
        for (const auto& [fd, client] : clients_) {
            if (client.get() != sender) {
                SendToClient(client.get(), packet);
            }
        }
    }
    
    void BroadcastChat(ClientSession* sender, const std::string& message) {
        // 채팅 패킷 생성
        std::vector<uint8_t> packet;
        
        // 패킷 길이
        uint32_t length = 2 + message.size();  // 타입 + 메시지
        packet.insert(packet.end(),
                     reinterpret_cast<uint8_t*>(&length),
                     reinterpret_cast<uint8_t*>(&length) + 4);
        
        // 패킷 타입
        uint16_t type = 2;
        packet.insert(packet.end(),
                     reinterpret_cast<uint8_t*>(&type),
                     reinterpret_cast<uint8_t*>(&type) + 2);
        
        // 메시지 데이터
        packet.insert(packet.end(), message.begin(), message.end());
        
        // 모든 클라이언트에게 전송
        for (const auto& [fd, client] : clients_) {
            SendToClient(client.get(), packet);
        }
    }
    
    void MonitorPerformance(int events_processed) {
        static auto last_time = std::chrono::steady_clock::now();
        static int total_events = 0;
        total_events += events_processed;
        
        auto now = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_time);
        
        if (duration.count() >= 10) {  // 10초마다 통계 출력
            double events_per_sec = static_cast<double>(total_events) / duration.count();
            
            std::cout << "📊 성능 통계:" << std::endl;
            std::cout << "  이벤트/초: " << static_cast<int>(events_per_sec) << std::endl;
            std::cout << "  연결 수: " << clients_.size() << std::endl;
            std::cout << "  예상 PPS: " << static_cast<int>(events_per_sec * clients_.size()) << std::endl;
            
            last_time = now;
            total_events = 0;
        }
    }
};

// 사용 예시
int main() {
    try {
        EpollGameServer server(8080);
        server.Run();
    } catch (const std::exception& e) {
        std::cerr << "서버 오류: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
```

### **1.3 Windows IOCP - 완료 포트 기반 고성능 I/O**

```cpp
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <thread>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "mswsock.lib")

class IOCPGameServer {
private:
    HANDLE iocp_handle_;
    SOCKET listen_socket_;
    std::vector<std::thread> worker_threads_;
    
    enum class OperationType {
        ACCEPT,
        RECEIVE,
        SEND
    };
    
    struct OverlappedEx {
        OVERLAPPED overlapped;
        OperationType operation;
        SOCKET client_socket;
        WSABUF wsa_buf;
        std::vector<uint8_t> buffer;
        
        OverlappedEx(OperationType op, SOCKET sock = INVALID_SOCKET) 
            : operation(op), client_socket(sock) {
            ZeroMemory(&overlapped, sizeof(OVERLAPPED));
            buffer.resize(8192);
            wsa_buf.buf = reinterpret_cast<char*>(buffer.data());
            wsa_buf.len = static_cast<ULONG>(buffer.size());
        }
    };
    
    struct ClientSession {
        SOCKET socket;
        std::vector<uint8_t> recv_buffer;
        std::queue<std::vector<uint8_t>> send_queue;
        std::mutex send_mutex;
        bool send_pending = false;
        
        ClientSession(SOCKET s) : socket(s) {
            recv_buffer.reserve(8192);
        }
    };
    
    std::unordered_map<SOCKET, std::unique_ptr<ClientSession>> clients_;
    std::mutex clients_mutex_;
    
public:
    IOCPGameServer(int port) {
        // Winsock 초기화
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            throw std::runtime_error("WSAStartup failed");
        }
        
        // IOCP 핸들 생성
        iocp_handle_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
        if (iocp_handle_ == NULL) {
            throw std::runtime_error("CreateIoCompletionPort failed");
        }
        
        // 리스닝 소켓 생성
        CreateListenSocket(port);
        
        // 워커 스레드 생성 (CPU 코어 수의 2배)
        int thread_count = std::thread::hardware_concurrency() * 2;
        for (int i = 0; i < thread_count; ++i) {
            worker_threads_.emplace_back(&IOCPGameServer::WorkerThread, this);
        }
        
        std::cout << "🚀 IOCP 게임 서버 시작 (포트: " << port 
                  << ", 워커 스레드: " << thread_count << ")" << std::endl;
    }
    
    ~IOCPGameServer() {
        if (listen_socket_ != INVALID_SOCKET) {
            closesocket(listen_socket_);
        }
        
        if (iocp_handle_ != NULL) {
            CloseHandle(iocp_handle_);
        }
        
        WSACleanup();
    }
    
    void Run() {
        // 연속적으로 Accept 요청 발행
        for (int i = 0; i < 10; ++i) {
            PostAccept();
        }
        
        std::cout << "서버 실행 중... (Ctrl+C로 종료)" << std::endl;
        
        // 메인 스레드는 대기
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            PrintStatistics();
        }
    }

private:
    void CreateListenSocket(int port) {
        listen_socket_ = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 
                                  NULL, 0, WSA_FLAG_OVERLAPPED);
        
        if (listen_socket_ == INVALID_SOCKET) {
            throw std::runtime_error("WSASocket failed");
        }
        
        // 소켓 옵션 설정
        BOOL opt = TRUE;
        setsockopt(listen_socket_, SOL_SOCKET, SO_REUSEADDR, 
                  reinterpret_cast<char*>(&opt), sizeof(opt));
        setsockopt(listen_socket_, IPPROTO_TCP, TCP_NODELAY,
                  reinterpret_cast<char*>(&opt), sizeof(opt));
        
        // 바인드
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        
        if (bind(listen_socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
            throw std::runtime_error("bind failed");
        }
        
        if (listen(listen_socket_, SOMAXCONN) == SOCKET_ERROR) {
            throw std::runtime_error("listen failed");
        }
        
        // IOCP에 리스닝 소켓 연결
        if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(listen_socket_), 
                                  iocp_handle_, 0, 0) == NULL) {
            throw std::runtime_error("CreateIoCompletionPort for listen socket failed");
        }
    }
    
    void PostAccept() {
        SOCKET accept_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP,
                                        NULL, 0, WSA_FLAG_OVERLAPPED);
        
        if (accept_socket == INVALID_SOCKET) {
            std::cerr << "WSASocket for accept failed" << std::endl;
            return;
        }
        
        auto overlapped_ex = new OverlappedEx(OperationType::ACCEPT, accept_socket);
        
        DWORD bytes_received = 0;
        BOOL result = AcceptEx(listen_socket_, accept_socket,
                              overlapped_ex->buffer.data(),
                              0,  // 연결 즉시 수락 (데이터 대기 안함)
                              sizeof(sockaddr_in) + 16,
                              sizeof(sockaddr_in) + 16,
                              &bytes_received,
                              &overlapped_ex->overlapped);
        
        if (!result && WSAGetLastError() != WSA_IO_PENDING) {
            std::cerr << "AcceptEx failed: " << WSAGetLastError() << std::endl;
            closesocket(accept_socket);
            delete overlapped_ex;
        }
    }
    
    void PostReceive(ClientSession* session) {
        auto overlapped_ex = new OverlappedEx(OperationType::RECEIVE, session->socket);
        
        DWORD flags = 0;
        int result = WSARecv(session->socket, &overlapped_ex->wsa_buf, 1,
                            NULL, &flags, &overlapped_ex->overlapped, NULL);
        
        if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
            std::cerr << "WSARecv failed: " << WSAGetLastError() << std::endl;
            delete overlapped_ex;
            DisconnectClient(session->socket);
        }
    }
    
    void PostSend(ClientSession* session, const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(session->send_mutex);
        
        if (session->send_pending) {
            // 이미 전송 중이면 큐에 추가
            session->send_queue.push(data);
            return;
        }
        
        auto overlapped_ex = new OverlappedEx(OperationType::SEND, session->socket);
        overlapped_ex->buffer = data;
        overlapped_ex->wsa_buf.buf = reinterpret_cast<char*>(overlapped_ex->buffer.data());
        overlapped_ex->wsa_buf.len = static_cast<ULONG>(overlapped_ex->buffer.size());
        
        session->send_pending = true;
        
        int result = WSASend(session->socket, &overlapped_ex->wsa_buf, 1,
                            NULL, 0, &overlapped_ex->overlapped, NULL);
        
        if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
            std::cerr << "WSASend failed: " << WSAGetLastError() << std::endl;
            session->send_pending = false;
            delete overlapped_ex;
        }
    }
    
    void WorkerThread() {
        while (true) {
            DWORD bytes_transferred = 0;
            ULONG_PTR completion_key = 0;
            OVERLAPPED* overlapped = nullptr;
            
            // ✅ IOCP에서 완료된 I/O 작업 가져오기
            BOOL result = GetQueuedCompletionStatus(iocp_handle_, &bytes_transferred,
                                                   &completion_key, &overlapped, INFINITE);
            
            if (overlapped == nullptr) {
                // 서버 종료 신호
                break;
            }
            
            auto overlapped_ex = reinterpret_cast<OverlappedEx*>(overlapped);
            
            if (!result || bytes_transferred == 0) {
                // 연결 종료 또는 오류
                if (overlapped_ex->operation != OperationType::ACCEPT) {
                    DisconnectClient(overlapped_ex->client_socket);
                }
                delete overlapped_ex;
                continue;
            }
            
            // 작업 타입별 처리
            switch (overlapped_ex->operation) {
                case OperationType::ACCEPT:
                    HandleAcceptCompletion(overlapped_ex, bytes_transferred);
                    break;
                    
                case OperationType::RECEIVE:
                    HandleReceiveCompletion(overlapped_ex, bytes_transferred);
                    break;
                    
                case OperationType::SEND:
                    HandleSendCompletion(overlapped_ex, bytes_transferred);
                    break;
            }
            
            delete overlapped_ex;
        }
    }
    
    void HandleAcceptCompletion(OverlappedEx* overlapped_ex, DWORD bytes_transferred) {
        SOCKET client_socket = overlapped_ex->client_socket;
        
        // AcceptEx 소켓 옵션 상속
        if (setsockopt(client_socket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT,
                      reinterpret_cast<char*>(&listen_socket_), sizeof(listen_socket_)) == SOCKET_ERROR) {
            std::cerr << "setsockopt SO_UPDATE_ACCEPT_CONTEXT failed" << std::endl;
            closesocket(client_socket);
            PostAccept();  // 새로운 Accept 요청
            return;
        }
        
        // TCP_NODELAY 설정
        BOOL opt = TRUE;
        setsockopt(client_socket, IPPROTO_TCP, TCP_NODELAY,
                  reinterpret_cast<char*>(&opt), sizeof(opt));
        
        // IOCP에 클라이언트 소켓 연결
        if (CreateIoCompletionPort(reinterpret_cast<HANDLE>(client_socket),
                                  iocp_handle_, client_socket, 0) == NULL) {
            std::cerr << "CreateIoCompletionPort for client failed" << std::endl;
            closesocket(client_socket);
            PostAccept();
            return;
        }
        
        // 클라이언트 세션 생성
        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            auto session = std::make_unique<ClientSession>(client_socket);
            clients_[client_socket] = std::move(session);
        }
        
        std::cout << "🔗 새 클라이언트 연결: " << client_socket 
                  << " (총 " << clients_.size() << "명)" << std::endl;
        
        // 받기 요청 시작
        auto session_it = clients_.find(client_socket);
        if (session_it != clients_.end()) {
            PostReceive(session_it->second.get());
        }
        
        // 새로운 Accept 요청
        PostAccept();
    }
    
    void HandleReceiveCompletion(OverlappedEx* overlapped_ex, DWORD bytes_transferred) {
        SOCKET client_socket = overlapped_ex->client_socket;
        
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto session_it = clients_.find(client_socket);
        if (session_it == clients_.end()) {
            return;
        }
        
        ClientSession* session = session_it->second.get();
        
        // 받은 데이터를 버퍼에 추가
        session->recv_buffer.insert(session->recv_buffer.end(),
                                   overlapped_ex->buffer.begin(),
                                   overlapped_ex->buffer.begin() + bytes_transferred);
        
        // 패킷 처리
        ProcessReceivedData(session);
        
        // 다음 받기 요청
        PostReceive(session);
    }
    
    void HandleSendCompletion(OverlappedEx* overlapped_ex, DWORD bytes_transferred) {
        SOCKET client_socket = overlapped_ex->client_socket;
        
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto session_it = clients_.find(client_socket);
        if (session_it == clients_.end()) {
            return;
        }
        
        ClientSession* session = session_it->second.get();
        
        std::lock_guard<std::mutex> send_lock(session->send_mutex);
        session->send_pending = false;
        
        // 대기 중인 전송 데이터가 있으면 계속 전송
        if (!session->send_queue.empty()) {
            auto next_data = session->send_queue.front();
            session->send_queue.pop();
            send_lock.~lock_guard();  // 뮤텍스 해제
            
            PostSend(session, next_data);
        }
    }
    
    void ProcessReceivedData(ClientSession* session) {
        // epoll 버전과 동일한 패킷 처리 로직
        const size_t HEADER_SIZE = 6;
        
        while (session->recv_buffer.size() >= HEADER_SIZE) {
            uint32_t packet_length;
            memcpy(&packet_length, session->recv_buffer.data(), 4);
            
            if (session->recv_buffer.size() < packet_length + 4) {
                break;
            }
            
            uint16_t packet_type;
            memcpy(&packet_type, session->recv_buffer.data() + 4, 2);
            
            std::vector<uint8_t> packet_data(
                session->recv_buffer.begin() + HEADER_SIZE,
                session->recv_buffer.begin() + packet_length + 4
            );
            
            ProcessGamePacket(session, packet_type, packet_data);
            
            size_t total_packet_size = packet_length + 4;
            session->recv_buffer.erase(
                session->recv_buffer.begin(),
                session->recv_buffer.begin() + total_packet_size
            );
        }
    }
    
    void ProcessGamePacket(ClientSession* session, uint16_t type,
                          const std::vector<uint8_t>& data) {
        // 게임 패킷 처리 (epoll 버전과 동일)
        switch (type) {
            case 1: // 플레이어 이동
                HandlePlayerMovement(session, data);
                break;
            case 2: // 채팅 메시지
                HandleChatMessage(session, data);
                break;
        }
    }
    
    void HandlePlayerMovement(ClientSession* session, const std::vector<uint8_t>& data) {
        if (data.size() >= 12) {
            float x, y, z;
            memcpy(&x, data.data(), 4);
            memcpy(&y, data.data() + 4, 4);
            memcpy(&z, data.data() + 8, 4);
            
            // 다른 클라이언트들에게 브로드캐스트
            BroadcastMovement(session, x, y, z);
        }
    }
    
    void HandleChatMessage(ClientSession* session, const std::vector<uint8_t>& data) {
        std::string message(data.begin(), data.end());
        BroadcastChat(session, message);
    }
    
    void BroadcastMovement(ClientSession* sender, float x, float y, float z) {
        std::vector<uint8_t> packet;
        
        uint32_t length = 14;
        packet.insert(packet.end(),
                     reinterpret_cast<uint8_t*>(&length),
                     reinterpret_cast<uint8_t*>(&length) + 4);
        
        uint16_t type = 1;
        packet.insert(packet.end(),
                     reinterpret_cast<uint8_t*>(&type),
                     reinterpret_cast<uint8_t*>(&type) + 2);
        
        packet.insert(packet.end(),
                     reinterpret_cast<uint8_t*>(&x),
                     reinterpret_cast<uint8_t*>(&x) + 4);
        packet.insert(packet.end(),
                     reinterpret_cast<uint8_t*>(&y),
                     reinterpret_cast<uint8_t*>(&y) + 4);
        packet.insert(packet.end(),
                     reinterpret_cast<uint8_t*>(&z),
                     reinterpret_cast<uint8_t*>(&z) + 4);
        
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (const auto& [socket, client] : clients_) {
            if (client.get() != sender) {
                PostSend(client.get(), packet);
            }
        }
    }
    
    void BroadcastChat(ClientSession* sender, const std::string& message) {
        std::vector<uint8_t> packet;
        
        uint32_t length = 2 + message.size();
        packet.insert(packet.end(),
                     reinterpret_cast<uint8_t*>(&length),
                     reinterpret_cast<uint8_t*>(&length) + 4);
        
        uint16_t type = 2;
        packet.insert(packet.end(),
                     reinterpret_cast<uint8_t*>(&type),
                     reinterpret_cast<uint8_t*>(&type) + 2);
        
        packet.insert(packet.end(), message.begin(), message.end());
        
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (const auto& [socket, client] : clients_) {
            PostSend(client.get(), packet);
        }
    }
    
    void DisconnectClient(SOCKET client_socket) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        auto it = clients_.find(client_socket);
        if (it != clients_.end()) {
            closesocket(client_socket);
            clients_.erase(it);
            
            std::cout << "👋 클라이언트 연결 종료: " << client_socket
                      << " (남은 클라이언트: " << clients_.size() << "명)" << std::endl;
        }
    }
    
    void PrintStatistics() {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        std::cout << "📊 IOCP 서버 통계:" << std::endl;
        std::cout << "  연결된 클라이언트: " << clients_.size() << "명" << std::endl;
        std::cout << "  워커 스레드: " << worker_threads_.size() << "개" << std::endl;
    }
};

#endif  // _WIN32
```

### **1.4 성능 비교: select vs epoll vs IOCP**

```cpp
// 📊 성능 벤치마크 결과 (10,000개 동시 연결 기준)

struct PerformanceBenchmark {
    std::string method;
    int max_connections;
    int packets_per_second;
    double cpu_usage_percent;
    double memory_mb;
    double latency_ms;
};

std::vector<PerformanceBenchmark> benchmarks = {
    // 전통적인 방식
    {"select()", 1024, 5000, 95.0, 50, 200},
    {"poll()", 65536, 15000, 85.0, 100, 100},
    
    // 고성능 방식
    {"epoll (Linux)", 1000000, 500000, 25.0, 200, 10},
    {"IOCP (Windows)", 1000000, 600000, 30.0, 250, 8},
    
    // 하이브리드 방식
    {"epoll + 배칭", 1000000, 800000, 20.0, 180, 5}
};

void PrintBenchmarkResults() {
    std::cout << "📊 네트워킹 방식별 성능 비교 (10,000 동시 연결)" << std::endl;
    std::cout << "방식\t\t최대연결\t\tPPS\t\tCPU%\t메모리(MB)\t지연(ms)" << std::endl;
    std::cout << "=================================================================" << std::endl;
    
    for (const auto& bench : benchmarks) {
        printf("%-15s\t%d\t\t%d\t%.1f%%\t%.0f\t\t%.1f\n",
               bench.method.c_str(), bench.max_connections, bench.packets_per_second,
               bench.cpu_usage_percent, bench.memory_mb, bench.latency_ms);
    }
    
    std::cout << "\n🎯 결론:" << std::endl;
    std::cout << "- 게임 서버에는 epoll(Linux) 또는 IOCP(Windows) 필수" << std::endl;
    std::cout << "- 배칭 기법으로 추가 성능 향상 가능 (2배)" << std::endl;
    std::cout << "- select/poll은 프로토타입 용도로만 사용" << std::endl;
}
```

---

## 🚀 2. 패킷 배칭 및 압축 최적화

### **2.1 패킷 배칭 - 시스템 콜 오버헤드 감소**

```cpp
#include <vector>
#include <chrono>
#include <algorithm>

class PacketBatcher {
private:
    struct BatchedPacket {
        std::vector<uint8_t> data;
        std::chrono::steady_clock::time_point timestamp;
        int priority;  // 0: 높음, 1: 보통, 2: 낮음
        
        BatchedPacket(std::vector<uint8_t> d, int p = 1) 
            : data(std::move(d)), timestamp(std::chrono::steady_clock::now()), priority(p) {}
    };
    
    std::vector<BatchedPacket> pending_packets_;
    std::mutex batch_mutex_;
    
    // 배칭 설정
    static constexpr size_t MAX_BATCH_SIZE = 1400;      // MTU 고려 (1500바이트)
    static constexpr size_t MAX_PACKETS_PER_BATCH = 50; // 패킷 개수 제한
    static constexpr int BATCH_TIMEOUT_MS = 5;          // 최대 대기 시간
    
public:
    // 패킷을 배치에 추가
    void AddPacket(std::vector<uint8_t> packet_data, int priority = 1) {
        std::lock_guard<std::mutex> lock(batch_mutex_);
        
        pending_packets_.emplace_back(std::move(packet_data), priority);
        
        // 조건을 만족하면 즉시 플러시
        if (ShouldFlush()) {
            FlushBatch();
        }
    }
    
    // 강제로 배치 전송
    void ForceBatch() {
        std::lock_guard<std::mutex> lock(batch_mutex_);
        if (!pending_packets_.empty()) {
            FlushBatch();
        }
    }
    
    // 주기적으로 호출하여 타임아웃된 패킷 처리
    void ProcessTimeouts() {
        std::lock_guard<std::mutex> lock(batch_mutex_);
        
        if (pending_packets_.empty()) return;
        
        auto now = std::chrono::steady_clock::now();
        auto oldest_time = pending_packets_.front().timestamp;
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - oldest_time);
        
        if (elapsed.count() >= BATCH_TIMEOUT_MS) {
            FlushBatch();
        }
    }

private:
    bool ShouldFlush() const {
        if (pending_packets_.empty()) return false;
        
        // 패킷 개수 확인
        if (pending_packets_.size() >= MAX_PACKETS_PER_BATCH) {
            return true;
        }
        
        // 전체 크기 확인
        size_t total_size = 0;
        for (const auto& packet : pending_packets_) {
            total_size += packet.data.size();
        }
        
        return total_size >= MAX_BATCH_SIZE;
    }
    
    void FlushBatch() {
        if (pending_packets_.empty()) return;
        
        // 우선순위별로 정렬 (높은 우선순위 먼저)
        std::sort(pending_packets_.begin(), pending_packets_.end(),
                  [](const BatchedPacket& a, const BatchedPacket& b) {
                      return a.priority < b.priority;
                  });
        
        // 배치된 패킷 생성
        auto batched_data = CreateBatchedPacket();
        
        // 실제 전송
        SendBatchedPacket(batched_data);
        
        // 배치 초기화
        pending_packets_.clear();
        
        std::cout << "📦 배치 전송 완료: " << batched_data.size() << " 바이트" << std::endl;
    }
    
    std::vector<uint8_t> CreateBatchedPacket() {
        std::vector<uint8_t> batched_data;
        
        // 배치 헤더: 패킷 개수 (4바이트)
        uint32_t packet_count = static_cast<uint32_t>(pending_packets_.size());
        batched_data.insert(batched_data.end(),
                           reinterpret_cast<uint8_t*>(&packet_count),
                           reinterpret_cast<uint8_t*>(&packet_count) + 4);
        
        // 각 패킷을 길이와 함께 추가
        for (const auto& packet : pending_packets_) {
            // 개별 패킷 길이 (4바이트)
            uint32_t packet_length = static_cast<uint32_t>(packet.data.size());
            batched_data.insert(batched_data.end(),
                               reinterpret_cast<uint8_t*>(&packet_length),
                               reinterpret_cast<uint8_t*>(&packet_length) + 4);
            
            // 패킷 데이터
            batched_data.insert(batched_data.end(), 
                               packet.data.begin(), packet.data.end());
        }
        
        return batched_data;
    }
    
    void SendBatchedPacket(const std::vector<uint8_t>& data) {
        // 실제 네트워크 전송 로직
        // (여기서는 시뮬레이션)
        std::cout << "🚀 " << pending_packets_.size() << "개 패킷을 " 
                  << data.size() << "바이트로 배치 전송" << std::endl;
    }
};

// 사용 예시
void PacketBatchingDemo() {
    PacketBatcher batcher;
    
    std::cout << "=== 패킷 배칭 데모 ===" << std::endl;
    
    // 여러 패킷 추가
    for (int i = 0; i < 10; ++i) {
        std::vector<uint8_t> packet = {
            static_cast<uint8_t>(i), 0x01, 0x02, 0x03, 0x04
        };
        
        int priority = (i < 3) ? 0 : 1;  // 처음 3개는 높은 우선순위
        batcher.AddPacket(packet, priority);
        
        // 약간의 지연
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // 강제 플러시
    batcher.ForceBatch();
}
```

### **2.2 패킷 압축 - 대역폭 최적화**

```cpp
#include <zlib.h>
#include <vector>
#include <iostream>
#include <chrono>

class PacketCompressor {
private:
    // 압축 레벨 (1: 빠름, 9: 높은 압축률)
    static constexpr int COMPRESSION_LEVEL = 6;
    static constexpr size_t MIN_COMPRESSION_SIZE = 64;  // 작은 패킷은 압축 안함
    
public:
    struct CompressionResult {
        std::vector<uint8_t> data;
        bool is_compressed;
        float compression_ratio;
        std::chrono::microseconds compression_time;
    };
    
    static CompressionResult CompressPacket(const std::vector<uint8_t>& input) {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        CompressionResult result;
        result.is_compressed = false;
        result.compression_ratio = 1.0f;
        
        // 작은 패킷은 압축하지 않음
        if (input.size() < MIN_COMPRESSION_SIZE) {
            result.data = input;
            result.compression_time = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::high_resolution_clock::now() - start_time);
            return result;
        }
        
        // zlib을 사용한 압축
        uLongf compressed_size = compressBound(static_cast<uLong>(input.size()));
        std::vector<uint8_t> compressed_data(compressed_size);
        
        int compress_result = compress2(compressed_data.data(), &compressed_size,
                                       input.data(), static_cast<uLong>(input.size()),
                                       COMPRESSION_LEVEL);
        
        if (compress_result == Z_OK) {
            compressed_data.resize(compressed_size);
            
            // 압축률 계산
            float ratio = static_cast<float>(compressed_size) / input.size();
            
            // 압축이 효과적인 경우만 사용 (10% 이상 절약)
            if (ratio < 0.9f) {
                result.data = std::move(compressed_data);
                result.is_compressed = true;
                result.compression_ratio = ratio;
            } else {
                result.data = input;  // 압축 효과 없음
            }
        } else {
            result.data = input;  // 압축 실패
        }
        
        result.compression_time = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - start_time);
        
        return result;
    }
    
    static std::vector<uint8_t> DecompressPacket(const std::vector<uint8_t>& compressed_data,
                                                 size_t original_size) {
        std::vector<uint8_t> decompressed_data(original_size);
        uLongf decompressed_size = static_cast<uLongf>(original_size);
        
        int result = uncompress(decompressed_data.data(), &decompressed_size,
                               compressed_data.data(), static_cast<uLong>(compressed_data.size()));
        
        if (result != Z_OK) {
            throw std::runtime_error("Decompression failed");
        }
        
        decompressed_data.resize(decompressed_size);
        return decompressed_data;
    }
    
    // 게임 데이터별 압축 효과 테스트
    static void TestCompressionEfficiency() {
        std::cout << "=== 패킷 압축 효율성 테스트 ===" << std::endl;
        
        // 1. 플레이어 위치 데이터 (반복성 낮음)
        std::vector<uint8_t> position_data;
        for (int i = 0; i < 100; ++i) {
            float x = 100.0f + i * 0.1f;
            float y = 200.0f + i * 0.2f;
            float z = 50.0f + i * 0.05f;
            
            position_data.insert(position_data.end(),
                                reinterpret_cast<uint8_t*>(&x),
                                reinterpret_cast<uint8_t*>(&x) + 4);
            position_data.insert(position_data.end(),
                                reinterpret_cast<uint8_t*>(&y),
                                reinterpret_cast<uint8_t*>(&y) + 4);
            position_data.insert(position_data.end(),
                                reinterpret_cast<uint8_t*>(&z),
                                reinterpret_cast<uint8_t*>(&z) + 4);
        }
        
        auto pos_result = CompressPacket(position_data);
        std::cout << "위치 데이터: " << position_data.size() << " → " 
                  << pos_result.data.size() << " 바이트 ("
                  << static_cast<int>(pos_result.compression_ratio * 100) << "%)" << std::endl;
        
        // 2. 채팅 메시지 (텍스트, 압축률 높음)
        std::string chat_message = "안녕하세요! 오늘 날씨가 정말 좋네요. "
                                  "같이 던전 가실 분 계신가요? "
                                  "레벨 50 이상이면 환영합니다! "
                                  "함께 즐거운 게임 하세요~";
        std::vector<uint8_t> chat_data(chat_message.begin(), chat_message.end());
        
        auto chat_result = CompressPacket(chat_data);
        std::cout << "채팅 데이터: " << chat_data.size() << " → " 
                  << chat_result.data.size() << " 바이트 ("
                  << static_cast<int>(chat_result.compression_ratio * 100) << "%)" << std::endl;
        
        // 3. 인벤토리 데이터 (반복 패턴 많음)
        std::vector<uint8_t> inventory_data;
        for (int slot = 0; slot < 50; ++slot) {
            uint32_t item_id = (slot < 10) ? 1001 : 0;  // 처음 10개 슬롯만 아이템
            uint32_t quantity = (item_id > 0) ? 1 : 0;
            uint32_t enhancement = 0;
            
            inventory_data.insert(inventory_data.end(),
                                reinterpret_cast<uint8_t*>(&item_id),
                                reinterpret_cast<uint8_t*>(&item_id) + 4);
            inventory_data.insert(inventory_data.end(),
                                reinterpret_cast<uint8_t*>(&quantity),
                                reinterpret_cast<uint8_t*>(&quantity) + 4);
            inventory_data.insert(inventory_data.end(),
                                reinterpret_cast<uint8_t*>(&enhancement),
                                reinterpret_cast<uint8_t*>(&enhancement) + 4);
        }
        
        auto inv_result = CompressPacket(inventory_data);
        std::cout << "인벤토리: " << inventory_data.size() << " → " 
                  << inv_result.data.size() << " 바이트 ("
                  << static_cast<int>(inv_result.compression_ratio * 100) << "%)" << std::endl;
        
        std::cout << "\n💡 결론:" << std::endl;
        std::cout << "- 텍스트 데이터: 압축 효과 매우 높음 (50-70% 절약)" << std::endl;
        std::cout << "- 반복 패턴 데이터: 압축 효과 높음 (30-50% 절약)" << std::endl;
        std::cout << "- 실시간 위치 데이터: 압축 효과 낮음 (0-20% 절약)" << std::endl;
    }
};

// 압축 전략 매니저
class CompressionStrategy {
public:
    enum class PacketType {
        REALTIME_MOVEMENT,  // 실시간 이동 (압축 안함)
        CHAT_MESSAGE,       // 채팅 (항상 압축)
        INVENTORY_UPDATE,   // 인벤토리 (조건부 압축)
        BULK_DATA          // 대용량 데이터 (항상 압축)
    };
    
    static bool ShouldCompress(PacketType type, size_t packet_size) {
        switch (type) {
            case PacketType::REALTIME_MOVEMENT:
                return false;  // 지연 시간 우선
                
            case PacketType::CHAT_MESSAGE:
                return packet_size > 32;  // 짧은 메시지는 압축 안함
                
            case PacketType::INVENTORY_UPDATE:
                return packet_size > 100;  // 중간 크기 이상만 압축
                
            case PacketType::BULK_DATA:
                return true;  // 항상 압축
        }
        
        return packet_size > 64;  // 기본 임계값
    }
    
    static PacketCompressor::CompressionResult ProcessPacket(
        PacketType type, const std::vector<uint8_t>& data) {
        
        if (ShouldCompress(type, data.size())) {
            return PacketCompressor::CompressPacket(data);
        } else {
            PacketCompressor::CompressionResult result;
            result.data = data;
            result.is_compressed = false;
            result.compression_ratio = 1.0f;
            result.compression_time = std::chrono::microseconds(0);
            return result;
        }
    }
};
```

### **2.3 압축된 배치 패킷 - 궁극의 최적화**

```cpp
class OptimizedPacketManager {
private:
    PacketBatcher batcher_;
    
    struct PacketStats {
        size_t total_packets = 0;
        size_t total_bytes_original = 0;
        size_t total_bytes_compressed = 0;
        size_t total_bytes_sent = 0;
        std::chrono::microseconds total_compression_time{0};
    };
    
    PacketStats stats_;
    std::mutex stats_mutex_;
    
public:
    void SendOptimizedPacket(CompressionStrategy::PacketType type,
                            const std::vector<uint8_t>& data, int priority = 1) {
        
        // 1단계: 개별 패킷 압축 (타입별 전략 적용)
        auto compression_result = CompressionStrategy::ProcessPacket(type, data);
        
        // 2단계: 배치에 추가
        auto final_packet = CreateOptimizedPacket(compression_result, type);
        batcher_.AddPacket(std::move(final_packet), priority);
        
        // 3단계: 통계 업데이트
        UpdateStats(data.size(), compression_result);
    }
    
    void FlushAll() {
        batcher_.ForceBatch();
    }
    
    void PrintOptimizationStats() {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        
        std::cout << "📊 최적화 통계:" << std::endl;
        std::cout << "총 패킷 수: " << stats_.total_packets << "개" << std::endl;
        std::cout << "원본 크기: " << stats_.total_bytes_original << " 바이트" << std::endl;
        std::cout << "압축 후: " << stats_.total_bytes_compressed << " 바이트" << std::endl;
        std::cout << "전송 크기: " << stats_.total_bytes_sent << " 바이트" << std::endl;
        
        float compression_ratio = static_cast<float>(stats_.total_bytes_compressed) / 
                                 stats_.total_bytes_original;
        float batching_ratio = static_cast<float>(stats_.total_bytes_sent) / 
                              stats_.total_bytes_compressed;
        float total_ratio = static_cast<float>(stats_.total_bytes_sent) / 
                           stats_.total_bytes_original;
        
        std::cout << "압축률: " << static_cast<int>(compression_ratio * 100) << "%" << std::endl;
        std::cout << "배칭 효율: " << static_cast<int>(batching_ratio * 100) << "%" << std::endl;
        std::cout << "전체 효율: " << static_cast<int>(total_ratio * 100) << "%" << std::endl;
        
        double avg_compression_time = static_cast<double>(stats_.total_compression_time.count()) /
                                     stats_.total_packets;
        std::cout << "평균 압축 시간: " << avg_compression_time << " μs" << std::endl;
        
        // 대역폭 절약 계산
        size_t bytes_saved = stats_.total_bytes_original - stats_.total_bytes_sent;
        std::cout << "대역폭 절약: " << bytes_saved << " 바이트 (" 
                  << static_cast<int>((1.0f - total_ratio) * 100) << "%)" << std::endl;
    }

private:
    std::vector<uint8_t> CreateOptimizedPacket(
        const PacketCompressor::CompressionResult& compression_result,
        CompressionStrategy::PacketType type) {
        
        std::vector<uint8_t> optimized_packet;
        
        // 최적화된 패킷 헤더 (6바이트)
        struct OptimizedHeader {
            uint16_t packet_type;
            uint8_t flags;           // bit 0: compressed, bit 1-7: reserved
            uint8_t reserved;
            uint16_t data_length;
        } header{};
        
        header.packet_type = static_cast<uint16_t>(type);
        header.flags = compression_result.is_compressed ? 0x01 : 0x00;
        header.data_length = static_cast<uint16_t>(compression_result.data.size());
        
        // 헤더 추가
        optimized_packet.insert(optimized_packet.end(),
                               reinterpret_cast<uint8_t*>(&header),
                               reinterpret_cast<uint8_t*>(&header) + sizeof(header));
        
        // 데이터 추가
        optimized_packet.insert(optimized_packet.end(),
                               compression_result.data.begin(),
                               compression_result.data.end());
        
        return optimized_packet;
    }
    
    void UpdateStats(size_t original_size, 
                    const PacketCompressor::CompressionResult& result) {
        std::lock_guard<std::mutex> lock(stats_mutex_);
        
        stats_.total_packets++;
        stats_.total_bytes_original += original_size;
        stats_.total_bytes_compressed += result.data.size();
        stats_.total_bytes_sent += result.data.size() + 6;  // 헤더 포함
        stats_.total_compression_time += result.compression_time;
    }
};

// 실전 사용 예시
void OptimizationDemo() {
    OptimizedPacketManager manager;
    
    std::cout << "=== 종합 최적화 데모 ===" << std::endl;
    
    // 다양한 타입의 패킷 전송
    for (int i = 0; i < 100; ++i) {
        // 실시간 이동 패킷 (압축 안함)
        std::vector<uint8_t> movement_data = {
            0x01, 0x02, 0x03, 0x04,  // x 좌표
            0x05, 0x06, 0x07, 0x08,  // y 좌표
            0x09, 0x0A, 0x0B, 0x0C   // z 좌표
        };
        manager.SendOptimizedPacket(CompressionStrategy::PacketType::REALTIME_MOVEMENT,
                                   movement_data, 0);  // 높은 우선순위
        
        // 채팅 메시지 (압축함)
        if (i % 10 == 0) {
            std::string chat = "안녕하세요! 게임을 함께 즐겨봐요~ 레벨업 축하드립니다!";
            std::vector<uint8_t> chat_data(chat.begin(), chat.end());
            manager.SendOptimizedPacket(CompressionStrategy::PacketType::CHAT_MESSAGE,
                                       chat_data, 1);  // 보통 우선순위
        }
        
        // 인벤토리 업데이트 (조건부 압축)
        if (i % 25 == 0) {
            std::vector<uint8_t> inventory_data(200, 0x00);  // 큰 데이터
            manager.SendOptimizedPacket(CompressionStrategy::PacketType::INVENTORY_UPDATE,
                                       inventory_data, 2);  // 낮은 우선순위
        }
    }
    
    manager.FlushAll();
    manager.PrintOptimizationStats();
}
```

---

## 🌐 3. UDP vs TCP 하이브리드 구조

### **3.1 게임에서 UDP vs TCP 선택 기준**

```cpp
// 📊 프로토콜 선택 가이드

struct ProtocolCharacteristics {
    std::string protocol;
    bool reliable;
    bool ordered;
    bool connection_based;
    int overhead_bytes;
    double latency_ms;
    std::string best_use_case;
};

std::vector<ProtocolCharacteristics> protocols = {
    {"TCP", true, true, true, 20, 10.0, "중요한 데이터, 로그인, 채팅"},
    {"UDP", false, false, false, 8, 2.0, "실시간 위치, 즉시 액션"},
    {"TCP + Nagle Off", true, true, true, 20, 5.0, "게임 서버 표준"},
    {"UDP + 커스텀 신뢰성", true, false, false, 12, 3.0, "최적화된 게임"},
    {"QUIC (UDP 기반)", true, true, true, 16, 4.0, "차세대 프로토콜"}
};

// 게임 데이터별 프로토콜 추천
enum class GameDataType {
    PLAYER_LOGIN,        // TCP: 신뢰성 필수
    PLAYER_MOVEMENT,     // UDP: 지연시간 우선
    CHAT_MESSAGE,        // TCP: 메시지 손실 불허
    SKILL_ACTIVATION,    // UDP + ACK: 빠르지만 확실히
    INVENTORY_UPDATE,    // TCP: 아이템 손실 방지
    HEARTBEAT,          // UDP: 가볍고 빠르게
    WORLD_STATE_SYNC,   // UDP: 최신 상태만 중요
    TRANSACTION,        // TCP: 거래 보안
    VOICE_CHAT,         // UDP: 실시간 우선
    FILE_TRANSFER       // TCP: 완전성 보장
};

class ProtocolSelector {
public:
    static std::string GetRecommendedProtocol(GameDataType data_type) {
        switch (data_type) {
            case GameDataType::PLAYER_LOGIN:
            case GameDataType::CHAT_MESSAGE:
            case GameDataType::INVENTORY_UPDATE:
            case GameDataType::TRANSACTION:
            case GameDataType::FILE_TRANSFER:
                return "TCP";
                
            case GameDataType::PLAYER_MOVEMENT:
            case GameDataType::HEARTBEAT:
            case GameDataType::WORLD_STATE_SYNC:
            case GameDataType::VOICE_CHAT:
                return "UDP";
                
            case GameDataType::SKILL_ACTIVATION:
                return "UDP + Custom ACK";
        }
        return "TCP";  // 기본값
    }
    
    static void PrintRecommendations() {
        std::cout << "🎯 게임 데이터별 프로토콜 추천:" << std::endl;
        std::cout << "데이터 타입\t\t\t추천 프로토콜\t\t이유" << std::endl;
        std::cout << "========================================================================" << std::endl;
        
        struct DataTypeInfo {
            GameDataType type;
            std::string name;
            std::string reason;
        };
        
        std::vector<DataTypeInfo> data_types = {
            {GameDataType::PLAYER_LOGIN, "플레이어 로그인", "보안과 신뢰성 필수"},
            {GameDataType::PLAYER_MOVEMENT, "플레이어 이동", "지연시간 최소화"},
            {GameDataType::CHAT_MESSAGE, "채팅 메시지", "메시지 손실 방지"},
            {GameDataType::SKILL_ACTIVATION, "스킬 발동", "빠르지만 확실한 전달"},
            {GameDataType::INVENTORY_UPDATE, "인벤토리 업데이트", "아이템 손실 방지"},
            {GameDataType::HEARTBEAT, "하트비트", "가벼운 연결 확인"},
            {GameDataType::WORLD_STATE_SYNC, "월드 상태 동기화", "최신 상태만 중요"},
            {GameDataType::TRANSACTION, "거래", "데이터 무결성"},
            {GameDataType::VOICE_CHAT, "음성 채팅", "실시간성 우선"},
            {GameDataType::FILE_TRANSFER, "파일 전송", "완전성 보장"}
        };
        
        for (const auto& info : data_types) {
            std::string protocol = GetRecommendedProtocol(info.type);
            printf("%-20s\t%-15s\t%s\n", 
                   info.name.c_str(), protocol.c_str(), info.reason.c_str());
        }
    }
};
```

### **3.2 하이브리드 게임 서버 구현**

```cpp
#include <thread>
#include <unordered_set>

class HybridGameServer {
private:
    // TCP 서버 (신뢰성 필요한 데이터)
    std::unique_ptr<EpollGameServer> tcp_server_;
    
    // UDP 서버 (실시간 데이터)
    int udp_socket_;
    std::thread udp_thread_;
    
    // 클라이언트 매핑 (TCP와 UDP 연결 연결)
    struct ClientConnection {
        int tcp_socket;
        sockaddr_in udp_address;
        bool udp_verified = false;
        std::chrono::steady_clock::time_point last_udp_time;
        uint32_t udp_sequence = 0;
    };
    
    std::unordered_map<uint32_t, ClientConnection> client_connections_;
    std::mutex connections_mutex_;
    
    // UDP 패킷 재전송 관리
    struct PendingPacket {
        std::vector<uint8_t> data;
        sockaddr_in destination;
        std::chrono::steady_clock::time_point send_time;
        int retry_count = 0;
        uint32_t sequence_id;
    };
    
    std::unordered_map<uint32_t, PendingPacket> pending_udp_packets_;
    std::mutex pending_mutex_;
    
    std::atomic<bool> running_{true};
    
public:
    HybridGameServer(int tcp_port, int udp_port) {
        // TCP 서버 초기화
        tcp_server_ = std::make_unique<EpollGameServer>(tcp_port);
        
        // UDP 서버 초기화
        CreateUDPServer(udp_port);
        
        std::cout << "🌐 하이브리드 게임 서버 시작" << std::endl;
        std::cout << "TCP 포트: " << tcp_port << " (신뢰성 데이터)" << std::endl;
        std::cout << "UDP 포트: " << udp_port << " (실시간 데이터)" << std::endl;
    }
    
    ~HybridGameServer() {
        running_ = false;
        if (udp_thread_.joinable()) {
            udp_thread_.join();
        }
        if (udp_socket_ != -1) {
            close(udp_socket_);
        }
    }
    
    void Run() {
        // UDP 처리 스레드 시작
        udp_thread_ = std::thread(&HybridGameServer::UDPWorkerThread, this);
        
        // TCP 서버 실행 (메인 스레드)
        tcp_server_->Run();
    }
    
    // TCP로 신뢰성 있는 데이터 전송
    void SendReliableData(uint32_t client_id, GameDataType data_type,
                         const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        
        auto it = client_connections_.find(client_id);
        if (it != client_connections_.end()) {
            auto packet = CreateGamePacket(data_type, data, false);
            // TCP 소켓으로 전송 (EpollGameServer 사용)
            SendTCPPacket(it->second.tcp_socket, packet);
            
            std::cout << "📡 TCP 전송: 클라이언트 " << client_id 
                      << ", 크기: " << packet.size() << " 바이트" << std::endl;
        }
    }
    
    // UDP로 실시간 데이터 전송
    void SendRealtimeData(uint32_t client_id, GameDataType data_type,
                         const std::vector<uint8_t>& data, bool require_ack = false) {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        
        auto it = client_connections_.find(client_id);
        if (it != client_connections_.end() && it->second.udp_verified) {
            auto packet = CreateGamePacket(data_type, data, require_ack);
            
            if (require_ack) {
                // ACK가 필요한 패킷은 재전송 관리
                SendUDPWithACK(client_id, packet, it->second.udp_address);
            } else {
                // Fire-and-forget UDP 전송
                SendUDPPacket(packet, it->second.udp_address);
            }
            
            std::cout << "🚀 UDP 전송: 클라이언트 " << client_id 
                      << ", 크기: " << packet.size() << " 바이트"
                      << (require_ack ? " (ACK 필요)" : "") << std::endl;
        }
    }

private:
    void CreateUDPServer(int port) {
        udp_socket_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (udp_socket_ == -1) {
            throw std::runtime_error("UDP socket creation failed");
        }
        
        // 논블로킹 모드 설정
        int flags = fcntl(udp_socket_, F_GETFL, 0);
        fcntl(udp_socket_, F_SETFL, flags | O_NONBLOCK);
        
        // 소켓 옵션 설정
        int opt = 1;
        setsockopt(udp_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        // 바인드
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        
        if (bind(udp_socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1) {
            close(udp_socket_);
            throw std::runtime_error("UDP bind failed");
        }
    }
    
    void UDPWorkerThread() {
        const size_t BUFFER_SIZE = 8192;
        std::vector<uint8_t> buffer(BUFFER_SIZE);
        
        while (running_) {
            sockaddr_in sender_addr{};
            socklen_t addr_len = sizeof(sender_addr);
            
            ssize_t bytes = recvfrom(udp_socket_, buffer.data(), buffer.size(), 0,
                                   reinterpret_cast<sockaddr*>(&sender_addr), &addr_len);
            
            if (bytes > 0) {
                ProcessUDPPacket(buffer.data(), bytes, sender_addr);
            } else if (bytes == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // 데이터 없음 - 다른 작업 수행
                    ProcessUDPTimeouts();
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                } else {
                    std::cerr << "UDP recvfrom error: " << strerror(errno) << std::endl;
                    break;
                }
            }
        }
    }
    
    void ProcessUDPPacket(const uint8_t* data, size_t size, const sockaddr_in& sender) {
        if (size < 8) return;  // 최소 헤더 크기
        
        // UDP 패킷 헤더 파싱
        struct UDPHeader {
            uint32_t client_id;
            uint16_t packet_type;
            uint8_t flags;       // bit 0: requires_ack, bit 1: is_ack
            uint8_t reserved;
        };
        
        UDPHeader header;
        memcpy(&header, data, sizeof(header));
        
        if (header.flags & 0x02) {
            // ACK 패킷 처리
            ProcessUDPAck(header.client_id, sender);
        } else {
            // 일반 게임 데이터 패킷
            std::vector<uint8_t> packet_data(data + sizeof(header), data + size);
            ProcessGameUDPPacket(header.client_id, header.packet_type, 
                                packet_data, sender, header.flags & 0x01);
        }
    }
    
    void ProcessGameUDPPacket(uint32_t client_id, uint16_t packet_type,
                             const std::vector<uint8_t>& data,
                             const sockaddr_in& sender, bool requires_ack) {
        
        // 클라이언트 UDP 주소 업데이트/검증
        UpdateClientUDPAddress(client_id, sender);
        
        // 게임 데이터 처리
        switch (packet_type) {
            case 1: // 플레이어 이동
                ProcessPlayerMovement(client_id, data);
                break;
            case 2: // 스킬 사용
                ProcessSkillActivation(client_id, data);
                break;
            case 3: // 하트비트
                ProcessHeartbeat(client_id);
                break;
            default:
                std::cout << "알 수 없는 UDP 패킷 타입: " << packet_type << std::endl;
                break;
        }
        
        // ACK 전송 (필요한 경우)
        if (requires_ack) {
            SendUDPAck(client_id, sender);
        }
    }
    
    void UpdateClientUDPAddress(uint32_t client_id, const sockaddr_in& address) {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        
        auto it = client_connections_.find(client_id);
        if (it != client_connections_.end()) {
            it->second.udp_address = address;
            it->second.udp_verified = true;
            it->second.last_udp_time = std::chrono::steady_clock::now();
            
            std::cout << "✅ 클라이언트 " << client_id << " UDP 주소 확인됨" << std::endl;
        }
    }
    
    void SendUDPWithACK(uint32_t client_id, const std::vector<uint8_t>& packet,
                       const sockaddr_in& destination) {
        uint32_t sequence_id = GenerateSequenceId();
        
        // 재전송 관리에 등록
        {
            std::lock_guard<std::mutex> lock(pending_mutex_);
            pending_udp_packets_[sequence_id] = {
                packet, destination, std::chrono::steady_clock::now(), 0, sequence_id
            };
        }
        
        // 첫 번째 전송
        SendUDPPacket(packet, destination);
    }
    
    void SendUDPPacket(const std::vector<uint8_t>& packet, const sockaddr_in& destination) {
        ssize_t sent = sendto(udp_socket_, packet.data(), packet.size(), 0,
                             reinterpret_cast<const sockaddr*>(&destination),
                             sizeof(destination));
        
        if (sent == -1) {
            std::cerr << "UDP sendto failed: " << strerror(errno) << std::endl;
        }
    }
    
    void SendUDPAck(uint32_t client_id, const sockaddr_in& destination) {
        struct UDPHeader {
            uint32_t client_id;
            uint16_t packet_type = 0;  // ACK
            uint8_t flags = 0x02;      // is_ack bit
            uint8_t reserved = 0;
        } ack_header{client_id};
        
        std::vector<uint8_t> ack_packet(
            reinterpret_cast<uint8_t*>(&ack_header),
            reinterpret_cast<uint8_t*>(&ack_header) + sizeof(ack_header)
        );
        
        SendUDPPacket(ack_packet, destination);
    }
    
    void ProcessUDPAck(uint32_t client_id, const sockaddr_in& sender) {
        // 해당 클라이언트의 대기 중인 패킷 제거
        std::lock_guard<std::mutex> lock(pending_mutex_);
        
        auto it = std::find_if(pending_udp_packets_.begin(), pending_udp_packets_.end(),
            [&sender](const auto& pair) {
                const auto& pending = pair.second;
                return pending.destination.sin_addr.s_addr == sender.sin_addr.s_addr &&
                       pending.destination.sin_port == sender.sin_port;
            });
        
        if (it != pending_udp_packets_.end()) {
            std::cout << "✅ UDP ACK 수신: " << it->first << std::endl;
            pending_udp_packets_.erase(it);
        }
    }
    
    void ProcessUDPTimeouts() {
        auto now = std::chrono::steady_clock::now();
        std::lock_guard<std::mutex> lock(pending_mutex_);
        
        for (auto it = pending_udp_packets_.begin(); it != pending_udp_packets_.end();) {
            auto& pending = it->second;
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - pending.send_time);
            
            if (elapsed.count() > 100) {  // 100ms 타임아웃
                if (pending.retry_count < 3) {  // 최대 3회 재전송
                    SendUDPPacket(pending.data, pending.destination);
                    pending.send_time = now;
                    pending.retry_count++;
                    
                    std::cout << "🔄 UDP 재전송: " << it->first 
                              << " (시도 " << pending.retry_count << "/3)" << std::endl;
                    ++it;
                } else {
                    std::cout << "❌ UDP 전송 실패: " << it->first << std::endl;
                    it = pending_udp_packets_.erase(it);
                }
            } else {
                ++it;
            }
        }
    }
    
    std::vector<uint8_t> CreateGamePacket(GameDataType data_type,
                                         const std::vector<uint8_t>& data,
                                         bool require_ack) {
        struct GamePacketHeader {
            uint16_t data_type;
            uint16_t data_length;
            uint8_t flags;
            uint8_t reserved[3];
        } header{};
        
        header.data_type = static_cast<uint16_t>(data_type);
        header.data_length = static_cast<uint16_t>(data.size());
        header.flags = require_ack ? 0x01 : 0x00;
        
        std::vector<uint8_t> packet;
        packet.insert(packet.end(),
                     reinterpret_cast<uint8_t*>(&header),
                     reinterpret_cast<uint8_t*>(&header) + sizeof(header));
        packet.insert(packet.end(), data.begin(), data.end());
        
        return packet;
    }
    
    uint32_t GenerateSequenceId() {
        static std::atomic<uint32_t> sequence_counter{1};
        return sequence_counter++;
    }
    
    void ProcessPlayerMovement(uint32_t client_id, const std::vector<uint8_t>& data) {
        if (data.size() >= 12) {
            float x, y, z;
            memcpy(&x, data.data(), 4);
            memcpy(&y, data.data() + 4, 4);
            memcpy(&z, data.data() + 8, 4);
            
            std::cout << "🏃 플레이어 " << client_id 
                      << " 이동: (" << x << ", " << y << ", " << z << ")" << std::endl;
            
            // 다른 클라이언트들에게 UDP로 브로드캐스트
            BroadcastMovementUDP(client_id, x, y, z);
        }
    }
    
    void ProcessSkillActivation(uint32_t client_id, const std::vector<uint8_t>& data) {
        if (data.size() >= 4) {
            uint32_t skill_id;
            memcpy(&skill_id, data.data(), 4);
            
            std::cout << "⚔️ 플레이어 " << client_id 
                      << " 스킬 사용: " << skill_id << std::endl;
            
            // 스킬 결과를 UDP + ACK로 전송
            NotifySkillResult(client_id, skill_id, true);
        }
    }
    
    void ProcessHeartbeat(uint32_t client_id) {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        auto it = client_connections_.find(client_id);
        if (it != client_connections_.end()) {
            it->second.last_udp_time = std::chrono::steady_clock::now();
        }
    }
    
    void BroadcastMovementUDP(uint32_t sender_id, float x, float y, float z) {
        std::vector<uint8_t> movement_data;
        movement_data.insert(movement_data.end(),
                           reinterpret_cast<uint8_t*>(&x),
                           reinterpret_cast<uint8_t*>(&x) + 4);
        movement_data.insert(movement_data.end(),
                           reinterpret_cast<uint8_t*>(&y),
                           reinterpret_cast<uint8_t*>(&y) + 4);
        movement_data.insert(movement_data.end(),
                           reinterpret_cast<uint8_t*>(&z),
                           reinterpret_cast<uint8_t*>(&z) + 4);
        
        std::lock_guard<std::mutex> lock(connections_mutex_);
        for (const auto& [client_id, connection] : client_connections_) {
            if (client_id != sender_id && connection.udp_verified) {
                SendRealtimeData(client_id, GameDataType::PLAYER_MOVEMENT, 
                               movement_data, false);
            }
        }
    }
    
    void NotifySkillResult(uint32_t client_id, uint32_t skill_id, bool success) {
        std::vector<uint8_t> result_data;
        result_data.insert(result_data.end(),
                          reinterpret_cast<uint8_t*>(&skill_id),
                          reinterpret_cast<uint8_t*>(&skill_id) + 4);
        
        uint8_t result = success ? 1 : 0;
        result_data.push_back(result);
        
        SendRealtimeData(client_id, GameDataType::SKILL_ACTIVATION, result_data, true);
    }
    
    void SendTCPPacket(int socket, const std::vector<uint8_t>& data) {
        // EpollGameServer의 SendToClient 메서드 호출
        // (실제 구현에서는 TCP 서버와 연동)
        std::cout << "📡 TCP 패킷 전송: " << data.size() << " 바이트" << std::endl;
    }
};

// 사용 예시
void HybridServerDemo() {
    try {
        HybridGameServer server(8080, 8081);  // TCP: 8080, UDP: 8081
        
        std::cout << "=== 하이브리드 서버 데모 ===" << std::endl;
        ProtocolSelector::PrintRecommendations();
        
        server.Run();
        
    } catch (const std::exception& e) {
        std::cerr << "서버 오류: " << e.what() << std::endl;
    }
}
```

---

## 📊 4. 성능 측정 및 벤치마킹

### **4.1 실시간 성능 모니터링**

```cpp
#include <chrono>
#include <atomic>
#include <iomanip>

class NetworkPerformanceMonitor {
private:
    // 성능 메트릭
    std::atomic<uint64_t> packets_sent_{0};
    std::atomic<uint64_t> packets_received_{0};
    std::atomic<uint64_t> bytes_sent_{0};
    std::atomic<uint64_t> bytes_received_{0};
    std::atomic<uint64_t> connections_active_{0};
    std::atomic<uint64_t> connections_total_{0};
    
    // 지연시간 측정
    std::vector<std::chrono::microseconds> latency_samples_;
    std::mutex latency_mutex_;
    
    // 에러 카운터
    std::atomic<uint64_t> send_errors_{0};
    std::atomic<uint64_t> receive_errors_{0};
    std::atomic<uint64_t> timeout_errors_{0};
    
    // 시간 측정
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point last_report_time_;
    
public:
    NetworkPerformanceMonitor() {
        start_time_ = std::chrono::steady_clock::now();
        last_report_time_ = start_time_;
        latency_samples_.reserve(10000);  // 10,000개 샘플 버퍼
    }
    
    // 메트릭 업데이트
    void RecordPacketSent(size_t bytes) {
        packets_sent_++;
        bytes_sent_ += bytes;
    }
    
    void RecordPacketReceived(size_t bytes) {
        packets_received_++;
        bytes_received_ += bytes;
    }
    
    void RecordLatency(std::chrono::microseconds latency) {
        std::lock_guard<std::mutex> lock(latency_mutex_);
        latency_samples_.push_back(latency);
        
        // 버퍼가 가득 차면 오래된 샘플 제거
        if (latency_samples_.size() > 10000) {
            latency_samples_.erase(latency_samples_.begin(), 
                                 latency_samples_.begin() + 1000);
        }
    }
    
    void RecordConnection(bool is_new = true) {
        connections_active_++;
        if (is_new) {
            connections_total_++;
        }
    }
    
    void RecordDisconnection() {
        if (connections_active_ > 0) {
            connections_active_--;
        }
    }
    
    void RecordError(const std::string& error_type) {
        if (error_type == "send") {
            send_errors_++;
        } else if (error_type == "receive") {
            receive_errors_++;
        } else if (error_type == "timeout") {
            timeout_errors_++;
        }
    }
    
    // 실시간 성능 리포트
    void PrintPerformanceReport() {
        auto now = std::chrono::steady_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::seconds>(
            now - start_time_);
        auto report_duration = std::chrono::duration_cast<std::chrono::seconds>(
            now - last_report_time_);
        
        // 기본 통계
        uint64_t packets_sent = packets_sent_.load();
        uint64_t packets_received = packets_received_.load();
        uint64_t bytes_sent = bytes_sent_.load();
        uint64_t bytes_received = bytes_received_.load();
        
        std::cout << "\n📊 실시간 성능 리포트" << std::endl;
        std::cout << "=================================" << std::endl;
        
        // 연결 통계
        std::cout << "🔗 연결 정보:" << std::endl;
        std::cout << "  활성 연결: " << connections_active_.load() << "개" << std::endl;
        std::cout << "  총 연결: " << connections_total_.load() << "개" << std::endl;
        
        // 패킷 통계
        if (total_duration.count() > 0) {
            double pps_sent = static_cast<double>(packets_sent) / total_duration.count();
            double pps_received = static_cast<double>(packets_received) / total_duration.count();
            
            std::cout << "\n📦 패킷 통계:" << std::endl;
            std::cout << "  전송: " << packets_sent << "개 (" 
                      << std::fixed << std::setprecision(1) << pps_sent << " PPS)" << std::endl;
            std::cout << "  수신: " << packets_received << "개 (" 
                      << std::fixed << std::setprecision(1) << pps_received << " PPS)" << std::endl;
        }
        
        // 대역폭 통계
        if (total_duration.count() > 0) {
            double mbps_sent = (static_cast<double>(bytes_sent) * 8) / 
                              (total_duration.count() * 1024 * 1024);
            double mbps_received = (static_cast<double>(bytes_received) * 8) / 
                                  (total_duration.count() * 1024 * 1024);
            
            std::cout << "\n🌐 대역폭 사용량:" << std::endl;
            std::cout << "  송신: " << FormatBytes(bytes_sent) << " (" 
                      << std::fixed << std::setprecision(2) << mbps_sent << " Mbps)" << std::endl;
            std::cout << "  수신: " << FormatBytes(bytes_received) << " (" 
                      << std::fixed << std::setprecision(2) << mbps_received << " Mbps)" << std::endl;
        }
        
        // 지연시간 통계
        PrintLatencyStatistics();
        
        // 에러 통계
        uint64_t total_errors = send_errors_ + receive_errors_ + timeout_errors_;
        if (total_errors > 0) {
            std::cout << "\n❌ 에러 통계:" << std::endl;
            std::cout << "  전송 에러: " << send_errors_.load() << "개" << std::endl;
            std::cout << "  수신 에러: " << receive_errors_.load() << "개" << std::endl;
            std::cout << "  타임아웃: " << timeout_errors_.load() << "개" << std::endl;
            std::cout << "  총 에러율: " << std::fixed << std::setprecision(3) 
                      << (static_cast<double>(total_errors) / packets_sent * 100) << "%" << std::endl;
        }
        
        last_report_time_ = now;
    }

private:
    void PrintLatencyStatistics() {
        std::lock_guard<std::mutex> lock(latency_mutex_);
        
        if (latency_samples_.empty()) {
            std::cout << "\n⏱️ 지연시간: 데이터 없음" << std::endl;
            return;
        }
        
        // 지연시간 분석
        auto samples = latency_samples_;
        std::sort(samples.begin(), samples.end());
        
        size_t count = samples.size();
        auto min_latency = samples.front();
        auto max_latency = samples.back();
        
        // 평균 계산
        uint64_t total_us = 0;
        for (const auto& sample : samples) {
            total_us += sample.count();
        }
        double avg_latency = static_cast<double>(total_us) / count;
        
        // 백분위수 계산
        auto p50 = samples[count * 50 / 100];
        auto p95 = samples[count * 95 / 100];
        auto p99 = samples[count * 99 / 100];
        
        std::cout << "\n⏱️ 지연시간 (샘플 수: " << count << "):" << std::endl;
        std::cout << "  최소: " << min_latency.count() << " μs" << std::endl;
        std::cout << "  평균: " << std::fixed << std::setprecision(1) << avg_latency << " μs" << std::endl;
        std::cout << "  P50: " << p50.count() << " μs" << std::endl;
        std::cout << "  P95: " << p95.count() << " μs" << std::endl;
        std::cout << "  P99: " << p99.count() << " μs" << std::endl;
        std::cout << "  최대: " << max_latency.count() << " μs" << std::endl;
        
        // 성능 평가
        if (avg_latency < 10000) {  // 10ms 미만
            std::cout << "  상태: ✅ 우수" << std::endl;
        } else if (avg_latency < 50000) {  // 50ms 미만
            std::cout << "  상태: ⚠️ 양호" << std::endl;
        } else {
            std::cout << "  상태: ❌ 개선 필요" << std::endl;
        }
    }
    
    std::string FormatBytes(uint64_t bytes) {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        int unit_index = 0;
        double size = static_cast<double>(bytes);
        
        while (size >= 1024.0 && unit_index < 4) {
            size /= 1024.0;
            unit_index++;
        }
        
        return std::to_string(static_cast<int>(size)) + " " + units[unit_index];
    }
};

// 패킷 지연시간 측정 유틸리티
class LatencyMeasurer {
private:
    struct PendingMeasurement {
        std::chrono::high_resolution_clock::time_point send_time;
        uint32_t sequence_id;
    };
    
    std::unordered_map<uint32_t, PendingMeasurement> pending_measurements_;
    std::mutex measurements_mutex_;
    NetworkPerformanceMonitor* monitor_;
    std::atomic<uint32_t> sequence_counter_{1};
    
public:
    LatencyMeasurer(NetworkPerformanceMonitor* monitor) : monitor_(monitor) {}
    
    uint32_t StartMeasurement() {
        uint32_t sequence = sequence_counter_++;
        
        std::lock_guard<std::mutex> lock(measurements_mutex_);
        pending_measurements_[sequence] = {
            std::chrono::high_resolution_clock::now(),
            sequence
        };
        
        return sequence;
    }
    
    void EndMeasurement(uint32_t sequence) {
        auto end_time = std::chrono::high_resolution_clock::now();
        
        std::lock_guard<std::mutex> lock(measurements_mutex_);
        auto it = pending_measurements_.find(sequence);
        
        if (it != pending_measurements_.end()) {
            auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
                end_time - it->second.send_time);
            
            monitor_->RecordLatency(latency);
            pending_measurements_.erase(it);
        }
    }
    
    void CleanupOldMeasurements() {
        auto now = std::chrono::high_resolution_clock::now();
        
        std::lock_guard<std::mutex> lock(measurements_mutex_);
        for (auto it = pending_measurements_.begin(); it != pending_measurements_.end();) {
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                now - it->second.send_time);
            
            if (elapsed.count() > 10) {  // 10초 이상 된 측정은 제거
                it = pending_measurements_.erase(it);
            } else {
                ++it;
            }
        }
    }
};
```

### **4.2 부하 테스트 클라이언트**

```cpp
#include <vector>
#include <thread>
#include <random>

class LoadTestClient {
private:
    struct TestScenario {
        int client_count;
        int packets_per_second_per_client;
        int test_duration_seconds;
        std::string test_name;
    };
    
    std::vector<TestScenario> scenarios_ = {
        {100, 10, 60, "기본 부하 테스트"},
        {500, 20, 120, "중간 부하 테스트"},
        {1000, 30, 180, "고부하 테스트"},
        {2000, 50, 300, "극한 부하 테스트"},
        {5000, 60, 600, "최대 용량 테스트"}
    };
    
    NetworkPerformanceMonitor monitor_;
    LatencyMeasurer latency_measurer_;
    
public:
    LoadTestClient() : latency_measurer_(&monitor_) {}
    
    void RunAllScenarios(const std::string& server_host, int server_port) {
        std::cout << "🧪 부하 테스트 시작" << std::endl;
        std::cout << "서버: " << server_host << ":" << server_port << std::endl;
        
        for (const auto& scenario : scenarios_) {
            std::cout << "\n" << std::string(50, '=') << std::endl;
            std::cout << "📊 " << scenario.test_name << std::endl;
            std::cout << "클라이언트 수: " << scenario.client_count << std::endl;
            std::cout << "PPS/클라이언트: " << scenario.packets_per_second_per_client << std::endl;
            std::cout << "총 예상 PPS: " << (scenario.client_count * scenario.packets_per_second_per_client) << std::endl;
            std::cout << "지속 시간: " << scenario.test_duration_seconds << "초" << std::endl;
            
            RunSingleScenario(scenario, server_host, server_port);
            
            // 시나리오 간 휴식
            std::cout << "\n😴 다음 테스트까지 30초 대기..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(30));
        }
        
        std::cout << "\n🎉 모든 부하 테스트 완료!" << std::endl;
    }
    
    void RunSingleScenario(const TestScenario& scenario, 
                          const std::string& server_host, int server_port) {
        
        std::vector<std::thread> client_threads;
        std::atomic<bool> test_running{true};
        
        // 클라이언트 스레드 생성
        for (int i = 0; i < scenario.client_count; ++i) {
            client_threads.emplace_back(&LoadTestClient::ClientWorker, this,
                                       i, server_host, server_port,
                                       scenario.packets_per_second_per_client,
                                       std::ref(test_running));
        }
        
        // 성능 모니터링 스레드
        std::thread monitor_thread(&LoadTestClient::MonitorWorker, this,
                                  scenario.test_duration_seconds, std::ref(test_running));
        
        // 테스트 실행
        auto start_time = std::chrono::steady_clock::now();
        std::this_thread::sleep_for(std::chrono::seconds(scenario.test_duration_seconds));
        
        // 테스트 중지
        test_running = false;
        
        // 모든 스레드 종료 대기
        for (auto& thread : client_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        
        if (monitor_thread.joinable()) {
            monitor_thread.join();
        }
        
        auto end_time = std::chrono::steady_clock::now();
        auto actual_duration = std::chrono::duration_cast<std::chrono::seconds>(
            end_time - start_time);
        
        // 최종 결과 출력
        std::cout << "\n📈 " << scenario.test_name << " 최종 결과:" << std::endl;
        std::cout << "실제 실행 시간: " << actual_duration.count() << "초" << std::endl;
        monitor_.PrintPerformanceReport();
        
        // 성능 평가
        EvaluatePerformance(scenario);
    }

private:
    void ClientWorker(int client_id, const std::string& host, int port,
                     int target_pps, const std::atomic<bool>& test_running) {
        
        // 시뮬레이션된 TCP 연결
        // (실제로는 socket() 호출)
        bool connected = SimulateConnection(host, port);
        if (!connected) {
            std::cerr << "클라이언트 " << client_id << " 연결 실패" << std::endl;
            return;
        }
        
        monitor_.RecordConnection();
        
        // 패킷 전송 루프
        auto packet_interval = std::chrono::microseconds(1000000 / target_pps);
        auto next_send_time = std::chrono::steady_clock::now();
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> size_dist(50, 500);  // 패킷 크기 랜덤
        
        while (test_running) {
            auto now = std::chrono::steady_clock::now();
            
            if (now >= next_send_time) {
                // 패킷 생성 및 전송
                size_t packet_size = size_dist(gen);
                auto sequence = latency_measurer_.StartMeasurement();
                
                bool sent = SimulatePacketSend(client_id, packet_size, sequence);
                if (sent) {
                    monitor_.RecordPacketSent(packet_size);
                } else {
                    monitor_.RecordError("send");
                }
                
                next_send_time += packet_interval;
            } else {
                // 다음 전송 시간까지 대기
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            
            // 응답 처리 시뮬레이션
            if (SimulatePacketReceive(client_id)) {
                // 지연시간 측정 완료 시뮬레이션
                // (실제로는 응답 패킷의 sequence ID 사용)
                latency_measurer_.EndMeasurement(sequence);
            }
        }
        
        monitor_.RecordDisconnection();
        SimulateDisconnection(client_id);
    }
    
    void MonitorWorker(int duration_seconds, const std::atomic<bool>& test_running) {
        int report_interval = 10;  // 10초마다 리포트
        
        for (int elapsed = 0; elapsed < duration_seconds && test_running; elapsed += report_interval) {
            std::this_thread::sleep_for(std::chrono::seconds(report_interval));
            
            if (test_running) {
                std::cout << "\n⏰ 경과 시간: " << (elapsed + report_interval) << "초" << std::endl;
                monitor_.PrintPerformanceReport();
                
                // 오래된 지연시간 측정 정리
                latency_measurer_.CleanupOldMeasurements();
            }
        }
    }
    
    bool SimulateConnection(const std::string& host, int port) {
        // 실제로는 socket(), connect() 호출
        std::this_thread::sleep_for(std::chrono::milliseconds(1));  // 연결 지연 시뮬레이션
        return true;  // 90% 성공률
    }};

---

## 🔥 흔한 실수와 해결방법 (Common Mistakes & Solutions)

### 1. 블로킹 I/O로 인한 성능 저하
```cpp
// [SEQUENCE: 1] ❌ 잘못된 예: 블로킹 recv() 사용
void BadNetworkHandler() {
    char buffer[1024];
    while (true) {
        // 블로킹 모드에서 대기 - 다른 클라이언트 처리 불가\!
        int bytes = recv(socket_fd, buffer, sizeof(buffer), 0);
        if (bytes > 0) {
            ProcessPacket(buffer, bytes);
        }
    }
}

// ✅ 올바른 예: 논블로킹 I/O + epoll
void GoodNetworkHandler() {
    // 소켓을 논블로킹으로 설정
    int flags = fcntl(socket_fd, F_GETFL, 0);
    fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);
    
    // epoll로 이벤트 기반 처리
    epoll_event ev;
    ev.events = EPOLLIN | EPOLLET;  // Edge-triggered
    ev.data.fd = socket_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_fd, &ev);
}
```

### 2. 작은 패킷 개별 전송으로 인한 오버헤드
```cpp
// [SEQUENCE: 2] ❌ 잘못된 예: 패킷 하나씩 전송
void BadPacketSender() {
    for (const auto& update : player_updates) {
        send(socket_fd, &update, sizeof(update), 0);  // 시스템 콜 남발\!
    }
}

// ✅ 올바른 예: 패킷 배칭
void GoodPacketSender() {
    char batch_buffer[65536];
    size_t offset = 0;
    
    // 여러 패킷을 하나의 버퍼에 모음
    for (const auto& update : player_updates) {
        memcpy(batch_buffer + offset, &update, sizeof(update));
        offset += sizeof(update);
    }
    
    // 한 번에 전송
    send(socket_fd, batch_buffer, offset, MSG_NOSIGNAL);
}
```

### 3. TCP 소켓 옵션 미설정
```cpp
// [SEQUENCE: 3] ❌ 잘못된 예: 기본 TCP 설정 사용
int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
// Nagle 알고리즘으로 인한 지연 발생\!

// ✅ 올바른 예: 게임 서버에 최적화된 설정
int socket_fd = socket(AF_INET, SOCK_STREAM, 0);

// TCP_NODELAY: Nagle 알고리즘 비활성화
int flag = 1;
setsockopt(socket_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

// SO_REUSEADDR: 빠른 재시작
setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));

// 버퍼 크기 최적화
int buffer_size = 262144;  // 256KB
setsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, &buffer_size, sizeof(buffer_size));
setsockopt(socket_fd, SOL_SOCKET, SO_SNDBUF, &buffer_size, sizeof(buffer_size));
```

---

## 🚀 실습 프로젝트 (Practice Projects)

### 📚 기초 프로젝트: 고성능 에코 서버
**목표**: epoll/IOCP를 활용한 10만 동시 연결 에코 서버

```cpp
// 구현 요구사항:
// 1. epoll (Linux) / IOCP (Windows) 구현
// 2. 논블로킹 I/O 처리
// 3. 연결 풀링
// 4. 성능 모니터링 (PPS, 지연시간)
// 5. 부하 테스트 도구
// 6. 10만 동시 연결 목표
```

### 🎮 중급 프로젝트: 실시간 위치 동기화 서버
**목표**: UDP 기반 고성능 위치 동기화 시스템

```cpp
// 핵심 기능:
// 1. UDP 신뢰성 레이어 구현
// 2. 델타 압축 알고리즘
// 3. 관심 영역(AOI) 관리
// 4. 패킷 우선순위 시스템
// 5. 지연 보상 알고리즘
// 6. 50만 PPS 처리 목표
```

### 🏆 고급 프로젝트: DPDK 기반 게임 서버
**목표**: 커널 바이패스로 100만 PPS 달성

```cpp
// 구현 내용:
// 1. DPDK 환경 구성
// 2. 사용자 공간 TCP/IP 스택
// 3. Zero-copy 패킷 처리
// 4. CPU 코어 친화성 설정
// 5. NUMA 최적화
// 6. 하드웨어 오프로드 활용
// 7. 100만 PPS 벤치마크
```

---

## 📊 학습 체크리스트 (Learning Checklist)

### 🥉 브론즈 레벨
- [ ] select/poll/epoll 차이점 이해
- [ ] 논블로킹 I/O 구현
- [ ] TCP 소켓 옵션 설정
- [ ] 기본 패킷 프로토콜 설계

### 🥈 실버 레벨
- [ ] epoll ET/LT 모드 활용
- [ ] IOCP 구현 (Windows)
- [ ] 패킷 배칭과 압축
- [ ] 연결 풀 관리

### 🥇 골드 레벨
- [ ] kqueue 활용 (BSD/macOS)
- [ ] io_uring 구현 (Linux 5.1+)
- [ ] 커스텀 프로토콜 최적화
- [ ] 대규모 부하 테스트

### 💎 플래티넘 레벨
- [ ] DPDK 마스터
- [ ] 커널 바이패스 기술
- [ ] RDMA 프로그래밍
- [ ] 100만 PPS 달성

---

## 📚 추가 학습 자료 (Additional Resources)

### 필독서
- 📖 "UNIX Network Programming" - W. Richard Stevens
- 📖 "High Performance Browser Networking" - Ilya Grigorik
- 📖 "The Linux Programming Interface" - Michael Kerrisk

### 온라인 강의
- 🎓 Linux System Programming (Coursera)
- 🎓 Network Programming with Go
- 🎓 DPDK Programming Guide

### 오픈소스 프로젝트
- 🔧 libuv: 크로스 플랫폼 비동기 I/O
- 🔧 Seastar: 고성능 C++ 프레임워크
- 🔧 DPDK: Data Plane Development Kit
- 🔧 Netty: Java 고성능 네트워크 프레임워크

### 성능 도구
- 🛠️ iperf3: 네트워크 성능 측정
- 🛠️ tcpdump/Wireshark: 패킷 분석
- 🛠️ perf: 리눅스 성능 프로파일링
- 🛠️ BPF/eBPF: 커널 레벨 모니터링
