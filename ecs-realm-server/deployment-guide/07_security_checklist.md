# 07. Security Checklist - 보안 체크리스트 및 강화 가이드

## 🎯 목표
게임 서버의 모든 계층에서 보안을 강화하고, 일반적인 공격 벡터로부터 시스템을 보호합니다.

## 📋 Prerequisites
- 보안 스캔 도구 (Trivy, OWASP ZAP)
- SSL/TLS 인증서
- 시크릿 관리 시스템
- WAF(Web Application Firewall)
- 보안 모니터링 도구

---

## 1. 보안 체크리스트 마스터

### 1.1 인프라 보안
```yaml
infrastructure_security:
  network:
    - [ ] VPC 격리 구성
    - [ ] 프라이빗 서브넷 사용
    - [ ] 보안 그룹 최소 권한
    - [ ] NACLs 설정
    - [ ] VPN/Private Link 사용
    - [ ] DDoS 방어 활성화
    
  access_control:
    - [ ] IAM 역할 최소 권한
    - [ ] MFA 필수 적용
    - [ ] SSH 키 관리
    - [ ] Bastion Host 사용
    - [ ] Session Manager 우선
    - [ ] 루트 계정 비활성화
    
  data_protection:
    - [ ] 저장 데이터 암호화
    - [ ] 전송 데이터 암호화
    - [ ] 백업 암호화
    - [ ] KMS 키 로테이션
    - [ ] S3 버킷 정책
    - [ ] CloudTrail 로깅
```

### 1.2 애플리케이션 보안
```yaml
application_security:
  authentication:
    - [ ] JWT 토큰 검증
    - [ ] 세션 타임아웃
    - [ ] 비밀번호 정책
    - [ ] 2FA 구현
    - [ ] 계정 잠금 정책
    - [ ] OAuth2 구현
    
  authorization:
    - [ ] RBAC 구현
    - [ ] API 권한 검증
    - [ ] 리소스 접근 제어
    - [ ] 권한 에스컬레이션 방지
    
  input_validation:
    - [ ] SQL Injection 방지
    - [ ] XSS 방지
    - [ ] CSRF 토큰
    - [ ] 파일 업로드 검증
    - [ ] Rate Limiting
    - [ ] Input Sanitization
```

---

## 2. 네트워크 보안

### 2.1 방화벽 규칙
**scripts/configure-firewall.sh**
```bash
#!/bin/bash

# iptables 규칙 설정
iptables -F
iptables -X

# 기본 정책: 모두 차단
iptables -P INPUT DROP
iptables -P FORWARD DROP
iptables -P OUTPUT ACCEPT

# Loopback 허용
iptables -A INPUT -i lo -j ACCEPT

# 기존 연결 허용
iptables -A INPUT -m conntrack --ctstate ESTABLISHED,RELATED -j ACCEPT

# SSH (특정 IP만)
iptables -A INPUT -p tcp --dport 22 -s 10.0.0.0/8 -j ACCEPT

# Game Server 포트
iptables -A INPUT -p tcp --dport 8080 -j ACCEPT
iptables -A INPUT -p udp --dport 8080 -j ACCEPT

# Monitoring
iptables -A INPUT -p tcp --dport 9090 -s 10.0.0.0/8 -j ACCEPT

# DDoS 방어
iptables -A INPUT -p tcp --dport 8080 -m connlimit --connlimit-above 100 -j REJECT
iptables -A INPUT -p tcp --dport 8080 -m limit --limit 100/second --limit-burst 200 -j ACCEPT

# SYN Flood 방어
iptables -A INPUT -p tcp --syn -m limit --limit 1/s --limit-burst 3 -j ACCEPT

# Ping Flood 방어
iptables -A INPUT -p icmp --icmp-type echo-request -m limit --limit 1/s -j ACCEPT

# 로깅
iptables -A INPUT -m limit --limit 5/min -j LOG --log-prefix "iptables-dropped: "

# 저장
iptables-save > /etc/iptables/rules.v4
```

