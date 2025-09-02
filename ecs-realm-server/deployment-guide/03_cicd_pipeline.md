# 03. CI/CD Pipeline - 지속적 통합/배포 파이프라인 구축

## 🎯 목표
GitHub Actions를 사용하여 자동화된 빌드, 테스트, 배포 파이프라인을 구축합니다.

## 📋 Prerequisites
- GitHub 저장소
- Docker Hub 또는 AWS ECR 계정
- AWS 자격 증명
- 테스트 커버리지 도구

---

## 1. GitHub Actions 워크플로우 설정

### 1.1 디렉토리 구조 생성
```bash
mkdir -p .github/workflows
```

### 1.2 메인 CI/CD 워크플로우
**.github/workflows/main-cicd.yml**
```yaml
name: Main CI/CD Pipeline

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]
  release:
    types: [ created ]

env:
  REGISTRY: ghcr.io
  IMAGE_NAME: ${{ github.repository }}
  AWS_REGION: us-east-1

jobs:
  # ==================== BUILD & TEST ====================
  build-and-test:
    name: Build and Test
    runs-on: ubuntu-latest
    
    steps:
    - name: Checkout code
      uses: actions/checkout@v3
      with:
        submodules: recursive
    
    - name: Set up build environment
      run: |
        sudo apt-get update
        sudo apt-get install -y \
          cmake \
          g++ \
          libboost-all-dev \
          libprotobuf-dev \
          protobuf-compiler \
          libmysqlclient-dev \
          libssl-dev
    
    - name: Cache dependencies
      uses: actions/cache@v3
      with:
        path: |
          ~/.conan
          build/_deps
        key: ${{ runner.os }}-deps-${{ hashFiles('**/conanfile.txt') }}
        restore-keys: |
          ${{ runner.os }}-deps-
    
    - name: Configure CMake
      run: |
        mkdir -p build
        cd build
        cmake .. \
          -DCMAKE_BUILD_TYPE=Release \
          -DBUILD_TESTS=ON \
          -DENABLE_COVERAGE=ON
    
    - name: Build
      run: |
        cd build
        make -j$(nproc)
    
    - name: Run unit tests
      run: |
        cd build
        ctest --output-on-failure --verbose
    
    - name: Generate test report
      if: always()
      run: |
        cd build
        lcov --capture --directory . --output-file coverage.info
        lcov --remove coverage.info '/usr/*' --output-file coverage.info
        lcov --list coverage.info
    
    - name: Upload coverage to Codecov
      uses: codecov/codecov-action@v3
      with:
        file: ./build/coverage.info
        flags: unittests
        name: codecov-umbrella
    
    - name: Static code analysis
      run: |
        sudo apt-get install -y cppcheck clang-tidy
        cppcheck --enable=all --suppress=missingIncludeSystem src/
        find src -name '*.cpp' -o -name '*.h' | xargs clang-tidy -p build/

  # ==================== SECURITY SCAN ====================
  security-scan:
    name: Security Scanning
    runs-on: ubuntu-latest
    needs: build-and-test
    
    steps:
    - name: Checkout code
      uses: actions/checkout@v3
    
    - name: Run Trivy vulnerability scanner
      uses: aquasecurity/trivy-action@master
      with:
        scan-type: 'fs'
        scan-ref: '.'
        format: 'sarif'
        output: 'trivy-results.sarif'
    
    - name: Upload Trivy results to GitHub Security
      uses: github/codeql-action/upload-sarif@v2
      with:
        sarif_file: 'trivy-results.sarif'
    
    - name: SonarCloud Scan
      uses: SonarSource/sonarcloud-github-action@master
      env:
        GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
        SONAR_TOKEN: ${{ secrets.SONAR_TOKEN }}

  # ==================== DOCKER BUILD ====================
  docker-build:
    name: Build Docker Image
    runs-on: ubuntu-latest
    needs: [build-and-test, security-scan]
    permissions:
      contents: read
      packages: write
    
    outputs:
      image-tag: ${{ steps.meta.outputs.tags }}
      image-digest: ${{ steps.build.outputs.digest }}
    
    steps:
    - name: Checkout code
      uses: actions/checkout@v3
    
    - name: Set up Docker Buildx
      uses: docker/setup-buildx-action@v2
    
    - name: Log in to Container Registry
      uses: docker/login-action@v2
      with:
        registry: ${{ env.REGISTRY }}
        username: ${{ github.actor }}
        password: ${{ secrets.GITHUB_TOKEN }}
    
    - name: Extract metadata
      id: meta
      uses: docker/metadata-action@v4
      with:
        images: ${{ env.REGISTRY }}/${{ env.IMAGE_NAME }}
        tags: |
          type=ref,event=branch
          type=ref,event=pr
          type=semver,pattern={{version}}
          type=semver,pattern={{major}}.{{minor}}
          type=sha,prefix={{branch}}-
          type=raw,value=latest,enable={{is_default_branch}}
    
    - name: Build and push Docker image
      id: build
      uses: docker/build-push-action@v4
      with:
        context: .
        push: true
        tags: ${{ steps.meta.outputs.tags }}
        labels: ${{ steps.meta.outputs.labels }}
        cache-from: type=gha
        cache-to: type=gha,mode=max
        build-args: |
          BUILD_DATE=${{ github.event.head_commit.timestamp }}
          VCS_REF=${{ github.sha }}
          VERSION=${{ steps.meta.outputs.version }}
    
    - name: Scan Docker image
      uses: aquasecurity/trivy-action@master
      with:
        image-ref: ${{ env.REGISTRY }}/${{ env.IMAGE_NAME }}:${{ steps.meta.outputs.version }}
        format: 'table'
        exit-code: '1'
        ignore-unfixed: true
        vuln-type: 'os,library'
        severity: 'CRITICAL,HIGH'

  # ==================== INTEGRATION TESTS ====================
  integration-tests:
    name: Integration Tests
    runs-on: ubuntu-latest
    needs: docker-build
    
    services:
      mysql:
        image: mysql:8.0
        env:
          MYSQL_ROOT_PASSWORD: root
          MYSQL_DATABASE: gamedb_test
        ports:
          - 3306:3306
        options: >-
          --health-cmd="mysqladmin ping"
          --health-interval=10s
          --health-timeout=5s
          --health-retries=5
      
      redis:
        image: redis:7-alpine
        ports:
          - 6379:6379
        options: >-
          --health-cmd="redis-cli ping"
          --health-interval=10s
          --health-timeout=5s
          --health-retries=5
    
    steps:
    - name: Checkout code
      uses: actions/checkout@v3
    
    - name: Set up test environment
      run: |
        docker-compose -f docker-compose.test.yml up -d
        sleep 10
    
    - name: Run integration tests
      run: |
        docker-compose -f docker-compose.test.yml \
          run --rm test-runner \
          pytest tests/integration/ -v --tb=short
    
    - name: Run API tests
      run: |
        npm install -g newman
        newman run tests/api/game-server-api.postman_collection.json \
          --environment tests/api/test-env.postman_environment.json \
          --reporters cli,json \
          --reporter-json-export test-results.json
    
    - name: Cleanup
      if: always()
      run: docker-compose -f docker-compose.test.yml down -v

  # ==================== PERFORMANCE TESTS ====================
  performance-tests:
    name: Performance Tests
    runs-on: ubuntu-latest
    needs: integration-tests
    if: github.event_name == 'push' && github.ref == 'refs/heads/main'
    
    steps:
    - name: Checkout code
      uses: actions/checkout@v3
    
    - name: Set up K6
      run: |
        sudo apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys C5AD17C747E3415A3642D57D77C6C491D6AC1D69
        echo "deb https://dl.k6.io/deb stable main" | sudo tee /etc/apt/sources.list.d/k6.list
        sudo apt-get update
        sudo apt-get install k6
    
    - name: Start test environment
      run: |
        docker-compose -f docker-compose.perf.yml up -d
        sleep 30
    
    - name: Run load tests
      run: |
        k6 run tests/performance/load-test.js \
          --out json=load-test-results.json \
          --summary-export=summary.json
    
    - name: Analyze results
      run: |
        python3 scripts/analyze-perf-results.py \
          --input load-test-results.json \
          --threshold tests/performance/thresholds.yml
    
    - name: Upload performance results
      uses: actions/upload-artifact@v3
      with:
        name: performance-results
        path: |
          load-test-results.json
          summary.json
          performance-report.html

  # ==================== DEPLOY TO STAGING ====================
  deploy-staging:
    name: Deploy to Staging
    runs-on: ubuntu-latest
    needs: [docker-build, integration-tests]
    if: github.ref == 'refs/heads/develop'
    environment:
      name: staging
      url: https://staging.game-server.com
    
    steps:
    - name: Checkout code
      uses: actions/checkout@v3
    
    - name: Configure AWS credentials
      uses: aws-actions/configure-aws-credentials@v2
      with:
        aws-access-key-id: ${{ secrets.AWS_ACCESS_KEY_ID }}
        aws-secret-access-key: ${{ secrets.AWS_SECRET_ACCESS_KEY }}
        aws-region: ${{ env.AWS_REGION }}
    
    - name: Update Kubernetes config
      run: |
        aws eks update-kubeconfig --name game-server-staging --region ${{ env.AWS_REGION }}
    
    - name: Deploy to Kubernetes
      run: |
        kubectl set image deployment/game-server \
          game-server=${{ needs.docker-build.outputs.image-tag }} \
          -n game-staging
        
        kubectl rollout status deployment/game-server -n game-staging
    
    - name: Run smoke tests
      run: |
        ./scripts/smoke-tests.sh https://staging.game-server.com
    
    - name: Notify Slack
      uses: 8398a7/action-slack@v3
      with:
        status: ${{ job.status }}
        text: 'Staging deployment ${{ job.status }}'
        webhook_url: ${{ secrets.SLACK_WEBHOOK }}
      if: always()

  # ==================== DEPLOY TO PRODUCTION ====================
  deploy-production:
    name: Deploy to Production
    runs-on: ubuntu-latest
    needs: [docker-build, performance-tests]
    if: github.event_name == 'release'
    environment:
      name: production
      url: https://game-server.com
    
    steps:
    - name: Checkout code
      uses: actions/checkout@v3
    
    - name: Configure AWS credentials
      uses: aws-actions/configure-aws-credentials@v2
      with:
        aws-access-key-id: ${{ secrets.AWS_ACCESS_KEY_ID_PROD }}
        aws-secret-access-key: ${{ secrets.AWS_SECRET_ACCESS_KEY_PROD }}
        aws-region: ${{ env.AWS_REGION }}
    
    - name: Update Kubernetes config
      run: |
        aws eks update-kubeconfig --name game-server-prod --region ${{ env.AWS_REGION }}
    
    - name: Blue-Green Deployment
      run: |
        # Create new deployment (green)
        kubectl apply -f k8s/overlays/prod/deployment-green.yaml
        
        # Wait for green deployment to be ready
        kubectl wait --for=condition=available \
          --timeout=600s \
          deployment/game-server-green \
          -n game-production
        
        # Run health checks on green deployment
        ./scripts/health-check.sh game-server-green
        
        # Switch traffic to green
        kubectl patch service game-server-service \
          -p '{"spec":{"selector":{"version":"green"}}}' \
          -n game-production
        
        # Monitor for 5 minutes
        sleep 300
        
        # If successful, remove blue deployment
        kubectl delete deployment game-server-blue -n game-production
        
        # Rename green to blue for next deployment
        kubectl patch deployment game-server-green \
          -p '{"metadata":{"name":"game-server-blue"}}' \
          -n game-production
    
    - name: Verify deployment
      run: |
        ./scripts/production-tests.sh https://game-server.com
    
    - name: Create GitHub release notes
      uses: softprops/action-gh-release@v1
      with:
        files: |
          CHANGELOG.md
          docs/release-notes-${{ github.event.release.tag_name }}.md
    
    - name: Notify team
      uses: 8398a7/action-slack@v3
      with:
        status: ${{ job.status }}
        text: |
          Production deployment ${{ job.status }}
          Version: ${{ github.event.release.tag_name }}
          URL: https://game-server.com
        webhook_url: ${{ secrets.SLACK_WEBHOOK_PROD }}
      if: always()
```

