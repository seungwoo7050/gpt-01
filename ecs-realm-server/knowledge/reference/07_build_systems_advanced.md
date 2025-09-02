# 고급 빌드 시스템 마스터 - CMake와 현대 C++ 프로젝트 관리
*복잡한 C++ 게임 서버 프로젝트를 위한 전문가급 빌드 시스템 구축*

> **🎯 목표**: CMake 고급 기능을 마스터하여 대규모 C++ 게임 서버 프로젝트의 빌드, 패키징, 배포를 완전히 자동화하고 최적화

---

## 📋 문서 정보

- **난이도**: 🔴 고급
- **예상 학습 시간**: 4-6시간
- **필요 선수 지식**: 
  - ✅ C++ 기본 문법과 프로젝트 구조
  - ✅ CMake 기초 사용 경험
  - ✅ 라이브러리와 링킹 개념
- **실습 환경**: CMake 3.20 이상, C++17/20 지원 컴파일러

---

## 🤔 왜 고급 빌드 시스템이 게임 서버에 중요할까?

### **복잡한 게임 서버 프로젝트의 현실**

```cmake
# 😰 기초적인 CMakeLists.txt의 한계
cmake_minimum_required(VERSION 3.10)
project(GameServer)

set(CMAKE_CXX_STANDARD 17)

# 문제 1: 하드코딩된 의존성 경로
include_directories(/usr/local/include)
link_directories(/usr/local/lib)

# 문제 2: 모든 소스가 한 곳에
file(GLOB_RECURSE SOURCES "src/*.cpp")

# 문제 3: 단일 실행파일만 생성
add_executable(GameServer ${SOURCES})

# 문제 4: 수동 라이브러리 링킹
target_link_libraries(GameServer boost_system boost_asio mysql++ redis++)

# 결과: 유지보수 불가능한 빌드 시스템
# - 다른 환경에서 빌드 실패
# - 의존성 버전 관리 불가
# - 테스트/패키징/배포 자동화 불가
```

### **현대적인 고급 빌드 시스템**

```cmake
# ✨ 전문가급 CMakeLists.txt 구조
cmake_minimum_required(VERSION 3.20)

# 프로젝트 메타데이터와 버전 관리
project(GameServer 
    VERSION 1.0.0
    DESCRIPTION "High-Performance Multiplayer Game Server"
    LANGUAGES CXX)

# 모던 C++ 설정
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 빌드 타입별 최적화
include(cmake/CompilerSettings.cmake)
include(cmake/Dependencies.cmake)
include(cmake/Testing.cmake)
include(cmake/Packaging.cmake)

# 모듈화된 타겟들
add_subdirectory(src/core)
add_subdirectory(src/network)  
add_subdirectory(src/game)
add_subdirectory(src/database)
add_subdirectory(tests)
add_subdirectory(tools)

# 자동 설치 및 패키징
install(TARGETS GameServer DESTINATION bin)
include(CPack)

# 결과: 전문가급 빌드 시스템
# - 크로스 플랫폼 호환성
# - 자동 의존성 관리
# - 테스트/패키징 자동화
# - CI/CD 완전 통합
```

---

## 📚 1. 모던 CMake 기법과 타겟 중심 설계

### **1.1 Interface Libraries와 IMPORTED 타겟**

```cmake
# 🎯 현대적인 타겟 중심 CMake 설계

# 컴파일러 설정을 Interface Library로 관리
add_library(compiler_settings INTERFACE)

target_compile_features(compiler_settings INTERFACE 
    cxx_std_20
    cxx_constexpr
    cxx_lambda_init_captures
)

# 빌드 타입별 컴파일 옵션
target_compile_options(compiler_settings INTERFACE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    $<$<CONFIG:Debug>:-g -O0 -DDEBUG>
    $<$<CONFIG:Release>:-O3 -DNDEBUG -march=native>
    $<$<CONFIG:RelWithDebInfo>:-O2 -g>
)

# 게임 서버 공통 설정
add_library(game_server_common INTERFACE)

target_compile_definitions(game_server_common INTERFACE
    $<$<CONFIG:Debug>:GAME_DEBUG_MODE>
    $<$<CONFIG:Release>:GAME_RELEASE_MODE>
    GAME_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    GAME_VERSION_MINOR=${PROJECT_VERSION_MINOR}
)

target_include_directories(game_server_common INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# 의존성을 IMPORTED 타겟으로 관리
function(create_imported_target target_name lib_path include_path)
    add_library(${target_name} UNKNOWN IMPORTED)
    set_target_properties(${target_name} PROPERTIES
        IMPORTED_LOCATION ${lib_path}
        INTERFACE_INCLUDE_DIRECTORIES ${include_path}
    )
endfunction()

# Boost 라이브러리 세팅 (find_package 전에)
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)

find_package(Boost 1.75 REQUIRED COMPONENTS system thread asio)

# 커스텀 Boost 타겟 생성 (더 나은 인터페이스)
if(TARGET Boost::boost)
    add_library(GameServer::Boost ALIAS Boost::boost)
else()
    # 폴백: 수동으로 타겟 생성
    add_library(GameServer::Boost INTERFACE IMPORTED)
    target_include_directories(GameServer::Boost INTERFACE ${Boost_INCLUDE_DIRS})
    target_link_libraries(GameServer::Boost INTERFACE ${Boost_LIBRARIES})
endif()
```