### 2.2 WAF 설정 (ModSecurity)
**nginx/modsecurity.conf**
```nginx
# ModSecurity 활성화
SecRuleEngine On
SecRequestBodyAccess On
SecResponseBodyAccess Off
SecRequestBodyLimit 13107200
SecRequestBodyNoFilesLimit 131072
SecRequestBodyLimitAction Reject

# OWASP CRS 로드
Include /etc/nginx/modsecurity/crs/crs-setup.conf
Include /etc/nginx/modsecurity/crs/rules/*.conf

# 커스텀 규칙
# SQL Injection 방지
SecRule ARGS "@detectSQLi" \
    "id:1001,\
    phase:2,\
    block,\
    msg:'SQL Injection Attack Detected',\
    logdata:'Matched Data: %{MATCHED_VAR} found within %{MATCHED_VAR_NAME}',\
    severity:'CRITICAL',\
    tag:'application-multi',\
    tag:'language-multi',\
    tag:'platform-multi',\
    tag:'attack-sqli'"

# XSS 방지
SecRule ARGS "@detectXSS" \
    "id:1002,\
    phase:2,\
    block,\
    msg:'XSS Attack Detected',\
    severity:'CRITICAL'"

# Game-specific: 치트 시도 감지
SecRule REQUEST_URI "@contains /api/v1/player/stats" \
    "id:2001,\
    phase:1,\
    chain,\
    msg:'Potential stat manipulation attempt'"
    SecRule ARGS:gold "@gt 1000000" \
        "setvar:'tx.anomaly_score=+10'"

# Rate Limiting per IP
SecRule IP:bf_counter "@gt 5" \
    "id:3001,\
    phase:2,\
    deny,\
    status:429,\
    msg:'Rate limit exceeded',\
    chain"
    SecRule REQUEST_METHOD "^(GET|POST)$" \
        "setvar:ip.bf_counter=+1,\
        expirevar:ip.bf_counter=60"
```

---

## 3. 컨테이너 보안

### 3.1 Docker 보안 스캔
**scripts/container-security-scan.sh**
```bash
#!/bin/bash

IMAGE=$1

echo "=== Container Security Scan ==="

# 1. Trivy 스캔
echo "Running Trivy scan..."
trivy image --severity CRITICAL,HIGH --exit-code 0 $IMAGE

# 2. Docker Bench Security
echo "Running Docker Bench..."
docker run --rm --net host --pid host --userns host --cap-add audit_control \
    -e DOCKER_CONTENT_TRUST=$DOCKER_CONTENT_TRUST \
    -v /var/lib:/var/lib \
    -v /var/run/docker.sock:/var/run/docker.sock \
    -v /etc:/etc --label docker_bench_security \
    docker/docker-bench-security

# 3. Dockerfile 검증
echo "Checking Dockerfile best practices..."
hadolint Dockerfile

# 4. 시크릿 스캔
echo "Scanning for secrets..."
docker run --rm -v $(pwd):/src trufflesecurity/trufflehog:latest \
    filesystem /src --json

# 5. SBOM 생성
echo "Generating SBOM..."
syft $IMAGE -o json > sbom.json

echo "=== Scan Complete ==="
```

### 3.2 보안 강화된 Dockerfile
**Dockerfile.secure**
```dockerfile
# 보안 베이스 이미지
FROM gcr.io/distroless/cc-debian12:nonroot AS runtime

# 메타데이터
LABEL maintainer="security@gameserver.com"
LABEL security.scan="required"

# 비루트 사용자
USER 1000:1000

# 읽기 전용 파일시스템
RUN chmod -R 555 /app

# 불필요한 capabilities 제거
RUN setcap -r /app/game_server || true

# 보안 환경 변수
ENV DOCKER_CONTENT_TRUST=1
ENV SECURE_MODE=true

# 헬스체크 (no shell)
HEALTHCHECK --interval=30s --timeout=3s --start-period=10s --retries=3 \
    CMD ["/app/healthcheck"]

# 시크릿 마운트 (런타임)
# docker run --mount type=secret,id=db_password,target=/run/secrets/db_password

# 보안 설정
EXPOSE 8080
ENTRYPOINT ["/app/game_server"]
CMD ["--secure"]

# 보안 레이블
LABEL security.capabilities="none"
LABEL security.no-new-privileges="true"
LABEL security.read-only-root="true"
```

