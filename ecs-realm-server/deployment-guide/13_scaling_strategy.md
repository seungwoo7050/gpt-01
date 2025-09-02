# 13. Scaling Strategy - 스케일링 전략 가이드

## 🎯 목표
트래픽 증가에 따라 게임 서버를 효율적으로 확장하고, 비용을 최적화하는 스케일링 전략을 수립합니다.

## 📋 Prerequisites
- Auto Scaling 도구 (HPA, VPA, Cluster Autoscaler)
- 메트릭 수집 시스템
- 부하 예측 모델
- 비용 모니터링
- 캐싱 인프라

---

## 1. 스케일링 전략 개요

### 1.1 스케일링 차원
```yaml
scaling_dimensions:
  horizontal:
    description: "인스턴스 수 증가"
    use_cases:
      - "동시 접속자 증가"
      - "지역별 분산"
      - "고가용성"
    tools:
      - "Kubernetes HPA"
      - "AWS Auto Scaling"
      - "Load Balancer"
  
  vertical:
    description: "인스턴스 크기 증가"
    use_cases:
      - "CPU/메모리 집약적 작업"
      - "단일 세션 성능"
      - "데이터베이스 성능"
    tools:
      - "Kubernetes VPA"
      - "Instance resizing"
  
  functional:
    description: "기능별 분리"
    use_cases:
      - "마이크로서비스"
      - "특정 기능 부하"
      - "독립적 스케일링"
    tools:
      - "Service mesh"
      - "API Gateway"
```

### 1.2 스케일링 메트릭
```yaml
scaling_metrics:
  primary:
    - cpu_utilization: 70%
    - memory_utilization: 80%
    - request_rate: 1000/s
    - concurrent_users: 5000
  
  custom:
    - game_tick_latency: 50ms
    - matchmaking_queue_size: 100
    - combat_events_per_second: 10000
    - world_instance_load: 80%
```

---

## 2. Horizontal Pod Autoscaling (HPA)

### 2.1 HPA 설정
**k8s/hpa.yaml**
```yaml
apiVersion: autoscaling/v2
kind: HorizontalPodAutoscaler
metadata:
  name: game-server-hpa
  namespace: game-production
spec:
  scaleTargetRef:
    apiVersion: apps/v1
    kind: Deployment
    name: game-server
  
  minReplicas: 3
  maxReplicas: 100
  
  metrics:
  # CPU 기반 스케일링
  - type: Resource
    resource:
      name: cpu
      target:
        type: Utilization
        averageUtilization: 70
  
  # 메모리 기반 스케일링
  - type: Resource
    resource:
      name: memory
      target:
        type: Utilization
        averageUtilization: 80
  
  # 커스텀 메트릭 - 동시 접속자
  - type: Pods
    pods:
      metric:
        name: concurrent_players
      target:
        type: AverageValue
        averageValue: "1000"
  
  # 커스텀 메트릭 - 요청 처리율
  - type: Object
    object:
      metric:
        name: requests_per_second
      describedObject:
        apiVersion: v1
        kind: Service
        name: game-server-service
      target:
        type: Value
        value: "10000"
  
  # 외부 메트릭 - 큐 크기
  - type: External
    external:
      metric:
        name: matchmaking_queue_size
        selector:
          matchLabels:
            queue: ranked
      target:
        type: Value
        value: "100"
  
  behavior:
    scaleDown:
      stabilizationWindowSeconds: 300
      policies:
      - type: Percent
        value: 10
        periodSeconds: 60
      - type: Pods
        value: 2
        periodSeconds: 60
      selectPolicy: Min
    
    scaleUp:
      stabilizationWindowSeconds: 60
      policies:
      - type: Percent
        value: 100
        periodSeconds: 60
      - type: Pods
        value: 10
        periodSeconds: 60
      selectPolicy: Max
```

### 2.2 커스텀 메트릭 제공자
**src/metrics/custom_metrics.cpp**
```cpp
class CustomMetricsProvider {
private:
    prometheus::Registry registry_;
    
public:
    void RegisterMetrics() {
        // 동시 접속자 메트릭
        auto& concurrent_players = prometheus::BuildGauge()
            .Name("game_concurrent_players")
            .Help("Number of concurrent players")
            .Register(registry_);
        
        // 매치메이킹 큐 크기
        auto& queue_size = prometheus::BuildGauge()
            .Name("matchmaking_queue_size")
            .Help("Size of matchmaking queue")
            .Labels({{"queue", "ranked"}})
            .Register(registry_);
        
        // 월드 인스턴스 부하
        auto& world_load = prometheus::BuildGauge()
            .Name("world_instance_load")
            .Help("Load percentage of world instance")
            .Labels({{"instance", ""}})
            .Register(registry_);
    }
    
    void UpdateMetrics() {
        // 실시간 메트릭 업데이트
        concurrent_players.Set(GetActivePlayers());
        queue_size.Set(GetQueueSize("ranked"));
        
        for (auto& instance : GetWorldInstances()) {
            world_load.Labels({{"instance", instance.id}})
                      .Set(instance.GetLoadPercentage());
        }
    }
};
```

