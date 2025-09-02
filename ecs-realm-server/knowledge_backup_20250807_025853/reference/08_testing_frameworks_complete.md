# 테스팅 프레임워크 완전 가이드 - 게임 서버 품질 보증의 핵심
*Google Test, Catch2, 벤치마킹까지 - 게임 서버를 위한 종합 테스팅 전략*

> **🎯 목표**: 게임 서버 개발에 필요한 모든 테스팅 기법을 마스터하여 버그 없는 고품질 서버를 구축하고 성능을 지속적으로 검증

---

## 📋 문서 정보

- **난이도**: 🟡 중급 → 🔴 고급
- **예상 학습 시간**: 6-8시간
- **필요 선수 지식**: 
  - ✅ C++ 기본 문법과 STL
  - ✅ CMake 기초 지식
  - ✅ 게임 서버 아키텍처 이해
- **실습 환경**: CMake 3.20+, Google Test, Catch2, Google Benchmark

---

## 🤔 왜 체계적인 테스팅이 게임 서버에 필수일까?

### **테스트 없는 게임 서버의 참사**

```cpp
// 😰 테스트 없이 개발된 게임 서버 - 재앙의 시작

class UntestGameServer {
public:
    // 문제 1: 검증되지 않은 플레이어 이동 로직
    void MovePlayer(uint64_t player_id, float x, float y) {
        auto player = players_[player_id];  // null 체크 없음!
        player->SetPosition(x, y);          // 크래시 위험!
        
        // 범위 검증 없음
        if (x > 1000000 || y > 1000000) {   // 이미 늦었음
            // 부정 이동 감지 로직 없음
        }
    }
    
    // 문제 2: 검증되지 않은 전투 시스템
    int CalculateDamage(int attack, int defense) {
        return attack - defense;  // 음수 데미지? 오버플로우?
    }
    
    // 문제 3: 테스트 불가능한 데이터베이스 로직
    void SavePlayer(Player* player) {
        auto connection = mysql_connect("localhost", "user", "pass", "db");
        // 실제 DB에만 의존 - 테스트 불가능
        mysql_query(connection, ("UPDATE players SET level=" + 
                               std::to_string(player->level)).c_str());
        mysql_close(connection);
    }
};

// 🔥 실제 운영에서 발생하는 문제들:
// - 플레이어 이동 중 서버 크래시 (null pointer)
// - 음수 데미지로 인한 플레이어 체력 증가 버그
// - 데이터베이스 연결 실패 시 플레이어 데이터 손실
// - 동시 접속자 증가 시 예상치 못한 성능 저하
// - 새 기능 추가할 때마다 기존 기능 파괴
```

### **테스트 주도로 개발된 견고한 게임 서버**

```cpp
// ✨ 철저한 테스팅으로 검증된 게임 서버

class TestedGameServer {
private:
    std::unique_ptr<DatabaseInterface> db_interface_;  // 의존성 주입
    std::unique_ptr<PlayerValidator> validator_;
    
public:
    // 의존성 주입으로 테스트 가능하게 설계
    TestedGameServer(std::unique_ptr<DatabaseInterface> db,
                    std::unique_ptr<PlayerValidator> validator)
        : db_interface_(std::move(db)), validator_(std::move(validator)) {}
    
    // 완전히 검증된 플레이어 이동 로직
    MoveResult MovePlayer(uint64_t player_id, float x, float y) {
        // 1. 입력 검증
        if (!validator_->IsValidPosition(x, y)) {
            return MoveResult::INVALID_POSITION;
        }
        
        // 2. 플레이어 존재 확인
        auto player = GetPlayerSafely(player_id);
        if (!player) {
            return MoveResult::PLAYER_NOT_FOUND;
        }
        
        // 3. 이동 권한 검증
        if (!validator_->CanPlayerMove(player.get(), x, y)) {
            return MoveResult::MOVE_NOT_ALLOWED;
        }
        
        // 4. 안전한 이동 실행
        player->SetPosition(x, y);
        return MoveResult::SUCCESS;
    }
    
    // 경계값까지 완전히 테스트된 전투 시스템
    DamageResult CalculateDamage(int attack, int defense) {
        // 오버플로우 방지
        if (attack < 0 || defense < 0) return {0, DamageResult::INVALID_INPUT};
        if (attack > MAX_ATTACK) attack = MAX_ATTACK;
        if (defense > MAX_DEFENSE) defense = MAX_DEFENSE;
        
        int damage = std::max(1, attack - defense);  // 최소 1 데미지
        return {damage, DamageResult::SUCCESS};
    }
    
    // Mock을 통해 완전히 테스트 가능한 DB 로직
    SaveResult SavePlayer(const Player& player) {
        try {
            return db_interface_->SavePlayer(player);
        } catch (const DatabaseException& e) {
            LogError("Player save failed: {}", e.what());
            return SaveResult::DATABASE_ERROR;
        }
    }
};

// ✅ 테스팅의 결과:
// - 모든 에지 케이스가 검증됨 (null, 오버플로우, 경계값)
// - 실제 DB 없이도 모든 로직 테스트 가능
// - 성능 기준이 명확하고 지속적으로 검증됨
// - 새 기능 추가 시 기존 기능 영향도 자동 검증
// - 코드 변경 시 즉시 회귀 테스트로 안정성 확보
```

---

## 📚 1. Google Test 마스터하기

### **1.1 Google Test 기초부터 고급까지**

