# [SEQUENCE: MVP1-1] MVP 1: Core Networking & Server Loop (Redocumented)

## [SEQUENCE: MVP1-2] Introduction
*This document has been rewritten from scratch to accurately reflect the current, improved codebase and establish a logical, sequential history for MVP 1.*

MVP 1 builds the fundamental backbone of the server: the network communication layer and the main server loop. This stage implements an asynchronous TCP server using Boost.Asio, capable of handling concurrent client connections efficiently. It establishes a robust, SSL-encrypted communication channel and a clear protocol for message exchange using Protocol Buffers. The goal is to create a stable and scalable network engine, forming a solid foundation for all subsequent features.

## Core Components

### Logging
*   **[SEQUENCE: MVP1-3] `logger.h`:** Defines the `Logger` class, a thread-safe, singleton-based utility wrapping the `spdlog` library to provide consistent logging throughout the server.
*   **[SEQUENCE: MVP1-4] `logger.cpp`:** Implements the `Initialize` method to configure the logger settings.
*   **[SEQUENCE: MVP1-5] `logger.cpp`:** Implements the `GetLogger` method to provide global access to the logger instance.

### Data Contracts (Protocol Buffers)
*   **[SEQUENCE: MVP1-6] `common.proto`:** Defines common data structures like `Vector3` used across different domains.
*   **[SEQUENCE: MVP1-7] `auth.proto`:** Defines messages related to authentication, such as login and logout requests/responses.
*   **[SEQUENCE: MVP1-8] `game.proto`:** Defines messages for core gameplay, including movement and combat.
*   **[SEQUENCE: MVP1-9] `packet.proto`:** Defines the top-level `Packet` wrapper that contains a header and a payload for all network messages, ensuring a consistent communication structure.

### Network Utilities
*   **[SEQUENCE: MVP1-10] `packet_serializer.h`:** Declares the `PacketSerializer` namespace, a utility for serializing Protobuf messages into a byte buffer with a length prefix and deserializing them back.
*   **[SEQUENCE: MVP1-11] `packet_serializer.cpp`:** Implements the `Serialize` function.
*   **[SEQUENCE: MVP1-12] `packet_serializer.cpp`:** Implements the `Deserialize` function.
*   **[SEQUENCE: MVP1-13] `packet_handler.h`:** Defines the `IPacketHandler` interface and the `PacketHandlerCallback` type, establishing a contract for packet processing.
*   **[SEQUENCE: MVP1-14] `packet_handler.h`:** Defines the `PacketHandler` class, which maps message types to their corresponding handler functions.
*   **[SEQUENCE: MVP1-15] `packet_handler.cpp`:** Implements the `RegisterHandler` function.
*   **[SEQUENCE: MVP1-16] `packet_handler.cpp`:** Implements the `Handle` function, which dispatches received packets to the correct handler.

### Network Session Layer
*   **[SEQUENCE: MVP1-17] `session.h`:** Defines the `Session` class, which represents a single client connection, handling its I/O operations and lifecycle.
*   **[SEQUENCE: MVP1-18] `session.h`:** Declares the core public methods for the `Session` class (`Start`, `Disconnect`, `Send`).
*   **[SEQUENCE: MVP1-19] `session.h`:** Declares the getters and setters for session state and properties.
*   **[SEQUENCE: MVP1-20] `session.h`:** Declares the private methods for handling the asynchronous I/O chain (`DoHandshake`, `DoReadHeader`, etc.).
*   **[SEQUENCE: MVP1-21] `session.cpp`:** Implements the `Session` constructor.
*   **[SEQUENCE: MVP1-22] `session.cpp`:** Implements the session lifecycle methods (`Start`, `Disconnect`).
*   **[SEQUENCE: MVP1-23] `session.cpp`:** Implements the asynchronous `Send` method.
*   **[SEQUENCE: MVP1-24] `session.cpp`:** Implements the asynchronous read chain, from SSL handshake to packet processing.
*   **[SEQUENCE: MVP1-25] `session.cpp`:** Implements the asynchronous write chain.
*   **[SEQUENCE: MVP1-26] `session_manager.h`:** Defines the `SessionManager` class to manage all active sessions in a thread-safe manner.
*   **[SEQUENCE: MVP1-27] `session_manager.h`:** Declares the core public methods for session lifecycle and broadcasting.
*   **[SEQUENCE: MVP1-28] `session_manager.h`:** Declares methods for mapping sessions to player IDs.
*   **[SEQUENCE: MVP1-29] `session_manager.h`:** Declares methods for UDP endpoint management (placeholders for MVP6).
*   **[SEQUENCE: MVP1-30] `session_manager.cpp`:** Implements `Register` and `Unregister`.
*   **[SEQUENCE: MVP1-31] `session_manager.cpp`:** Implements `GetSession` and `Broadcast`.
*   **[SEQUENCE: MVP1-32] `session_manager.cpp`:** Implements the player ID mapping functions.
*   **[SEQUENCE: MVP1-33] `session_manager.cpp`:** Implements the UDP-related functions.

