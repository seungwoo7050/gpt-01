# 24단계: Kubernetes 운영 가이드 - 게임 서버를 자동으로 관리하는 마법사
*1000개의 게임 서버가 스스로 생성되고, 장애가 나면 스스로 복구하는 자동화의 극치*

> **🎯 목표**: 대규모 게임 서버를 Kubernetes로 자동화하고 확장 가능하게 운영하는 프로덕션급 시스템 구축

---

## 📋 문서 정보

- **난이도**: 🟡 중급→🔴 고급 (DevOps의 꽃!)
- **예상 학습 시간**: 10-12시간 (자동화는 처음 설정이 관건)
- **필요 선수 지식**: 
  - ✅ [1-23단계](./01_advanced_cpp_features.md) 모든 내용 완료
  - ✅ Docker 기본 사용법
  - ✅ 리눅스 시스템 관리 경험
  - ✅ "서버 관리가 얼마나 귀찮은지" 아는 고통
- **실습 환경**: Kubernetes 클러스터, kubectl, Helm
- **최종 결과물**: 구글급 자동화 운영 시스템!

---

## 📚 Integration with MMORPG Server Architecture

**Current Container Setup:**
- **Docker**: Single container deployment with docker-compose
- **Manual Scaling**: Horizontal scaling through manual container creation
- **Basic Monitoring**: Docker stats and simple health checks

**Kubernetes Enhancement:**
- **Pod Management**: Automatic pod lifecycle management
- **Service Discovery**: Built-in DNS and load balancing
- **Auto-scaling**: HPA and VPA for dynamic resource allocation
- **Rolling Updates**: Zero-downtime deployments

```
🚢 Kubernetes Architecture for Game Servers
├── Control Plane
│   ├── API Server: Game server deployment management
│   ├── etcd: Service discovery and configuration
│   └── Scheduler: Optimal pod placement for low latency
├── Worker Nodes
│   ├── Game Server Pods: Stateful game world instances
│   ├── Database Pods: MySQL/Redis with persistent volumes
│   └── Supporting Services: Monitoring, logging, ingress
└── Networking
    ├── Service Mesh: Istio for advanced traffic management
    ├── Ingress: NGINX for external load balancing
    └── CNI: Calico for network policies and security
```

---

## 🎮 Game Server Specific Kubernetes Patterns

### Stateful Game World Pods

**`k8s/game-server/statefulset.yaml` - Actual Implementation:**
```yaml
# [SEQUENCE: 1] StatefulSet for game world instances
apiVersion: apps/v1
kind: StatefulSet
metadata:
  name: game-world-servers
  namespace: mmorpg-game
  labels:
    app: game-world-server
    component: world-service
spec:
  serviceName: game-world-service
  replicas: 3  # 3 world instances for load distribution
  selector:
    matchLabels:
      app: game-world-server
  template:
    metadata:
      labels:
        app: game-world-server
        component: world-service
      annotations:
        prometheus.io/scrape: "true"
        prometheus.io/port: "9090"
        prometheus.io/path: "/metrics"
    spec:
      # [SEQUENCE: 2] Anti-affinity for geographic distribution
      affinity:
        podAntiAffinity:
          requiredDuringSchedulingIgnoredDuringExecution:
          - labelSelector:
              matchExpressions:
              - key: app
                operator: In
                values:
                - game-world-server
            topologyKey: kubernetes.io/hostname
      
      # [SEQUENCE: 3] Init container for world data loading
      initContainers:
      - name: world-data-loader
        image: mmorpg/world-data-loader:v1.2.0
        command:
        - /bin/sh
        - -c
        - |
          echo "Loading world data for instance $HOSTNAME"
          # Extract world number from hostname (game-world-servers-0, game-world-servers-1, etc.)
          WORLD_ID=$(echo $HOSTNAME | sed 's/.*-//')
          /usr/local/bin/load-world-data --world-id=$WORLD_ID --data-path=/data/world
        volumeMounts:
        - name: world-data
          mountPath: /data/world
        env:
        - name: HOSTNAME
          valueFrom:
            fieldRef:
              fieldPath: metadata.name
      
      containers:
      - name: game-server
        image: mmorpg/game-server:v2.1.0
        ports:
        - containerPort: 8080
          name: game-port
          protocol: TCP
        - containerPort: 9090
          name: metrics-port
          protocol: TCP
        
        # [SEQUENCE: 4] Resource requirements for game performance
        resources:
          requests:
            memory: "2Gi"
            cpu: "1000m"      # 1 CPU core minimum
            nvidia.com/gpu: 0 # No GPU required for server
          limits:
            memory: "4Gi"
            cpu: "2000m"      # 2 CPU cores maximum
            nvidia.com/gpu: 0
        
        # [SEQUENCE: 5] Environment configuration
        env:
        - name: WORLD_ID
          valueFrom:
            fieldRef:
              fieldPath: metadata.name
        - name: REDIS_HOST
          value: "redis-cluster-service"
        - name: MYSQL_HOST
          value: "mysql-primary-service"
        - name: LOG_LEVEL
          value: "INFO"
        - name: MAX_PLAYERS_PER_WORLD
          value: "2000"
        
        # [SEQUENCE: 6] Configuration from ConfigMap and Secrets
        envFrom:
        - configMapRef:
            name: game-server-config
        - secretRef:
            name: game-server-secrets
        
        # [SEQUENCE: 7] Volume mounts
        volumeMounts:
        - name: world-data
          mountPath: /app/data/world
          readOnly: true
        - name: logs
          mountPath: /app/logs
        - name: config
          mountPath: /app/config
          readOnly: true
        
        # [SEQUENCE: 8] Health checks
        livenessProbe:
          httpGet:
            path: /health/live
            port: 8080
          initialDelaySeconds: 30
          periodSeconds: 10
          timeoutSeconds: 5
          failureThreshold: 3
        
        readinessProbe:
          httpGet:
            path: /health/ready
            port: 8080
          initialDelaySeconds: 10
          periodSeconds: 5
          timeoutSeconds: 3
          successThreshold: 1
          failureThreshold: 2
        
        # [SEQUENCE: 9] Startup probe for slow-starting game worlds
        startupProbe:
          httpGet:
            path: /health/startup
            port: 8080
          initialDelaySeconds: 10
          periodSeconds: 10
          timeoutSeconds: 5
          failureThreshold: 10  # Allow up to 100 seconds for startup
      
      # [SEQUENCE: 10] Volumes
      volumes:
      - name: config
        configMap:
          name: game-server-config
      - name: logs
        emptyDir: {}
  
  # [SEQUENCE: 11] Persistent volumes for world state
  volumeClaimTemplates:
  - metadata:
      name: world-data
    spec:
      accessModes: ["ReadWriteOnce"]
      storageClassName: fast-ssd
      resources:
        requests:
          storage: 10Gi

---
# [SEQUENCE: 12] Headless service for StatefulSet
apiVersion: v1
kind: Service
metadata:
  name: game-world-service
  namespace: mmorpg-game
  labels:
    app: game-world-server
spec:
  clusterIP: None  # Headless service for StatefulSet
  selector:
    app: game-world-server
  ports:
  - port: 8080
    targetPort: 8080
    name: game-port
  - port: 9090
    targetPort: 9090
    name: metrics-port
```