---

## 3. Vertical Pod Autoscaling (VPA)

### 3.1 VPA 설정
**k8s/vpa.yaml**
```yaml
apiVersion: autoscaling.k8s.io/v1
kind: VerticalPodAutoscaler
metadata:
  name: game-server-vpa
  namespace: game-production
spec:
  targetRef:
    apiVersion: apps/v1
    kind: Deployment
    name: game-server
  
  updatePolicy:
    updateMode: "Auto"  # Off, Initial, Auto
  
  resourcePolicy:
    containerPolicies:
    - containerName: game-server
      minAllowed:
        cpu: 500m
        memory: 1Gi
      maxAllowed:
        cpu: 4000m
        memory: 8Gi
      controlledResources: ["cpu", "memory"]
      mode: Auto
    
    - containerName: sidecar-proxy
      mode: "Off"  # 사이드카는 스케일링 제외
```

### 3.2 리소스 권장 사항 분석
```bash
#!/bin/bash
# analyze-vpa-recommendations.sh

# VPA 권장사항 확인
kubectl get vpa game-server-vpa -n game-production -o json | jq '.status.recommendation'

# 현재 vs 권장 비교
echo "Current resources:"
kubectl get deployment game-server -n game-production -o json | \
    jq '.spec.template.spec.containers[0].resources'

echo "Recommended resources:"
kubectl get vpa game-server-vpa -n game-production -o json | \
    jq '.status.recommendation.containerRecommendations[0]'

# 비용 영향 계산
python3 calculate_cost_impact.py
```

---

## 4. Cluster Autoscaling

### 4.1 Cluster Autoscaler 설정
**k8s/cluster-autoscaler.yaml**
```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: cluster-autoscaler
  namespace: kube-system
spec:
  template:
    spec:
      containers:
      - image: k8s.gcr.io/autoscaling/cluster-autoscaler:v1.27.0
        name: cluster-autoscaler
        command:
        - ./cluster-autoscaler
        - --v=4
        - --stderrthreshold=info
        - --cloud-provider=aws
        - --skip-nodes-with-local-storage=false
        - --expander=least-waste
        - --node-group-auto-discovery=asg:tag=k8s.io/cluster-autoscaler/enabled,k8s.io/cluster-autoscaler/game-cluster
        - --balance-similar-node-groups
        - --skip-nodes-with-system-pods=false
        - --scale-down-enabled=true
        - --scale-down-delay-after-add=10m
        - --scale-down-unneeded-time=10m
        - --scale-down-utilization-threshold=0.5
        - --max-node-provision-time=15m
        - --max-nodes-total=100
        env:
        - name: AWS_REGION
          value: us-east-1
```

### 4.2 노드 그룹 설정
```yaml
# eks-nodegroup.yaml
apiVersion: eks.amazonaws.com/v1alpha1
kind: Nodegroup
metadata:
  name: game-server-nodes
spec:
  clusterName: game-cluster
  
  scalingConfig:
    minSize: 3
    maxSize: 100
    desiredSize: 5
  
  instanceTypes:
    - c5.2xlarge
    - c5a.2xlarge  # 비용 최적화를 위한 AMD 인스턴스
    - c5n.2xlarge  # 네트워크 최적화 인스턴스
  
  capacityType: MIXED  # ON_DEMAND + SPOT
  
  mixedInstancesPolicy:
    instancesDistribution:
      onDemandBaseCapacity: 3
      onDemandPercentageAboveBaseCapacity: 30
      spotAllocationStrategy: capacity-optimized
  
  labels:
    workload: game-server
    scaling: auto
  
  taints:
    - key: game-server
      value: "true"
      effect: NoSchedule
  
  tags:
    Environment: production
    Team: platform
    "k8s.io/cluster-autoscaler/enabled": "true"
    "k8s.io/cluster-autoscaler/game-cluster": "owned"
```

---

## 5. 데이터베이스 스케일링