---

## 4. 시크릿 관리

### 4.1 Kubernetes Secrets 암호화
**k8s/sealed-secrets.yaml**
```yaml
apiVersion: bitnami.com/v1alpha1
kind: SealedSecret
metadata:
  name: game-secrets
  namespace: game-production
spec:
  encryptedData:
    db_password: AgBvA8N3O...encrypted...
    jwt_secret: AgCkL9M2P...encrypted...
    api_key: AgDmN7K4Q...encrypted...
```

### 4.2 HashiCorp Vault 통합
**scripts/vault-init.sh**
```bash
#!/bin/bash

# Vault 초기화
vault operator init -key-shares=5 -key-threshold=3

# 시크릿 엔진 활성화
vault secrets enable -path=gameserver kv-v2

# 시크릿 저장
vault kv put gameserver/database \
    username="gameuser" \
    password="$(openssl rand -base64 32)" \
    host="mysql.internal" \
    port="3306"

vault kv put gameserver/jwt \
    secret="$(openssl rand -base64 64)" \
    issuer="gameserver.com" \
    expiry="3600"

# 정책 생성
vault policy write gameserver - <<EOF
path "gameserver/data/*" {
  capabilities = ["read", "list"]
}
EOF

# Kubernetes 인증 설정
vault auth enable kubernetes

vault write auth/kubernetes/config \
    token_reviewer_jwt="$(cat /var/run/secrets/kubernetes.io/serviceaccount/token)" \
    kubernetes_host="https://kubernetes.default.svc:443" \
    kubernetes_ca_cert=@/var/run/secrets/kubernetes.io/serviceaccount/ca.crt

# 역할 생성
vault write auth/kubernetes/role/gameserver \
    bound_service_account_names=game-server \
    bound_service_account_namespaces=game-production \
    policies=gameserver \
    ttl=24h
```

---

## 5. 코드 보안

### 5.1 SAST (Static Application Security Testing)
**.github/workflows/security-scan.yml**
```yaml
name: Security Scan

on:
  push:
    branches: [main, develop]
  pull_request:

jobs:
  sast:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v3
    
    - name: SonarQube Scan
      uses: sonarsource/sonarqube-scan-action@master
      env:
        GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
        SONAR_TOKEN: ${{ secrets.SONAR_TOKEN }}
        SONAR_HOST_URL: ${{ secrets.SONAR_HOST_URL }}
    
    - name: CodeQL Analysis
      uses: github/codeql-action/analyze@v2
      with:
        languages: cpp
    
    - name: Semgrep
      uses: returntocorp/semgrep-action@v1
      with:
        config: >-
          p/security-audit
          p/cpp
          p/owasp-top-ten
    
    - name: Dependency Check
      uses: dependency-check/Dependency-Check_Action@main
      with:
        project: 'game-server'
        path: '.'
        format: 'HTML'
```

