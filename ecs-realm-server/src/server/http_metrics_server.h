#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <thread>
#include <memory>

namespace mmorpg::server {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = net::ip::tcp;

// [SEQUENCE: 1] HTTP metrics server for monitoring endpoints
class HttpMetricsServer {
public:
    HttpMetricsServer(net::io_context& io_context, uint16_t port);
    ~HttpMetricsServer();
    
    void Start();
    void Stop();
    
private:
    // [SEQUENCE: 2] HTTP session handler
    class Session : public std::enable_shared_from_this<Session> {
    public:
        explicit Session(tcp::socket socket);
        void Run();
        
    private:
        void DoRead();
        void OnRead(beast::error_code ec, std::size_t bytes_transferred);
        void HandleRequest();
        void OnWrite(beast::error_code ec, std::size_t bytes_transferred);
        
        tcp::socket socket_;
        beast::flat_buffer buffer_;
        http::request<http::string_body> request_;
        std::shared_ptr<http::response<http::string_body>> response_;
    };
    
    // [SEQUENCE: 3] Accept incoming connections
    void DoAccept();
    void OnAccept(beast::error_code ec, tcp::socket socket);
    
    net::io_context& io_context_;
    tcp::acceptor acceptor_;
    uint16_t port_;
    bool running_ = false;
};

} // namespace mmorpg::server