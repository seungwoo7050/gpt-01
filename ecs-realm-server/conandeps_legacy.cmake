message(STATUS "Conan: Using CMakeDeps conandeps_legacy.cmake aggregator via include()")
message(STATUS "Conan: It is recommended to use explicit find_package() per dependency instead")

find_package(Boost)
find_package(protobuf)
find_package(spdlog)
find_package(GTest)
find_package(nlohmann_json)
find_package(benchmark)

set(CONANDEPS_LEGACY  boost::boost  protobuf::protobuf  spdlog::spdlog  gtest::gtest  nlohmann_json::nlohmann_json  benchmark::benchmark_main )