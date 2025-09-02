# [SEQUENCE: MVP1-1] MVP 1: Core Networking & Server Loop

## [SEQUENCE: MVP1-2] Introduction
MVP 1은 MMORPG 서버의 가장 기본적인 뼈대인 네트워크 통신 계층과 서버의 메인 루프를 구축하는 데 중점을 둡니다. 이 단계에서는 Boost.Asio를 사용하여 여러 클라이언트의 동시 접속을 비동기적으로 처리할 수 있는 TCP 서버를 구현합니다. 또한, 서버의 안정적인 운영을 위한 로깅, 시그널 처리, 기본적인 모니터링 기능의 기반을 마련합니다. 이 단계의 목표는 수천 명의 동시 접속을 안정적으로 처리할 수 있는 확장 가능한 네트워크 엔진을 구축하는 것입니다.

## [SEQUENCE: MVP1-3] Core Network Engine (`src/core/network/`)
Boost.Asio를 기반으로 비동기 I/O를 처리하는 서버의 핵심 네트워킹 엔진입니다. 이 모듈은 저수준 소켓 통신을 추상화하여, 상위 레벨에서는 비즈니스 로직에만 집중할 수 있도록 설계되었습니다.

### [SEQUENCE: MVP1-4] TCP Server (`tcp_server.h` & `.cpp`)
비동기 I/O를 처리하는 메인 서버 클래스입니다. 다중 스레드 환경에서 효율적으로 동작하도록 I/O Context Pool을 사용하여 클라이언트 요청을 여러 워커 스레드에 분산시킵니다.
*   `[SEQUENCE: MVP1-5]` Server configuration
*   `[SEQUENCE: MVP1-6]` Asynchronous TCP server using Boost.Asio
*   `[SEQUENCE: MVP1-7]` Start the server
*   `[SEQUENCE: MVP1-8]` Stop the server
*   `[SEQUENCE: MVP1-9]` Check if server is running
*   `[SEQUENCE: MVP1-10]` Get connection count
*   `[SEQUENCE: MVP1-11]` Set packet handler
*   `[SEQUENCE: MVP1-12]` Get packet handler
*   `[SEQUENCE: MVP1-13]` Get next IO context from pool
*   `[SEQUENCE: MVP1-14]` Asynchronously accept new connections
*   `[SEQUENCE: MVP1-15]` Handle a new connection

### [SEQUENCE: MVP1-16] Session (`session.h` & `.cpp`)
개별 클라이언트 연결을 나타내는 클래스입니다. 각 세션은 자신만의 소켓과 데이터 버퍼를 가지며, 비동기적으로 데이터를 읽고 씁니다. `shared_from_this`를 사용하여 비동기 작업 중에도 객체의 생명주기를 안전하게 관리합니다.
*   `[SEQUENCE: MVP1-17]` Session state enumeration
*   `[SEQUENCE: MVP1-18]` Represents a single client connection
*   `[SEQUENCE: MVP1-19]` Start session and begin reading
*   `[SEQUENCE: MVP1-20]` Disconnect session
*   `[SEQUENCE: MVP1-21]` Send packet to client
*   `[SEQUENCE: MVP1-22]` Getters
*   `[SEQUENCE: MVP1-23]` Setters
*   `[SEQUENCE: MVP1-24]` Authentication
*   `[SEQUENCE: MVP1-25]` Read packet header
*   `[SEQUENCE: MVP1-26]` Read packet body
*   `[SEQUENCE: MVP1-27]` Process received packet
*   `[SEQUENCE: MVP1-28]` Write packet to socket
*   `[SEQUENCE: MVP1-29]` Handle network errors
*   `[SEQUENCE: MVP1-30]` Get remote address

### [SEQUENCE: MVP1-31] Session Manager (`session_manager.h` & `.cpp`)
서버에 접속된 모든 활성 세션을 추적하고 관리하는 중앙 관리자 클래스입니다. 전체 브로드캐스팅이나 특정 플레이어에게 메시지를 보내는 등의 작업을 수행하며, 스레드 안전성을 위해 `shared_mutex`를 사용합니다.
*   `[SEQUENCE: MVP1-32]` Manages all active client sessions
*   `[SEQUENCE: MVP1-33]` Register a new session
*   `[SEQUENCE: MVP1-34]` Unregister a session
*   `[SEQUENCE: MVP1-35]` Get a session by its ID
*   `[SEQUENCE: MVP1-36]` Broadcast a packet to all sessions
*   `[SEQUENCE: MVP1-37]` Send a packet to a specific player
*   `[SEQUENCE: MVP1-38]` Get the number of active sessions

