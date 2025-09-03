# [SEQUENCE: MVP0-1] MVP 0: Infrastructure and Foundation

## [SEQUENCE: MVP0-2] Introduction
모든 대규모 프로젝트의 시작은 견고한 기반을 구축하는 것입니다. MVP 0는 게임 기능이 아닌, 전문적인 C++ 프로젝트에 필요한 핵심 인프라, 빌드 시스템, 그리고 개발 환경을 설정하는 데 중점을 둡니다. 이 단계에서는 재현 가능하고, 유지보수 가능하며, 확장 가능한 개발 환경을 만드는 것을 목표로 합니다.

## [SEQUENCE: MVP0-3] Build System with CMake
프로젝트의 빌드 시스템으로는 CMake를 사용합니다. 루트 `CMakeLists.txt` 파일은 프로젝트의 기본 설정을 정의하고, 의존성을 관리하며, 여러 서버 컴포넌트의 빌드 프로세스를 총괄합니다.

*   **C++ Standard:** C++20 표준을 필수로 설정하여 최신 언어 기능을 활용합니다.
*   **Library Definitions:** `mmorpg_core`, `mmorpg_game`, `mmorpg_database`와 같은 여러 정적 라이브러리와 최종 서버 실행 파일(`mmorpg_server`)을 정의하여 코드를 모듈식으로 구성합니다.
*   **Dependency Linking:** 각 라이브러리와 실행 파일이 필요로 하는 의존성을 명확히 연결합니다.

```cmake
# [SEQUENCE: MVP0-4] CMake build system setup
cmake_minimum_required(VERSION 3.20)
project(mmorpg-server-engine VERSION 1.0.0 LANGUAGES CXX)

# C++ standard and compiler settings
set(CMAKE_CXX_STANDARD 20)
# ... (rest of the initial setup)

# Find required packages
find_package(Threads REQUIRED)
find_package(Boost 1.82 REQUIRED COMPONENTS system filesystem thread)
# ... (other packages)

# Core library
add_library(mmorpg_core STATIC
    # ...
)

# Game systems library
add_library(mmorpg_game STATIC
    # ...
)

# Main server executable
add_executable(mmorpg_server
    # ...
)

target_link_libraries(mmorpg_server PRIVATE
    mmorpg_core
    mmorpg_game
    # ...
)
```

## [SEQUENCE: MVP0-5] Dependency Management with Conan
C++ 라이브러리 의존성은 Conan을 통해 관리됩니다. `conanfile.txt`는 Boost, Protobuf, spdlog 등 프로젝트에 필요한 모든 외부 라이브러리를 선언하여, 어떤 개발자든 동일한 빌드 환경을 쉽게 구성할 수 있도록 보장합니다.

```ini
# [SEQUENCE: MVP0-6] Conan dependency management
[requires]
boost/1.82.0
protobuf/3.21.12
spdlog/1.12.0
gtest/1.14.0
nlohmann_json/3.11.3
benchmark/1.8.3

[generators]
CMakeDeps
CMakeToolchain

[options]
boost/*:shared=False
# ... (other options)
```

## [SEQUENCE: MVP0-7] Containerization with Docker
개발부터 프로덕션까지 일관된 실행 환경을 보장하기 위해 Docker를 사용하여 애플리케이션을 컨테이너화합니다. `Dockerfile`은 최종 이미지 크기를 최소화하고 보안을 강화하기 위해 멀티-스테이지 빌드를 사용합니다.

*   **Builder Stage:** 빌드에 필요한 모든 의존성을 설치하고 프로젝트를 컴파일합니다.
*   **Runtime Stage:** 빌드된 바이너리와 최소한의 런타임 의존성만 포함하는 경량 이미지입니다. 또한, 보안을 위해 non-root 사용자로 서버를 실행합니다.

```dockerfile
# [SEQUENCE: MVP0-8] Docker containerization setup
# Build stage
FROM ubuntu:22.04 AS builder
# ... (build dependencies installation)

WORKDIR /app
COPY . .
# ... (build process)

# Runtime stage
FROM ubuntu:22.04 AS runtime
# ... (runtime dependencies installation)

# Create and switch to non-root user
RUN groupadd -g 1000 gameserver && \
    useradd -u 1000 -g gameserver -m -s /bin/bash gameserver
USER gameserver

# Copy binary and set entrypoint
COPY --from=builder /app/build/src/server/game_server /app/bin/
ENTRYPOINT ["/app/bin/game_server"]
```
