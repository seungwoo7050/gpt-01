# 04. Docker Registry - 컨테이너 레지스트리 설정 가이드

## 🎯 목표
Docker 이미지를 안전하게 저장하고 배포하기 위한 프라이빗 컨테이너 레지스트리를 구축합니다.

## 📋 Prerequisites
- Docker 20.10+ 설치
- AWS ECR, Docker Hub, 또는 Harbor 접근 권한
- 이미지 스캔 도구 (Trivy, Clair)
- 최소 100GB 스토리지 (자체 호스팅 시)

---

## 1. 레지스트리 옵션 비교

| 옵션 | 비용 | 관리 부담 | 기능 | 추천 사용 케이스 |
|------|------|-----------|------|------------------|
| AWS ECR | 사용량 기반 | 낮음 | 높음 | AWS 인프라 사용 시 |
| Docker Hub | 무료/유료 | 낮음 | 중간 | 소규모 프로젝트 |
| Harbor | 무료 (자체호스팅) | 높음 | 매우 높음 | 온프레미스/대규모 |
| GitHub Container Registry | 무료/유료 | 낮음 | 중간 | GitHub 사용 시 |
| Google Artifact Registry | 사용량 기반 | 낮음 | 높음 | GCP 사용 시 |

---

## 2. AWS ECR 설정

### 2.1 ECR 리포지토리 생성
```bash
# 게임 서버 리포지토리 생성
aws ecr create-repository \
    --repository-name game-server \
    --image-scanning-configuration scanOnPush=true \
    --encryption-configuration encryptionType=AES256 \
    --region us-east-1

# 모니터링 도구 리포지토리
aws ecr create-repository \
    --repository-name game-server/prometheus \
    --image-scanning-configuration scanOnPush=true

aws ecr create-repository \
    --repository-name game-server/grafana \
    --image-scanning-configuration scanOnPush=true
```

### 2.2 수명 주기 정책
**ecr-lifecycle-policy.json**
```json
{
  "rules": [
    {
      "rulePriority": 1,
      "description": "Keep last 10 production images",
      "selection": {
        "tagStatus": "tagged",
        "tagPrefixList": ["prod", "v"],
        "countType": "imageCountMoreThan",
        "countNumber": 10
      },
      "action": {
        "type": "expire"
      }
    },
    {
      "rulePriority": 2,
      "description": "Keep last 5 staging images",
      "selection": {
        "tagStatus": "tagged",
        "tagPrefixList": ["staging", "stage"],
        "countType": "imageCountMoreThan",
        "countNumber": 5
      },
      "action": {
        "type": "expire"
      }
    },
    {
      "rulePriority": 3,
      "description": "Remove untagged images after 7 days",
      "selection": {
        "tagStatus": "untagged",
        "countType": "sinceImagePushed",
        "countUnit": "days",
        "countNumber": 7
      },
      "action": {
        "type": "expire"
      }
    },
    {
      "rulePriority": 4,
      "description": "Keep only last 3 dev images",
      "selection": {
        "tagStatus": "tagged",
        "tagPrefixList": ["dev", "feature"],
        "countType": "imageCountMoreThan",
        "countNumber": 3
      },
      "action": {
        "type": "expire"
      }
    }
  ]
}
```

```bash
# 정책 적용
aws ecr put-lifecycle-policy \
    --repository-name game-server \
    --lifecycle-policy-text file://ecr-lifecycle-policy.json
```

### 2.3 접근 권한 정책
**ecr-repository-policy.json**
```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Sid": "AllowPushPull",
      "Effect": "Allow",
      "Principal": {
        "AWS": [
          "arn:aws:iam::123456789012:role/GameServerCI",
          "arn:aws:iam::123456789012:role/EKSNodeInstanceRole"
        ]
      },
      "Action": [
        "ecr:BatchCheckLayerAvailability",
        "ecr:BatchGetImage",
        "ecr:CompleteLayerUpload",
        "ecr:GetDownloadUrlForLayer",
        "ecr:InitiateLayerUpload",
        "ecr:PutImage",
        "ecr:UploadLayerPart"
      ]
    },
    {
      "Sid": "AllowPullOnly",
      "Effect": "Allow",
      "Principal": {
        "AWS": "arn:aws:iam::123456789012:role/GameServerReadOnly"
      },
      "Action": [
        "ecr:BatchCheckLayerAvailability",
        "ecr:BatchGetImage",
        "ecr:GetDownloadUrlForLayer"
      ]
    }
  ]
}
```