## [SEQUENCE: MVP1-39] Packet Handling (`src/core/network/` & `proto/`)
클라이언트와 서버 간의 통신 프로토콜과 그 처리 방식을 정의합니다. Protocol Buffers를 사용하여 메시지 형식을 명확하게 정의하고, 직렬화/역직렬화를 통해 효율적으로 데이터를 전송합니다.

### [SEQUENCE: MVP1-40] Protocol Buffers (`.proto`)
*   `[SEQUENCE: MVP1-41]` `packet.proto`: 모든 메시지를 감싸는 최상위 Packet 래퍼를 정의합니다. 여기에는 메시지 타입, 크기, 순서 번호 등의 메타데이터가 포함됩니다.
*   `[SEQUENCE: MVP1-42]` `auth.proto`: 로그인, 로그아웃, 서버 목록 등 인증 관련 메시지를 정의합니다.
*   `[SEQUENCE: MVP1-43]` `common.proto`: Vector3, 에러 코드 등 여러 메시지에서 공통으로 사용되는 데이터 구조를 정의합니다.
*   `[SEQUENCE: MVP1-44]` `game.proto`: 게임 월드 진입, 캐릭터 이동 등 핵심 게임플레이 관련 메시지를 정의합니다.

### [SEQUENCE: MVP1-45] Packet Serializer (`packet_serializer.h` & `.cpp`)
Protocol Buffers 메시지를 네트워크로 전송하기 위한 형태로 직렬화하고, 받은 데이터를 다시 메시지로 역직렬화하는 유틸리티 클래스입니다. 패킷의 시작과 끝을 명확히 구분하기 위해, 실제 데이터 앞에 4바이트 크기의 헤더를 추가합니다.
*   `[SEQUENCE: MVP1-46]` Packet serialization utilities
*   `[SEQUENCE: MVP1-47]` Serialize packet with 4-byte size header
*   `[SEQUENCE: MVP1-48]` Create a packet with header and payload
*   `[SEQUENCE: MVP1-49]` Extract message from packet payload

### [SEQUENCE: MVP1-50] Packet Handler (`packet_handler.h` & `.cpp`)
수신된 패킷을 타입에 따라 적절한 처리 함수로 보내주는 라우팅 역할을 합니다. `std::function`과 `unordered_map`을 사용하여, 각 패킷 타입에 대한 핸들러를 동적으로 등록하고 호출할 수 있는 유연한 구조를 가집니다.
*   `[SEQUENCE: MVP1-51]` Packet handler callback type
*   `[SEQUENCE: MVP1-52]` Packet handler interface
*   `[SEQUENCE: MVP1-53]` Basic packet handler implementation

## [SEQUENCE: MVP1-54] Server Applications (`src/server/`)
실제 서버 프로세스의 진입점(main 함수)과 각 서버의 특화된 로직을 포함합니다. MVP 1에서는 로그인 서버와 게임 서버, 두 개의 독립적인 서버 애플리케이션을 구축합니다.

### [SEQUENCE: MVP1-55] Login Server (`login/`)
사용자 인증을 전담하는 서버입니다. 게임 서버와 분리하여, 인증 로직의 복잡성과 부하가 게임 월드에 영향을 주지 않도록 합니다.
*   `[SEQUENCE: MVP1-56]` `main.cpp`: 로그인 서버의 main 함수, 시그널 처리, 로깅 설정 등.
*   `[SEQUENCE: MVP1-57]` `auth_handler.h`: 로그인/아웃, 하트비트 등 인증 관련 패킷 핸들러 인터페이스.
*   `[SEQUENCE: MVP1-58]` `auth_handler.cpp`: 핸들러 구현부. MVP 1에서는 DB 대신 인-메모리 방식으로 사용자 정보를 관리.

