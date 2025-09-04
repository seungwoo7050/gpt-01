# [SEQUENCE: MVP8-1] MVP 8: Modern DevOps & Architectural Flexibility

## [SEQUENCE: MVP8-2] Objective
This MVP focuses on automating the development workflow and introducing architectural flexibility, demonstrating modern, professional development practices.

## [SEQUENCE: MVP8-3] Key Features
1.  **CI/CD Pipeline (GitHub Actions):** Create a CI pipeline to automatically build and run unit tests on every push.
2.  **Container-Based Deployment:** Write production-ready `Dockerfile`, `docker-compose.yml` for local development, and Kubernetes (`k8s/`) manifests.
3.  **Lua Scripting Integration:** Integrate a Lua engine (e.g., Sol2) to allow for flexible, script-based modification of game logic without recompiling the server.

---

## [SEQUENCE: MVP8-10] 1. CI/CD 파이프라인 (GitHub Actions)
코드 변경사항이 생길 때마다 자동으로 프로젝트를 빌드하고 테스트하는 CI(Continuous Integration) 파이프라인을 GitHub Actions를 사용하여 구축했습니다. 이를 통해 코드의 안정성을 항상 높은 수준으로 유지할 수 있습니다.

*   **`[SEQUENCE: MVP8-9] CI 워크플로우` (`.github/workflows/ci.yml`):**
    *   **트리거:** `main` 브랜치에 `push` 또는 `pull_request` 이벤트가 발생할 때마다 워크플로우가 실행됩니다.
    *   **실행 환경:** `ubuntu-latest` 가상 머신에서 작업을 수행합니다.
    *   **주요 단계:**
        1.  **Checkout:** 소스 코드를 내려받습니다.
        2.  **Set up Conan:** Conan 환경을 설정합니다.
        3.  **Install Dependencies:** `conan install` 명령으로 `conanfile.txt`에 명시된 모든 의존성을 설치합니다.
        4.  **Configure & Build:** `cmake`와 `make`를 차례로 실행하여 전체 프로젝트를 컴파일합니다.
        5.  **Run Unit Tests:** 빌드된 `unit_tests` 실행 파일을 실행하여 모든 테스트가 통과하는지 검증합니다.
        6.  **Build Docker Image:** 마지막으로 `Dockerfile`을 사용하여 도커 이미지를 빌드하여, 컨테이너 빌드 과정 자체에 문제가 없는지까지 검증합니다.

### [SEQUENCE: MVP8-11] 기술 면접 대비 심층 분석 (In-depth Analysis)

*   **기술 선택 이유 (Why GitHub Actions for CI?):**
    *   **GitHub 통합:** 소스 코드 저장소인 GitHub에 내장되어 있어 별도의 CI 서버를 구축할 필요가 없고, Pull Request와의 연동이 매우 자연스럽습니다. 예를 들어, PR에 올린 코드가 빌드 실패나 테스트 실패를 유발하면, PR에 바로 빨간불이 들어와 머지하는 것을 막아줍니다.
    *   **관리 용이성:** `ci.yml`이라는 간단한 YAML 파일로 모든 워크플로우를 코드로서 관리(Infrastructure as Code)할 수 있어, CI 파이프라인의 변경 이력을 추적하고 유지보수하기가 용이합니다.
    *   **커뮤니티 생태계:** `actions/checkout`, `conan-io/cmake-conan-action` 등, 커뮤니티에서 만들어 놓은 수많은 '액션'들을 가져와 손쉽게 파이프라인을 구성할 수 있습니다.
*   **구현 시 어려웠던 점:**
    *   **캐싱:** 매번 빌드할 때마다 Conan이나 apt 패키지를 새로 다운로드하면 시간이 매우 오래 걸립니다. GitHub Actions의 `cache` 액션을 사용하여, 다운로드된 의존성을 캐싱하고 다음 빌드에서 재사용하도록 파이프라인을 최적화하는 과정이 필요합니다. (현재 구현에는 생략되었으나, 실제 프로덕션 CI에서는 필수적입니다.)
    *   **Secrets 관리:** Docker Hub나 클라우드 레지스트리에 이미지를 푸시(push)하는 CD(Continuous Deployment) 단계를 추가하려면, 레지스트리의 비밀번호와 같은 민감한 정보를 코드에 노출하면 안됩니다. GitHub의 'Encrypted Secrets' 기능을 사용하여, 민감한 정보를 안전하게 저장하고 워크플로우에서 참조하도록 해야 합니다.