### **1.2 제너레이터 표현식과 조건부 빌드**

```cmake
# ⚙️ 고급 제너레이터 표현식 활용

# 플랫폼별 소스 파일 포함
set(PLATFORM_SOURCES
    src/platform/base_platform.cpp
    $<$<PLATFORM_ID:Windows>:src/platform/windows_platform.cpp>
    $<$<PLATFORM_ID:Linux>:src/platform/linux_platform.cpp>
    $<$<PLATFORM_ID:Darwin>:src/platform/macos_platform.cpp>
)

# 컴파일러별 최적화 플래그
target_compile_options(GameServer PRIVATE
    # GCC/Clang 최적화
    $<$<CXX_COMPILER_ID:GNU,Clang>:
        -ffast-math
        -fno-rtti
        $<$<CONFIG:Release>:-flto>
        $<$<CONFIG:Debug>:-fsanitize=address>
    >
    
    # MSVC 최적화
    $<$<CXX_COMPILER_ID:MSVC>:
        /fp:fast
        /GR-
        $<$<CONFIG:Release>:/GL /Gy>
        $<$<CONFIG:Debug>:/fsanitize=address>
    >
)

# 아키텍처별 정의
target_compile_definitions(GameServer PRIVATE
    $<$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>:ARCH_64BIT>
    $<$<EQUAL:${CMAKE_SIZEOF_VOID_P},4>:ARCH_32BIT>
    $<$<BOOL:${CMAKE_CROSSCOMPILING}>:CROSS_COMPILING>
)

# 조건부 의존성 링킹
target_link_libraries(GameServer PRIVATE
    GameServer::Core
    GameServer::Network
    
    # Windows 전용 라이브러리
    $<$<PLATFORM_ID:Windows>:ws2_32 mswsock>
    
    # Linux/macOS 전용 라이브러리
    $<$<NOT:$<PLATFORM_ID:Windows>>:pthread>
    
    # 디버그 모드에서만 디버깅 라이브러리
    $<$<CONFIG:Debug>:GameServer::Debug>
)

# 기능별 조건부 컴파일
option(ENABLE_PROFILING "Enable performance profiling" OFF)
option(ENABLE_METRICS "Enable metrics collection" ON)
option(ENABLE_TESTING "Build with testing support" OFF)

target_compile_definitions(GameServer PRIVATE
    $<$<BOOL:${ENABLE_PROFILING}>:PROFILING_ENABLED>
    $<$<BOOL:${ENABLE_METRICS}>:METRICS_ENABLED>
    $<$<BOOL:${ENABLE_TESTING}>:TESTING_ENABLED>
)

# 조건부 소스 포함
if(ENABLE_PROFILING)
    target_sources(GameServer PRIVATE src/profiling/profiler.cpp)
    target_link_libraries(GameServer PRIVATE gperftools::profiler)
endif()

if(ENABLE_METRICS)
    target_sources(GameServer PRIVATE src/metrics/collector.cpp)
    find_package(Prometheus REQUIRED)
    target_link_libraries(GameServer PRIVATE prometheus-cpp::prometheus-cpp)
endif()
```

---

## 📚 2. 의존성 관리와 패키지 시스템

### **2.1 FetchContent와 외부 프로젝트 관리**

```cmake
# 📦 현대적인 의존성 관리 - FetchContent 활용

include(FetchContent)

# 의존성 버전 중앙 관리
set(SPDLOG_VERSION "1.10.0")
set(FMT_VERSION "9.1.0")
set(JSON_VERSION "3.11.2")
set(CATCH2_VERSION "3.1.0")

# 고성능 로깅 라이브러리
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v${SPDLOG_VERSION}
    GIT_SHALLOW TRUE
)

# JSON 라이브러리 (헤더 온리)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v${JSON_VERSION}
    GIT_SHALLOW TRUE
)

# 테스트 프레임워크 (조건부)
if(ENABLE_TESTING)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v${CATCH2_VERSION}
        GIT_SHALLOW TRUE
    )
endif()

# 의존성 사용 가능하게 만들기
FetchContent_MakeAvailable(spdlog nlohmann_json)

if(ENABLE_TESTING)
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(Catch)
endif()

# 커스텀 Find 모듈로 복잡한 의존성 처리
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules)

find_package(MySQL REQUIRED)
find_package(Redis REQUIRED)
find_package(ProtocolBuffers REQUIRED)

# 의존성 정보 출력 (디버깅용)
function(print_dependency_info target)
    message(STATUS "=== Dependency Info for ${target} ===")
    get_target_property(DEPS ${target} LINK_LIBRARIES)
    if(DEPS)
        message(STATUS "Link libraries: ${DEPS}")
    endif()
    
    get_target_property(INCLUDES ${target} INTERFACE_INCLUDE_DIRECTORIES)
    if(INCLUDES)
        message(STATUS "Include directories: ${INCLUDES}")
    endif()
endfunction()

# 사용 예
print_dependency_info(GameServer)
```