```cpp
// 🧪 Google Test로 게임 서버 핵심 로직 테스트

#include <gtest/gtest.h>
#include <gmock/gmock.h>

// 테스트 대상 클래스
class Player {
private:
    uint64_t id_;
    std::string name_;
    int level_;
    int health_;
    float position_x_, position_y_;
    
public:
    Player(uint64_t id, const std::string& name, int level)
        : id_(id), name_(name), level_(level), health_(level * 100),
          position_x_(0.0f), position_y_(0.0f) {}
    
    // Getters
    uint64_t GetId() const { return id_; }
    const std::string& GetName() const { return name_; }
    int GetLevel() const { return level_; }
    int GetHealth() const { return health_; }
    float GetX() const { return position_x_; }
    float GetY() const { return position_y_; }
    
    // 테스트할 핵심 메서드들
    void TakeDamage(int damage) {
        if (damage > 0) {
            health_ = std::max(0, health_ - damage);
        }
    }
    
    void Heal(int amount) {
        if (amount > 0) {
            int max_health = level_ * 100;
            health_ = std::min(max_health, health_ + amount);
        }
    }
    
    void SetPosition(float x, float y) {
        position_x_ = x;
        position_y_ = y;
    }
    
    bool IsAlive() const { return health_ > 0; }
    
    void LevelUp() {
        level_++;
        int old_max = (level_ - 1) * 100;
        int new_max = level_ * 100;
        health_ += (new_max - old_max);  // 레벨업 시 체력 증가
    }
};

// 1. 기본 테스트 케이스들
class PlayerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 각 테스트마다 실행되는 초기화 코드
        test_player_ = std::make_unique<Player>(12345, "TestHero", 10);
    }
    
    void TearDown() override {
        // 각 테스트 후 정리 코드
        test_player_.reset();
    }
    
    std::unique_ptr<Player> test_player_;
    
    // 헬퍼 메서드들
    void ExpectPlayerState(int expected_level, int expected_health, bool expected_alive) {
        EXPECT_EQ(test_player_->GetLevel(), expected_level);
        EXPECT_EQ(test_player_->GetHealth(), expected_health);
        EXPECT_EQ(test_player_->IsAlive(), expected_alive);
    }
};

// 기본 생성자 테스트
TEST_F(PlayerTest, Constructor_CreatesPlayerWithCorrectInitialState) {
    EXPECT_EQ(test_player_->GetId(), 12345);
    EXPECT_EQ(test_player_->GetName(), "TestHero");
    EXPECT_EQ(test_player_->GetLevel(), 10);
    EXPECT_EQ(test_player_->GetHealth(), 1000);  // level * 100
    EXPECT_TRUE(test_player_->IsAlive());
}

// 데미지 시스템 테스트
TEST_F(PlayerTest, TakeDamage_WithValidDamage_DecreasesHealth) {
    int initial_health = test_player_->GetHealth();
    int damage = 300;
    
    test_player_->TakeDamage(damage);
    
    EXPECT_EQ(test_player_->GetHealth(), initial_health - damage);
    EXPECT_TRUE(test_player_->IsAlive());
}

TEST_F(PlayerTest, TakeDamage_WithLethalDamage_KillsPlayer) {
    test_player_->TakeDamage(1500);  // 체력보다 큰 데미지
    
    EXPECT_EQ(test_player_->GetHealth(), 0);
    EXPECT_FALSE(test_player_->IsAlive());
}

TEST_F(PlayerTest, TakeDamage_WithNegativeDamage_DoesNotChangeHealth) {
    int initial_health = test_player_->GetHealth();
    
    test_player_->TakeDamage(-100);  // 음수 데미지
    
    EXPECT_EQ(test_player_->GetHealth(), initial_health);
}

// 힐링 시스템 테스트
TEST_F(PlayerTest, Heal_WithDamagedPlayer_RestoresHealth) {
    test_player_->TakeDamage(400);  // 600 체력 남음
    
    test_player_->Heal(200);
    
    EXPECT_EQ(test_player_->GetHealth(), 800);
}

TEST_F(PlayerTest, Heal_AboveMaxHealth_CapsAtMaxHealth) {
    test_player_->TakeDamage(100);  // 900 체력 남음
    
    test_player_->Heal(500);  // 최대 체력을 초과하는 힐
    
    EXPECT_EQ(test_player_->GetHealth(), 1000);  // 최대 체력으로 제한됨
}

// 레벨업 시스템 테스트
TEST_F(PlayerTest, LevelUp_IncreasesLevelAndMaxHealth) {
    int initial_health = test_player_->GetHealth();
    
    test_player_->LevelUp();
    
    EXPECT_EQ(test_player_->GetLevel(), 11);
    EXPECT_EQ(test_player_->GetHealth(), initial_health + 100);  // 체력 증가
}

// 위치 시스템 테스트
TEST_F(PlayerTest, SetPosition_UpdatesPlayerPosition) {
    float x = 123.45f, y = 678.90f;
    
    test_player_->SetPosition(x, y);
    
    EXPECT_FLOAT_EQ(test_player_->GetX(), x);
    EXPECT_FLOAT_EQ(test_player_->GetY(), y);
}
```

### **1.2 매개변수화 테스트와 테스트 픽스처**

```cpp
// 🎯 Google Test 고급 기법들

// 매개변수화 테스트 - 다양한 입력값으로 같은 로직 테스트
class DamageCalculatorTest : public ::testing::TestWithParam<std::tuple<int, int, int>> {
protected:
    int CalculateDamage(int attack, int defense) {
        return std::max(1, attack - defense);  // 최소 1 데미지
    }
};

TEST_P(DamageCalculatorTest, CalculateDamage_WithVariousInputs_ProducesExpectedResults) {
    auto [attack, defense, expected_damage] = GetParam();
    
    int actual_damage = CalculateDamage(attack, defense);
    
    EXPECT_EQ(actual_damage, expected_damage);
}

INSTANTIATE_TEST_SUITE_P(
    DamageCalculations,
    DamageCalculatorTest,
    ::testing::Values(
        std::make_tuple(100, 50, 50),   // 일반적인 경우
        std::make_tuple(100, 100, 1),   // 방어력이 공격력과 같을 때
        std::make_tuple(50, 100, 1),    // 방어력이 공격력보다 높을 때
        std::make_tuple(200, 0, 200),   // 방어력이 0일 때
        std::make_tuple(0, 50, 1)       // 공격력이 0일 때 (최소 1 데미지)
    )
);

// 타입화된 테스트 - 여러 타입에 대해 같은 테스트
template<typename T>
class NumericStatsTest : public ::testing::Test {
protected:
    T GetMaxValue() {
        if constexpr (std::is_same_v<T, int>) {
            return 2147483647;
        } else if constexpr (std::is_same_v<T, float>) {
            return 999999.9f;
        } else if constexpr (std::is_same_v<T, double>) {
            return 999999999.9;
        }
    }
};

using NumericTypes = ::testing::Types<int, float, double>;
TYPED_TEST_SUITE(NumericStatsTest, NumericTypes);

TYPED_TEST(NumericStatsTest, MaxValue_DoesNotOverflow) {
    TypeParam max_val = this->GetMaxValue();
    TypeParam result = max_val + 1;  // 오버플로우 테스트
    
    // 타입별로 적절한 검증
    if constexpr (std::is_integral_v<TypeParam>) {
        EXPECT_LT(result, max_val);  // 정수는 오버플로우로 작아짐
    } else {
        EXPECT_GE(result, max_val);  // 실수는 inf가 될 수 있음
    }
}

// 데스 테스트 - 크래시나 assertion을 테스트
TEST(PlayerDeathTest, AccessDeletedPlayer_CausesAssertion) {
    EXPECT_DEATH({
        Player* player = new Player(1, "Test", 10);
        delete player;
        player->GetHealth();  // 삭제된 객체 접근
    }, ".*");  // 아무 메시지나 매칭
}

// 예외 테스트
class DatabaseException : public std::exception {
public:
    explicit DatabaseException(const std::string& message) : message_(message) {}
    const char* what() const noexcept override { return message_.c_str(); }
private:
    std::string message_;
};

TEST(DatabaseTest, ConnectionFailure_ThrowsException) {
    auto simulate_connection_failure = []() {
        throw DatabaseException("Connection refused");
    };
    
    EXPECT_THROW(simulate_connection_failure(), DatabaseException);
    EXPECT_THROW(simulate_connection_failure(), std::exception);  // 기본 클래스도 매칭
}

TEST(DatabaseTest, ValidConnection_DoesNotThrow) {
    auto simulate_valid_connection = []() {
        return true;  // 정상 처리
    };
    
    EXPECT_NO_THROW(simulate_valid_connection());
}
```

---

## 📚 2. Google Mock으로 의존성 테스트하기

### **2.1 Mock 객체 생성과 활용**

