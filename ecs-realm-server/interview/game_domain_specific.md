# 게임 서버 특화 질문

## 1. 게임 밸런싱과 서버

### Q: 게임 밸런스 데이터를 서버에서 어떻게 관리하나요?

**답변 예시:**

```cpp
// 1. 데이터 주도 설계
class BalanceDataManager {
    struct SkillData {
        int skillId;
        float damage;
        float cooldown;
        float manaCost;
        string formula;  // "ATK * 1.5 + 100"
        
        json conditions;  // 발동 조건
        json effects;     // 버프/디버프
    };
    
    unordered_map<int, SkillData> skills;
    
    // 런타임 리로드 지원
    void ReloadBalanceData() {
        // JSON/CSV에서 로드
        auto data = LoadFromFile("balance_data.json");
        
        // 원자적 교체
        unordered_map<int, SkillData> newSkills;
        for (const auto& skillJson : data["skills"]) {
            SkillData skill;
            skill.skillId = skillJson["id"];
            skill.damage = skillJson["damage"];
            skill.cooldown = skillJson["cooldown"];
            // ...
            newSkills[skill.skillId] = skill;
        }
        
        // 스왑으로 안전한 교체
        skills.swap(newSkills);
        
        LOG_INFO("Balance data reloaded: {} skills", skills.size());
    }
    
    // 수식 평가기
    float EvaluateFormula(const string& formula, const CombatContext& context) {
        // 간단한 수식 파서
        ExpressionParser parser;
        parser.SetVariable("ATK", context.attacker.attack);
        parser.SetVariable("DEF", context.target.defense);
        parser.SetVariable("LEVEL", context.attacker.level);
        
        return parser.Evaluate(formula);
    }
};

// 2. A/B 테스트 지원
class BalanceExperiment {
    void ApplyExperimentalBalance(Player& player) {
        // 플레이어를 실험 그룹에 할당
        uint32_t group = Hash(player.id) % 100;
        
        if (group < 10) {  // 10% 실험군
            // 실험적 밸런스 적용
            player.balanceVersion = "experiment_v2";
            ApplyExperimentalSkillData(player);
        } else {
            player.balanceVersion = "stable";
        }
        
        // 메트릭 수집
        analytics.Track("balance_group_assigned", {
            {"player_id", player.id},
            {"group", player.balanceVersion}
        });
    }
};
```

### Q: 실시간으로 밸런스를 조정하는 시스템을 어떻게 구현하나요?

**답변 예시:**

```cpp
class DynamicBalanceSystem {
    // 실시간 밸런스 조정
    void AdjustBalance() {
        // 1. 통계 데이터 수집
        auto stats = CollectGameplayStats();
        
        // 2. 이상 패턴 감지
        for (const auto& [skillId, usage] : stats.skillUsage) {
            float usageRate = usage.count / (float)stats.totalSkillUses;
            
            if (usageRate > 0.3f) {  // 30% 이상 사용
                // 너무 강한 스킬 - 너프
                skills[skillId].damage *= 0.95f;
                LOG_WARNING("Skill {} overused ({}%), nerfing", skillId, usageRate * 100);
            } else if (usageRate < 0.01f) {  // 1% 미만 사용
                // 너무 약한 스킬 - 버프
                skills[skillId].damage *= 1.05f;
                LOG_WARNING("Skill {} underused ({}%), buffing", skillId, usageRate * 100);
            }
        }
        
        // 3. 승률 기반 조정
        for (const auto& [classId, winRate] : stats.classWinRates) {
            if (winRate > 0.55f) {  // 55% 이상 승률
                AdjustClassBalance(classId, -0.02f);  // 2% 너프
            } else if (winRate < 0.45f) {  // 45% 미만 승률
                AdjustClassBalance(classId, 0.02f);   // 2% 버프
            }
        }
    }
    
    // 핫픽스 시스템
    void ApplyHotfix(const BalanceHotfix& hotfix) {
        // 버전 체크
        if (hotfix.targetVersion != currentVersion) {
            LOG_ERROR("Hotfix version mismatch");
            return;
        }
        
        // 변경사항 적용
        for (const auto& change : hotfix.changes) {
            switch (change.type) {
                case ChangeType::SKILL_DAMAGE:
                    skills[change.targetId].damage = change.newValue;
                    break;
                case ChangeType::ITEM_STATS:
                    items[change.targetId].stats = change.newStats;
                    break;
            }
        }
        
        // 모든 플레이어에게 알림
        BroadcastBalanceUpdate(hotfix);
    }
};
```

