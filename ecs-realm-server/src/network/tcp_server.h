#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <memory>
#include <vector>
#include <thread>
#include <string>
#include <atomic>
#include <cstdint>
#include <google/protobuf/message.h>

// Forward declarations
namespace mmorpg::network {
class IPacketHandler;
class SessionManager;
class Session;
}

namespace mmorpg::network {

using boost::asio::ip::tcp;

// [SEQUENCE: MVP1-7] Network Engine (`src/network/`)
// [SEQUENCE: MVP1-8] Asynchronous TCP server using Boost.Asio
class TcpServer {
public:
    // [SEQUENCE: MVP1-9] `TcpServer::TcpServer()`: 서버 설정을 초기화합니다.
    // [SEQUENCE: MVP6-24] Updated constructor to accept SSL certificate and key files.
    TcpServer(std::string address, uint16_t port, size_t thread_pool_size, const std::string& cert_file, const std::string& key_file);
    ~TcpServer();

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;
    TcpServer(TcpServer&&) = delete;
    TcpServer& operator=(TcpServer&&) = delete;

    // [SEQUENCE: MVP1-10] `TcpServer::Start()`: I/O 컨텍스트 스레드를 생성하고 클라이언트 접속을 받기 시작합니다.
    bool Start();
    // [SEQUENCE: MVP1-11] `TcpServer::Stop()`: 모든 I/O 작업을 중지하고 스레드를 정리합니다.
    void Stop();

    bool IsRunning() const { return m_isRunning; }
    size_t GetConnectionCount();

    void SetPacketHandler(std::shared_ptr<IPacketHandler> handler) { m_packetHandler = std::move(handler); }
    std::shared_ptr<IPacketHandler> GetPacketHandler() const { return m_packetHandler; }

    // [SEQUENCE: MVP6-21] Expose SessionManager for UdpServer to use.
    std::shared_ptr<SessionManager> GetSessionManager() const { return m_sessionManager; }

    void Broadcast(const google::protobuf::Message& message);

private:
    boost::asio::io_context& GetNextIoContext();
    // [SEQUENCE: MVP1-12] `TcpServer::DoAccept()`: 새로운 클라이언트 연결을 비동기적으로 대기합니다.
    void DoAccept();
    // [SEQUENCE: MVP6-25] The OnAccept method is updated to handle SSL streams.
    void OnAccept(const boost::system::error_code& ec, std::shared_ptr<boost::asio::ssl::stream<tcp::socket>> ssl_stream);
    void OnHandshake(const boost::system::error_code& ec, std::shared_ptr<boost::asio::ssl::stream<tcp::socket>> ssl_stream);

    std::atomic<bool> m_isRunning;
    const std::string m_address;
    const uint16_t m_port;
    const size_t m_threadPoolSize;
    std::atomic<uint32_t> m_nextSessionId;

    boost::asio::io_context m_acceptorIoContext;
    tcp::acceptor m_acceptor;
    std::shared_ptr<boost::asio::ssl::context> m_ssl_context;

    std::vector<std::shared_ptr<boost::asio::io_context>> m_ioContextPool;
    std::vector<std::thread> m_workerThreads;
    std::atomic<size_t> m_nextIoContextIndex;

    std::shared_ptr<IPacketHandler> m_packetHandler;
    std::shared_ptr<SessionManager> m_sessionManager;
};

}