### 5.2 보안 코딩 패턴
**src/security/secure_coding.cpp**
```cpp
#include <sodium.h>
#include <jwt-cpp/jwt.h>

class SecurityManager {
private:
    std::string jwt_secret_;
    
public:
    // 안전한 문자열 비교 (timing attack 방지)
    bool SecureCompare(const std::string& a, const std::string& b) {
        if (a.length() != b.length()) {
            return false;
        }
        return sodium_memcmp(a.data(), b.data(), a.length()) == 0;
    }
    
    // SQL Injection 방지
    std::string PrepareQuery(const std::string& query, 
                            const std::vector<std::string>& params) {
        // Prepared statement 사용
        auto stmt = db_->prepare(query);
        for (size_t i = 0; i < params.size(); ++i) {
            stmt->bind(i + 1, SanitizeInput(params[i]));
        }
        return stmt->str();
    }
    
    // Input Sanitization
    std::string SanitizeInput(const std::string& input) {
        std::string sanitized;
        for (char c : input) {
            if (std::isalnum(c) || c == ' ' || c == '_' || c == '-') {
                sanitized += c;
            }
        }
        return sanitized;
    }
    
    // XSS 방지
    std::string EscapeHtml(const std::string& input) {
        std::string escaped;
        for (char c : input) {
            switch (c) {
                case '<': escaped += "&lt;"; break;
                case '>': escaped += "&gt;"; break;
                case '&': escaped += "&amp;"; break;
                case '"': escaped += "&quot;"; break;
                case '\'': escaped += "&#x27;"; break;
                default: escaped += c;
            }
        }
        return escaped;
    }
    
    // 안전한 랜덤 생성
    std::string GenerateSecureToken(size_t length = 32) {
        std::vector<unsigned char> buffer(length);
        randombytes_buf(buffer.data(), length);
        return base64_encode(buffer);
    }
    
    // Rate Limiting
    bool CheckRateLimit(const std::string& client_ip, 
                       const std::string& endpoint) {
        auto key = client_ip + ":" + endpoint;
        auto& counter = rate_limits_[key];
        
        auto now = std::chrono::steady_clock::now();
        if (now - counter.reset_time > std::chrono::seconds(60)) {
            counter.count = 0;
            counter.reset_time = now;
        }
        
        if (++counter.count > 100) {
            LOG_SECURITY("Rate limit exceeded", client_ip, endpoint);
            return false;
        }
        return true;
    }
    
    // 파일 업로드 검증
    bool ValidateFileUpload(const std::vector<uint8_t>& file_data,
                           const std::string& filename) {
        // 파일 크기 검증
        if (file_data.size() > 10 * 1024 * 1024) { // 10MB
            return false;
        }
        
        // 확장자 검증
        std::set<std::string> allowed = {".jpg", ".png", ".gif"};
        auto ext = std::filesystem::path(filename).extension();
        if (allowed.find(ext) == allowed.end()) {
            return false;
        }
        
        // Magic number 검증
        if (!ValidateMagicNumber(file_data, ext)) {
            return false;
        }
        
        // 악성 코드 스캔
        return ScanForMalware(file_data);
    }
};
```

---

## 6. 보안 모니터링

### 6.1 보안 이벤트 로깅
**src/security/security_logger.cpp**
```cpp
class SecurityLogger {
public:
    enum SecurityEventType {
        LOGIN_ATTEMPT,
        LOGIN_FAILED,
        SUSPICIOUS_ACTIVITY,
        PRIVILEGE_ESCALATION,
        DATA_BREACH_ATTEMPT,
        RATE_LIMIT_EXCEEDED,
        INVALID_TOKEN,
        SQL_INJECTION_ATTEMPT,
        XSS_ATTEMPT
    };
    
    void LogSecurityEvent(SecurityEventType type,
                         const std::string& user_id,
                         const std::string& ip_address,
                         const std::string& details) {
        nlohmann::json event = {
            {"timestamp", std::chrono::system_clock::now()},
            {"event_type", GetEventTypeName(type)},
            {"severity", GetEventSeverity(type)},
            {"user_id", user_id},
            {"ip_address", ip_address},
            {"details", details},
            {"geo_location", GetGeoLocation(ip_address)}
        };
        
        // 로그 저장
        logger_->error(event.dump());
        
        // SIEM 전송
        SendToSIEM(event);
        
        // 임계값 체크
        if (GetEventSeverity(type) >= CRITICAL) {
            TriggerAlert(event);
        }
    }
    
private:
    void SendToSIEM(const nlohmann::json& event) {
        // Splunk/ELK로 전송
        http_client_.post("https://siem.internal/api/events", event.dump());
    }
    
    void TriggerAlert(const nlohmann::json& event) {
        // PagerDuty 알림
        pagerduty_.trigger_incident(
            "Security Alert: " + event["event_type"].get<std::string>(),
            event.dump()
        );
    }
};
```