## 2. 핵/치트 대응

### Q: 서버에서 치트를 어떻게 감지하고 방지하나요?

**답변 예시:**

```cpp
class AntiCheatSystem {
    // 1. 서버 권한 검증
    bool ValidatePlayerAction(const Player& player, const Action& action) {
        // 위치 검증
        if (!ValidatePosition(player, action)) {
            LOG_WARNING("Invalid position: Player {} at {}", player.id, action.position);
            return false;
        }
        
        // 속도 검증
        if (!ValidateSpeed(player, action)) {
            LOG_WARNING("Speed hack detected: Player {} speed {}", player.id, action.speed);
            RecordViolation(player.id, ViolationType::SPEED_HACK);
            return false;
        }
        
        // 쿨다운 검증
        if (!ValidateCooldown(player, action)) {
            LOG_WARNING("Cooldown hack: Player {} skill {}", player.id, action.skillId);
            RecordViolation(player.id, ViolationType::COOLDOWN_HACK);
            return false;
        }
        
        return true;
    }
    
    // 2. 행동 패턴 분석
    class BehaviorAnalyzer {
        struct PlayerBehavior {
            RollingAverage<float> apm{300};           // Actions per minute
            RollingAverage<float> accuracy{100};      // 명중률
            RollingAverage<float> reactionTime{100};  // 반응 시간
            
            vector<Vector3> positionHistory;
            vector<float> damageHistory;
        };
        
        map<PlayerId, PlayerBehavior> behaviors;
        
        void AnalyzePlayer(const Player& player) {
            auto& behavior = behaviors[player.id];
            
            // 비정상적 APM
            if (behavior.apm.GetAverage() > 400) {  // 인간 한계 초과
                FlagSuspiciousActivity(player.id, "Abnormal APM");
            }
            
            // 비정상적 정확도
            if (behavior.accuracy.GetAverage() > 0.95f && 
                behavior.apm.GetAverage() > 200) {
                FlagSuspiciousActivity(player.id, "Aimbot suspected");
            }
            
            // 텔레포트 감지
            if (DetectTeleport(behavior.positionHistory)) {
                FlagSuspiciousActivity(player.id, "Teleport detected");
            }
            
            // 데미지 이상치
            if (DetectDamageAnomaly(behavior.damageHistory)) {
                FlagSuspiciousActivity(player.id, "Damage hack");
            }
        }
    };
    
    // 3. 메모리 무결성 검사
    class MemoryIntegrityChecker {
        struct CriticalValue {
            void* address;
            size_t size;
            uint32_t checksum;
        };
        
        vector<CriticalValue> criticalValues;
        
        void RegisterCriticalValue(void* addr, size_t size) {
            CriticalValue cv;
            cv.address = addr;
            cv.size = size;
            cv.checksum = CalculateChecksum(addr, size);
            
            criticalValues.push_back(cv);
        }
        
        bool VerifyIntegrity() {
            for (const auto& cv : criticalValues) {
                uint32_t currentChecksum = CalculateChecksum(cv.address, cv.size);
                if (currentChecksum != cv.checksum) {
                    LOG_ERROR("Memory tampering detected at {}", cv.address);
                    return false;
                }
            }
            return true;
        }
    };
    
    // 4. 제재 시스템
    void ApplySanction(PlayerId playerId, ViolationType type) {
        auto& record = violationRecords[playerId];
        record.violations.push_back({type, GetCurrentTime()});
        
        // 누적 제재
        if (record.violations.size() >= 3) {
            if (record.violations.size() == 3) {
                // 첫 제재: 경고
                SendWarning(playerId, "Suspicious activity detected");
            } else if (record.violations.size() == 5) {
                // 임시 정지
                TempBan(playerId, 24h);
            } else if (record.violations.size() >= 10) {
                // 영구 정지
                PermBan(playerId, "Multiple violations");
            }
        }
        
        // 증거 보관
        SaveEvidence(playerId, type);
    }
};
```

