# C++ MMORPG Server Engine

This project is a high-performance, scalable, and modern C++ server engine for a Massively Multiplayer Online Role-Playing Game (MMORPG). It is built from the ground up using industry-standard best practices to serve as a robust backend for a real-time, multi-user game world. 

This repository serves as a comprehensive portfolio piece, demonstrating advanced concepts in C++, networking, database design, and DevOps.

---

## Core Features

- **High-Performance Networking:** Asynchronous, multi-threaded I/O using **Boost.Asio**. Supports thousands of concurrent clients.
- **Secure Communication:** All client-server communication is encrypted using **TLS 1.2/1.3 (OpenSSL)**.
- **Dual-Protocol:** Uses both **TCP** for reliable messages (chat, commands) and **UDP** for low-latency, real-time data (player movement).
- **Data-Oriented ECS:** A high-performance **Entity Component System** designed with data-oriented principles (SoA) for maximum CPU cache efficiency.
- **Advanced Database Architecture:**
    - **Connection Pooling** to manage database connections efficiently.
    - **In-Memory Caching** with TTL-based eviction.
    - Support for **Read/Write Splitting** to scale read-heavy workloads.
    - **Sharding** and **Partitioning** managers for horizontal and vertical database scaling.
- **Distributed Locking:** Uses **Redis** to handle distributed locks, ensuring data consistency in a multi-server environment.
- **Scripting Engine:** Integrates a **Lua** engine using **sol2**, allowing for flexible game logic that can be updated without recompiling the server.
- **Modern DevOps:**
    - **Containerized** with a multi-stage `Dockerfile`.
    - **Local Development Stack** managed by `docker-compose`.
    - **Kubernetes** manifests for production-grade deployment.
    - **CI/CD Pipeline** with GitHub Actions to automatically build and test the project.

---

## Documentation

This project is extensively documented to explain not just *what* was built, but *why* it was built that way.

- **[Getting Started](./docs/GETTING_STARTED.md):** A practical, step-by-step guide to set up your environment, build the project, and run the server.
- **[Project Architecture](./docs/ARCHITECTURE.md):** A deep dive into the technical architecture, design patterns, and key technology choices.
- **[Deployment Guide](./docs/DEPLOYMENT.md):** Instructions on how to deploy the server using Docker and Kubernetes.
- **[Contributing Guide](./CONTRIBUTING.md):** Guidelines for contributing to the project.

---

## Quick Start

> For detailed instructions, please see the [Getting Started](./docs/GETTING_STARTED.md) guide.

1.  **Prerequisites:** C++20 Compiler, CMake, Conan, Docker.

2.  **Install Dependencies (from project root):**
    ```bash
    cd ecs-realm-server
    conan install . --build=missing
    ```

3.  **Build (from project root):**
    ```bash
    cd ecs-realm-server/build
    cmake ..
    make -j
    ```

4.  **Run Local Stack (from `ecs-realm-server/` directory):**
    ```bash
    docker-compose up -d
    ```

5.  **Generate Certificate & Run Server (from project root):**
    ```bash
    openssl req -x509 -newkey rsa:2048 -keyout server.key -out server.crt -days 365 -nodes -subj "/C=US/ST=CA/L=SF/O=MyCo/CN=localhost"
    ./ecs-realm-server/build/mmorpg_server
    ```