### Database High Availability

**MySQL Cluster with Kubernetes:**
```yaml
# [SEQUENCE: 13] MySQL primary-replica setup
apiVersion: apps/v1
kind: Deployment
metadata:
  name: mysql-primary
  namespace: mmorpg-game
spec:
  replicas: 1  # Only one primary
  selector:
    matchLabels:
      app: mysql
      role: primary
  template:
    metadata:
      labels:
        app: mysql
        role: primary
    spec:
      containers:
      - name: mysql
        image: mysql:8.0
        env:
        - name: MYSQL_ROOT_PASSWORD
          valueFrom:
            secretRef:
              name: mysql-secrets
              key: root-password
        - name: MYSQL_REPLICATION_USER
          value: "replicator"
        - name: MYSQL_REPLICATION_PASSWORD
          valueFrom:
            secretRef:
              name: mysql-secrets
              key: replication-password
        
        # [SEQUENCE: 14] MySQL configuration for game servers
        args:
        - --server-id=1
        - --log-bin=mysql-bin
        - --binlog-format=ROW
        - --innodb-buffer-pool-size=1G
        - --max-connections=1000
        - --query-cache-size=256M
        - --innodb-log-file-size=256M
        
        ports:
        - containerPort: 3306
        
        volumeMounts:
        - name: mysql-data
          mountPath: /var/lib/mysql
        - name: mysql-config
          mountPath: /etc/mysql/conf.d
        
        # [SEQUENCE: 15] MySQL health checks
        livenessProbe:
          exec:
            command:
            - mysqladmin
            - ping
            - -h
            - localhost
          initialDelaySeconds: 30
          periodSeconds: 10
        
        readinessProbe:
          exec:
            command:
            - mysql
            - -h
            - localhost
            - -e
            - "SELECT 1"
          initialDelaySeconds: 10
          periodSeconds: 5
      
      volumes:
      - name: mysql-config
        configMap:
          name: mysql-config
      - name: mysql-data
        persistentVolumeClaim:
          claimName: mysql-primary-pvc

---
# [SEQUENCE: 16] Redis Cluster for game sessions
apiVersion: apps/v1
kind: StatefulSet
metadata:
  name: redis-cluster
  namespace: mmorpg-game
spec:
  serviceName: redis-cluster-service
  replicas: 6  # 3 masters + 3 replicas
  selector:
    matchLabels:
      app: redis-cluster
  template:
    metadata:
      labels:
        app: redis-cluster
    spec:
      containers:
      - name: redis
        image: redis:7.0-alpine
        command:
        - redis-server
        - /etc/redis/redis.conf
        - --cluster-enabled
        - --cluster-config-file
        - /data/nodes.conf
        - --cluster-node-timeout
        - "5000"
        - --appendonly
        - "yes"
        - --protected-mode
        - "no"
        
        ports:
        - containerPort: 6379
          name: client
        - containerPort: 16379
          name: cluster
        
        # [SEQUENCE: 17] Redis resource optimization
        resources:
          requests:
            memory: "512Mi"
            cpu: "250m"
          limits:
            memory: "1Gi"
            cpu: "500m"
        
        volumeMounts:
        - name: redis-data
          mountPath: /data
        - name: redis-config
          mountPath: /etc/redis
      
      volumes:
      - name: redis-config
        configMap:
          name: redis-config
  
  volumeClaimTemplates:
  - metadata:
      name: redis-data
    spec:
      accessModes: ["ReadWriteOnce"]
      storageClassName: fast-ssd
      resources:
        requests:
          storage: 5Gi
```