### **2.2 커스텀 Find 모듈 작성**

```cmake
# cmake/modules/FindRedis.cmake - 커스텀 Find 모듈 예제

#[=======================================================================[
FindRedis
--------

Find the Redis C++ client library (redis-plus-plus)

IMPORTED Targets
^^^^^^^^^^^^^^^^

``Redis::Redis``
  The Redis library, if found.

Result Variables
^^^^^^^^^^^^^^^^

``Redis_FOUND``
  True if Redis was found, false otherwise.
``Redis_VERSION``
  The version of Redis found.
``Redis_INCLUDE_DIRS``
  Include directories for Redis.
``Redis_LIBRARIES``
  Libraries to link for Redis.

Hints
^^^^^

Set ``Redis_ROOT`` to a directory that contains a Redis installation.

#]=======================================================================]

# 라이브러리 검색 경로들
set(_Redis_SEARCH_PATHS
    ${Redis_ROOT}
    $ENV{Redis_ROOT}
    /usr/local
    /usr
    /opt/local
    /opt
)

# 헤더 파일 찾기
find_path(Redis_INCLUDE_DIR
    NAMES sw/redis++/redis++.h
    PATHS ${_Redis_SEARCH_PATHS}
    PATH_SUFFIXES include
    DOC "Redis include directory"
)

# 라이브러리 파일 찾기
find_library(Redis_LIBRARY
    NAMES redis++ redis++-static
    PATHS ${_Redis_SEARCH_PATHS}
    PATH_SUFFIXES lib lib64
    DOC "Redis library"
)

# hiredis 의존성도 필요
find_library(HiRedis_LIBRARY
    NAMES hiredis
    PATHS ${_Redis_SEARCH_PATHS}
    PATH_SUFFIXES lib lib64
    DOC "HiRedis library"
)

# 버전 정보 추출 (헤더에서)
if(Redis_INCLUDE_DIR)
    file(READ "${Redis_INCLUDE_DIR}/sw/redis++/version.h" _Redis_VERSION_CONTENT)
    string(REGEX MATCH "#define REDISPLUSPLUS_VERSION \"([0-9]+\\.[0-9]+\\.[0-9]+)\"" 
           _Redis_VERSION_MATCH "${_Redis_VERSION_CONTENT}")
    if(_Redis_VERSION_MATCH)
        set(Redis_VERSION "${CMAKE_MATCH_1}")
    endif()
endif()

# FindPackageHandleStandardArgs 사용
include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Redis
    REQUIRED_VARS Redis_LIBRARY HiRedis_LIBRARY Redis_INCLUDE_DIR
    VERSION_VAR Redis_VERSION
)

# IMPORTED 타겟 생성
if(Redis_FOUND AND NOT TARGET Redis::Redis)
    add_library(Redis::Redis UNKNOWN IMPORTED)
    
    set_target_properties(Redis::Redis PROPERTIES
        IMPORTED_LOCATION ${Redis_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES ${Redis_INCLUDE_DIR}
        INTERFACE_LINK_LIBRARIES ${HiRedis_LIBRARY}
    )
    
    # 플랫폼별 추가 의존성
    if(WIN32)
        set_property(TARGET Redis::Redis APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES ws2_32)
    endif()
endif()

# 변수 설정 (하위 호환성)
if(Redis_FOUND)
    set(Redis_LIBRARIES ${Redis_LIBRARY} ${HiRedis_LIBRARY})
    set(Redis_INCLUDE_DIRS ${Redis_INCLUDE_DIR})
endif()

# 디버그 정보 출력
if(Redis_FOUND)
    message(STATUS "Found Redis: ${Redis_LIBRARY} (version ${Redis_VERSION})")
endif()

# 캐시 변수로 표시 (고급 옵션에서 수정 가능)
mark_as_advanced(Redis_INCLUDE_DIR Redis_LIBRARY HiRedis_LIBRARY)
```

---

## 📚 3. 테스트 자동화와 품질 보증

### **3.1 CTest와 통합된 테스트 시스템**