### 6.2 침입 탐지 시스템 (IDS)
**scripts/ids-rules.yaml**
```yaml
rules:
  - name: "Brute Force Detection"
    pattern: "LOGIN_FAILED"
    threshold: 5
    window: 60s
    action: "block_ip"
    
  - name: "SQL Injection Detection"
    pattern: ".*(\bUNION\b|\bSELECT\b.*\bFROM\b|\bDROP\b|\bDELETE\b).*"
    action: "alert_and_block"
    
  - name: "Speed Hack Detection"
    condition: "movement_speed > max_allowed_speed * 1.1"
    action: "flag_player"
    
  - name: "Teleport Hack Detection"
    condition: "distance_traveled > max_possible_distance"
    action: "ban_player"
    
  - name: "Currency Manipulation"
    condition: "gold_increase > legitimate_max_per_second"
    action: "rollback_and_ban"
```

---

## 7. 컴플라이언스

### 7.1 GDPR 준수
```cpp
class GDPRCompliance {
public:
    // 개인정보 암호화
    std::string EncryptPII(const std::string& data) {
        return AES256_encrypt(data, master_key_);
    }
    
    // Right to be Forgotten
    void DeleteUserData(const std::string& user_id) {
        // 데이터베이스에서 삭제
        db_->execute("DELETE FROM players WHERE player_id = ?", user_id);
        db_->execute("DELETE FROM characters WHERE player_id = ?", user_id);
        
        // 백업에서도 삭제 마킹
        MarkForDeletionInBackups(user_id);
        
        // 로그 익명화
        AnonymizeLogs(user_id);
    }
    
    // Data Portability
    nlohmann::json ExportUserData(const std::string& user_id) {
        nlohmann::json user_data;
        
        // 모든 관련 데이터 수집
        user_data["account"] = GetAccountData(user_id);
        user_data["characters"] = GetCharacterData(user_id);
        user_data["transactions"] = GetTransactionHistory(user_id);
        user_data["game_logs"] = GetGameLogs(user_id);
        
        return user_data;
    }
};
```

---

## 8. 보안 테스트

### 8.1 침투 테스트 스크립트
**scripts/pentest.sh**
```bash
#!/bin/bash

TARGET=$1

echo "=== Penetration Testing ==="

# 1. OWASP ZAP 스캔
docker run -t owasp/zap2docker-stable zap-baseline.py \
    -t https://$TARGET \
    -r zap_report.html

# 2. Nikto 웹 스캐너
nikto -h $TARGET -o nikto_report.txt

# 3. SQLMap (SQL Injection)
sqlmap -u "https://$TARGET/api/v1/player?id=1" \
    --batch --level=5 --risk=3

# 4. Metasploit
msfconsole -q -x "
    use auxiliary/scanner/http/dir_scanner;
    set RHOSTS $TARGET;
    run;
    exit
"

# 5. 커스텀 게임 서버 테스트
python3 game_security_test.py --target $TARGET

echo "=== Pentest Complete ==="
```

---

## ✅ 보안 체크리스트 요약

### Critical (필수)
- [ ] SSL/TLS 암호화
- [ ] 인증 및 권한 검증
- [ ] SQL Injection 방지
- [ ] XSS 방지
- [ ] 시크릿 암호화
- [ ] 로깅 및 모니터링
- [ ] 백업 암호화
- [ ] DDoS 방어

### High (권장)
- [ ] WAF 구성
- [ ] 침입 탐지 시스템
- [ ] 보안 스캔 자동화
- [ ] 취약점 관리
- [ ] 컴플라이언스 준수
- [ ] 인시던트 대응 계획

### Medium (개선)
- [ ] 보안 교육
- [ ] 정기 침투 테스트
- [ ] 보안 감사
- [ ] Bug Bounty 프로그램

## 🎯 다음 단계
→ [09_testing_strategy.md](09_testing_strategy.md) - 테스팅 전략