---

## 🔄 Auto-scaling and Load Management

### Horizontal Pod Autoscaler (HPA)

**`src/k8s/autoscaling/hpa.yaml` - Implementation:**
```yaml
# [SEQUENCE: 18] HPA for game servers based on player load
apiVersion: autoscaling/v2
kind: HorizontalPodAutoscaler
metadata:
  name: game-server-hpa
  namespace: mmorpg-game
spec:
  scaleTargetRef:
    apiVersion: apps/v1
    kind: Deployment
    name: game-server
  minReplicas: 3
  maxReplicas: 20
  
  # [SEQUENCE: 19] Multiple scaling metrics
  metrics:
  # CPU utilization
  - type: Resource
    resource:
      name: cpu
      target:
        type: Utilization
        averageUtilization: 70
  
  # Memory utilization  
  - type: Resource
    resource:
      name: memory
      target:
        type: Utilization
        averageUtilization: 80
  
  # [SEQUENCE: 20] Custom metrics for game-specific scaling
  - type: Pods
    pods:
      metric:
        name: active_players_per_pod
      target:
        type: AverageValue
        averageValue: "1500"  # Scale when average > 1500 players per pod
  
  - type: Pods
    pods:
      metric:
        name: average_response_time_ms
      target:
        type: AverageValue
        averageValue: "100"   # Scale when response time > 100ms
  
  # [SEQUENCE: 21] Scaling behavior configuration
  behavior:
    scaleUp:
      stabilizationWindowSeconds: 60      # Wait 1 minute before scaling up
      policies:
      - type: Percent
        value: 50                         # Scale up by 50% of current replicas
        periodSeconds: 60
      - type: Pods
        value: 2                          # Or add 2 pods, whichever is smaller
        periodSeconds: 60
    
    scaleDown:
      stabilizationWindowSeconds: 300     # Wait 5 minutes before scaling down
      policies:
      - type: Percent
        value: 10                         # Scale down by 10% of current replicas
        periodSeconds: 60

---
# [SEQUENCE: 22] Vertical Pod Autoscaler for resource optimization
apiVersion: autoscaling.k8s.io/v1
kind: VerticalPodAutoscaler
metadata:
  name: game-server-vpa
  namespace: mmorpg-game
spec:
  targetRef:
    apiVersion: apps/v1
    kind: Deployment
    name: game-server
  updatePolicy:
    updateMode: "Auto"  # Automatically apply recommendations
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
```

### Custom Metrics for Game Servers

**Prometheus Custom Metrics Adapter:**
```yaml
# [SEQUENCE: 23] Custom metrics configuration
apiVersion: v1
kind: ConfigMap
metadata:
  name: adapter-config
  namespace: custom-metrics
data:
  config.yaml: |
    rules:
    # [SEQUENCE: 24] Active players per pod metric
    - seriesQuery: 'game_server_active_players{namespace!="",pod!=""}'
      resources:
        overrides:
          namespace: {resource: "namespace"}
          pod: {resource: "pod"}
      name:
        matches: "^game_server_active_players"
        as: "active_players_per_pod"
      metricsQuery: 'sum(<<.Series>>{<<.LabelMatchers>>}) by (<<.GroupBy>>)'
    
    # [SEQUENCE: 25] Response time metric
    - seriesQuery: 'game_server_response_time_ms{namespace!="",pod!=""}'
      resources:
        overrides:
          namespace: {resource: "namespace"}
          pod: {resource: "pod"}
      name:
        matches: "^game_server_response_time_ms"
        as: "average_response_time_ms"
      metricsQuery: 'avg(<<.Series>>{<<.LabelMatchers>>}) by (<<.GroupBy>>)'
    
    # [SEQUENCE: 26] Queue depth metric for matchmaking
    - seriesQuery: 'game_server_matchmaking_queue_depth{namespace!="",pod!=""}'
      resources:
        overrides:
          namespace: {resource: "namespace"}
          pod: {resource: "pod"}
      name:
        matches: "^game_server_matchmaking_queue_depth"
        as: "matchmaking_queue_depth"
      metricsQuery: 'sum(<<.Series>>{<<.LabelMatchers>>}) by (<<.GroupBy>>)'

---
# [SEQUENCE: 27] ServiceMonitor for Prometheus scraping
apiVersion: monitoring.coreos.com/v1
kind: ServiceMonitor
metadata:
  name: game-server-metrics
  namespace: mmorpg-game
spec:
  selector:
    matchLabels:
      app: game-server
  endpoints:
  - port: metrics-port
    interval: 15s
    path: /metrics
    honorLabels: true
```

---

## 🌐 Ingress and Load Balancing

### NGINX Ingress for Game Traffic