```cmake
# cmake/Testing.cmake - 종합적인 테스트 설정

if(ENABLE_TESTING)
    enable_testing()
    include(CTest)
    
    # 테스트 관련 옵션들
    option(BUILD_TESTING "Build tests" ON)
    option(ENABLE_COVERAGE "Enable code coverage" OFF)
    option(ENABLE_SANITIZERS "Enable sanitizers" OFF)
    option(ENABLE_STATIC_ANALYSIS "Enable static analysis" OFF)
    
    # 커버리지 설정
    if(ENABLE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage -fprofile-arcs -ftest-coverage")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
        
        # gcovr 또는 lcov 찾기
        find_program(GCOVR_PATH gcovr)
        if(GCOVR_PATH)
            add_custom_target(coverage
                COMMAND ${GCOVR_PATH} --root ${CMAKE_SOURCE_DIR} --html --html-details -o coverage.html
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Generating code coverage report"
            )
        endif()
    endif()
    
    # 새니타이저 설정
    if(ENABLE_SANITIZERS)
        set(SANITIZER_FLAGS "-fsanitize=address,undefined,leak -fno-omit-frame-pointer")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS}")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SANITIZER_FLAGS}")
    endif()
    
    # 정적 분석 도구 설정
    if(ENABLE_STATIC_ANALYSIS)
        find_program(CLANG_TIDY_COMMAND clang-tidy)
        if(CLANG_TIDY_COMMAND)
            set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND};-checks=*,-fuchsia-*,-google-readability-*")
        endif()
        
        find_program(CPPCHECK_COMMAND cppcheck)
        if(CPPCHECK_COMMAND)
            set(CMAKE_CXX_CPPCHECK "${CPPCHECK_COMMAND};--enable=all;--suppress=missingIncludeSystem")
        endif()
    endif()
    
    # 테스트 유틸리티 함수
    function(add_game_test test_name)
        cmake_parse_arguments(TEST "" "WORKING_DIRECTORY" "SOURCES;LIBRARIES" ${ARGN})
        
        add_executable(${test_name} ${TEST_SOURCES})
        
        target_link_libraries(${test_name} PRIVATE
            GameServer::TestFramework
            ${TEST_LIBRARIES}
        )
        
        # 테스트별 컴파일 정의
        target_compile_definitions(${test_name} PRIVATE
            TESTING_ENABLED
            TEST_NAME="${test_name}"
        )
        
        # CTest에 등록
        if(TEST_WORKING_DIRECTORY)
            add_test(NAME ${test_name} 
                    COMMAND ${test_name}
                    WORKING_DIRECTORY ${TEST_WORKING_DIRECTORY})
        else()
            add_test(NAME ${test_name} COMMAND ${test_name})
        endif()
        
        # 테스트 속성 설정
        set_tests_properties(${test_name} PROPERTIES
            TIMEOUT 30
            LABELS "unit"
        )
        
        # Catch2 자동 테스트 발견
        if(TARGET Catch2::Catch2WithMain)
            catch_discover_tests(${test_name})
        endif()
    endfunction()
    
    # 통합 테스트 함수
    function(add_integration_test test_name)
        cmake_parse_arguments(TEST "" "TIMEOUT" "SOURCES;LIBRARIES;DEPENDS" ${ARGN})
        
        add_game_test(${test_name} SOURCES ${TEST_SOURCES} LIBRARIES ${TEST_LIBRARIES})
        
        set_tests_properties(${test_name} PROPERTIES
            TIMEOUT ${TEST_TIMEOUT}
            LABELS "integration"
            DEPENDS "${TEST_DEPENDS}"
        )
    endfunction()
    
    # 성능 테스트 함수
    function(add_benchmark_test test_name)
        cmake_parse_arguments(TEST "" "" "SOURCES;LIBRARIES" ${ARGN})
        
        add_executable(${test_name} ${TEST_SOURCES})
        target_link_libraries(${test_name} PRIVATE 
            GameServer::Benchmark 
            ${TEST_LIBRARIES}
        )
        
        # 벤치마크는 별도 실행
        set_target_properties(${test_name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/benchmarks
        )
    endfunction()

endif()

# 테스트 공통 라이브러리
add_library(GameServer_TestFramework INTERFACE)
target_compile_features(GameServer_TestFramework INTERFACE cxx_std_20)

if(TARGET Catch2::Catch2WithMain)
    target_link_libraries(GameServer_TestFramework INTERFACE Catch2::Catch2WithMain)
endif()

target_include_directories(GameServer_TestFramework INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/include
)

# 별칭 생성
add_library(GameServer::TestFramework ALIAS GameServer_TestFramework)
```

### **3.2 지속적 통합을 위한 CTest 설정**

