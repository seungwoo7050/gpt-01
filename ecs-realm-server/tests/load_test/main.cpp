#include "load_test_client.h"
#include <CLI/CLI.hpp>
#include <spdlog/spdlog.h>

// [SEQUENCE: MVP6.5-8] Test Scenario
int main(int argc, char* argv[]) {
    CLI::App app{"MMORPG Server Load Test Client"};

    mmorpg::tests::LoadTestClient::Config config;
    app.add_option("-H,--host", config.host, "Server host")->default_val("127.0.0.1");
    app.add_option("-p,--port", config.port, "Server port")->default_val(12345);
    app.add_option("-c,--clients", config.num_clients, "Number of concurrent clients")->default_val(100);
    app.add_option("-d,--duration", config.test_duration_sec, "Test duration in seconds")->default_val(30);
    app.add_option("--pps", config.packets_per_sec, "Packets per second per client")->default_val(5);

    CLI11_PARSE(app, argc, argv);

    try {
        mmorpg::tests::LoadTestClient load_tester(config);
        load_tester.Run();
    } catch (const std::exception& e) {
        spdlog::error("Load test failed with exception: {}", e.what());
        return 1;
    }

    return 0;
}
