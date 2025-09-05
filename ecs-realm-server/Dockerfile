# [SEQUENCE: MVP8-9] A multi-stage Dockerfile for creating optimized runtime images.
# The builder stage compiles the application, and the runtime stage copies only the necessary artifacts.
FROM ubuntu:22.04 AS builder

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

RUN pip3 install conan==2.0.0

WORKDIR /app
ENV CMAKE_BUILD_TYPE=Release

COPY CMakeLists.txt conanfile.txt* ./

RUN if [ -f conanfile.txt ]; then \
        conan install . --output-folder=build --build=missing; \
    fi

COPY src/ ./src/
COPY include/ ./include/
COPY proto/ ./proto/
COPY tests/ ./tests/

RUN mkdir -p build && cd build && \
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_FLAGS="-O3 -march=native" \
        -DBUILD_TESTS=OFF && \
    make -j$(nproc) mmorpg_server

FROM ubuntu:22.04 AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    libboost-system1.74.0 \
    libboost-thread1.74.0 \
    libboost-filesystem1.74.0 \
    libprotobuf23 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

RUN groupadd -g 1000 gameserver && \
    useradd -u 1000 -g gameserver -m -s /bin/bash gameserver

WORKDIR /app

COPY --from=builder --chown=gameserver:gameserver /app/build/mmorpg_server /app/bin/
COPY --chown=gameserver:gameserver config/ ./config/

RUN mkdir -p /app/logs /app/data && \
    chown -R gameserver:gameserver /app

ENV LOG_LEVEL=info \
    SERVER_PORT=8080 \
    METRICS_PORT=9090 \
    HEALTH_PORT=8081

USER gameserver

EXPOSE 8080 8081 9090

HEALTHCHECK --interval=30s --timeout=3s --start-period=10s --retries=3 \
    CMD curl -f http://localhost:8081/health || exit 1

ENTRYPOINT ["/app/bin/mmorpg_server"]
CMD ["--config", "/app/config/server.yaml"]