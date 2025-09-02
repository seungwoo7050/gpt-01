# 11. Rollback Plan - 롤백 계획 및 재해 복구 가이드

## 🎯 목표
배포 실패 시 신속하고 안전하게 이전 버전으로 복원하는 체계적인 롤백 프로세스를 수립합니다.

## 📋 Prerequisites
- 이전 버전 이미지 보관
- 데이터베이스 백업 존재
- 설정 파일 버전 관리
- 롤백 권한 설정
- 모니터링 시스템 활성화

---

## 1. 롤백 트리거 조건

### 1.1 자동 롤백 조건
```yaml
triggers:
  - name: "High Error Rate"
    condition: "error_rate > 5%"
    duration: "5m"
    action: "automatic"
    
  - name: "Low Success Rate"
    condition: "success_rate < 95%"
    duration: "10m"
    action: "automatic"
    
  - name: "High Latency"
    condition: "p95_latency > 1s"
    duration: "15m"
    action: "automatic"
    
  - name: "Memory Leak"
    condition: "memory_usage > 90%"
    duration: "20m"
    action: "automatic"
    
  - name: "Crash Loop"
    condition: "restart_count > 5"
    duration: "10m"
    action: "automatic"
```

### 1.2 수동 롤백 시나리오
- 데이터 손상 발견
- 보안 취약점 확인
- 중요 기능 오작동
- 성능 심각한 저하
- 비즈니스 로직 오류

---

## 2. 롤백 전략별 절차

### 2.1 Kubernetes 롤백 (가장 빠름 - 2분)
```bash
#!/bin/bash
# rollback-k8s.sh

NAMESPACE=game-production
DEPLOYMENT=game-server

# 1. 현재 상태 확인
echo "Current deployment status:"
kubectl rollout history deployment/$DEPLOYMENT -n $NAMESPACE

# 2. 이전 버전으로 롤백
echo "Rolling back to previous version..."
kubectl rollout undo deployment/$DEPLOYMENT -n $NAMESPACE

# 3. 롤백 진행 모니터링
kubectl rollout status deployment/$DEPLOYMENT -n $NAMESPACE

# 4. Pod 상태 확인
kubectl get pods -n $NAMESPACE -l app=game-server

# 5. 버전 확인
kubectl describe deployment $DEPLOYMENT -n $NAMESPACE | grep Image

# 6. 헬스체크
for pod in $(kubectl get pods -n $NAMESPACE -l app=game-server -o name); do
    kubectl exec $pod -n $NAMESPACE -- curl -s http://localhost:8080/health
done
```

### 2.2 Blue-Green 롤백 (즉시 - 30초)
```bash
#!/bin/bash
# rollback-blue-green.sh

NAMESPACE=game-production

# 1. 트래픽을 Blue(이전 버전)로 100% 전환
kubectl patch service game-server -n $NAMESPACE -p '
{
  "spec": {
    "selector": {
      "version": "blue"
    }
  }
}'

# 2. Istio VirtualService 업데이트 (사용 시)
kubectl patch virtualservice game-server-vs -n $NAMESPACE --type merge -p '
{
  "spec": {
    "http": [{
      "route": [{
        "destination": {
          "host": "game-server",
          "subset": "blue"
        },
        "weight": 100
      }]
    }]
  }
}'

# 3. Green 환경 제거
kubectl delete deployment game-server-green -n $NAMESPACE --grace-period=30

# 4. 확인
curl -s https://game.example.com/api/v1/version
```

### 2.3 Canary 롤백 (점진적 - 5분)
```bash
#!/bin/bash
# rollback-canary.sh

NAMESPACE=game-production

# 1. Canary 트래픽 0%로 감소
for weight in 75 50 25 10 0; do
    echo "Reducing canary traffic to ${weight}%..."
    
    kubectl patch virtualservice game-server-vs -n $NAMESPACE --type merge -p "{
        \"spec\": {
            \"http\": [{
                \"route\": [
                    {\"destination\": {\"host\": \"game-server\", \"subset\": \"stable\"}, \"weight\": $((100-weight))},
                    {\"destination\": {\"host\": \"game-server\", \"subset\": \"canary\"}, \"weight\": ${weight}}
                ]
            }]
        }
    }"
    
    sleep 30
    
    # 메트릭 확인
    ERROR_RATE=$(curl -s http://prometheus:9090/api/v1/query?query=rate\(game_errors_total[1m]\) | jq '.data.result[0].value[1]')
    echo "Current error rate: ${ERROR_RATE}"
done

# 2. Canary 배포 제거
kubectl delete deployment game-server-canary -n $NAMESPACE
```

---

## 3. 데이터베이스 롤백

