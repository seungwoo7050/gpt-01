#include "server/http_metrics_server.h"
#include "core/monitoring/metrics_collector.h"
#include <spdlog/spdlog.h>

namespace mmorpg::server {

// [SEQUENCE: 1] Constructor
HttpMetricsServer::HttpMetricsServer(net::io_context& io_context, uint16_t port)
    : io_context_(io_context)
    , acceptor_(io_context, tcp::endpoint(tcp::v4(), port))
    , port_(port) {
    
    acceptor_.set_option(net::socket_base::reuse_address(true));
}

HttpMetricsServer::~HttpMetricsServer() {
    Stop();
}

// [SEQUENCE: 2] Start server
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

// [SEQUENCE: 3] Accept connections
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

// [SEQUENCE: 4] Session implementation
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

// [SEQUENCE: 5] Handle request
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

// [SEQUENCE: 6] Process HTTP request and generate response
void HttpMetricsServer::Session::HandleRequest() {
    response_ = std::make_shared<http::response<http::string_body>>();
    response_->version(request_.version());
    response_->keep_alive(request_.keep_alive());
    
    // Route handling
    std::string target = request_.target();
    
    if (target == "/metrics") {
        // Prometheus format metrics
        response_->result(http::status::ok);
        response_->set(http::field::content_type, "text/plain; version=0.0.4");
        response_->body() = core::monitoring::MetricsCollector::Instance().ExportPrometheusFormat();
        
    } else if (target == "/metrics/json") {
        // JSON format metrics
        response_->result(http::status::ok);
        response_->set(http::field::content_type, "application/json");
        response_->body() = core::monitoring::MetricsCollector::Instance().ExportMetricsJson();
        
    } else if (target == "/health") {
        // Health check endpoint
        response_->result(http::status::ok);
        response_->set(http::field::content_type, "application/json");
        response_->body() = R"({"status":"healthy","service":"mmorpg-server"})";
        
    } else if (target == "/") {
        // Simple dashboard
        response_->result(http::status::ok);
        response_->set(http::field::content_type, "text/html");
        response_->body() = R"(
<!DOCTYPE html>
<html>
<head>
    <title>MMORPG Server Metrics</title>
    <meta http-equiv="refresh" content="5">
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #f0f0f0; }
        .metric-card { background: white; padding: 15px; margin: 10px; border-radius: 5px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }
        .metric-title { font-weight: bold; color: #333; margin-bottom: 10px; }
        .metric-value { font-size: 24px; color: #2196F3; }
        .warning { color: #ff9800; }
        .error { color: #f44336; }
        .grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); gap: 15px; }
    </style>
</head>
<body>
    <h1>MMORPG Server Metrics Dashboard</h1>
    <p>Auto-refresh every 5 seconds | <a href="/metrics">Prometheus</a> | <a href="/metrics/json">JSON</a></p>
    <div id="metrics"></div>
    <script>
        fetch('/metrics/json')
            .then(response => response.json())
            .then(data => {
                const container = document.getElementById('metrics');
                let html = '<div class="grid">';
                
                // Network metrics
                html += '<div class="metric-card">';
                html += '<div class="metric-title">Active Connections</div>';
                html += '<div class="metric-value">' + data.network.active_connections + '</div>';
                html += '</div>';
                
                html += '<div class="metric-card">';
                html += '<div class="metric-title">Packets/sec In</div>';
                html += '<div class="metric-value">' + Math.round(data.network.packets_per_second_in) + '</div>';
                html += '</div>';
                
                html += '<div class="metric-card">';
                html += '<div class="metric-title">Bandwidth In</div>';
                html += '<div class="metric-value">' + data.network.bandwidth_in_mbps.toFixed(2) + ' Mbps</div>';
                html += '</div>';
                
                // Performance metrics
                html += '<div class="metric-card">';
                html += '<div class="metric-title">Server FPS</div>';
                html += '<div class="metric-value">' + Math.round(data.performance.current_fps) + '</div>';
                html += '</div>';
                
                html += '<div class="metric-card">';
                html += '<div class="metric-title">Avg Tick Time</div>';
                const tickTime = data.performance.average_tick_time_ms;
                const tickClass = tickTime > 33 ? 'warning' : '';
                html += '<div class="metric-value ' + tickClass + '">' + tickTime.toFixed(1) + ' ms</div>';
                html += '</div>';
                
                // Game metrics
                html += '<div class="metric-card">';
                html += '<div class="metric-title">Active Entities</div>';
                html += '<div class="metric-value">' + data.game.active_entities + '</div>';
                html += '</div>';
                
                // Resource metrics
                html += '<div class="metric-card">';
                html += '<div class="metric-title">Memory Usage</div>';
                html += '<div class="metric-value">' + Math.round(data.resources.memory_used_mb) + ' MB</div>';
                html += '</div>';
                
                html += '<div class="metric-card">';
                html += '<div class="metric-title">CPU Usage</div>';
                const cpuUsage = data.resources.cpu_usage_percent;
                const cpuClass = cpuUsage > 80 ? 'warning' : '';
                html += '<div class="metric-value ' + cpuClass + '">' + cpuUsage.toFixed(1) + '%</div>';
                html += '</div>';
                
                html += '</div>';
                
                // Health status
                if (data.health.warnings && data.health.warnings.length > 0) {
                    html += '<div class="metric-card" style="margin-top: 20px;">';
                    html += '<div class="metric-title warning">Warnings</div>';
                    html += '<ul>';
                    data.health.warnings.forEach(warning => {
                        html += '<li>' + warning + '</li>';
                    });
                    html += '</ul>';
                    html += '</div>';
                }
                
                container.innerHTML = html;
            })
            .catch(error => {
                document.getElementById('metrics').innerHTML = '<div class="error">Failed to load metrics</div>';
            });
    </script>
</body>
</html>
        )";
        
    } else {
        // 404 Not Found
        response_->result(http::status::not_found);
        response_->set(http::field::content_type, "text/plain");
        response_->body() = "Not Found";
    }
    
    response_->prepare_payload();
    
    // Send response
    http::async_write(socket_, *response_,
        [self = shared_from_this()](beast::error_code ec, std::size_t bytes) {
            self->OnWrite(ec, bytes);
        });
}

// [SEQUENCE: 7] Handle write completion
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
    
    // Read another request
    DoRead();
}

} // namespace mmorpg::server