```cpp
// 🎭 Google Mock으로 복잡한 의존성 테스트

#include <gmock/gmock.h>

// 인터페이스 정의 (테스트를 위한 추상화)
class DatabaseInterface {
public:
    virtual ~DatabaseInterface() = default;
    virtual bool SavePlayer(const Player& player) = 0;
    virtual std::optional<Player> LoadPlayer(uint64_t player_id) = 0;
    virtual bool DeletePlayer(uint64_t player_id) = 0;
    virtual std::vector<Player> GetPlayersByLevel(int min_level, int max_level) = 0;
};

class NetworkInterface {
public:
    virtual ~NetworkInterface() = default;
    virtual bool SendPacket(uint64_t player_id, const std::string& packet) = 0;
    virtual void BroadcastPacket(const std::string& packet) = 0;
    virtual bool IsPlayerConnected(uint64_t player_id) = 0;
};

// Mock 클래스들
class MockDatabase : public DatabaseInterface {
public:
    MOCK_METHOD(bool, SavePlayer, (const Player& player), (override));
    MOCK_METHOD(std::optional<Player>, LoadPlayer, (uint64_t player_id), (override));
    MOCK_METHOD(bool, DeletePlayer, (uint64_t player_id), (override));
    MOCK_METHOD(std::vector<Player>, GetPlayersByLevel, (int min_level, int max_level), (override));
};

class MockNetwork : public NetworkInterface {
public:
    MOCK_METHOD(bool, SendPacket, (uint64_t player_id, const std::string& packet), (override));
    MOCK_METHOD(void, BroadcastPacket, (const std::string& packet), (override));
    MOCK_METHOD(bool, IsPlayerConnected, (uint64_t player_id), (override));
};

// Mock을 사용하는 게임 서비스 클래스
class PlayerService {
private:
    std::unique_ptr<DatabaseInterface> database_;
    std::unique_ptr<NetworkInterface> network_;
    
public:
    PlayerService(std::unique_ptr<DatabaseInterface> db,
                 std::unique_ptr<NetworkInterface> net)
        : database_(std::move(db)), network_(std::move(net)) {}
    
    bool LoginPlayer(uint64_t player_id) {
        auto player_opt = database_->LoadPlayer(player_id);
        if (!player_opt) {
            return false;
        }
        
        Player& player = *player_opt;
        std::string welcome_msg = "Welcome back, " + player.GetName() + "!";
        
        return network_->SendPacket(player_id, welcome_msg);
    }
    
    bool ProcessLevelUp(uint64_t player_id) {
        auto player_opt = database_->LoadPlayer(player_id);
        if (!player_opt) {
            return false;
        }
        
        Player player = *player_opt;
        int old_level = player.GetLevel();
        player.LevelUp();
        
        // 데이터베이스 저장
        if (!database_->SavePlayer(player)) {
            return false;
        }
        
        // 레벨업 알림 전송
        std::string level_up_msg = player.GetName() + " reached level " + 
                                  std::to_string(player.GetLevel()) + "!";
        network_->BroadcastPacket(level_up_msg);
        
        return true;
    }
    
    int GetHighLevelPlayerCount(int min_level) {
        auto players = database_->GetPlayersByLevel(min_level, 100);
        int connected_count = 0;
        
        for (const auto& player : players) {
            if (network_->IsPlayerConnected(player.GetId())) {
                connected_count++;
            }
        }
        
        return connected_count;
    }
};

// Mock을 활용한 테스트들
class PlayerServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        auto mock_db = std::make_unique<MockDatabase>();
        auto mock_net = std::make_unique<MockNetwork>();
        
        // Mock 포인터 저장 (테스트에서 사용하기 위해)
        mock_database_ = mock_db.get();
        mock_network_ = mock_net.get();
        
        // 서비스에 Mock 주입
        player_service_ = std::make_unique<PlayerService>(
            std::move(mock_db), std::move(mock_net)
        );
        
        // 테스트용 플레이어 데이터
        test_player_ = Player(12345, "TestPlayer", 10);
    }
    
    MockDatabase* mock_database_;
    MockNetwork* mock_network_;
    std::unique_ptr<PlayerService> player_service_;
    Player test_player_;
};

TEST_F(PlayerServiceTest, LoginPlayer_WithExistingPlayer_SendsWelcomeMessage) {
    uint64_t player_id = test_player_.GetId();
    
    // Mock 설정: 데이터베이스에서 플레이어를 찾을 수 있음
    EXPECT_CALL(*mock_database_, LoadPlayer(player_id))
        .WillOnce(::testing::Return(test_player_));
    
    // Mock 설정: 네트워크 전송이 성공함
    EXPECT_CALL(*mock_network_, SendPacket(player_id, ::testing::HasSubstr("Welcome back")))
        .WillOnce(::testing::Return(true));
    
    // 테스트 실행
    bool result = player_service_->LoginPlayer(player_id);
    
    EXPECT_TRUE(result);
}

TEST_F(PlayerServiceTest, LoginPlayer_WithNonExistentPlayer_ReturnsFalse) {
    uint64_t non_existent_id = 99999;
    
    // Mock 설정: 데이터베이스에서 플레이어를 찾을 수 없음
    EXPECT_CALL(*mock_database_, LoadPlayer(non_existent_id))
        .WillOnce(::testing::Return(std::nullopt));
    
    // 네트워크 호출이 일어나지 않아야 함
    EXPECT_CALL(*mock_network_, SendPacket(::testing::_, ::testing::_))
        .Times(0);
    
    bool result = player_service_->LoginPlayer(non_existent_id);
    
    EXPECT_FALSE(result);
}

TEST_F(PlayerServiceTest, ProcessLevelUp_WithValidPlayer_SavesAndBroadcasts) {
    uint64_t player_id = test_player_.GetId();
    
    // Mock 설정: 순차적 호출 검증
    {
        ::testing::InSequence seq;  // 순서 보장
        
        // 1. 플레이어 로드
        EXPECT_CALL(*mock_database_, LoadPlayer(player_id))
            .WillOnce(::testing::Return(test_player_));
        
        // 2. 레벨업된 플레이어 저장
        EXPECT_CALL(*mock_database_, SavePlayer(::testing::_))
            .WillOnce(::testing::Return(true));
        
        // 3. 브로드캐스트
        EXPECT_CALL(*mock_network_, BroadcastPacket(::testing::HasSubstr("reached level")));
    }
    
    bool result = player_service_->ProcessLevelUp(player_id);
    
    EXPECT_TRUE(result);
}

TEST_F(PlayerServiceTest, GetHighLevelPlayerCount_CountsOnlyConnectedPlayers) {
    int min_level = 20;
    
    // 테스트 데이터 준비
    std::vector<Player> high_level_players = {
        Player(1, "Player1", 25),
        Player(2, "Player2", 30),
        Player(3, "Player3", 35)
    };
    
    // Mock 설정
    EXPECT_CALL(*mock_database_, GetPlayersByLevel(min_level, 100))
        .WillOnce(::testing::Return(high_level_players));
    
    // 연결 상태 Mock (첫 번째와 세 번째만 연결됨)
    EXPECT_CALL(*mock_network_, IsPlayerConnected(1))
        .WillOnce(::testing::Return(true));
    EXPECT_CALL(*mock_network_, IsPlayerConnected(2))
        .WillOnce(::testing::Return(false));
    EXPECT_CALL(*mock_network_, IsPlayerConnected(3))
        .WillOnce(::testing::Return(true));
    
    int count = player_service_->GetHighLevelPlayerCount(min_level);
    
    EXPECT_EQ(count, 2);  // 연결된 플레이어만 카운트
}
```

