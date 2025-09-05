# [SEQUENCE: MVP0-4] CMake build system setup
cmake_minimum_required(VERSION 3.20)
project(mmorpg-server-engine VERSION 1.0.0 LANGUAGES CXX)

# [SEQUENCE: MVP7-13] Include the conan toolchain file to ensure all packages are found correctly.
include(conan_toolchain.cmake)



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

find_package(Threads REQUIRED)
find_package(Boost 1.82 REQUIRED COMPONENTS system filesystem thread)
find_package(Protobuf REQUIRED)
find_package(spdlog REQUIRED)
find_package(nlohmann_json 3.11 REQUIRED)
find_package(OpenSSL REQUIRED)
# [SEQUENCE: MVP7-2] Find packages for database and cache connectors.
find_package(mysql-concpp REQUIRED)
find_package(redis++ REQUIRED)
# [SEQUENCE: MVP8-2] Find package for Lua scripting engine.
find_package(sol2 REQUIRED)

# Optional packages
find_package(GTest)
find_package(benchmark)
find_package(CLI11 REQUIRED)

# Protocol Buffers generation
file(GLOB PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/proto/*.proto")
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/proto)
set(PROTO_SRCS)
set(PROTO_HDRS)
foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_FILENAME ${PROTO_FILE} NAME_WE)
    set(OUT_SRC "${CMAKE_CURRENT_BINARY_DIR}/proto/${PROTO_FILENAME}.pb.cc")
    set(OUT_HDR "${CMAKE_CURRENT_BINARY_DIR}/proto/${PROTO_FILENAME}.pb.h")
    list(APPEND PROTO_SRCS ${OUT_SRC})
    list(APPEND PROTO_HDRS ${OUT_HDR})
    add_custom_command(
        OUTPUT ${OUT_SRC} ${OUT_HDR}
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        ARGS --cpp_out=${CMAKE_CURRENT_BINARY_DIR}/proto --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
        COMMENT "Generating C++ from ${PROTO_FILE}"
    )
endforeach()
add_custom_target(generate_protobuf ALL DEPENDS ${PROTO_SRCS} ${PROTO_HDRS})

# [SEQUENCE: MVP8-3] Add all core source files, including the new script manager.
add_library(mmorpg_core STATIC
    # Core
    src/core/logger.cpp
    src/core/ecs/optimized/dense_entity_manager.cpp
    src/core/ecs/optimized/optimized_world.cpp
    src/core/scripting/script_manager.cpp

    # Networking
    src/network/tcp_server.cpp
    src/network/udp_server.cpp
    src/network/session.cpp
    src/network/session_manager.cpp
    src/network/packet_handler.cpp
    src/network/udp_packet_handler.cpp
    src/network/packet_serializer.cpp
    src/network/guild_handler.cpp
    src/network/pvp_handler.cpp

    # Database
    src/database/connection_pool.cpp
    src/database/db_monitor.cpp
    src/database/cache_manager.cpp
    src/database/query_optimizer.cpp
    src/database/read_replica_manager.cpp
    src/database/db_partitioning.cpp
    src/database/shard_manager.cpp
    src/database/distributed_lock_manager.cpp

    # Monitoring
    src/monitoring/metrics_collector.cpp
    
    # Generated protobuf files
    ${PROTO_SRCS}
)

target_include_directories(mmorpg_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/proto
    ${sol2_INCLUDE_DIRS}
    ${lua_INCLUDE_DIRS}
)

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
)

# [SEQUENCE: MVP5-102] Adds Guild and PvP systems to the game library.
add_library(mmorpg_game STATIC
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
    src/game/systems/pvp_manager.cpp
    src/game/social/guild_manager.cpp
)
target_link_libraries(mmorpg_game PUBLIC mmorpg_core)

add_executable(mmorpg_server
    src/server/game/main.cpp
)

target_include_directories(mmorpg_server PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/proto
)

target_link_libraries(mmorpg_server PRIVATE mmorpg_core mmorpg_game sol2::sol2)

option(BUILD_TESTS "Build unit tests" ON)
if(GTest_FOUND AND BUILD_TESTS)
    enable_testing()
    
    # [SEQUENCE: MVP6-39] Add the new lock-free queue unit test to the test executable.
    add_executable(unit_tests
        tests/unit/test_lock_free_queue.cpp
        tests/unit/test_ecs_system.cpp
        tests/unit/test_spatial_indexing.cpp
        tests/unit/test_combat_system.cpp
        tests/unit/test_guild_system.cpp
        tests/unit/test_pvp_system.cpp
    )
    
    target_link_libraries(unit_tests PRIVATE mmorpg_core mmorpg_game GTest::gtest GTest::gtest_main)
    
    add_test(NAME unit_tests COMMAND unit_tests)

    # [SEQUENCE: MVP9-4] Add the load test client executable.
    add_executable(load_test_client
        tests/load_test/main.cpp
        tests/load_test/load_test_client.cpp
    )
    target_link_libraries(load_test_client PRIVATE mmorpg_core Boost::program_options spdlog::spdlog CLI11::CLI11)
endif()

install(TARGETS mmorpg_server
    RUNTIME DESTINATION bin
)

install(DIRECTORY config/
    DESTINATION etc/mmorpg-server
)

include(CPack)


