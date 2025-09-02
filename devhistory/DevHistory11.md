## MVP 11: Social Systems

An MMORPG is defined by its community. This MVP lays the essential groundwork for player interaction by building a comprehensive suite of social systems. These features are the glue that holds a player base together, enabling communication, cooperation, and community building. This phase covers everything from basic chat and friend lists to complex guild management and secure trading.

### Chat System

#### `src/game/social/chat_system.h`
The chat system provides the primary means of communication for players, supporting various channels, moderation features, and spam prevention.
```cpp
#pragma once
// [SEQUENCE: MVP11-167] Chat system for player communication
// ... (enum ChatChannelType, struct ChatMessage, etc.) ...

// [SEQUENCE: MVP11-192] Chat manager
class ChatManager {
public:
    // [SEQUENCE: MVP11-193] Initialize system
    // [SEQUENCE: MVP11-194] Send message
    // [SEQUENCE: MVP11-195] Send whisper
    // [SEQUENCE: MVP11-196] Join custom channel
    // ... other methods
};
```

#### 프로덕션 고려사항 (Production Considerations)
*   **데이터 관리:** 욕설 필터, 스팸 패턴, 채널 설정 등은 외부 설정 파일(e.g., `chat_config.json`)에서 로드하여 GM이 실시간으로 업데이트할 수 있어야 합니다. 채팅 로그는 법적 분쟁 및 운영 이슈 해결을 위해 최소 3개월 이상 별도의 로그 서버나 데이터베이스에 저장해야 합니다.
*   **API 및 운영 도구:** `/mute <player> <duration>`, `/unmute <player>`, `/kick <player> <channel>`, `/announce <message>` 와 같은 GM 명령어가 필수적입니다. 특정 유저의 채팅 기록을 조회하거나, 반복적인 스팸 유저를 자동으로 감지하여 제재하는 시스템이 필요합니다.
*   **성능 및 확장성:** 월드 채널이나 대규모 채널은 별도의 채팅 서버(또는 마이크로서비스)로 분리하여 부하를 분산하는 것을 고려해야 합니다. 메시지 라우팅 시, 특히 지역 기반(Say, Yell) 채팅은 공간 인덱싱 시스템과 연동하여 불필요한 검색을 최소화해야 합니다.
*   **보안:** 클라이언트에서 오는 모든 채팅 메시지의 길이, 형식, 발송 빈도를 검증하여 버퍼 오버플로우나 서비스 거부(DoS) 공격을 방지해야 합니다. 링크나 스크립트 삽입을 원천적으로 차단해야 합니다.

### Friend System

#### `src/game/social/friend_system.h`
Allows players to build and manage a list of friends for easy communication and grouping.
```cpp
#pragma once
// [SEQUENCE: MVP11-33] Friend system for social interactions
// ... (enum FriendStatus, struct FriendInfo, etc.) ...

// [SEQUENCE: MVP11-54] Friend system manager
class FriendSystemManager {
public:
    // [SEQUENCE: MVP11-55] Initialize system
    // [SEQUENCE: MVP11-57] Process friend request
    // [SEQUENCE: MVP11-58] Process friend acceptance
    // [SEQUENCE: MVP11-59] Update player online status
    // ... other methods
};
```

#### 프로덕션 고려사항 (Production Considerations)
*   **데이터 관리:** 친구 목록, 차단 목록, 보류 중인 요청은 모두 플레이어 데이터의 일부로 데이터베이스에 영구 저장되어야 합니다. 친구와의 상호작용 기록(마지막 대화, 함께한 던전 횟수 등)도 저장하여 관계를 시각화하는 데 사용할 수 있습니다.
*   **API 및 운영 도구:** 유저의 친구/차단 목록을 조회하거나, 악의적인 친구 요청 스팸을 보내는 유저를 제재하는 GM 기능이 필요합니다.
*   **성능 및 확장성:** 친구의 접속 상태 업데이트는 매우 빈번하게 발생합니다. 모든 친구에게 실시간으로 알리기보다는, Pub/Sub 모델(e.g., Redis)을 사용하여 로그인/로그아웃 서버가 상태 변경을 브로드캐스트하고, 각 플레이어의 세션이 이를 구독하여 친구에게만 알리는 방식이 효율적입니다.
*   **보안:** 친구 요청 메시지에 대한 길이 및 내용 검증이 필요하며, 단기간에 과도한 친구 요청을 보내는 행위를 방지하기 위해 Rate Limiting을 적용해야 합니다.