### Q: 봇 탐지 시스템을 어떻게 구현하나요?

**답변 예시:**

```cpp
class BotDetectionSystem {
    // 1. 휴리스틱 기반 탐지
    bool DetectBotBehavior(const Player& player) {
        // 입력 패턴 분석
        auto inputPattern = AnalyzeInputPattern(player.inputHistory);
        
        // 너무 규칙적인 패턴
        if (inputPattern.regularityScore > 0.9f) {
            return true;
        }
        
        // 24시간 연속 플레이
        if (player.sessionDuration > 24h && player.idleTime < 5min) {
            return true;
        }
        
        // 반복적인 경로
        if (DetectPathRepetition(player.movementHistory)) {
            return true;
        }
        
        return false;
    }
    
    // 2. 머신러닝 기반 탐지
    class MLBotDetector {
        struct FeatureVector {
            float avgAPM;
            float apmVariance;
            float pathEntropy;
            float socialInteraction;
            float economicBehavior;
            float combatPattern;
        };
        
        NeuralNetwork model;
        
        float PredictBotProbability(const Player& player) {
            FeatureVector features = ExtractFeatures(player);
            return model.Predict(features);
        }
        
        FeatureVector ExtractFeatures(const Player& player) {
            return {
                .avgAPM = CalculateAPM(player),
                .apmVariance = CalculateAPMVariance(player),
                .pathEntropy = CalculateMovementEntropy(player),
                .socialInteraction = GetSocialScore(player),
                .economicBehavior = GetEconomicScore(player),
                .combatPattern = GetCombatScore(player)
            };
        }
    };
    
    // 3. 캡차 시스템
    void ChallengePlayer(Player& player) {
        // 게임플레이 중 자연스러운 검증
        GameplayChallenge challenge = GenerateChallenge();
        
        // 예: "빨간색 크리스탈 3개를 10초 안에 수집하세요"
        player.SendChallenge(challenge);
        
        // 타임아웃 설정
        scheduler.Schedule(10s, [&]() {
            if (!challenge.completed) {
                player.failedChallenges++;
                
                if (player.failedChallenges >= 3) {
                    KickPlayer(player, "Failed bot verification");
                }
            }
        });
    }
};
```

## 3. 실시간 PvP 구현

### Q: 래그 보상(Lag Compensation)을 어떻게 구현하나요?

**답변 예시:**