**`k8s/ingress/game-ingress.yaml` - Implementation:**
```yaml
# [SEQUENCE: 28] NGINX Ingress with game-specific optimizations
apiVersion: networking.k8s.io/v1
kind: Ingress
metadata:
  name: game-server-ingress
  namespace: mmorpg-game
  annotations:
    # [SEQUENCE: 29] NGINX optimizations for game traffic
    nginx.ingress.kubernetes.io/rewrite-target: /
    nginx.ingress.kubernetes.io/proxy-read-timeout: "3600"     # 1 hour for long connections
    nginx.ingress.kubernetes.io/proxy-send-timeout: "3600"
    nginx.ingress.kubernetes.io/proxy-connect-timeout: "10"
    nginx.ingress.kubernetes.io/proxy-body-size: "1m"         # Max request size
    
    # [SEQUENCE: 30] Session affinity for stateful game connections
    nginx.ingress.kubernetes.io/affinity: "cookie"
    nginx.ingress.kubernetes.io/affinity-canary-behavior: "stick"
    nginx.ingress.kubernetes.io/session-cookie-name: "game-session"
    nginx.ingress.kubernetes.io/session-cookie-expires: "3600"
    nginx.ingress.kubernetes.io/session-cookie-max-age: "3600"
    nginx.ingress.kubernetes.io/session-cookie-path: "/"
    
    # [SEQUENCE: 31] Rate limiting to prevent DDoS
    nginx.ingress.kubernetes.io/rate-limit: "1000"            # 1000 requests per minute
    nginx.ingress.kubernetes.io/rate-limit-window: "1m"
    nginx.ingress.kubernetes.io/rate-limit-burst: "50"        # Allow bursts up to 50
    
    # [SEQUENCE: 32] SSL and security headers
    nginx.ingress.kubernetes.io/ssl-redirect: "true"
    nginx.ingress.kubernetes.io/force-ssl-redirect: "true"
    nginx.ingress.kubernetes.io/ssl-ciphers: "ECDHE-RSA-AES128-GCM-SHA256,ECDHE-RSA-AES256-GCM-SHA384"
    
    # [SEQUENCE: 33] Custom NGINX configuration
    nginx.ingress.kubernetes.io/configuration-snippet: |
      # Optimize for game traffic
      proxy_buffering off;
      proxy_cache off;
      
      # WebSocket support for real-time communication
      proxy_set_header Upgrade $http_upgrade;
      proxy_set_header Connection "upgrade";
      proxy_set_header Host $host;
      proxy_set_header X-Real-IP $remote_addr;
      proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
      proxy_set_header X-Forwarded-Proto $scheme;
      
      # Game-specific headers
      proxy_set_header X-Game-Region $http_x_game_region;
      proxy_set_header X-Player-ID $http_x_player_id;

spec:
  ingressClassName: nginx
  
  # [SEQUENCE: 34] TLS configuration
  tls:
  - hosts:
    - api.mmorpg-game.com
    - ws.mmorpg-game.com
    secretName: game-server-tls
  
  rules:
  # [SEQUENCE: 35] API traffic routing
  - host: api.mmorpg-game.com
    http:
      paths:
      - path: /api/auth
        pathType: Prefix
        backend:
          service:
            name: auth-service
            port:
              number: 8080
      
      - path: /api/players
        pathType: Prefix
        backend:
          service:
            name: player-service
            port:
              number: 8080
      
      - path: /api/guilds
        pathType: Prefix
        backend:
          service:
            name: guild-service
            port:
              number: 8080
      
      - path: /api/world
        pathType: Prefix
        backend:
          service:
            name: world-service
            port:
              number: 8080
  
  # [SEQUENCE: 36] WebSocket traffic routing
  - host: ws.mmorpg-game.com
    http:
      paths:
      - path: /ws/game
        pathType: Prefix
        backend:
          service:
            name: world-service
            port:
              number: 8080
```

### Geographic Load Balancing

**Multi-Region Deployment Strategy:**
```yaml
# [SEQUENCE: 37] Region-specific deployments
apiVersion: v1
kind: ConfigMap
metadata:
  name: region-config
  namespace: mmorpg-game
data:
  regions.yaml: |
    regions:
      us-west:
        enabled: true
        capacity: 10000
        latency_threshold_ms: 50
        game_servers:
          min_replicas: 5
          max_replicas: 50
      
      us-east:
        enabled: true
        capacity: 8000
        latency_threshold_ms: 60
        game_servers:
          min_replicas: 4
          max_replicas: 40
      
      eu-west:
        enabled: true
        capacity: 12000
        latency_threshold_ms: 45
        game_servers:
          min_replicas: 6
          max_replicas: 60
      
      asia-pacific:
        enabled: false  # Maintenance mode
        capacity: 0
        game_servers:
          min_replicas: 0
          max_replicas: 0

---
# [SEQUENCE: 38] Regional node affinity
apiVersion: apps/v1
kind: Deployment
metadata:
  name: game-server-us-west
  namespace: mmorpg-game
spec:
  replicas: 5
  selector:
    matchLabels:
      app: game-server
      region: us-west
  template:
    metadata:
      labels:
        app: game-server
        region: us-west
    spec:
      # [SEQUENCE: 39] Node affinity for regional deployment
      affinity:
        nodeAffinity:
          requiredDuringSchedulingIgnoredDuringExecution:
            nodeSelectorTerms:
            - matchExpressions:
              - key: topology.kubernetes.io/region
                operator: In
                values:
                - us-west-1
                - us-west-2
          
          preferredDuringSchedulingIgnoredDuringExecution:
          - weight: 100
            preference:
              matchExpressions:
              - key: node-type
                operator: In
                values:
                - game-optimized
        
        # [SEQUENCE: 40] Pod anti-affinity for availability
        podAntiAffinity:
          preferredDuringSchedulingIgnoredDuringExecution:
          - weight: 100
            podAffinityTerm:
              labelSelector:
                matchExpressions:
                - key: app
                  operator: In
                  values:
                  - game-server
              topologyKey: kubernetes.io/hostname
      
      containers:
      - name: game-server
        image: mmorpg/game-server:v2.1.0
        env:
        - name: REGION
          value: "us-west"
        - name: LATENCY_THRESHOLD_MS
          value: "50"
        # ... rest of container spec
```

