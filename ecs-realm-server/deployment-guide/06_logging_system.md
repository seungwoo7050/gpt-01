# 06. Logging System - 로깅 시스템 구축 가이드

## 🎯 목표
ELK Stack(Elasticsearch, Logstash, Kibana)을 사용하여 중앙화된 로그 수집, 분석, 시각화 시스템을 구축합니다.

## 📋 Prerequisites
- Kubernetes 클러스터 또는 Docker 환경
- 최소 16GB RAM (ELK Stack용)
- 200GB+ 스토리지 (로그 보관용)
- 로그 수집 에이전트 (Filebeat/Fluentd)

---

## 1. 로깅 아키텍처

### 1.1 전체 구조
```
Game Servers → Filebeat/Fluentd → Logstash → Elasticsearch → Kibana
     ↓                                              ↑
Application Logs                            Query & Visualize
System Logs
Audit Logs
```

### 1.2 로그 레벨 전략
```yaml
log_levels:
  production:
    default: INFO
    database: WARN
    network: INFO
    game_logic: INFO
    security: DEBUG
  
  staging:
    default: DEBUG
    database: DEBUG
    network: DEBUG
    game_logic: DEBUG
    security: DEBUG
```

---

## 2. ELK Stack 설치

### 2.1 Docker Compose 설정
**docker-compose.elk.yml**
```yaml
version: '3.8'

services:
  elasticsearch:
    image: docker.elastic.co/elasticsearch/elasticsearch:8.11.0
    container_name: elasticsearch
    environment:
      - node.name=elasticsearch
      - cluster.name=game-logs-cluster
      - discovery.type=single-node
      - bootstrap.memory_lock=true
      - "ES_JAVA_OPTS=-Xms4g -Xmx4g"
      - xpack.security.enabled=true
      - xpack.security.enrollment.enabled=true
      - ELASTIC_PASSWORD=${ELASTIC_PASSWORD:-elastic123}
    ulimits:
      memlock:
        soft: -1
        hard: -1
      nofile:
        soft: 65536
        hard: 65536
    volumes:
      - elasticsearch-data:/usr/share/elasticsearch/data
    ports:
      - "9200:9200"
      - "9300:9300"
    networks:
      - elk
    healthcheck:
      test: ["CMD-SHELL", "curl -s -X GET http://localhost:9200/_cluster/health?pretty | grep status | grep -q '\\(green\\|yellow\\)'"]
      interval: 30s
      timeout: 10s
      retries: 5

  logstash:
    image: docker.elastic.co/logstash/logstash:8.11.0
    container_name: logstash
    volumes:
      - ./logstash/pipeline:/usr/share/logstash/pipeline:ro
      - ./logstash/config/logstash.yml:/usr/share/logstash/config/logstash.yml:ro
    ports:
      - "5044:5044"  # Beats
      - "5000:5000"  # TCP input
      - "9600:9600"  # Monitoring
      - "12201:12201/udp"  # GELF
    environment:
      - "LS_JAVA_OPTS=-Xms2g -Xmx2g"
      - ELASTIC_PASSWORD=${ELASTIC_PASSWORD:-elastic123}
    networks:
      - elk
    depends_on:
      elasticsearch:
        condition: service_healthy

  kibana:
    image: docker.elastic.co/kibana/kibana:8.11.0
    container_name: kibana
    environment:
      - SERVERNAME=kibana
      - ELASTICSEARCH_HOSTS=http://elasticsearch:9200
      - ELASTICSEARCH_USERNAME=elastic
      - ELASTICSEARCH_PASSWORD=${ELASTIC_PASSWORD:-elastic123}
      - XPACK_SECURITY_ENABLED=true
    ports:
      - "5601:5601"
    networks:
      - elk
    depends_on:
      elasticsearch:
        condition: service_healthy

  filebeat:
    image: docker.elastic.co/beats/filebeat:8.11.0
    container_name: filebeat
    user: root
    volumes:
      - ./filebeat/filebeat.yml:/usr/share/filebeat/filebeat.yml:ro
      - /var/lib/docker/containers:/var/lib/docker/containers:ro
      - /var/run/docker.sock:/var/run/docker.sock:ro
      - /var/log:/var/log:ro
    command: filebeat -e -strict.perms=false
    networks:
      - elk
    depends_on:
      - logstash

networks:
  elk:
    driver: bridge

volumes:
  elasticsearch-data:
    driver: local
```

