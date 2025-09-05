# Extension 01: Prometheus-based Metrics Expansion

## 1. Objective

This guide provides a complete walkthrough for integrating a robust, industry-standard metrics system into the server using `prometheus-cpp`. The goal is to replace the basic, internal-only `MetricsCollector` with a powerful system capable of exposing detailed performance metrics (Counters, Gauges, Histograms) over an HTTP endpoint for collection by a Prometheus server.

## 2. Analysis of Current Implementation

The current metrics system is rudimentary and serves only as an internal placeholder.

*   **`conanfile.txt` & `CMakeLists.txt`**: These files lack any dependency on a real metrics library like `prometheus-cpp`.
*   **`src/monitoring/metrics_collector.h` & `.cpp`**: The `MetricsCollector` class is a simple wrapper around `std::unordered_map`. It can only store values internally and has no mechanism to expose them to the outside world. It also lacks support for crucial metric types like Histograms, which are essential for tracking latency distributions (p50, p95, p99).

**Legacy Code (`src/monitoring/metrics_collector.h`):**
```cpp
#pragma once

#include <string>
#include <unordered_map>
#include <mutex>

namespace mmorpg::monitoring {

class MetricsCollector {
public:
    void RecordGauge(const std::string& name, double value);
    void RecordCounter(const std::string& name, uint64_t value);

private:
    std::unordered_map<std::string, double> gauges_;
    std::unordered_map<std::string, uint64_t> counters_;
    mutable std::mutex mutex_;
};

}
```

## 3. Proposed Implementation

### Step 3.1: Add `prometheus-cpp` Dependency

First, add the `prometheus-cpp` library to your list of dependencies.

**Modified File (`ecs-realm-server/conanfile.txt`):**
```diff
# [SEQUENCE: MVP0-6] Conan dependency management
[requires]
boost/1.82.0
protobuf/3.21.12
spdlog/1.12.0
gtest/1.14.0
nlohmann_json/3.11.3
benchmark/1.8.3
openssl/3.0.7
# [SEQUENCE: MVP7-1] Add database and distributed cache dependencies.
mysql-connector-cpp/9.2.0
redis-plus-plus/1.3.8
# [SEQUENCE: MVP8-1] Add Lua scripting engine dependency.
sol2/3.3.1
cli11/2.3.2
zlib/1.2.13 # Explicitly added to resolve version conflict from openssl
+ # [EXTENSION-01] Add prometheus-cpp for metrics
+ prometheus-cpp/1.1.0

[generators]
CMakeDeps
CMakeToolchain

[options]
boost/*:shared=False
gtest/*:shared=False
openssl/*:shared=False

```

### Step 3.2: Update CMake to Find and Link `prometheus-cpp`

Next, instruct CMake to find the new package and link it to the `mmorpg_core` library.

**Modified File (`ecs-realm-server/CMakeLists.txt`):**
```diff
find_package(OpenSSL REQUIRED)
# [SEQUENCE: MVP7-2] Find packages for database and cache connectors.
find_package(mysql-concpp REQUIRED)
find_package(redis++ REQUIRED)
# [SEQUENCE: MVP8-2] Find package for Lua scripting engine.
find_package(sol2 REQUIRED)
+ # [EXTENSION-01] Find package for Prometheus metrics
+ find_package(prometheus-cpp REQUIRED)

# Optional packages
find_package(GTest)
# ... (rest of the file is the same until target_link_libraries)

# ...

target_link_libraries(mmorpg_core PUBLIC
    Threads::Threads
    Boost::system
    Boost::filesystem
    Boost::thread
    protobuf::libprotobuf
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
    # [SEQUENCE: MVP7-3] Link database and cache libraries.
    mysql::concpp
    redis++::redis++_static
+   # [EXTENSION-01] Link Prometheus libraries
+   prometheus-cpp::core
+   prometheus-cpp::pull
)

# ... (rest of the file)
```

### Step 3.3: Refactor `MetricsCollector`

This is the core part of the change. We will transform `MetricsCollector` from a simple data store into a manager for real Prometheus metric objects.

**Modified File (`ecs-realm-server/src/monitoring/metrics_collector.h`):**
```cpp
#pragma once

#include <prometheus/registry.h>
#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>
#include <memory>
#include <string>
#include <vector>

namespace mmorpg::monitoring {

class MetricsCollector {
public:
    static MetricsCollector& Instance();

    void Initialize(std::shared_ptr<prometheus::Registry> registry);

    // Typed metric creation and retrieval
    prometheus::Counter& GetCounter(const std::string& name, const std::string& help);
    prometheus::Gauge& GetGauge(const std::string& name, const std::string& help);
    prometheus::Histogram& GetHistogram(const std::string& name, const std::string& help, const std::vector<double>& buckets);

    std::shared_ptr<prometheus::Registry> GetRegistry();

private:
    MetricsCollector() = default;
    ~MetricsCollector() = default;
    MetricsCollector(const MetricsCollector&) = delete;
    MetricsCollector& operator=(const MetricsCollector&) = delete;

    std::shared_ptr<prometheus::Registry> registry_;
};

}
```

