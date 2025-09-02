# 05. Monitoring Setup - 모니터링 시스템 구축 가이드

## 🎯 목표
Prometheus, Grafana, Jaeger를 사용하여 게임 서버의 성능, 가용성, 에러를 실시간으로 모니터링하는 시스템을 구축합니다.

## 📋 Prerequisites
- Docker 및 Docker Compose
- Kubernetes 클러스터 (선택사항)
- 최소 4GB RAM (모니터링 스택용)
- 시계열 데이터베이스 저장 공간 (50GB+)

---

## 1. 모니터링 아키텍처

### 1.1 전체 구조
```
┌─────────────────────────────────────────────────┐
│                   Grafana                       │
│         (Visualization & Dashboards)            │
└─────────────┬──────────┬──────────┬────────────┘
              │          │          │
    ┌─────────▼────┐ ┌──▼────┐ ┌──▼──────┐
    │  Prometheus  │ │ Loki  │ │ Jaeger  │
    │   (Metrics)  │ │(Logs) │ │(Traces) │
    └──────┬───────┘ └───┬───┘ └────┬────┘
           │             │           │
    ┌──────▼─────────────▼───────────▼────┐
    │         Game Server Instances        │
    │  (Instrumented with Exporters)       │
    └──────────────────────────────────────┘
```

### 1.2 메트릭 수집 전략
- **Infrastructure Metrics**: Node Exporter
- **Application Metrics**: Custom Prometheus metrics
- **Database Metrics**: MySQL/Redis exporters
- **Network Metrics**: Blackbox exporter
- **Logs**: Loki + Promtail
- **Traces**: Jaeger + OpenTelemetry

---

## 2. Prometheus 설정

### 2.1 Docker Compose 구성
**docker-compose.monitoring.yml**
```yaml
version: '3.8'

services:
  prometheus:
    image: prom/prometheus:latest
    container_name: prometheus
    command:
      - '--config.file=/etc/prometheus/prometheus.yml'
      - '--storage.tsdb.path=/prometheus'
      - '--storage.tsdb.retention.time=30d'
      - '--web.enable-lifecycle'
      - '--web.enable-admin-api'
    volumes:
      - ./prometheus/prometheus.yml:/etc/prometheus/prometheus.yml
      - ./prometheus/alerts.yml:/etc/prometheus/alerts.yml
      - prometheus-data:/prometheus
    ports:
      - "9090:9090"
    networks:
      - monitoring
    restart: unless-stopped

  grafana:
    image: grafana/grafana:latest
    container_name: grafana
    environment:
      - GF_SECURITY_ADMIN_USER=admin
      - GF_SECURITY_ADMIN_PASSWORD=${GRAFANA_PASSWORD:-admin123}
      - GF_INSTALL_PLUGINS=grafana-piechart-panel,grafana-worldmap-panel
    volumes:
      - ./grafana/provisioning:/etc/grafana/provisioning
      - ./grafana/dashboards:/var/lib/grafana/dashboards
      - grafana-data:/var/lib/grafana
    ports:
      - "3000:3000"
    networks:
      - monitoring
    restart: unless-stopped

  jaeger:
    image: jaegertracing/all-in-one:latest
    container_name: jaeger
    environment:
      - COLLECTOR_ZIPKIN_HOST_PORT=:9411
      - COLLECTOR_OTLP_ENABLED=true
    ports:
      - "6831:6831/udp"  # Jaeger agent
      - "16686:16686"     # Jaeger UI
      - "14268:14268"     # Jaeger collector
      - "4317:4317"       # OTLP gRPC
      - "4318:4318"       # OTLP HTTP
    networks:
      - monitoring
    restart: unless-stopped

  loki:
    image: grafana/loki:latest
    container_name: loki
    command: -config.file=/etc/loki/local-config.yaml
    volumes:
      - ./loki/loki-config.yaml:/etc/loki/local-config.yaml
      - loki-data:/loki
    ports:
      - "3100:3100"
    networks:
      - monitoring
    restart: unless-stopped

  promtail:
    image: grafana/promtail:latest
    container_name: promtail
    volumes:
      - ./promtail/promtail-config.yaml:/etc/promtail/config.yml
      - /var/log:/var/log:ro
      - /var/lib/docker/containers:/var/lib/docker/containers:ro
    command: -config.file=/etc/promtail/config.yml
    networks:
      - monitoring
    restart: unless-stopped

  alertmanager:
    image: prom/alertmanager:latest
    container_name: alertmanager
    volumes:
      - ./alertmanager/alertmanager.yml:/etc/alertmanager/alertmanager.yml
      - alertmanager-data:/alertmanager
    command:
      - '--config.file=/etc/alertmanager/alertmanager.yml'
      - '--storage.path=/alertmanager'
    ports:
      - "9093:9093"
    networks:
      - monitoring
    restart: unless-stopped

  # Exporters
  node-exporter:
    image: prom/node-exporter:latest
    container_name: node-exporter
    volumes:
      - /proc:/host/proc:ro
      - /sys:/host/sys:ro
      - /:/rootfs:ro
    command:
      - '--path.procfs=/host/proc'
      - '--path.sysfs=/host/sys'
      - '--collector.filesystem.mount-points-exclude=^/(sys|proc|dev|host|etc)($$|/)'
    ports:
      - "9100:9100"
    networks:
      - monitoring
    restart: unless-stopped

  mysql-exporter:
    image: prom/mysqld-exporter:latest
    container_name: mysql-exporter
    environment:
      - DATA_SOURCE_NAME=exporter:password@(mysql:3306)/
    ports:
      - "9104:9104"
    networks:
      - monitoring
    restart: unless-stopped

  redis-exporter:
    image: oliver006/redis_exporter:latest
    container_name: redis-exporter
    environment:
      - REDIS_ADDR=redis:6379
      - REDIS_PASSWORD=${REDIS_PASSWORD}
    ports:
      - "9121:9121"
    networks:
      - monitoring
    restart: unless-stopped

networks:
  monitoring:
    driver: bridge

volumes:
  prometheus-data:
  grafana-data:
  loki-data:
  alertmanager-data:
```