```cpp
class LagCompensationSystem {
    // 1. 되감기(Rewind) 시스템
    class RewindSystem {
        static constexpr size_t HISTORY_SIZE = 60;  // 1초 (60 ticks)
        
        struct WorldSnapshot {
            uint32_t tick;
            chrono::milliseconds timestamp;
            vector<PlayerState> playerStates;
            vector<ProjectileState> projectileStates;
        };
        
        circular_buffer<WorldSnapshot> history{HISTORY_SIZE};
        
        void RecordSnapshot() {
            WorldSnapshot snapshot;
            snapshot.tick = currentTick;
            snapshot.timestamp = GetServerTime();
            
            // 모든 엔티티 상태 저장
            for (const auto& player : players) {
                snapshot.playerStates.push_back(player.GetState());
            }
            
            history.push_back(snapshot);
        }
        
        bool RewindAndValidate(const HitEvent& hit, int playerLatency) {
            // 클라이언트가 쏜 시점으로 되감기
            auto targetTime = GetServerTime() - chrono::milliseconds(playerLatency);
            
            // 해당 시점의 스냅샷 찾기
            auto snapshot = FindSnapshot(targetTime);
            if (!snapshot) return false;
            
            // 그 시점의 월드 상태로 복원
            RestoreWorldState(*snapshot);
            
            // 명중 판정 재검증
            bool isValidHit = ValidateHit(hit);
            
            // 현재 상태로 복원
            RestoreCurrentState();
            
            return isValidHit;
        }
    };
    
    // 2. 클라이언트 예측 조정
    class ClientPredictionReconciliation {
        void ReconcilePlayerState(Player& player, const ClientState& clientState) {
            // 서버와 클라이언트 상태 차이 계산
            Vector3 positionError = player.serverPos - clientState.predictedPos;
            
            // 오차가 크면 즉시 보정
            if (positionError.Length() > MAX_PREDICTION_ERROR) {
                CorrectionPacket packet;
                packet.serverPosition = player.serverPos;
                packet.serverVelocity = player.velocity;
                packet.serverTick = currentTick;
                
                player.SendCorrection(packet);
            } else {
                // 작은 오차는 부드럽게 보간
                player.positionCorrection = positionError;
                player.correctionRate = 0.1f;  // 10프레임에 걸쳐 보정
            }
        }
    };
    
    // 3. 인터폴레이션/외삽
    class InterpolationSystem {
        void InterpolateEntities(float renderTime) {
            float targetTime = GetServerTime() - INTERPOLATION_DELAY;
            
            for (auto& entity : entities) {
                // 두 스냅샷 사이 보간
                auto [before, after] = FindSnapshots(targetTime);
                
                if (before && after) {
                    float t = (targetTime - before->time) / (after->time - before->time);
                    
                    entity.renderPos = Lerp(before->position, after->position, t);
                    entity.renderRot = Slerp(before->rotation, after->rotation, t);
                } else {
                    // 외삽 (최신 데이터만 있을 때)
                    entity.renderPos = entity.position + entity.velocity * EXTRAPOLATION_TIME;
                }
            }
        }
    };
};
```

### Q: 스킬 동기화는 어떻게 처리하나요?

**답변 예시:**

```cpp
class SkillSynchronization {
    // 1. 스킬 타임라인 시스템
    class SkillTimeline {
        struct SkillEvent {
            uint32_t casterId;
            uint32_t skillId;
            uint32_t targetId;
            
            chrono::milliseconds startTime;
            chrono::milliseconds castTime;
            chrono::milliseconds duration;
            
            enum Phase { CASTING, ACTIVE, COOLDOWN } phase;
        };
        
        multimap<chrono::milliseconds, SkillEvent> timeline;
        
        void ProcessSkillRequest(const SkillRequest& request) {
            // 1. 검증
            if (!ValidateSkillUse(request)) {
                SendSkillFailed(request.playerId, "Invalid skill use");
                return;
            }
            
            // 2. 스킬 이벤트 생성
            SkillEvent event;
            event.casterId = request.playerId;
            event.skillId = request.skillId;
            event.startTime = GetServerTime() + GetPlayerLatency(request.playerId);
            
            // 3. 타임라인에 추가
            timeline.emplace(event.startTime, event);
            
            // 4. 모든 클라이언트에 브로드캐스트
            BroadcastSkillStart(event);
        }
        
        void UpdateTimeline() {
            auto now = GetServerTime();
            
            for (auto& [time, event] : timeline) {
                if (time > now) break;
                
                switch (event.phase) {
                    case Phase::CASTING:
                        if (now >= time + event.castTime) {
                            ActivateSkill(event);
                            event.phase = Phase::ACTIVE;
                        }
                        break;
                        
                    case Phase::ACTIVE:
                        if (now >= time + event.castTime + event.duration) {
                            EndSkill(event);
                            event.phase = Phase::COOLDOWN;
                        }
                        break;
                }
            }
        }
    };
    
    // 2. 충돌 해결
    class SkillConflictResolver {
        void ResolveConflicts(SkillEvent& event1, SkillEvent& event2) {
            // 우선순위 기반 해결
            int priority1 = GetSkillPriority(event1.skillId);
            int priority2 = GetSkillPriority(event2.skillId);
            
            if (priority1 > priority2) {
                CancelSkill(event2);
            } else if (priority2 > priority1) {
                CancelSkill(event1);
            } else {
                // 같은 우선순위면 먼저 시작한 스킬 우선
                if (event1.startTime < event2.startTime) {
                    CancelSkill(event2);
                } else {
                    CancelSkill(event1);
                }
            }
        }
    };
};
```

