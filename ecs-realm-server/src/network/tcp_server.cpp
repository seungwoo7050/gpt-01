#include "network/tcp_server.h"
#include "network/session.h"
#include "core/logger.h"

namespace mmorpg::network {

// [SEQUENCE: MVP6-10] Constructor: Initializes the SSL context and the acceptor.
// It loads the server's certificate and private key, which are required for TLS.
TcpServer::TcpServer(boost::asio::io_context& io_context,
                   std::shared_ptr<SessionManager> session_manager,
                   std::shared_ptr<IPacketHandler> packet_handler,
                   short port,
                   const std::string& cert_file,
                   const std::string& key_file)
    : acceptor_(io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)),
      ssl_context_(boost::asio::ssl::context::sslv23),
      session_manager_(session_manager),
      packet_handler_(packet_handler) {

    ssl_context_.set_options(
        boost::asio::ssl::context::default_workarounds |
        boost::asio::ssl::context::no_sslv2 |
        boost::asio::ssl::context::single_dh_use);

    try {
        ssl_context_.use_certificate_chain_file(cert_file);
        ssl_context_.use_private_key_file(key_file, boost::asio::ssl::context::pem);
    } catch (const boost::system::system_error& e) {
        LOG_ERROR("Failed to set up SSL context: {}", e.what());
        throw;
    }
}

void TcpServer::run() {
    LOG_INFO("Asynchronous SSL TCP server started on port {}", acceptor_.local_endpoint().port());
    do_accept();
}

void TcpServer::stop() {
    acceptor_.close();
}

// [SEQUENCE: MVP6-11] The core accept loop. When a new connection is accepted, it creates a Session,
// passes the SSL context to it, and calls Start() on the new session to initiate the handshake.
void TcpServer::do_accept() {
    acceptor_.async_accept(
        [this](const boost::system::error_code& error, boost::asio::ip::tcp::socket socket) {
            if (!error) {
                try {
                    auto session_id = session_manager_->get_next_session_id();
                    auto new_session = std::make_shared<Session>(
                        std::move(socket),
                        ssl_context_,
                        session_id,
                        packet_handler_);
                    
                    session_manager_->Register(new_session);
                    new_session->Start(); // The session itself will initiate the handshake

                } catch (const std::exception& e) {
                    LOG_ERROR("Exception while creating session: {}", e.what());
                }
            }
            else {
                LOG_ERROR("Accept error: {}", error.message());
            }

            if (acceptor_.is_open()) {
                do_accept();
            }
        });
}

}
