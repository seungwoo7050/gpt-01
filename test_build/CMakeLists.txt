cmake_minimum_required(VERSION 3.20)
project(test_build)

set(CMAKE_CXX_STANDARD 20)

add_executable(test_build main.cpp)

target_include_directories(test_build PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../ecs-realm-server/src)
target_include_directories(test_build PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../ecs-realm-server/build)

target_link_libraries(test_build PRIVATE mmorpg_core)