### 5.1 Read Replica 자동 스케일링
```python
#!/usr/bin/env python3
# db-autoscaling.py

import boto3
import time
from datetime import datetime, timedelta

class DatabaseAutoscaler:
    def __init__(self):
        self.rds = boto3.client('rds')
        self.cloudwatch = boto3.client('cloudwatch')
        self.cluster_id = 'game-aurora-cluster'
        
    def get_metrics(self):
        """데이터베이스 메트릭 조회"""
        response = self.cloudwatch.get_metric_statistics(
            Namespace='AWS/RDS',
            MetricName='DatabaseConnections',
            Dimensions=[
                {'Name': 'DBClusterIdentifier', 'Value': self.cluster_id}
            ],
            StartTime=datetime.now() - timedelta(minutes=5),
            EndTime=datetime.now(),
            Period=300,
            Statistics=['Average']
        )
        
        if response['Datapoints']:
            return response['Datapoints'][-1]['Average']
        return 0
    
    def get_read_replica_count(self):
        """현재 Read Replica 수 확인"""
        response = self.rds.describe_db_clusters(
            DBClusterIdentifier=self.cluster_id
        )
        return len(response['DBClusters'][0]['DBClusterMembers']) - 1
    
    def add_read_replica(self):
        """Read Replica 추가"""
        replica_id = f"{self.cluster_id}-replica-{int(time.time())}"
        
        self.rds.create_db_instance(
            DBInstanceIdentifier=replica_id,
            DBClusterIdentifier=self.cluster_id,
            DBInstanceClass='db.r5.xlarge',
            Engine='aurora-mysql',
            PubliclyAccessible=False
        )
        
        print(f"Added read replica: {replica_id}")
        
    def remove_read_replica(self, replica_id):
        """Read Replica 제거"""
        self.rds.delete_db_instance(
            DBInstanceIdentifier=replica_id,
            SkipFinalSnapshot=True
        )
        
        print(f"Removed read replica: {replica_id}")
    
    def scale(self):
        """자동 스케일링 로직"""
        connections = self.get_metrics()
        current_replicas = self.get_read_replica_count()
        
        # 스케일 업 조건
        if connections > 1000 and current_replicas < 5:
            self.add_read_replica()
            
        # 스케일 다운 조건
        elif connections < 500 and current_replicas > 1:
            # 가장 오래된 replica 제거
            replicas = self.get_replica_list()
            if replicas:
                self.remove_read_replica(replicas[0])

if __name__ == "__main__":
    scaler = DatabaseAutoscaler()
    
    while True:
        try:
            scaler.scale()
        except Exception as e:
            print(f"Error: {e}")
        
        time.sleep(300)  # 5분마다 체크
```

### 5.2 샤딩 전략
```sql
-- Sharding by player_id
CREATE TABLE players_shard_0 LIKE players;
CREATE TABLE players_shard_1 LIKE players;
CREATE TABLE players_shard_2 LIKE players;
CREATE TABLE players_shard_3 LIKE players;

-- 샤드 라우팅 함수
DELIMITER $$
CREATE FUNCTION get_shard_id(player_id BIGINT)
RETURNS INT
DETERMINISTIC
BEGIN
    RETURN player_id % 4;
END$$
DELIMITER ;

-- 샤드별 데이터 분배
INSERT INTO players_shard_0 
SELECT * FROM players WHERE player_id % 4 = 0;

INSERT INTO players_shard_1 
SELECT * FROM players WHERE player_id % 4 = 1;

-- 이하 생략...
```

---

## 6. 캐싱 스케일링

### 6.1 Redis Cluster 자동 스케일링
```bash
#!/bin/bash
# redis-autoscaling.sh

# 현재 메모리 사용률 확인
MEMORY_USAGE=$(redis-cli INFO memory | grep used_memory_rss_human | cut -d: -f2 | tr -d 'M')
MEMORY_LIMIT=4096  # 4GB

if [ ${MEMORY_USAGE%.*} -gt $((MEMORY_LIMIT * 80 / 100)) ]; then
    echo "Memory usage high, adding Redis node..."
    
    # 새 노드 추가
    redis-cli --cluster add-node new-node:6379 existing-node:6379
    
    # 리샤딩
    redis-cli --cluster reshard existing-node:6379 \
        --cluster-from all \
        --cluster-to new-node-id \
        --cluster-slots 1024 \
        --cluster-yes
fi
```