---

## 🔍 Monitoring and Observability

### Prometheus and Grafana Setup

**Complete Monitoring Stack:**
```yaml
# [SEQUENCE: 41] Prometheus configuration for game metrics
apiVersion: v1
kind: ConfigMap
metadata:
  name: prometheus-config
  namespace: monitoring
data:
  prometheus.yml: |
    global:
      scrape_interval: 15s
      evaluation_interval: 15s
    
    # [SEQUENCE: 42] Game server specific scrape configs
    scrape_configs:
    - job_name: 'game-servers'
      kubernetes_sd_configs:
      - role: pod
        namespaces:
          names:
          - mmorpg-game
      relabel_configs:
      - source_labels: [__meta_kubernetes_pod_annotation_prometheus_io_scrape]
        action: keep
        regex: true
      - source_labels: [__meta_kubernetes_pod_annotation_prometheus_io_path]
        action: replace
        target_label: __metrics_path__
        regex: (.+)
      - source_labels: [__address__, __meta_kubernetes_pod_annotation_prometheus_io_port]
        action: replace
        regex: ([^:]+)(?::\d+)?;(\d+)
        replacement: $1:$2
        target_label: __address__
    
    # [SEQUENCE: 43] Database monitoring
    - job_name: 'mysql-exporter'
      static_configs:
      - targets: ['mysql-exporter:9104']
    
    - job_name: 'redis-exporter'
      static_configs:
      - targets: ['redis-exporter:9121']
    
    # [SEQUENCE: 44] Node and cluster metrics
    - job_name: 'node-exporter'
      kubernetes_sd_configs:
      - role: node
      relabel_configs:
      - action: labelmap
        regex: __meta_kubernetes_node_label_(.+)
    
    - job_name: 'kube-state-metrics'
      static_configs:
      - targets: ['kube-state-metrics:8080']
    
    # [SEQUENCE: 45] Alert rules for game servers
    rule_files:
    - "/etc/prometheus/rules/*.yml"

---
# [SEQUENCE: 46] Game server alert rules
apiVersion: v1
kind: ConfigMap
metadata:
  name: game-server-alerts
  namespace: monitoring
data:
  game-server-rules.yml: |
    groups:
    - name: game-server.rules
      rules:
      # [SEQUENCE: 47] High player load alert
      - alert: HighPlayerLoad
        expr: game_server_active_players > 1800
        for: 2m
        labels:
          severity: warning
        annotations:
          summary: "Game server {{ $labels.pod }} has high player load"
          description: "Pod {{ $labels.pod }} has {{ $value }} active players (threshold: 1800)"
      
      # [SEQUENCE: 48] High response time alert
      - alert: HighResponseTime
        expr: game_server_response_time_p99 > 200
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "Game server {{ $labels.pod }} has high response time"
          description: "Pod {{ $labels.pod }} P99 response time is {{ $value }}ms (threshold: 200ms)"
      
      # [SEQUENCE: 49] Database connection issues
      - alert: DatabaseConnectionFailure
        expr: increase(game_server_database_connection_errors_total[5m]) > 10
        for: 30s
        labels:
          severity: critical
        annotations:
          summary: "Database connection issues on {{ $labels.pod }}"
          description: "{{ $value }} database connection errors in the last 5 minutes"
      
      # [SEQUENCE: 50] Memory usage alert
      - alert: HighMemoryUsage
        expr: (container_memory_usage_bytes{pod=~"game-server-.*"} / container_spec_memory_limit_bytes) > 0.9
        for: 2m
        labels:
          severity: warning
        annotations:
          summary: "High memory usage on {{ $labels.pod }}"
          description: "Memory usage is {{ $value | humanizePercentage }} of limit"
```

### Distributed Tracing with Jaeger

**Jaeger Deployment for Game Server Tracing:**
```yaml
# [SEQUENCE: 51] Jaeger all-in-one deployment
apiVersion: apps/v1
kind: Deployment
metadata:
  name: jaeger
  namespace: monitoring
spec:
  replicas: 1
  selector:
    matchLabels:
      app: jaeger
  template:
    metadata:
      labels:
        app: jaeger
    spec:
      containers:
      - name: jaeger
        image: jaegertracing/all-in-one:1.35
        env:
        - name: COLLECTOR_OTLP_ENABLED
          value: "true"
        - name: SPAN_STORAGE_TYPE
          value: "elasticsearch"
        - name: ES_SERVER_URLS
          value: "http://elasticsearch:9200"
        
        ports:
        - containerPort: 16686
          name: ui
        - containerPort: 14268
          name: collector
        - containerPort: 4317
          name: otlp-grpc
        - containerPort: 4318
          name: otlp-http
        
        # [SEQUENCE: 52] Resource limits for tracing
        resources:
          requests:
            memory: "256Mi"
            cpu: "100m"
          limits:
            memory: "512Mi"
            cpu: "200m"

---
# [SEQUENCE: 53] Game server tracing configuration
apiVersion: v1
kind: ConfigMap
metadata:
  name: tracing-config
  namespace: mmorpg-game
data:
  tracing.yaml: |
    tracing:
      enabled: true
      service_name: "game-server"
      jaeger:
        endpoint: "http://jaeger-collector:14268/api/traces"
        sampler:
          type: "probabilistic"
          param: 0.1  # Sample 10% of traces
      
      # [SEQUENCE: 54] Game-specific trace tags
      tags:
        - key: "game.region"
          value: "${REGION}"
        - key: "game.world_id"
          value: "${WORLD_ID}"
        - key: "service.version"
          value: "${BUILD_VERSION}"
```