### 2.2 Prometheus 설정 파일
**prometheus/prometheus.yml**
```yaml
global:
  scrape_interval: 15s
  evaluation_interval: 15s
  external_labels:
    cluster: 'game-production'
    environment: 'prod'

alerting:
  alertmanagers:
    - static_configs:
        - targets: ['alertmanager:9093']

rule_files:
  - '/etc/prometheus/alerts.yml'

scrape_configs:
  # Game Server Metrics
  - job_name: 'game-servers'
    static_configs:
      - targets: ['game-server-1:9090', 'game-server-2:9090', 'game-server-3:9090']
    relabel_configs:
      - source_labels: [__address__]
        target_label: instance
        regex: '([^:]+):.*'
        replacement: '${1}'

  # Node Metrics
  - job_name: 'node'
    static_configs:
      - targets: ['node-exporter:9100']

  # MySQL Metrics
  - job_name: 'mysql'
    static_configs:
      - targets: ['mysql-exporter:9104']

  # Redis Metrics
  - job_name: 'redis'
    static_configs:
      - targets: ['redis-exporter:9121']

  # Kubernetes Service Discovery
  - job_name: 'kubernetes-pods'
    kubernetes_sd_configs:
      - role: pod
    relabel_configs:
      - source_labels: [__meta_kubernetes_pod_annotation_prometheus_io_scrape]
        action: keep
        regex: true
      - source_labels: [__meta_kubernetes_pod_annotation_prometheus_io_path]
        action: replace
        target_label: __metrics_path__
        regex: (.+)
```

---

## 3. 게임 서버 메트릭 구현

