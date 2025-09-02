# 08. SSL/TLS Certificates - SSL/TLS 인증서 설정 가이드

## 🎯 목표
게임 서버와 클라이언트 간의 안전한 통신을 위해 SSL/TLS 인증서를 설정하고 관리합니다.

## 📋 Prerequisites
- 도메인 이름 소유권
- DNS 관리 권한
- Let's Encrypt 또는 상용 인증서 제공자 계정
- Nginx/ALB 설정 권한

---

## 1. SSL/TLS 전략 선택

### 1.1 인증서 옵션 비교
| 방법 | 비용 | 자동 갱신 | 신뢰도 | 추천 사용 케이스 |
|------|------|-----------|---------|------------------|
| Let's Encrypt | 무료 | ✅ | 높음 | 개발/스테이징/소규모 프로덕션 |
| AWS Certificate Manager | 무료 (AWS 내) | ✅ | 높음 | AWS 인프라 사용 시 |
| DigiCert/Comodo | 유료 | ❌ | 매우 높음 | 대규모 프로덕션 |
| Self-signed | 무료 | ❌ | 낮음 | 개발 환경만 |

### 1.2 추천 아키텍처
```
Internet
    │
    ├── CloudFront (SSL Termination)
    │
    ├── ALB (SSL Termination)
    │       │
    │       └── Target Group (HTTP)
    │               │
    │               ├── Game Server 1
    │               ├── Game Server 2
    │               └── Game Server 3
    │
    └── Direct Game Connection (TLS)
            │
            └── Game Servers (TLS Enabled)
```

---

## 2. AWS Certificate Manager (ACM) 설정

### 2.1 인증서 요청
```bash
# CLI로 인증서 요청
aws acm request-certificate \
    --domain-name game-server.com \
    --subject-alternative-names "*.game-server.com" "api.game-server.com" \
    --validation-method DNS \
    --region us-east-1

# 인증서 ARN 저장
export CERT_ARN=arn:aws:acm:us-east-1:123456789012:certificate/xxxxx
```

### 2.2 DNS 검증
```bash
# 검증용 DNS 레코드 확인
aws acm describe-certificate \
    --certificate-arn $CERT_ARN \
    --query 'Certificate.DomainValidationOptions[*].[DomainName,ResourceRecord]'

# Route 53에 검증 레코드 추가
aws route53 change-resource-record-sets \
    --hosted-zone-id Z123456789 \
    --change-batch file://dns-validation.json
```

**dns-validation.json:**
```json
{
  "Changes": [{
    "Action": "CREATE",
    "ResourceRecordSet": {
      "Name": "_acme-challenge.game-server.com",
      "Type": "CNAME",
      "TTL": 300,
      "ResourceRecords": [{
        "Value": "_validation-record.acm-validations.aws."
      }]
    }
  }]
}
```

### 2.3 ALB에 인증서 연결
```bash
# HTTPS 리스너 추가
aws elbv2 create-listener \
    --load-balancer-arn $ALB_ARN \
    --protocol HTTPS \
    --port 443 \
    --certificates CertificateArn=$CERT_ARN \
    --default-actions Type=forward,TargetGroupArn=$TG_ARN

# HTTP to HTTPS 리다이렉션
aws elbv2 create-listener \
    --load-balancer-arn $ALB_ARN \
    --protocol HTTP \
    --port 80 \
    --default-actions Type=redirect,RedirectConfig='{Protocol=HTTPS,Port=443,StatusCode=HTTP_301}'
```

---

## 3. Let's Encrypt with Certbot

### 3.1 Certbot 설치
```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y certbot python3-certbot-nginx

# Amazon Linux 2
sudo yum install -y certbot python3-certbot-nginx
```

### 3.2 인증서 발급
```bash
# Nginx 자동 설정
sudo certbot --nginx \
    -d game-server.com \
    -d www.game-server.com \
    -d api.game-server.com \
    --non-interactive \
    --agree-tos \
    --email admin@game-server.com

# 수동 발급 (DNS 챌린지)
sudo certbot certonly \
    --manual \
    --preferred-challenges dns \
    -d game-server.com \
    -d *.game-server.com
```

### 3.3 자동 갱신 설정
```bash
# Cron 작업 추가
echo "0 3 * * * /usr/bin/certbot renew --quiet --post-hook 'systemctl reload nginx'" | sudo crontab -

# 갱신 테스트
sudo certbot renew --dry-run
```

---

## 4. Nginx SSL 설정