```cmake
# CTestConfig.cmake - CDash 설정

set(CTEST_PROJECT_NAME "GameServer")
set(CTEST_NIGHTLY_START_TIME "00:00:00 EST")

set(CTEST_DROP_METHOD "http")
set(CTEST_DROP_SITE "my.cdash.org")
set(CTEST_DROP_LOCATION "/submit.php?project=GameServer")
set(CTEST_DROP_SITE_CDASH TRUE)

# 테스트 설정 스크립트
# cmake/CTestScript.cmake
set(CTEST_SITE "${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CTEST_BUILD_NAME "${CMAKE_CXX_COMPILER_ID}-${CMAKE_BUILD_TYPE}")

set(CTEST_SOURCE_DIRECTORY "${CMAKE_SOURCE_DIR}")
set(CTEST_BINARY_DIRECTORY "${CMAKE_BINARY_DIR}")

# Git에서 소스 업데이트
set(CTEST_GIT_COMMAND "git")
set(CTEST_UPDATE_COMMAND "${CTEST_GIT_COMMAND}")

# 빌드 설정
set(CTEST_CMAKE_GENERATOR "Ninja")
set(CTEST_BUILD_FLAGS "-j8")  # 병렬 빌드

# 테스트 설정
set(CTEST_TEST_ARGS PARALLEL_LEVEL 4)

# 실행할 단계들
ctest_start("Continuous")
ctest_update()
ctest_configure()
ctest_build()
ctest_test()

# 커버리지 수집 (가능한 경우)
if(ENABLE_COVERAGE)
    ctest_coverage()
endif()

# 메모리 검사 (Valgrind)
find_program(MEMORYCHECK_COMMAND valgrind)
if(MEMORYCHECK_COMMAND)
    set(MEMORYCHECK_COMMAND_OPTIONS "--trace-children=yes --leak-check=full")
    ctest_memcheck()
endif()

ctest_submit()
```

---

## 📚 4. 패키징과 배포 자동화

### **4.1 CPack을 이용한 패키징 시스템**

```cmake
# cmake/Packaging.cmake - 완전한 패키징 솔루션

# 패키징 기본 정보
set(CPACK_PACKAGE_NAME "GameServer")
set(CPACK_PACKAGE_VENDOR "GameStudio")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "High-Performance Multiplayer Game Server")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})

# 연락처 정보
set(CPACK_PACKAGE_CONTACT "admin@gamestudio.com")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://gamestudio.com/gameserver")

# 패키지 파일명
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")

# 설치 디렉토리 구조
set(CPACK_PACKAGING_INSTALL_PREFIX "/opt/gameserver")

# 리소스 파일들
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")

# 플랫폼별 패키지 설정
if(WIN32)
    # Windows - NSIS 인스톨러
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_DISPLAY_NAME "Game Server")
    set(CPACK_NSIS_PACKAGE_NAME "GameServer")
    set(CPACK_NSIS_CONTACT "${CPACK_PACKAGE_CONTACT}")
    set(CPACK_NSIS_HELP_LINK "${CPACK_PACKAGE_HOMEPAGE_URL}")
    
    # 시작 메뉴 항목
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\GameServer.lnk' '$INSTDIR\\\\bin\\\\GameServer.exe'"
    )
    
    # Windows 서비스 설치 스크립트
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS
        "ExecWait '$INSTDIR\\\\scripts\\\\install_service.bat'"
    )
    
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS
        "ExecWait '$INSTDIR\\\\scripts\\\\uninstall_service.bat'"
    )

elseif(APPLE)
    # macOS - Bundle과 DMG
    set(CPACK_GENERATOR "Bundle;DragNDrop")
    set(CPACK_BUNDLE_NAME "GameServer")
    set(CPACK_BUNDLE_ICON "${CMAKE_SOURCE_DIR}/resources/gameserver.icns")
    set(CPACK_BUNDLE_PLIST "${CMAKE_SOURCE_DIR}/resources/Info.plist")

else()
    # Linux - DEB, RPM, TAR.GZ
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
    
    # DEB 패키지 설정
    set(CPACK_DEB_COMPONENT_INSTALL ON)
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
    set(CPACK_DEBIAN_PACKAGE_SECTION "games")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    
    # 의존성 자동 탐지
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    
    # 추가 의존성
    set(CPACK_DEBIAN_PACKAGE_DEPENDS 
        "libboost-system1.74.0, libboost-thread1.74.0, libmysqlclient21, libssl1.1")
    
    # 사전/사후 설치 스크립트
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA 
        "${CMAKE_SOURCE_DIR}/packaging/debian/postinst;${CMAKE_SOURCE_DIR}/packaging/debian/prerm")
    
    # RPM 패키지 설정
    set(CPACK_RPM_COMPONENT_INSTALL ON)
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
    set(CPACK_RPM_PACKAGE_GROUP "Amusements/Games")
    set(CPACK_RPM_PACKAGE_REQUIRES 
        "boost-system >= 1.74, boost-thread >= 1.74, mysql-libs, openssl-libs")
    
    # RPM 스크립트
    set(CPACK_RPM_POST_INSTALL_SCRIPT_FILE "${CMAKE_SOURCE_DIR}/packaging/rpm/postinst")
    set(CPACK_RPM_PRE_UNINSTALL_SCRIPT_FILE "${CMAKE_SOURCE_DIR}/packaging/rpm/prerm")
endif()

# 컴포넌트 기반 패키징
set(CPACK_COMPONENTS_ALL Runtime Development Documentation)

# Runtime 컴포넌트
set(CPACK_COMPONENT_RUNTIME_DISPLAY_NAME "Game Server Runtime")
set(CPACK_COMPONENT_RUNTIME_DESCRIPTION "Game server executable and runtime libraries")
set(CPACK_COMPONENT_RUNTIME_REQUIRED ON)

# Development 컴포넌트  
set(CPACK_COMPONENT_DEVELOPMENT_DISPLAY_NAME "Development Files")
set(CPACK_COMPONENT_DEVELOPMENT_DESCRIPTION "Headers and libraries for plugin development")
set(CPACK_COMPONENT_DEVELOPMENT_DEPENDS Runtime)

# Documentation 컴포넌트
set(CPACK_COMPONENT_DOCUMENTATION_DISPLAY_NAME "Documentation")
set(CPACK_COMPONENT_DOCUMENTATION_DESCRIPTION "API documentation and user manual")

# 설치 규칙들
install(TARGETS GameServer
    RUNTIME DESTINATION bin COMPONENT Runtime
    LIBRARY DESTINATION lib COMPONENT Runtime
    ARCHIVE DESTINATION lib COMPONENT Development
)

install(DIRECTORY include/
    DESTINATION include
    COMPONENT Development
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

install(DIRECTORY docs/
    DESTINATION share/doc/gameserver
    COMPONENT Documentation
)

install(FILES 
    ${CMAKE_SOURCE_DIR}/config/server.yaml.example
    ${CMAKE_SOURCE_DIR}/scripts/start_server.sh
    DESTINATION share/gameserver
    COMPONENT Runtime
)

# Docker 이미지 생성 (선택사항)
option(BUILD_DOCKER_IMAGE "Build Docker image" OFF)
if(BUILD_DOCKER_IMAGE)
    find_program(DOCKER_COMMAND docker)
    if(DOCKER_COMMAND)
        add_custom_target(docker_image
            COMMAND ${DOCKER_COMMAND} build -t gameserver:${PROJECT_VERSION} .
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMENT "Building Docker image"
        )
        
        add_custom_target(docker_push
            COMMAND ${DOCKER_COMMAND} push gameserver:${PROJECT_VERSION}
            DEPENDS docker_image
            COMMENT "Pushing Docker image"
        )
    endif()
endif()

include(CPack)
```

