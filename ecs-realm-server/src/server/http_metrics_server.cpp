#include "server/http_metrics_server.h"
#include "core/monitoring/metrics_collector.h"
#include <spdlog/spdlog.h>

// [SEQUENCE: MVP1-66] HTTP metrics server for monitoring endpoints

namespace mmorpg::server {

HttpMetricsServer::HttpMetricsServer(net::io_context& io_context, uint16_t port)
    : io_context_(io_context)
    , acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    , port_(port) {
    
    acceptor_.set_option(net::socket_base::reuse_address(true));
}

HttpMetricsServer::~HttpMetricsServer() {
    Stop();
}

void HttpMetricsServer::Start() {
    if (running_) return;
    
    running_ = true;
    spdlog::info("HTTP metrics server listening on port {}", port_);
    DoAccept();
}

void HttpMetricsServer::Stop() {
    running_ = false;
    acceptor_.close();
}

void HttpMetricsServer::DoAccept() {
    acceptor_.async_accept(
        [this](beast::error_code ec, tcp::socket socket) {
            OnAccept(ec, std::move(socket));
        });
}

void HttpMetricsServer::OnAccept(beast::error_code ec, tcp::socket socket) {
    if (!ec && running_) {
        std::make_shared<Session>(std::move(socket))->Run();
        DoAccept();
    }
}

HttpMetricsServer::Session::Session(tcp::socket socket)
    : socket_(std::move(socket)) {
}

void HttpMetricsServer::Session::Run() {
    DoRead();
}

void HttpMetricsServer::Session::DoRead() {
    request_ = {};
    
    http::async_read(socket_, buffer_, request_,
        [self = shared_from_this()](beast::error_code ec, std::size_t bytes) {
            self->OnRead(ec, bytes);
        });
}

void HttpMetricsServer::Session::OnRead(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);
    
    if (ec == http::error::end_of_stream) {
        socket_.shutdown(tcp::socket::shutdown_send, ec);
        return;
    }
    
    if (ec) {
        spdlog::debug("HTTP read error: {}", ec.message());
        return;
    }
    
    HandleRequest();
}

void HttpMetricsServer::Session::HandleRequest() {
    response_ = std::make_shared<http::response<http::string_body>>();
    response_->version(request_.version());
    response_->keep_alive(request_.keep_alive());
    
    std::string target = request_.target();
    
    if (target == "/metrics") {
        response_->result(http::status::ok);
        response_->set(http::field::content_type, "text/plain; version=0.0.4");
        response_->body() = core::monitoring::MetricsCollector::Instance().ExportPrometheusFormat();
    } else if (target == "/metrics/json") {
        response_->result(http::status::ok);
        response_->set(http::field::content_type, "application/json");
        response_->body() = core::monitoring::MetricsCollector::Instance().ExportMetricsJson();
    } else if (target == "/health") {
        response_->result(http::status::ok);
        response_->set(http::field::content_type, "application/json");
        response_->body() = R"({"status":"healthy","service":"mmorpg-server"})";
    } else {
        response_->result(http::status::not_found);
        response_->set(http::field::content_type, "text/plain");
        response_->body() = "Not Found";
    }
    
    response_->prepare_payload();
    
    http::async_write(socket_, *response_,
        [self = shared_from_this()](beast::error_code ec, std::size_t bytes) {
            self->OnWrite(ec, bytes);
        });
}

void HttpMetricsServer::Session::OnWrite(beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);
    
    if (ec) {
        spdlog::debug("HTTP write error: {}", ec.message());
        return;
    }
    
    if (response_->need_eof()) {
        socket_.shutdown(tcp::socket::shutdown_send, ec);
        return;
    }
    
    DoRead();
}

} // namespace mmorpg::server