### 2.2 Logstash Pipeline 설정
**logstash/pipeline/logstash.conf**
```ruby
input {
  # Filebeat input
  beats {
    port => 5044
    type => "beats"
  }
  
  # TCP input for direct logging
  tcp {
    port => 5000
    codec => json_lines
    type => "tcp"
  }
  
  # GELF input for structured logs
  gelf {
    port => 12201
    type => "gelf"
  }
  
  # Syslog input
  syslog {
    port => 5514
    type => "syslog"
  }
}

filter {
  # Parse game server logs
  if [type] == "game-server" {
    grok {
      match => {
        "message" => "%{TIMESTAMP_ISO8601:timestamp} %{LOGLEVEL:level} \[%{DATA:component}\] %{GREEDYDATA:log_message}"
      }
    }
    
    # Parse JSON fields if present
    if [log_message] =~ /^\{.*\}$/ {
      json {
        source => "log_message"
        target => "game_data"
      }
    }
    
    # Add custom fields
    mutate {
      add_field => {
        "environment" => "${ENVIRONMENT:production}"
        "service" => "game-server"
      }
    }
    
    # Parse player actions
    if [component] == "PLAYER_ACTION" {
      grok {
        match => {
          "log_message" => "player_id=%{NUMBER:player_id} action=%{WORD:action} zone=%{WORD:zone} x=%{NUMBER:x} y=%{NUMBER:y}"
        }
      }
      
      mutate {
        convert => {
          "player_id" => "integer"
          "x" => "float"
          "y" => "float"
        }
      }
    }
    
    # Parse combat logs
    if [component] == "COMBAT" {
      grok {
        match => {
          "log_message" => "attacker=%{NUMBER:attacker_id} defender=%{NUMBER:defender_id} damage=%{NUMBER:damage} skill=%{WORD:skill}"
        }
      }
      
      mutate {
        convert => {
          "attacker_id" => "integer"
          "defender_id" => "integer"
          "damage" => "integer"
        }
      }
    }
  }
  
  # Parse error logs
  if [level] == "ERROR" or [level] == "FATAL" {
    # Extract stack trace
    grok {
      match => {
        "message" => "(?m)%{GREEDYDATA:error_message}\n%{GREEDYDATA:stack_trace}"
      }
    }
    
    # Add alert flag
    mutate {
      add_field => {
        "alert" => "true"
        "alert_priority" => "high"
      }
    }
  }
  
  # Geo IP enrichment
  if [client_ip] {
    geoip {
      source => "client_ip"
      target => "geoip"
    }
  }
  
  # Add timestamp
  date {
    match => [ "timestamp", "ISO8601" ]
    target => "@timestamp"
  }
  
  # Remove unnecessary fields
  mutate {
    remove_field => [ "host", "agent", "ecs", "input", "log" ]
  }
}

output {
  # Output to Elasticsearch
  elasticsearch {
    hosts => ["elasticsearch:9200"]
    user => "elastic"
    password => "${ELASTIC_PASSWORD:elastic123}"
    index => "gameserver-%{+YYYY.MM.dd}"
    template_name => "gameserver"
    template => "/usr/share/logstash/templates/gameserver.json"
    template_overwrite => true
  }
  
  # Alert critical errors to monitoring
  if [alert] == "true" {
    http {
      url => "http://alertmanager:9093/api/v1/alerts"
      http_method => "post"
      format => "json"
      mapping => {
        "alerts" => [
          {
            "labels" => {
              "alertname" => "GameServerError"
              "severity" => "%{alert_priority}"
              "component" => "%{component}"
            }
            "annotations" => {
              "summary" => "%{error_message}"
              "description" => "%{stack_trace}"
            }
          }
        ]
      }
    }
  }
  
  # Debug output (development only)
  if [@metadata][debug] {
    stdout {
      codec => rubydebug
    }
  }
}
```