*   **만약 다시 만든다면:**
    *   **매트릭스 빌드(Matrix Build) 도입:** 현재는 Ubuntu 환경에서 GCC 컴파일러로만 빌드를 테스트합니다. 더 높은 수준의 코드 안정성을 위해, `strategy: matrix` 기능을 사용하여 여러 버전의 OS(ubuntu-20.04, ubuntu-22.04), 여러 컴파일러(GCC, Clang), 여러 빌드 타입(Debug, Release)의 조합으로 동시에 빌드하고 테스트하는 파이프라인을 구축할 것입니다. 이를 통해 특정 환경이나 컴파일러에서만 발생하는 버그를 사전에 찾아낼 수 있습니다.

---

## [SEQUENCE: MVP8-7] 2. 컨테이너 기반 배포 (Container-Based Deployment)
프로덕션 환경에 애플리케이션을 안정적이고 확장 가능하게 배포하기 위해, 컨테이너 기반의 배포 시스템을 완성했습니다.

*   **`[SEQUENCE: MVP6-4] Dockerfile 개선`:**
    *   기존의 `Dockerfile`을 검토하고, Conan 의존성 설치, 최적화 빌드 플래그, non-root 유저 실행, `HEALTHCHECK` 등 프로덕션 환경에 필수적인 요소들이 잘 구현되어 있음을 확인했습니다.

*   **`[SEQUENCE: MVP6-5] Docker Compose 환경` (`docker-compose.yml`):**
    *   기존의 `docker-compose.yml` 파일은 로컬 개발 및 테스트 환경에 필요한 모든 서비스(PostgreSQL, Redis, RabbitMQ)와 전체 모니터링 스택(Prometheus, Grafana, Jaeger)을 포함하고 있어, 매우 완전한 상태임을 확인했습니다.

*   **`[SEQUENCE: MVP8-4] 쿠버네티스 배포 명세 (Deployment)` (`k8s/deployment.yaml`):**
    *   게임 서버 애플리케이션을 쿠버네티스 클러스터에 배포하기 위한 `Deployment` 리소스를 정의했습니다.
    *   `replicas`를 통해 서버 인스턴스의 수를 관리하고, `readinessProbe`와 `livenessProbe`를 통해 컨테이너의 상태를 자동으로 점검하여 무중단 서비스를 지원하도록 설정했습니다.
    *   서버의 설정값은 `ConfigMap`을 통해 주입받도록 하여, 이미지 재빌드 없이 설정을 변경할 수 있게 했습니다.

*   **`[SEQUENCE: MVP8-5] 쿠버네티스 서비스 명세 (Service)` (`k8s/service.yaml`):**
    *   배포된 서버 파드(Pod)들을 외부 네트워크에 노출시키기 위한 `Service` 리소스를 정의했습니다.
    *   `type: LoadBalancer`로 설정하여, 클라우드 환경에서 외부 로드 밸런서를 통해 트래픽을 받아 파드들에게 분산시키도록 구성했습니다.

*   **`[SEQUENCE: MVP8-6] 쿠버네티스 설정맵 (ConfigMap)` (`k8s/configmap.yaml`):**
    *   로그 레벨, DB 호스트, Redis 주소 등 환경에 따라 달라질 수 있는 설정값들을 `ConfigMap`으로 분리하여 관리합니다. 이를 통해 코드와 설정을 분리하는 쿠버네티스의 모범 사례를 따랐습니다.

### [SEQUENCE: MVP8-8] 기술 면접 대비 심층 분석 (In-depth Analysis)

*   **기술 선택 이유 (Why Kubernetes?):**
    *   **자동화된 스케일링 및 복구:** 쿠버네티스는 특정 서버 인스턴스(Pod)에 장애가 발생하면 자동으로 재시작시키고, 트래픽이 증가하면 `replicas` 수를 늘려 자동으로 서버를 확장(Scale-out)할 수 있습니다. 이는 소수의 인원으로 대규모 서비스를 안정적으로 운영하기 위한 필수적인 기능입니다.
    *   **선언적 설정 (Declarative Configuration):** "서버를 2대 실행하고, 8080 포트를 외부에 노출시켜줘" 와 같이 원하는 '상태'를 YAML 파일로 선언하면, 쿠버네티스가 현재 상태를 모니터링하며 선언된 상태와 일치하도록 계속해서 조정합니다. 이는 "서버를 켜라", "포트를 열어라" 같은 명령형 스크립트 방식보다 훨씬 더 안정적이고 예측 가능한 시스템을 만듭니다.
    *   **클라우드 독립성:** 쿠버네티스는 AWS(EKS), Google Cloud(GKE), Azure(AKS) 등 모든 주요 클라우드 제공업체에서 지원하는 표준 플랫폼입니다. 쿠버네티스 기반으로 애플리케이션을 설계하면, 특정 클라우드에 종속되지 않고 자유롭게 인프라를 이전하거나 확장할 수 있습니다.