### **4.2 크로스 플랫폼 빌드와 배포**

```cmake
# cmake/CrossCompile.cmake - 크로스 컴파일 설정

# 타겟 플랫폼별 설정
if(CMAKE_CROSSCOMPILING)
    message(STATUS "Cross-compiling for ${CMAKE_SYSTEM_NAME}")
    
    # 크로스 컴파일 시 테스트 비활성화
    set(ENABLE_TESTING OFF CACHE BOOL "Disable testing when cross-compiling" FORCE)
    
    # 호스트 도구 사용
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
    
    # ARM64 Linux 타겟
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=cortex-a72")
        
        # ARM64 최적화 라이브러리 경로
        set(CMAKE_FIND_ROOT_PATH 
            /usr/aarch64-linux-gnu
            /opt/cross/aarch64-linux-gnu
        )
        
    # x86_64 Windows 타겟 (mingw)
    elseif(WIN32 AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=x86-64")
        
        # Windows 특화 설정
        set(CMAKE_FIND_ROOT_PATH 
            /usr/x86_64-w64-mingw32
            /opt/mingw64
        )
        
        # Windows 라이브러리들
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    endif()
    
endif()

# 멀티 아키텍처 빌드 스크립트 생성
function(generate_build_scripts)
    set(PLATFORMS 
        "linux-x86_64"
        "linux-aarch64" 
        "windows-x86_64"
        "darwin-x86_64"
        "darwin-arm64"
    )
    
    foreach(PLATFORM ${PLATFORMS})
        string(REPLACE "-" ";" PLATFORM_PARTS ${PLATFORM})
        list(GET PLATFORM_PARTS 0 OS)
        list(GET PLATFORM_PARTS 1 ARCH)
        
        set(SCRIPT_NAME "build_${PLATFORM}.sh")
        file(WRITE ${CMAKE_BINARY_DIR}/${SCRIPT_NAME}
            "#!/bin/bash\n"
            "set -e\n"
            "\n"
            "mkdir -p build_${PLATFORM}\n"
            "cd build_${PLATFORM}\n"
            "\n"
            "cmake .. \\\n"
            "  -DCMAKE_BUILD_TYPE=Release \\\n"
            "  -DCMAKE_SYSTEM_NAME=${OS} \\\n"
            "  -DCMAKE_SYSTEM_PROCESSOR=${ARCH} \\\n"
            "  -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/${PLATFORM}.cmake\n"
            "\n"
            "cmake --build . --parallel\n"
            "cpack\n"
        )
        
        # 실행 권한 부여
        file(CHMOD ${CMAKE_BINARY_DIR}/${SCRIPT_NAME}
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                       GROUP_READ GROUP_EXECUTE
                       WORLD_READ WORLD_EXECUTE
        )
    endforeach()
endfunction()

# CI/CD용 전체 빌드 스크립트
file(WRITE ${CMAKE_BINARY_DIR}/build_all_platforms.sh
    "#!/bin/bash\n"
    "set -e\n"
    "\n"
    "echo 'Building for all platforms...'\n"
    "\n"
    "for script in build_*.sh; do\n"
    "    if [ -f \"$script\" ]; then\n"
    "        echo \"Running $script...\"\n"
    "        ./$script\n"
    "    fi\n"
    "done\n"
    "\n"
    "echo 'All builds completed!'\n"
    "ls -la */*.tar.gz */*.deb */*.rpm 2>/dev/null || true\n"
)

file(CHMOD ${CMAKE_BINARY_DIR}/build_all_platforms.sh
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
               GROUP_READ GROUP_EXECUTE  
               WORLD_READ WORLD_EXECUTE
)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    generate_build_scripts()
endif()
```

