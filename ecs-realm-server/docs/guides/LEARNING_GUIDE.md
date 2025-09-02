# C++ Multiplayer Game Server - Learning Guide

## 🎯 학습 목표
이 가이드는 프로젝트 내 버전별 코드와 문서를 활용하여 게임 서버 개발을 단계적으로 학습하는 방법을 안내합니다.

## 📚 필수 전제 지식
- C++ 기본 문법
- 네트워크 프로그래밍 기초
- 멀티스레딩 개념
- 데이터베이스 기본

## 🗺️ 학습 순서

### Phase 1: 기초 개념 학습 (1주차)

1. **C++ 기초 복습**
   - 📖 `knowledge/cpp_basics_for_game_server.md`
   - 📖 `knowledge/multithreading_basics.md`

2. **게임 서버 핵심 개념**
   - 📖 `knowledge/game_server_core_concepts.md`
   - 📖 `knowledge/network_programming_basics.md`

3. **프로젝트 구조 이해**
   - 📖 `README.md` - 전체 개요
   - 📖 `ARCHITECTURE_DECISIONS.md` - 설계 결정 이유
   - 📖 `DESIGN_TRADEOFFS.md` - 트레이드오프 이해

### Phase 2: MVP별 순차 학습 (2-3주차)

#### MVP0: 인프라 설정
- 📁 `versions/mvp0-infrastructure/`
- 목표: 개발 환경 구축, 데이터베이스 설정
- 학습 포인트: CMake, MySQL 설정

#### MVP1: 네트워킹 기초
- 📁 `versions/mvp1-networking/`
- 📖 `versions/mvp1-networking/README.md`
- 목표: TCP 서버 구현, 비동기 I/O 이해
- 핵심 코드: `src/core/network/tcp_server.cpp`

#### MVP2: ECS 아키텍처
- 📁 `versions/mvp2-ecs-basic/` → `versions/mvp2-ecs-optimized/`
- 📖 `knowledge/ecs_architecture_system.md`
- 목표: Entity-Component-System 패턴 이해
- 비교 학습: 기본 구현 vs 최적화 구현

#### MVP3: 공간 분할
- 📁 `versions/mvp3-world-grid/` vs `versions/mvp3-world-octree/`
- 📖 `knowledge/spatial_systems_performance_comparison.md`
- 목표: Grid와 Octree 비교, 성능 차이 이해
- 실습: 두 방식의 성능 테스트

#### MVP4: 전투 시스템
- 📁 `versions/mvp4-combat-action/` vs `versions/mvp4-combat-targeted/`
- 목표: 액션 전투 vs 타겟팅 전투 구현
- 핵심 코드: `src/game/combat/combat_system.cpp`

#### MVP5: PVP 시스템
- 📁 `versions/mvp5-pvp-systems/`
- 📁 `versions/mvp5-guild-wars/`
- 목표: 실시간 PVP, 길드전 구현
- 테스트: `tests/pvp/PVP_TEST_GUIDE.md`

### Phase 3: 고급 주제 (4주차)

1. **성능 최적화**
   - 📖 `knowledge/performance_optimization_basics.md`
   - 📖 `knowledge/performance_engineering_advanced.md`
   - 📖 `knowledge/lockfree_programming_guide.md`
   - 📁 `versions/phase-125-final-optimization/`

2. **보안 및 안티치트**
   - 📖 `knowledge/security_authentication_guide.md`
   - 📖 `SECURITY_GUIDE.md`

3. **모니터링 및 운영**
   - 📖 `knowledge/server_monitoring_metrics_guide.md`
   - 📁 `monitoring/` - Prometheus/Grafana 설정

### Phase 4: 통합 및 배포 (5주차)

1. **배포 준비**
   - 📖 `DEPLOYMENT_GUIDE.md`
   - 📁 `versions/mvp6-deployment/`
   - 📁 `k8s/` - Kubernetes 설정

2. **부하 테스트**
   - 📖 `tests/load_test/load_test_plan.md`
   - 실행: `tests/performance/test_load_capacity.cpp`

3. **프로덕션 고려사항**
   - 📖 `PRODUCTION_REALITY_CHECK.md`
   - 📖 `FINAL_EXECUTION_GUIDE.md`

## 💻 실습 가이드

### 코드 실습 순서

1. **단위 테스트로 시작**
   ```bash
   # ECS 시스템 이해
   cd tests/unit
   ./test_ecs_system
   
   # 네트워킹 테스트
   ./test_networking
   ```

2. **버전별 코드 비교**
   ```bash
   # 기본 ECS vs 최적화 ECS
   diff versions/mvp2-ecs-basic/src/ecs/ versions/mvp2-ecs-optimized/src/ecs/
   ```

3. **통합 테스트 실행**
   ```bash
   cd tests/integration
   ./test_game_flow
   ```

### 문서 활용법

1. **개발 과정 추적**
   - 📖 `DEVELOPMENT_JOURNEY.md` - 126단계 개발 과정
   - 각 단계별 결정과 문제 해결 방법 학습

2. **테스트 개발 이해**
   - 📖 `TEST_JOURNEY.md` - 테스트 작성 과정
   - 버그 발견과 수정 과정 학습

3. **API 참조**
   - 📖 `API_DOCUMENTATION.md` - 전체 API 명세
   - 프로토콜 버퍼: `proto/` 디렉토리

## 🔧 디버깅 팁

1. **로그 분석**
   - spdlog 설정: `src/core/logging/logger.h`
   - 로그 레벨 조정으로 문제 추적

2. **성능 프로파일링**
   - 📁 `src/core/monitoring/` - 메트릭 수집
   - Grafana 대시보드 활용

3. **메모리 디버깅**
   - AddressSanitizer 사용
   - Valgrind로 메모리 누수 체크

## 📊 학습 체크포인트

- [ ] TCP 서버를 직접 구현할 수 있다
- [ ] ECS 패턴을 이해하고 적용할 수 있다
- [ ] 동시성 문제를 해결할 수 있다
- [ ] 공간 분할 알고리즘을 선택할 수 있다
- [ ] 성능 최적화 기법을 적용할 수 있다
- [ ] 부하 테스트를 수행할 수 있다
- [ ] 프로덕션 배포를 할 수 있다

## 🎓 추가 학습 자료

- **실시간 시스템**: `knowledge/realtime_physics_synchronization.md`
- **분산 시스템**: `knowledge/microservices_architecture_guide.md`
- **빅데이터 파이프라인**: `knowledge/realtime_analytics_bigdata_pipeline.md`
- **차세대 기술**: `knowledge/next_gen_game_tech.md`

## ❓ 자주 묻는 질문

**Q: 어느 버전부터 시작해야 하나요?**
A: MVP0부터 순차적으로 진행하세요. 각 MVP는 이전 내용을 기반으로 합니다.

**Q: Grid와 Octree 중 뭘 선택해야 하나요?**
A: `DESIGN_TRADEOFFS.md`와 `spatial_systems_performance_comparison.md`를 참고하세요.

**Q: 프로덕션과의 차이점은?**
A: `PRODUCTION_REALITY_CHECK.md`에서 교육용 구현과 실제 차이를 확인하세요.