## 4. 게임 경제 시스템

### Q: 인플레이션을 방지하는 경제 시스템을 어떻게 설계하나요?

**답변 예시:**

```cpp
class GameEconomySystem {
    // 1. 통화 총량 관리
    class CurrencyController {
        struct EconomyMetrics {
            uint64_t totalGoldInCirculation;
            uint64_t dailyGoldGenerated;
            uint64_t dailyGoldSink;
            float averagePlayerWealth;
            float wealthGiniCoefficient;  // 빈부격차 지표
        };
        
        EconomyMetrics metrics;
        
        void RegulateEconomy() {
            // 인플레이션 감지
            float inflationRate = CalculateInflationRate();
            
            if (inflationRate > 0.05f) {  // 5% 이상 인플레이션
                // 골드 싱크 증가
                IncreaseRepairCosts(1.1f);
                IncreaseTaxRates(1.05f);
                
                // 보상 감소
                ReduceQuestRewards(0.95f);
                ReduceMonsterDropRates(0.9f);
            }
            
            // 디플레이션 대응
            if (inflationRate < -0.02f) {
                // 보상 증가
                IncreaseEventRewards(1.2f);
                ReduceTaxRates(0.95f);
            }
        }
    };
    
    // 2. 거래 시스템
    class TradingSystem {
        // 시장 조작 방지
        bool ValidateTrade(const Trade& trade) {
            // 가격 이상 감지
            float marketPrice = GetMarketPrice(trade.itemId);
            float deviation = abs(trade.price - marketPrice) / marketPrice;
            
            if (deviation > 10.0f) {  // 시세의 10배 이상/이하
                LOG_WARNING("Suspicious trade: Item {} for {} gold (market: {})",
                           trade.itemId, trade.price, marketPrice);
                
                // RMT 의심
                if (trade.price < marketPrice * 0.01f) {
                    FlagForRMT(trade);
                }
                
                return false;
            }
            
            return true;
        }
        
        // 동적 세금 시스템
        float CalculateTradeTax(const Trade& trade) {
            float baseTax = 0.05f;  // 5% 기본세
            
            // 고액 거래 누진세
            if (trade.price > 10000) {
                baseTax += 0.05f;  // 10%
            }
            if (trade.price > 100000) {
                baseTax += 0.10f;  // 20%
            }
            
            // 빈번한 거래자 추가세
            int recentTrades = GetRecentTradeCount(trade.sellerId, 24h);
            if (recentTrades > 100) {
                baseTax += 0.05f;
            }
            
            return baseTax;
        }
    };
    
    // 3. 아이템 가치 관리
    class ItemEconomyManager {
        void ManageItemScarcity() {
            for (auto& [itemId, dropRate] : itemDropRates) {
                int circulation = GetItemCirculation(itemId);
                int targetCirculation = GetTargetCirculation(itemId);
                
                // 동적 드롭률 조정
                if (circulation > targetCirculation * 1.2f) {
                    dropRate *= 0.8f;  // 20% 감소
                } else if (circulation < targetCirculation * 0.8f) {
                    dropRate *= 1.2f;  // 20% 증가
                }
            }
        }
        
        // 아이템 소각 시스템
        void ImplementItemSinks() {
            // 강화 실패 시 파괴
            // 수리 시스템
            // 시간 제한 아이템
            // 소모품
        }
    };
};
```