---

## 📚 5. 성능 최적화와 디버깅 설정

### **5.1 컴파일 시간 최적화**

```cmake
# cmake/CompileOptimization.cmake - 컴파일 시간 최적화

# 프리컴파일드 헤더 설정
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.16")
    # PCH 지원하는 헤더들
    set(PCH_HEADERS
        <iostream>
        <vector>
        <string>
        <memory>
        <thread>
        <mutex>
        <future>
        <chrono>
        <algorithm>
        <boost/asio.hpp>
        <boost/system/error_code.hpp>
        <spdlog/spdlog.h>
        <nlohmann/json.hpp>
    )
    
    # PCH 타겟 생성
    add_library(GameServer_PCH INTERFACE)
    target_precompile_headers(GameServer_PCH INTERFACE ${PCH_HEADERS})
    
    # 메인 타겟에 PCH 적용
    target_precompile_headers(GameServer REUSE_FROM GameServer_PCH)
    
    message(STATUS "Precompiled headers enabled")
endif()

# Unity 빌드 설정 (선택적)
option(ENABLE_UNITY_BUILD "Enable unity build for faster compilation" OFF)
if(ENABLE_UNITY_BUILD)
    set_target_properties(GameServer PROPERTIES
        UNITY_BUILD ON
        UNITY_BUILD_BATCH_SIZE 16
    )
    message(STATUS "Unity build enabled")
endif()

# ccache 설정
find_program(CCACHE_PROGRAM ccache)
if(CCACHE_PROGRAM)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    set(CMAKE_C_COMPILER_LAUNCHER "${CCACHE_PROGRAM}")
    message(STATUS "Using ccache: ${CCACHE_PROGRAM}")
endif()

# Ninja 생성기 권장
if(NOT CMAKE_GENERATOR MATCHES "Ninja")
    message(WARNING "Consider using Ninja generator for faster builds: cmake -G Ninja")
endif()

# 병렬 링킹 (gold/lld 링커)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    option(USE_GOLD_LINKER "Use Gold linker" OFF)
    option(USE_LLD_LINKER "Use LLD linker" OFF)
    
    if(USE_LLD_LINKER)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
        message(STATUS "Using LLD linker")
    elseif(USE_GOLD_LINKER)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=gold")
        message(STATUS "Using Gold linker")
    endif()
endif()

# 링크 시간 최적화 (LTO)
include(CheckIPOSupported)
check_ipo_supported(RESULT LTO_SUPPORTED)

if(LTO_SUPPORTED AND CMAKE_BUILD_TYPE STREQUAL "Release")
    set_target_properties(GameServer PROPERTIES
        INTERPROCEDURAL_OPTIMIZATION TRUE
    )
    message(STATUS "Link Time Optimization enabled")
endif()

# 컴파일 시간 분석 도구들
option(ENABLE_BUILD_ANALYSIS "Enable build time analysis" OFF)
if(ENABLE_BUILD_ANALYSIS)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftime-trace")
        message(STATUS "Clang build time tracing enabled")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ftime-report")
        message(STATUS "GCC build time reporting enabled")
    endif()
endif()
```

### **5.2 런타임 성능 프로파일링 통합**