---

## 🚀 CI/CD and GitOps

### GitOps with ArgoCD

**`argocd/game-server-app.yaml` - Application Definition:**
```yaml
# [SEQUENCE: 55] ArgoCD application for game server
apiVersion: argoproj.io/v1alpha1
kind: Application
metadata:
  name: mmorpg-game-server
  namespace: argocd
  finalizers:
    - resources-finalizer.argocd.argoproj.io
spec:
  project: default
  
  # [SEQUENCE: 56] Source configuration
  source:
    repoURL: https://github.com/your-org/mmorpg-server-k8s
    targetRevision: HEAD
    path: k8s/overlays/production
    
    # [SEQUENCE: 57] Kustomize configuration
    kustomize:
      namePrefix: prod-
      commonLabels:
        environment: production
        managed-by: argocd
      
      # [SEQUENCE: 58] Image tag overrides
      images:
      - name: mmorpg/game-server
        newTag: v2.1.0
      - name: mmorpg/auth-service
        newTag: v1.3.0
  
  # [SEQUENCE: 59] Destination cluster
  destination:
    server: https://kubernetes.default.svc
    namespace: mmorpg-game
  
  # [SEQUENCE: 60] Sync policy
  syncPolicy:
    automated:
      prune: true
      selfHeal: true
    syncOptions:
    - CreateNamespace=true
    - ApplyOutOfSyncOnly=true
    
    # [SEQUENCE: 61] Retry configuration
    retry:
      limit: 5
      backoff:
        duration: 5s
        factor: 2
        maxDuration: 3m
  
  # [SEQUENCE: 62] Health check configuration
  ignoreDifferences:
  - group: apps
    kind: Deployment
    jsonPointers:
    - /spec/replicas  # Ignore HPA-managed replica count

---
# [SEQUENCE: 63] Multi-environment promotion
apiVersion: argoproj.io/v1alpha1
kind: AppProject
metadata:
  name: mmorpg-game
  namespace: argocd
spec:
  description: MMORPG Game Server Project
  
  # [SEQUENCE: 64] Source repositories
  sourceRepos:
  - 'https://github.com/your-org/mmorpg-server-k8s'
  - 'https://helm.releases.hashicorp.com'  # For Vault, Consul
  
  # [SEQUENCE: 65] Destination clusters and namespaces
  destinations:
  - namespace: mmorpg-game-dev
    server: https://dev-cluster.example.com
  - namespace: mmorpg-game-staging
    server: https://staging-cluster.example.com
  - namespace: mmorpg-game
    server: https://prod-cluster.example.com
  
  # [SEQUENCE: 66] RBAC configuration
  roles:
  - name: developers
    description: Developer access
    policies:
    - p, proj:mmorpg-game:developers, applications, get, mmorpg-game/*, allow
    - p, proj:mmorpg-game:developers, applications, sync, mmorpg-game/*-dev, allow
    groups:
    - your-org:developers
  
  - name: operators
    description: Production operators
    policies:
    - p, proj:mmorpg-game:operators, applications, *, mmorpg-game/*, allow
    groups:
    - your-org:sre-team
```

---

## 🔒 Security and Network Policies

### Network Policies for Game Security

**`k8s/security/network-policies.yaml` - Implementation:**
```yaml
# [SEQUENCE: 67] Default deny all ingress traffic
apiVersion: networking.k8s.io/v1
kind: NetworkPolicy
metadata:
  name: default-deny-ingress
  namespace: mmorpg-game
spec:
  podSelector: {}
  policyTypes:
  - Ingress

---
# [SEQUENCE: 68] Game server network policy
apiVersion: networking.k8s.io/v1
kind: NetworkPolicy
metadata:
  name: game-server-network-policy
  namespace: mmorpg-game
spec:
  podSelector:
    matchLabels:
      app: game-server
  
  policyTypes:
  - Ingress
  - Egress
  
  # [SEQUENCE: 69] Allowed ingress traffic
  ingress:
  # Allow traffic from ingress controller
  - from:
    - namespaceSelector:
        matchLabels:
          name: ingress-nginx
    ports:
    - protocol: TCP
      port: 8080
  
  # Allow traffic from other game servers (for clustering)
  - from:
    - podSelector:
        matchLabels:
          app: game-server
    ports:
    - protocol: TCP
      port: 8080
    - protocol: TCP
      port: 9090  # Metrics
  
  # Allow monitoring traffic
  - from:
    - namespaceSelector:
        matchLabels:
          name: monitoring
    ports:
    - protocol: TCP
      port: 9090
  
  # [SEQUENCE: 70] Allowed egress traffic
  egress:
  # Allow DNS resolution
  - to: []
    ports:
    - protocol: UDP
      port: 53
  
  # Allow database connections
  - to:
    - podSelector:
        matchLabels:
          app: mysql
    ports:
    - protocol: TCP
      port: 3306
  
  - to:
    - podSelector:
        matchLabels:
          app: redis-cluster
    ports:
    - protocol: TCP
      port: 6379
  
  # Allow external API calls (authentication, payment processing)
  - to: []
    ports:
    - protocol: TCP
      port: 443
    - protocol: TCP
      port: 80

---
# [SEQUENCE: 71] Database network isolation
apiVersion: networking.k8s.io/v1
kind: NetworkPolicy
metadata:
  name: database-network-policy
  namespace: mmorpg-game
spec:
  podSelector:
    matchLabels:
      component: database
  
  policyTypes:
  - Ingress
  
  ingress:
  # Only allow connections from game servers
  - from:
    - podSelector:
        matchLabels:
          app: game-server
    - podSelector:
        matchLabels:
          app: auth-service
    - podSelector:
        matchLabels:
          app: player-service
    ports:
    - protocol: TCP
      port: 3306  # MySQL
    - protocol: TCP
      port: 6379  # Redis
  
  # Allow monitoring
  - from:
    - namespaceSelector:
        matchLabels:
          name: monitoring
    ports:
    - protocol: TCP
      port: 9104  # MySQL exporter
    - protocol: TCP
      port: 9121  # Redis exporter
```

