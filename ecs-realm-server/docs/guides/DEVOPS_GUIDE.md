# C++ Multiplayer Game Server - DevOps Guide

## 🎯 Overview
This guide covers the complete DevOps pipeline for the C++ MMORPG game server, from development to production deployment with monitoring and automation.

## 📋 Table of Contents
1. [CI/CD Pipeline](#cicd-pipeline)
2. [Containerization](#containerization)
3. [Kubernetes Deployment](#kubernetes-deployment)
4. [Monitoring & Logging](#monitoring--logging)
5. [Performance Testing](#performance-testing)
6. [Security Practices](#security-practices)
7. [Disaster Recovery](#disaster-recovery)

## CI/CD Pipeline

### GitHub Actions Workflow

**.github/workflows/game-server-ci.yml**
```yaml
name: Game Server CI/CD

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]

env:
  REGISTRY: ghcr.io
  IMAGE_NAME: ${{ github.repository }}

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v3
    
    - name: Set up C++ environment
      run: |
        sudo apt-get update
        sudo apt-get install -y cmake g++ libboost-all-dev libprotobuf-dev protobuf-compiler
        
    - name: Build
      run: |
        mkdir build && cd build
        cmake .. -DCMAKE_BUILD_TYPE=Release
        make -j$(nproc)
        
    - name: Run tests
      run: |
        cd build
        ctest --output-on-failure
        
    - name: Memory leak check
      run: |
        sudo apt-get install -y valgrind
        cd build
        valgrind --leak-check=full --error-exitcode=1 ./tests/test_game_logic

  build-docker:
    needs: test
    runs-on: ubuntu-latest
    permissions:
      contents: read
      packages: write
      
    steps:
    - uses: actions/checkout@v3
    
    - name: Set up Docker Buildx
      uses: docker/setup-buildx-action@v2
      
    - name: Log in to Container Registry
      uses: docker/login-action@v2
      with:
        registry: ${{ env.REGISTRY }}
        username: ${{ github.actor }}
        password: ${{ secrets.GITHUB_TOKEN }}
        
    - name: Build and push Docker image
      uses: docker/build-push-action@v4
      with:
        context: .
        push: true
        tags: |
          ${{ env.REGISTRY }}/${{ env.IMAGE_NAME }}:latest
          ${{ env.REGISTRY }}/${{ env.IMAGE_NAME }}:${{ github.sha }}
        cache-from: type=gha
        cache-to: type=gha,mode=max

  stress-test:
    needs: build-docker
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v3
    
    - name: Run stress tests
      run: |
        docker run -d -p 8080:8080 --name game-server ${{ env.REGISTRY }}/${{ env.IMAGE_NAME }}:${{ github.sha }}
        sleep 10
        python tests/stress_test.py --players 1000 --duration 300
        
    - name: Collect metrics
      run: |
        docker stats --no-stream game-server
        docker logs game-server > server.log
        
    - name: Upload logs
      uses: actions/upload-artifact@v3
      with:
        name: stress-test-logs
        path: server.log

  deploy-staging:
    needs: [build-docker, stress-test]
    runs-on: ubuntu-latest
    if: github.ref == 'refs/heads/develop'
    
    steps:
    - name: Deploy to staging
      run: |
        echo "Deploying to staging environment..."
        # kubectl apply -f k8s/staging/
```

## Containerization

### Multi-stage Dockerfile
```dockerfile
# Build stage
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    cmake g++ libboost-all-dev libprotobuf-dev \
    protobuf-compiler libssl-dev

WORKDIR /app
COPY . .

RUN mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc)

# Runtime stage
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libboost-system1.74.0 libboost-thread1.74.0 \
    libprotobuf23 libssl3 && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy binary and configs
COPY --from=builder /app/build/game_server .
COPY --from=builder /app/configs ./configs
COPY --from=builder /app/data ./data

# Health check
HEALTHCHECK --interval=30s --timeout=3s --retries=3 \
    CMD ./game_server --health-check || exit 1

EXPOSE 8080 8081 9090

CMD ["./game_server"]
```

### Docker Compose for Development
```yaml
version: '3.8'

services:
  game-server-zone1:
    build: .
    environment:
      - ZONE_ID=1
      - MAX_PLAYERS=500
      - DB_HOST=postgres
      - REDIS_HOST=redis
    ports:
      - "8080:8080"
    depends_on:
      - postgres
      - redis
    volumes:
      - ./logs:/app/logs
    
  game-server-zone2:
    build: .
    environment:
      - ZONE_ID=2
      - MAX_PLAYERS=500
      - DB_HOST=postgres
      - REDIS_HOST=redis
    ports:
      - "8081:8080"
    depends_on:
      - postgres
      - redis
      
  postgres:
    image: postgres:15
    environment:
      POSTGRES_DB: gamedb
      POSTGRES_USER: gameuser
      POSTGRES_PASSWORD: ${DB_PASSWORD}
    volumes:
      - postgres-data:/var/lib/postgresql/data
      
  redis:
    image: redis:7-alpine
    command: redis-server --appendonly yes
    volumes:
      - redis-data:/data

  prometheus:
    image: prom/prometheus
    ports:
      - "9090:9090"
    volumes:
      - ./monitoring/prometheus.yml:/etc/prometheus/prometheus.yml
      
volumes:
  postgres-data:
  redis-data:
```

## Kubernetes Deployment

### Helm Chart Structure
```
helm/
├── Chart.yaml
├── values.yaml
├── templates/
│   ├── deployment.yaml
│   ├── service.yaml
│   ├── configmap.yaml
│   ├── hpa.yaml
│   └── ingress.yaml
```

### values.yaml
```yaml
replicaCount: 3

image:
  repository: ghcr.io/your-org/game-server
  pullPolicy: IfNotPresent
  tag: ""

gameServer:
  zones:
    - id: 1
      maxPlayers: 1000
      port: 8080
    - id: 2
      maxPlayers: 1000
      port: 8081
    - id: 3
      maxPlayers: 1000
      port: 8082

resources:
  limits:
    cpu: 2000m
    memory: 4Gi
  requests:
    cpu: 1000m
    memory: 2Gi

autoscaling:
  enabled: true
  minReplicas: 3
  maxReplicas: 10
  targetCPUUtilizationPercentage: 70
  targetMemoryUtilizationPercentage: 80

persistence:
  enabled: true
  storageClass: "fast-ssd"
  size: 50Gi

monitoring:
  enabled: true
  serviceMonitor:
    enabled: true
```

### Deployment Script
```bash
#!/bin/bash
# deploy-k8s.sh

ENVIRONMENT=$1
NAMESPACE="game-server-${ENVIRONMENT}"

# Create namespace
kubectl create namespace ${NAMESPACE} --dry-run=client -o yaml | kubectl apply -f -

# Deploy secrets
kubectl create secret generic game-server-secrets \
  --from-literal=db-password=${DB_PASSWORD} \
  --from-literal=redis-password=${REDIS_PASSWORD} \
  -n ${NAMESPACE} --dry-run=client -o yaml | kubectl apply -f -

# Deploy with Helm
helm upgrade --install game-server ./helm \
  --namespace ${NAMESPACE} \
  --values ./helm/values.${ENVIRONMENT}.yaml \
  --set image.tag=${IMAGE_TAG} \
  --wait

# Check deployment status
kubectl rollout status deployment/game-server -n ${NAMESPACE}
```

## Monitoring & Logging

### Prometheus Configuration
```yaml
# monitoring/prometheus.yml
global:
  scrape_interval: 15s

scrape_configs:
  - job_name: 'game-server'
    kubernetes_sd_configs:
      - role: pod
        namespaces:
          names:
            - game-server-prod
            - game-server-staging
    relabel_configs:
      - source_labels: [__meta_kubernetes_pod_annotation_prometheus_io_scrape]
        action: keep
        regex: true
      - source_labels: [__meta_kubernetes_pod_annotation_prometheus_io_path]
        action: replace
        target_label: __metrics_path__
        regex: (.+)
```

### Grafana Dashboard
```json
{
  "dashboard": {
    "title": "Game Server Metrics",
    "panels": [
      {
        "title": "Active Players",
        "targets": [{
          "expr": "game_server_active_players"
        }]
      },
      {
        "title": "Network Throughput",
        "targets": [{
          "expr": "rate(game_server_bytes_sent[5m])"
        }]
      },
      {
        "title": "Game Loop Performance",
        "targets": [{
          "expr": "histogram_quantile(0.99, game_server_tick_duration_seconds)"
        }]
      }
    ]
  }
}
```

### ELK Stack for Logging
```yaml
# filebeat-config.yml
filebeat.inputs:
- type: container
  paths:
    - /var/log/containers/*game-server*.log
  processors:
    - add_kubernetes_metadata:
        host: ${NODE_NAME}
        matchers:
        - logs_path:
            logs_path: "/var/log/containers/"

output.elasticsearch:
  hosts: ['${ELASTICSEARCH_HOST:elasticsearch}:${ELASTICSEARCH_PORT:9200}']
  username: ${ELASTICSEARCH_USERNAME}
  password: ${ELASTICSEARCH_PASSWORD}
```

## Performance Testing

### Load Testing Script
```python
# tests/load_test.py
import asyncio
import websockets
import json
import time
from concurrent.futures import ThreadPoolExecutor

class GameClient:
    def __init__(self, server_url, player_id):
        self.server_url = server_url
        self.player_id = player_id
        self.connected = False
        
    async def connect_and_play(self):
        async with websockets.connect(self.server_url) as websocket:
            # Login
            await websocket.send(json.dumps({
                "type": "login",
                "player_id": self.player_id
            }))
            
            # Simulate gameplay
            for _ in range(100):
                await websocket.send(json.dumps({
                    "type": "move",
                    "x": random.randint(0, 1000),
                    "y": random.randint(0, 1000)
                }))
                await asyncio.sleep(0.1)

async def stress_test(num_players, server_url):
    tasks = []
    for i in range(num_players):
        client = GameClient(server_url, f"bot_{i}")
        tasks.append(client.connect_and_play())
    
    start_time = time.time()
    await asyncio.gather(*tasks)
    duration = time.time() - start_time
    
    print(f"Completed {num_players} connections in {duration:.2f} seconds")
    print(f"Throughput: {num_players/duration:.2f} connections/second")

if __name__ == "__main__":
    asyncio.run(stress_test(1000, "ws://localhost:8080"))
```

### JMeter Test Plan
```xml
<?xml version="1.0" encoding="UTF-8"?>
<jmeterTestPlan version="1.2">
  <TestPlan>
    <stringProp name="TestPlan.name">Game Server Load Test</stringProp>
    <elementProp name="TestPlan.user_defined_variables">
      <Arguments>
        <elementProp name="server" elementType="Argument">
          <stringProp name="Argument.name">server</stringProp>
          <stringProp name="Argument.value">game-server.example.com</stringProp>
        </elementProp>
      </Arguments>
    </elementProp>
    <ThreadGroup>
      <stringProp name="ThreadGroup.num_threads">1000</stringProp>
      <stringProp name="ThreadGroup.ramp_time">60</stringProp>
      <WebSocketSampler>
        <stringProp name="serverAddress">${server}</stringProp>
        <stringProp name="serverPort">8080</stringProp>
        <stringProp name="implementation">RFC6455</stringProp>
      </WebSocketSampler>
    </ThreadGroup>
  </TestPlan>
</jmeterTestPlan>
```

## Security Practices

### Security Scanning
```yaml
# .github/workflows/security-scan.yml
name: Security Scan

on:
  push:
    branches: [ main, develop ]
  schedule:
    - cron: '0 0 * * 0'  # Weekly scan

jobs:
  trivy-scan:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v3
    
    - name: Run Trivy vulnerability scanner
      uses: aquasecurity/trivy-action@master
      with:
        image-ref: '${{ env.REGISTRY }}/${{ env.IMAGE_NAME }}:latest'
        format: 'sarif'
        output: 'trivy-results.sarif'
        
    - name: Upload Trivy scan results
      uses: github/codeql-action/upload-sarif@v2
      with:
        sarif_file: 'trivy-results.sarif'

  sonarqube:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v3
    
    - name: SonarQube Scan
      uses: sonarsource/sonarqube-scan-action@master
      env:
        GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
        SONAR_TOKEN: ${{ secrets.SONAR_TOKEN }}
```

### Network Policies
```yaml
apiVersion: networking.k8s.io/v1
kind: NetworkPolicy
metadata:
  name: game-server-network-policy
spec:
  podSelector:
    matchLabels:
      app: game-server
  policyTypes:
  - Ingress
  - Egress
  ingress:
  - from:
    - namespaceSelector:
        matchLabels:
          name: game-server-prod
    ports:
    - protocol: TCP
      port: 8080
  egress:
  - to:
    - namespaceSelector:
        matchLabels:
          name: game-server-prod
    ports:
    - protocol: TCP
      port: 5432  # PostgreSQL
    - protocol: TCP
      port: 6379  # Redis
```

## Disaster Recovery

### Backup Strategy
```bash
#!/bin/bash
# backup-game-data.sh

BACKUP_DATE=$(date +%Y%m%d_%H%M%S)
BACKUP_DIR="/backups/game-server/${BACKUP_DATE}"

# Create backup directory
mkdir -p ${BACKUP_DIR}

# Backup PostgreSQL
kubectl exec -n game-server-prod postgres-0 -- \
  pg_dump -U gameuser gamedb | gzip > ${BACKUP_DIR}/gamedb.sql.gz

# Backup game state from Redis
kubectl exec -n game-server-prod redis-0 -- \
  redis-cli BGSAVE

kubectl cp game-server-prod/redis-0:/data/dump.rdb \
  ${BACKUP_DIR}/redis-dump.rdb

# Backup persistent volumes
kubectl get pv -o json > ${BACKUP_DIR}/persistent-volumes.json

# Upload to S3
aws s3 sync ${BACKUP_DIR} s3://game-backups/${BACKUP_DATE}/

# Clean old backups (keep 30 days)
find /backups/game-server -type d -mtime +30 -exec rm -rf {} \;
```

### Rollback Procedure
```bash
#!/bin/bash
# rollback.sh

ROLLBACK_VERSION=$1

echo "Rolling back to version: ${ROLLBACK_VERSION}"

# Rollback deployment
kubectl set image deployment/game-server \
  game-server=ghcr.io/your-org/game-server:${ROLLBACK_VERSION} \
  -n game-server-prod

# Wait for rollout
kubectl rollout status deployment/game-server -n game-server-prod

# Verify health
./scripts/health-check.sh
```

## 🚀 Quick Commands

```bash
# Local development
make build && make test

# Docker build
docker build -t game-server:dev .

# Run with docker-compose
docker-compose up -d

# Deploy to staging
./scripts/deploy.sh staging

# Deploy to production
./scripts/deploy.sh production

# View logs
kubectl logs -f deployment/game-server -n game-server-prod

# Scale deployment
kubectl scale deployment/game-server --replicas=5 -n game-server-prod

# Run performance test
python tests/stress_test.py --players 5000 --duration 600
```

## 📊 Metrics to Monitor

1. **System Metrics**
   - CPU usage per game loop
   - Memory allocation/deallocation rate
   - Network I/O throughput
   - Disk I/O for persistence

2. **Game Metrics**
   - Active player count
   - Game loop tick rate
   - Physics calculation time
   - Network message queue size

3. **Business Metrics**
   - Player session duration
   - Actions per minute
   - Zone distribution
   - Peak concurrent users

## 🔍 Troubleshooting

### Common Issues

1. **High CPU Usage**
   ```bash
   # Profile CPU usage
   kubectl exec -it game-server-pod -- /app/game_server --profile-cpu
   ```

2. **Memory Leaks**
   ```bash
   # Run with memory profiler
   kubectl exec -it game-server-pod -- valgrind --leak-check=full /app/game_server
   ```

3. **Network Latency**
   ```bash
   # Check network metrics
   kubectl exec -it game-server-pod -- ss -s
   ```

## 📚 Additional Resources

- [Game Server Architecture](./ARCHITECTURE.md)
- [Performance Tuning Guide](./docs/PERFORMANCE.md)
- [Security Best Practices](./docs/SECURITY.md)
- [Monitoring Dashboard Templates](./monitoring/)