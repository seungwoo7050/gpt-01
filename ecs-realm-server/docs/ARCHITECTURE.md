# Project Architecture

This document provides a high-level overview of the technical architecture of the C++ MMORPG server. The server is designed from the ground up to be scalable, performant, and maintainable, employing modern C++ and industry-standard architectural patterns.

---

## 1. Overall Architecture

The server is built on a modular, multi-layered architecture. The core philosophy is to separate concerns, allowing for independent development and testing of each major component. 

![High-Level Architecture Diagram](https://i.imgur.com/your-diagram-link.png)  
*(Note: A proper diagram should be created and linked here to illustrate the interaction between the layers.)*

The main layers are:

- **Networking Layer:** Handles all communication with clients using a high-performance, asynchronous I/O model.
- **Core Engine:** The heart of the server, featuring a data-oriented Entity Component System (ECS) to manage all game state and logic.
- **Database & Caching Layer:** Manages data persistence, caching, and scaling strategies for handling large volumes of player and game data.
- **Scripting Layer:** Provides an interface to a Lua scripting engine, allowing for flexible and hot-reloadable game logic.

---

## 2. Networking Layer

The networking layer is designed for massive concurrency and low latency, which are critical for a real-time MMORPG.

### Key Technologies
- **Boost.Asio:** Used for its platform-independent, scalable, and asynchronous I/O model (Proactor pattern). This allows a small pool of threads to efficiently manage thousands of concurrent client connections.
- **OpenSSL:** Integrated with Boost.Asio to provide TLS-encrypted communication channels, securing all data transfer against snooping and man-in-the-middle attacks.

### Core Design Patterns

- **Dual-Protocol (TCP/UDP):** The server uses two parallel communication channels for each client:
    - **Secure TCP (via TLS):** A reliable, ordered channel used for critical game events like login, chat, and quest completion. 
    - **Unreliable UDP:** A low-latency channel used for frequent, non-critical updates like real-time player movement. A custom "UDP Handshake" mechanism associates the UDP endpoint with an authenticated TCP session.

- **Asynchronous Operations & Object Lifetime:** To prevent blocking the server threads, all network operations are asynchronous. The lifetime of session objects during these operations is carefully managed using `std::shared_ptr` and `std::enable_shared_from_this`, which are bound into the operation callbacks to prevent use-after-free errors.

- **Strands for Synchronization:** Instead of traditional mutexes, each session's handlers are wrapped in a `boost::asio::strand`. This guarantees that handlers for a specific session are executed serially, preventing data races within that session without the overhead of locking.

- **Protocol Buffers:** All network packets are defined and serialized using Google Protocol Buffers. This provides a language-neutral, efficient binary format with a strict schema, reducing bandwidth and preventing data format mismatches.

---

## 3. Core Engine (Entity Component System)

The server's core logic and state management are built around a high-performance, **Data-Oriented Entity Component System (ECS)**.

### Design Philosophy
- **Data-Oriented Design (DoD):** Instead of a traditional Object-Oriented approach where data and logic are encapsulated together (which leads to poor CPU cache performance), we separate them. 
    - **Components:** Are plain-old-data structures (PODS) holding game state (e.g., `PositionComponent`, `HealthComponent`).
    - **Systems:** Contain the logic that iterates over entities with specific sets of components (e.g., a `MovementSystem` iterates over entities with `PositionComponent` and `VelocityComponent`).
- **Structure of Arrays (SoA):** Components are stored in tightly packed, contiguous arrays. This ensures that when a system runs, all the data it needs is loaded into the CPU cache in a linear fashion, maximizing cache hits and providing a massive performance boost for processing thousands of entities per frame.

### Key Components
- **`OptimizedWorld`:** The central facade for all ECS operations.
- **`DenseEntityManager`:** Manages the lifecycle of entities, recycling IDs to maintain a dense entity list.
- **`ComponentManager`:** Stores all components in packed arrays, one for each component type.
- **`SystemManager`:** Manages all systems and ensures they operate on the correct set of entities.

---

## 4. Database & Caching Layer

This layer is designed for scalability and performance, anticipating the need to handle large amounts of persistent data for players, guilds, and game state.

### Key Strategies

- **Connection Pooling:** To avoid the high overhead of creating database connections, the server maintains a pool of pre-established connections. This is managed by the `ConnectionPoolManager`.

- **In-Memory Caching (`CacheManager`):** A thread-safe, in-memory cache with a TTL-based eviction policy is used to store frequently accessed but rarely changed data (e.g., item stats, NPC definitions). This dramatically reduces read load on the database.

- **Read/Write Splitting (`ReadReplicaManager`):** The architecture supports splitting database traffic between a primary (write) database and one or more read replicas. The `ReadReplicaManager` uses a simple round-robin algorithm to distribute read queries across the replicas, linearly scaling read throughput.

- **Sharding (`ShardManager`):** The design anticipates the need for horizontal scaling. The `ShardManager` provides the logic to distribute data across multiple independent database servers (shards) based on a key (e.g., `player_id`). This allows the system to scale its write capacity beyond the limits of a single server.

- **Distributed Locking (`DistributedLockManager`):** For operations that must be atomic across a distributed, multi-server environment (e.g., unique character name creation), a distributed lock manager is implemented using Redis. It leverages Redis's atomic `SET NX PX` command to ensure that only one server can acquire a lock at a time, preventing race conditions.

---

## 5. Scripting Layer

To provide flexibility and allow for rapid iteration of game logic without recompiling the C++ core, the server integrates a Lua scripting engine.

### Key Technologies
- **Lua:** A lightweight, fast, and easily embeddable scripting language, which is the industry standard for game scripting.
- **sol2:** A modern, header-only C++ library used to create safe and easy-to-use bindings between C++ and Lua.

### Core Design
- **`ScriptManager`:** A singleton class that owns the `sol::state` (the Lua virtual machine).
- **C++/Lua Bindings:** The `ScriptManager` is responsible for exposing C++ functions and classes to the Lua environment. This allows scripts to call performance-critical C++ code (e.g., `native_print(...)`) and for C++ to invoke scripted logic (e.g., for quest triggers or skill effects).
- **Safety:** Lua runs in a sandboxed environment, meaning a bug in a Lua script will not crash the entire C++ server, making it a safe way to deploy new logic quickly.