**Modified File (`ecs-realm-server/src/monitoring/metrics_collector.cpp`):**
```cpp
#include "monitoring/metrics_collector.h"
#include <prometheus/family.h>

namespace mmorpg::monitoring {

MetricsCollector& MetricsCollector::Instance() {
    static MetricsCollector instance;
    return instance;
}

void MetricsCollector::Initialize(std::shared_ptr<prometheus::Registry> registry) {
    registry_ = std::move(registry);
}

prometheus::Counter& MetricsCollector::GetCounter(const std::string& name, const std::string& help) {
    auto& family = prometheus::BuildCounter().Name(name).Help(help).Register(*registry_);
    return family.Add({});
}

prometheus::Gauge& MetricsCollector::GetGauge(const std::string& name, const std::string& help) {
    auto& family = prometheus::BuildGauge().Name(name).Help(help).Register(*registry_);
    return family.Add({});
}

prometheus::Histogram& MetricsCollector::GetHistogram(const std::string& name, const std::string& help, const std::vector<double>& buckets) {
    auto& family = prometheus::BuildHistogram().Name(name).Help(help).Register(*registry_);
    return family.Add({}, buckets);
}

std::shared_ptr<prometheus::Registry> MetricsCollector::GetRegistry() {
    return registry_;
}

}
```

### Step 3.4: Expose Metrics via an HTTP Endpoint

To make the metrics accessible to a Prometheus server, you need to run an "Exposer" in a background thread.

**Modified File (`ecs-realm-server/src/server/game/main.cpp`):**
```cpp
#include "core/logger.h"
#include "network/tcp_server.h"
#include "network/packet_handler.h"
#include <iostream>
#include <memory>

// [EXTENSION-01] Include Prometheus headers
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include "monitoring/metrics_collector.h"

int main() {
    // ... (Logger setup)

    // [EXTENSION-01] Setup Prometheus metrics
    // Create a new registry
    auto registry = std::make_shared<prometheus::Registry>();
    // Initialize the singleton with the registry
    mmorpg::monitoring::MetricsCollector::Instance().Initialize(registry);
    // Create an exposer that will listen on port 8080
    prometheus::Exposer exposer{"127.0.0.1:8080"};
    // Register the registry with the exposer
    exposer.RegisterCollectable(registry);

    // [EXTENSION-01] Example of creating and using a metric
    auto& connections_gauge = mmorpg::monitoring::MetricsCollector::Instance().GetGauge(
        "server_active_connections", "Number of active connections");
    connections_gauge.Set(0); // Example usage

    // ... (Server startup logic)

    // ...
    return 0;
}
```

## 4. Rationale for Changes

*   **Dependency:** `prometheus-cpp` is the de-facto standard C++ client for Prometheus, providing all the necessary data structures and an HTTP server (Exposer).
*   **Singleton and Registry:** The `MetricsCollector` is now a singleton that holds a `std::shared_ptr<prometheus::Registry>`. The registry is the central object that owns all the metrics. This pattern allows any part of the application to access the `MetricsCollector::Instance()` and create or update metrics without needing to pass the registry around.
*   **Metric Types:** The new `GetCounter`, `GetGauge`, and `GetHistogram` methods provide a clean, typed API for metric management.
*   **Exposer:** The `prometheus::Exposer` runs its own lightweight HTTP server in a background thread, responding to requests on the `/metrics` endpoint. This is the standard way Prometheus scrapes targets.

## 5. Testing Guide

1.  **Run `conan install`**: After modifying `conanfile.txt`, run `conan install ecs-realm-server/ --output-folder=ecs-realm-server/build --build=missing` to download the new dependency.
2.  **Re-build the Server**: Build the `mmorpg_server` executable.
3.  **Run the Server**: Start the server.
4.  **Verify the Endpoint**: Open a web browser or use a command-line tool like `curl` to access the metrics endpoint: `curl http://127.0.0.1:8080/metrics`.
5.  **Check the Output**: You should see the Prometheus-formatted metrics, including the example one we created:
    ```
    # HELP server_active_connections Number of active connections
    # TYPE server_active_connections gauge
    server_active_connections 0
    ```
6.  **Unit Test (Optional but Recommended)**: Create a new test file `tests/unit/test_metrics_collector.cpp` to verify the logic:

    ```cpp
    #include <gtest/gtest.h>
    #include "monitoring/metrics_collector.h"
    #include <prometheus/registry.h>
    #include <prometheus/counter.h>

    TEST(MetricsCollectorTest, GetOrCreateCounter) {
        auto registry = std::make_shared<prometheus::Registry>();
        mmorpg::monitoring::MetricsCollector::Instance().Initialize(registry);

        auto& counter1 = mmorpg::monitoring::MetricsCollector::Instance().GetCounter("test_counter", "A test counter");
        counter1.Increment();

        // Requesting it again should return the same instance
        auto& counter2 = mmorpg::monitoring::MetricsCollector::Instance().GetCounter("test_counter", "A test counter");
        EXPECT_EQ(&counter1, &counter2);
        counter2.Increment(2);

        // The value should be 3
        EXPECT_EQ(counter1.Value(), 3.0);
    }
    ```
    Add this new test file to the `unit_tests` executable in `CMakeLists.txt` and run it.