### **2.2 고급 Mock 기법들**

```cpp
// 🚀 Google Mock 고급 패턴들

// 복잡한 매개변수 매칭
class AdvancedMockTest : public ::testing::Test {
protected:
    MockDatabase mock_db_;
};

// 커스텀 Matcher 작성
MATCHER_P(HasLevel, expected_level, "Player has level " + std::to_string(expected_level)) {
    return arg.GetLevel() == expected_level;
}

MATCHER_P2(HasLevelBetween, min_level, max_level, 
          "Player level is between " + std::to_string(min_level) + 
          " and " + std::to_string(max_level)) {
    int level = arg.GetLevel();
    return level >= min_level && level <= max_level;
}

TEST_F(AdvancedMockTest, CustomMatchers_WorkCorrectly) {
    Player high_level_player(1, "HighLevel", 50);
    
    EXPECT_CALL(mock_db_, SavePlayer(HasLevel(50)))
        .WillOnce(::testing::Return(true));
    
    EXPECT_CALL(mock_db_, SavePlayer(HasLevelBetween(45, 55)))
        .WillOnce(::testing::Return(true));
    
    mock_db_.SavePlayer(high_level_player);
    mock_db_.SavePlayer(high_level_player);
}

// Action을 통한 복잡한 동작 정의
ACTION_P(SetPlayerLevel, new_level) {
    // arg0는 첫 번째 매개변수 (Player&)
    const_cast<Player&>(arg0).LevelUp();  // 실제로는 더 복잡한 로직
}

ACTION_P2(SimulateDbDelay, min_ms, max_ms) {
    int delay = min_ms + (rand() % (max_ms - min_ms));
    std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    return true;
}

TEST_F(AdvancedMockTest, Actions_SimulateComplexBehavior) {
    Player test_player(1, "Test", 10);
    
    EXPECT_CALL(mock_db_, SavePlayer(::testing::_))
        .WillOnce(::testing::DoAll(
            SetPlayerLevel(11),           // 플레이어 레벨 수정
            SimulateDbDelay(10, 50),      // 데이터베이스 지연 시뮬레이션
            ::testing::Return(true)       // 최종 반환값
        ));
    
    bool result = mock_db_.SavePlayer(test_player);
    EXPECT_TRUE(result);
}

// Mock 호출 횟수와 순서 검증
TEST_F(AdvancedMockTest, CallOrder_AndFrequency_AreVerified) {
    {
        ::testing::InSequence seq;
        
        EXPECT_CALL(mock_db_, LoadPlayer(::testing::_))
            .Times(::testing::AtLeast(1));
        
        EXPECT_CALL(mock_db_, SavePlayer(::testing::_))
            .Times(::testing::Exactly(2));
        
        EXPECT_CALL(mock_db_, DeletePlayer(::testing::_))
            .Times(::testing::AtMost(1));
    }
    
    // 실제 호출들
    mock_db_.LoadPlayer(1);
    mock_db_.LoadPlayer(2);
    mock_db_.SavePlayer(Player(1, "Test1", 10));
    mock_db_.SavePlayer(Player(2, "Test2", 20));
    mock_db_.DeletePlayer(1);
}

// 상태 기반 Mock (Stateful Mock)
class StatefulMockDatabase : public DatabaseInterface {
private:
    std::map<uint64_t, Player> stored_players_;
    
public:
    MOCK_METHOD(bool, SavePlayer, (const Player& player), (override));
    MOCK_METHOD(std::optional<Player>, LoadPlayer, (uint64_t player_id), (override));
    MOCK_METHOD(bool, DeletePlayer, (uint64_t player_id), (override));
    MOCK_METHOD(std::vector<Player>, GetPlayersByLevel, (int min_level, int max_level), (override));
    
    void SetupRealisticBehavior() {
        // SavePlayer가 실제로 데이터를 저장하도록 설정
        ON_CALL(*this, SavePlayer(::testing::_))
            .WillByDefault([this](const Player& player) {
                stored_players_[player.GetId()] = player;
                return true;
            });
        
        // LoadPlayer가 저장된 데이터를 반환하도록 설정
        ON_CALL(*this, LoadPlayer(::testing::_))
            .WillByDefault([this](uint64_t player_id) -> std::optional<Player> {
                auto it = stored_players_.find(player_id);
                if (it != stored_players_.end()) {
                    return it->second;
                }
                return std::nullopt;
            });
        
        // DeletePlayer가 실제로 데이터를 삭제하도록 설정
        ON_CALL(*this, DeletePlayer(::testing::_))
            .WillByDefault([this](uint64_t player_id) {
                return stored_players_.erase(player_id) > 0;
            });
    }
};

TEST_F(AdvancedMockTest, StatefulMock_BehavesLikeRealDatabase) {
    StatefulMockDatabase stateful_db;
    stateful_db.SetupRealisticBehavior();
    
    Player test_player(123, "StatefulTest", 25);
    
    // 저장 후 로드가 동작하는지 확인
    EXPECT_TRUE(stateful_db.SavePlayer(test_player));
    
    auto loaded = stateful_db.LoadPlayer(123);
    ASSERT_TRUE(loaded.has_value());
    EXPECT_EQ(loaded->GetName(), "StatefulTest");
    EXPECT_EQ(loaded->GetLevel(), 25);
    
    // 삭제 후 로드 실패 확인
    EXPECT_TRUE(stateful_db.DeletePlayer(123));
    EXPECT_FALSE(stateful_db.LoadPlayer(123).has_value());
}
```

---

## 📚 3. Catch2로 더 표현력 있는 테스트 작성하기

### **3.1 Catch2의 자연스러운 문법**