### 3.1 마이그레이션 롤백
```bash
#!/bin/bash
# rollback-database.sh

DB_HOST=mysql.production.local
DB_NAME=gamedb
MIGRATION_VERSION=$1

# 1. 현재 마이그레이션 버전 확인
echo "Current migration version:"
mysql -h $DB_HOST -u admin -p -e "SELECT version FROM schema_migrations ORDER BY version DESC LIMIT 1;" $DB_NAME

# 2. 마이그레이션 롤백 실행
echo "Rolling back to version: $MIGRATION_VERSION"
./migrate -database "mysql://admin:password@$DB_HOST/$DB_NAME" -path ./migrations down $MIGRATION_VERSION

# 3. 데이터 무결성 검사
mysql -h $DB_HOST -u admin -p $DB_NAME <<EOF
-- 외래 키 제약 확인
SELECT 
    TABLE_NAME,
    CONSTRAINT_NAME,
    REFERENCED_TABLE_NAME
FROM
    information_schema.KEY_COLUMN_USAGE
WHERE
    REFERENCED_TABLE_NAME IS NOT NULL
    AND TABLE_SCHEMA = '$DB_NAME';

-- 인덱스 상태 확인
SHOW INDEX FROM players;
SHOW INDEX FROM characters;
SHOW INDEX FROM guilds;

-- 데이터 건수 확인
SELECT 
    (SELECT COUNT(*) FROM players) as players,
    (SELECT COUNT(*) FROM characters) as characters,
    (SELECT COUNT(*) FROM guilds) as guilds;
EOF
```

### 3.2 데이터 백업 복원
```bash
#!/bin/bash
# restore-database-backup.sh

BACKUP_DATE=$1
BACKUP_FILE="/backups/db/gamedb-${BACKUP_DATE}.sql"
DB_HOST=mysql.production.local
DB_NAME=gamedb

# 1. 백업 파일 확인
if [ ! -f "$BACKUP_FILE" ]; then
    echo "Backup file not found: $BACKUP_FILE"
    aws s3 cp s3://game-backups/db/gamedb-${BACKUP_DATE}.sql $BACKUP_FILE
fi

# 2. 현재 데이터베이스 백업 (안전장치)
mysqldump -h $DB_HOST -u admin -p $DB_NAME > /tmp/gamedb-before-restore-$(date +%s).sql

# 3. 데이터베이스 복원
echo "Restoring database from $BACKUP_FILE..."
mysql -h $DB_HOST -u admin -p $DB_NAME < $BACKUP_FILE

# 4. 복원 확인
mysql -h $DB_HOST -u admin -p -e "
    SELECT 
        table_name, 
        table_rows 
    FROM information_schema.tables 
    WHERE table_schema = '$DB_NAME'
    ORDER BY table_rows DESC;
" $DB_NAME
```

---

## 4. 설정 롤백

### 4.1 ConfigMap/Secret 롤백
```bash
#!/bin/bash
# rollback-config.sh

NAMESPACE=game-production
TIMESTAMP=$1

# 1. 백업에서 ConfigMap 복원
kubectl apply -f /backups/configs/configmap-${TIMESTAMP}.yaml

# 2. 백업에서 Secret 복원
kubectl apply -f /backups/configs/secret-${TIMESTAMP}.yaml

# 3. Pod 재시작 (새 설정 적용)
kubectl rollout restart deployment/game-server -n $NAMESPACE

# 4. 설정 확인
kubectl get configmap game-config -n $NAMESPACE -o yaml
kubectl get secret game-secrets -n $NAMESPACE -o yaml
```

### 4.2 환경 변수 롤백
```yaml
# rollback-env-patch.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: game-server
  namespace: game-production
spec:
  template:
    spec:
      containers:
      - name: game-server
        env:
        - name: FEATURE_FLAG_NEW_COMBAT
          value: "false"  # 롤백
        - name: FEATURE_FLAG_GUILD_WAR_V2
          value: "false"  # 롤백
        - name: MAX_PLAYERS
          value: "1000"   # 이전 값으로 복원
```

---

## 5. 완전 재해 복구 (Full DR)

### 5.1 재해 복구 시나리오
```bash
#!/bin/bash
# disaster-recovery.sh

set -e

echo "=== Starting Disaster Recovery ==="

# 1. 인프라 복구
echo "Step 1: Recovering infrastructure..."
terraform apply -auto-approve -var-file=dr.tfvars

# 2. 데이터베이스 복구
echo "Step 2: Restoring database..."
./restore-database-backup.sh $(date -d "1 hour ago" +%Y%m%d-%H)

# 3. Redis 캐시 재구축
echo "Step 3: Rebuilding cache..."
redis-cli FLUSHALL
./scripts/warm-cache.sh

# 4. 애플리케이션 배포
echo "Step 4: Deploying application..."
kubectl apply -f k8s/disaster-recovery/

# 5. 데이터 일관성 검증
echo "Step 5: Validating data consistency..."
./scripts/validate-data-integrity.sh

# 6. 서비스 복구 확인
echo "Step 6: Verifying service recovery..."
./scripts/health-check-all.sh

echo "=== Disaster Recovery Completed ==="
```

