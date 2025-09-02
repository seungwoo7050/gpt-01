// [SEQUENCE: MVP0-4] CMake build system setup
cmake_minimum_required(VERSION 3.20)
project(mmorpg-server-engine VERSION 1.0.0 LANGUAGES CXX)

# [SEQUENCE: 1] C++ standard and compiler settings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# [SEQUENCE: 2] Find required packages
find_package(Threads REQUIRED)
find_package(Boost 1.82 REQUIRED COMPONENTS system filesystem thread)
find_package(Protobuf REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json 3.11 REQUIRED)

# Optional packages
find_package(GTest)
find_package(benchmark)

# [SEQUENCE: 3] Protocol Buffers generation
set(PROTO_FILES
    proto/common.proto
    proto/network.proto
    proto/game_messages.proto
)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# [SEQUENCE: 4] Core library
add_library(mmorpg_core STATIC
    # Networking
    src/core/network/tcp_server.cpp
    src/core/network/session.cpp
    src/core/network/session_manager.cpp
    src/core/network/packet_handler.cpp
    src/core/network/packet_serializer.cpp
    
    # ECS
    src/core/ecs/entity.cpp
    src/core/ecs/world.cpp
    src/core/ecs/optimized/dense_entity_manager.cpp
    src/core/ecs/optimized/optimized_world.cpp

    # Concurrency
    src/core/concurrent/lock_free_queue.h
    
    # Database
    src/core/database/partition_manager.cpp
    src/core/database/shard_manager.cpp
    src/database/db_monitor.cpp
    
    # Monitoring
    src/core/monitoring/server_monitor.cpp
    
    # Generated protobuf files
    ${PROTO_SRCS}
)

target_include_directories(mmorpg_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}  # For generated protobuf headers
)

target_link_libraries(mmorpg_core PUBLIC
    Threads::Threads
    Boost::system
    Boost::filesystem
    Boost::thread
    protobuf::libprotobuf
    spdlog::spdlog
    nlohmann_json::nlohmann_json
)

# [SEQUENCE: 5] Game systems library
add_library(mmorpg_game STATIC
    # Game systems
    src/game/systems/movement_system.cpp
    src/game/systems/health_regeneration_system.cpp
    src/game/systems/network_sync_system.cpp
    src/game/systems/optimized/optimized_movement_system.cpp
    src/game/world/grid/world_grid.cpp
    src/game/world/octree/octree_world.cpp
    src/game/systems/grid_spatial_system.cpp
    src/game/systems/octree_spatial_system.cpp
    src/game/systems/combat/targeted_combat_system.cpp
    src/game/systems/combat/action_combat_system.cpp
    src/game/systems/guild/guild_war_instanced_system.cpp
    src/game/systems/guild/guild_war_seamless_system.cpp
    src/game/systems/pvp/openworld_pvp_system.cpp
    src/game/systems/pvp/arena_system.cpp
)

target_link_libraries(mmorpg_game PUBLIC
    mmorpg_core
)

# [SEQUENCE: 6] Main server executable
add_executable(mmorpg_server
    src/server/game/main.cpp
    src/server/game_server.cpp
    src/server/http_metrics_server.cpp
)

target_link_libraries(mmorpg_server PRIVATE
    mmorpg_core
    mmorpg_game
)

# [SEQUENCE: 7] Load test client
add_executable(load_test_client
    tests/load_test/main.cpp
    tests/load_test/load_test_client.cpp
)

target_link_libraries(load_test_client PRIVATE
    mmorpg_core
    CLI11::CLI11  # Would need to add CLI11 as dependency
)

# [SEQUENCE: 8] Unit tests
if(GTest_FOUND AND BUILD_TESTS)
    enable_testing()
    
    add_executable(unit_tests
        tests/unit/test_ecs.cpp
        tests/unit/test_spatial.cpp
        tests/unit/test_combat.cpp
        tests/unit/test_networking.cpp
    )
    
    target_link_libraries(unit_tests PRIVATE
        mmorpg_core
        mmorpg_game
        GTest::gtest
        GTest::gtest_main
    )
    
    add_test(NAME unit_tests COMMAND unit_tests)
endif()

# [SEQUENCE: 9] Benchmarks
if(benchmark_FOUND AND BUILD_BENCHMARKS)
    add_executable(benchmarks
        tests/benchmark/bench_ecs.cpp
        tests/benchmark/bench_spatial.cpp
        tests/benchmark/bench_network.cpp
    )
    
    target_link_libraries(benchmarks PRIVATE
        mmorpg_core
        mmorpg_game
        benchmark::benchmark
    )
endif()

# [SEQUENCE: 10] Installation
install(TARGETS mmorpg_server
    RUNTIME DESTINATION bin
)

install(DIRECTORY config/
    DESTINATION etc/mmorpg-server
)

# [SEQUENCE: 11] CPack for packaging
set(CPACK_PACKAGE_VENDOR "MMORPG Team")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "High-performance MMORPG server engine")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_GENERATOR "TGZ;DEB;RPM")

include(CPack)

# [SEQUENCE: 12] Build options
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
option(ENABLE_PROFILING "Enable profiling support" OFF)
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_TSAN "Enable ThreadSanitizer" OFF)

# Sanitizers
if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
elseif(ENABLE_TSAN)
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
endif()

# Profiling
if(ENABLE_PROFILING)
    add_compile_options(-pg)
    add_link_options(-pg)
endif()

# [SEQUENCE: 13] Print configuration
message(STATUS "=== MMORPG Server Build Configuration ===")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Boost version: ${Boost_VERSION}")
message(STATUS "Protobuf version: ${Protobuf_VERSION}")
message(STATUS "Build tests: ${BUILD_TESTS}")
message(STATUS "Build benchmarks: ${BUILD_BENCHMARKS}")
message(STATUS "Enable profiling: ${ENABLE_PROFILING}")
message(STATUS "Enable ASAN: ${ENABLE_ASAN}")
message(STATUS "Enable TSAN: ${ENABLE_TSAN}")
message(STATUS "=========================================")

# [SEQUENCE: 14] Custom targets
add_custom_target(format
    COMMAND find ${CMAKE_SOURCE_DIR}/src -name '*.cpp' -o -name '*.h' | xargs clang-format -i
    COMMENT "Formatting source code..."
)

add_custom_target(lint
    COMMAND clang-tidy ${CMAKE_SOURCE_DIR}/src/**/*.cpp -- -I${CMAKE_SOURCE_DIR}/src
    COMMENT "Running clang-tidy..."
)