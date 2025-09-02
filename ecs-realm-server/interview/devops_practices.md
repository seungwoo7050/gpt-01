# DevOps 실무 질문

## 1. CI/CD 파이프라인

### Q: 게임 서버의 CI/CD 파이프라인을 설계해주세요.

**답변 예시:**

```yaml
# .gitlab-ci.yml 예시
stages:
  - build
  - test
  - deploy

variables:
  DOCKER_REGISTRY: registry.gamecompany.com
  
build:
  stage: build
  script:
    - docker build -t $DOCKER_REGISTRY/game-server:$CI_COMMIT_SHA .
    - docker push $DOCKER_REGISTRY/game-server:$CI_COMMIT_SHA
  only:
    - main
    - develop

unit-test:
  stage: test
  script:
    - mkdir build && cd build
    - cmake .. -DCMAKE_BUILD_TYPE=Debug
    - make -j$(nproc)
    - ctest --output-on-failure
    
integration-test:
  stage: test
  services:
    - redis:latest
    - mysql:latest
  script:
    - ./run_integration_tests.sh
    
deploy-staging:
  stage: deploy
  script:
    - kubectl set image deployment/game-server game-server=$DOCKER_REGISTRY/game-server:$CI_COMMIT_SHA -n staging
    - kubectl rollout status deployment/game-server -n staging
  only:
    - develop
    
deploy-production:
  stage: deploy
  script:
    - kubectl set image deployment/game-server game-server=$DOCKER_REGISTRY/game-server:$CI_COMMIT_SHA -n production
    - kubectl rollout status deployment/game-server -n production
  when: manual
  only:
    - main
```

### Q: 빌드 시간을 단축하는 방법은?

**답변 포인트:**
- **캐시 활용**
  - ccache 사용
  - Docker 레이어 캐싱
  - 의존성 캐시

- **병렬 빌드**
  ```bash
  make -j$(nproc)
  ninja -j$(nproc)
  ```

- **분산 빌드**
  - distcc 활용
  - Bazel 원격 캐시

- **증분 빌드**
  - 변경된 모듈만 빌드
  - 모듈화된 아키텍처

## 2. 인프라 자동화

### Infrastructure as Code (IaC)

**Terraform 예시:**
```hcl
# game_server.tf
resource "aws_instance" "game_server" {
  count             = var.server_count
  ami               = data.aws_ami.game_server.id
  instance_type     = "c5.2xlarge"
  key_name          = var.key_name
  
  vpc_security_group_ids = [aws_security_group.game_server.id]
  
  tags = {
    Name = "GameServer-${count.index}"
    Environment = var.environment
  }
  
  user_data = templatefile("${path.module}/init_script.sh", {
    server_id = count.index
    redis_endpoint = aws_elasticache_cluster.game_cache.cache_nodes[0].address
  })
}

resource "aws_autoscaling_group" "game_servers" {
  name                = "game-servers-asg"
  vpc_zone_identifier = var.subnet_ids
  target_group_arns   = [aws_lb_target_group.game_servers.arn]
  
  min_size         = 2
  max_size         = 20
  desired_capacity = 4
  
  launch_template {
    id      = aws_launch_template.game_server.id
    version = "$Latest"
  }
  
  tag {
    key                 = "Name"
    value               = "GameServer-ASG"
    propagate_at_launch = true
  }
}
```

### Kubernetes 배포

**Deployment 설정:**
```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: game-server
  namespace: production
spec:
  replicas: 10
  selector:
    matchLabels:
      app: game-server
  template:
    metadata:
      labels:
        app: game-server
    spec:
      containers:
      - name: game-server
        image: registry.gamecompany.com/game-server:v1.2.3
        ports:
        - containerPort: 8080
          protocol: TCP
        - containerPort: 8081
          protocol: UDP
        resources:
          requests:
            memory: "2Gi"
            cpu: "2"
          limits:
            memory: "4Gi"
            cpu: "4"
        env:
        - name: SERVER_ID
          valueFrom:
            fieldRef:
              fieldPath: metadata.name
        - name: REDIS_HOST
          value: "redis-cluster.default.svc.cluster.local"
        livenessProbe:
          httpGet:
            path: /health
            port: 8080
          initialDelaySeconds: 30
          periodSeconds: 10
        readinessProbe:
          httpGet:
            path: /ready
            port: 8080
          initialDelaySeconds: 5
          periodSeconds: 5
---
apiVersion: v1
kind: Service
metadata:
  name: game-server-service
spec:
  selector:
    app: game-server
  ports:
  - name: tcp
    port: 8080
    protocol: TCP
  - name: udp
    port: 8081
    protocol: UDP
  type: LoadBalancer
```

## 3. 모니터링 및 알림

### Prometheus 설정
```yaml
# prometheus.yml
global:
  scrape_interval: 15s

scrape_configs:
  - job_name: 'game-servers'
    kubernetes_sd_configs:
    - role: pod
    relabel_configs:
    - source_labels: [__meta_kubernetes_pod_label_app]
      action: keep
      regex: game-server
    - source_labels: [__meta_kubernetes_pod_ip]
      action: replace
      target_label: __address__
      regex: (.+)
      replacement: $1:9090
```