### 3.1 C++ Prometheus 메트릭
**src/monitoring/metrics.cpp**
```cpp
#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>

class GameMetrics {
private:
    std::shared_ptr<prometheus::Registry> registry_;
    prometheus::Exposer exposer_;
    
    // Counters
    prometheus::Family<prometheus::Counter>& player_logins_;
    prometheus::Family<prometheus::Counter>& combat_events_;
    prometheus::Family<prometheus::Counter>& errors_;
    
    // Gauges
    prometheus::Family<prometheus::Gauge>& active_players_;
    prometheus::Family<prometheus::Gauge>& memory_usage_;
    prometheus::Family<prometheus::Gauge>& tick_rate_;
    
    // Histograms
    prometheus::Family<prometheus::Histogram>& request_duration_;
    prometheus::Family<prometheus::Histogram>& db_query_duration_;
    
public:
    GameMetrics(const std::string& bind_address = "0.0.0.0:9090")
        : registry_(std::make_shared<prometheus::Registry>())
        , exposer_(bind_address)
        , player_logins_(prometheus::BuildCounter()
            .Name("game_player_logins_total")
            .Help("Total number of player logins")
            .Register(*registry_))
        , combat_events_(prometheus::BuildCounter()
            .Name("game_combat_events_total")
            .Help("Total number of combat events")
            .Register(*registry_))
        , errors_(prometheus::BuildCounter()
            .Name("game_errors_total")
            .Help("Total number of errors")
            .Register(*registry_))
        , active_players_(prometheus::BuildGauge()
            .Name("game_active_players")
            .Help("Number of currently active players")
            .Register(*registry_))
        , memory_usage_(prometheus::BuildGauge()
            .Name("game_memory_usage_bytes")
            .Help("Current memory usage in bytes")
            .Register(*registry_))
        , tick_rate_(prometheus::BuildGauge()
            .Name("game_tick_rate_fps")
            .Help("Current server tick rate")
            .Register(*registry_))
        , request_duration_(prometheus::BuildHistogram()
            .Name("game_request_duration_seconds")
            .Help("Request duration in seconds")
            .Buckets({0.001, 0.005, 0.01, 0.05, 0.1, 0.5, 1.0})
            .Register(*registry_))
        , db_query_duration_(prometheus::BuildHistogram()
            .Name("game_db_query_duration_seconds")
            .Help("Database query duration in seconds")
            .Buckets({0.001, 0.01, 0.1, 1.0, 10.0})
            .Register(*registry_))
    {
        exposer_.RegisterCollectable(registry_);
    }
    
    void RecordPlayerLogin(const std::string& zone) {
        player_logins_.Add({{"zone", zone}}).Increment();
        active_players_.Add({{"zone", zone}}).Increment();
    }
    
    void RecordPlayerLogout(const std::string& zone) {
        active_players_.Add({{"zone", zone}}).Decrement();
    }
    
    void RecordCombatEvent(const std::string& type) {
        combat_events_.Add({{"type", type}}).Increment();
    }
    
    void RecordError(const std::string& category, const std::string& error) {
        errors_.Add({{"category", category}, {"error", error}}).Increment();
    }
    
    void UpdateTickRate(double fps) {
        tick_rate_.Add({}).Set(fps);
    }
    
    void UpdateMemoryUsage(size_t bytes) {
        memory_usage_.Add({}).Set(static_cast<double>(bytes));
    }
    
    class TimerScope {
        prometheus::Histogram& histogram_;
        std::chrono::steady_clock::time_point start_;
        
    public:
        TimerScope(prometheus::Histogram& hist) 
            : histogram_(hist)
            , start_(std::chrono::steady_clock::now()) {}
        
        ~TimerScope() {
            auto duration = std::chrono::steady_clock::now() - start_;
            auto seconds = std::chrono::duration<double>(duration).count();
            histogram_.Observe(seconds);
        }
    };
    
    auto TimeRequest(const std::string& endpoint) {
        return TimerScope(request_duration_.Add({{"endpoint", endpoint}}));
    }
    
    auto TimeDBQuery(const std::string& query_type) {
        return TimerScope(db_query_duration_.Add({{"type", query_type}}));
    }
};

// 사용 예시
void HandlePlayerLogin(const Player& player, GameMetrics& metrics) {
    auto timer = metrics.TimeRequest("/login");
    
    // 로그인 처리
    if (AuthenticatePlayer(player)) {
        metrics.RecordPlayerLogin(player.zone);
        // ...
    } else {
        metrics.RecordError("auth", "login_failed");
    }
}
```

---

## 4. Grafana 대시보드