### 4.1 SSL 설정 파일
**/etc/nginx/sites-available/game-server-ssl**
```nginx
# HTTP to HTTPS 리다이렉션
server {
    listen 80;
    listen [::]:80;
    server_name game-server.com www.game-server.com;
    
    location /.well-known/acme-challenge/ {
        root /var/www/certbot;
    }
    
    location / {
        return 301 https://$server_name$request_uri;
    }
}

# HTTPS 서버 블록
server {
    listen 443 ssl http2;
    listen [::]:443 ssl http2;
    server_name game-server.com www.game-server.com;
    
    # SSL 인증서
    ssl_certificate /etc/letsencrypt/live/game-server.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/game-server.com/privkey.pem;
    
    # SSL 보안 설정
    ssl_protocols TLSv1.2 TLSv1.3;
    ssl_ciphers ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384;
    ssl_prefer_server_ciphers off;
    
    # OCSP Stapling
    ssl_stapling on;
    ssl_stapling_verify on;
    ssl_trusted_certificate /etc/letsencrypt/live/game-server.com/chain.pem;
    
    # Security Headers
    add_header Strict-Transport-Security "max-age=63072000; includeSubDomains; preload" always;
    add_header X-Frame-Options "DENY" always;
    add_header X-Content-Type-Options "nosniff" always;
    add_header X-XSS-Protection "1; mode=block" always;
    
    # DH Parameters
    ssl_dhparam /etc/nginx/ssl/dhparam.pem;
    
    # Session 설정
    ssl_session_timeout 1d;
    ssl_session_cache shared:SSL:50m;
    ssl_session_tickets off;
    
    # 게임 서버 프록시
    location / {
        proxy_pass http://game_servers;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        
        # WebSocket 지원
        proxy_read_timeout 3600;
        proxy_send_timeout 3600;
    }
    
    # 상태 체크 엔드포인트
    location /health {
        access_log off;
        proxy_pass http://game_servers/health;
    }
}

# TCP/UDP 스트림 (게임 프로토콜용)
stream {
    upstream game_tcp {
        least_conn;
        server game-server-1:8080;
        server game-server-2:8080;
        server game-server-3:8080;
    }
    
    server {
        listen 8443 ssl;
        ssl_certificate /etc/letsencrypt/live/game-server.com/fullchain.pem;
        ssl_certificate_key /etc/letsencrypt/live/game-server.com/privkey.pem;
        ssl_protocols TLSv1.2 TLSv1.3;
        proxy_pass game_tcp;
    }
}
```

### 4.2 DH Parameters 생성
```bash
# 강력한 DH 파라미터 생성 (시간이 걸림)
sudo openssl dhparam -out /etc/nginx/ssl/dhparam.pem 4096
```

---

## 5. 게임 서버 TLS 구현

### 5.1 C++ TLS 서버 구현
**src/network/tls_server.cpp**
```cpp
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

class TLSGameServer {
private:
    boost::asio::io_context& io_context_;
    boost::asio::ssl::context ssl_context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    
public:
    TLSGameServer(boost::asio::io_context& io_context, unsigned short port)
        : io_context_(io_context)
        , ssl_context_(boost::asio::ssl::context::tlsv12)
        , acceptor_(io_context, boost::asio::ip::tcp::endpoint(
            boost::asio::ip::tcp::v4(), port))
    {
        configure_ssl();
        start_accept();
    }
    
private:
    void configure_ssl() {
        // SSL 옵션 설정
        ssl_context_.set_options(
            boost::asio::ssl::context::default_workarounds |
            boost::asio::ssl::context::no_sslv2 |
            boost::asio::ssl::context::no_sslv3 |
            boost::asio::ssl::context::no_tlsv1 |
            boost::asio::ssl::context::no_tlsv1_1 |
            boost::asio::ssl::context::single_dh_use
        );
        
        // 인증서 및 키 로드
        ssl_context_.use_certificate_chain_file("/app/certs/server.crt");
        ssl_context_.use_private_key_file("/app/certs/server.key", 
            boost::asio::ssl::context::pem);
        
        // DH 파라미터
        ssl_context_.use_tmp_dh_file("/app/certs/dhparam.pem");
        
        // Cipher suites 설정
        SSL_CTX_set_cipher_list(ssl_context_.native_handle(),
            "ECDHE-ECDSA-AES256-GCM-SHA384:"
            "ECDHE-RSA-AES256-GCM-SHA384:"
            "ECDHE-ECDSA-CHACHA20-POLY1305:"
            "ECDHE-RSA-CHACHA20-POLY1305:"
            "ECDHE-ECDSA-AES128-GCM-SHA256:"
            "ECDHE-RSA-AES128-GCM-SHA256"
        );
    }
    
    void start_accept() {
        auto new_session = std::make_shared<TLSSession>(io_context_, ssl_context_);
        
        acceptor_.async_accept(new_session->socket(),
            [this, new_session](std::error_code ec) {
                if (!ec) {
                    new_session->start();
                }
                start_accept();
            });
    }
};

class TLSSession : public std::enable_shared_from_this<TLSSession> {
private:
    boost::asio::ssl::stream<boost::asio::ip::tcp::socket> socket_;
    
public:
    TLSSession(boost::asio::io_context& io_context,
               boost::asio::ssl::context& ssl_context)
        : socket_(io_context, ssl_context) {}
    
    auto& socket() { return socket_.lowest_layer(); }
    
    void start() {
        // SSL 핸드셰이크
        socket_.async_handshake(boost::asio::ssl::stream_base::server,
            [self = shared_from_this()](std::error_code ec) {
                if (!ec) {
                    self->read_header();
                }
            });
    }
    
    void read_header() {
        // 암호화된 데이터 읽기
        boost::asio::async_read(socket_,
            boost::asio::buffer(header_buffer_),
            [self = shared_from_this()](std::error_code ec, std::size_t) {
                if (!ec) {
                    self->process_packet();
                }
            });
    }
};
```

