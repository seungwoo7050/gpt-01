# MVP7: Infrastructure Integration

## 개요

기존 게임 서버 (MVP1-6)에 프로덕션 인프라 (MVP0)를 통합하는 버전입니다.

## 주요 변경사항

1. **Session 클래스 확장**
   - JWT 인증 메서드 추가
   - 인증 상태 관리
   - 토큰 만료 시간 추적

2. **AuthHandler 구현**
   - 로그인 요청 처리
   - JWT 토큰 발급
   - 로그아웃 처리
   - 하트비트 관리

3. **게임 서버 인프라 통합**
   - MySQL 연결 풀 초기화
   - Redis 세션 관리 초기화
   - AuthService 통합
   - 패킷 핸들러 등록

## 아키텍처

```
Game Client
    ↓
TCP Connection
    ↓
Session (with JWT Auth)
    ↓
PacketHandler
    ↓
AuthHandler → AuthService → MySQL/Redis
```

## 설정

```cpp
// MySQL 설정
db_config.host = "localhost";
db_config.user = "mmorpg_user";
db_config.password = "password";
db_config.database = "mmorpg";

// Redis 설정
redis_config.host = "localhost";
redis_config.port = 6379;

// JWT Secret (환경변수로 관리 필요)
jwt_secret = "your-super-secret-key-change-this-in-production";
```

## 다음 단계

- Phase 44: ECS 컴포넌트 DB 영속성
- Phase 45: Redis 세션 관리 완전 통합
- Phase 46: 통합 테스트