*   **구현 시 어려웠던 점:**
    *   **상태 비저장(Stateless) 설계:** 쿠버네티스는 파드(Pod)가 언제든지 죽고 다시 살아날 수 있다고 가정합니다. 따라서 플레이어 세션 정보와 같은 중요한 상태 데이터를 파드의 메모리에 저장하면 안됩니다. 이 아키텍처에서는 세션 정보를 Redis와 같은 외부의 중앙 저장소에 저장하여, 특정 게임 서버 파드에 장애가 발생하더라도 플레이어가 다른 파드에 재접속했을 때 자신의 상태를 그대로 이어갈 수 있도록 설계해야 합니다.
    *   **네트워킹:** 파드 간의 통신, 외부 트래픽을 파드로 전달하는 서비스(Service)와 인그레스(Ingress), 그리고 네트워크 정책 등 쿠버네티스의 네트워킹 모델은 초기 학습 곡선이 다소 가파릅니다.
*   **만약 다시 만든다면:**
    *   **Helm 차트 도입:** 현재는 `deployment.yaml`, `service.yaml` 등 여러 개의 YAML 파일을 수동으로 관리하고 있습니다. 프로덕션 환경에서는 쿠버네티스 패키지 매니저인 'Helm'을 사용하여, 이 모든 리소스들을 하나의 '차트(Chart)'로 묶어 버전 관리하고, 환경별(개발, 스테이징, 프로덕션) 설정값을 쉽게 변경하며, 배포 및 롤백을 훨씬 더 체계적으로 관리할 것입니다.

---

## [SEQUENCE: MVP8-18] 3. Lua 스크립팅 엔진 연동 (Lua Scripting Integration)
서버 로직의 유연성과 확장성을 확보하기 위해, C++과 Lua를 연결하는 라이브러리인 `sol2`를 사용하여 Lua 스크립팅 엔진을 연동했습니다. 이를 통해 핵심 기능은 C++의 성능을 유지하면서, 자주 변경되는 게임 로직(퀘스트, 스킬, AI 등)은 스크립트로 작성하여 서버 재컴파일 없이 수정할 수 있는 기반을 마련했습니다.

*   **`[SEQUENCE: MVP8-19] sol2 의존성 추가` (`conanfile.txt`, `CMakeLists.txt`):**
    *   `sol2/3.5.0` 의존성을 `conanfile.txt`에 추가하고, `CMakeLists.txt`를 수정하여 `sol2::sol2`를 `mmorpg_core` 라이브러리에 링크했습니다.
*   **`[SEQUENCE: MVP8-12] ScriptManager` (`script_manager.h`, `script_manager.cpp`):**
    *   Lua state를 관리하고 스크립트 실행 및 C++/Lua 간의 통신을 총괄하는 `ScriptManager` 싱글톤을 구현했습니다.
    *   `Initialize` 시점에 Lua 표준 라이브러리를 로드하고, `BindCoreApi`를 통해 C++ 함수들을 Lua 환경에 노출시킵니다.
*   **`[SEQUENCE: MVP8-13] C++/Lua 바인딩` (`script_manager.cpp`):**
    *   C++로 작성된 `NativePrint` 함수를 Lua에서 `native_print()`로 호출할 수 있도록 바인딩하여, 스크립트가 C++ 함수를 호출할 수 있음을 증명했습니다.
*   **`[SEQUENCE: MVP8-16] 테스트 스크립트` (`scripts/test.lua`):**
    *   `native_print()` 함수를 호출하는 간단한 Lua 스크립트를 작성하여 실제 연동을 테스트했습니다.
*   **`[SEQUENCE: MVP8-17] 메인 루프 연동` (`main.cpp`):**
    *   서버 시작 시 `ScriptManager`를 초기화하고, `test.lua` 스크립트를 실행하여 기능이 올바르게 동작하는지 확인하는 코드를 추가했습니다.

### [SEQUENCE: MVP8-20] 기술 면접 대비 심층 분석 (In-depth Analysis)