---

## 6. 인증서 모니터링 및 관리

### 6.1 인증서 만료 모니터링
**scripts/check-ssl-expiry.sh**
```bash
#!/bin/bash

DOMAINS=(
    "game-server.com"
    "api.game-server.com"
    "admin.game-server.com"
)

WARNING_DAYS=30

for domain in "${DOMAINS[@]}"; do
    expiry_date=$(echo | openssl s_client -servername "$domain" \
        -connect "$domain:443" 2>/dev/null | \
        openssl x509 -noout -enddate | \
        cut -d= -f2)
    
    expiry_epoch=$(date -d "$expiry_date" +%s)
    current_epoch=$(date +%s)
    days_remaining=$(( ($expiry_epoch - $current_epoch) / 86400 ))
    
    if [ $days_remaining -lt $WARNING_DAYS ]; then
        echo "WARNING: $domain certificate expires in $days_remaining days"
        # 알림 전송
        curl -X POST $SLACK_WEBHOOK \
            -H 'Content-Type: application/json' \
            -d "{\"text\":\"⚠️ SSL Certificate for $domain expires in $days_remaining days\"}"
    else
        echo "OK: $domain certificate valid for $days_remaining days"
    fi
done
```

### 6.2 Prometheus 모니터링
```yaml
# SSL 인증서 익스포터 설정
- job_name: 'ssl'
  metrics_path: /probe
  static_configs:
    - targets:
      - https://game-server.com
      - https://api.game-server.com
  relabel_configs:
    - source_labels: [__address__]
      target_label: __param_target
    - source_labels: [__param_target]
      target_label: instance
    - target_label: __address__
      replacement: ssl-exporter:9219
```

---

## 7. 보안 베스트 프랙티스

### 7.1 SSL Labs 테스트
```bash
# SSL 설정 검증
curl -X GET "https://api.ssllabs.com/api/v3/analyze?host=game-server.com"

# 결과 확인 (A+ 등급 목표)
# https://www.ssllabs.com/ssltest/analyze.html?d=game-server.com
```

### 7.2 보안 헤더 설정
```nginx
# Content Security Policy
add_header Content-Security-Policy "default-src 'self'; script-src 'self' 'unsafe-inline'; style-src 'self' 'unsafe-inline';" always;

# Referrer Policy
add_header Referrer-Policy "strict-origin-when-cross-origin" always;

# Feature Policy
add_header Feature-Policy "geolocation 'none'; microphone 'none'; camera 'none';" always;
```

### 7.3 HSTS Preload
```bash
# HSTS preload 리스트 등록
# 1. 도메인이 HSTS 헤더를 전송하는지 확인
# 2. https://hstspreload.org/ 에서 등록
```

---

## 8. 트러블슈팅

### 8.1 일반적인 문제 해결

#### 인증서 체인 문제
```bash
# 인증서 체인 확인
openssl s_client -connect game-server.com:443 -showcerts

# 중간 인증서 누락 확인
openssl verify -CAfile /etc/ssl/certs/ca-certificates.crt server.crt
```

#### 권한 문제
```bash
# 인증서 파일 권한 수정
sudo chown root:root /etc/letsencrypt/live/game-server.com/*.pem
sudo chmod 600 /etc/letsencrypt/live/game-server.com/privkey.pem
sudo chmod 644 /etc/letsencrypt/live/game-server.com/fullchain.pem
```

#### Mixed Content 문제
```javascript
// 클라이언트 코드에서 프로토콜 자동 감지
const protocol = window.location.protocol;
const wsProtocol = protocol === 'https:' ? 'wss:' : 'ws:';
const socket = new WebSocket(`${wsProtocol}//${window.location.host}/ws`);
```

---

## ✅ 체크리스트

### 필수 확인사항
- [ ] SSL 인증서 발급 완료
- [ ] HTTPS 리다이렉션 설정
- [ ] 강력한 암호화 스위트 설정
- [ ] HSTS 헤더 구성
- [ ] 인증서 자동 갱신 설정
- [ ] 모니터링 알림 구성

### 권장사항
- [ ] SSL Labs A+ 등급 달성
- [ ] OCSP Stapling 활성화
- [ ] TLS 1.3 지원
- [ ] HTTP/2 활성화
- [ ] HSTS Preload 등록

## 🎯 다음 단계
→ [09_testing_strategy.md](09_testing_strategy.md) - 프로덕션 테스트 전략