### 2.4 크로스 리전 복제
```bash
# 복제 규칙 생성
aws ecr put-replication-configuration \
    --replication-configuration '{
      "rules": [
        {
          "destinations": [
            {
              "region": "ap-northeast-2",
              "registryId": "123456789012"
            },
            {
              "region": "eu-west-1",
              "registryId": "123456789012"
            }
          ],
          "repositoryFilters": [
            {
              "filter": "game-server",
              "filterType": "PREFIX_MATCH"
            }
          ]
        }
      ]
    }'
```

---

## 3. Harbor 프라이빗 레지스트리

### 3.1 Harbor 설치 (Kubernetes)
**harbor-values.yaml**
```yaml
expose:
  type: ingress
  tls:
    enabled: true
    certSource: secret
    secret:
      secretName: harbor-tls
  ingress:
    hosts:
      core: registry.game-server.com
      notary: notary.game-server.com
    className: nginx
    annotations:
      nginx.ingress.kubernetes.io/ssl-redirect: "true"
      nginx.ingress.kubernetes.io/proxy-body-size: "0"

externalURL: https://registry.game-server.com

persistence:
  enabled: true
  persistentVolumeClaim:
    registry:
      size: 100Gi
      storageClass: gp3
    database:
      size: 10Gi
      storageClass: gp3
    redis:
      size: 5Gi
      storageClass: gp3

harborAdminPassword: "${HARBOR_ADMIN_PASSWORD}"

database:
  type: internal
  internal:
    password: "${DB_PASSWORD}"

redis:
  type: internal

trivy:
  enabled: true
  vulnType: "os,library"
  severity: "CRITICAL,HIGH,MEDIUM"
  skipUpdate: false

notary:
  enabled: true

metrics:
  enabled: true
  serviceMonitor:
    enabled: true
```

```bash
# Harbor 설치
helm repo add harbor https://helm.goharbor.io
helm repo update

helm install harbor harbor/harbor \
    --namespace harbor \
    --create-namespace \
    --values harbor-values.yaml \
    --wait
```

### 3.2 Harbor 프로젝트 설정
```bash
# Harbor CLI 사용
harbor project create --name game-server --public=false
harbor project create --name game-server-dev --public=false

# 복제 규칙 설정
harbor replication-policy create \
    --name "Sync to DR" \
    --src-registry local \
    --dest-registry dr-registry \
    --dest-namespace game-server \
    --trigger-type scheduled \
    --cron "0 2 * * *"
```

### 3.3 이미지 스캔 정책
```json
{
  "name": "production-scan-policy",
  "description": "Scanning policy for production images",
  "project_id": 1,
  "enabled": true,
  "rule": {
    "vulnerabilities": {
      "severity": ["critical", "high"],
      "cvss_score_threshold": 7.0,
      "fix_available": true
    },
    "scan_before_push": true,
    "prevent_vulnerable": true,
    "whitelist": [
      "CVE-2021-12345"
    ]
  }
}
```

---

## 4. Docker 이미지 빌드 및 푸시

