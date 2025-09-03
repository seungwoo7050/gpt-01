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

### [SEQUENCE: MVP0-9] 기술 면접 대비 심층 분석 (In-depth Analysis for Technical Interviews)

#### 1. 빌드 시스템: CMake
*   **기술 선택 이유 (Why CMake?):** C++ 생태계의 사실상 표준 빌드 시스템입니다. 플랫폼(Windows, Linux, macOS)에 종속되지 않는 빌드 스크립트를 생성할 수 있어 프로젝트의 이식성을 극대화합니다. Make, Ninja, Visual Studio 등 다양한 네이티브 빌드 도구를 지원하는 유연성 또한 큰 장점입니다.
*   **다른 대안과 비교 (Trade-offs):
    *   **Makefile:** 저수준 제어가 가능하지만, 플랫폼 종속적이며 대규모 프로젝트에서는 관리가 복잡해집니다.
    *   **Bazel (by Google):** 대규모 모노레포에서 강력한 성능과 재현성을 보장하지만, 학습 곡선이 가파르고 설정이 복잡하여 이 프로젝트 규모에는 과도한 측면이 있습니다.
*   **구현 시 어려웠던 점:** `target_link_libraries`의 `PUBLIC`, `PRIVATE`, `INTERFACE` 키워드의 의미를 정확히 이해하고 적용하는 것이 중요합니다. 잘못 사용하면 의존성이 불필요하게 전파되어 빌드 시간이 늘어나거나, 반대로 필요한 라이브러리가 링크되지 않는 문제가 발생할 수 있습니다.

#### 2. 의존성 관리: Conan
*   **기술 선택 이유 (Why Conan?):** C++에는 공식적인 패키지 매니저가 없어 의존성 관리가 어렵습니다. Conan은 미리 컴파일된 바이너리 패키지를 중앙 저장소(ConanCenter) 또는 자체 호스팅(Artifactory)을 통해 관리할 수 있게 해주어, 팀원 모두가 동일한 버전의 라이브러리로 일관된 개발 환경을 빠르게 구축할 수 있도록 합니다.
*   **다른 대안과 비교 (Trade-offs):
    *   **vcpkg (by Microsoft):** 소스 기반 패키지 매니저로, MSVC와의 통합이 매우 뛰어납니다. 하지만 모든 라이브러리를 소스에서부터 빌드해야 하므로 초기 설정 시간이 오래 걸릴 수 있습니다.
    *   **Git Submodules:** 각 의존성을 Git 저장소로 관리하는 방식입니다. 간단하지만, 의존성의 의존성(transitive dependencies)을 수동으로 관리해야 하고, 버전 관리가 복잡해지기 쉽습니다.
*   **구현 시 어려웠던 점:** `conan install` 후 생성되는 `conan_toolchain.cmake` 파일을 CMake에 정확히 연동하는 초기 설정 과정이 다소 생소할 수 있습니다. 또한, 각 라이브러리의 옵션(e.g., `shared=False`)을 프로젝트 요구사항에 맞게 조정하는 과정이 필요합니다.

#### 3. 컨테이너화: Docker
*   **기술 선택 이유 (Why Docker?):** "제 컴퓨터에서는 되는데요" 문제를 근본적으로 해결합니다. 개발, 테스트, 프로덕션 환경을 모두 동일한 컨테이너 이미지 기반으로 구성하여 환경 불일치에서 오는 문제를 원천 차단합니다. 가상 머신(VM)보다 훨씬 가볍고 빠르게 실행되는 장점이 있습니다.
*   **다른 대안과 비교 (Trade-offs):
    *   **Vagrant:** VM을 사용하여 완전한 격리 환경을 제공하지만, Docker보다 무겁고 리소스 소모가 큽니다.
    *   **Bare Metal:** 최고 성능을 낼 수 있지만, 환경 구성을 수동으로 해야 하므로 재현성이 떨어지고 확장성이 부족합니다.
*   **구현 시 어려웠던 점:**
    *   **이미지 크기 최적화:** 최종 런타임 이미지에 빌드 도구(컴파일러 등)가 포함되지 않도록 멀티-스테이지 빌드(`multi-stage build`)를 구성하는 것이 중요합니다. 이를 통해 이미지 크기를 수 GB에서 수십 MB로 줄일 수 있습니다.
    *   **보안:** 런타임 컨테이너를 `root`가 아닌 별도의 권한 없는 사용자(`non-root user`)로 실행하여, 컨테이너 탈취 시 호스트 시스템에 가해질 수 있는 잠재적 위협을 최소화하는 것이 중요합니다. 이 부분을 놓치는 경우가 많습니다.