# [SEQUENCE: MVP0-8] Docker containerization setup
# [SEQUENCE: MVP6-4] A multi-stage Dockerfile for creating optimized runtime images.
# [SEQUENCE: 1] Multi-stage build for production MMORPG server
FROM ubuntu:22.04 AS builder

# [SEQUENCE: 2] Install build dependencies with specific versions
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libboost-all-dev \
    protobuf-compiler \
    libprotobuf-dev \
    python3-pip \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# [SEQUENCE: 3] Install conan for dependency management
RUN pip3 install conan==2.0.0

# [SEQUENCE: 4] Set build environment
WORKDIR /app
ENV CMAKE_BUILD_TYPE=Release

# [SEQUENCE: 5] Copy dependency files first (for better caching)
COPY CMakeLists.txt conanfile.txt* ./

# [SEQUENCE: 6] Install dependencies
RUN if [ -f conanfile.txt ]; then \
        conan install . --output-folder=build --build=missing; \
    fi

# [SEQUENCE: 7] Copy source files
COPY src/ ./src/
COPY include/ ./include/
COPY proto/ ./proto/
COPY tests/ ./tests/

# [SEQUENCE: 8] Build the project with optimizations
RUN mkdir -p build && cd build && \
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_FLAGS="-O3 -march=native" \
        -DBUILD_TESTS=OFF && \
    make -j$(nproc) game_server

# [SEQUENCE: 9] Runtime stage - minimal image
FROM ubuntu:22.04 AS runtime

# [SEQUENCE: 10] Install only runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    libboost-system1.74.0 \
    libboost-thread1.74.0 \
    libboost-filesystem1.74.0 \
    libprotobuf23 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# [SEQUENCE: 11] Create non-root user with specific UID
RUN groupadd -g 1000 gameserver && \
    useradd -u 1000 -g gameserver -m -s /bin/bash gameserver

# [SEQUENCE: 12] Setup application structure
WORKDIR /app

# [SEQUENCE: 13] Copy binary and configs with proper permissions
COPY --from=builder --chown=gameserver:gameserver /app/build/src/server/game_server /app/bin/
COPY --chown=gameserver:gameserver config/ ./config/

# [SEQUENCE: 14] Create necessary directories
RUN mkdir -p /app/logs /app/data && \
    chown -R gameserver:gameserver /app

# [SEQUENCE: 15] Configure for production
ENV LOG_LEVEL=info \
    SERVER_PORT=8080 \
    METRICS_PORT=9090 \
    HEALTH_PORT=8081

# [SEQUENCE: 16] Switch to non-root user
USER gameserver

# [SEQUENCE: 17] Expose service ports
EXPOSE 8080 8081 9090

# [SEQUENCE: 18] Health check configuration
HEALTHCHECK --interval=30s --timeout=3s --start-period=10s --retries=3 \
    CMD curl -f http://localhost:8081/health || exit 1

# [SEQUENCE: 19] Entry point with signal handling
ENTRYPOINT ["/app/bin/game_server"]
CMD ["--config", "/app/config/server.yaml"]
