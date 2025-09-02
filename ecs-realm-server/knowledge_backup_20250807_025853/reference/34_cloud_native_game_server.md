# 25단계: 클라우드 네이티브 게임 서버 - 전 세계를 하나로 연결하는 거대한 시스템
*전 세계 어디서든 100ms 이내 응답속도로 5000명이 동시에 플레이할 수 있는 글로벌 게임 인프라*

> **🎯 목표**: 5,000명 이상 동시 접속자를 처리하는 클라우드 네이티브 게임 서버를 Docker와 Kubernetes로 구축하고 운영하는 완전한 시스템 마스터

---

## 📋 문서 정보

- **난이도**: 🔴 고급 (클라우드 아키텍처의 대가!)
- **예상 학습 시간**: 12-15시간 (글로벌 서비스는 복잡함)
- **필요 선수 지식**: 
  - ✅ [1-24단계](./01_advanced_cpp_features.md) 모든 내용 완료
  - ✅ Docker 컨테이너 기술
  - ✅ Kubernetes 기본 개념
  - ✅ 클라우드 서비스 이해 (AWS/GCP/Azure)
- **실습 환경**: 클라우드 플랫폼, Kubernetes 클러스터
- **최종 결과물**: 아마존급 글로벌 게임 인프라!

## 🔥 흔한 실수와 해결방법 (Common Mistakes & Solutions)

### 1. Docker 이미지 크기 최적화 실패
```cpp
// ❌ 잘못된 방법 - 모든 빌드 도구를 런타임에 포함
// [SEQUENCE: 1] 단일 스테이지로 빌드하면 이미지가 2GB+
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y \
    build-essential cmake git \
    libboost-all-dev    # 모든 Boost 라이브러리 설치
COPY . /app
RUN cd /app && cmake . && make
CMD ["/app/game_server"]

// ✅ 올바른 방법 - 멀티스테이지 빌드로 최종 이미지 200MB
// [SEQUENCE: 2] 빌드와 런타임 분리
FROM ubuntu:22.04 AS builder
# 빌드 도구만 설치
RUN apt-get update && apt-get install -y build-essential cmake
COPY . /app
RUN cd /app && cmake . && make

FROM ubuntu:22.04 AS runtime
# 런타임에 필요한 라이브러리만 복사
COPY --from=builder /app/game_server /usr/local/bin/
RUN apt-get update && apt-get install -y --no-install-recommends \
    libboost-system1.74.0
CMD ["game_server"]
```

### 2. Kubernetes 리소스 제한 미설정
```yaml
# ❌ 잘못된 방법 - 리소스 제한 없음
# [SEQUENCE: 3] Pod가 노드의 모든 리소스를 소비 가능
apiVersion: apps/v1
kind: Deployment
metadata:
  name: game-server
spec:
  replicas: 3
  template:
    spec:
      containers:
      - name: game-server
        image: mmorpg/game-server:latest

# ✅ 올바른 방법 - 명확한 리소스 제한
# [SEQUENCE: 4] 예측 가능한 성능과 안정성
apiVersion: apps/v1
kind: Deployment
metadata:
  name: game-server
spec:
  replicas: 3
  template:
    spec:
      containers:
      - name: game-server
        image: mmorpg/game-server:latest
        resources:
          requests:
            memory: "1Gi"
            cpu: "500m"
          limits:
            memory: "2Gi"
            cpu: "1000m"
```

### 3. 헬스체크 없는 배포
```cpp
// ❌ 잘못된 방법 - 단순 프로세스 확인만 수행
// [SEQUENCE: 5] 서버가 실제로 동작하는지 알 수 없음
int main() {
    GameServer server;
    server.Run();  // 무한 루프
    return 0;
}

// ✅ 올바른 방법 - 실제 동작 상태 확인
// [SEQUENCE: 6] Readiness와 Liveness 구분
class GameServer {
public:
    void RegisterHealthChecks() {
        // Liveness: 프로세스가 살아있는가?
        http_server_.RegisterRoute("/health/live", [this]() {
            return json{{"status", "ok"}};
        });
        
        // Readiness: 트래픽을 받을 준비가 되었는가?
        http_server_.RegisterRoute("/health/ready", [this]() {
            if (db_connected_ && redis_connected_ && 
                world_loaded_ && network_ready_) {
                return json{{"status", "ready"}};
            }
            return json{{"status", "not_ready"}};
        });
    }
};
```

## 🚀 실습 프로젝트 (Practice Projects)

### 📌 기초 프로젝트: 간단한 게임 서버 컨테이너화
**목표**: 기본 TCP 게임 서버를 Docker로 컨테이너화하고 로컬 Kubernetes에 배포

1. **단계별 구현**:
   - Echo 게임 서버 작성 (1,000줄)
   - Dockerfile 작성 및 최적화
   - Docker Compose로 로컬 테스트
   - Minikube에 배포
   - 기본 스케일링 테스트

2. **학습 포인트**:
   - 컨테이너 네트워킹 이해
   - 볼륨 마운트와 설정 관리
   - 기본 Kubernetes 오브젝트

### 🚀 중급 프로젝트: 마이크로서비스 게임 서버
**목표**: 5개 마이크로서비스로 구성된 게임 서버를 Kubernetes에 배포

1. **서비스 구성**:
   - Gateway Service (인증/라우팅)
   - World Service (게임 로직)
   - Database Service (PostgreSQL)
   - Cache Service (Redis)
   - Match Service (매치메이킹)

2. **구현 기능**:
   - Helm 차트 작성
   - Service Mesh (Istio) 통합
   - 자동 스케일링 설정
   - 모니터링 대시보드 구축

### 🏆 고급 프로젝트: 글로벌 MMORPG 인프라
**목표**: 멀티리전 클러스터에서 5,000명 동시접속 처리

1. **고급 기능 구현**:
   - 멀티 클러스터 페더레이션
   - GitOps 기반 배포 파이프라인
   - 카오스 엔지니어링 테스트
   - 비용 최적화 자동화
   - AI 기반 예측 스케일링

2. **성능 목표**:
   - P95 레이턴시 < 100ms
   - 99.99% 가용성
   - 자동 장애 복구 < 1분
   - 클러스터 간 자동 페일오버

## 📊 학습 체크리스트 (Learning Checklist)

### 🥉 브론즈 레벨
- [ ] Docker 기본 명령어 숙달
- [ ] Dockerfile 작성 및 빌드
- [ ] Kubernetes 기본 개념 이해
- [ ] kubectl 기본 명령어 사용

### 🥈 실버 레벨
- [ ] 멀티스테이지 빌드 최적화
- [ ] Kubernetes 리소스 작성 (Deployment, Service, ConfigMap)
- [ ] Helm 차트 생성 및 관리
- [ ] 기본 모니터링 설정 (Prometheus/Grafana)

### 🥇 골드 레벨
- [ ] Service Mesh (Istio) 구성
- [ ] 자동 스케일링 (HPA/VPA) 튜닝
- [ ] CI/CD 파이프라인 구축
- [ ] 보안 스캐닝 및 정책 적용

### 💎 플래티넘 레벨
- [ ] 멀티 클러스터 운영
- [ ] GitOps 워크플로우 설계
- [ ] 카오스 엔지니어링 실행
- [ ] 비용 최적화 자동화 구현

## 📚 추가 학습 자료 (Additional Resources)

### 📖 추천 도서
1. **"Kubernetes in Action"** - Marko Lukša
   - Kubernetes 핵심 개념부터 고급 패턴까지
   
2. **"Site Reliability Engineering"** - Google
   - 구글의 SRE 방법론과 실전 경험

3. **"Cloud Native DevOps with Kubernetes"** - O'Reilly
   - 실전 DevOps 파이프라인 구축

### 🎓 온라인 코스
1. **CKA (Certified Kubernetes Administrator)**
   - Linux Foundation 공식 인증 과정
   
2. **Istio Service Mesh Deep Dive**
   - 서비스 메시 고급 패턴

3. **AWS/GCP/Azure Game Tech Specialization**
   - 클라우드별 게임 서버 최적화

### 🛠 필수 도구
1. **k9s** - Kubernetes CLI UI
2. **Lens** - Kubernetes IDE
3. **Kustomize** - 구성 관리
4. **ArgoCD** - GitOps 배포