### 6.2 캐시 워밍 전략
```cpp
class CacheWarmer {
private:
    RedisClient redis_;
    DatabaseClient db_;
    
public:
    void WarmCache() {
        // 1. 자주 접근하는 데이터 프리로드
        PreloadPopularItems();
        PreloadActivePlayerData();
        PreloadLeaderboards();
        
        // 2. 예측 기반 캐싱
        PredictiveCache();
    }
    
    void PreloadPopularItems() {
        auto popular_items = db_.Query(
            "SELECT * FROM items "
            "WHERE access_count > 1000 "
            "ORDER BY access_count DESC "
            "LIMIT 100"
        );
        
        for (const auto& item : popular_items) {
            redis_.Set("item:" + item.id, item.Serialize(), 3600);
        }
    }
    
    void PredictiveCache() {
        // 시간대별 예측 모델 기반 캐싱
        auto current_hour = GetCurrentHour();
        auto predicted_players = GetPredictedActiveUsers(current_hour);
        
        for (const auto& player_id : predicted_players) {
            auto player_data = db_.GetPlayerData(player_id);
            redis_.Set("player:" + player_id, player_data, 1800);
        }
    }
};
```

---

## 7. 지역별 스케일링

### 7.1 Multi-Region 배포
```yaml
# multi-region-deployment.yaml
regions:
  us-east-1:
    name: "North America East"
    capacity:
      min: 10
      max: 50
    latency_target: 20ms
    
  eu-west-1:
    name: "Europe West"
    capacity:
      min: 8
      max: 40
    latency_target: 25ms
    
  ap-northeast-1:
    name: "Asia Pacific"
    capacity:
      min: 15
      max: 60
    latency_target: 30ms

traffic_routing:
  method: "geoproximity"
  health_check:
    interval: 30s
    timeout: 5s
  failover:
    enabled: true
    priority:
      - us-east-1
      - eu-west-1
      - ap-northeast-1
```

### 7.2 GeoDNS 설정
```python
# geodns-routing.py
import boto3

route53 = boto3.client('route53')

def create_geolocation_routing():
    """지역별 라우팅 설정"""
    
    # 북미 라우팅
    route53.change_resource_record_sets(
        HostedZoneId='Z123456789',
        ChangeBatch={
            'Changes': [{
                'Action': 'CREATE',
                'ResourceRecordSet': {
                    'Name': 'game.example.com',
                    'Type': 'A',
                    'SetIdentifier': 'North America',
                    'GeoLocation': {
                        'ContinentCode': 'NA'
                    },
                    'AliasTarget': {
                        'HostedZoneId': 'Z215JYRZR8TBV5',
                        'DNSName': 'us-east-1-alb.amazonaws.com',
                        'EvaluateTargetHealth': True
                    }
                }
            }]
        }
    )
    
    # 유럽 라우팅
    # ... (similar for EU)
    
    # 아시아 라우팅
    # ... (similar for APAC)
```

---

## 8. 비용 최적화

### 8.1 Spot Instance 활용
```yaml
# spot-instance-config.yaml
apiVersion: v1
kind: ConfigMap
metadata:
  name: spot-config
  namespace: karpenter
data:
  # Spot 인스턴스 비율
  spot_ratio: "70"
  
  # 인스턴스 다양성
  instance_types: |
    c5.xlarge
    c5a.xlarge
    c5n.xlarge
    m5.xlarge
    m5a.xlarge
  
  # 인터럽션 처리
  interruption_handler: |
    #!/bin/bash
    # 2분 경고 시 graceful shutdown
    kubectl drain $NODE_NAME --ignore-daemonsets --delete-emptydir-data
    kubectl cordon $NODE_NAME
```

### 8.2 비용 모니터링 대시보드
```python
# cost-monitoring.py
import boto3
from datetime import datetime, timedelta

ce = boto3.client('ce')

def get_daily_costs():
    """일일 비용 조회"""
    response = ce.get_cost_and_usage(
        TimePeriod={
            'Start': (datetime.now() - timedelta(days=30)).strftime('%Y-%m-%d'),
            'End': datetime.now().strftime('%Y-%m-%d')
        },
        Granularity='DAILY',
        Metrics=['UnblendedCost'],
        GroupBy=[
            {'Type': 'DIMENSION', 'Key': 'SERVICE'},
            {'Type': 'TAG', 'Key': 'Environment'}
        ]
    )
    
    return response['ResultsByTime']

def analyze_cost_trends():
    """비용 트렌드 분석"""
    costs = get_daily_costs()
    
    # 서비스별 비용 집계
    service_costs = {}
    for day in costs:
        for group in day['Groups']:
            service = group['Keys'][0]
            cost = float(group['Metrics']['UnblendedCost']['Amount'])
            
            if service not in service_costs:
                service_costs[service] = []
            service_costs[service].append(cost)
    
    # 비용 증가 알림
    for service, costs in service_costs.items():
        if len(costs) > 7:
            week_avg = sum(costs[-7:]) / 7
            prev_week_avg = sum(costs[-14:-7]) / 7
            
            if week_avg > prev_week_avg * 1.2:  # 20% 증가
                send_alert(f"Cost increase detected for {service}: {week_avg:.2f}/day")
```

