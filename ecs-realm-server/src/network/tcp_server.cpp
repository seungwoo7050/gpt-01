#include "network/tcp_server.h"
#include "network/session.h"
#include "network/session_manager.h"
#include "network/packet_handler.h"

#include <iostream>
#include <utility>

namespace mmorpg::network {

// [SEQUENCE: MVP1-9] `TcpServer::TcpServer()`: 서버 설정을 초기화합니다.
TcpServer::TcpServer(std::string address, uint16_t port, size_t thread_pool_size, const std::string& cert_file, const std::string& key_file)
    : m_isRunning(false),
      m_address(std::move(address)),
      m_port(port),
      m_threadPoolSize(thread_pool_size > 0 ? thread_pool_size : 1),
      m_nextSessionId(1),
      m_acceptor(m_acceptorIoContext),
      m_ssl_context(std::make_shared<boost::asio::ssl::context>(boost::asio::ssl::context::sslv23)),
      m_nextIoContextIndex(0) {
    
    m_sessionManager = std::make_shared<SessionManager>();

    try {
        m_ssl_context->set_options(
            boost::asio::ssl::context::default_workarounds |
            boost::asio::ssl::context::no_sslv2 |
            boost::asio::ssl::context::single_dh_use);

        m_ssl_context->use_certificate_chain_file(cert_file);
        m_ssl_context->use_private_key_file(key_file, boost::asio::ssl::context::pem);
    } catch (const std::exception& e) {
        std::cerr << "[TcpServer] SSL Context Error: " << e.what() << std::endl;
    }
}

TcpServer::~TcpServer() {
    Stop();
}

// [SEQUENCE: MVP1-10] `TcpServer::Start()`: I/O 컨텍스트 스레드를 생성하고 클라이언트 접속을 받기 시작합니다.
bool TcpServer::Start() {
    if (m_isRunning.exchange(true)) {
        return true; // Already running
    }

    try {
        tcp::endpoint endpoint(boost::asio::ip::make_address(m_address), m_port);
        m_acceptor.open(endpoint.protocol());
        m_acceptor.set_option(tcp::acceptor::reuse_address(true));
        m_acceptor.bind(endpoint);
        m_acceptor.listen();
    } catch (const boost::system::system_error& e) {
        std::cerr << "[TcpServer] Error starting server: " << e.what() << std::endl;
        m_isRunning = false;
        return false;
    }

    std::cout << "[TcpServer] Started on " << m_address << ":" << m_port << std::endl;

    for (size_t i = 0; i < m_threadPoolSize; ++i) {
        auto io_context = std::make_shared<boost::asio::io_context>();
        m_ioContextPool.push_back(io_context);
        m_workerThreads.emplace_back([io_context]() {
            boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard(io_context->get_executor());
            io_context->run();
        });
    }

    DoAccept();
    return true;
}

// [SEQUENCE: MVP1-11] `TcpServer::Stop()`: 모든 I/O 작업을 중지하고 스레드를 정리합니다.
void TcpServer::Stop() {
    if (!m_isRunning.exchange(false)) {
        return;
    }
    m_acceptorIoContext.stop();
    if (m_acceptor.is_open()) {
        m_acceptor.close();
    }
    for (const auto& io_context : m_ioContextPool) {
        io_context->stop();
    }
    for (auto& thread : m_workerThreads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    m_workerThreads.clear();
    m_ioContextPool.clear();
    std::cout << "[TcpServer] Stopped." << std::endl;
}

size_t TcpServer::GetConnectionCount() {
    return m_sessionManager ? m_sessionManager->GetSessionCount() : 0;
}

void TcpServer::Broadcast(const google::protobuf::Message& message) {
    if (m_sessionManager) {
        m_sessionManager->Broadcast(message);
    }
}

boost::asio::io_context& TcpServer::GetNextIoContext() {
    auto& io_context = *m_ioContextPool[m_nextIoContextIndex++ % m_threadPoolSize];
    return io_context;
}

// [SEQUENCE: MVP1-12] `TcpServer::DoAccept()`: 새로운 클라이언트 연결을 비동기적으로 대기합니다.
void TcpServer::DoAccept() {
    auto ssl_stream = std::make_shared<boost::asio::ssl::stream<tcp::socket>>(GetNextIoContext(), *m_ssl_context);
    m_acceptor.async_accept(ssl_stream->next_layer(),
        [this, ssl_stream](const boost::system::error_code& ec) {
            OnAccept(ec, ssl_stream);
        });
}

void TcpServer::OnAccept(const boost::system::error_code& ec, std::shared_ptr<boost::asio::ssl::stream<tcp::socket>> ssl_stream) {
    if (!ec) {
        ssl_stream->async_handshake(boost::asio::ssl::stream_base::server,
            [this, ssl_stream](const boost::system::error_code& ec) {
                OnHandshake(ec, ssl_stream);
            });
    } else {
        std::cerr << "[TcpServer] Accept error: " << ec.message() << std::endl;
    }

    if (m_isRunning) {
        DoAccept();
    }
}

void TcpServer::OnHandshake(const boost::system::error_code& ec, std::shared_ptr<boost::asio::ssl::stream<tcp::socket>> ssl_stream) {
    if (!ec) {
        auto session_id = m_nextSessionId++;
        auto new_session = std::make_shared<Session>(ssl_stream, session_id, m_packetHandler);
        m_sessionManager->Register(new_session);
        new_session->Start();
    } else {
        std::cerr << "[TcpServer] Handshake error: " << ec.message() << std::endl;
    }
}

}