### Guild System

#### `src/game/social/guild_system.h`
Provides the framework for players to form large, persistent organizations with ranks, a shared bank, and a unified identity.
```cpp
#pragma once
// [SEQUENCE: MVP11-65] Guild system for player organizations
// ... (enum GuildPermission, struct GuildRank, etc.) ...

// [SEQUENCE: MVP11-90] Guild manager
class GuildManager {
public:
    // [SEQUENCE: MVP11-91] Create guild
    // [SEQUENCE: MVP11-92] Get guild
    // [SEQUENCE: MVP11-95] Process guild invite
    // ... other methods
};
```

#### 프로덕션 고려사항 (Production Considerations)
*   **데이터 관리:** 길드 정보, 멤버 목록, 랭크, 은행 내역, 로그 등 모든 데이터는 데이터베이스에 저장되어야 합니다. 특히 길드 은행의 아이템과 골드 입출금 내역은 중요한 감사 로그이므로 절대 유실되어서는 안 됩니다.
*   **API 및 운영 도구:** `/gdisband <guild_name>`, `/ginvite <player>`, `/gkick <player>`, `/setmotd <message>` 등 길드 관리를 위한 GM 명령어가 필요합니다. 길드장 부재 시 리더를 위임하거나, 비활성 길드를 정리하는 등의 운영 정책이 필요합니다.
*   **성능 및 확장성:** 대규모 길드의 경우, 모든 멤버에게 길드 채팅이나 공지를 전달하는 것은 큰 부하가 될 수 있습니다. 이를 위해 별도의 길드 채널 서버를 두거나, 메시지 큐를 활용하는 방안을 고려해야 합니다. 길드 정보는 자주 변경되지 않으므로 캐싱(e.g., Redis)하여 DB 부하를 줄이는 것이 효과적입니다.
*   **보안:** 길드 은행 접근 권한, 멤버 추방/초대 권한 등 모든 권한 관련 요청은 서버에서 이중으로 검증해야 합니다. 골드 및 아이템 이동은 모두 트랜잭션으로 처리하여 데이터 정합성을 보장해야 합니다.

### Party System

#### `src/game/social/party_system.h`
Enables small groups of players to cooperate, sharing experience and loot.
```cpp
#pragma once
// [SEQUENCE: MVP11-208] Party system for group gameplay
// ... (enum PartyRole, enum LootMethod, etc.) ...

// [SEQUENCE: MVP11-230] Party manager
class PartyManager {
public:
    // [SEQUENCE: MVP11-231] Create party
    // [SEQUENCE: MVP11-232] Invite to party
    // [SEQUENCE: MVP11-233] Accept party invite
    // ... other methods
};
```

#### 프로덕션 고려사항 (Production Considerations)
*   **데이터 관리:** 파티 정보는 보통 실시간으로만 유지되며, 데이터베이스에 영구 저장할 필요는 없습니다. (단, 레이드 파티 구성은 저장될 수 있음). 하지만 파티 찾기(LFG) 시스템을 위해서는 플레이어의 역할(탱/딜/힐), 아이템 레벨 등의 정보가 필요합니다.
*   **API 및 운영 도구:** 특정 유저를 파티에서 강제로 추방하거나, 버그로 해체되지 않는 파티를 정리하는 GM 기능이 필요합니다.
*   **성능 및 확장성:** 파티원의 위치, 체력 등 상태 정보는 매우 자주 업데이트됩니다. 이 정보를 파티원들에게 브로드캐스트할 때, UDP를 사용하거나, 변경된 데이터만 보내는 델타 압축을 사용하여 네트워크 트래픽을 최적화해야 합니다.
*   **보안:** 파티 초대 스팸을 방지하기 위한 Rate Limiting이 필요하며, 아이템 분배(루팅) 권한은 서버에서 명확하게 관리하여 분쟁을 최소화해야 합니다.