### Grafana 대시보드
```json
{
  "dashboard": {
    "title": "Game Server Metrics",
    "panels": [
      {
        "title": "Active Players",
        "targets": [
          {
            "expr": "sum(game_server_active_players)"
          }
        ]
      },
      {
        "title": "Server CPU Usage",
        "targets": [
          {
            "expr": "avg(rate(process_cpu_seconds_total[5m])) by (instance)"
          }
        ]
      },
      {
        "title": "Request Latency",
        "targets": [
          {
            "expr": "histogram_quantile(0.95, sum(rate(http_request_duration_seconds_bucket[5m])) by (le))"
          }
        ]
      }
    ]
  }
}
```

### 알림 설정
```yaml
# alertmanager.yml
route:
  group_by: ['alertname', 'cluster', 'service']
  group_wait: 10s
  group_interval: 10s
  repeat_interval: 12h
  receiver: 'game-ops-team'
  
receivers:
- name: 'game-ops-team'
  slack_configs:
  - api_url: 'YOUR_SLACK_WEBHOOK_URL'
    channel: '#game-server-alerts'
    title: 'Game Server Alert'
    text: '{{ range .Alerts }}{{ .Annotations.summary }}{{ end }}'
  pagerduty_configs:
  - routing_key: 'YOUR_PAGERDUTY_KEY'
    description: '{{ .GroupLabels.alertname }}'
```

## 4. 로그 관리

### ELK Stack 구성
```json
// Logstash 설정
{
  "input": {
    "beats": {
      "port": 5044
    }
  },
  "filter": {
    "grok": {
      "match": {
        "message": "%{TIMESTAMP_ISO8601:timestamp} %{LOGLEVEL:level} %{DATA:logger} - %{GREEDYDATA:message}"
      }
    },
    "date": {
      "match": ["timestamp", "ISO8601"]
    },
    "mutate": {
      "add_field": {
        "game": "mmorpg",
        "environment": "%{[@metadata][env]}"
      }
    }
  },
  "output": {
    "elasticsearch": {
      "hosts": ["elasticsearch:9200"],
      "index": "gameserver-%{+YYYY.MM.dd}"
    }
  }
}
```

### 로그 분석 쿼리
```json
// Kibana 쿼리 예시
{
  "query": {
    "bool": {
      "must": [
        { "match": { "level": "ERROR" }},
        { "range": { "@timestamp": { "gte": "now-1h" }}}
      ]
    }
  },
  "aggs": {
    "errors_per_server": {
      "terms": {
        "field": "server_id.keyword",
        "size": 10
      }
    }
  }
}
```

## 5. 보안 DevOps

### 비밀 관리
```yaml
# Kubernetes Secret
apiVersion: v1
kind: Secret
metadata:
  name: game-server-secrets
type: Opaque
data:
  db-password: <base64-encoded-password>
  api-key: <base64-encoded-api-key>
```

### Vault 통합
```cpp
class VaultClient {
    string GetSecret(const string& path) {
        // Vault 인증
        auto token = AuthenticateWithVault();
        
        // 시크릿 조회
        http::Request request;
        request.SetHeader("X-Vault-Token", token);
        
        auto response = http::Get(vaultAddr + "/v1/" + path);
        return json::parse(response.body)["data"]["value"];
    }
};
```

## 6. 성능 테스트 자동화

### 부하 테스트
```python
# locust 테스트 스크립트
from locust import HttpUser, task, between

class GameServerUser(HttpUser):
    wait_time = between(1, 3)
    
    @task
    def login(self):
        self.client.post("/api/login", json={
            "username": f"player_{random.randint(1, 10000)}",
            "password": "test_password"
        })
    
    @task(3)
    def move_player(self):
        self.client.post("/api/move", json={
            "x": random.randint(0, 1000),
            "y": random.randint(0, 1000)
        })
    
    @task(2)
    def attack(self):
        self.client.post("/api/attack", json={
            "target_id": random.randint(1, 100),
            "skill_id": random.randint(1, 10)
        })
```

## 면접 예상 질문

1. **"무중단 배포를 어떻게 구현하셨나요?"**
   - Blue-Green, Rolling Update 경험
   - 세션 유지 전략
   - 롤백 계획

2. **"대규모 트래픽 상황에서의 대응 경험은?"**
   - 오토스케일링 설정
   - 부하 분산 전략
   - 캐싱 최적화

3. **"장애 발생 시 대응 프로세스는?"**
   - 모니터링 및 알림
   - 원인 분석 방법
   - 복구 절차

4. **"보안을 위한 DevOps 실천 사항은?"**
   - 시크릿 관리
   - 취약점 스캔
   - 접근 제어

5. **"성능 최적화를 위한 DevOps 접근법은?"**
   - APM 도구 활용
   - 병목 지점 발견
   - 지속적인 개선