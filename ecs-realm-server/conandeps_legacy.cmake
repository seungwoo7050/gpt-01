message(STATUS "Conan: Using CMakeDeps conandeps_legacy.cmake aggregator via include()")
message(STATUS "Conan: It is recommended to use explicit find_package() per dependency instead")

find_package(Boost)
find_package(spdlog)
find_package(GTest)
find_package(nlohmann_json)
find_package(benchmark)
find_package(mysql-concpp)
find_package(protobuf)
find_package(redis++)
find_package(sol2)

set(CONANDEPS_LEGACY  boost::boost  spdlog::spdlog  gtest::gtest  nlohmann_json::nlohmann_json  benchmark::benchmark_main  mysql::concpp  protobuf::protobuf  redis++::redis++_static  sol2::sol2 )