### 4.1 멀티스테이지 Dockerfile 최적화
**Dockerfile.optimized**
```dockerfile
# Build stage with cache mount
FROM ubuntu:22.04 AS builder

# Install dependencies with cache
RUN --mount=type=cache,target=/var/cache/apt \
    --mount=type=cache,target=/var/lib/apt \
    apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libboost-all-dev \
    protobuf-compiler \
    libprotobuf-dev

WORKDIR /build

# Copy dependency files first (better layer caching)
COPY CMakeLists.txt conanfile.txt ./
RUN --mount=type=cache,target=/root/.conan \
    conan install . --build=missing

# Copy source code
COPY src/ ./src/
COPY include/ ./include/
COPY proto/ ./proto/

# Build with cache mount
RUN --mount=type=cache,target=/build/build \
    cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build -j$(nproc) && \
    cp build/bin/game_server /usr/local/bin/

# Runtime stage (distroless for security)
FROM gcr.io/distroless/cc-debian12

# Copy binary
COPY --from=builder /usr/local/bin/game_server /app/game_server

# Copy runtime dependencies
COPY --from=builder /usr/lib/x86_64-linux-gnu/libboost*.so* /usr/lib/
COPY --from=builder /usr/lib/x86_64-linux-gnu/libprotobuf*.so* /usr/lib/

# Configuration
COPY config/ /app/config/

# Non-root user
USER 1000:1000

EXPOSE 8080 9090

ENTRYPOINT ["/app/game_server"]
CMD ["--config", "/app/config/server.yaml"]
```

### 4.2 빌드 스크립트
**scripts/build-and-push.sh**
```bash
#!/bin/bash
set -e

# 설정
REGISTRY=${REGISTRY:-"123456789012.dkr.ecr.us-east-1.amazonaws.com"}
REPO_NAME="game-server"
GIT_COMMIT=$(git rev-parse --short HEAD)
BRANCH=$(git rev-parse --abbrev-ref HEAD)
VERSION=${VERSION:-"dev"}

# ECR 로그인
aws ecr get-login-password --region us-east-1 | \
    docker login --username AWS --password-stdin $REGISTRY

# BuildKit 활성화
export DOCKER_BUILDKIT=1
export BUILDKIT_PROGRESS=plain

# 이미지 태그 결정
if [[ "$BRANCH" == "main" ]]; then
    TAGS="$REGISTRY/$REPO_NAME:latest,$REGISTRY/$REPO_NAME:$VERSION,$REGISTRY/$REPO_NAME:$GIT_COMMIT"
elif [[ "$BRANCH" == "develop" ]]; then
    TAGS="$REGISTRY/$REPO_NAME:staging,$REGISTRY/$REPO_NAME:staging-$GIT_COMMIT"
else
    TAGS="$REGISTRY/$REPO_NAME:dev-$GIT_COMMIT"
fi

# 멀티플랫폼 빌드
docker buildx create --use --name game-builder || true

# 빌드 및 푸시
docker buildx build \
    --platform linux/amd64,linux/arm64 \
    --cache-from type=registry,ref=$REGISTRY/$REPO_NAME:buildcache \
    --cache-to type=registry,ref=$REGISTRY/$REPO_NAME:buildcache,mode=max \
    --build-arg BUILD_DATE=$(date -u +'%Y-%m-%dT%H:%M:%SZ') \
    --build-arg VCS_REF=$GIT_COMMIT \
    --build-arg VERSION=$VERSION \
    --label "org.opencontainers.image.created=$(date -u +'%Y-%m-%dT%H:%M:%SZ')" \
    --label "org.opencontainers.image.revision=$GIT_COMMIT" \
    --label "org.opencontainers.image.version=$VERSION" \
    --push \
    $(echo $TAGS | tr ',' '\n' | sed 's/^/-t /') \
    -f Dockerfile.optimized \
    .

# 이미지 스캔
for TAG in $(echo $TAGS | tr ',' ' '); do
    echo "Scanning $TAG..."
    trivy image --severity CRITICAL,HIGH --exit-code 1 $TAG || {
        echo "Security vulnerabilities found in $TAG"
        exit 1
    }
done

echo "Successfully built and pushed: $TAGS"
```

---

## 5. 이미지 보안 스캔