```cpp
// 🎯 Catch2로 읽기 쉬운 테스트 작성

#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

// 테스트 대상: 게임 인벤토리 시스템
class Inventory {
private:
    std::map<std::string, int> items_;
    int max_slots_;
    
public:
    explicit Inventory(int max_slots = 50) : max_slots_(max_slots) {}
    
    bool AddItem(const std::string& item, int count = 1) {
        if (items_.size() >= max_slots_ && items_.find(item) == items_.end()) {
            return false;  // 슬롯 부족
        }
        items_[item] += count;
        return true;
    }
    
    bool RemoveItem(const std::string& item, int count = 1) {
        auto it = items_.find(item);
        if (it == items_.end() || it->second < count) {
            return false;
        }
        
        it->second -= count;
        if (it->second == 0) {
            items_.erase(it);
        }
        return true;
    }
    
    int GetItemCount(const std::string& item) const {
        auto it = items_.find(item);
        return (it != items_.end()) ? it->second : 0;
    }
    
    bool HasItem(const std::string& item) const {
        return items_.find(item) != items_.end();
    }
    
    size_t GetUsedSlots() const { return items_.size(); }
    size_t GetMaxSlots() const { return max_slots_; }
    bool IsFull() const { return items_.size() >= max_slots_; }
    
    std::vector<std::string> GetAllItems() const {
        std::vector<std::string> result;
        for (const auto& [item, count] : items_) {
            result.push_back(item);
        }
        return result;
    }
};

// Catch2 테스트들 - 자연스러운 언어로 작성
TEST_CASE("Inventory can store and retrieve items", "[inventory]") {
    Inventory inv;
    
    SECTION("Adding items increases count") {
        REQUIRE(inv.AddItem("Sword", 1));
        REQUIRE(inv.GetItemCount("Sword") == 1);
        
        REQUIRE(inv.AddItem("Sword", 2));
        REQUIRE(inv.GetItemCount("Sword") == 3);
    }
    
    SECTION("Removing items decreases count") {
        inv.AddItem("Shield", 5);
        
        REQUIRE(inv.RemoveItem("Shield", 2));
        REQUIRE(inv.GetItemCount("Shield") == 3);
        
        REQUIRE(inv.RemoveItem("Shield", 3));
        REQUIRE(inv.GetItemCount("Shield") == 0);
        REQUIRE_FALSE(inv.HasItem("Shield"));
    }
    
    SECTION("Cannot remove more items than available") {
        inv.AddItem("Potion", 3);
        
        REQUIRE_FALSE(inv.RemoveItem("Potion", 5));
        REQUIRE(inv.GetItemCount("Potion") == 3);  // 변경되지 않음
        
        REQUIRE_FALSE(inv.RemoveItem("NonExistent", 1));
    }
}

TEST_CASE("Inventory respects slot limitations", "[inventory][slots]") {
    SECTION("Full inventory rejects new item types") {
        Inventory small_inv(2);  // 최대 2슬롯
        
        REQUIRE(small_inv.AddItem("Item1"));
        REQUIRE(small_inv.AddItem("Item2"));
        REQUIRE(small_inv.IsFull());
        
        // 새로운 아이템 타입 추가 실패
        REQUIRE_FALSE(small_inv.AddItem("Item3"));
        
        // 기존 아이템 추가는 성공
        REQUIRE(small_inv.AddItem("Item1", 5));
        REQUIRE(small_inv.GetItemCount("Item1") == 6);
    }
}

// 데이터 생성기를 사용한 속성 기반 테스트
#include <catch2/generators/catch_generators.hpp>

TEST_CASE("Inventory operations work with generated data", "[inventory][generated]") {
    auto item_name = GENERATE(as<std::string>{}, "Sword", "Shield", "Potion", "Key", "Gem");
    auto count = GENERATE(1, 5, 10, 100);
    
    Inventory inv;
    
    CAPTURE(item_name, count);  // 테스트 실패 시 값들을 출력
    
    REQUIRE(inv.AddItem(item_name, count));
    REQUIRE(inv.GetItemCount(item_name) == count);
    REQUIRE(inv.HasItem(item_name));
}

// 예외 테스트
class InventoryException : public std::exception {
public:
    explicit InventoryException(const std::string& msg) : message_(msg) {}
    const char* what() const noexcept override { return message_.c_str(); }
private:
    std::string message_;
};

class SecureInventory : public Inventory {
public:
    void AddItemSecure(const std::string& item, int count) {
        if (count <= 0) {
            throw InventoryException("Count must be positive");
        }
        if (item.empty()) {
            throw InventoryException("Item name cannot be empty");
        }
        if (!AddItem(item, count)) {
            throw InventoryException("Cannot add item: inventory full");
        }
    }
};

TEST_CASE("SecureInventory throws appropriate exceptions", "[inventory][exceptions]") {
    SecureInventory secure_inv;
    
    SECTION("Negative count throws exception") {
        REQUIRE_THROWS_AS(secure_inv.AddItemSecure("Sword", -1), InventoryException);
        REQUIRE_THROWS_WITH(secure_inv.AddItemSecure("Sword", 0), 
                           Catch::Matchers::ContainsSubstring("must be positive"));
    }
    
    SECTION("Empty item name throws exception") {
        REQUIRE_THROWS_AS(secure_inv.AddItemSecure("", 1), InventoryException);
        REQUIRE_THROWS_WITH(secure_inv.AddItemSecure("", 1),
                           Catch::Matchers::ContainsSubstring("cannot be empty"));
    }
}

// 벤치마크 테스트 (Catch2 v3)
TEST_CASE("Inventory performance benchmarks", "[inventory][benchmark]") {
    Inventory inv(10000);  // 큰 인벤토리
    
    BENCHMARK("Adding 1000 different items") {
        for (int i = 0; i < 1000; ++i) {
            inv.AddItem("Item" + std::to_string(i));
        }
    };
    
    // 아이템을 미리 추가해둠
    for (int i = 0; i < 1000; ++i) {
        inv.AddItem("BenchItem" + std::to_string(i), 10);
    }
    
    BENCHMARK("Searching for existing items") {
        for (int i = 0; i < 1000; ++i) {
            volatile bool found = inv.HasItem("BenchItem" + std::to_string(i));
            (void)found;  // unused variable warning 방지
        }
    };
}
```

### **3.2 BDD 스타일과 고급 Catch2 기능**