### 5.2 Cross-Region 페일오버
```bash
#!/bin/bash
# failover-to-dr-region.sh

PRIMARY_REGION=us-east-1
DR_REGION=us-west-2

# 1. DNS 페일오버
echo "Updating Route53 for failover..."
aws route53 change-resource-record-sets \
    --hosted-zone-id Z123456 \
    --change-batch '{
        "Changes": [{
            "Action": "UPSERT",
            "ResourceRecordSet": {
                "Name": "game.example.com",
                "Type": "A",
                "AliasTarget": {
                    "HostedZoneId": "Z456789",
                    "DNSName": "dr-alb.us-west-2.elb.amazonaws.com",
                    "EvaluateTargetHealth": true
                }
            }
        }]
    }'

# 2. 데이터베이스 프로모션
echo "Promoting DR database to primary..."
aws rds promote-read-replica \
    --db-instance-identifier game-db-dr \
    --region $DR_REGION

# 3. 캐시 동기화
echo "Syncing cache data..."
redis-cli -h dr-redis.$DR_REGION.cache.amazonaws.com \
    --rdb /tmp/redis-backup.rdb

# 4. 트래픽 전환 확인
echo "Verifying traffic switch..."
for i in {1..10}; do
    curl -s https://game.example.com/api/v1/region
    sleep 2
done
```

---

## 6. 롤백 자동화

### 6.1 자동 롤백 스크립트
```python
#!/usr/bin/env python3
# auto-rollback.py

import os
import sys
import time
import subprocess
import requests
from prometheus_client.parser import text_string_to_metric_families

class AutoRollback:
    def __init__(self):
        self.prometheus_url = "http://prometheus:9090"
        self.namespace = "game-production"
        self.deployment = "game-server"
        self.thresholds = {
            "error_rate": 0.05,
            "p95_latency": 1.0,
            "success_rate": 0.95,
            "memory_usage": 0.9,
            "cpu_usage": 0.8
        }
        
    def get_metric(self, query):
        """Prometheus 메트릭 조회"""
        response = requests.get(
            f"{self.prometheus_url}/api/v1/query",
            params={"query": query}
        )
        data = response.json()
        if data["data"]["result"]:
            return float(data["data"]["result"][0]["value"][1])
        return 0
    
    def check_health(self):
        """헬스 체크"""
        metrics = {
            "error_rate": self.get_metric('rate(game_errors_total[5m])'),
            "p95_latency": self.get_metric('histogram_quantile(0.95, rate(game_request_duration_seconds_bucket[5m]))'),
            "success_rate": self.get_metric('rate(game_success_total[5m]) / rate(game_requests_total[5m])'),
            "memory_usage": self.get_metric('container_memory_usage_bytes / container_spec_memory_limit_bytes'),
            "cpu_usage": self.get_metric('rate(container_cpu_usage_seconds_total[5m])')
        }
        
        violations = []
        for metric, value in metrics.items():
            threshold = self.thresholds[metric]
            if metric == "success_rate":
                if value < threshold:
                    violations.append(f"{metric}: {value:.2%} < {threshold:.2%}")
            else:
                if value > threshold:
                    violations.append(f"{metric}: {value:.2f} > {threshold:.2f}")
        
        return violations
    
    def perform_rollback(self):
        """롤백 실행"""
        print("🚨 Initiating automatic rollback...")
        
        # Kubernetes 롤백
        subprocess.run([
            "kubectl", "rollout", "undo",
            f"deployment/{self.deployment}",
            "-n", self.namespace
        ])
        
        # 롤백 상태 확인
        subprocess.run([
            "kubectl", "rollout", "status",
            f"deployment/{self.deployment}",
            "-n", self.namespace
        ])
        
        # Slack 알림
        self.send_alert("Automatic rollback executed")
        
    def send_alert(self, message):
        """알림 전송"""
        webhook_url = os.environ.get("SLACK_WEBHOOK")
        if webhook_url:
            requests.post(webhook_url, json={
                "text": f"🚨 {message}",
                "attachments": [{
                    "color": "danger",
                    "fields": [
                        {"title": "Environment", "value": self.namespace},
                        {"title": "Deployment", "value": self.deployment},
                        {"title": "Time", "value": time.strftime("%Y-%m-%d %H:%M:%S")}
                    ]
                }]
            })
    
    def monitor(self, duration=300):
        """모니터링 루프"""
        print(f"Monitoring deployment for {duration} seconds...")
        start_time = time.time()
        check_interval = 30
        
        while time.time() - start_time < duration:
            violations = self.check_health()
            
            if violations:
                print(f"⚠️ Health check violations detected:")
                for violation in violations:
                    print(f"  - {violation}")
                
                # 3번 연속 실패 시 롤백
                if len(violations) >= 3:
                    self.perform_rollback()
                    return False
            else:
                print(f"✅ Health check passed at {time.strftime('%H:%M:%S')}")
            
            time.sleep(check_interval)
        
        print("✅ Monitoring completed successfully")
        return True

if __name__ == "__main__":
    rollback = AutoRollback()
    success = rollback.monitor(duration=600)  # 10분 모니터링
    sys.exit(0 if success else 1)
```

