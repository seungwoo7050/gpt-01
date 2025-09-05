# Extension 02: Large-scale Load Testing & Graphing

## 1. Objective

This guide enhances the `load_test_client` to perform large-scale tests and visualize the results in Grafana. We will replace the simple, in-memory metrics with `prometheus-cpp` metrics and push them to a Prometheus PushGateway. This allows us to collect detailed performance data (like latency histograms) from an ephemeral load test job and analyze it in a persistent dashboard.

## 2. Analysis of Current Implementation

The current `load_test_client` has two main limitations for large-scale testing:

1.  **Internal Metrics**: The `Metrics` struct is a collection of `std::atomic` counters. While thread-safe, it only provides simple counts (e.g., `connections_succeeded`, `packets_sent`). It cannot measure latency distributions (p50/p95/p99) and the results are only printed to the console at the end of the test, making them difficult to aggregate and analyze over time.
2.  **Scalability**: While it uses `boost::asio` for asynchronous operations, the metrics collection and overall structure could be optimized for simulating thousands of clients, each reporting detailed metrics.

**Legacy Code (`tests/load_test/load_test_client.h` - partial):**
```cpp
// This struct is internal to the load test client and its results are transient.
struct Metrics {
    std::atomic<uint64_t> connections_succeeded{0};
    std::atomic<uint64_t> connections_failed{0};
    std::atomic<uint64_t> packets_sent{0};
    std::atomic<uint64_t> packets_received{0};
};
```

## 3. Proposed Implementation

### Step 3.1: Add `prometheus-cpp` PushGateway Dependency

Update `conanfile.txt` to include the `prometheus-cpp` library, if you haven't already from the previous guide. The key is that we will use the `push` components.

**Modified File (`ecs-realm-server/conanfile.txt`):**
```diff
# ... (previous dependencies)
zlib/1.2.13 # Explicitly added to resolve version conflict from openssl
# [EXTENSION-01] Add prometheus-cpp for metrics
- prometheus-cpp/1.1.0
+ prometheus-cpp/1.1.0 # Already added in Ext. 01, ensure it's here

[generators]
# ...
```

Next, ensure CMake links the `push` library from `prometheus-cpp` to the `load_test_client` executable.

**Modified File (`ecs-realm-server/CMakeLists.txt`):**
```diff
    add_executable(load_test_client
        tests/load_test/main.cpp
        tests/load_test/load_test_client.cpp
    )
-   target_link_libraries(load_test_client PRIVATE mmorpg_core Boost::program_options spdlog::spdlog CLI11::CLI11)
+   target_link_libraries(load_test_client PRIVATE mmorpg_core Boost::program_options spdlog::spdlog CLI11::CLI11 prometheus-cpp::core prometheus-cpp::push)
 endif()

# ... (rest of file)
```

### Step 3.2: Refactor `LoadTestClient` to Use Prometheus Metrics

We will replace the `Metrics` struct with a `prometheus::Registry` and define metrics families for counters and histograms.

**Modified File (`tests/load_test/load_test_client.h`):**
```cpp
#pragma once

#include "network/packet_serializer.h"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <memory>
#include <atomic>
#include <vector>
#include <chrono>

// [MODIFIED] Include Prometheus headers
#include <prometheus/registry.h>
#include <prometheus/counter.h>
#include <prometheus/histogram.h>

namespace mmorpg::tests {

class LoadTestClient : public std::enable_shared_from_this<LoadTestClient> {
public:
    struct Config { /* ... same as before ... */ };

    explicit LoadTestClient(Config config);
    ~LoadTestClient();
    void Run();

private:
    // [REMOVED] Old Metrics struct is gone

    // [NEW] Prometheus metrics families
    prometheus::Family<prometheus::Counter>& connections_total;
    prometheus::Family<prometheus::Histogram>& login_latency_seconds;

    // ... (rest of the class)
};

class LoadTestClient::ClientSession : public std::enable_shared_from_this<ClientSession> {
public:
    // [MODIFIED] Constructor now takes references to the metric families
    ClientSession(boost::asio::io_context& io_context, /* ... */,
                  prometheus::Family<prometheus::Counter>& connections_total,
                  prometheus::Family<prometheus::Histogram>& login_latency_seconds);

private:
    // [NEW] Latency measurement
    std::chrono::steady_clock::time_point start_time_;

    // [MODIFIED] References to metric families
    prometheus::Family<prometheus::Counter>& connections_total_;
    prometheus::Family<prometheus::Histogram>& login_latency_seconds_;
};

}
```