### 2.3 Filebeat 설정
**filebeat/filebeat.yml**
```yaml
filebeat.inputs:
  # Game server logs
  - type: container
    enabled: true
    paths:
      - '/var/lib/docker/containers/*/*.log'
    processors:
      - add_docker_metadata:
          host: "unix:///var/run/docker.sock"
      - decode_json_fields:
          fields: ["message"]
          target: "json"
          overwrite_keys: true
    multiline.pattern: '^\d{4}-\d{2}-\d{2}'
    multiline.negate: true
    multiline.match: after
    fields:
      log_type: game-server
  
  # System logs
  - type: log
    enabled: true
    paths:
      - /var/log/syslog
      - /var/log/auth.log
    fields:
      log_type: system
  
  # Application logs
  - type: log
    enabled: true
    paths:
      - /app/logs/*.log
    fields:
      log_type: application
    multiline.pattern: '^\['
    multiline.negate: true
    multiline.match: after

processors:
  # Add metadata
  - add_host_metadata:
      when.not.contains:
        tags: forwarded
  
  # Drop empty lines
  - drop_event:
      when:
        regexp:
          message: '^\s*$'
  
  # Parse timestamps
  - timestamp:
      field: json.timestamp
      layouts:
        - '2006-01-02T15:04:05.000Z'
      test:
        - '2024-01-15T14:30:45.123Z'

output.logstash:
  hosts: ["logstash:5044"]
  bulk_max_size: 2048
  
logging.level: info
logging.to_files: true
logging.files:
  path: /var/log/filebeat
  name: filebeat
  keepfiles: 7
  permissions: 0644
```

---

## 3. 구조화된 로깅 구현

### 3.1 C++ 구조화 로깅
**src/logging/structured_logger.cpp**
```cpp
#include <spdlog/spdlog.h>
#include <spdlog/sinks/tcp_sink.h>
#include <nlohmann/json.hpp>

class StructuredLogger {
private:
    std::shared_ptr<spdlog::logger> logger_;
    std::string service_name_;
    std::string environment_;
    
public:
    StructuredLogger(const std::string& name) 
        : service_name_(name)
        , environment_(std::getenv("ENVIRONMENT") ?: "production") {
        
        // TCP sink to Logstash
        auto tcp_sink = std::make_shared<spdlog::sinks::tcp_sink_mt>(
            "logstash", 5000
        );
        
        // Console sink for development
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        
        // File sink with rotation
        auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "/app/logs/game-server.log", 
            1024 * 1024 * 100,  // 100MB
            10  // 10 files
        );
        
        logger_ = std::make_shared<spdlog::logger>(
            name, 
            spdlog::sinks_init_list{tcp_sink, console_sink, file_sink}
        );
        
        logger_->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%n] %v");
        spdlog::register_logger(logger_);
    }
    
    void LogPlayerAction(const std::string& player_id, 
                         const std::string& action,
                         const nlohmann::json& details) {
        nlohmann::json log_entry = {
            {"timestamp", std::chrono::system_clock::now()},
            {"level", "INFO"},
            {"component", "PLAYER_ACTION"},
            {"service", service_name_},
            {"environment", environment_},
            {"player_id", player_id},
            {"action", action},
            {"details", details}
        };
        
        logger_->info(log_entry.dump());
    }
    
    void LogError(const std::string& component,
                  const std::string& error_message,
                  const std::exception& e) {
        nlohmann::json log_entry = {
            {"timestamp", std::chrono::system_clock::now()},
            {"level", "ERROR"},
            {"component", component},
            {"service", service_name_},
            {"environment", environment_},
            {"error_message", error_message},
            {"exception", e.what()},
            {"stack_trace", GetStackTrace()}
        };
        
        logger_->error(log_entry.dump());
    }
    
    void LogMetric(const std::string& metric_name,
                   double value,
                   const std::map<std::string, std::string>& tags) {
        nlohmann::json log_entry = {
            {"timestamp", std::chrono::system_clock::now()},
            {"level", "INFO"},
            {"component", "METRICS"},
            {"metric_name", metric_name},
            {"value", value},
            {"tags", tags}
        };
        
        logger_->info(log_entry.dump());
    }
    
    void LogGameEvent(const std::string& event_type,
                      const nlohmann::json& event_data) {
        nlohmann::json log_entry = {
            {"timestamp", std::chrono::system_clock::now()},
            {"level", "INFO"},
            {"component", "GAME_EVENT"},
            {"event_type", event_type},
            {"event_data", event_data}
        };
        
        logger_->info(log_entry.dump());
    }
    
private:
    std::string GetStackTrace() {
        // Implement stack trace capture
        void* array[20];
        size_t size = backtrace(array, 20);
        char** strings = backtrace_symbols(array, size);
        
        std::stringstream ss;
        for (size_t i = 0; i < size; i++) {
            ss << strings[i] << "\n";
        }
        
        free(strings);
        return ss.str();
    }
};

// 사용 예시
void HandlePlayerLogin(const Player& player, StructuredLogger& logger) {
    try {
        // 로그인 처리
        auto result = AuthenticatePlayer(player);
        
        logger.LogPlayerAction(
            player.id,
            "LOGIN",
            {
                {"success", result.success},
                {"ip_address", player.ip_address},
                {"client_version", player.client_version},
                {"login_time", std::chrono::system_clock::now()}
            }
        );
        
        logger.LogMetric(
            "player_login_duration",
            result.duration_ms,
            {{"zone", player.zone}, {"success", std::to_string(result.success)}}
        );
        
    } catch (const std::exception& e) {
        logger.LogError("AUTH", "Login failed", e);
    }
}
```