---

## 7. 롤백 후 검증

### 7.1 검증 체크리스트
```bash
#!/bin/bash
# post-rollback-validation.sh

echo "=== Post-Rollback Validation ==="

# 1. 버전 확인
echo "1. Checking deployed version..."
kubectl get deployment game-server -n game-production -o jsonpath='{.spec.template.spec.containers[0].image}'

# 2. Pod 상태
echo "2. Checking pod status..."
kubectl get pods -n game-production -l app=game-server

# 3. 서비스 엔드포인트
echo "3. Checking service endpoints..."
kubectl get endpoints game-server -n game-production

# 4. 헬스체크
echo "4. Running health checks..."
for i in {1..5}; do
    curl -s https://game.example.com/health | jq .
    sleep 2
done

# 5. 메트릭 확인
echo "5. Checking metrics..."
curl -s http://prometheus:9090/api/v1/query?query=up\{job=\"game-server\"\} | jq .

# 6. 로그 확인
echo "6. Checking recent logs..."
kubectl logs -n game-production -l app=game-server --tail=50 --since=5m

# 7. 데이터베이스 연결
echo "7. Verifying database connectivity..."
kubectl exec -n game-production deployment/game-server -- nc -zv mysql 3306

# 8. Redis 연결
echo "8. Verifying Redis connectivity..."
kubectl exec -n game-production deployment/game-server -- nc -zv redis 6379

echo "=== Validation Complete ==="
```

### 7.2 사용자 영향 평가
```sql
-- 롤백 영향 분석 쿼리
-- 영향받은 사용자 수
SELECT COUNT(DISTINCT player_id) as affected_players
FROM player_sessions
WHERE last_activity BETWEEN '${DEPLOYMENT_TIME}' AND '${ROLLBACK_TIME}';

-- 트랜잭션 손실 확인
SELECT COUNT(*) as lost_transactions
FROM transactions
WHERE created_at BETWEEN '${DEPLOYMENT_TIME}' AND '${ROLLBACK_TIME}'
  AND status = 'pending';

-- 진행 중이던 전투
SELECT COUNT(*) as interrupted_battles
FROM combat_logs
WHERE start_time <= '${ROLLBACK_TIME}'
  AND end_time IS NULL;
```

---

## 8. 롤백 문서화

### 8.1 롤백 보고서 템플릿
```markdown
# Rollback Report - [Date]

## Summary
- **Deployment Version**: v2.1.0 → v2.0.5 (rollback)
- **Rollback Time**: 2024-01-15 14:30 UTC
- **Duration**: 5 minutes
- **Impact**: ~500 users affected

## Root Cause
- Memory leak in combat system
- Increased latency in matchmaking

## Timeline
- 14:00 - Deployment started
- 14:15 - Error rate spike detected
- 14:20 - Decision to rollback
- 14:25 - Rollback initiated
- 14:30 - Rollback completed
- 14:35 - Service normalized

## Lessons Learned
1. Need better memory profiling in staging
2. Load test scenarios need updating
3. Canary deployment duration too short

## Action Items
- [ ] Fix memory leak (JIRA-1234)
- [ ] Update load test scenarios
- [ ] Extend canary period to 30 minutes
```

---

## ✅ 체크리스트

### 롤백 준비
- [ ] 이전 버전 이미지 확인
- [ ] 데이터베이스 백업 확인
- [ ] 설정 백업 확인
- [ ] 롤백 스크립트 테스트
- [ ] 팀 알림 설정

### 롤백 실행
- [ ] 트리거 조건 확인
- [ ] 롤백 승인 획득
- [ ] 롤백 스크립트 실행
- [ ] 진행 상황 모니터링
- [ ] 완료 확인

### 롤백 후
- [ ] 서비스 정상화 확인
- [ ] 사용자 영향 평가
- [ ] 근본 원인 분석
- [ ] 보고서 작성
- [ ] 개선 사항 도출

## 🎯 다음 단계
→ [12_operation_runbook.md](12_operation_runbook.md) - 운영 런북