```cpp
// 🎭 BDD (Behavior Driven Development) 스타일 테스트

// 복잡한 게임 매치메이킹 시스템 테스트
class MatchmakingSystem {
private:
    struct Player {
        uint64_t id;
        std::string name;
        int skill_rating;
        std::chrono::steady_clock::time_point queue_time;
    };
    
    std::vector<Player> waiting_players_;
    std::vector<std::vector<uint64_t>> active_matches_;
    
public:
    bool AddPlayerToQueue(uint64_t id, const std::string& name, int skill_rating) {
        // 중복 체크
        if (IsPlayerInQueue(id)) return false;
        
        waiting_players_.push_back({
            id, name, skill_rating, std::chrono::steady_clock::now()
        });
        return true;
    }
    
    bool IsPlayerInQueue(uint64_t id) const {
        return std::find_if(waiting_players_.begin(), waiting_players_.end(),
                          [id](const Player& p) { return p.id == id; }) 
               != waiting_players_.end();
    }
    
    std::optional<std::vector<uint64_t>> TryCreateMatch(int team_size = 2) {
        if (waiting_players_.size() < team_size) return std::nullopt;
        
        // 간단한 매칭: 스킬 레이팅이 비슷한 플레이어들
        std::sort(waiting_players_.begin(), waiting_players_.end(),
                 [](const Player& a, const Player& b) {
                     return a.skill_rating < b.skill_rating;
                 });
        
        std::vector<uint64_t> match;
        for (int i = 0; i < team_size && i < waiting_players_.size(); ++i) {
            match.push_back(waiting_players_[i].id);
        }
        
        // 매치에 참여한 플레이어들을 큐에서 제거
        waiting_players_.erase(waiting_players_.begin(), 
                             waiting_players_.begin() + team_size);
        
        active_matches_.push_back(match);
        return match;
    }
    
    size_t GetQueueSize() const { return waiting_players_.size(); }
    size_t GetActiveMatchCount() const { return active_matches_.size(); }
};

// BDD 스타일 테스트
SCENARIO("Players can join matchmaking queue and get matched", "[matchmaking]") {
    GIVEN("An empty matchmaking system") {
        MatchmakingSystem mm;
        
        WHEN("A player joins the queue") {
            bool joined = mm.AddPlayerToQueue(1, "Player1", 1500);
            
            THEN("The player is successfully added") {
                REQUIRE(joined);
                REQUIRE(mm.IsPlayerInQueue(1));
                REQUIRE(mm.GetQueueSize() == 1);
            }
            
            AND_WHEN("The same player tries to join again") {
                bool joined_again = mm.AddPlayerToQueue(1, "Player1", 1500);
                
                THEN("The second join attempt fails") {
                    REQUIRE_FALSE(joined_again);
                    REQUIRE(mm.GetQueueSize() == 1);
                }
            }
        }
        
        WHEN("Multiple players with similar skill join") {
            mm.AddPlayerToQueue(1, "Player1", 1500);
            mm.AddPlayerToQueue(2, "Player2", 1520);
            mm.AddPlayerToQueue(3, "Player3", 1480);
            
            THEN("A match can be created") {
                auto match = mm.TryCreateMatch(2);
                
                REQUIRE(match.has_value());
                REQUIRE(match->size() == 2);
                REQUIRE(mm.GetQueueSize() == 1);  // 한 명 남음
                REQUIRE(mm.GetActiveMatchCount() == 1);
            }
        }
        
        WHEN("Not enough players are in queue") {
            mm.AddPlayerToQueue(1, "LonePlayer", 1500);
            
            THEN("No match can be created") {
                auto match = mm.TryCreateMatch(2);
                
                REQUIRE_FALSE(match.has_value());
                REQUIRE(mm.GetQueueSize() == 1);
                REQUIRE(mm.GetActiveMatchCount() == 0);
            }
        }
    }
}

// 커스텀 Matcher 작성
class IsInRange : public Catch::Matchers::MatcherGenericBase {
    int low_, high_;
public:
    IsInRange(int low, int high) : low_(low), high_(high) {}
    
    bool match(int value) const {
        return value >= low_ && value <= high_;
    }
    
    std::string describe() const override {
        return "is between " + std::to_string(low_) + " and " + std::to_string(high_);
    }
};

IsInRange InRange(int low, int high) {
    return IsInRange(low, high);
}

TEST_CASE("Custom matchers work correctly", "[matchers]") {
    int player_level = 25;
    
    REQUIRE_THAT(player_level, InRange(20, 30));
    REQUIRE_THAT(player_level, !InRange(1, 10));
}

// 타임아웃 테스트
#include <thread>

class SlowOperation {
public:
    static bool ProcessWithTimeout(int delay_ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        return true;
    }
};

TEST_CASE("Operations complete within expected time", "[performance][timeout]") {
    using namespace std::chrono_literals;
    
    SECTION("Fast operation completes quickly") {
        auto start = std::chrono::steady_clock::now();
        bool result = SlowOperation::ProcessWithTimeout(10);
        auto end = std::chrono::steady_clock::now();
        
        REQUIRE(result);
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        REQUIRE(duration < 100ms);
    }
    
    SECTION("Slow operation is detected") {
        auto start = std::chrono::steady_clock::now();
        SlowOperation::ProcessWithTimeout(200);
        auto end = std::chrono::steady_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        REQUIRE(duration >= 150ms);  // 적어도 예상 시간은 걸려야 함
    }
}
```

---

## 📚 4. 성능 테스팅과 벤치마킹

### **4.1 Google Benchmark로 성능 측정**