---

## 2. 추가 워크플로우

### 2.1 PR 검증 워크플로우
**.github/workflows/pr-validation.yml**
```yaml
name: PR Validation

on:
  pull_request:
    types: [opened, synchronize, reopened]

jobs:
  validate:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Check commit messages
      uses: wagoid/commitlint-github-action@v5
    
    - name: Check PR title
      uses: deepakputhraya/action-pr-title@master
      with:
        regex: '^(feat|fix|docs|style|refactor|perf|test|chore)(\(.+\))?: .{1,50}'
    
    - name: Label PR
      uses: actions/labeler@v4
      with:
        repo-token: "${{ secrets.GITHUB_TOKEN }}"
    
    - name: Size label
      uses: codelytv/pr-size-labeler@v1
      with:
        GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

### 2.2 일일 빌드 워크플로우
**.github/workflows/nightly-build.yml**
```yaml
name: Nightly Build

on:
  schedule:
    - cron: '0 2 * * *'  # 매일 오전 2시 UTC
  workflow_dispatch:

jobs:
  nightly:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Full test suite
      run: |
        docker-compose -f docker-compose.full-test.yml up --abort-on-container-exit
    
    - name: Security audit
      run: |
        ./scripts/security-audit.sh
    
    - name: Dependency check
      run: |
        ./scripts/check-dependencies.sh
    
    - name: Performance benchmark
      run: |
        ./scripts/run-benchmarks.sh