### 4.1 게임 서버 대시보드
**grafana/dashboards/game-server.json**
```json
{
  "dashboard": {
    "title": "Game Server Monitoring",
    "panels": [
      {
        "title": "Active Players",
        "targets": [{
          "expr": "sum(game_active_players) by (zone)"
        }],
        "type": "graph"
      },
      {
        "title": "Server Tick Rate",
        "targets": [{
          "expr": "game_tick_rate_fps"
        }],
        "type": "gauge"
      },
      {
        "title": "Request Latency (P95)",
        "targets": [{
          "expr": "histogram_quantile(0.95, rate(game_request_duration_seconds_bucket[5m]))"
        }],
        "type": "graph"
      },
      {
        "title": "Combat Events/sec",
        "targets": [{
          "expr": "rate(game_combat_events_total[1m])"
        }],
        "type": "graph"
      },
      {
        "title": "Memory Usage",
        "targets": [{
          "expr": "game_memory_usage_bytes / 1024 / 1024 / 1024"
        }],
        "type": "graph",
        "yaxes": [{
          "format": "GB"
        }]
      },
      {
        "title": "Error Rate",
        "targets": [{
          "expr": "rate(game_errors_total[5m])"
        }],
        "type": "graph"
      }
    ]
  }
}
```

### 4.2 자동 프로비저닝
**grafana/provisioning/dashboards/dashboard.yml**
```yaml
apiVersion: 1

providers:
  - name: 'Game Server Dashboards'
    orgId: 1
    folder: ''
    type: file
    disableDeletion: false
    updateIntervalSeconds: 10
    allowUiUpdates: false
    options:
      path: /var/lib/grafana/dashboards
```

---

## 5. 알림 설정

### 5.1 Prometheus 알림 규칙
**prometheus/alerts.yml**
```yaml
groups:
  - name: game_server
    interval: 30s
    rules:
      # 서버 다운
      - alert: GameServerDown
        expr: up{job="game-servers"} == 0
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "Game server {{ $labels.instance }} is down"
          description: "Game server has been down for more than 1 minute"
      
      # 높은 메모리 사용
      - alert: HighMemoryUsage
        expr: (game_memory_usage_bytes / (16 * 1024 * 1024 * 1024)) > 0.9
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High memory usage on {{ $labels.instance }}"
          description: "Memory usage is above 90% ({{ $value | humanizePercentage }})"
      
      # 낮은 틱 레이트
      - alert: LowTickRate
        expr: game_tick_rate_fps < 25
        for: 2m
        labels:
          severity: warning
        annotations:
          summary: "Low tick rate on {{ $labels.instance }}"
          description: "Server tick rate is {{ $value }} FPS (expected: 30)"
      
      # 높은 에러율
      - alert: HighErrorRate
        expr: rate(game_errors_total[5m]) > 10
        for: 5m
        labels:
          severity: warning
        annotations:
          summary: "High error rate"
          description: "Error rate is {{ $value }} errors/sec"
      
      # DB 연결 문제
      - alert: DatabaseConnectionFailure
        expr: mysql_up == 0
        for: 1m
        labels:
          severity: critical
        annotations:
          summary: "MySQL database is down"
          description: "Cannot connect to MySQL database"
      
      # 플레이어 수 급증
      - alert: PlayerSurge
        expr: rate(game_active_players[5m]) > 100
        for: 2m
        labels:
          severity: info
        annotations:
          summary: "Rapid increase in player count"
          description: "Player count increasing at {{ $value }} players/sec"
```

### 5.2 AlertManager 설정
**alertmanager/alertmanager.yml**
```yaml
global:
  resolve_timeout: 5m
  slack_api_url: '${SLACK_WEBHOOK_URL}'

route:
  group_by: ['alertname', 'cluster', 'service']
  group_wait: 10s
  group_interval: 5m
  repeat_interval: 12h
  receiver: 'default'
  
  routes:
    - match:
        severity: critical
      receiver: 'critical'
      continue: true
    
    - match:
        severity: warning
      receiver: 'warning'

receivers:
  - name: 'default'
    slack_configs:
      - channel: '#game-alerts'
        title: 'Game Server Alert'
        text: '{{ range .Alerts }}{{ .Annotations.summary }}\n{{ end }}'
  
  - name: 'critical'
    slack_configs:
      - channel: '#game-critical'
        title: '🚨 CRITICAL Alert'
        color: 'danger'
    pagerduty_configs:
      - service_key: '${PAGERDUTY_SERVICE_KEY}'
  
  - name: 'warning'
    slack_configs:
      - channel: '#game-warnings'
        title: '⚠️ Warning'
        color: 'warning'

inhibit_rules:
  - source_match:
      severity: 'critical'
    target_match:
      severity: 'warning'
    equal: ['alertname', 'cluster', 'service']
```

---

## 6. 로그 수집 (Loki)

