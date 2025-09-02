# 기술 면접 행동 질문

## 1. 프로젝트 경험

### Q: 가장 도전적이었던 프로젝트에 대해 설명해주세요.

**STAR 방식 답변 예시:**

**Situation (상황):**
"이전 회사에서 기존 턴제 게임을 실시간 PvP로 전환하는 프로젝트를 맡았습니다. 기존 서버는 요청-응답 방식이었고, 동시접속 1만명을 목표로 했습니다."

**Task (과제):**
"레이턴시를 50ms 이하로 유지하면서 실시간 동기화를 구현해야 했고, 기존 코드베이스를 최대한 재사용해야 했습니다."

**Action (행동):**
```cpp
// 1. 이벤트 기반 아키텍처로 전환
class RealtimeGameServer {
    EventLoop eventLoop;
    ConnectionManager connections;
    
    void Initialize() {
        // TCP + UDP 하이브리드 구조
        tcpServer.OnConnection([this](Connection conn) {
            connections.Add(conn);
            conn.EnableUDP();
        });
        
        // 중요 데이터는 TCP, 위치 업데이트는 UDP
        udpServer.OnPacket([this](Packet packet) {
            ProcessRealtimeUpdate(packet);
        });
    }
};

// 2. 상태 예측 및 보간
class ClientPrediction {
    void PredictMovement(Player& player) {
        // 클라이언트 예측
        player.predictedPos = player.pos + player.velocity * deltaTime;
        
        // 서버 조정 시 부드러운 보간
        if (serverCorrection) {
            player.pos = Lerp(player.predictedPos, serverPos, 0.1f);
        }
    }
};
```

**Result (결과):**
"평균 레이턴시 35ms 달성, 동시접속 1.5만명 지원, 플레이어 만족도 40% 향상. 이 경험으로 실시간 네트워킹의 깊은 이해를 얻었습니다."

### Q: 기술적 의견 충돌이 있었던 경험은?

**답변 예시:**

"동료가 모든 게임 로직을 마이크로서비스로 분리하자고 제안했지만, 저는 레이턴시 우려를 제기했습니다.

**해결 과정:**
1. **데이터 수집**: 프로토타입으로 성능 측정
2. **객관적 분석**: 네트워크 홉 증가로 20ms 추가 레이턴시
3. **절충안 제시**: 코어 게임플레이는 모놀리식, 주변 기능은 마이크로서비스
4. **합의 도출**: 하이브리드 아키텍처 채택

**배운 점:** 기술적 결정은 데이터 기반으로, 트레이드오프를 명확히 제시하는 것이 중요"

## 2. 문제 해결 능력

### Q: 프로덕션에서 발생한 심각한 버그를 해결한 경험은?

**답변 예시:**

"새벽 3시에 서버 메모리 사용률 급증으로 호출받았습니다.

**문제 분석:**
```cpp
// 문제 코드
class PlayerManager {
    unordered_map<int, shared_ptr<Player>> players;
    
    void OnPlayerDisconnect(int playerId) {
        // players.erase(playerId); // 이 줄이 누락됨!
        // 메모리 누수 발생
    }
};
```

**해결 과정:**
1. **즉시 대응**: 서버 재시작으로 임시 해결
2. **원인 분석**: 
   - 힙 덤프 분석
   - 플레이어 객체 수 이상 증가 발견
   - 코드 리뷰로 누락된 정리 코드 발견
3. **영구 해결**:
   ```cpp
   class PlayerManager {
       void OnPlayerDisconnect(int playerId) {
           auto it = players.find(playerId);
           if (it != players.end()) {
               // 정리 작업
               it->second->Cleanup();
               players.erase(it);
               
               // 모니터링 추가
               metrics.RecordPlayerCount(players.size());
           }
       }
   };
   ```
4. **재발 방지**:
   - 자동화된 메모리 누수 테스트 추가
   - 코드 리뷰 체크리스트에 리소스 정리 항목 추가

**결과:** 3개월간 메모리 관련 이슈 0건"

### Q: 데드라인이 촉박한 상황을 어떻게 관리했나요?

**답변 예시:**

"게임 출시 2주 전, 핵심 매칭 시스템에서 심각한 성능 이슈 발견.

**대응 전략:**
1. **우선순위 재조정**:
   - P0: 매칭 성능 (게임 불가)
   - P1: 밸런스 조정
   - P2: UI 개선 (연기)

2. **팀 재편성**:
   - 매칭 시스템에 시니어 2명 추가 투입
   - 다른 태스크는 주니어에게 위임

3. **단계적 해결**:
   ```cpp
   // Phase 1: Quick Fix (1일)
   // 매칭 풀 크기 제한으로 임시 해결
   const int MAX_MATCHING_POOL = 1000;
   
   // Phase 2: 알고리즘 개선 (3일)
   // O(n²) → O(n log n)으로 최적화
   
   // Phase 3: 캐싱 도입 (2일)
   // 자주 매칭되는 그룹 미리 계산
   ```

4. **지속적 커뮤니케이션**:
   - 일일 스탠드업을 2회로 증가
   - 경영진에 일일 진행상황 보고

**결과:** 데드라인 내 출시, 매칭 시간 5초→0.5초 단축"

## 3. 팀워크와 리더십

### Q: 주니어 개발자를 멘토링한 경험은?

**답변 예시:**

"신입 개발자에게 실시간 전투 시스템 모듈을 맡겼을 때:

**멘토링 접근법:**
1. **단계적 학습**:
   - Week 1: 기존 코드 읽고 문서화
   - Week 2: 작은 버그 수정
   - Week 3: 새 기능 구현
   - Week 4: 코드 리뷰 주도

2. **페어 프로그래밍**:
   ```cpp
   // 함께 작성한 코드
   class CombatSystem {
       // 주니어: 기본 구조 작성
       void Attack(Player& attacker, Player& target) {
           int damage = CalculateDamage(attacker, target);
           target.TakeDamage(damage);
       }
       
       // 멘토: 동시성 이슈 설명 및 해결
       void ThreadSafeAttack(Player& attacker, Player& target) {
           lock_guard<mutex> lock(combatMutex);
           // 또는 lock-free 구조 설명
           Attack(attacker, target);
       }
   };
   ```

3. **자율성 부여**:
   - 설계 결정에 참여
   - 코드 리뷰어로 활동
   - 기술 공유 세션 발표

**결과:** 
- 3개월 후 독립적으로 기능 개발
- 팀 생산성 20% 향상
- 주니어가 '올해의 신인상' 수상"

### Q: 의견이 다른 팀원과 협업한 경험은?

**답변 예시:**

"서버 아키텍처 선택에서 팀원과 의견 차이:
- 나: 검증된 모놀리식 구조 선호
- 동료: 최신 마이크로서비스 주장

**협업 과정:**
1. **서로의 관점 이해**:
   - 각자 장단점 발표
   - 실제 사례 공유

2. **실험적 접근**:
   - 2주간 PoC 개발
   - 성능, 복잡도, 유지보수성 측정

3. **데이터 기반 결정**:
   ```
   모놀리식: 레이턴시 10ms, 배포 30분
   마이크로서비스: 레이턴시 25ms, 배포 10분
   ```

4. **하이브리드 솔루션**:
   - 코어: 모놀리식 (성능 중요)
   - 부가기능: 마이크로서비스 (유연성)

**배운 점:** 
- 기술 선택은 팀 결정
- 실험과 데이터가 설득력
- 절충안이 최선일 수 있음"

## 4. 성장과 학습

### Q: 최신 기술을 학습하고 적용한 경험은?

**답변 예시:**

"C++20 코루틴을 게임 서버에 도입한 경험:

**학습 과정:**
1. **자료 학습**: CppCon 발표, 공식 문서
2. **실습**: 간단한 비동기 서버 구현
3. **팀 공유**: 내부 기술 세미나 진행

**적용 사례:**
```cpp
// 기존 콜백 지옥
void LoginPlayer(int playerId) {
    LoadPlayerData(playerId, [](PlayerData data) {
        ValidateData(data, [](bool valid) {
            if (valid) {
                InitializePlayer(data, [](Player player) {
                    // 깊은 중첩...
                });
            }
        });
    });
}

// 코루틴 적용 후
task<void> LoginPlayerCoroutine(int playerId) {
    auto data = co_await LoadPlayerDataAsync(playerId);
    bool valid = co_await ValidateDataAsync(data);
    if (valid) {
        auto player = co_await InitializePlayerAsync(data);
        // 깔끔한 흐름
    }
}
```

**결과:**
- 코드 가독성 50% 향상
- 비동기 버그 70% 감소
- 팀 전체가 코루틴 도입"

### Q: 실패한 프로젝트에서 배운 점은?

**답변 예시:**

"블록체인 기반 게임 아이템 거래 시스템 프로젝트:

**실패 원인:**
1. **기술적 과대평가**: 블록체인 처리 속도 한계
2. **사용자 니즈 부재**: 복잡성 대비 이점 부족
3. **비용 문제**: 가스비로 인한 거래 비용

**배운 교훈:**
1. **기술 검증 우선**: PoC로 실현 가능성 확인
2. **사용자 중심 사고**: 기술보다 가치가 중요
3. **점진적 접근**: 작게 시작해서 검증

**개선된 접근법:**
- 하이브리드 방식: 중요 아이템만 블록체인
- 사이드체인 활용으로 비용 절감
- A/B 테스트로 사용자 반응 확인

**적용 결과:**
다음 프로젝트에서는 MVP 우선 개발로 리스크 최소화"

## 5. 커뮤니케이션

### Q: 비기술직 팀원에게 기술적 내용을 설명한 경험은?

**답변 예시:**

"기획팀에 서버 용량 증설 필요성 설명:

**비유 활용:**
'서버는 식당, 플레이어는 손님입니다. 
- 현재: 100석 식당에 150명 대기
- 문제: 주문 지연, 서비스 품질 저하
- 해결: 식당 확장 또는 체인점 추가'

**시각화:**
```
현재 상황:
[서버1] ████████████ 120% (과부하)

제안:
[서버1] ████████░░░░ 60%
[서버2] ████████░░░░ 60%
```

**ROI 제시:**
- 비용: 월 $2,000 추가
- 효과: 
  - 접속 대기 시간 5분→30초
  - 이탈률 30%→5%
  - 예상 매출 증가: 월 $10,000

**결과:** 즉시 승인 및 예산 확보"

## 면접 팁

1. **STAR 기법 활용**: 구체적 상황과 결과 제시
2. **숫자로 표현**: 개선율, 처리량 등 정량화
3. **실패도 긍정적으로**: 배운 점 강조
4. **팀워크 강조**: 혼자가 아닌 팀의 성과
5. **지속적 성장**: 학습 의지와 적용 사례