---

## 4. Kibana 대시보드

### 4.1 인덱스 패턴 생성
```bash
# Elasticsearch 인덱스 템플릿
curl -X PUT "localhost:9200/_index_template/gameserver" \
-H 'Content-Type: application/json' -d'
{
  "index_patterns": ["gameserver-*"],
  "template": {
    "settings": {
      "number_of_shards": 3,
      "number_of_replicas": 1,
      "index.lifecycle.name": "gameserver-policy",
      "index.lifecycle.rollover_alias": "gameserver"
    },
    "mappings": {
      "properties": {
        "@timestamp": { "type": "date" },
        "level": { "type": "keyword" },
        "component": { "type": "keyword" },
        "player_id": { "type": "keyword" },
        "action": { "type": "keyword" },
        "zone": { "type": "keyword" },
        "error_message": { "type": "text" },
        "stack_trace": { "type": "text" },
        "geoip": {
          "properties": {
            "location": { "type": "geo_point" }
          }
        }
      }
    }
  }
}'
```

### 4.2 대시보드 설정
**kibana/dashboards/game-server-dashboard.json**
```json
{
  "version": "8.11.0",
  "objects": [
    {
      "id": "game-server-overview",
      "type": "dashboard",
      "attributes": {
        "title": "Game Server Overview",
        "panels": [
          {
            "visualization": {
              "title": "Log Level Distribution",
              "type": "pie",
              "params": {
                "addTooltip": true,
                "addLegend": true,
                "legendPosition": "right"
              }
            }
          },
          {
            "visualization": {
              "title": "Player Actions Over Time",
              "type": "line",
              "params": {
                "grid": {"categoryLines": false, "style": {"color": "#eee"}},
                "categoryAxes": [{"id": "CategoryAxis-1", "type": "category", "position": "bottom"}],
                "valueAxes": [{"id": "ValueAxis-1", "name": "LeftAxis-1", "type": "value", "position": "left"}]
              }
            }
          },
          {
            "visualization": {
              "title": "Error Rate",
              "type": "metric",
              "params": {
                "metric": {
                  "percentageMode": false,
                  "useRanges": false,
                  "colorSchema": "Green to Red",
                  "metricColorMode": "None"
                }
              }
            }
          },
          {
            "visualization": {
              "title": "Top Error Messages",
              "type": "table",
              "params": {
                "perPage": 10,
                "showPartialRows": false,
                "showMetricsAtAllLevels": false
              }
            }
          },
          {
            "visualization": {
              "title": "Player Geographic Distribution",
              "type": "tile_map",
              "params": {
                "mapType": "Scaled Circle Markers",
                "isDesaturated": true,
                "addTooltip": true
              }
            }
          }
        ]
      }
    }
  ]
}
```

---

## 5. 로그 보관 및 관리