```cmake
# cmake/Profiling.cmake - 성능 프로파일링 도구 통합

option(ENABLE_PROFILING "Enable performance profiling" OFF)
option(ENABLE_MEMORY_PROFILING "Enable memory profiling" OFF)
option(ENABLE_CPU_PROFILING "Enable CPU profiling" OFF)

if(ENABLE_PROFILING OR ENABLE_MEMORY_PROFILING OR ENABLE_CPU_PROFILING)
    
    # Google Perftools (gperftools)
    find_package(PkgConfig)
    if(PkgConfig_FOUND)
        pkg_check_modules(PROFILER libprofiler)
        pkg_check_modules(TCMALLOC libtcmalloc)
    endif()
    
    # Intel VTune 프로파일러 지원
    find_path(VTUNE_INCLUDE_DIR
        NAMES ittnotify.h
        PATHS
            $ENV{VTUNE_PROFILER_DIR}/include
            /opt/intel/vtune_profiler/include
    )
    
    find_library(VTUNE_LIBRARY
        NAMES ittnotify
        PATHS
            $ENV{VTUNE_PROFILER_DIR}/lib64
            /opt/intel/vtune_profiler/lib64
    )
    
    # 프로파일링 설정 적용
    if(ENABLE_CPU_PROFILING AND PROFILER_FOUND)
        target_link_libraries(GameServer PRIVATE ${PROFILER_LIBRARIES})
        target_compile_definitions(GameServer PRIVATE CPU_PROFILING_ENABLED)
        message(STATUS "CPU profiling enabled with gperftools")
    endif()
    
    if(ENABLE_MEMORY_PROFILING AND TCMALLOC_FOUND)
        target_link_libraries(GameServer PRIVATE ${TCMALLOC_LIBRARIES})
        target_compile_definitions(GameServer PRIVATE MEMORY_PROFILING_ENABLED)
        message(STATUS "Memory profiling enabled with tcmalloc")
    endif()
    
    if(VTUNE_INCLUDE_DIR AND VTUNE_LIBRARY)
        target_include_directories(GameServer PRIVATE ${VTUNE_INCLUDE_DIR})
        target_link_libraries(GameServer PRIVATE ${VTUNE_LIBRARY})
        target_compile_definitions(GameServer PRIVATE VTUNE_PROFILING_ENABLED)
        message(STATUS "Intel VTune profiling enabled")
    endif()
    
    # 프로파일링 실행 스크립트 생성
    configure_file(
        ${CMAKE_SOURCE_DIR}/scripts/profile_template.sh.in
        ${CMAKE_BINARY_DIR}/profile_gameserver.sh
        @ONLY
    )
    
    # 성능 테스트 타겟
    add_custom_target(profile
        COMMAND ${CMAKE_BINARY_DIR}/profile_gameserver.sh
        DEPENDS GameServer
        COMMENT "Running performance profile"
    )
    
endif()

# 벤치마크 프레임워크 (Google Benchmark)
option(ENABLE_BENCHMARKS "Build benchmark tests" OFF)
if(ENABLE_BENCHMARKS)
    FetchContent_Declare(
        benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.7.1
        GIT_SHALLOW TRUE
    )
    
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Disable benchmark testing" FORCE)
    FetchContent_MakeAvailable(benchmark)
    
    # 벤치마크 라이브러리 생성
    add_library(GameServer_Benchmark INTERFACE)
    target_link_libraries(GameServer_Benchmark INTERFACE benchmark::benchmark)
    
    add_library(GameServer::Benchmark ALIAS GameServer_Benchmark)
endif()
```

---

## 📝 정리 및 실전 적용

### **완성된 프로젝트 구조**

```
GameServer/
├── CMakeLists.txt              # 메인 CMake 설정
├── cmake/
│   ├── CompilerSettings.cmake  # 컴파일러 설정
│   ├── Dependencies.cmake      # 의존성 관리
│   ├── Testing.cmake          # 테스트 설정
│   ├── Packaging.cmake        # 패키징 설정
│   ├── CrossCompile.cmake     # 크로스 컴파일
│   ├── CompileOptimization.cmake # 컴파일 최적화
│   ├── Profiling.cmake        # 프로파일링
│   └── modules/
│       ├── FindRedis.cmake    # 커스텀 Find 모듈
│       └── FindMySQL.cmake
├── src/
│   ├── core/CMakeLists.txt    # 모듈별 CMake
│   ├── network/CMakeLists.txt
│   ├── game/CMakeLists.txt
│   └── database/CMakeLists.txt
├── tests/
│   └── CMakeLists.txt         # 테스트 설정
├── packaging/                 # 패키징 스크립트들
├── scripts/                   # 빌드/배포 스크립트
└── docs/                      # 문서들
```

### **다음 학습 권장사항**

1. **[테스팅 프레임워크 완전 가이드](./22_testing_frameworks_complete.md)** 🔥
2. **[디버깅과 프로파일링 도구](./11_debugging_profiling_toolchain.md)** 🔥  
3. **[현대 C++ 컨테이너와 유틸리티](./37_modern_cpp_containers_utilities.md)** 🔥

---

**💡 핵심 포인트**: 현대적인 빌드 시스템은 단순히 컴파일하는 도구가 아니라, 개발부터 배포까지 전체 라이프사이클을 관리하는 DevOps 인프라의 핵심입니다. 게임 서버처럼 복잡한 프로젝트에서는 이러한 자동화된 빌드 시스템이 개발 생산성과 제품 품질을 크게 좌우합니다.