### Network Server Layer
*   **[SEQUENCE: MVP1-34] `tcp_server.h`:** Defines the `TcpServer` class, which accepts incoming TCP connections.
*   **[SEQUENCE: MVP1-35] `tcp_server.cpp`:** Implements the constructor, initializing the SSL context and acceptor.
*   **[SEQUENCE: MVP1-36] `tcp_server.cpp`:** Implements `run` and `stop` to control the server.
*   **[SEQUENCE: MVP1-37] `tcp_server.cpp`:** Implements `do_accept`, the core asynchronous loop for accepting new connections.

### Application Entry Point
*   **[SEQUENCE: MVP1-38] `main.cpp`:** The main entry point, responsible for initializing the logger and setting up signal handlers for graceful shutdown.
*   **[SEQUENCE: MVP1-39] `main.cpp`:** Sets up the core server components: `io_context`, `SessionManager`, and `PacketHandler`.
*   **[SEQUENCE: MVP1-40] `main.cpp`:** Registers a simple handler for `LoginRequest` for basic testing purposes.
*   **[SEQUENCE: MVP1-41] `main.cpp`:** Creates the server instance and a thread pool to run the `io_context`, starting the server's event processing.

## Build Verification after Sync

The entire codebase for MVP 1 was refactored to ensure logical consistency between the documentation and the code. All sequence markers were reassigned. A build was performed to ensure that this new, clean baseline is stable.

*   **Build Command:** `cd ecs-realm-server/build && cmake .. && make`
*   **Result:** **SUCCESS (100%)**
*   **Analysis:** The successful build confirms that the re-sequenced and refactored components integrate correctly. The networking layer, though using an advanced implementation from later MVPs, forms a coherent and buildable unit for MVP 1. This provides a stable foundation for the next stages.

## In-depth Analysis for Technical Interviews

*   **Asynchronous I/O (Boost.Asio):** We chose Boost.Asio for its Proactor-based, platform-independent asynchronous model. This allows a small number of threads to handle thousands of concurrent connections, which is essential for a scalable MMO server. The alternative, raw `epoll`/`kqueue`, offers maximum performance but at the cost of complexity and portability. Asio provides the right balance.
*   **Object Lifetime in Async Operations:** The most critical challenge in Asio is managing object lifetimes. We use `std::shared_ptr` and `std::enable_shared_from_this` extensively. When an async operation (like `async_read`) is initiated on a `Session`, a `shared_ptr` to the session is bound into the callback lambda. This ensures the `Session` object remains alive until the callback completes, preventing use-after-free errors.
*   **Strands for Synchronization:** Instead of using mutexes within a `Session`, we use a `boost::asio::strand`. A strand is a "virtual mutex" that guarantees that no two handlers for a given session will execute concurrently. This serializes access to the session's internal data (like its read/write queues) without the overhead of traditional locking, improving performance.
*   **Data Serialization (Protobuf):** We use Protocol Buffers for its efficient binary serialization and strict schema definition (`.proto` files). This reduces packet size and parsing time compared to JSON and prevents data format mismatches between client and server.