---

## 📊 Performance Optimization

### Resource Quotas and Limits

**Cluster Resource Management:**
```yaml
# [SEQUENCE: 72] Namespace resource quota
apiVersion: v1
kind: ResourceQuota
metadata:
  name: mmorpg-game-quota
  namespace: mmorpg-game
spec:
  hard:
    # [SEQUENCE: 73] Compute resources
    requests.cpu: "50"      # 50 CPU cores
    requests.memory: 100Gi  # 100GB RAM
    limits.cpu: "100"       # 100 CPU cores max
    limits.memory: 200Gi    # 200GB RAM max
    
    # [SEQUENCE: 74] Storage resources
    requests.storage: 1Ti   # 1TB storage
    persistentvolumeclaims: "20"
    
    # [SEQUENCE: 75] Object limits
    pods: "200"
    services: "50"
    secrets: "100"
    configmaps: "100"
    
    # [SEQUENCE: 76] Load balancer limits
    services.loadbalancers: "10"
    services.nodeports: "20"

---
# [SEQUENCE: 77] Limit ranges for pods
apiVersion: v1
kind: LimitRange
metadata:
  name: mmorpg-game-limits
  namespace: mmorpg-game
spec:
  limits:
  # [SEQUENCE: 78] Container limits
  - type: Container
    default:
      cpu: "500m"
      memory: "1Gi"
    defaultRequest:
      cpu: "100m"
      memory: "256Mi"
    max:
      cpu: "4"
      memory: "8Gi"
    min:
      cpu: "50m"
      memory: "128Mi"
  
  # [SEQUENCE: 79] Pod limits
  - type: Pod
    max:
      cpu: "8"
      memory: "16Gi"
    min:
      cpu: "100m"
      memory: "256Mi"
  
  # [SEQUENCE: 80] PVC limits
  - type: PersistentVolumeClaim
    max:
      storage: 100Gi
    min:
      storage: 1Gi
```

---

## 🎯 Best Practices Summary

### 1. Pod Design Patterns
```yaml
# ✅ Good: Proper resource requests and limits
resources:
  requests:
    memory: "1Gi"    # What the pod needs
    cpu: "500m"
  limits:
    memory: "2Gi"    # Maximum the pod can use
    cpu: "1000m"

# ❌ Avoid: No resource specifications
resources: {}  # This can cause resource contention
```

### 2. Health Checks
```yaml
# ✅ Good: Comprehensive health checks
livenessProbe:
  httpGet:
    path: /health/live
    port: 8080
  initialDelaySeconds: 30
  periodSeconds: 10

readinessProbe:
  httpGet:
    path: /health/ready
    port: 8080
  initialDelaySeconds: 10
  periodSeconds: 5

# ❌ Avoid: No health checks (pods may receive traffic when not ready)
```

### 3. Security Best Practices
```yaml
# ✅ Good: Security context with non-root user
securityContext:
  runAsUser: 1000
  runAsGroup: 1000
  runAsNonRoot: true
  fsGroup: 1000
  capabilities:
    drop:
    - ALL

# ❌ Avoid: Running as root user
securityContext:
  runAsUser: 0  # Root user is a security risk
```

---

## 🔗 Integration Points

This guide complements:
- **microservices_architecture.md**: Kubernetes orchestrates microservices deployment
- **message_queue_systems.md**: Message queues run as Kubernetes services
- **nosql_integration_guide.md**: Databases deployed with StatefulSets

**Implementation Roadmap:**
1. **Week 1-2**: Basic Kubernetes setup with single game server deployment
2. **Week 3-4**: Add auto-scaling, monitoring, and database StatefulSets
3. **Week 5-6**: Implement security policies, network policies, and GitOps
4. **Week 7-8**: Multi-region deployment and advanced observability

---

## 🔥 흔한 실수와 해결방법 (Common Mistakes & Solutions)

### 1. 리소스 제한 미설정
```yaml
# [SEQUENCE: 1] ❌ 잘못된 예: 리소스 제한 없음
apiVersion: apps/v1
kind: Deployment
metadata:
  name: game-server
spec:
  template:
    spec:
      containers:
      - name: game-server
        image: mmorpg/game-server:latest
        # 리소스 제한 없음 - 노드 전체를 사용할 수 있음!

# ✅ 올바른 예: 적절한 리소스 설정
apiVersion: apps/v1
kind: Deployment
metadata:
  name: game-server
spec:
  template:
    spec:
      containers:
      - name: game-server
        image: mmorpg/game-server:latest
        resources:
          requests:
            memory: "2Gi"
            cpu: "1000m"
          limits:
            memory: "4Gi"
            cpu: "2000m"
```