### [SEQUENCE: MVP1-59] Game Server (`game/`)
실제 게임 로직이 실행될 게임 서버의 기본 구조입니다. MVP 1에서는 기본적인 서버 루프와 인증 핸들러 연결 부분만 구현합니다.
*   `[SEQUENCE: MVP1-60]` `main.cpp`: 게임 서버의 main 함수 및 기본 설정.
*   `[SEQUENCE: MVP1-61]` `auth_handler.h`: 게임 서버용 인증 핸들러 인터페이스.
*   `[SEQUENCE: MVP1-62]` `auth_handler.cpp`: 로그인 서버에서 받은 토큰을 검증하는 로직 구현.
*   `[SEQUENCE: MVP1-81]` `game_server.h`: The main game server class that orchestrates all major systems.
*   `[SEQUENCE: MVP1-82]` `game_server.cpp`: Implementation of the GameServer class.
*   `[SEQUENCE: MVP1-83]` The main `Run()` method containing the core game loop.
*   `[SEQUENCE: MVP1-84]` A lock-free MPSC queue (`MpscLfQueue`) to receive packets from network threads.
*   `[SEQUENCE: MVP1-85]` `ProcessIncomingPackets()` method to dequeue and handle packets.
*   `[SEQUENCE: MVP1-86]` Integration with the ECS `World` for system updates.
*   `[SEQUENCE: MVP1-87]` Tick-rate management to ensure a consistent simulation speed.

## [SEQUENCE: MVP1-63] Monitoring & Components
서버의 상태를 외부에서 확인하고, 네트워크 동기화에 필요한 기본 컴포넌트를 정의합니다.

### [SEQUENCE: MVP1-64] Server Monitoring (`http_metrics_server.h` & `.cpp`, `server_monitor.h` & `.cpp`)
서버의 CPU, 메모리 사용량, 동시 접속자 수 등의 주요 지표를 수집하고, 이를 외부에서 조회할 수 있도록 HTTP 엔드포인트를 제공합니다. Prometheus와 같은 표준 모니터링 도구와 쉽게 연동할 수 있도록 설계되었습니다.
*   `[SEQUENCE: MVP1-65]` `server_monitor.h`: 서버의 시스템 리소스(CPU, 메모리 등)를 주기적으로 수집하는 클래스.
*   `[SEQUENCE: MVP1-66]` `http_metrics_server.h`: 수집된 메트릭을 HTTP로 노출하는 경량 웹서버.

### [SEQUENCE: MVP1-67] Network Component (`src/game/components/network_component.h`)
ECS 아키텍처 내에서, 상태 변경 시 클라이언트에 동기화가 필요한 엔티티를 식별하기 위한 컴포넌트입니다. 이 컴포넌트는 동기화가 필요한 데이터(위치, 체력 등)가 변경되었음을 알리는 'dirty flag' 역할을 수행하여, 불필요한 네트워크 트래픽을 줄이는 데 사용됩니다.
*   `[SEQUENCE: MVP1-68]` Network synchronization component
*   `[SEQUENCE: MVP1-69]` Mark for update
*   `[SEQUENCE: MVP1-70]` Check if owned by session
*   `[SEQUENCE: MVP1-71]` Check if needs any update

## [SEQUENCE: MVP1-72] Networking Tests (`tests/unit/test_networking.cpp`)
네트워킹 계층의 안정성과 정확성을 보장하기 위한 단위 테스트입니다. 서버의 시작/종료, 클라이언트의 동시 접속, 대용량 패킷 전송, 세션 타임아웃 등 다양한 시나리오를 검증하여 네트워크 엔진의 신뢰도를 확보합니다.
*   `[SEQUENCE: MVP1-73]` Basic networking tests
*   `[SEQUENCE: MVP1-74]` Test server startup and shutdown
*   `[SEQUENCE: MVP1-75]` Test client connection
*   `[SEQUENCE: MVP1-76]` Test packet sending and receiving
*   `[SEQUENCE: MVP1-77]` Test multiple concurrent connections
*   `[SEQUENCE: MVP1-78]` Test packet fragmentation handling
*   `[SEQUENCE: MVP1-79]` Test session timeout
*   `[SEQUENCE: MVP1-80]` Test broadcast functionality