```

---

## 3. 테스트 스크립트

### 3.1 연기 테스트
**scripts/smoke-tests.sh**
```bash
#!/bin/bash
set -e

SERVER_URL=$1

echo "Running smoke tests against $SERVER_URL"

# Health check
curl -f "$SERVER_URL/health" || exit 1

# API availability
curl -f "$SERVER_URL/api/v1/status" || exit 1

# Database connectivity
curl -f "$SERVER_URL/api/v1/db-check" || exit 1

# Redis connectivity
curl -f "$SERVER_URL/api/v1/cache-check" || exit 1

echo "Smoke tests passed!"
```

### 3.2 성능 테스트 (K6)
**tests/performance/load-test.js**
```javascript
import http from 'k6/http';
import { check, sleep } from 'k6';

export let options = {
  stages: [
    { duration: '2m', target: 100 },  // Ramp up
    { duration: '5m', target: 1000 }, // Stay at 1000 users
    { duration: '2m', target: 5000 }, // Spike to 5000
    { duration: '5m', target: 5000 }, // Stay at 5000
    { duration: '2m', target: 0 },    // Ramp down
  ],
  thresholds: {
    http_req_duration: ['p(95)<500'], // 95% of requests under 500ms
    http_req_failed: ['rate<0.1'],    // Error rate under 10%
  },
};