### 2. 상태 저장 서비스를 Deployment로 배포
```yaml
# [SEQUENCE: 2] ❌ 잘못된 예: 게임 월드를 Deployment로 배포
apiVersion: apps/v1
kind: Deployment
metadata:
  name: game-world
spec:
  replicas: 3
  # Pod가 재시작되면 다른 노드로 이동하고 데이터 손실!

# ✅ 올바른 예: StatefulSet 사용
apiVersion: apps/v1
kind: StatefulSet
metadata:
  name: game-world
spec:
  replicas: 3
  serviceName: game-world-service
  volumeClaimTemplates:
  - metadata:
      name: world-data
    spec:
      accessModes: ["ReadWriteOnce"]
      resources:
        requests:
          storage: 100Gi
```

### 3. 잘못된 헬스체크 설정
```yaml
# [SEQUENCE: 3] ❌ 잘못된 예: 너무 공격적인 헬스체크
livenessProbe:
  httpGet:
    path: /health
    port: 8080
  initialDelaySeconds: 5   # 너무 짧음!
  periodSeconds: 5
  failureThreshold: 1      # 한 번만 실패해도 재시작!

# ✅ 올바른 예: 적절한 헬스체크
livenessProbe:
  httpGet:
    path: /health/live
    port: 8080
  initialDelaySeconds: 60  # 충분한 시작 시간
  periodSeconds: 10
  failureThreshold: 3      # 3번 연속 실패 시 재시작
  
readinessProbe:
  httpGet:
    path: /health/ready
    port: 8080
  initialDelaySeconds: 30
  periodSeconds: 5
  successThreshold: 2      # 2번 연속 성공 시 트래픽 받기
```

---

## 🚀 실습 프로젝트 (Practice Projects)

### 📚 기초 프로젝트: 간단한 게임 서버 배포
**목표**: 기본적인 Kubernetes 배포 및 서비스 구성

```yaml
# 구현 요구사항:
# 1. 게임 서버 Deployment (3 replicas)
# 2. LoadBalancer Service
# 3. ConfigMap으로 설정 관리
# 4. 기본 HPA 설정
# 5. Prometheus 모니터링
# 6. 로그 수집 (Fluentd)
```

### 🎮 중급 프로젝트: 스테이트풀 게임 월드
**목표**: StatefulSet을 활용한 게임 월드 관리

```yaml
# 핵심 기능:
# 1. StatefulSet 게임 월드 (5 instances)
# 2. Persistent Volume으로 월드 데이터 저장
# 3. Headless Service로 직접 연결
# 4. 월드 간 통신 메시 구성
# 5. 자동 백업 CronJob
# 6. 무중단 업데이트 전략
```

### 🏆 고급 프로젝트: 멀티 리전 게임 서버
**목표**: 글로벌 서비스를 위한 멀티 클러스터 배포

```yaml
# 구현 내용:
# 1. 멀티 리전 클러스터 (3 regions)
# 2. Federation으로 클러스터 관리
# 3. 글로벌 로드 밸런싱
# 4. 크로스 리전 데이터 동기화
# 5. 재해 복구 시스템
# 6. 비용 최적화 (Spot instances)
# 7. GitOps 기반 배포
```

---

## 📊 학습 체크리스트 (Learning Checklist)

### 🥉 브론즈 레벨
- [ ] Pod, Service 기본 개념
- [ ] kubectl 기본 명령어
- [ ] 간단한 Deployment 작성
- [ ] 기본 네트워킹 이해

### 🥈 실버 레벨
- [ ] StatefulSet 활용
- [ ] PV/PVC 관리
- [ ] HPA/VPA 설정
- [ ] Helm 차트 작성

### 🥇 골드 레벨
- [ ] 복잡한 네트워크 정책
- [ ] 고급 스케줄링
- [ ] Operator 패턴
- [ ] 서비스 메시 구현

### 💎 플래티넘 레벨
- [ ] 멀티 클러스터 관리
- [ ] 커스텀 컨트롤러 개발
- [ ] 성능 최적화
- [ ] 프로덕션 운영 자동화

---

## 📚 추가 학습 자료 (Additional Resources)

### 필독서
- 📖 "Kubernetes in Action" - Marko Lukša
- 📖 "The Kubernetes Book" - Nigel Poulton
- 📖 "Production Kubernetes" - Josh Rosso

### 온라인 강의
- 🎓 CNCF Kubernetes Course
- 🎓 Linux Foundation CKA/CKAD
- 🎓 Google Cloud Kubernetes Course

### 공식 문서
- 📄 Kubernetes Official Documentation
- 📄 Helm Documentation
- 📄 Prometheus Operator Guide
- 📄 Istio Service Mesh

### 유용한 도구
- 🔧 k9s: Terminal UI for Kubernetes
- 🔧 Lens: Kubernetes IDE
- 🔧 Kustomize: Configuration management
- 🔧 ArgoCD: GitOps continuous delivery

*💡 Key Insight: Kubernetes for game servers requires special attention to stateful workloads, low-latency networking, and predictable resource allocation. Unlike typical web applications, game servers need consistent performance and careful pod lifecycle management to maintain player experience.*