### 5.1 Trivy 통합
**scripts/security-scan.sh**
```bash
#!/bin/bash

IMAGE=$1
REPORT_FILE="scan-report-$(date +%Y%m%d-%H%M%S).html"

# Trivy 스캔 실행
trivy image \
    --severity CRITICAL,HIGH,MEDIUM \
    --format template \
    --template "@/usr/local/share/trivy/templates/html.tpl" \
    --output $REPORT_FILE \
    $IMAGE

# JSON 형식으로도 저장 (CI/CD 통합용)
trivy image \
    --severity CRITICAL,HIGH,MEDIUM \
    --format json \
    --output scan-results.json \
    $IMAGE

# 결과 분석
CRITICAL=$(jq '[.Results[].Vulnerabilities[]? | select(.Severity=="CRITICAL")] | length' scan-results.json)
HIGH=$(jq '[.Results[].Vulnerabilities[]? | select(.Severity=="HIGH")] | length' scan-results.json)

echo "Scan Results:"
echo "  CRITICAL: $CRITICAL"
echo "  HIGH: $HIGH"

# 임계값 체크
if [ $CRITICAL -gt 0 ]; then
    echo "❌ CRITICAL vulnerabilities found. Blocking deployment."
    exit 1
fi

if [ $HIGH -gt 5 ]; then
    echo "⚠️ Too many HIGH vulnerabilities ($HIGH). Review required."
    exit 1
fi

echo "✅ Security scan passed"
```

### 5.2 정책 기반 스캔 (OPA)
**image-policy.rego**
```rego
package docker.security

import future.keywords.contains
import future.keywords.if
import future.keywords.in

# 금지된 베이스 이미지
denied_base_images := [
    "ubuntu:latest",
    "alpine:latest"
]

# 필수 레이블
required_labels := [
    "maintainer",
    "version",
    "description"
]

# 베이스 이미지 검증
deny[msg] {
    input.Config.Image in denied_base_images
    msg := sprintf("Base image %s is not allowed", [input.Config.Image])
}

# 루트 사용자 금지
deny[msg] {
    input.Config.User == "root"
    msg := "Container must not run as root user"
}

deny[msg] {
    input.Config.User == ""
    msg := "Container must specify a non-root user"
}

# 필수 레이블 확인
deny[msg] {
    required := required_labels[_]
    not input.Config.Labels[required]
    msg := sprintf("Required label '%s' is missing", [required])
}

# 포트 검증
deny[msg] {
    port := input.Config.ExposedPorts[_]
    to_number(port) < 1024
    to_number(port) != 80
    to_number(port) != 443
    msg := sprintf("Privileged port %s is not allowed", [port])
}
```

---

## 6. 레지스트리 모니터링

### 6.1 메트릭 수집
**prometheus-registry-rules.yaml**
```yaml
groups:
  - name: registry
    interval: 30s
    rules:
      - alert: RegistryStorageFull
        expr: |
          (registry_storage_used_bytes / registry_storage_total_bytes) > 0.9
        for: 5m
        labels:
          severity: critical
        annotations:
          summary: "Registry storage is almost full"
          description: "Registry storage usage is {{ $value | humanizePercentage }}"
      
      - alert: HighPullErrorRate
        expr: |
          rate(registry_http_requests_total{code=~"5.."}[5m]) > 0.1
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High error rate in registry"
          description: "Error rate is {{ $value | humanizePercentage }}"
      
      - alert: ImagePullLatency
        expr: |
          histogram_quantile(0.95, rate(registry_http_request_duration_seconds_bucket[5m])) > 10
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High image pull latency"
          description: "P95 latency is {{ $value }}s"
```