```cpp
// ⚡ Google Benchmark로 게임 서버 성능 측정

#include <benchmark/benchmark.h>
#include <vector>
#include <algorithm>
#include <random>
#include <unordered_map>
#include <unordered_set>

// 성능 테스트용 데이터 구조
struct GameEntity {
    uint64_t id;
    float x, y, z;
    int health;
    int level;
    std::string name;
    
    GameEntity(uint64_t id, float x, float y, float z, int health, int level, const std::string& name)
        : id(id), x(x), y(y), z(z), health(health), level(level), name(name) {}
};

// 벤치마크 1: 컨테이너 성능 비교
static void BM_VectorSearch(benchmark::State& state) {
    // 테스트 데이터 생성
    std::vector<GameEntity> entities;
    entities.reserve(state.range(0));
    
    for (int i = 0; i < state.range(0); ++i) {
        entities.emplace_back(i, i*1.0f, i*2.0f, i*3.0f, 100, i%50, "Entity" + std::to_string(i));
    }
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> dist(0, state.range(0) - 1);
    
    for (auto _ : state) {
        uint64_t search_id = dist(gen);
        
        // 선형 검색
        auto it = std::find_if(entities.begin(), entities.end(),
            [search_id](const GameEntity& e) { return e.id == search_id; });
        
        benchmark::DoNotOptimize(it);
    }
    
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

static void BM_UnorderedMapSearch(benchmark::State& state) {
    // 해시맵으로 데이터 저장
    std::unordered_map<uint64_t, GameEntity> entity_map;
    entity_map.reserve(state.range(0));
    
    for (int i = 0; i < state.range(0); ++i) {
        entity_map.emplace(i, GameEntity(i, i*1.0f, i*2.0f, i*3.0f, 100, i%50, "Entity" + std::to_string(i)));
    }
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> dist(0, state.range(0) - 1);
    
    for (auto _ : state) {
        uint64_t search_id = dist(gen);
        
        auto it = entity_map.find(search_id);
        benchmark::DoNotOptimize(it);
    }
    
    state.SetItemsProcessed(state.iterations());
}

// 벤치마크 등록 - 다양한 데이터 크기로 테스트
BENCHMARK(BM_VectorSearch)->Range(1000, 100000)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_UnorderedMapSearch)->Range(1000, 100000)->Unit(benchmark::kMicrosecond);

// 벤치마크 2: 메모리 할당 패턴 비교
static void BM_RepeatedAllocation(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<GameEntity> entities;
        
        for (int i = 0; i < state.range(0); ++i) {
            entities.emplace_back(i, 0, 0, 0, 100, 1, "Test");
        }
        
        benchmark::DoNotOptimize(entities);
    }
}

static void BM_PreallocatedVector(benchmark::State& state) {
    for (auto _ : state) {
        std::vector<GameEntity> entities;
        entities.reserve(state.range(0));  // 미리 할당
        
        for (int i = 0; i < state.range(0); ++i) {
            entities.emplace_back(i, 0, 0, 0, 100, 1, "Test");
        }
        
        benchmark::DoNotOptimize(entities);
    }
}

BENCHMARK(BM_RepeatedAllocation)->Range(100, 10000);
BENCHMARK(BM_PreallocatedVector)->Range(100, 10000);

// 벤치마크 3: 알고리즘 성능 비교
class SpatialIndex {
public:
    virtual ~SpatialIndex() = default;
    virtual void Insert(const GameEntity& entity) = 0;
    virtual std::vector<GameEntity> FindInRadius(float x, float y, float radius) = 0;
    virtual void Clear() = 0;
};

// 단순 선형 검색 구현
class LinearSpatialIndex : public SpatialIndex {
private:
    std::vector<GameEntity> entities_;
    
public:
    void Insert(const GameEntity& entity) override {
        entities_.push_back(entity);
    }
    
    std::vector<GameEntity> FindInRadius(float x, float y, float radius) override {
        std::vector<GameEntity> result;
        float radius_sq = radius * radius;
        
        for (const auto& entity : entities_) {
            float dx = entity.x - x;
            float dy = entity.y - y;
            if (dx*dx + dy*dy <= radius_sq) {
                result.push_back(entity);
            }
        }
        return result;
    }
    
    void Clear() override { entities_.clear(); }
};

// 그리드 기반 공간 인덱스
class GridSpatialIndex : public SpatialIndex {
private:
    static constexpr float GRID_SIZE = 100.0f;
    std::unordered_map<std::pair<int, int>, std::vector<GameEntity>, 
                      boost::hash<std::pair<int, int>>> grid_;
    
    std::pair<int, int> GetGridCell(float x, float y) {
        return {static_cast<int>(x / GRID_SIZE), static_cast<int>(y / GRID_SIZE)};
    }
    
public:
    void Insert(const GameEntity& entity) override {
        auto cell = GetGridCell(entity.x, entity.y);
        grid_[cell].push_back(entity);
    }
    
    std::vector<GameEntity> FindInRadius(float x, float y, float radius) override {
        std::vector<GameEntity> result;
        float radius_sq = radius * radius;
        
        // 검색할 그리드 셀 범위 계산
        int min_x = static_cast<int>((x - radius) / GRID_SIZE);
        int max_x = static_cast<int>((x + radius) / GRID_SIZE);
        int min_y = static_cast<int>((y - radius) / GRID_SIZE);
        int max_y = static_cast<int>((y + radius) / GRID_SIZE);
        
        for (int gx = min_x; gx <= max_x; ++gx) {
            for (int gy = min_y; gy <= max_y; ++gy) {
                auto it = grid_.find({gx, gy});
                if (it != grid_.end()) {
                    for (const auto& entity : it->second) {
                        float dx = entity.x - x;
                        float dy = entity.y - y;
                        if (dx*dx + dy*dy <= radius_sq) {
                            result.push_back(entity);
                        }
                    }
                }
            }
        }
        return result;
    }
    
    void Clear() override { grid_.clear(); }
};

// 공간 인덱스 성능 비교 벤치마크
template<typename IndexType>
static void BM_SpatialIndexSearch(benchmark::State& state) {
    IndexType index;
    
    // 테스트 데이터 생성 (균등 분포)
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> pos_dist(0, 1000);
    
    for (int i = 0; i < state.range(0); ++i) {
        float x = pos_dist(gen);
        float y = pos_dist(gen);
        index.Insert(GameEntity(i, x, y, 0, 100, 1, "Entity"));
    }
    
    // 검색 성능 측정
    std::uniform_real_distribution<float> search_dist(100, 900);  // 가장자리 제외
    
    for (auto _ : state) {
        float search_x = search_dist(gen);
        float search_y = search_dist(gen);
        
        auto results = index.FindInRadius(search_x, search_y, 50.0f);
        benchmark::DoNotOptimize(results);
    }
    
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK_TEMPLATE(BM_SpatialIndexSearch, LinearSpatialIndex)
    ->Range(1000, 50000)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(BM_SpatialIndexSearch, GridSpatialIndex)
    ->Range(1000, 50000)->Unit(benchmark::kMicrosecond);

// 벤치마크 4: 문자열 처리 성능
static void BM_StringConcatenation_Plus(benchmark::State& state) {
    std::vector<std::string> words;
    for (int i = 0; i < state.range(0); ++i) {
        words.push_back("Word" + std::to_string(i));
    }
    
    for (auto _ : state) {
        std::string result;
        for (const auto& word : words) {
            result += word + " ";
        }
        benchmark::DoNotOptimize(result);
    }
}

static void BM_StringConcatenation_Stream(benchmark::State& state) {
    std::vector<std::string> words;
    for (int i = 0; i < state.range(0); ++i) {
        words.push_back("Word" + std::to_string(i));
    }
    
    for (auto _ : state) {
        std::ostringstream oss;
        for (const auto& word : words) {
            oss << word << " ";
        }
        std::string result = oss.str();
        benchmark::DoNotOptimize(result);
    }
}

static void BM_StringConcatenation_Reserve(benchmark::State& state) {
    std::vector<std::string> words;
    size_t total_length = 0;
    for (int i = 0; i < state.range(0); ++i) {
        words.push_back("Word" + std::to_string(i));
        total_length += words.back().length() + 1;  // +1 for space
    }
    
    for (auto _ : state) {
        std::string result;
        result.reserve(total_length);
        for (const auto& word : words) {
            result += word + " ";
        }
        benchmark::DoNotOptimize(result);
    }
}

BENCHMARK(BM_StringConcatenation_Plus)->Range(100, 10000);
BENCHMARK(BM_StringConcatenation_Stream)->Range(100, 10000);
BENCHMARK(BM_StringConcatenation_Reserve)->Range(100, 10000);

// 커스텀 벤치마크 픽스처
class NetworkPacketBenchmark : public benchmark::Fixture {
public:
    void SetUp(const ::benchmark::State& state) override {
        // 테스트 패킷 데이터 생성
        packet_data_.reserve(state.range(0));
        for (int i = 0; i < state.range(0); ++i) {
            packet_data_.push_back(static_cast<char>(i % 256));
        }
    }
    
    void TearDown(const ::benchmark::State& state) override {
        packet_data_.clear();
    }
    
protected:
    std::vector<char> packet_data_;
};

BENCHMARK_DEFINE_F(NetworkPacketBenchmark, Compression)(benchmark::State& state) {
    for (auto _ : state) {
        // 간단한 RLE 압축 시뮬레이션
        std::vector<char> compressed;
        compressed.reserve(packet_data_.size());
        
        for (size_t i = 0; i < packet_data_.size(); ) {
            char current = packet_data_[i];
            int count = 1;
            
            while (i + count < packet_data_.size() && 
                   packet_data_[i + count] == current && count < 255) {
                count++;
            }
            
            compressed.push_back(static_cast<char>(count));
            compressed.push_back(current);
            i += count;
        }
        
        benchmark::DoNotOptimize(compressed);
        state.SetBytesProcessed(state.iterations() * packet_data_.size());
    }
}

BENCHMARK_REGISTER_F(NetworkPacketBenchmark, Compression)
    ->Range(1024, 1024*1024)->Unit(benchmark::kMillisecond);

// 벤치마크 실행을 위한 메인 함수
BENCHMARK_MAIN();
```

### **4.2 메모리 사용량과 캐시 성능 측정**

