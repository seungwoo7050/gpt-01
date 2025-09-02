## MVP 10: Quest System

This MVP introduces a foundational pillar of the MMORPG experience: the quest system. This collection of systems provides the framework for storytelling, player progression, and guided content. It handles the entire lifecycle of a quest, from acceptance and objective tracking to completion and reward distribution.

### Core Quest Logic

#### `src/game/quest/quest_system.h` & `.cpp`
This is the central hub for managing a player's interaction with quests. It handles accepting new quests and turning in completed ones.
```cpp
// [SEQUENCE: MVP10-1] Quest system for managing all quest-related logic
class QuestSystem : public core::ecs::System {
public:
    // [SEQUENCE: MVP10-2] Update quest progress, check for completion
    void Update(float delta_time);
    // [SEQUENCE: MVP10-3] Player accepts a quest
    bool AcceptQuest(uint64_t player_id, uint32_t quest_id);
    // [SEQUENCE: MVP10-4] Player turns in a completed quest
    bool CompleteQuest(uint64_t player_id, uint32_t quest_id);
    // [SEQUENCE: MVP10-5] Handle a game event that could advance a quest
    void HandleEvent(const GameEvent& event);
};

// --- Implementation ---
// [SEQUENCE: MVP10-6] Update loop for the quest system
// [SEQUENCE: MVP10-7] Player accepts a quest
// [SEQUENCE: MVP10-8] Player completes a quest
// [SEQUENCE: MVP10-9] Handle a quest-related event
```

### Quest Objective Tracking

#### `src/game/quest/quest_tracker.h` & `.cpp`
This system is responsible for monitoring game events, such as killing a monster or collecting an item, and updating the player's quest objectives accordingly.
```cpp
// [SEQUENCE: MVP10-10] Quest Tracker for processing events and updating objectives
class QuestTracker {
public:
    // [SEQUENCE: MVP10-11] Process a generic game event
    void ProcessEvent(uint64_t player_id, const GameEvent& event);
private:
    // [SEQUENCE: MVP10-12] Specific event handlers
    void HandleEntityKilled(uint64_t player_id, const EntityKilledEvent& event);
    void HandleItemCollected(uint64_t player_id, const ItemCollectedEvent& event);
};

// --- Implementation ---
// [SEQUENCE: MVP10-13] Process a generic game event for quest tracking
// [SEQUENCE: MVP10-14] Handle entity killed event
// [SEQUENCE: MVP10-15] Handle item collected event
```

### Quest Rewards

#### `src/game/quest/quest_reward_system.h` & `.cpp`
Upon quest completion, this system calculates and distributes the appropriate rewards to the player, such as experience points, currency, and items.
```cpp
// [SEQUENCE: MVP10-16] Quest reward system for calculating and distributing rewards
class QuestRewardSystem {
public:
    // [SEQUENCE: MVP10-17] Grant rewards for a completed quest
    void GrantRewards(uint64_t player_id, uint32_t quest_id);
private:
    // [SEQUENCE: MVP10-18] Calculate rewards based on quest data and player level
    QuestRewards CalculateRewards(uint64_t player_id, const QuestData& quest);
};

// --- Implementation ---
// [SEQUENCE: MVP10-19] Grant rewards for a quest
// [SEQUENCE: MVP10-20] Calculate rewards
```

### 프로덕션 고려사항 (Production Considerations)
*   **데이터 관리:** 퀘스트는 게임 콘텐츠의 핵심이므로, 모든 퀘스트 정보는 100% 데이터베이스나 외부 파일(JSON, XML 등)에서 로드되어야 합니다. 퀘스트의 제목, 설명, 목표, 선행 조건, 보상, 관련 NPC, 대화 스크립트까지 모두 데이터화하여 기획자가 자유롭게 퀘스트를 추가하고 수정할 수 있어야 합니다.
    ```json
    // 예시: data/quests/101.json
    {
      "id": 101,
      "title": "A Growing Threat",
      "level": 5,
      "prerequisite_quest": 98,
      "start_npc": 50,
      "end_npc": 51,
      "objectives": [
        { "type": "KILL", "target_id": 205, "count": 10, "description": "Slay 10 Vicious Wolves" },
        { "type": "COLLECT", "item_id": 3001, "count": 5, "description": "Collect 5 Wolf Pelts" }
      ],
      "rewards": {
        "experience": 1200,
        "gold": 500,
        "items": [ { "item_id": 401, "count": 1 } ]
      }
    }
    ```
*   **API 및 운영 도구:** GM이 플레이어의 퀘스트 관련 문제를 해결해주기 위한 명령어는 필수입니다. `/quest status <player>` (플레이어의 퀘스트 진행 상태 조회), `/quest start <player> <quest_id>` (퀘스트 강제 시작), `/quest complete <player> <quest_id>` (퀘스트 강제 완료), `/quest reset <player> <quest_id>` (퀘스트 초기화) 등의 기능이 필요합니다.
*   **성능 및 확장성:** 모든 게임 이벤트(몬스터 처치, 아이템 획득 등)가 발생할 때마다 모든 플레이어의 전체 퀘스트 목록을 확인하는 것은 극심한 성능 저하를 유발합니다. 프로덕션 환경에서는 **이벤트 구독 모델**을 사용해야 합니다. 플레이어가 퀘스트를 받으면, 해당 퀘스트 목표와 관련된 이벤트(예: '늑대 몬스터 처치 이벤트')에만 '구독'합니다. 이벤트가 발생하면, 해당 이벤트를 구독한 플레이어들에게만 알림이 가므로 불필요한 연산을 크게 줄일 수 있습니다.
*   **보안:** 퀘스트 완료 판정은 전적으로 서버가 수행해야 합니다. 클라이언트가 "늑대 10마리를 다 잡았다"는 메시지를 보내는 것을 믿어서는 안 됩니다. 서버의 `QuestTracker`가 `CombatSystem`으로부터 '몬스터 처치' 이벤트를 직접 수신하여 내부적으로 카운트를 증가시키고, 목표가 달성되었을 때 퀘스트 상태를 '완료 가능'으로 변경해야 합니다. 보상 지급 역시 서버가 모든 조건을 검증한 후에만 이루어져야 합니다.