### 5.1 Index Lifecycle Management (ILM)
```bash
# ILM 정책 생성
curl -X PUT "localhost:9200/_ilm/policy/gameserver-policy" \
-H 'Content-Type: application/json' -d'
{
  "policy": {
    "phases": {
      "hot": {
        "min_age": "0ms",
        "actions": {
          "rollover": {
            "max_size": "50GB",
            "max_age": "7d"
          },
          "set_priority": {
            "priority": 100
          }
        }
      },
      "warm": {
        "min_age": "7d",
        "actions": {
          "set_priority": {
            "priority": 50
          },
          "allocate": {
            "number_of_replicas": 1
          },
          "forcemerge": {
            "max_num_segments": 1
          }
        }
      },
      "cold": {
        "min_age": "30d",
        "actions": {
          "set_priority": {
            "priority": 0
          },
          "allocate": {
            "number_of_replicas": 0
          }
        }
      },
      "delete": {
        "min_age": "90d",
        "actions": {
          "delete": {}
        }
      }
    }
  }
}'
```

### 5.2 로그 백업 스크립트
**scripts/backup-logs.sh**
```bash
#!/bin/bash

BACKUP_DIR="/backup/logs/$(date +%Y%m%d)"
ES_HOST="localhost:9200"

# Elasticsearch 스냅샷 생성
curl -X PUT "$ES_HOST/_snapshot/log_backup/snapshot_$(date +%Y%m%d)?wait_for_completion=true" \
-H 'Content-Type: application/json' -d'{
  "indices": "gameserver-*",
  "include_global_state": false
}'

# S3로 백업
aws s3 sync /backup/logs/ s3://game-backups/logs/ \
    --storage-class GLACIER

echo "Log backup completed"
```

---

## 6. 로그 분석 쿼리

### 6.1 유용한 Elasticsearch 쿼리
```json
// 최근 에러 조회
{
  "query": {
    "bool": {
      "must": [
        {"term": {"level": "ERROR"}},
        {"range": {"@timestamp": {"gte": "now-1h"}}}
      ]
    }
  },
  "sort": [{"@timestamp": {"order": "desc"}}],
  "size": 100
}

// 플레이어 활동 집계
{
  "aggs": {
    "player_actions": {
      "terms": {
        "field": "action",
        "size": 10
      },
      "aggs": {
        "unique_players": {
          "cardinality": {
            "field": "player_id"
          }
        }
      }
    }
  }
}

// 성능 메트릭 시계열
{
  "aggs": {
    "performance_over_time": {
      "date_histogram": {
        "field": "@timestamp",
        "interval": "5m"
      },
      "aggs": {
        "avg_response_time": {
          "avg": {
            "field": "response_time"
          }
        }
      }
    }
  }
}
```

---

## 7. 알림 설정

### 7.1 Elasticsearch Watcher
```json
{
  "trigger": {
    "schedule": {
      "interval": "1m"
    }
  },
  "input": {
    "search": {
      "request": {
        "indices": ["gameserver-*"],
        "body": {
          "query": {
            "bool": {
              "must": [
                {"term": {"level": "ERROR"}},
                {"range": {"@timestamp": {"gte": "now-5m"}}}
              ]
            }
          }
        }
      }
    }
  },
  "condition": {
    "compare": {
      "ctx.payload.hits.total": {
        "gte": 10
      }
    }
  },
  "actions": {
    "send_slack": {
      "webhook": {
        "scheme": "https",
        "host": "hooks.slack.com",
        "port": 443,
        "method": "post",
        "path": "/services/YOUR/SLACK/WEBHOOK",
        "body": "{\"text\":\"⚠️ High error rate detected: {{ctx.payload.hits.total}} errors in last 5 minutes\"}"
      }
    }
  }
}
```

---

## ✅ 체크리스트

### 필수 확인사항
- [ ] ELK Stack 설치 및 설정
- [ ] 로그 수집 에이전트 배포
- [ ] 구조화된 로깅 구현
- [ ] 인덱스 패턴 생성
- [ ] 기본 대시보드 구성
- [ ] 로그 보관 정책 설정

### 권장사항
- [ ] 로그 파싱 규칙 최적화
- [ ] 커스텀 대시보드 생성
- [ ] 알림 규칙 설정
- [ ] 로그 백업 자동화
- [ ] 보안 설정 강화

## 🎯 다음 단계
→ [07_security_checklist.md](07_security_checklist.md) - 보안 체크리스트