### 💬 커뮤니티
1. **CNCF Slack** (#kubernetes-users)
2. **Game Server Hosting Discord**
3. **Reddit** r/kubernetes, r/gamedev
4. **Stack Overflow** [kubernetes] 태그

## Docker Implementation

### Multi-Stage Dockerfile
```dockerfile
# From actual Dockerfile - Multi-stage build for production MMORPG server
FROM ubuntu:22.04 AS builder

# Install build dependencies with specific versions
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

# Install conan for dependency management
RUN pip3 install conan==2.0.0

# Set build environment
WORKDIR /app
ENV CMAKE_BUILD_TYPE=Release

# Copy dependency files first (for better caching)
COPY CMakeLists.txt conanfile.txt* ./

# Install dependencies
RUN if [ -f conanfile.txt ]; then \
        conan install . --output-folder=build --build=missing; \
    fi

# Copy source files
COPY src/ ./src/
COPY include/ ./include/
COPY proto/ ./proto/
COPY tests/ ./tests/

# Build the project with optimizations
RUN mkdir -p build && cd build && \
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_CXX_FLAGS="-O3 -march=native" \
        -DBUILD_TESTS=OFF && \
    make -j$(nproc) game_server

# Runtime stage - minimal image
FROM ubuntu:22.04 AS runtime

# Install only runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    libboost-system1.74.0 \
    libboost-thread1.74.0 \
    libboost-filesystem1.74.0 \
    libprotobuf23 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user with specific UID
RUN groupadd -g 1000 gameserver && \
    useradd -u 1000 -g gameserver -m -s /bin/bash gameserver

# Setup application structure
WORKDIR /app

# Copy binary and configs with proper permissions
COPY --from=builder --chown=gameserver:gameserver /app/build/src/server/game_server /app/bin/
COPY --chown=gameserver:gameserver config/ ./config/

# Create necessary directories
RUN mkdir -p /app/logs /app/data && \
    chown -R gameserver:gameserver /app

# Configure for production
ENV LOG_LEVEL=info \
    SERVER_PORT=8080 \
    METRICS_PORT=9090 \
    HEALTH_PORT=8081

# Switch to non-root user
USER gameserver

# Expose service ports
EXPOSE 8080 8081 9090

# Health check configuration
HEALTHCHECK --interval=30s --timeout=3s --start-period=10s --retries=3 \
    CMD curl -f http://localhost:8081/health || exit 1

# Entry point with signal handling
ENTRYPOINT ["/app/bin/game_server"]
CMD ["--config", "/app/config/server.yaml"]
```

**Docker Build Optimization**:
- **Multi-stage build**: Reduces final image size by 70% (from 2GB to 600MB)
- **Layer caching**: Dependencies installed first for faster rebuilds
- **Security**: Non-root user with specific UID for Kubernetes compatibility
- **Health checks**: Built-in container health monitoring

## Docker Compose Multi-Zone Deployment

### Production Docker Compose Setup
```yaml
# From actual docker-compose.yml - Production-ready multi-server deployment
version: '3.8'

services:
  # Game server zone 1 - handles world area A
  game-server-zone1:
    build:
      context: .
      dockerfile: Dockerfile
      target: runtime
    container_name: mmorpg-server-zone1
    command: ["--config", "/app/config/server.yaml", "--zone", "1"]
    environment:
      - SERVER_NAME=Zone1
      - WORLD_ZONE_ID=1
      - SERVER_PORT=8080
      - METRICS_PORT=9090
      - HEALTH_PORT=8081
      - MAX_PLAYERS=1000
      - REDIS_URL=redis://redis:6379/0
      - DB_HOST=postgres
      - DB_NAME=gamedb
      - DB_USER=gameserver
      - DB_PASSWORD=${DB_PASSWORD:-gamepass123}
      - RABBITMQ_URL=amqp://game:game123@rabbitmq:5672/
      - LOG_LEVEL=${LOG_LEVEL:-info}
      - ENABLE_METRICS=true
    ports:
      - "8080:8080"
      - "9090:9090"
    volumes:
      - ./logs/zone1:/app/logs
      - ./config:/app/config:ro
    depends_on:
      redis:
        condition: service_healthy
      postgres:
        condition: service_healthy
      rabbitmq:
        condition: service_healthy
    networks:
      - game-network
    restart: unless-stopped
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8081/health"]
      interval: 30s
      timeout: 3s
      retries: 3

  # Game server zone 2 - handles world area B
  game-server-zone2:
    build:
      context: .
      dockerfile: Dockerfile
      target: runtime
    container_name: mmorpg-server-zone2
    command: ["--config", "/app/config/server.yaml", "--zone", "2"]
    environment:
      - SERVER_NAME=Zone2
      - WORLD_ZONE_ID=2
      - SERVER_PORT=8080
      - METRICS_PORT=9090
      - HEALTH_PORT=8081
      - MAX_PLAYERS=1000
      - REDIS_URL=redis://redis:6379/0
      - DB_HOST=postgres
      - DB_NAME=gamedb
      - DB_USER=gameserver
      - DB_PASSWORD=${DB_PASSWORD:-gamepass123}
      - RABBITMQ_URL=amqp://game:game123@rabbitmq:5672/
      - LOG_LEVEL=${LOG_LEVEL:-info}
      - ENABLE_METRICS=true
    ports:
      - "8082:8080"
      - "9091:9090"
    volumes:
      - ./logs/zone2:/app/logs
      - ./config:/app/config:ro
    depends_on:
      redis:
        condition: service_healthy
      postgres:
        condition: service_healthy
      rabbitmq:
        condition: service_healthy
    networks:
      - game-network
    restart: unless-stopped

  # PostgreSQL for persistent storage
  postgres:
    image: postgres:15-alpine
    container_name: mmorpg-postgres
    environment:
      POSTGRES_DB: gamedb
      POSTGRES_USER: gameserver
      POSTGRES_PASSWORD: ${DB_PASSWORD:-gamepass123}
      POSTGRES_INITDB_ARGS: "--encoding=UTF8 --lc-collate=C --lc-ctype=C"
    ports:
      - "5432:5432"
    volumes:
      - postgres-data:/var/lib/postgresql/data
      - ./sql/init.sql:/docker-entrypoint-initdb.d/01-init.sql:ro
      - ./sql/tables.sql:/docker-entrypoint-initdb.d/02-tables.sql:ro
    networks:
      - game-network
    restart: unless-stopped
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U gameserver -d gamedb"]
      interval: 10s
      timeout: 5s
      retries: 5

  # Redis for session state and matchmaking
  redis:
    image: redis:7-alpine
    container_name: mmorpg-redis
    command: >
      redis-server
      --appendonly yes
      --maxmemory 2gb
      --maxmemory-policy allkeys-lru
      --requirepass ${REDIS_PASSWORD:-redis123}
    ports:
      - "6379:6379"
    volumes:
      - redis-data:/data
    networks:
      - game-network
    restart: unless-stopped

  # RabbitMQ for cross-server messaging
  rabbitmq:
    image: rabbitmq:3-management-alpine
    container_name: mmorpg-rabbitmq
    environment:
      RABBITMQ_DEFAULT_USER: game
      RABBITMQ_DEFAULT_PASS: ${RABBITMQ_PASSWORD:-game123}
      RABBITMQ_DEFAULT_VHOST: /
    ports:
      - "5672:5672"
      - "15672:15672"  # Management UI
    volumes:
      - rabbitmq-data:/var/lib/rabbitmq
    networks:
      - game-network
    restart: unless-stopped

  # Nginx load balancer
  nginx:
    image: nginx:alpine
    container_name: mmorpg-nginx
    volumes:
      - ./nginx/nginx.conf:/etc/nginx/nginx.conf:ro
      - ./nginx/ssl:/etc/nginx/ssl:ro
    ports:
      - "80:80"
      - "443:443"
    depends_on:
      - game-server-zone1
      - game-server-zone2
    networks:
      - game-network
    restart: unless-stopped

  # Prometheus for metrics
  prometheus:
    image: prom/prometheus:latest
    container_name: mmorpg-prometheus
    command:
      - '--config.file=/etc/prometheus/prometheus.yml'
      - '--storage.tsdb.path=/prometheus'
      - '--storage.tsdb.retention.time=30d'
      - '--web.enable-lifecycle'
    volumes:
      - ./monitoring/prometheus.yml:/etc/prometheus/prometheus.yml:ro
      - ./monitoring/alerts.yml:/etc/prometheus/alerts.yml:ro
      - prometheus-data:/prometheus
    ports:
      - "9000:9090"
    networks:
      - game-network
    restart: unless-stopped

  # Grafana for visualization
  grafana:
    image: grafana/grafana:latest
    container_name: mmorpg-grafana
    environment:
      - GF_SECURITY_ADMIN_USER=${GRAFANA_USER:-admin}
      - GF_SECURITY_ADMIN_PASSWORD=${GRAFANA_PASSWORD:-admin123}
      - GF_USERS_ALLOW_SIGN_UP=false
      - GF_INSTALL_PLUGINS=grafana-clock-panel,grafana-simple-json-datasource
    volumes:
      - grafana-data:/var/lib/grafana
      - ./monitoring/grafana/provisioning:/etc/grafana/provisioning:ro
    ports:
      - "3000:3000"
    depends_on:
      - prometheus
    networks:
      - game-network
    restart: unless-stopped

# Network configuration
networks:
  game-network:
    driver: bridge
    ipam:
      config:
        - subnet: 172.20.0.0/16

# Volume definitions
volumes:
  postgres-data:
    driver: local
  redis-data:
    driver: local
  rabbitmq-data:
    driver: local
  prometheus-data:
    driver: local
  grafana-data:
    driver: local
```

**Multi-Zone Architecture Benefits**:
- **Zone Separation**: Each game server handles different world areas
- **Load Distribution**: 1,000 players per zone with horizontal scaling
- **Cross-Zone Communication**: RabbitMQ enables inter-zone messaging
- **Shared State**: Redis provides session management across zones
- **Monitoring**: Prometheus + Grafana for comprehensive metrics

## Kubernetes Implementation

### Base Deployment Configuration
```yaml
# From actual k8s/base/deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: game-server
  namespace: mmorpg-game
  labels:
    app.kubernetes.io/name: game-server
    app.kubernetes.io/component: backend
spec:
  # Replica configuration
  replicas: 2
  selector:
    matchLabels:
      app.kubernetes.io/name: game-server
      app.kubernetes.io/component: backend
  
  # Rolling update strategy
  strategy:
    type: RollingUpdate
    rollingUpdate:
      maxSurge: 1
      maxUnavailable: 0
  
  template:
    metadata:
      labels:
        app.kubernetes.io/name: game-server
        app.kubernetes.io/component: backend
      annotations:
        prometheus.io/scrape: "true"
        prometheus.io/port: "9090"
        prometheus.io/path: "/metrics"
    
    spec:
      # Security context
      securityContext:
        runAsNonRoot: true
        runAsUser: 1000
        fsGroup: 1000
      
      # Init container for DB migration
      initContainers:
      - name: db-migrate
        image: migrate/migrate:latest
        command:
        - migrate
        - -path=/migrations
        - -database=postgres://$(DB_USER):$(DB_PASSWORD)@$(DB_HOST)/$(DB_NAME)?sslmode=disable
        - up
        env:
        - name: DB_HOST
          value: postgres-service
        - name: DB_NAME
          value: gamedb
        - name: DB_USER
          valueFrom:
            secretKeyRef:
              name: db-credentials
              key: username
        - name: DB_PASSWORD
          valueFrom:
            secretKeyRef:
              name: db-credentials
              key: password
        volumeMounts:
        - name: migrations
          mountPath: /migrations
      
      # Main game server container
      containers:
      - name: game-server
        image: mmorpg/game-server:latest
        imagePullPolicy: IfNotPresent
        
        # Container ports
        ports:
        - name: game
          containerPort: 8080
          protocol: TCP
        - name: health
          containerPort: 8081
          protocol: TCP
        - name: metrics
          containerPort: 9090
          protocol: TCP
        
        # Environment variables
        env:
        - name: SERVER_NAME
          valueFrom:
            fieldRef:
              fieldPath: metadata.name
        - name: POD_IP
          valueFrom:
            fieldRef:
              fieldPath: status.podIP
        - name: WORLD_ZONE_ID
          value: "1"
        - name: LOG_LEVEL
          value: "info"
        - name: DB_HOST
          value: postgres-service
        - name: DB_NAME
          value: gamedb
        - name: DB_USER
          valueFrom:
            secretKeyRef:
              name: db-credentials
              key: username
        - name: DB_PASSWORD
          valueFrom:
            secretKeyRef:
              name: db-credentials
              key: password
        - name: REDIS_URL
          value: "redis://redis-service:6379/0"
        - name: RABBITMQ_URL
          valueFrom:
            secretKeyRef:
              name: rabbitmq-credentials
              key: url
        
        # Resource limits
        resources:
          requests:
            cpu: "500m"
            memory: "1Gi"
          limits:
            cpu: "2000m"
            memory: "4Gi"
        
        # Health checks
        livenessProbe:
          httpGet:
            path: /health
            port: health
          initialDelaySeconds: 30
          periodSeconds: 10
          timeoutSeconds: 5
          failureThreshold: 3
        
        readinessProbe:
          httpGet:
            path: /ready
            port: health
          initialDelaySeconds: 10
          periodSeconds: 5
          timeoutSeconds: 3
          failureThreshold: 3
        
        # Volume mounts
        volumeMounts:
        - name: config
          mountPath: /app/config
          readOnly: true
        - name: logs
          mountPath: /app/logs
      
      # Volumes
      volumes:
      - name: config
        configMap:
          name: game-server-config
      - name: migrations
        configMap:
          name: db-migrations
      - name: logs
        emptyDir:
          sizeLimit: 1Gi
```

### Production Overlay Configuration
```yaml
# From actual k8s/overlays/prod/deployment-patch.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: game-server
spec:
  # Production replicas
  replicas: 5
  
  # Production update strategy
  strategy:
    type: RollingUpdate
    rollingUpdate:
      maxSurge: 2
      maxUnavailable: 1
  
  template:
    spec:
      # Production node affinity
      affinity:
        podAntiAffinity:
          requiredDuringSchedulingIgnoredDuringExecution:
          - labelSelector:
              matchExpressions:
              - key: app.kubernetes.io/name
                operator: In
                values:
                - game-server
            topologyKey: kubernetes.io/hostname
      
      # Production tolerations
      tolerations:
      - key: "game-server"
        operator: "Equal"
        value: "true"
        effect: "NoSchedule"
      
      containers:
      - name: game-server
        # Production image
        image: your-registry.com/mmorpg/game-server:1.0.0
        imagePullPolicy: Always
        
        # Production resources
        resources:
          requests:
            cpu: "2"
            memory: "4Gi"
          limits:
            cpu: "4"
            memory: "8Gi"
        
        # Production environment
        env:
        - name: DEPLOYMENT_ENV
          value: "production"
        - name: LOG_LEVEL
          value: "info"
        - name: ENABLE_PROFILING
          value: "false"
        
        # Production probes
        livenessProbe:
          httpGet:
            path: /health
            port: health
          initialDelaySeconds: 60
          periodSeconds: 30
          timeoutSeconds: 10
          failureThreshold: 5
        
        readinessProbe:
          httpGet:
            path: /ready
            port: health
          initialDelaySeconds: 30
          periodSeconds: 10
          timeoutSeconds: 5
          failureThreshold: 3
```

### Service Configuration
```yaml
# From actual k8s/base/service.yaml
apiVersion: v1
kind: Service
metadata:
  name: game-server-service
  namespace: mmorpg-game
  labels:
    app.kubernetes.io/name: game-server
    app.kubernetes.io/component: backend
  annotations:
    prometheus.io/scrape: "true"
    prometheus.io/port: "9090"
spec:
  # Service type and selector
  type: ClusterIP
  selector:
    app.kubernetes.io/name: game-server
    app.kubernetes.io/component: backend
  
  # Port mappings
  ports:
  - name: game
    port: 8080
    targetPort: game
    protocol: TCP
  - name: health
    port: 8081
    targetPort: health
    protocol: TCP
  - name: metrics
    port: 9090
    targetPort: metrics
    protocol: TCP

---
# Headless service for StatefulSet
apiVersion: v1
kind: Service
metadata:
  name: game-server-headless
  namespace: mmorpg-game
  labels:
    app.kubernetes.io/name: game-server
    app.kubernetes.io/component: backend
spec:
  clusterIP: None
  selector:
    app.kubernetes.io/name: game-server
    app.kubernetes.io/component: backend
  ports:
  - name: game
    port: 8080
    targetPort: game

---
# LoadBalancer service for external access
apiVersion: v1
kind: Service
metadata:
  name: game-server-lb
  namespace: mmorpg-game
  labels:
    app.kubernetes.io/name: game-server
    app.kubernetes.io/component: backend
  annotations:
    service.beta.kubernetes.io/aws-load-balancer-type: "nlb"
    service.beta.kubernetes.io/aws-load-balancer-backend-protocol: "tcp"
spec:
  type: LoadBalancer
  selector:
    app.kubernetes.io/name: game-server
    app.kubernetes.io/component: backend
  ports:
  - name: game
    port: 8080
    targetPort: game
    protocol: TCP
```

### Kustomization for Environment Management
```yaml
# From actual k8s/overlays/prod/kustomization.yaml
apiVersion: kustomize.config.k8s.io/v1beta1
kind: Kustomization

# Base resources
bases:
  - ../../base

# Production namespace
namespace: mmorpg-prod

# Name suffix for production
nameSuffix: -prod

# Common labels
commonLabels:
  environment: production
  app.kubernetes.io/version: "1.0.0"

# Production patches
patchesStrategicMerge:
  - deployment-patch.yaml
  - service-patch.yaml

# ConfigMap generator with production config
configMapGenerator:
  - name: game-server-config
    behavior: merge
    literals:
      - LOG_LEVEL=info
      - MAX_PLAYERS=2000
      - ENABLE_METRICS=true

# Secret generator (use sealed-secrets in real production)
secretGenerator:
  - name: db-credentials
    literals:
      - username=produser
      - password=prodpass123
  - name: rabbitmq-credentials
    literals:
      - url=amqp://prod:prodpass@rabbitmq-prod:5672/

# Production replicas
replicas:
  - name: game-server
    count: 5
```

## Production Deployment Performance

### Container Orchestration Benefits
```
🚀 MMORPG Cloud Native Performance

📦 Container Benefits:
├── Image Size: 600MB (70% reduction from multi-stage)
├── Startup Time: <30 seconds (optimized build)
├── Resource Usage: 4GB RAM per zone instance
├── Health Monitoring: Built-in health checks
└── Scalability: Horizontal pod autoscaling

🌐 Multi-Zone Architecture:
├── Zone 1: Handles world area A (1,000 players)
├── Zone 2: Handles world area B (1,000 players)
├── Cross-Zone Communication: RabbitMQ messaging
├── Shared Session State: Redis cluster
└── Load Balancing: Nginx with SSL termination

☸️ Kubernetes Orchestration:
├── Base Deployment: 2 replicas for development
├── Production Overlay: 5 replicas with anti-affinity
├── Rolling Updates: Zero-downtime deployments
├── Resource Management: CPU/Memory limits enforced
├── Health Checks: Liveness and readiness probes
└── Service Discovery: DNS-based service resolution
```

### Deployment Strategies
- **Rolling Updates**: Zero-downtime deployments with health checks
- **Environment Separation**: Kustomize overlays for dev/staging/prod
- **Resource Isolation**: Node affinity and pod anti-affinity rules
- **Security**: Non-root containers with minimal privileges
- **Monitoring**: Prometheus metrics and Grafana dashboards

## Auto-Scaling Configuration

### Horizontal Pod Autoscaler (HPA)
```yaml
# HPA for game server scaling
apiVersion: autoscaling/v2
kind: HorizontalPodAutoscaler
metadata:
  name: game-server-hpa
  namespace: mmorpg-prod
spec:
  scaleTargetRef:
    apiVersion: apps/v1
    kind: Deployment
    name: game-server
  minReplicas: 3
  maxReplicas: 20
  metrics:
  - type: Resource
    resource:
      name: cpu
      target:
        type: Utilization
        averageUtilization: 70
  - type: Resource
    resource:
      name: memory
      target:
        type: Utilization
        averageUtilization: 80
  - type: Pods
    pods:
      metric:
        name: active_players_per_pod
      target:
        type: AverageValue
        averageValue: "200"
  behavior:
    scaleUp:
      stabilizationWindowSeconds: 60
      policies:
      - type: Percent
        value: 100
        periodSeconds: 60
    scaleDown:
      stabilizationWindowSeconds: 300
      policies:
      - type: Percent
        value: 50
        periodSeconds: 60
```

### Pod Disruption Budget
```yaml
# Ensure minimum availability during updates
apiVersion: policy/v1
kind: PodDisruptionBudget
metadata:
  name: game-server-pdb
  namespace: mmorpg-prod
spec:
  minAvailable: 3
  selector:
    matchLabels:
      app.kubernetes.io/name: game-server
```

## Monitoring and Observability

### Prometheus Metrics Configuration
```yaml
# ServiceMonitor for Prometheus scraping
apiVersion: monitoring.coreos.com/v1
kind: ServiceMonitor
metadata:
  name: game-server-metrics
  namespace: mmorpg-prod
spec:
  selector:
    matchLabels:
      app.kubernetes.io/name: game-server
  endpoints:
  - port: metrics
    interval: 15s
    path: /metrics
    relabelings:
    - sourceLabels: [__meta_kubernetes_pod_name]
      targetLabel: pod
    - sourceLabels: [__meta_kubernetes_pod_node_name]
      targetLabel: node
```

### Key Performance Indicators
```yaml
# Production metrics targets
performance_targets:
  availability: 99.9%                 # Monthly uptime target
  response_time_p95: 100ms           # 95th percentile response time
  concurrent_players: 5000           # Maximum concurrent players
  requests_per_second: 10000         # Peak RPS capacity
  error_rate: 0.1%                   # Maximum error rate
  deployment_frequency: daily        # CI/CD deployment rate
  recovery_time: 5min                # Mean time to recovery
  
resource_efficiency:
  cpu_utilization_target: 70%        # Target CPU usage
  memory_utilization_target: 80%     # Target memory usage
  pod_density: 10                    # Pods per node
  cost_per_player_hour: $0.02        # Cost efficiency target
```

## Security Implementation

### Pod Security Standards
```yaml
# Security context for game server pods
apiVersion: v1
kind: Pod
spec:
  securityContext:
    runAsNonRoot: true
    runAsUser: 1000
    runAsGroup: 1000
    fsGroup: 1000
    seccompProfile:
      type: RuntimeDefault
  containers:
  - name: game-server
    securityContext:
      allowPrivilegeEscalation: false
      readOnlyRootFilesystem: true
      capabilities:
        drop:
        - ALL
      runAsNonRoot: true
      runAsUser: 1000
```

### Network Policies
```yaml
# Network policy to restrict traffic
apiVersion: networking.k8s.io/v1
kind: NetworkPolicy
metadata:
  name: game-server-netpol
  namespace: mmorpg-prod
spec:
  podSelector:
    matchLabels:
      app.kubernetes.io/name: game-server
  policyTypes:
  - Ingress
  - Egress
  ingress:
  - from:
    - namespaceSelector:
        matchLabels:
          name: nginx-ingress
    ports:
    - protocol: TCP
      port: 8080
  egress:
  - to:
    - namespaceSelector:
        matchLabels:
          name: database
    ports:
    - protocol: TCP
      port: 5432
  - to:
    - namespaceSelector:
        matchLabels:
          name: redis
    ports:
    - protocol: TCP
      port: 6379
```

## CI/CD Pipeline Integration

### GitOps Deployment with ArgoCD
```yaml
# ArgoCD Application
apiVersion: argoproj.io/v1alpha1
kind: Application
metadata:
  name: mmorpg-game-server
  namespace: argocd
spec:
  project: default
  source:
    repoURL: https://github.com/mmorpg-team/k8s-manifests
    path: overlays/production
    targetRevision: main
  destination:
    server: https://kubernetes.default.svc
    namespace: mmorpg-prod
  syncPolicy:
    automated:
      prune: true
      selfHeal: true
    syncOptions:
    - CreateNamespace=true
    retry:
      limit: 5
      backoff:
        duration: 5s
        factor: 2
        maxDuration: 3m
```

### Deployment Pipeline Steps
1. **Build Stage**: Multi-stage Docker build with layer caching
2. **Security Scan**: Container vulnerability scanning with Trivy
3. **Testing**: Integration tests in staging environment
4. **Deployment**: GitOps-based deployment with ArgoCD
5. **Monitoring**: Automated health checks and rollback triggers

## Operational Excellence

### Backup and Disaster Recovery
```yaml
# Velero backup for Kubernetes resources
apiVersion: velero.io/v1
kind: Backup
metadata:
  name: mmorpg-daily-backup
spec:
  includedNamespaces:
  - mmorpg-prod
  includedResources:
  - persistentvolumes
  - persistentvolumeclaims
  - secrets
  - configmaps
  schedule: "0 2 * * *"  # Daily at 2 AM
  ttl: 720h0m0s         # 30 days retention
```

### Health Check Endpoints
- **/health**: Liveness probe endpoint
- **/ready**: Readiness probe endpoint  
- **/metrics**: Prometheus metrics endpoint
- **/version**: Version and build information

This cloud native implementation provides a production-ready MMORPG server deployment with comprehensive monitoring, security, and operational capabilities for handling thousands of concurrent players.
        diskType: pd-ssd
        preemptible: false
        labels:
          workload-type: game-server
          node-type: cpu-optimized
        taints:
          - key: "game-server"
            value: "true"
            effect: "NoSchedule"
      autoscaling:
        enabled: true
        minNodeCount: 3
        maxNodeCount: 20
      management:
        autoRepair: true
        autoUpgrade: true

  # 데이터베이스 노드 풀 (메모리 최적화)
  database-pool.yaml: |
    apiVersion: container.gke.io/v1beta1
    kind: NodePool
    metadata:
      name: database-pool
    spec:
      cluster: mmorpg-cluster
      initialNodeCount: 2
      nodeConfig:
        machineType: n2-highmem-4  # 4 vCPU, 32GB RAM
        diskSizeGb: 500
        diskType: pd-ssd
        labels:
          workload-type: database
          node-type: memory-optimized
        taints:
          - key: "database"
            value: "true"
            effect: "NoSchedule"

---
# 리소스 쿼터 설정
apiVersion: v1
kind: ResourceQuota
metadata:
  name: mmorpg-quota
  namespace: mmorpg-production
spec:
  hard:
    requests.cpu: "100"
    requests.memory: 200Gi
    limits.cpu: "200"
    limits.memory: 400Gi
    persistentvolumeclaims: "20"
    pods: "100"
    services: "20"
```

### **1.3 게임 서버 Deployment 최적화**

```yaml
# 고성능 게임 서버 Deployment
apiVersion: apps/v1
kind: Deployment
metadata:
  name: mmorpg-game-server
  namespace: mmorpg-production
  labels:
    app: game-server
    version: v1.2.3
spec:
  replicas: 5
  strategy:
    type: RollingUpdate
    rollingUpdate:
      maxSurge: 1
      maxUnavailable: 0  # 무중단 배포
  selector:
    matchLabels:
      app: game-server
  template:
    metadata:
      labels:
        app: game-server
        version: v1.2.3
      annotations:
        prometheus.io/scrape: "true"
        prometheus.io/port: "9090"
        prometheus.io/path: "/metrics"
    spec:
      # 게임 서버 전용 노드에만 스케줄링
      tolerations:
        - key: "game-server"
          operator: "Equal"
          value: "true"
          effect: "NoSchedule"
      nodeSelector:
        workload-type: game-server
      
      # 안티 어피니티 (동일 노드에 여러 게임 서버 방지)
      affinity:
        podAntiAffinity:
          preferredDuringSchedulingIgnoredDuringExecution:
            - weight: 100
              podAffinityTerm:
                labelSelector:
                  matchExpressions:
                    - key: app
                      operator: In
                      values: ["game-server"]
                topologyKey: kubernetes.io/hostname
      
      # 초기화 컨테이너 (DB 마이그레이션, 설정 확인)
      initContainers:
        - name: db-migration
          image: mmorpg/db-migrator:v1.2.3
          env:
            - name: DB_HOST
              valueFrom:
                secretKeyRef:
                  name: database-credentials
                  key: host
            - name: DB_PASSWORD
              valueFrom:
                secretKeyRef:
                  name: database-credentials
                  key: password
          command: ["./migrate.sh"]
        
        - name: config-validator
          image: mmorpg/config-validator:v1.2.3
          volumeMounts:
            - name: game-config
              mountPath: /config
          command: ["./validate-config.sh"]
      
      containers:
        - name: game-server
          image: mmorpg/game-server:v1.2.3
          imagePullPolicy: Always
          
          # 컨테이너 보안 설정
          securityContext:
            runAsNonRoot: true
            runAsUser: 1000
            readOnlyRootFilesystem: true
            allowPrivilegeEscalation: false
            capabilities:
              drop:
                - ALL
          
          # 포트 설정
          ports:
            - name: game-tcp
              containerPort: 8080
              protocol: TCP
            - name: game-udp
              containerPort: 8081
              protocol: UDP
            - name: metrics
              containerPort: 9090
              protocol: TCP
            - name: health
              containerPort: 8888
              protocol: TCP
          
          # 리소스 제한 (중요!)
          resources:
            requests:
              cpu: "2"
              memory: "4Gi"
            limits:
              cpu: "4"
              memory: "8Gi"
          
          # 환경 변수
          env:
            - name: SERVER_ID
              valueFrom:
                fieldRef:
                  fieldPath: metadata.name
            - name: NAMESPACE
              valueFrom:
                fieldRef:
                  fieldPath: metadata.namespace
            - name: NODE_NAME
              valueFrom:
                fieldRef:
                  fieldPath: spec.nodeName
            - name: REDIS_CLUSTER_ENDPOINT
              valueFrom:
                configMapKeyRef:
                  name: redis-config
                  key: cluster-endpoint
            - name: MYSQL_HOST
              valueFrom:
                secretKeyRef:
                  name: database-credentials
                  key: host
          
          # 헬스체크 설정
          livenessProbe:
            httpGet:
              path: /health/live
              port: health
            initialDelaySeconds: 30
            periodSeconds: 10
            timeoutSeconds: 5
            failureThreshold: 3
          
          readinessProbe:
            httpGet:
              path: /health/ready
              port: health
            initialDelaySeconds: 10
            periodSeconds: 5
            timeoutSeconds: 3
            failureThreshold: 2
          
          # 볼륨 마운트
          volumeMounts:
            - name: game-config
              mountPath: /app/config
              readOnly: true
            - name: logs
              mountPath: /app/logs
            - name: tmp
              mountPath: /tmp
      
      # 볼륨 정의
      volumes:
        - name: game-config
          configMap:
            name: game-server-config
        - name: logs
          emptyDir:
            sizeLimit: 1Gi
        - name: tmp
          emptyDir:
            sizeLimit: 500Mi

---
# 게임 서버 Service
apiVersion: v1
kind: Service
metadata:
  name: game-server-service
  namespace: mmorpg-production
  annotations:
    service.beta.kubernetes.io/aws-load-balancer-type: nlb
    service.beta.kubernetes.io/aws-load-balancer-cross-zone-load-balancing-enabled: "true"
spec:
  type: LoadBalancer
  sessionAffinity: ClientIP  # 클라이언트 IP 기반 세션 유지
  sessionAffinityConfig:
    clientIP:
      timeoutSeconds: 3600  # 1시간 세션 유지
  ports:
    - name: game-tcp
      port: 8080
      targetPort: 8080
      protocol: TCP
    - name: game-udp
      port: 8081
      targetPort: 8081
      protocol: UDP
  selector:
    app: game-server
```

---

## 📊 2. 자동 스케일링 (HPA, VPA, CA)

### **2.1 Horizontal Pod Autoscaler (HPA) - 트래픽 기반**

```yaml
# CPU와 메모리 기반 HPA
apiVersion: autoscaling/v2
kind: HorizontalPodAutoscaler
metadata:
  name: game-server-hpa
  namespace: mmorpg-production
spec:
  scaleTargetRef:
    apiVersion: apps/v1
    kind: Deployment
    name: mmorpg-game-server
  minReplicas: 3
  maxReplicas: 50
  metrics:
    # CPU 사용률 기반 스케일링
    - type: Resource
      resource:
        name: cpu
        target:
          type: Utilization
          averageUtilization: 70
    
    # 메모리 사용률 기반 스케일링
    - type: Resource
      resource:
        name: memory
        target:
          type: Utilization
          averageUtilization: 80
    
    # 커스텀 메트릭: 동시 접속자 수
    - type: Pods
      pods:
        metric:
          name: concurrent_players
        target:
          type: AverageValue
          averageValue: "100"  # 팟당 100명 유지
    
    # 외부 메트릭: Redis 큐 길이
    - type: External
      external:
        metric:
          name: redis_queue_length
          selector:
            matchLabels:
              queue: "matchmaking"
        target:
          type: AverageValue
          averageValue: "50"
  
  behavior:
    scaleUp:
      stabilizationWindowSeconds: 60  # 1분 대기 후 스케일 업
      policies:
        - type: Percent
          value: 100  # 최대 100% 증가 (팟 수 2배)
          periodSeconds: 60
        - type: Pods
          value: 5    # 또는 최대 5개 팟 추가
          periodSeconds: 60
      selectPolicy: Max
    
    scaleDown:
      stabilizationWindowSeconds: 300  # 5분 대기 후 스케일 다운
      policies:
        - type: Percent
          value: 50   # 최대 50% 감소
          periodSeconds: 60
        - type: Pods
          value: 2    # 또는 최대 2개 팟 제거
          periodSeconds: 60
      selectPolicy: Min

---
# 커스텀 메트릭을 위한 ServiceMonitor
apiVersion: monitoring.coreos.com/v1
kind: ServiceMonitor
metadata:
  name: game-server-metrics
  namespace: mmorpg-production
spec:
  selector:
    matchLabels:
      app: game-server
  endpoints:
    - port: metrics
      interval: 15s
      path: /metrics
```

### **2.2 Vertical Pod Autoscaler (VPA) - 리소스 최적화**

```yaml
# VPA로 자동 리소스 최적화
apiVersion: autoscaling.k8s.io/v1
kind: VerticalPodAutoscaler
metadata:
  name: game-server-vpa
  namespace: mmorpg-production
spec:
  targetRef:
    apiVersion: apps/v1
    kind: Deployment
    name: mmorpg-game-server
  updatePolicy:
    updateMode: "Auto"  # 자동으로 팟 재시작하여 리소스 조정
  resourcePolicy:
    containerPolicies:
      - containerName: game-server
        minAllowed:
          cpu: "1"
          memory: "2Gi"
        maxAllowed:
          cpu: "8"
          memory: "16Gi"
        controlledResources: ["cpu", "memory"]
        controlledValues: RequestsAndLimits

---
# Cluster Autoscaler를 위한 노드 그룹 설정
apiVersion: v1
kind: ConfigMap
metadata:
  name: cluster-autoscaler-config
  namespace: kube-system
data:
  autoscaling-config.yaml: |
    # 게임 서버 노드 그룹 자동 스케일링
    nodeGroups:
      - name: game-server-nodes
        minSize: 3
        maxSize: 100
        desiredCapacity: 5
        instanceType: c5.2xlarge
        subnets:
          - subnet-12345678
          - subnet-87654321
        tags:
          k8s.io/cluster-autoscaler/enabled: "true"
          k8s.io/cluster-autoscaler/mmorpg-cluster: "owned"
        
        # 스케일링 정책
        scaleDownUtilizationThreshold: 0.5  # 50% 미만 시 스케일 다운
        scaleDownDelayAfterAdd: 10m         # 노드 추가 후 10분 대기
        scaleDownUnneededTime: 10m          # 불필요한 노드 10분 후 제거
        maxNodeProvisionTime: 15m           # 최대 노드 프로비저닝 시간
```

### **2.3 예측 기반 스케일링 (KEDA + ML)**

```yaml
# KEDA를 이용한 고급 스케일링
apiVersion: keda.sh/v1alpha1
kind: ScaledObject
metadata:
  name: game-server-advanced-hpa
  namespace: mmorpg-production
spec:
  scaleTargetRef:
    name: mmorpg-game-server
  pollingInterval: 15
  cooldownPeriod: 300
  idleReplicaCount: 3
  minReplicaCount: 3
  maxReplicaCount: 100
  
  triggers:
    # Redis 기반 매치메이킹 큐 길이
    - type: redis
      metadata:
        address: redis-cluster.cache.amazonaws.com:6379
        listName: matchmaking_queue
        listLength: "50"
        activationListLength: "10"
    
    # Prometheus 메트릭 기반
    - type: prometheus
      metadata:
        serverAddress: http://prometheus.monitoring.svc.cluster.local:9090
        metricName: game_server_active_players_total
        threshold: "100"
        query: avg(game_server_active_players_total{job="game-server"})
    
    # 시간 기반 예측 스케일링 (피크 타임 대비)
    - type: cron
      metadata:
        timezone: Asia/Seoul
        start: "0 19 * * *"    # 저녁 7시부터
        end: "0 2 * * *"       # 새벽 2시까지
        desiredReplicas: "20"  # 피크 타임 기본 20개 팟
    
    - type: cron
      metadata:
        timezone: America/New_York
        start: "0 19 * * *"    # 미국 피크 타임
        end: "0 2 * * *"
        desiredReplicas: "15"

---
# 실시간 부하 예측을 위한 ML 파이프라인
apiVersion: batch/v1
kind: CronJob
metadata:
  name: load-prediction-job
  namespace: mmorpg-production
spec:
  schedule: "*/5 * * * *"  # 5분마다 실행
  jobTemplate:
    spec:
      template:
        spec:
          containers:
            - name: load-predictor
              image: mmorpg/load-predictor:v1.0.0
              env:
                - name: PROMETHEUS_URL
                  value: "http://prometheus.monitoring.svc.cluster.local:9090"
                - name: PREDICTION_HORIZON_MINUTES
                  value: "30"
              command:
                - python3
                - predict_load.py
              resources:
                requests:
                  cpu: "200m"
                  memory: "512Mi"
                limits:
                  cpu: "500m"
                  memory: "1Gi"
          restartPolicy: OnFailure
```

---

## 🌐 3. Istio 서비스 메시 구축

### **3.1 Istio 설치 및 기본 설정**

```yaml
# Istio 설치 설정
apiVersion: install.istio.io/v1alpha1
kind: IstioOperator
metadata:
  name: mmorpg-istio
spec:
  values:
    global:
      meshID: mmorpg-mesh
      multiCluster:
        clusterName: mmorpg-primary
      network: mmorpg-network
    pilot:
      env:
        EXTERNAL_ISTIOD: false
  components:
    pilot:
      k8s:
        resources:
          requests:
            cpu: 200m
            memory: 128Mi
        hpaSpec:
          minReplicas: 2
          maxReplicas: 5
    ingressGateways:
      - name: istio-ingressgateway
        enabled: true
        k8s:
          resources:
            requests:
              cpu: 100m
              memory: 128Mi
          hpaSpec:
            minReplicas: 3
            maxReplicas: 10
          service:
            type: LoadBalancer
            annotations:
              service.beta.kubernetes.io/aws-load-balancer-type: nlb
              service.beta.kubernetes.io/aws-load-balancer-cross-zone-load-balancing-enabled: "true"

---
# 네임스페이스에 Istio 주입 활성화
apiVersion: v1
kind: Namespace
metadata:
  name: mmorpg-production
  labels:
    istio-injection: enabled
    
---
# 게임 서버를 위한 DestinationRule
apiVersion: networking.istio.io/v1beta1
kind: DestinationRule
metadata:
  name: game-server-destination
  namespace: mmorpg-production
spec:
  host: game-server-service
  trafficPolicy:
    loadBalancer:
      simple: LEAST_CONN  # 연결 수가 적은 팟으로 라우팅
    connectionPool:
      tcp:
        maxConnections: 1000
        connectTimeout: 30s
        keepAlive:
          time: 7200s
          interval: 75s
      http:
        http1MaxPendingRequests: 100
        http2MaxRequests: 1000
        maxRequestsPerConnection: 2
        maxRetries: 3
        idleTimeout: 900s
    circuitBreaker:
      consecutiveGatewayErrors: 5
      consecutive5xxErrors: 5
      interval: 30s
      baseEjectionTime: 30s
      maxEjectionPercent: 50
      minHealthPercent: 30
  portLevelSettings:
    - port:
        number: 8080
      loadBalancer:
        simple: ROUND_ROBIN
```

### **3.2 트래픽 관리 및 A/B 테스트**

```yaml
# 게임 서버 버전별 트래픽 분할
apiVersion: networking.istio.io/v1beta1
kind: VirtualService
metadata:
  name: game-server-traffic-split
  namespace: mmorpg-production
spec:
  hosts:
    - game-server-service
  http:
    # A/B 테스트: 90% stable, 10% beta
    - match:
        - headers:
            x-beta-user:
              exact: "true"
      route:
        - destination:
            host: game-server-service
            subset: beta
          weight: 100
    
    # 지역별 라우팅
    - match:
        - headers:
            x-user-region:
              exact: "us-east"
      route:
        - destination:
            host: game-server-service
            subset: stable
          weight: 90
        - destination:
            host: game-server-service
            subset: beta
          weight: 10
      fault:
        delay:
          percentage:
            value: 0.1  # 0.1% 트래픽에 지연 주입 (테스트용)
          fixedDelay: 5s
    
    # 기본 라우팅
    - route:
        - destination:
            host: game-server-service
            subset: stable
          weight: 95
        - destination:
            host: game-server-service
            subset: beta
          weight: 5

---
# DestinationRule에서 서브셋 정의
apiVersion: networking.istio.io/v1beta1
kind: DestinationRule
metadata:
  name: game-server-subsets
  namespace: mmorpg-production
spec:
  host: game-server-service
  subsets:
    - name: stable
      labels:
        version: v1.2.3
    - name: beta
      labels:
        version: v1.3.0-beta

---
# 게이트웨이 설정 (외부 트래픽 진입점)
apiVersion: networking.istio.io/v1beta1
kind: Gateway
metadata:
  name: mmorpg-gateway
  namespace: mmorpg-production
spec:
  selector:
    istio: ingressgateway
  servers:
    # HTTPS 게임 API
    - port:
        number: 443
        name: https-api
        protocol: HTTPS
      tls:
        mode: SIMPLE
        credentialName: mmorpg-tls-secret
      hosts:
        - api.mmorpg.com
        - "*.mmorpg.com"
    
    # TCP 게임 서버 (실시간 게임 트래픽)
    - port:
        number: 8080
        name: tcp-game
        protocol: TCP
      hosts:
        - "*"
    
    # UDP 게임 서버 (실시간 위치 동기화)
    - port:
        number: 8081
        name: udp-game
        protocol: UDP
      hosts:
        - "*"

---
# VirtualService for API Gateway
apiVersion: networking.istio.io/v1beta1
kind: VirtualService
metadata:
  name: mmorpg-api-routing
  namespace: mmorpg-production
spec:
  hosts:
    - api.mmorpg.com
  gateways:
    - mmorpg-gateway
  http:
    # 인증 서비스
    - match:
        - uri:
            prefix: /auth/
      route:
        - destination:
            host: auth-service
            port:
              number: 8080
      timeout: 10s
      retries:
        attempts: 3
        perTryTimeout: 3s
    
    # 게임 API
    - match:
        - uri:
            prefix: /api/game/
      route:
        - destination:
            host: game-api-service
            port:
              number: 8080
      timeout: 30s
      
    # 채팅 서비스
    - match:
        - uri:
            prefix: /api/chat/
      route:
        - destination:
            host: chat-service
            port:
              number: 8080
      websocketUpgrade: true  # WebSocket 지원
    
    # 기본 라우팅 (정적 파일)
    - route:
        - destination:
            host: static-content-service
            port:
              number: 8080
```

### **3.3 보안 정책 및 인증**

```yaml
# 서비스 간 mTLS 설정
apiVersion: security.istio.io/v1beta1
kind: PeerAuthentication
metadata:
  name: default
  namespace: mmorpg-production
spec:
  mtls:
    mode: STRICT  # 모든 서비스 간 통신 암호화 필수

---
# 인증 정책
apiVersion: security.istio.io/v1beta1
kind: AuthorizationPolicy
metadata:
  name: game-server-authz
  namespace: mmorpg-production
spec:
  selector:
    matchLabels:
      app: game-server
  rules:
    # 인증된 사용자만 게임 서버 접근 허용
    - from:
        - source:
            principals: ["cluster.local/ns/mmorpg-production/sa/auth-service"]
        - source:
            requestPrincipals: ["cluster.local/ns/mmorpg-production/sa/api-gateway"]
      to:
        - operation:
            methods: ["GET", "POST", "PUT"]
            paths: ["/game/*"]
      when:
        - key: request.headers[authorization]
          values: ["Bearer *"]
    
    # 헬스체크 예외
    - to:
        - operation:
            methods: ["GET"]
            paths: ["/health/*"]

---
# RequestAuthentication (JWT 검증)
apiVersion: security.istio.io/v1beta1
kind: RequestAuthentication
metadata:
  name: jwt-auth
  namespace: mmorpg-production
spec:
  selector:
    matchLabels:
      app: game-server
  jwtRules:
    - issuer: "https://auth.mmorpg.com"
      jwksUri: "https://auth.mmorpg.com/.well-known/jwks.json"
      audiences:
        - "mmorpg-game-api"
      forwardOriginalToken: true
```

---

## 🚀 4. GitOps 배포 전략

### **4.1 ArgoCD 기반 GitOps 설정**

```yaml
# ArgoCD Application for 게임 서버
apiVersion: argoproj.io/v1alpha1
kind: Application
metadata:
  name: mmorpg-game-server
  namespace: argocd
  finalizers:
    - resources-finalizer.argocd.argoproj.io
spec:
  project: mmorpg-production
  source:
    repoURL: https://github.com/mmorpg-team/game-server-config
    path: environments/production
    targetRevision: main
    helm:
      valueFiles:
        - values-production.yaml
      parameters:
        - name: image.tag
          value: v1.2.3
        - name: replicaCount
          value: "5"
  destination:
    server: https://kubernetes.default.svc
    namespace: mmorpg-production
  syncPolicy:
    automated:
      prune: true
      selfHeal: true
      allowEmpty: false
    syncOptions:
      - CreateNamespace=true
      - PrunePropagationPolicy=foreground
      - PruneLast=true
    retry:
      limit: 5
      backoff:
        duration: 5s
        factor: 2
        maxDuration: 3m
  revisionHistoryLimit: 10

---
# 프로덕션 환경 Helm Values
# values-production.yaml
apiVersion: v1
kind: ConfigMap
metadata:
  name: helm-values-production
data:
  values.yaml: |
    # 이미지 설정
    image:
      repository: mmorpg/game-server
      tag: v1.2.3
      pullPolicy: Always
    
    # 레플리카 수
    replicaCount: 5
    
    # 리소스 설정
    resources:
      requests:
        cpu: 2
        memory: 4Gi
      limits:
        cpu: 4
        memory: 8Gi
    
    # 오토스케일링
    autoscaling:
      enabled: true
      minReplicas: 3
      maxReplicas: 50
      targetCPUUtilizationPercentage: 70
      targetMemoryUtilizationPercentage: 80
    
    # 서비스 설정
    service:
      type: LoadBalancer
      ports:
        tcp: 8080
        udp: 8081
    
    # 스토리지
    persistence:
      enabled: true
      storageClass: "gp3"
      size: 100Gi
    
    # 모니터링
    monitoring:
      enabled: true
      serviceMonitor:
        enabled: true
        interval: 15s
    
    # 보안
    security:
      runAsNonRoot: true
      runAsUser: 1000
      readOnlyRootFilesystem: true
```

### **4.2 멀티 환경 배포 파이프라인**

```yaml
# GitHub Actions 워크플로우
name: Deploy MMORPG Server
on:
  push:
    branches: [main, develop]
    tags: ['v*']

env:
  REGISTRY: ghcr.io
  IMAGE_NAME: mmorpg/game-server

jobs:
  # 1단계: 빌드 및 테스트
  build-and-test:
    runs-on: ubuntu-latest
    outputs:
      image-tag: ${{ steps.meta.outputs.tags }}
      image-digest: ${{ steps.build.outputs.digest }}
    steps:
      - name: Checkout
        uses: actions/checkout@v4
      
      - name: Set up Docker Buildx
        uses: docker/setup-buildx-action@v3
      
      - name: Log in to Container Registry
        uses: docker/login-action@v3
        with:
          registry: ${{ env.REGISTRY }}
          username: ${{ github.actor }}
          password: ${{ secrets.GITHUB_TOKEN }}
      
      - name: Extract metadata
        id: meta
        uses: docker/metadata-action@v5
        with:
          images: ${{ env.REGISTRY }}/${{ env.IMAGE_NAME }}
          tags: |
            type=ref,event=branch
            type=ref,event=pr
            type=semver,pattern={{version}}
            type=semver,pattern={{major}}.{{minor}}
            type=sha,prefix={{branch}}-
      
      - name: Build and push
        id: build
        uses: docker/build-push-action@v5
        with:
          context: .
          platforms: linux/amd64,linux/arm64
          push: true
          tags: ${{ steps.meta.outputs.tags }}
          labels: ${{ steps.meta.outputs.labels }}
          cache-from: type=gha
          cache-to: type=gha,mode=max
      
      - name: Run security scan
        uses: aquasecurity/trivy-action@master
        with:
          image-ref: ${{ env.REGISTRY }}/${{ env.IMAGE_NAME }}:${{ steps.meta.outputs.version }}
          format: 'sarif'
          output: 'trivy-results.sarif'
      
      - name: Upload security scan results
        uses: github/codeql-action/upload-sarif@v3
        with:
          sarif_file: 'trivy-results.sarif'

  # 2단계: 스테이징 배포
  deploy-staging:
    needs: build-and-test
    if: github.ref == 'refs/heads/develop'
    runs-on: ubuntu-latest
    environment: staging
    steps:
      - name: Deploy to Staging
        run: |
          # ArgoCD CLI를 사용한 배포
          argocd app set mmorpg-game-server-staging \
            --parameter image.tag=${{ needs.build-and-test.outputs.image-tag }}
          argocd app sync mmorpg-game-server-staging
          argocd app wait mmorpg-game-server-staging --health
      
      - name: Run integration tests
        run: |
          # 스테이징 환경 통합 테스트
          kubectl run test-client --rm -i --tty \
            --image=mmorpg/test-client:latest \
            --env="GAME_SERVER_URL=staging.mmorpg.com" \
            -- ./run-tests.sh

  # 3단계: 프로덕션 배포 (승인 필요)
  deploy-production:
    needs: [build-and-test, deploy-staging]
    if: startsWith(github.ref, 'refs/tags/v')
    runs-on: ubuntu-latest
    environment: production
    steps:
      - name: Blue-Green Deployment
        run: |
          # 현재 프로덕션 버전 확인
          CURRENT_VERSION=$(argocd app get mmorpg-game-server-prod -o json | jq -r '.status.sync.revision')
          
          # 새 버전으로 배포 (Blue-Green)
          argocd app set mmorpg-game-server-prod \
            --parameter image.tag=${{ needs.build-and-test.outputs.image-tag }}
          
          # 헬스체크 대기
          argocd app sync mmorpg-game-server-prod
          argocd app wait mmorpg-game-server-prod --health --timeout 300
          
          # 트래픽 전환 확인
          kubectl get virtualservice game-server-traffic-split -o yaml
      
      - name: Post-deployment validation
        run: |
          # 프로덕션 헬스체크
          curl -f https://api.mmorpg.com/health || exit 1
          
          # 핵심 메트릭 확인
          PLAYER_COUNT=$(curl -s "http://prometheus:9090/api/v1/query?query=sum(game_server_active_players)" | jq -r '.data.result[0].value[1]')
          echo "Current active players: $PLAYER_COUNT"
          
          # 롤백 준비
          if [ "$PLAYER_COUNT" -lt "100" ]; then
            echo "Player count too low, preparing rollback"
            echo "ROLLBACK_NEEDED=true" >> $GITHUB_ENV
          fi
      
      - name: Rollback if needed
        if: env.ROLLBACK_NEEDED == 'true'
        run: |
          argocd app rollback mmorpg-game-server-prod
          argocd app wait mmorpg-game-server-prod --health
```

---

## 🎯 5. 멀티 클러스터 관리

### **5.1 멀티 리전 클러스터 설정**

```yaml
# 멀티 클러스터 Istio 설정
apiVersion: install.istio.io/v1alpha1
kind: IstioOperator
metadata:
  name: primary-cluster
spec:
  values:
    global:
      meshID: mmorpg-mesh
      multiCluster:
        clusterName: us-east-1
      network: us-east-network
    pilot:
      env:
        EXTERNAL_ISTIOD: false
        CROSS_NETWORK_POLICY: true
  components:
    pilot:
      k8s:
        env:
          - name: PILOT_ENABLE_CROSS_CLUSTER_WORKLOAD_ENTRY
            value: true
          - name: PILOT_ENABLE_WORKLOAD_DISCOVERY_VALIDATION
            value: true

---
# 리모트 클러스터 설정 (EU West)
apiVersion: install.istio.io/v1alpha1
kind: IstioOperator
metadata:
  name: remote-cluster-eu
spec:
  values:
    global:
      meshID: mmorpg-mesh
      multiCluster:
        clusterName: eu-west-1
      network: eu-west-network
      remotePilotAddress: ${PILOT_ADDRESS}  # Primary 클러스터의 Pilot 주소
    istiodRemote:
      enabled: true

---
# 크로스 클러스터 서비스 디스커버리
apiVersion: networking.istio.io/v1alpha3
kind: ServiceEntry
metadata:
  name: game-server-eu-west
  namespace: mmorpg-production
spec:
  hosts:
    - game-server.mmorpg-production.global
  location: MESH_EXTERNAL
  ports:
    - number: 8080
      name: tcp-game
      protocol: TCP
  resolution: DNS
  addresses:
    - 240.0.0.1  # Virtual IP for cross-cluster
  endpoints:
    - address: game-server.eu-west-1.local
      ports:
        tcp-game: 8080

---
# 지리적 트래픽 라우팅
apiVersion: networking.istio.io/v1beta1
kind: VirtualService
metadata:
  name: geo-routing
  namespace: mmorpg-production
spec:
  hosts:
    - game-server.mmorpg-production.global
  http:
    # EU 사용자는 EU 클러스터로
    - match:
        - headers:
            x-user-region:
              exact: "eu"
      route:
        - destination:
            host: game-server.mmorpg-production.global
            subset: eu-west
          weight: 100
      fault:
        abort:
          percentage:
            value: 0.01  # 0.01% 요청 실패 시뮬레이션
          httpStatus: 503
    
    # 아시아 사용자는 아시아 클러스터로
    - match:
        - headers:
            x-user-region:
              exact: "asia"
      route:
        - destination:
            host: game-server.mmorpg-production.global
            subset: asia-pacific
          weight: 100
    
    # 기본: US 클러스터
    - route:
        - destination:
            host: game-server.mmorpg-production.global
            subset: us-east
          weight: 100

---
# 글로벌 로드 밸런서 (DNS 기반)
apiVersion: v1
kind: ConfigMap
metadata:
  name: global-dns-config
data:
  # AWS Route 53 설정
  route53-config.yaml: |
    hosted_zone: mmorpg.com
    records:
      - name: api.mmorpg.com
        type: A
        geolocation_routing:
          - continent: NA
            values: ["34.234.123.45"]  # US East Load Balancer
          - continent: EU  
            values: ["52.213.45.67"]   # EU West Load Balancer
          - continent: AS
            values: ["13.229.78.90"]   # Asia Pacific Load Balancer
        health_checks:
          - protocol: HTTPS
            path: /health
            interval: 30
            failure_threshold: 3
```

### **5.2 Cross-Cluster 모니터링**

```yaml
# Prometheus Federation 설정
apiVersion: v1
kind: ConfigMap
metadata:
  name: prometheus-global-config
  namespace: monitoring
data:
  prometheus.yml: |
    global:
      scrape_interval: 15s
      evaluation_interval: 15s
      
    # 글로벌 Prometheus Federation
    scrape_configs:
      # US East 클러스터
      - job_name: 'federate-us-east'
        scrape_interval: 15s
        honor_labels: true
        metrics_path: '/federate'
        params:
          'match[]':
            - '{job=~"game-server.*"}'
            - '{job=~"mysql.*"}'
            - '{job=~"redis.*"}'
        static_configs:
          - targets:
              - 'prometheus.us-east-1.mmorpg.com:9090'
        relabel_configs:
          - source_labels: [__address__]
            target_label: cluster
            replacement: us-east-1
      
      # EU West 클러스터
      - job_name: 'federate-eu-west'
        scrape_interval: 15s
        honor_labels: true
        metrics_path: '/federate'
        params:
          'match[]':
            - '{job=~"game-server.*"}'
            - '{job=~"mysql.*"}'
            - '{job=~"redis.*"}'
        static_configs:
          - targets:
              - 'prometheus.eu-west-1.mmorpg.com:9090'
        relabel_configs:
          - source_labels: [__address__]
            target_label: cluster
            replacement: eu-west-1
      
      # Asia Pacific 클러스터
      - job_name: 'federate-asia-pacific'
        scrape_interval: 15s
        honor_labels: true
        metrics_path: '/federate'
        params:
          'match[]':
            - '{job=~"game-server.*"}'
            - '{job=~"mysql.*"}'
            - '{job=~"redis.*"}'
        static_configs:
          - targets:
              - 'prometheus.asia-pacific-1.mmorpg.com:9090'
        relabel_configs:
          - source_labels: [__address__]
            target_label: cluster
            replacement: asia-pacific-1

---
# 글로벌 Grafana 대시보드
apiVersion: v1
kind: ConfigMap
metadata:
  name: global-dashboard
  namespace: monitoring
data:
  global-overview.json: |
    {
      "dashboard": {
        "id": null,
        "title": "MMORPG Global Cluster Overview",
        "tags": ["mmorpg", "global", "production"],
        "style": "dark",
        "timezone": "UTC",
        "panels": [
          {
            "id": 1,
            "title": "Global Active Players",
            "type": "stat",
            "targets": [
              {
                "expr": "sum(game_server_active_players_total) by (cluster)",
                "legendFormat": "{{cluster}}"
              }
            ],
            "fieldConfig": {
              "defaults": {
                "color": {
                  "mode": "palette-classic"
                },
                "mappings": [],
                "thresholds": {
                  "steps": [
                    {"color": "green", "value": null},
                    {"color": "red", "value": 10000}
                  ]
                }
              }
            }
          },
          {
            "id": 2,
            "title": "Cross-Cluster Latency",
            "type": "timeseries",
            "targets": [
              {
                "expr": "histogram_quantile(0.95, sum(rate(istio_request_duration_milliseconds_bucket{source_cluster!=destination_cluster}[5m])) by (le, source_cluster, destination_cluster))",
                "legendFormat": "{{source_cluster}} -> {{destination_cluster}}"
              }
            ]
          },
          {
            "id": 3,
            "title": "Cluster Health Status",
            "type": "table",
            "targets": [
              {
                "expr": "up{job=\"kubernetes-apiservers\"} by (cluster)",
                "format": "table"
              }
            ]
          }
        ],
        "time": {
          "from": "now-1h",
          "to": "now"
        },
        "refresh": "30s"
      }
    }
```

---

## 💰 6. 클라우드 비용 최적화

### **6.1 인텔리전트 리소스 스케줄링**

```yaml
# 비용 최적화를 위한 스케줄링 정책
apiVersion: v1
kind: ConfigMap
metadata:
  name: cost-optimization-config
  namespace: mmorpg-production
data:
  scheduler-config.yaml: |
    # 스팟 인스턴스 우선 사용
    nodeAffinity:
      preferredDuringSchedulingIgnoredDuringExecution:
        - weight: 100
          preference:
            matchExpressions:
              - key: node.kubernetes.io/instance-lifecycle
                operator: In
                values: ["spot"]
        - weight: 50
          preference:
            matchExpressions:
              - key: node.kubernetes.io/instance-type
                operator: In
                values: ["c5.large", "c5.xlarge"]  # 비용 효율적인 인스턴스

---
# 비용 효율적인 HPA 설정
apiVersion: autoscaling/v2
kind: HorizontalPodAutoscaler
metadata:
  name: cost-optimized-game-server-hpa
  namespace: mmorpg-production
spec:
  scaleTargetRef:
    apiVersion: apps/v1
    kind: Deployment
    name: mmorpg-game-server
  minReplicas: 2  # 최소 리플리카 감소
  maxReplicas: 30  # 최대 리플리카 제한
  metrics:
    - type: Resource
      resource:
        name: cpu
        target:
          type: Utilization
          averageUtilization: 80  # CPU 사용률 높여서 효율성 증대
    - type: Pods
      pods:
        metric:
          name: concurrent_players
        target:
          type: AverageValue
          averageValue: "150"  # 팟당 플레이어 수 증가
  behavior:
    scaleUp:
      stabilizationWindowSeconds: 300  # 스케일 업 지연으로 불필요한 리소스 방지
      policies:
        - type: Percent
          value: 50  # 점진적 스케일 업
          periodSeconds: 60
    scaleDown:
      stabilizationWindowSeconds: 60   # 빠른 스케일 다운으로 비용 절약
      policies:
        - type: Percent
          value: 25
          periodSeconds: 30

---
# 스팟 인스턴스 처리를 위한 PodDisruptionBudget
apiVersion: policy/v1
kind: PodDisruptionBudget
metadata:
  name: game-server-pdb
  namespace: mmorpg-production
spec:
  minAvailable: 2  # 최소 2개 팟 유지
  selector:
    matchLabels:
      app: game-server
```

### **6.2 자동 비용 모니터링 및 알림**

```yaml
# 비용 모니터링을 위한 CronJob
apiVersion: batch/v1
kind: CronJob
metadata:
  name: cost-monitoring
  namespace: mmorpg-production
spec:
  schedule: "0 */6 * * *"  # 6시간마다
  jobTemplate:
    spec:
      template:
        spec:
          containers:
            - name: cost-monitor
              image: mmorpg/cost-monitor:v1.0.0
              env:
                - name: AWS_REGION
                  value: "us-east-1"
                - name: PROMETHEUS_URL
                  value: "http://prometheus.monitoring.svc.cluster.local:9090"
                - name: SLACK_WEBHOOK_URL
                  valueFrom:
                    secretKeyRef:
                      name: slack-webhook
                      key: url
              command:
                - python3
                - cost_monitor.py
              resources:
                requests:
                  cpu: "100m"
                  memory: "128Mi"
                limits:
                  cpu: "200m"
                  memory: "256Mi"
          restartPolicy: OnFailure

---
# 비용 효율성 메트릭 수집
apiVersion: v1
kind: ConfigMap
metadata:
  name: cost-efficiency-queries
  namespace: monitoring
data:
  cost-queries.yaml: |
    # 시간당 인프라 비용 (추정)
    cost_per_hour:
      query: |
        sum(
          node_cost_hourly * on(instance) group_left() 
          count by (instance) (kube_node_info)
        )
    
    # 플레이어당 비용
    cost_per_player:
      query: |
        (
          sum(node_cost_hourly * on(instance) group_left() count by (instance) (kube_node_info))
          /
          sum(game_server_active_players_total)
        )
    
    # 리소스 사용률 대비 비용 효율성
    cost_efficiency:
      query: |
        (
          (sum(rate(container_cpu_usage_seconds_total[5m])) / sum(kube_node_status_allocatable{resource="cpu"})) +
          (sum(container_memory_usage_bytes) / sum(kube_node_status_allocatable{resource="memory"}))
        ) / 2
    
    # 미사용 리소스 비용
    wasted_cost:
      query: |
        sum(
          (kube_node_status_allocatable{resource="cpu"} - kube_node_status_capacity{resource="cpu"}) *
          node_cost_hourly
        )

---
# PrometheusRule for 비용 알림
apiVersion: monitoring.coreos.com/v1
kind: PrometheusRule
metadata:
  name: cost-optimization-alerts
  namespace: monitoring
spec:
  groups:
    - name: cost.rules
      rules:
        # 시간당 비용이 예산 초과
        - alert: HighInfrastructureCost
          expr: cost_per_hour > 100  # $100/hour 초과
          for: 30m
          labels:
            severity: warning
            team: infrastructure
          annotations:
            summary: "Infrastructure cost is too high"
            description: "Current hourly cost is ${{ $value }}, exceeding budget of $100/hour"
        
        # 플레이어당 비용이 비효율적
        - alert: HighCostPerPlayer
          expr: cost_per_player > 0.02  # $0.02/player/hour 초과
          for: 15m
          labels:
            severity: warning
            team: gameops
          annotations:
            summary: "Cost per player is inefficient"
            description: "Current cost per player is ${{ $value }}/hour, target is $0.02/hour"
        
        # 리소스 사용률이 낮음
        - alert: LowResourceUtilization
          expr: cost_efficiency < 0.3  # 30% 미만 사용률
          for: 1h
          labels:
            severity: info
            team: infrastructure
          annotations:
            summary: "Low resource utilization detected"
            description: "Resource utilization is {{ $value | humanizePercentage }}, consider scaling down"
        
        # 미사용 리소스 비용이 높음
        - alert: HighWastedCost
          expr: wasted_cost > 20  # $20/hour 이상 낭비
          for: 30m
          labels:
            severity: warning
            team: infrastructure
          annotations:
            summary: "High wasted resource cost"
            description: "Wasted cost is ${{ $value }}/hour due to unused resources"
```

### **6.3 Karpenter 기반 스마트 스케일링**

```yaml
# Karpenter NodePool (AWS EKS용)
apiVersion: karpenter.sh/v1beta1
kind: NodePool
metadata:
  name: game-server-nodepool
spec:
  # 노드 클래스 참조
  nodeClassRef:
    apiVersion: karpenter.k8s.aws/v1beta1
    kind: EC2NodeClass
    name: game-server-nodeclass
  
  # 스케일링 정책
  disruption:
    consolidationPolicy: WhenUntilized
    consolidateAfter: 30s
    expireAfter: 30m  # 30분 후 노드 종료
  
  # 리소스 제한
  limits:
    cpu: 1000
    memory: 1000Gi
  
  # 노드 템플릿
  template:
    metadata:
      labels:
        workload-type: game-server
        cost-optimization: enabled
    spec:
      # 다양한 인스턴스 타입 허용 (비용 최적화)
      nodeClassRef:
        apiVersion: karpenter.k8s.aws/v1beta1
        kind: EC2NodeClass
        name: game-server-nodeclass
      
      # 스팟 인스턴스 우선 사용
      requirements:
        - key: karpenter.sh/capacity-type
          operator: In
          values: ["spot", "on-demand"]
        - key: node.kubernetes.io/instance-type
          operator: In
          values: ["c5.large", "c5.xlarge", "c5.2xlarge", "m5.large", "m5.xlarge"]
        - key: kubernetes.io/arch
          operator: In
          values: ["amd64"]
      
      # 테인트 설정
      taints:
        - key: "game-server"
          value: "true"
          effect: "NoSchedule"

---
# EC2NodeClass 설정
apiVersion: karpenter.k8s.aws/v1beta1
kind: EC2NodeClass
metadata:
  name: game-server-nodeclass
spec:
  # AMI 자동 선택
  amiFamily: AL2
  
  # 서브넷 선택 (AZ 분산)
  subnetSelectorTerms:
    - tags:
        karpenter.sh/discovery: "mmorpg-cluster"
        kubernetes.io/cluster/mmorpg-cluster: "owned"
  
  # 보안 그룹
  securityGroupSelectorTerms:
    - tags:
        karpenter.sh/discovery: "mmorpg-cluster"
  
  # 인스턴스 프로파일
  role: "KarpenterInstanceProfile"
  
  # 사용자 데이터 (노드 최적화)
  userData: |
    #!/bin/bash
    /etc/eks/bootstrap.sh mmorpg-cluster
    
    # 게임 서버 최적화 설정
    echo 'net.core.rmem_default = 262144' >> /etc/sysctl.conf
    echo 'net.core.rmem_max = 16777216' >> /etc/sysctl.conf
    echo 'net.core.wmem_default = 262144' >> /etc/sysctl.conf
    echo 'net.core.wmem_max = 16777216' >> /etc/sysctl.conf
    sysctl -p
    
    # EBS 최적화
    echo 'deadline' > /sys/block/nvme0n1/queue/scheduler
  
  # 블록 디바이스 매핑
  blockDeviceMappings:
    - deviceName: /dev/xvda
      ebs:
        volumeSize: 100Gi
        volumeType: gp3
        iops: 3000
        deleteOnTermination: true
        encrypted: true
  
  # 태그
  tags:
    Environment: production
    Application: mmorpg
    CostCenter: gaming
    ManagedBy: karpenter
```

---

## 📈 7. 성능 측정 및 벤치마킹

### **7.1 실전 부하 테스트 시나리오**

```python
# 부하 테스트 스크립트 (Python + K6)
import subprocess
import json
import time
from kubernetes import client, config

class GameServerLoadTest:
    def __init__(self):
        config.load_incluster_config()  # 클러스터 내에서 실행
        self.v1 = client.CoreV1Api()
        self.apps_v1 = client.AppsV1Api()
        
    def run_comprehensive_load_test(self):
        """포괄적인 부하 테스트 실행"""
        
        # 1. 기준 성능 측정
        baseline_metrics = self.measure_baseline()
        
        # 2. 점진적 부하 증가 테스트
        self.ramp_up_test()
        
        # 3. 스파이크 테스트 (급격한 트래픽 증가)
        self.spike_test()
        
        # 4. 내구성 테스트 (장시간 유지)
        self.endurance_test()
        
        # 5. 장애 복구 테스트
        self.chaos_engineering_test()
        
        # 6. 결과 리포트 생성
        self.generate_performance_report(baseline_metrics)
    
    def ramp_up_test(self):
        """점진적 부하 증가 테스트"""
        print("🚀 점진적 부하 증가 테스트 시작")
        
        # K6 스크립트 실행
        k6_script = """
        import http from 'k6/http';
        import ws from 'k6/ws';
        import { check } from 'k6';
        
        export let options = {
          stages: [
            { duration: '2m', target: 100 },   // 100명으로 증가
            { duration: '5m', target: 100 },   // 100명 유지
            { duration: '2m', target: 500 },   // 500명으로 증가
            { duration: '5m', target: 500 },   // 500명 유지
            { duration: '2m', target: 1000 },  // 1000명으로 증가
            { duration: '10m', target: 1000 }, // 1000명 유지
            { duration: '5m', target: 0 },     // 종료
          ],
          thresholds: {
            http_req_duration: ['p(95)<100'],   // 95% 요청이 100ms 이하
            http_req_failed: ['rate<0.1'],      // 실패율 10% 이하
            ws_connecting: ['p(95)<1000'],       // WebSocket 연결 1초 이하
          },
        };
        
        export default function() {
          // 1. HTTP API 테스트 (로그인, 게임 상태)
          let loginResponse = http.post('https://api.mmorpg.com/auth/login', {
            username: `player_${__VU}`,
            password: 'test123'
          });
          
          check(loginResponse, {
            'login successful': (r) => r.status === 200,
            'login response time < 500ms': (r) => r.timings.duration < 500,
          });
          
          let token = loginResponse.json('token');
          
          // 2. WebSocket 연결 (실시간 게임)
          let wsUrl = 'wss://game.mmorpg.com/ws';
          let response = ws.connect(wsUrl, {
            headers: { Authorization: `Bearer ${token}` }
          }, function(socket) {
            
            // 게임 서버에 입장
            socket.send(JSON.stringify({
              type: 'join_game',
              map_id: 1,
              character_id: __VU
            }));
            
            // 주기적으로 위치 업데이트
            socket.setInterval(function() {
              socket.send(JSON.stringify({
                type: 'move',
                x: Math.random() * 1000,
                y: Math.random() * 1000,
                timestamp: Date.now()
              }));
            }, 100);  // 100ms마다 위치 업데이트
            
            // 채팅 메시지 전송
            socket.setInterval(function() {
              socket.send(JSON.stringify({
                type: 'chat',
                message: `Hello from player ${__VU}`,
                channel: 'global'
              }));
            }, 5000);  // 5초마다 채팅
            
            // 전투 액션 시뮬레이션
            socket.setInterval(function() {
              socket.send(JSON.stringify({
                type: 'attack',
                target_id: Math.floor(Math.random() * 1000) + 1,
                skill_id: Math.floor(Math.random() * 10) + 1
              }));
            }, 2000);  // 2초마다 공격
            
          });
          
          check(response, {
            'websocket connection successful': (r) => r && r.status === 101,
          });
        }
        """
        
        # K6 실행
        with open('/tmp/ramp_test.js', 'w') as f:
            f.write(k6_script)
        
        subprocess.run(['k6', 'run', '/tmp/ramp_test.js'], check=True)
    
    def spike_test(self):
        """스파이크 테스트 (급격한 트래픽 증가)"""
        print("⚡ 스파이크 테스트 시작")
        
        k6_script = """
        export let options = {
          stages: [
            { duration: '1m', target: 100 },    // 정상 상태
            { duration: '30s', target: 2000 },  // 급격히 2000명으로 증가
            { duration: '2m', target: 2000 },   // 2000명 유지
            { duration: '30s', target: 100 },   // 다시 정상 상태로
            { duration: '2m', target: 100 },    // 정상 상태 유지
          ],
          thresholds: {
            http_req_duration: ['p(95)<500'],   // 스파이크 시에는 500ms 허용
            http_req_failed: ['rate<0.2'],      // 실패율 20% 이하
          },
        };
        
        // 동일한 게임 로직...
        """
        
        with open('/tmp/spike_test.js', 'w') as f:
            f.write(k6_script)
        
        subprocess.run(['k6', 'run', '/tmp/spike_test.js'], check=True)
    
    def chaos_engineering_test(self):
        """카오스 엔지니어링 테스트"""
        print("🔥 카오스 엔지니어링 테스트 시작")
        
        # 1. 팟 무작위 종료
        self.chaos_kill_random_pods()
        
        # 2. 네트워크 지연 주입
        self.chaos_inject_network_delay()
        
        # 3. CPU 부하 주입
        self.chaos_inject_cpu_load()
        
        # 4. 메모리 부하 주입
        self.chaos_inject_memory_load()
        
        # 복구 시간 측정
        self.measure_recovery_time()
    
    def chaos_kill_random_pods(self):
        """무작위로 팟 종료"""
        pods = self.v1.list_namespaced_pod(
            namespace="mmorpg-production",
            label_selector="app=game-server"
        )
        
        import random
        if pods.items:
            target_pod = random.choice(pods.items)
            print(f"🎯 팟 종료: {target_pod.metadata.name}")
            
            self.v1.delete_namespaced_pod(
                name=target_pod.metadata.name,
                namespace="mmorpg-production"
            )
            
            # 복구 대기 및 측정
            self.wait_for_pod_recovery()
    
    def measure_recovery_time(self):
        """복구 시간 측정"""
        start_time = time.time()
        
        while True:
            # 헬스체크 확인
            try:
                response = subprocess.run([
                    'curl', '-f', 'https://api.mmorpg.com/health'
                ], capture_output=True, timeout=5)
                
                if response.returncode == 0:
                    recovery_time = time.time() - start_time
                    print(f"✅ 서비스 복구 완료: {recovery_time:.2f}초")
                    break
            except subprocess.TimeoutExpired:
                pass
            
            time.sleep(1)
            
            # 최대 5분 대기
            if time.time() - start_time > 300:
                print("❌ 복구 실패: 5분 초과")
                break
    
    def generate_performance_report(self, baseline_metrics):
        """성능 리포트 생성"""
        report = {
            "test_timestamp": time.time(),
            "baseline_metrics": baseline_metrics,
            "load_test_results": self.collect_load_test_metrics(),
            "recommendations": self.generate_recommendations()
        }
        
        with open('/tmp/performance_report.json', 'w') as f:
            json.dump(report, f, indent=2)
        
        print("📊 성능 리포트 생성 완료: /tmp/performance_report.json")

# 부하 테스트 실행
if __name__ == "__main__":
    tester = GameServerLoadTest()
    tester.run_comprehensive_load_test()
```

### **7.2 실시간 성능 대시보드**

```yaml
# Grafana 대시보드 설정
apiVersion: v1
kind: ConfigMap
metadata:
  name: game-server-dashboard
  namespace: monitoring
data:
  game-server-performance.json: |
    {
      "dashboard": {
        "id": null,
        "title": "Game Server Performance Dashboard",
        "tags": ["mmorpg", "game-server", "performance"],
        "style": "dark",
        "timezone": "UTC",
        "panels": [
          {
            "id": 1,
            "title": "Active Players",
            "type": "stat",
            "targets": [
              {
                "expr": "sum(game_server_active_players_total)",
                "legendFormat": "Total Players"
              }
            ],
            "fieldConfig": {
              "defaults": {
                "color": {
                  "mode": "palette-classic"
                },
                "thresholds": {
                  "steps": [
                    {"color": "green", "value": null},
                    {"color": "yellow", "value": 3000},
                    {"color": "red", "value": 4500}
                  ]
                }
              }
            }
          },
          {
            "id": 2,
            "title": "Response Time P95",
            "type": "timeseries",
            "targets": [
              {
                "expr": "histogram_quantile(0.95, sum(rate(http_request_duration_seconds_bucket{job=\"game-server\"}[5m])) by (le))",
                "legendFormat": "P95 Response Time"
              },
              {
                "expr": "histogram_quantile(0.99, sum(rate(http_request_duration_seconds_bucket{job=\"game-server\"}[5m])) by (le))",
                "legendFormat": "P99 Response Time"
              }
            ],
            "fieldConfig": {
              "defaults": {
                "unit": "s",
                "min": 0,
                "thresholds": {
                  "steps": [
                    {"color": "green", "value": null},
                    {"color": "yellow", "value": 0.1},
                    {"color": "red", "value": 0.5}
                  ]
                }
              }
            }
          },
          {
            "id": 3,
            "title": "Throughput (Requests/sec)",
            "type": "timeseries",
            "targets": [
              {
                "expr": "sum(rate(http_requests_total{job=\"game-server\"}[5m]))",
                "legendFormat": "Total RPS"
              },
              {
                "expr": "sum(rate(http_requests_total{job=\"game-server\", code=~\"2..\"}[5m]))",
                "legendFormat": "Successful RPS"
              },
              {
                "expr": "sum(rate(http_requests_total{job=\"game-server\", code=~\"5..\"}[5m]))",
                "legendFormat": "Error RPS"
              }
            ]
          },
          {
            "id": 4,
            "title": "Error Rate",
            "type": "timeseries",
            "targets": [
              {
                "expr": "sum(rate(http_requests_total{job=\"game-server\", code=~\"5..\"}[5m])) / sum(rate(http_requests_total{job=\"game-server\"}[5m]))",
                "legendFormat": "Error Rate"
              }
            ],
            "fieldConfig": {
              "defaults": {
                "unit": "percentunit",
                "min": 0,
                "max": 1,
                "thresholds": {
                  "steps": [
                    {"color": "green", "value": null},
                    {"color": "yellow", "value": 0.01},
                    {"color": "red", "value": 0.05}
                  ]
                }
              }
            }
          },
          {
            "id": 5,
            "title": "Resource Utilization",
            "type": "timeseries",
            "targets": [
              {
                "expr": "avg(rate(container_cpu_usage_seconds_total{pod=~\"mmorpg-game-server.*\"}[5m]))",
                "legendFormat": "CPU Usage"
              },
              {
                "expr": "avg(container_memory_usage_bytes{pod=~\"mmorpg-game-server.*\"}) / avg(container_spec_memory_limit_bytes{pod=~\"mmorpg-game-server.*\"})",
                "legendFormat": "Memory Usage"
              }
            ],
            "fieldConfig": {
              "defaults": {
                "unit": "percentunit",
                "min": 0,
                "max": 1
              }
            }
          },
          {
            "id": 6,
            "title": "Pod Scaling Events",
            "type": "timeseries",
            "targets": [
              {
                "expr": "kube_deployment_status_replicas{deployment=\"mmorpg-game-server\"}",
                "legendFormat": "Current Replicas"
              },
              {
                "expr": "kube_deployment_spec_replicas{deployment=\"mmorpg-game-server\"}",
                "legendFormat": "Desired Replicas"
              }
            ]
          },
          {
            "id": 7,
            "title": "Network Metrics",
            "type": "timeseries",
            "targets": [
              {
                "expr": "sum(rate(container_network_receive_bytes_total{pod=~\"mmorpg-game-server.*\"}[5m]))",
                "legendFormat": "Network In"
              },
              {
                "expr": "sum(rate(container_network_transmit_bytes_total{pod=~\"mmorpg-game-server.*\"}[5m]))",
                "legendFormat": "Network Out"
              }
            ],
            "fieldConfig": {
              "defaults": {
                "unit": "binBps"
              }
            }
          },
          {
            "id": 8,
            "title": "Database Performance",
            "type": "timeseries",
            "targets": [
              {
                "expr": "mysql_global_status_queries",
                "legendFormat": "MySQL Queries/sec"
              },
              {
                "expr": "redis_commands_processed_total",
                "legendFormat": "Redis Commands/sec"
              }
            ]
          }
        ],
        "time": {
          "from": "now-1h",
          "to": "now"
        },
        "refresh": "10s"
      }
    }
```

---

## 🎯 8. 실전 검증 체크리스트

### **8.1 Production Readiness 체크리스트**

```yaml
# Production 배포 전 검증 체크리스트
apiVersion: v1
kind: ConfigMap
metadata:
  name: production-readiness-checklist
data:
  checklist.yaml: |
    # 🔒 보안 검증
    security:
      - ✅ mTLS 설정 완료
      - ✅ RBAC 권한 최소화
      - ✅ Network Policies 적용
      - ✅ Pod Security Standards 준수
      - ✅ 시크릿 암호화 (etcd encryption)
      - ✅ 이미지 취약점 스캔 통과
      - ✅ 런타임 보안 모니터링
    
    # 📊 모니터링 & 관찰성
    observability:
      - ✅ Prometheus 메트릭 수집
      - ✅ Grafana 대시보드 구성
      - ✅ 알람 룰 설정
      - ✅ 로그 수집 (ELK/Loki)
      - ✅ 분산 추적 (Jaeger)
      - ✅ 에러 추적 (Sentry)
      - ✅ 성능 APM 연동
    
    # 🚀 성능 & 확장성
    performance:
      - ✅ 부하 테스트 통과 (5000+ 동시 접속)
      - ✅ 자동 스케일링 검증
      - ✅ 리소스 제한 설정
      - ✅ 헬스체크 구성
      - ✅ 레디니스 프로브 구성
      - ✅ PDB 설정
      - ✅ 노드 친화성 설정
    
    # 💾 데이터 & 백업
    data_management:
      - ✅ 데이터베이스 백업 자동화
      - ✅ 스토리지 클래스 설정
      - ✅ PV 백업 정책
      - ✅ 재해 복구 테스트
      - ✅ 데이터 암호화
      - ✅ 트랜잭션 로그 백업
    
    # 🌐 네트워킹
    networking:
      - ✅ Ingress/Gateway 설정
      - ✅ 로드 밸런서 구성
      - ✅ DNS 설정
      - ✅ SSL/TLS 인증서
      - ✅ CDN 연동
      - ✅ 지리적 라우팅
      - ✅ DDoS 방어
    
    # 🔄 CI/CD & GitOps
    deployment:
      - ✅ GitOps 파이프라인
      - ✅ 무중단 배포 전략
      - ✅ 카나리 배포 설정
      - ✅ 롤백 절차
      - ✅ 환경별 설정 분리
      - ✅ 시크릿 관리
      - ✅ 이미지 스캔 통과
    
    # 🆘 운영 & 장애 대응
    operations:
      - ✅ 런북 문서화
      - ✅ 알람 에스컬레이션
      - ✅ 장애 대응 프로세스
      - ✅ 로그 보존 정책
      - ✅ 용량 계획
      - ✅ 비용 모니터링
      - ✅ SLA 정의
```

### **8.2 성능 벤치마크 결과**

```yaml
# 실제 달성해야 하는 성능 지표
apiVersion: v1
kind: ConfigMap
metadata:
  name: performance-benchmarks
data:
  targets.yaml: |
    # 🎯 핵심 성능 지표
    primary_metrics:
      concurrent_users: 5000      # 동시 접속자
      response_time_p95: 100      # 95% 응답시간 (ms)
      response_time_p99: 500      # 99% 응답시간 (ms)
      throughput_rps: 10000       # 초당 요청 수
      error_rate: 0.1             # 에러율 (%)
      availability: 99.99         # 가용성 (%)
    
    # 📊 리소스 사용률
    resource_utilization:
      cpu_usage_avg: 70           # 평균 CPU 사용률 (%)
      memory_usage_avg: 80        # 평균 메모리 사용률 (%)
      network_bandwidth: 1000     # 네트워크 대역폭 (Mbps)
      storage_iops: 3000          # 스토리지 IOPS
    
    # ⚡ 자동 스케일링 성능
    autoscaling_performance:
      scale_up_time: 60           # 스케일 업 시간 (초)
      scale_down_time: 300        # 스케일 다운 시간 (초)
      pod_startup_time: 30        # 팟 시작 시간 (초)
      node_provision_time: 180    # 노드 프로비저닝 시간 (초)
    
    # 🔄 복구 성능
    recovery_metrics:
      mtbf_hours: 720             # 평균 장애 간격 (시간)
      mttr_minutes: 5             # 평균 복구 시간 (분)
      rpo_minutes: 15             # 복구 시점 목표 (분)
      rto_minutes: 30             # 복구 시간 목표 (분)

  results.yaml: |
    # ✅ 실제 달성 결과 (예시)
    achieved_results:
      test_date: "2024-01-15"
      test_duration: "2h"
      
      primary_metrics:
        concurrent_users: 5247     # ✅ 목표 초과 달성
        response_time_p95: 87      # ✅ 목표 달성
        response_time_p99: 342     # ✅ 목표 달성
        throughput_rps: 12500      # ✅ 목표 초과 달성
        error_rate: 0.05           # ✅ 목표 달성
        availability: 99.995       # ✅ 목표 달성
      
      resource_efficiency:
        cost_per_user_hour: 0.015  # $0.015/user/hour
        cpu_efficiency: 85         # 85% 효율적 사용
        memory_efficiency: 78      # 78% 효율적 사용
        
      scaling_validation:
        max_scale_test: 8000       # 8000명까지 확장 검증
        spike_recovery: 45         # 45초 내 스파이크 대응
        failover_time: 23          # 23초 내 장애 복구
```

---

## 🏆 마무리: 엔터프라이즈급 게임 서버 완성

**🎉 축하합니다!** 이제 여러분은 **엔터프라이즈급 클라우드 네이티브 게임 서버 아키텍처**를 완전히 이해했습니다!

### **달성한 역량들:**
- ✅ **Kubernetes 마스터**: 5,000명+ 동시 접속 처리하는 프로덕션 클러스터 운영
- ✅ **Istio 서비스 메시**: 마이크로서비스 간 보안, 트래픽 관리, 관찰성 구현
- ✅ **자동 스케일링**: HPA/VPA/CA로 트래픽 변화에 실시간 대응
- ✅ **GitOps 배포**: 무중단 배포와 즉시 롤백이 가능한 CI/CD 파이프라인
- ✅ **멀티 클러스터**: 글로벌 5개 리전에서 통합 운영
- ✅ **비용 최적화**: 인프라 비용 50% 절감하는 스마트 리소스 관리

### **실제 성과:**
- **가용성**: 99.99% (월 26초 다운타임)
- **성능**: P95 응답시간 100ms 이하, 초당 10,000+ 요청 처리
- **확장성**: 1분 내 자동 스케일링으로 트래픽 급증 대응
- **비용 효율성**: 플레이어당 시간당 $0.015 운영 비용

### **이제 할 수 있는 것들:**
- 🎮 **AAA급 게임 서버 아키텍트** 역할 수행
- 🏢 **엔터프라이즈 클라우드 컨설팅** 프로젝트 리딩
- 💼 **CTO/기술임원** 수준의 기술적 의사결정
- 🌍 **글로벌 서비스** 설계 및 운영

### **다음 도전:**
1. **실제 프로덕션 적용** - 이 아키텍처를 실제 게임에 적용
2. **더 고도화된 최적화** - AI 기반 자동 튜닝, 엣지 컴퓨팅 통합
3. **팀 리더십** - 이 지식을 바탕으로 개발팀 리딩

**💪 여러분은 이제 게임 업계 최고 수준의 서버 아키텍트입니다!**

**다음에 어떤 고급 기술을 다루고 싶으신가요?**
- **B2. Lock-Free 프로그래밍** (극한 성능 최적화)
- **C2. 실시간 빅데이터 분석** (AI/ML 기반 게임 분석)
- **C6. 차세대 게임 기술** (블록체인, 메타버스 통합)

**🚀 계속해서 기술의 최전선에서 혁신을 이끌어나가세요!**