export default function() {
  // Login
  let loginRes = http.post('http://localhost:8080/api/v1/login', {
    username: `user_${__VU}`,
    password: 'test123',
  });
  
  check(loginRes, {
    'login successful': (r) => r.status === 200,
  });
  
  let token = loginRes.json('token');
  
  // Game actions
  let params = {
    headers: { 'Authorization': `Bearer ${token}` },
  };
  
  // Move character
  let moveRes = http.post('http://localhost:8080/api/v1/move', {
    x: Math.random() * 1000,
    y: Math.random() * 1000,
  }, params);
  
  check(moveRes, {
    'move successful': (r) => r.status === 200,
  });
  
  sleep(1);
}
```

---

## 4. Docker 설정

### 4.1 멀티스테이지 Dockerfile
**Dockerfile**
```dockerfile
# Build stage
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libboost-all-dev \
    protobuf-compiler \
    libprotobuf-dev

WORKDIR /app
COPY . .

RUN mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc)

# Runtime stage
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libboost-system1.74.0 \
    libboost-thread1.74.0 \
    libprotobuf23 \
    && rm -rf /var/lib/apt/lists/*

RUN useradd -m -u 1000 gameserver

WORKDIR /app
COPY --from=builder --chown=gameserver:gameserver /app/build/bin/game_server /app/
COPY --chown=gameserver:gameserver config/ ./config/

USER gameserver
EXPOSE 8080 9090

HEALTHCHECK --interval=30s --timeout=3s --start-period=10s --retries=3 \
    CMD curl -f http://localhost:8080/health || exit 1

ENTRYPOINT ["./game_server"]
```

---

## 5. 시크릿 관리

### 5.1 GitHub Secrets 설정
```bash
# 필수 시크릿
gh secret set AWS_ACCESS_KEY_ID
gh secret set AWS_SECRET_ACCESS_KEY
gh secret set DOCKER_REGISTRY_PASSWORD
gh secret set SONAR_TOKEN
gh secret set SLACK_WEBHOOK
gh secret set DB_PASSWORD_PROD
```

### 5.2 환경별 변수
**.github/environments/production.yml**
```yaml
environment:
  DB_HOST: prod-db.amazonaws.com
  REDIS_HOST: prod-redis.amazonaws.com
  LOG_LEVEL: info
  MAX_PLAYERS: 5000
```

---

## ✅ 체크리스트

### 필수 확인사항
- [ ] GitHub Actions 워크플로우 생성
- [ ] Docker 이미지 빌드 및 푸시
- [ ] 자동 테스트 실행
- [ ] 보안 스캔 통과
- [ ] 스테이징 배포 성공
- [ ] 프로덕션 배포 프로세스 검증

### 권장사항
- [ ] 브랜치 보호 규칙 설정
- [ ] 코드 리뷰 필수 설정
- [ ] 자동 롤백 메커니즘
- [ ] 배포 승인 프로세스

## 🎯 다음 단계
→ [04_docker_registry.md](04_docker_registry.md) - Docker 레지스트리 설정