### 6.1 Loki 설정
**loki/loki-config.yaml**
```yaml
auth_enabled: false

server:
  http_listen_port: 3100

ingester:
  lifecycler:
    address: 127.0.0.1
    ring:
      kvstore:
        store: inmemory
      replication_factor: 1
    final_sleep: 0s
  chunk_idle_period: 5m
  chunk_retain_period: 30s

schema_config:
  configs:
    - from: 2020-10-24
      store: boltdb
      object_store: filesystem
      schema: v11
      index:
        prefix: index_
        period: 168h

storage_config:
  boltdb:
    directory: /loki/index
  filesystem:
    directory: /loki/chunks

limits_config:
  enforce_metric_name: false
  reject_old_samples: true
  reject_old_samples_max_age: 168h
  ingestion_rate_mb: 10
  ingestion_burst_size_mb: 20
```

### 6.2 Promtail 설정
**promtail/promtail-config.yaml**
```yaml
server:
  http_listen_port: 9080
  grpc_listen_port: 0

positions:
  filename: /tmp/positions.yaml

clients:
  - url: http://loki:3100/loki/api/v1/push

scrape_configs:
  - job_name: game_server
    static_configs:
      - targets:
          - localhost
        labels:
          job: game_server
          __path__: /var/log/game-server/*.log
    
    pipeline_stages:
      - regex:
          expression: '^(?P<timestamp>\S+)\s+(?P<level>\S+)\s+\[(?P<component>\S+)\]\s+(?P<message>.*)$'
      
      - labels:
          level:
          component:
      
      - timestamp:
          source: timestamp
          format: RFC3339
```

---

## 7. 분산 추적 (Jaeger)

### 7.1 OpenTelemetry 통합
**src/tracing/tracer.cpp**
```cpp
#include <opentelemetry/trace/provider.h>
#include <opentelemetry/exporters/jaeger/jaeger_exporter.h>

class GameTracer {
private:
    std::shared_ptr<opentelemetry::trace::TracerProvider> provider_;
    std::shared_ptr<opentelemetry::trace::Tracer> tracer_;
    
public:
    GameTracer() {
        auto exporter = std::make_unique<opentelemetry::exporter::jaeger::JaegerExporter>(
            opentelemetry::exporter::jaeger::JaegerExporterOptions{
                .endpoint = "http://jaeger:14268/api/traces",
                .service_name = "game-server"
            }
        );
        
        auto processor = std::make_unique<opentelemetry::sdk::trace::SimpleSpanProcessor>(
            std::move(exporter)
        );
        
        provider_ = std::make_shared<opentelemetry::sdk::trace::TracerProvider>(
            std::move(processor)
        );
        
        opentelemetry::trace::Provider::SetTracerProvider(provider_);
        tracer_ = provider_->GetTracer("game-server", "1.0.0");
    }
    
    auto StartSpan(const std::string& name) {
        return tracer_->StartSpan(name);
    }
    
    auto StartSpan(const std::string& name, 
                   const opentelemetry::trace::SpanContext& parent) {
        opentelemetry::trace::StartSpanOptions options;
        options.parent = parent;
        return tracer_->StartSpan(name, options);
    }
};

// 사용 예시
void ProcessCombat(const CombatRequest& request, GameTracer& tracer) {
    auto span = tracer.StartSpan("ProcessCombat");
    span->SetAttribute("player_id", request.player_id);
    span->SetAttribute("combat_type", request.type);
    
    {
        auto damage_span = tracer.StartSpan("CalculateDamage", span->GetContext());
        // 데미지 계산
    }
    
    {
        auto apply_span = tracer.StartSpan("ApplyEffects", span->GetContext());
        // 효과 적용
    }
    
    span->End();
}
```

---

## ✅ 체크리스트

### 필수 확인사항
- [ ] Prometheus 실행 및 메트릭 수집
- [ ] Grafana 대시보드 구성
- [ ] 알림 규칙 설정
- [ ] Jaeger 추적 활성화
- [ ] 로그 수집 파이프라인 구성
- [ ] 메트릭 익스포터 배포

### 권장사항
- [ ] 커스텀 대시보드 생성
- [ ] SLO/SLI 정의
- [ ] 용량 계획 메트릭
- [ ] 비용 모니터링
- [ ] 보안 이벤트 모니터링

## 🎯 다음 단계
→ [06_logging_system.md](06_logging_system.md) - 로깅 시스템 설정