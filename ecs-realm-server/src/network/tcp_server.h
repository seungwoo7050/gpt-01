#pragma once

#include <memory>
#include <string>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include "network/session_manager.h"
#include "network/packet_handler.h"

namespace mmorpg::network {

// [SEQUENCE: MVP6-9] The main server class that accepts incoming TCP connections, now with SSL support.
// It holds the SSL context required by all sessions.
class TcpServer : public std::enable_shared_from_this<TcpServer> {
public:
    TcpServer(boost::asio::io_context& io_context,
              std::shared_ptr<SessionManager> session_manager,
              std::shared_ptr<IPacketHandler> packet_handler,
              short port,
              const std::string& cert_file,
              const std::string& key_file);

    void run();
    void stop();

private:
    void do_accept();

    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::ssl::context ssl_context_;
    std::shared_ptr<SessionManager> session_manager_;
    std::shared_ptr<IPacketHandler> packet_handler_;
};

}