### Trade System

#### `src/game/social/trade_system.h`
Implements a secure, two-step verification process for players to exchange items and currency.
```cpp
#pragma once
// [SEQUENCE: MVP11-101] Trade system for secure item/gold exchange
// ... (enum TradeState, struct TradeOffer, etc.) ...

// [SEQUENCE: MVP11-120] Trade manager
class TradeManager {
public:
    // [SEQUENCE: MVP11-121] Request trade
    // [SEQUENCE: MVP11-122] Accept trade request
    // [SEQUENCE: MVP11-126] Complete trade
    // ... other methods
};
```

#### 프로덕션 고려사항 (Production Considerations)
*   **데이터 관리:** 모든 거래 내역(참여자, 아이템, 골드, 시간)은 사기 및 어뷰징 추적을 위해 데이터베이스에 영구 로그로 기록되어야 합니다.
*   **API 및 운영 도구:** 특정 유저의 거래 기록을 조회하거나, 사기 거래가 의심될 경우 거래를 강제로 취소/롤백하는 GM 기능이 필요합니다.
*   **성능 및 확장성:** 거래는 동기적으로 처리되어야 하며, 데이터 정합성이 매우 중요합니다. 데이터베이스 트랜잭션을 사용하여 아이템과 골드가 양쪽 플레이어에게 원자적으로(atomically) 이동하는 것을 보장해야 합니다.
*   **보안:** 거래 과정의 모든 단계(아이템 추가, 잠금, 확인)는 서버에서 검증되어야 합니다. 클라이언트에서 조작된 패킷으로 거래 내용을 변경하려는 시도를 막아야 하며, 귀속 아이템이나 퀘스트 아이템이 거래되지 않도록 서버에서 확인해야 합니다.

### Mail System

#### `src/game/social/mail_system.h`
Allows for asynchronous communication and item exchange between players.
```cpp
#pragma once
// [SEQUENCE: MVP11-133] Mail system for asynchronous player communication
// ... (enum MailType, struct Mail, etc.) ...

// [SEQUENCE: MVP11-155] Mail manager
class MailManager {
public:
    // [SEQUENCE: MVP11-156] Send mail
    // [SEQUENCE: MVP11-157] Send system mail
    // [SEQUENCE: MVP11-162] Mass mail (GM function)
    // ... other methods
};
```

#### 프로덕션 고려사항 (Production Considerations)
*   **데이터 관리:** 모든 메일 내용은 데이터베이스에 저장되어야 합니다. 특히 아이템이나 골드가 첨부된 메일은 절대 유실되어서는 안 됩니다. 메일은 보관 기한(예: 30일)이 지나면 자동으로 삭제되거나, 별도의 아카이브 테이블로 이동되어야 합니다.
*   **API 및 운영 도구:** GM이 특정 유저에게 아이템/골드를 보내거나, 전체 유저에게 공지 메일을 보내는 기능이 필수적입니다. 또한, 사기/어뷰징에 사용된 메일을 추적하고 회수하는 기능도 필요합니다.
*   **성능 및 확장성:** 대량의 메일 발송(이벤트 보상 등)은 서버에 큰 부하를 줄 수 있으므로, 별도의 배치(Batch) 작업으로 처리해야 합니다. 플레이어가 로그인할 때 모든 메일을 한 번에 로드하는 대신, 제목 목록만 먼저 보여주고 본문은 클릭 시 로드하여 부하를 줄일 수 있습니다.
*   **보안:** C.O.D(Cash on Delivery) 사기나 스팸 메일을 방지하기 위한 필터링 및 Rate Limiting이 필요합니다. 첨부된 아이템의 유효성을 서버에서 반드시 검증해야 합니다.