---

## 9. 예측 기반 스케일링

### 9.1 ML 기반 트래픽 예측
```python
# predictive-scaling.py
from sklearn.ensemble import RandomForestRegressor
import pandas as pd
import numpy as np

class PredictiveScaler:
    def __init__(self):
        self.model = RandomForestRegressor(n_estimators=100)
        self.train_model()
    
    def train_model(self):
        """과거 데이터로 모델 학습"""
        # 과거 트래픽 데이터 로드
        data = pd.read_csv('traffic_history.csv')
        
        # 특징 추출
        data['hour'] = pd.to_datetime(data['timestamp']).dt.hour
        data['day_of_week'] = pd.to_datetime(data['timestamp']).dt.dayofweek
        data['is_weekend'] = data['day_of_week'].isin([5, 6]).astype(int)
        data['is_event'] = data['event_active'].astype(int)
        
        # 학습
        features = ['hour', 'day_of_week', 'is_weekend', 'is_event']
        X = data[features]
        y = data['concurrent_users']
        
        self.model.fit(X, y)
    
    def predict_traffic(self, hours_ahead=1):
        """미래 트래픽 예측"""
        future_time = datetime.now() + timedelta(hours=hours_ahead)
        
        features = {
            'hour': future_time.hour,
            'day_of_week': future_time.weekday(),
            'is_weekend': 1 if future_time.weekday() in [5, 6] else 0,
            'is_event': self.check_event_schedule(future_time)
        }
        
        predicted_users = self.model.predict([list(features.values())])[0]
        return int(predicted_users)
    
    def scale_resources(self):
        """예측 기반 리소스 스케일링"""
        # 1시간 후 트래픽 예측
        predicted = self.predict_traffic(1)
        current = self.get_current_users()
        
        if predicted > current * 1.3:  # 30% 증가 예상
            # 미리 스케일 업
            required_pods = math.ceil(predicted / 1000)  # 1000 users per pod
            self.scale_deployment(required_pods)
            
        elif predicted < current * 0.7:  # 30% 감소 예상
            # 점진적 스케일 다운
            required_pods = math.ceil(predicted / 1000)
            self.scale_deployment(required_pods)
```

---

## 10. 스케일링 모니터링

### 10.1 스케일링 이벤트 추적
```yaml
# scaling-events-dashboard.yaml
apiVersion: v1
kind: ConfigMap
metadata:
  name: scaling-dashboard
data:
  queries: |
    # HPA 스케일링 이벤트
    kube_horizontalpodautoscaler_status_current_replicas{namespace="game-production"}
    
    # 노드 스케일링 이벤트
    kube_node_info{}
    
    # 스케일링 지연 시간
    histogram_quantile(0.95, 
      rate(cluster_autoscaler_scaled_up_nodes_duration_seconds_bucket[5m])
    )
    
    # 스케일링 실패
    cluster_autoscaler_failed_scale_ups_total
    
    # 비용 영향
    sum(kube_node_info) * 0.5  # $0.5 per hour per node
```

---

## ✅ 체크리스트

### 스케일링 설정
- [ ] HPA 구성 및 테스트
- [ ] VPA 구성 및 권장사항 검토
- [ ] Cluster Autoscaler 설정
- [ ] 데이터베이스 Read Replica 자동화
- [ ] 캐시 클러스터 확장 설정

### 최적화
- [ ] Spot Instance 활용
- [ ] 예측 모델 구축
- [ ] 비용 모니터링 대시보드
- [ ] 지역별 배포 전략
- [ ] 캐시 워밍 구현

### 모니터링
- [ ] 스케일링 메트릭 대시보드
- [ ] 비용 알림 설정
- [ ] 성능 임계값 정의
- [ ] 스케일링 이벤트 로깅
- [ ] 용량 계획 리포트

## 🎯 완료
배포 가이드의 모든 문서가 작성되었습니다!