**Modified File (`tests/load_test/load_test_client.cpp`):**
```cpp
#include "load_test_client.h"
#include <spdlog/spdlog.h>
#include <prometheus/push.h>
// ...

// In LoadTestClient constructor
LoadTestClient::LoadTestClient(Config config)
    : m_config(std::move(config)),
      m_ssl_context(boost::asio::ssl::context::sslv23_client),
      // [NEW] Initialize the registry and metric families
      m_registry(std::make_shared<prometheus::Registry>()),
      connections_total(prometheus::BuildCounter().Name("loadtest_connections_total").Help("Total connections attempted").Register(*m_registry)),
      login_latency_seconds(prometheus::BuildHistogram().Name("loadtest_login_latency_seconds").Help("Login latency from connect to response").Register(*m_registry)) {
    // ...
    for (uint32_t i = 0; i < m_config.num_clients; ++i) {
        // [MODIFIED] Pass metric families to each session
        m_clients.push_back(std::make_shared<ClientSession>(m_io_context, m_ssl_context, m_config, connections_total, login_latency_seconds));
    }
}

void LoadTestClient::Run() {
    // ... (test runs as before)

    spdlog::info("Stopping load test and pushing metrics...");
    m_io_context.stop();
    
    // [NEW] Push metrics to PushGateway
    try {
        prometheus::PushPusher pusher{m_config.push_gateway_address};
        pusher.Push(*m_registry, "mmorpg_load_test", {{"instance", m_config.host}});
        spdlog::info("Successfully pushed metrics to {}", m_config.push_gateway_address);
    } catch (const std::exception& e) {
        spdlog::error("Failed to push metrics: {}", e.what());
    }
}

// --- ClientSession Implementation ---

// [MODIFIED] Constructor
LoadTestClient::ClientSession::ClientSession(/* ... */, 
    prometheus::Family<prometheus::Counter>& connections_total,
    prometheus::Family<prometheus::Histogram>& login_latency_seconds)
    : /* ... */,
      connections_total_(connections_total),
      login_latency_seconds_(login_latency_seconds) {}

void LoadTestClient::ClientSession::Start() {
    // [NEW] Record start time for latency measurement
    start_time_ = std::chrono::steady_clock::now();
    Connect();
}

void LoadTestClient::ClientSession::OnResolve(const boost::system::error_code& ec, /* ... */) {
    if (ec) {
        // [MODIFIED] Use Prometheus counter with labels
        connections_total_.Add({{"status", "failed_resolve"}}).Increment();
        return;
    }
    // ...
}

// ... other error handlers similarly modified ...

void LoadTestClient::ClientSession::OnHandshake(const boost::system::error_code& ec) {
    if (ec) {
        connections_total_.Add({{"status", "failed_handshake"}}).Increment();
        return;
    }
    // Do not increment success here, wait for login response
    Login();
}

void LoadTestClient::ClientSession::ProcessPacket(std::vector<std::byte>&& data) {
    // ...
    if (packet->header().type() == proto::PACKET_LOGIN_RESPONSE) {
        // [NEW] On successful login, record latency and increment success counter
        auto end_time = std::chrono::steady_clock::now();
        std::chrono::duration<double> duration = end_time - start_time_;
        login_latency_seconds_.Add({}, prometheus::Histogram::BucketBoundaries{0.005, 0.01, 0.025, 0.05, 0.1, 0.25, 0.5, 1, 2.5, 5, 10}).Observe(duration.count());
        connections_total_.Add({{"status", "succeeded"}}).Increment();

        // ...
    }
}
```

### Step 3.3: Add PushGateway Address to Configuration

Update the `main.cpp` to include a command-line option for the PushGateway address.

**Modified File (`tests/load_test/main.cpp`):**
```diff
    app.add_option("-d,--duration", config.test_duration_sec, "Test duration in seconds")->default_val(30);
    app.add_option("--pps", config.packets_per_sec, "Packets per second per client")->default_val(5);
+   app.add_option("--push-gateway", config.push_gateway_address, "Prometheus PushGateway address (e.g., 127.0.0.1:9091)")->default_val("127.0.0.1:9091");
 
    CLI11_PARSE(app, argc, argv);
```

## 4. Rationale for Changes

*   **Prometheus PushGateway**: A standard Prometheus server scrapes metrics from long-running services. Since our load test is a short-lived batch job, it needs to *push* its metrics to an intermediary service before it terminates. The PushGateway is designed for this exact purpose.
*   **Histogram Metrics**: By using `prometheus::Histogram`, we can capture the distribution of login latencies. This is far more valuable than a simple average, as it allows us to calculate percentiles (p50, p95, p99) and understand the user experience for the slowest users.
*   **Labeled Metrics**: The `connections_total` counter now uses a `status` label (e.g., `succeeded`, `failed_resolve`). This allows us to see a breakdown of connection outcomes in a single metric, which is much cleaner than having separate counters for each status.

## 5. Testing and Visualization Guide

1.  **Set up PushGateway**: You need a running instance of the Prometheus PushGateway. You can run it via Docker:
    ```sh
    docker run -d --name pushgateway -p 9091:9091 prom/pushgateway
    ```
2.  **Configure Prometheus**: Add the PushGateway as a target in your `prometheus.yml` configuration file so Prometheus can scrape metrics from it.
    ```yaml
    scrape_configs:
      - job_name: 'pushgateway'
        honor_labels: true
        static_configs:
          - targets: ['localhost:9091']
    ```
3.  **Run the Load Test**: Execute the `load_test_client` and point it to your PushGateway instance.
    ```sh
    ./load_test_client -c 1000 -d 60 --push-gateway="127.0.0.1:9091"
    ```
4.  **Visualize in Grafana**: Once the test is complete and metrics are in Prometheus, you can build a Grafana dashboard. Here are some example PromQL queries:

    *   **Success Rate (as a percentage)**:
        ```promql
        sum(rate(loadtest_connections_total{job="mmorpg_load_test", status="succeeded"}[5m])) / sum(rate(loadtest_connections_total{job="mmorpg_load_test"}[5m])) * 100
        ```
    *   **95th Percentile Login Latency (p95)**:
        ```promql
        histogram_quantile(0.95, sum(rate(loadtest_login_latency_seconds_bucket{job="mmorpg_load_test"}[5m])) by (le))
        ```
    *   **Total Successful Connections per Second**:
        ```promql
        sum(rate(loadtest_connections_total{job="mmorpg_load_test", status="succeeded"}[5m]))
        ```