## 5. 매칭 시스템

### Q: 공정한 매칭 시스템을 어떻게 구현하나요?

**답변 예시:**

```cpp
class MatchmakingSystem {
    // 1. ELO/MMR 시스템
    class MMRCalculator {
        float CalculateMMRChange(const MatchResult& result) {
            float winnerMMR = result.winner.mmr;
            float loserMMR = result.loser.mmr;
            
            // 예상 승률
            float expectedWinRate = 1.0f / (1.0f + pow(10, (loserMMR - winnerMMR) / 400));
            
            // K-factor (변동성)
            float K = 32.0f;
            if (result.winner.gamesPlayed < 30) K = 64.0f;  // 신규 플레이어
            if (result.winner.mmr > 2400) K = 16.0f;        // 고수
            
            // MMR 변화량
            float change = K * (1.0f - expectedWinRate);
            
            return change;
        }
    };
    
    // 2. 매칭 풀 관리
    class MatchmakingPool {
        struct MatchRequest {
            PlayerId playerId;
            float mmr;
            chrono::milliseconds queueTime;
            Region region;
            vector<PlayerId> party;  // 파티원
        };
        
        multimap<float, MatchRequest> pool;  // MMR로 정렬
        
        vector<Match> FindMatches() {
            vector<Match> matches;
            
            for (auto it = pool.begin(); it != pool.end();) {
                auto& request = it->second;
                
                // 매칭 범위 동적 확장
                float range = GetMatchRange(request.queueTime);
                
                // 적절한 상대 찾기
                auto opponent = FindBestOpponent(request, range);
                
                if (opponent != pool.end()) {
                    // 매치 생성
                    Match match;
                    match.player1 = request;
                    match.player2 = opponent->second;
                    match.mmrDiff = abs(request.mmr - opponent->second.mmr);
                    
                    matches.push_back(match);
                    
                    // 풀에서 제거
                    pool.erase(it);
                    pool.erase(opponent);
                    it = pool.begin();  // 처음부터 다시
                } else {
                    ++it;
                }
            }
            
            return matches;
        }
        
        float GetMatchRange(chrono::milliseconds waitTime) {
            // 대기 시간에 따라 범위 확장
            float baseRange = 50.0f;
            float expansion = waitTime.count() / 1000.0f * 10.0f;  // 초당 10 MMR
            return min(baseRange + expansion, 500.0f);  // 최대 500
        }
    };
    
    // 3. 팀 밸런싱
    class TeamBalancer {
        struct Team {
            vector<PlayerId> players;
            float totalMMR;
            float avgMMR;
        };
        
        pair<Team, Team> BalanceTeams(const vector<Player>& players) {
            // 탐욕 알고리즘
            vector<Player> sorted = players;
            sort(sorted.begin(), sorted.end(), 
                 [](const auto& a, const auto& b) { return a.mmr > b.mmr; });
            
            Team team1, team2;
            
            for (const auto& player : sorted) {
                if (team1.totalMMR <= team2.totalMMR) {
                    team1.players.push_back(player.id);
                    team1.totalMMR += player.mmr;
                } else {
                    team2.players.push_back(player.id);
                    team2.totalMMR += player.mmr;
                }
            }
            
            // 미세 조정
            OptimizeBalance(team1, team2);
            
            return {team1, team2};
        }
    };
};
```

## 면접 대비 포인트

1. **게임 도메인 이해**: 장르별 특성과 기술적 요구사항
2. **실제 운영 경험**: 라이브 서비스 이슈와 해결책
3. **보안 의식**: 치트/핵 대응 전략
4. **밸런스 감각**: 데이터 기반 의사결정
5. **플레이어 경험**: 기술이 게임플레이에 미치는 영향