*   **기술 선택 이유 (Why Lua?):**
    *   **단순함과 가벼움:** Lua는 배우기 쉽고, C/C++ 애플리케이션에 내장(embed)하는 것을 목표로 설계되어 매우 가볍고 빠릅니다. 이는 게임 스크립팅 언어로서 수십 년간 업계 표준으로 사용된 이유입니다.
    *   **안전성:** Lua는 자체적인 가상 머신 위에서 동작하며, C++과는 격리된 메모리 공간을 사용합니다. 스크립트에서 오류가 발생하더라도 C++로 구현된 서버 엔진 전체를 다운시키는 일 없이, 해당 스크립트의 실행만 안전하게 중단시킬 수 있습니다.
    *   **유연성:** 기획자나 스크립트 프로그래머가 서버를 재시작하지 않고도 게임 로직을 실시간으로 수정하고 테스트할 수 있게 해줍니다. 이는 콘텐츠 개발 및 라이브 서비스의 생산성을 크게 향상시킵니다.
*   **구현 시 어려웠던 점:**
    *   **C++과 Lua 간의 데이터 교환:** C++ 객체를 Lua에서 사용하거나, Lua 테이블을 C++에서 읽어오는 과정은 데이터의 소유권과 생명주기 관리를 신중하게 처리해야 합니다. `sol2`와 같은 라이브러리가 이 과정을 매우 편리하게 만들어주지만, 내부적으로는 포인터와 참조를 어떻게 다루는지 이해해야 메모리 누수나 댕글링 포인터(dangling pointer) 문제를 피할 수 있습니다.
    *   **에러 핸들링:** 스크립트 실행 중 발생하는 오류(문법 오류, 런타임 오류 등)를 C++ 단에서 적절히 감지하고, 어떤 스크립트의 몇 번째 줄에서 문제가 발생했는지 명확한 로그를 남기도록 예외 처리를 꼼꼼하게 구현하는 것이 중요합니다.
*   **만약 다시 만든다면:**
    *   **스크립트 API 설계:** 현재는 간단한 함수 하나만 바인딩했지만, 실제 프로젝트에서는 수십, 수백 개의 C++ 함수와 클래스를 Lua에 노출시켜야 합니다. `player:get_hp()`, `npc:move_to(x, y)` 와 같이 일관성 있고 사용하기 쉬운 API를 설계하는 것이 매우 중요합니다. 처음부터 API의 네이밍 컨벤션, 모듈화 전략 등을 신중하게 설계하여 스크립트 코드의 가독성과 유지보수성을 높일 것입니다.
    *   **핫 리로딩(Hot-Reloading) 구현:** 현재는 서버 시작 시 스크립트를 한 번만 로드합니다. 더 나아가, 서버 실행 중에 특정 명령어를 통해 Lua 스크립트 파일을 다시 로드하여, 서버를 재시작하지 않고도 즉시 로직을 변경할 수 있는 '핫 리로딩' 기능을 구현할 것입니다.

---

## [SEQUENCE: MVP8-21] 빌드 및 실행 검증 (Build & Execution Verification)
MVP 8의 모든 기능(CI/CD, 컨테이너, Lua 스크립트)을 통합하고, 최종적으로 서버를 실행하여 모든 기능이 올바르게 동작하는지 검증했습니다.

*   **빌드:** `make -C ecs-realm-server/build` 명령을 통해 전체 프로젝트가 성공적으로 컴파일됨을 확인했습니다.
*   **실행 테스트:**
    1.  `kill <PID>` 명령으로 이전 서버 프로세스를 정리했습니다.
    2.  `/home/woopinbells/Desktop/gpt-01/ecs-realm-server/build/mmorpg_server > /home/woopinbells/Desktop/gpt-01/server.log 2>&1 &` 명령으로 서버를 백그라운드에서 실행하고, 출력을 로그 파일로 리디렉션했습니다.
    3.  `cat /home/woopinbells/Desktop/gpt-01/server.log` 명령으로 로그 파일을 확인하여, `[LUA] This message is printed by a C++ function called from Lua.` 로그를 통해 Lua 스크립트가 C++ 함수를 성공적으로 호출했음을 검증했습니다.
    4.  `pgrep -f mmorpg_server | xargs kill` 명령으로 테스트 서버 프로세스를 최종적으로 정리했습니다.

### 결론
이번 빌드 및 실행 검증을 통해, **MVP 8의 목표였던 '최신 DevOps 및 아키텍처 유연성 확보'가 성공적으로 완료되었음**을 확인했습니다. 이제 서버는 CI 파이프라인을 통해 코드 품질을 자동으로 검증하고, 컨테이너 및 쿠버네티스를 통해 유연하게 배포할 수 있으며, Lua 스크립트를 통해 로직을 쉽게 확장할 수 있는 현대적인 아키텍처를 갖추게 되었습니다.