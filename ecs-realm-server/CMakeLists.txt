# [SEQUENCE: MVP0-4] CMake build system setup
cmake_minimum_required(VERSION 3.20)
project(mmorpg-server-engine VERSION 1.0.0 LANGUAGES CXX)

# [SEQUENCE: MVP7-13] Force include the conan toolchain to fix build environment issues.
include(conan_toolchain.cmake)

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
find_package(OpenSSL REQUIRED)
find_package(mysql-concpp REQUIRED)
find_package(redis++ REQUIRED)

# Optional packages
find_package(GTest)
find_package(benchmark)

# [SEQUENCE: 3] Protocol Buffers generation
# Find all .proto files in the proto directory
file(GLOB PROTO_FILES "${CMAKE_CURRENT_SOURCE_DIR}/proto/*.proto")

# Create the directory for the generated files
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/proto)

# Create a list to hold the generated source and header files
set(PROTO_SRCS)
set(PROTO_HDRS)

# Loop through each .proto file and set up a custom command to generate C++ files
foreach(PROTO_FILE ${PROTO_FILES})
    # Get the filename without the extension
    get_filename_component(PROTO_FILENAME ${PROTO_FILE} NAME_WE)

    # Define the output source and header files
    set(OUT_SRC "${CMAKE_CURRENT_BINARY_DIR}/proto/${PROTO_FILENAME}.pb.cc")
    set(OUT_HDR "${CMAKE_CURRENT_BINARY_DIR}/proto/${PROTO_FILENAME}.pb.h")

    # Add the generated files to our lists
    list(APPEND PROTO_SRCS ${OUT_SRC})
    list(APPEND PROTO_HDRS ${OUT_HDR})

    # Add a custom command to run protoc
    add_custom_command(
        OUTPUT ${OUT_SRC} ${OUT_HDR}
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        ARGS --cpp_out=${CMAKE_CURRENT_BINARY_DIR}/proto --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
        COMMENT "Generating C++ from ${PROTO_FILE}"
    )
endforeach()

# Add a custom target that depends on all generated files to ensure they are created
add_custom_target(generate_protobuf ALL DEPENDS ${PROTO_SRCS} ${PROTO_HDRS})

# [SEQUENCE: 4] Core library
add_library(mmorpg_core STATIC
    # Core
    src/core/logger.cpp

    # Networking
    src/network/tcp_server.cpp
    src/network/udp_server.cpp
    src/network/session.cpp
    src/network/session_manager.cpp
    src/network/packet_handler.cpp
    src/network/udp_packet_handler.cpp
    src/network/packet_serializer.cpp
    
    # [SEQUENCE: MVP2-18] Build System (`CMakeLists.txt`)
# [SEQUENCE: MVP2-19] The build system is configured to compile the `optimized_world.cpp` and `dense_entity_manager.cpp` files as part of the `mmorpg_core` library.
    # ECS
    src/core/ecs/optimized/dense_entity_manager.cpp
    src/core/ecs/optimized/optimized_world.cpp

    # Database
    src/database/cache_manager.cpp
    src/database/connection_pool.cpp
    src/database/db_monitor.cpp
    src/database/db_partitioning.cpp
    src/database/shard_manager.cpp
    src/database/query_optimizer.cpp
    src/database/read_replica_manager.cpp
    src/database/distributed_lock_manager.cpp

    # Monitoring
    # src/monitoring/ab_testing.cpp
    # src/monitoring/crash_handler.cpp
    src/monitoring/metrics_collector.cpp
    
    # Generated protobuf files
    ${PROTO_SRCS}
)

target_include_directories(mmorpg_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/proto
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
    mysql::concpp
    redis++::redis++_static
)

# [SEQUENCE: 5] Game systems library
# [SEQUENCE: MVP3-11] Build System (`CMakeLists.txt`)
# [SEQUENCE: MVP3-12] Updated to include the source files for `WorldGrid`, `OctreeWorld`, `GridSpatialSystem`, and `OctreeSpatialSystem`.
# [SEQUENCE: MVP4-14] Build System (`CMakeLists.txt`)
# [SEQUENCE: MVP4-15] Updated to include the new combat system source files in the `mmorpg_game` library.
# [SEQUENCE: MVP5-18] Build System (`CMakeLists.txt`)
# [SEQUENCE: MVP5-19] Updated to include the source files for the guild and PvP systems in the `mmorpg_game` library.
add_library(mmorpg_game STATIC
    # Game systems
    # src/game/systems/movement_system.cpp
    # src/game/systems/health_regeneration_system.cpp
    # src/game/systems/network_sync_system.cpp
    # src/game/systems/optimized/optimized_movement_system.cpp
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
    src/game/pvp/pvp_system.cpp
    src/game/handlers/guild_handler.cpp
    src/game/handlers/pvp_handler.cpp
)

target_link_libraries(mmorpg_game PUBLIC mmorpg_core)

# [SEQUENCE: 6] Main server executable
add_executable(mmorpg_server
    src/server/game/main.cpp
)

target_include_directories(mmorpg_server PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/proto
)

target_link_libraries(mmorpg_server PRIVATE mmorpg_core mmorpg_game)

# [SEQUENCE: 7] Load test client
# add_executable(load_test_client
#     # tests/load_test/main.cpp # Restore this
# )
# target_link_libraries(load_test_client PRIVATE mmorpg_core)

# [SEQUENCE: 8] Unit tests
option(BUILD_TESTS "Build unit tests" ON)
if(GTest_FOUND AND BUILD_TESTS)
    enable_testing()
    
    add_executable(unit_tests
        tests/unit/test_ecs_system.cpp
        tests/unit/test_spatial_indexing.cpp
        tests/unit/test_combat_system.cpp
        tests/unit/test_guild_system.cpp
        tests/unit/test_pvp_system.cpp
        tests/unit/test_lock_free_queue.cpp
        tests/unit/test_database_systems.cpp
        # tests/unit/test_networking.cpp
    )
    
    target_link_libraries(unit_tests PRIVATE mmorpg_core mmorpg_game GTest::gtest GTest::gtest_main)
    
    add_test(NAME unit_tests COMMAND unit_tests)
endif()

# [SEQUENCE: 9] Benchmarks
# option(BUILD_BENCHMARKS "Build benchmarks" OFF)
# if(benchmark_FOUND AND BUILD_BENCHMARKS)
#     add_executable(benchmarks
#         # tests/benchmark/bench_ecs.cpp # Restore this
#     )
#     
#     target_link_libraries(benchmarks PRIVATE mmorpg_core mmorpg_game benchmark::benchmark)
# endif()

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