```cpp
// 🧠 메모리 효율성과 캐시 성능 벤치마크

#include <benchmark/benchmark.h>
#include <chrono>
#include <random>
#include <memory>

// 캐시 친화적 vs 비친화적 데이터 구조 비교
struct AoS_Entity {  // Array of Structures
    float x, y, z;
    float vx, vy, vz;
    int health;
    int level;
    uint64_t id;
    char name[32];
    
    void Update() {
        x += vx;
        y += vy;
        z += vz;
    }
};

// Structure of Arrays
struct SoA_Entities {
    std::vector<float> x, y, z;
    std::vector<float> vx, vy, vz;
    std::vector<int> health, level;
    std::vector<uint64_t> id;
    std::vector<std::array<char, 32>> names;
    
    size_t size() const { return x.size(); }
    
    void resize(size_t new_size) {
        x.resize(new_size);
        y.resize(new_size);
        z.resize(new_size);
        vx.resize(new_size);
        vy.resize(new_size);
        vz.resize(new_size);
        health.resize(new_size);
        level.resize(new_size);
        id.resize(new_size);
        names.resize(new_size);
    }
    
    void UpdateAll() {
        for (size_t i = 0; i < size(); ++i) {
            x[i] += vx[i];
            y[i] += vy[i];
            z[i] += vz[i];
        }
    }
};

static void BM_AoS_Update(benchmark::State& state) {
    std::vector<AoS_Entity> entities(state.range(0));
    
    // 초기화
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    
    for (auto& entity : entities) {
        entity.x = dist(gen) * 1000;
        entity.y = dist(gen) * 1000;
        entity.z = dist(gen) * 1000;
        entity.vx = dist(gen);
        entity.vy = dist(gen);
        entity.vz = dist(gen);
    }
    
    for (auto _ : state) {
        for (auto& entity : entities) {
            entity.Update();
        }
        benchmark::DoNotOptimize(entities.data());
    }
    
    state.SetItemsProcessed(state.iterations() * state.range(0));
    state.SetBytesProcessed(state.iterations() * state.range(0) * sizeof(AoS_Entity));
}

static void BM_SoA_Update(benchmark::State& state) {
    SoA_Entities entities;
    entities.resize(state.range(0));
    
    // 초기화
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    
    for (size_t i = 0; i < entities.size(); ++i) {
        entities.x[i] = dist(gen) * 1000;
        entities.y[i] = dist(gen) * 1000;
        entities.z[i] = dist(gen) * 1000;
        entities.vx[i] = dist(gen);
        entities.vy[i] = dist(gen);
        entities.vz[i] = dist(gen);
    }
    
    for (auto _ : state) {
        entities.UpdateAll();
        benchmark::DoNotOptimize(entities.x.data());
    }
    
    state.SetItemsProcessed(state.iterations() * state.range(0));
    state.SetBytesProcessed(state.iterations() * state.range(0) * 3 * sizeof(float));
}

BENCHMARK(BM_AoS_Update)->Range(1000, 1000000);
BENCHMARK(BM_SoA_Update)->Range(1000, 1000000);

// 메모리 할당 패턴 벤치마크
static void BM_MemoryPool_Allocation(benchmark::State& state) {
    const size_t pool_size = 1024 * 1024;  // 1MB
    const size_t block_size = 64;  // 64 bytes per allocation
    
    // 간단한 메모리 풀 구현
    class SimpleMemoryPool {
        std::unique_ptr<char[]> memory_;
        std::vector<void*> free_blocks_;
        size_t block_size_;
        
    public:
        SimpleMemoryPool(size_t total_size, size_t block_size)
            : memory_(std::make_unique<char[]>(total_size))
            , block_size_(block_size) {
            
            size_t block_count = total_size / block_size;
            free_blocks_.reserve(block_count);
            
            for (size_t i = 0; i < block_count; ++i) {
                free_blocks_.push_back(memory_.get() + i * block_size);
            }
        }
        
        void* Allocate() {
            if (free_blocks_.empty()) return nullptr;
            
            void* ptr = free_blocks_.back();
            free_blocks_.pop_back();
            return ptr;
        }
        
        void Deallocate(void* ptr) {
            free_blocks_.push_back(ptr);
        }
    };
    
    SimpleMemoryPool pool(pool_size, block_size);
    std::vector<void*> allocated_ptrs;
    allocated_ptrs.reserve(state.range(0));
    
    for (auto _ : state) {
        // 할당
        for (int i = 0; i < state.range(0); ++i) {
            void* ptr = pool.Allocate();
            if (ptr) {
                allocated_ptrs.push_back(ptr);
                benchmark::DoNotOptimize(ptr);
            }
        }
        
        // 해제
        for (void* ptr : allocated_ptrs) {
            pool.Deallocate(ptr);
        }
        allocated_ptrs.clear();
    }
}

static void BM_Standard_Allocation(benchmark::State& state) {
    const size_t block_size = 64;
    std::vector<std::unique_ptr<char[]>> allocated_ptrs;
    allocated_ptrs.reserve(state.range(0));
    
    for (auto _ : state) {
        // 할당
        for (int i = 0; i < state.range(0); ++i) {
            auto ptr = std::make_unique<char[]>(block_size);
            benchmark::DoNotOptimize(ptr.get());
            allocated_ptrs.push_back(std::move(ptr));
        }
        
        // 해제는 자동으로 됨
        allocated_ptrs.clear();
    }
}

BENCHMARK(BM_MemoryPool_Allocation)->Range(100, 10000);
BENCHMARK(BM_Standard_Allocation)->Range(100, 10000);

// CPU 캐시 미스 측정 (간접적)
static void BM_Sequential_Access(benchmark::State& state) {
    std::vector<int> data(state.range(0));
    std::iota(data.begin(), data.end(), 0);
    
    for (auto _ : state) {
        long long sum = 0;
        for (int value : data) {
            sum += value;
        }
        benchmark::DoNotOptimize(sum);
    }
    
    state.SetBytesProcessed(state.iterations() * state.range(0) * sizeof(int));
}

static void BM_Random_Access(benchmark::State& state) {
    std::vector<int> data(state.range(0));
    std::iota(data.begin(), data.end(), 0);
    
    // 랜덤 인덱스 생성
    std::vector<size_t> indices(state.range(0));
    std::iota(indices.begin(), indices.end(), 0);
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(indices.begin(), indices.end(), g);
    
    for (auto _ : state) {
        long long sum = 0;
        for (size_t idx : indices) {
            sum += data[idx];
        }
        benchmark::DoNotOptimize(sum);
    }
    
    state.SetBytesProcessed(state.iterations() * state.range(0) * sizeof(int));
}

BENCHMARK(BM_Sequential_Access)->Range(1024, 1024*1024);
BENCHMARK(BM_Random_Access)->Range(1024, 1024*1024);
```

---

## 📝 정리 및 실전 적용 가이드

### **완성된 테스트 전략**

```
GameServer/
├── tests/
│   ├── unit/                    # 단위 테스트
│   │   ├── test_player.cpp      # Google Test
│   │   ├── test_combat.cpp
│   │   └── test_inventory.cpp
│   ├── integration/             # 통합 테스트
│   │   ├── test_matchmaking.cpp # Catch2 BDD
│   │   └── test_database.cpp
│   ├── performance/             # 성능 테스트
│   │   ├── bench_spatial.cpp    # Google Benchmark
│   │   ├── bench_memory.cpp
│   │   └── bench_network.cpp
│   ├── mocks/                   # Mock 객체들
│   │   ├── mock_database.h
│   │   └── mock_network.h
│   └── CMakeLists.txt
```

### **테스트 실행 자동화**

```bash
# 모든 테스트 실행
make test

# 특정 카테고리만 실행
ctest -L unit
ctest -L integration
ctest -L benchmark

# 코드 커버리지 측정
make coverage

# 메모리 체크
ctest -T memcheck
```

### **다음 학습 권장사항**

1. **[디버깅과 프로파일링 도구](./36_debugging_profiling_toolchain.md)** 🔥
2. **[현대 C++ 컨테이너와 유틸리티](./37_modern_cpp_containers_utilities.md)** 🔥

---

**💡 핵심 포인트**: 체계적인 테스팅은 게임 서버 개발의 필수 요소입니다. 단위 테스트로 로직을 검증하고, 통합 테스트로 시스템 간 상호작용을 확인하며, 성능 테스트로 확장성을 보장하는 삼중 안전망을 구축해야 합니다.