### 6.2 레지스트리 대시보드
**grafana-registry-dashboard.json**
```json
{
  "dashboard": {
    "title": "Container Registry Monitoring",
    "panels": [
      {
        "title": "Total Images",
        "targets": [{
          "expr": "count(count by (name, tag) (registry_storage_blob_size_bytes))"
        }]
      },
      {
        "title": "Storage Usage",
        "targets": [{
          "expr": "sum(registry_storage_blob_size_bytes) / 1024 / 1024 / 1024"
        }]
      },
      {
        "title": "Pull/Push Rate",
        "targets": [
          {
            "expr": "rate(registry_http_requests_total{method=\"GET\"}[5m])",
            "legendFormat": "Pulls"
          },
          {
            "expr": "rate(registry_http_requests_total{method=\"PUT\"}[5m])",
            "legendFormat": "Pushes"
          }
        ]
      },
      {
        "title": "Request Latency (P95)",
        "targets": [{
          "expr": "histogram_quantile(0.95, rate(registry_http_request_duration_seconds_bucket[5m]))"
        }]
      }
    ]
  }
}
```

---

## 7. 이미지 서명 및 검증

### 7.1 Cosign을 사용한 이미지 서명
```bash
# Cosign 설치
brew install cosign

# 키 생성
cosign generate-key-pair

# 이미지 서명
cosign sign --key cosign.key $REGISTRY/$REPO_NAME:$VERSION

# 서명 검증
cosign verify --key cosign.pub $REGISTRY/$REPO_NAME:$VERSION
```

### 7.2 정책 적용 (Admission Controller)
**image-verification-policy.yaml**
```yaml
apiVersion: admissionregistration.k8s.io/v1
kind: ValidatingWebhookConfiguration
metadata:
  name: image-verification
webhooks:
  - name: verify.images.security
    clientConfig:
      service:
        name: image-verifier
        namespace: kube-system
        path: "/verify"
    rules:
      - operations: ["CREATE", "UPDATE"]
        apiGroups: [""]
        apiVersions: ["v1"]
        resources: ["pods"]
    admissionReviewVersions: ["v1", "v1beta1"]
    sideEffects: None
    failurePolicy: Fail
```

---

## 8. 레지스트리 백업 및 복구

### 8.1 백업 스크립트
**scripts/backup-registry.sh**
```bash
#!/bin/bash

BACKUP_DIR="/backup/registry/$(date +%Y%m%d)"
REGISTRY_DATA="/var/lib/registry"

# 디렉토리 생성
mkdir -p $BACKUP_DIR

# 레지스트리 데이터 백업
rsync -avz --delete \
    $REGISTRY_DATA/ \
    $BACKUP_DIR/data/

# 메타데이터 백업
docker exec harbor-db \
    pg_dump -U postgres registry > $BACKUP_DIR/registry.sql

# S3로 업로드
aws s3 sync $BACKUP_DIR s3://game-backups/registry/$(date +%Y%m%d)/ \
    --storage-class GLACIER

# 오래된 백업 정리
find /backup/registry -type d -mtime +30 -exec rm -rf {} \;

echo "Registry backup completed: $BACKUP_DIR"
```

### 8.2 복구 절차
```bash
#!/bin/bash

RESTORE_DATE=$1
BACKUP_DIR="/backup/registry/$RESTORE_DATE"

# S3에서 복원
aws s3 sync s3://game-backups/registry/$RESTORE_DATE/ $BACKUP_DIR/

# 레지스트리 중지
docker-compose down

# 데이터 복원
rsync -avz --delete \
    $BACKUP_DIR/data/ \
    /var/lib/registry/

# 데이터베이스 복원
docker exec harbor-db \
    psql -U postgres registry < $BACKUP_DIR/registry.sql

# 레지스트리 재시작
docker-compose up -d

echo "Registry restored from $RESTORE_DATE"
```

---

## ✅ 체크리스트

### 필수 확인사항
- [ ] 레지스트리 선택 및 구성
- [ ] 이미지 네이밍 규칙 정의
- [ ] 수명 주기 정책 설정
- [ ] 보안 스캔 통합
- [ ] 접근 권한 설정
- [ ] 백업 전략 수립

### 권장사항
- [ ] 이미지 서명 구현
- [ ] 멀티 리전 복제
- [ ] 취약점 화이트리스트
- [ ] 모니터링 대시보드
- [ ] 자동 정리 정책

## 🎯 다음 단계
→ [06_logging_system.md](06_logging_system.md) - 로깅 시스템 설정