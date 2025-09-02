# 1단계: C++ 프로그래밍 기초 - 게임 서버 개발을 위한 필수 지식
*게임 서버 개발자가 되기 위한 첫 걸음, C++ 마스터하기*

> **🎯 목표**: 게임 서버 개발에 필요한 C++ 핵심 개념을 완전 초보도 이해할 수 있도록 설명

---

## 📋 문서 정보

- **난이도**: 🟢 초급 (프로그래밍 첫 경험도 OK)
- **예상 학습 시간**: 4-5시간 (기초 이론 + 실습)
- **필요 선수 지식**: 
  - ✅ 컴퓨터 기본 사용법 (파일, 폴더 개념)
  - ✅ "프로그래밍"이 뭔지 대략 알고 있음
  - ✅ 변수, 함수가 뭔지 들어본 적 있음 (선택사항)
- **실습 환경**: C++17 이상 컴파일러 (Visual Studio 추천)
- **최종 결과물**: 간단한 플레이어 관리 시스템 완성!

---

## 🤔 왜 C++을 사용할까?

### **게임 서버에서 C++이 중요한 이유**

```cpp
// ❌ Python 코드 (느림)
def process_players(players):
    for player in players:
        update_position(player)
        check_collision(player)
    # 5000명 처리 시 약 500ms 소요

// ✅ C++ 코드 (빠름)  
void ProcessPlayers(std::vector<Player>& players) {
    for (auto& player : players) {
        UpdatePosition(player);
        CheckCollision(player);
    }
    // 5000명 처리 시 약 10ms 소요
}
```

**성능 차이**: C++은 Python보다 **50배 빠름**
- **실시간 게임**: 16ms 안에 모든 계산 완료 필요 (60fps 기준)
- **동시 접속자**: 5000명이 동시에 움직일 때도 끊김 없이 처리
- **메모리 효율**: 같은 데이터를 1/10 메모리로 처리 가능

---

## 📚 1. C++ 기본 문법 (게임 서버 관점)

### **1.1 변수와 데이터 타입**

```cpp
#include <iostream>
#include <string>

int main() {
    // 🎮 게임에서 자주 사용하는 데이터 타입들
    
    // 플레이어 ID (정수)
    uint21_t player_id = 12345;           // 64비트 양의 정수 (아주 큰 숫자)
    
    // 플레이어 위치 (실수)
    float position_x = 100.5f;            // 32비트 실수 (소수점)
    float position_y = 200.3f;
    
    // 플레이어 이름 (문자열)
    std::string player_name = "DragonSlayer";
    
    // 플레이어 레벨 (작은 정수)
    int level = 25;                       // 32비트 정수
    
    // HP/MP (더 정확한 실수)
    double health = 1000.0;               // 64비트 실수 (더 정확함)
    
    // 온라인 상태 (참/거짓)
    bool is_online = true;                // true 또는 false
    
    std::cout << "플레이어 " << player_name 
              << " (ID: " << player_id << ")"
              << " 위치: (" << position_x << ", " << position_y << ")"
              << " 레벨: " << level << std::endl;
    
    return 0;
}
```

**💡 게임 서버에서 중요한 이유:**
- `uint21_t`: 수십억 명의 플레이어 ID도 저장 가능
- `float`: 3D 좌표, 속도 계산에 사용
- `std::string`: 플레이어 이름, 채팅 메시지 저장

### **1.2 함수 (기능을 모듈화)**

```cpp
#include <iostream>
#include <cmath>

// 🎮 두 플레이어 사이의 거리 계산 함수
float CalculateDistance(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1;                   // x축 차이
    float dy = y2 - y1;                   // y축 차이
    return std::sqrt(dx * dx + dy * dy);  // 피타고라스 정리
}

// 🎮 플레이어가 공격 범위 안에 있는지 확인
bool IsInAttackRange(float attacker_x, float attacker_y, 
                     float target_x, float target_y, 
                     float attack_range) {
    float distance = CalculateDistance(attacker_x, attacker_y, target_x, target_y);
    return distance <= attack_range;
}

// 🎮 데미지 계산 함수
int CalculateDamage(int attacker_level, int target_defense) {
    int base_damage = attacker_level * 10;
    int final_damage = base_damage - target_defense;
    
    // 최소 데미지는 1
    return (final_damage < 1) ? 1 : final_damage;
}

int main() {
    // 실제 게임 상황 시뮬레이션
    float player1_x = 100.0f, player1_y = 200.0f;  // 플레이어1 위치
    float player2_x = 150.0f, player2_y = 250.0f;  // 플레이어2 위치
    float sword_range = 80.0f;                      // 검의 공격 범위
    
    // 공격 가능한지 확인
    if (IsInAttackRange(player1_x, player1_y, player2_x, player2_y, sword_range)) {
        int damage = CalculateDamage(25, 10);  // 레벨25가 방어력10을 공격
        std::cout << "공격 성공! 데미지: " << damage << std::endl;
    } else {
        std::cout << "너무 멀어서 공격할 수 없습니다." << std::endl;
    }
    
    return 0;
}
```

**💡 함수가 중요한 이유:**
- **재사용성**: 거리 계산을 여러 곳에서 사용 가능
- **유지보수**: 데미지 공식 변경 시 한 곳만 수정
- **가독성**: 코드를 읽기 쉽게 만듦

### **1.3 클래스 (게임 객체 표현)**

```cpp
#include <iostream>
#include <string>

// 🎮 플레이어 클래스 정의
class Player {
private:    // 외부에서 직접 접근 불가 (보안)
    std::string name;
    int level;
    float health;
    float max_health;
    float x, y;  // 위치

public:     // 외부에서 접근 가능
    // 생성자 (플레이어를 만들 때 실행)
    Player(const std::string& player_name, int start_level = 1) {
        name = player_name;
        level = start_level;
        max_health = level * 100.0f;  // 레벨당 체력 100
        health = max_health;
        x = 0.0f;
        y = 0.0f;
        
        std::cout << "플레이어 '" << name << "' 생성됨!" << std::endl;
    }
    
    // 데미지를 받는 함수
    void TakeDamage(float damage) {
        health -= damage;
        if (health < 0) health = 0;  // 체력은 0 미만이 될 수 없음
        
        std::cout << name << "이(가) " << damage << " 데미지를 받았습니다. "
                  << "남은 체력: " << health << "/" << max_health << std::endl;
    }
    
    // 체력 회복 함수
    void Heal(float amount) {
        health += amount;
        if (health > max_health) health = max_health;  // 최대치 초과 불가
        
        std::cout << name << "이(가) " << amount << " 체력을 회복했습니다. "
                  << "현재 체력: " << health << "/" << max_health << std::endl;
    }
    
    // 이동 함수
    void MoveTo(float new_x, float new_y) {
        x = new_x;
        y = new_y;
        std::cout << name << "이(가) (" << x << ", " << y << ")로 이동했습니다." << std::endl;
    }
    
    // 플레이어가 살아있는지 확인
    bool IsAlive() const {
        return health > 0;
    }
    
    // 정보 출력
    void ShowInfo() const {
        std::cout << "=== " << name << " 정보 ===" << std::endl;
        std::cout << "레벨: " << level << std::endl;
        std::cout << "체력: " << health << "/" << max_health << std::endl;
        std::cout << "위치: (" << x << ", " << y << ")" << std::endl;
        std::cout << "상태: " << (IsAlive() ? "생존" : "사망") << std::endl;
    }
    
    // Getter 함수들 (정보 조회용)
    float GetX() const { return x; }
    float GetY() const { return y; }
    std::string GetName() const { return name; }
};

int main() {
    // 🎮 실제 게임 시나리오
    Player warrior("전사왕", 10);      // 10레벨 전사 생성
    Player mage("마법사", 8);          // 8레벨 마법사 생성
    
    std::cout << "\n=== 게임 시작 ===" << std::endl;
    warrior.ShowInfo();
    mage.ShowInfo();
    
    std::cout << "\n=== 전투 시작 ===" << std::endl;
    warrior.MoveTo(100, 100);
    mage.MoveTo(120, 110);
    
    // 전투 시뮬레이션
    warrior.TakeDamage(150);  // 전사가 공격받음
    mage.TakeDamage(200);     // 마법사가 공격받음
    
    warrior.Heal(50);         // 전사가 회복
    
    std::cout << "\n=== 전투 후 상태 ===" << std::endl;
    warrior.ShowInfo();
    mage.ShowInfo();
    
    return 0;
}
```

**💡 클래스가 중요한 이유:**
- **데이터 보호**: private으로 중요한 정보 보호
- **객체지향**: 현실 세계의 개념을 코드로 표현
- **확장성**: 나중에 새로운 기능 추가 용이

---

## 🧠 2. 포인터와 메모리 관리

### **2.1 포인터가 뭔가요?**

```cpp
#include <iostream>

int main() {
    // 📦 일반 변수: 값 자체를 저장
    int player_health = 1000;
    
    // 📮 포인터: 변수가 저장된 "주소"를 저장
    int* health_pointer = &player_health;  // &는 "주소를 가져와라"
    
    std::cout << "체력 값: " << player_health << std::endl;
    std::cout << "체력이 저장된 주소: " << health_pointer << std::endl;
    std::cout << "포인터가 가리키는 값: " << *health_pointer << std::endl;  // *는 "주소의 값"
    
    // 포인터를 통해 값 변경
    *health_pointer = 500;  // player_health가 500으로 변경됨!
    std::cout << "포인터로 변경 후 체력: " << player_health << std::endl;
    
    return 0;
}
```

**💡 게임 서버에서 포인터가 중요한 이유:**
- **메모리 효율**: 큰 객체를 복사하지 않고 주소만 전달
- **동적 할당**: 플레이어 수에 따라 메모리를 유연하게 할당

### **2.2 동적 메모리 할당 (new/delete의 위험성)**

```cpp
#include <iostream>

class Player {
public:
    std::string name;
    int level;
    
    Player(const std::string& n, int l) : name(n), level(l) {
        std::cout << "플레이어 " << name << " 생성됨" << std::endl;
    }
    
    ~Player() {  // 소멸자 (객체가 삭제될 때 실행)
        std::cout << "플레이어 " << name << " 삭제됨" << std::endl;
    }
};

int main() {
    // ❌ 위험한 방식: new/delete 직접 사용
    std::cout << "=== 위험한 방식 ===" << std::endl;
    Player* player1 = new Player("위험한플레이어", 10);  // 힙 메모리에 생성
    
    // 만약 여기서 예외가 발생하면?
    // delete player1;  // 이 줄이 실행되지 않으면 메모리 누수!
    
    // ⚠️ delete를 깜빡하면 메모리 누수 발생!
    delete player1;  // 수동으로 메모리 해제
    
    return 0;
}
```

### **2.3 스마트 포인터 (안전한 메모리 관리)**

```cpp
#include <iostream>
#include <memory>  // 스마트 포인터 헤더

class Player {
public:
    std::string name;
    int level;
    
    Player(const std::string& n, int l) : name(n), level(l) {
        std::cout << "플레이어 " << name << " 생성됨" << std::endl;
    }
    
    ~Player() {
        std::cout << "플레이어 " << name << " 자동 삭제됨" << std::endl;
    }
    
    void ShowInfo() {
        std::cout << "이름: " << name << ", 레벨: " << level << std::endl;
    }
};

int main() {
    std::cout << "=== 안전한 방식: 스마트 포인터 ===" << std::endl;
    
    // ✅ unique_ptr: 하나의 소유자만 가능
    std::unique_ptr<Player> player1 = std::make_unique<Player>("안전한플레이어", 15);
    player1->ShowInfo();  // -> 는 포인터로 멤버에 접근하는 방법
    
    // ✅ shared_ptr: 여러 소유자 가능 (게임에서 자주 사용)
    std::shared_ptr<Player> player2 = std::make_shared<Player>("공유플레이어", 20);
    
    {
        std::shared_ptr<Player> player2_copy = player2;  // 소유권 공유
        std::cout << "참조 횟수: " << player2.use_count() << std::endl;  // 2개
        
        player2_copy->ShowInfo();
    }  // player2_copy가 scope를 벗어나면 자동으로 참조 해제
    
    std::cout << "참조 횟수: " << player2.use_count() << std::endl;  // 1개
    
    // 🎮 게임 서버에서의 실제 사용 예
    std::vector<std::shared_ptr<Player>> online_players;  // 온라인 플레이어 목록
    
    online_players.push_back(player2);  // 플레이어를 목록에 추가
    std::cout << "온라인 플레이어 수: " << online_players.size() << std::endl;
    
    // 여기서 main이 끝나면 모든 스마트 포인터가 자동으로 메모리 해제!
    // delete를 호출할 필요 없음!
    
    return 0;
}
```

**💡 스마트 포인터의 장점:**
- **자동 메모리 관리**: delete 호출 불필요
- **예외 안전성**: 예외 발생 시에도 메모리 누수 없음
- **명확한 소유권**: 누가 메모리를 관리하는지 명확

---

## 📦 3. STL 컨테이너 (데이터 저장소)

### **3.1 vector (동적 배열)**

```cpp
#include <iostream>
#include <vector>
#include <string>

int main() {
    // 🎮 온라인 플레이어 목록
    std::vector<std::string> online_players;
    
    // 플레이어 추가
    online_players.push_back("DragonSlayer");
    online_players.push_back("MagicMaster");
    online_players.push_back("SwordKing");
    
    std::cout << "현재 온라인 플레이어 수: " << online_players.size() << std::endl;
    
    // 모든 플레이어 출력
    std::cout << "=== 온라인 플레이어 목록 ===" << std::endl;
    for (size_t i = 0; i < online_players.size(); ++i) {
        std::cout << i + 1 << ". " << online_players[i] << std::endl;
    }
    
    // 🎮 범위 기반 for문 (더 간단한 방법)
    std::cout << "\n=== 간단한 방법으로 출력 ===" << std::endl;
    for (const auto& player : online_players) {
        std::cout << "- " << player << std::endl;
    }
    
    // 특정 플레이어 찾기
    std::string target = "MagicMaster";
    bool found = false;
    for (const auto& player : online_players) {
        if (player == target) {
            found = true;
            break;
        }
    }
    
    if (found) {
        std::cout << target << "을(를) 찾았습니다!" << std::endl;
    }
    
    return 0;
}
```

### **3.2 map (키-값 저장소)**

```cpp
#include <iostream>
#include <map>
#include <string>

int main() {
    // 🎮 플레이어 ID와 이름을 연결하는 맵
    std::map<uint21_t, std::string> player_names;
    
    // 플레이어 정보 추가
    player_names[12345] = "DragonSlayer";
    player_names[67890] = "MagicMaster";
    player_names[11111] = "SwordKing";
    
    // 🎮 플레이어 레벨 정보
    std::map<std::string, int> player_levels;
    player_levels["DragonSlayer"] = 25;
    player_levels["MagicMaster"] = 30;
    player_levels["SwordKing"] = 18;
    
    // ID로 플레이어 이름 찾기
    uint21_t search_id = 67890;
    if (player_names.find(search_id) != player_names.end()) {
        std::string name = player_names[search_id];
        int level = player_levels[name];
        
        std::cout << "플레이어 ID " << search_id << ": " 
                  << name << " (레벨 " << level << ")" << std::endl;
    }
    
    // 모든 플레이어 정보 출력
    std::cout << "\n=== 전체 플레이어 목록 ===" << std::endl;
    for (const auto& pair : player_names) {
        uint21_t id = pair.first;      // 키 (ID)
        std::string name = pair.second; // 값 (이름)
        int level = player_levels[name];
        
        std::cout << "ID: " << id << ", 이름: " << name 
                  << ", 레벨: " << level << std::endl;
    }
    
    return 0;
}
```

### **3.3 queue (대기열)**

```cpp
#include <iostream>
#include <queue>
#include <string>

int main() {
    // 🎮 매치메이킹 대기열
    std::queue<std::string> matchmaking_queue;
    
    // 플레이어들이 대기열에 입장
    matchmaking_queue.push("DragonSlayer");
    matchmaking_queue.push("MagicMaster");
    matchmaking_queue.push("SwordKing");
    matchmaking_queue.push("FireMage");
    
    std::cout << "대기열에 " << matchmaking_queue.size() << "명이 대기 중입니다." << std::endl;
    
    // 🎮 매치 만들기 (2명씩 짝짓기)
    int match_number = 1;
    while (matchmaking_queue.size() >= 2) {
        std::string player1 = matchmaking_queue.front();  // 첫 번째 플레이어
        matchmaking_queue.pop();  // 대기열에서 제거
        
        std::string player2 = matchmaking_queue.front();  // 두 번째 플레이어
        matchmaking_queue.pop();  // 대기열에서 제거
        
        std::cout << "매치 " << match_number << ": " 
                  << player1 << " vs " << player2 << std::endl;
        match_number++;
    }
    
    // 남은 플레이어 확인
    if (!matchmaking_queue.empty()) {
        std::cout << matchmaking_queue.front() << "는 아직 대기 중입니다." << std::endl;
    }
    
    return 0;
}
```

**💡 STL 컨테이너의 중요성:**
- **vector**: 플레이어 목록, 인벤토리 아이템 등
- **map**: 플레이어 정보 검색, 설정 저장 등
- **queue**: 매치메이킹, 이벤트 처리 순서 등

---

## 🚀 4. 모던 C++ 핵심 기능

### **4.1 auto 키워드 (타입 자동 추론)**

```cpp
#include <iostream>
#include <vector>
#include <map>
#include <string>

int main() {
    // 😵 길고 복잡한 타입명
    std::map<std::string, std::vector<int>> player_scores;
    player_scores["DragonSlayer"] = {100, 200, 150};
    player_scores["MagicMaster"] = {180, 190, 210};
    
    // ❌ 길고 복잡한 방식
    for (std::map<std::string, std::vector<int>>::iterator it = player_scores.begin(); 
         it != player_scores.end(); ++it) {
        // 너무 길어서 읽기 어려움...
    }
    
    // ✅ auto 사용: 컴파일러가 자동으로 타입 추론
    for (auto it = player_scores.begin(); it != player_scores.end(); ++it) {
        std::cout << "플레이어: " << it->first << std::endl;
        std::cout << "점수들: ";
        for (auto score : it->second) {  // 여기서도 auto 사용
            std::cout << score << " ";
        }
        std::cout << std::endl;
    }
    
    // 🎮 더 간단한 범위 기반 for문과 auto 조합
    std::cout << "\n=== 더 간단한 방법 ===" << std::endl;
    for (const auto& player_data : player_scores) {
        const auto& name = player_data.first;    // std::string&
        const auto& scores = player_data.second; // std::vector<int>&
        
        std::cout << name << "의 점수: ";
        for (auto score : scores) {
            std::cout << score << " ";
        }
        std::cout << std::endl;
    }
    
    return 0;
}
```

### **4.2 람다 함수 (익명 함수)**

```cpp
#include <iostream>
#include <vector>
#include <algorithm>
#include <string>

struct Player {
    std::string name;
    int level;
    int score;
};

int main() {
    // 🎮 플레이어 목록
    std::vector<Player> players = {
        {"DragonSlayer", 25, 1500},
        {"MagicMaster", 30, 2000},
        {"SwordKing", 18, 800},
        {"FireMage", 22, 1200}
    };
    
    std::cout << "=== 원본 플레이어 목록 ===" << std::endl;
    for (const auto& player : players) {
        std::cout << player.name << " (레벨 " << player.level 
                  << ", 점수 " << player.score << ")" << std::endl;
    }
    
    // 🎮 점수 순으로 정렬 (람다 함수 사용)
    std::sort(players.begin(), players.end(), 
              [](const Player& a, const Player& b) {
                  return a.score > b.score;  // 높은 점수부터
              });
    
    std::cout << "\n=== 점수 순 정렬 ===" << std::endl;
    for (const auto& player : players) {
        std::cout << player.name << " (점수 " << player.score << ")" << std::endl;
    }
    
    // 🎮 특정 조건의 플레이어 찾기
    auto high_level_player = std::find_if(players.begin(), players.end(),
                                         [](const Player& p) {
                                             return p.level >= 25;  // 레벨 25 이상
                                         });
    
    if (high_level_player != players.end()) {
        std::cout << "\n고레벨 플레이어 발견: " << high_level_player->name << std::endl;
    }
    
    // 🎮 모든 플레이어에게 보너스 점수 추가
    int bonus = 100;
    std::for_each(players.begin(), players.end(),
                  [bonus](Player& p) {  // bonus를 캡처해서 사용
                      p.score += bonus;
                      std::cout << p.name << "에게 보너스 " << bonus 
                                << "점 추가!" << std::endl;
                  });
    
    return 0;
}
```

**💡 람다 함수의 장점:**
- **간결성**: 짧은 로직을 즉석에서 정의
- **지역성**: 사용하는 곳 근처에서 정의
- **STL 알고리즘**: sort, find_if 등과 완벽한 조합

---

## 🎯 5. 실전 연습: 간단한 게임 서버 시뮬레이터

```cpp
#include <iostream>
#include <vector>
#include <map>
#include <memory>
#include <string>
#include <algorithm>
#include <chrono>
#include <thread>

// 🎮 플레이어 클래스
class Player {
private:
    std::string name;
    int level;
    float x, y;  // 위치
    int health;
    bool online;

public:
    Player(const std::string& n, int l) 
        : name(n), level(l), x(0), y(0), health(l * 100), online(true) {
        std::cout << "플레이어 " << name << " 입장! (레벨 " << level << ")" << std::endl;
    }
    
    void MoveTo(float new_x, float new_y) {
        x = new_x;
        y = new_y;
        std::cout << name << " 이동: (" << x << ", " << y << ")" << std::endl;
    }
    
    void Attack(Player& target) {
        if (!online || !target.online) return;
        
        int damage = level * 20;
        target.health -= damage;
        
        std::cout << name << "이(가) " << target.name << "을(를) 공격! "
                  << "데미지: " << damage << std::endl;
        
        if (target.health <= 0) {
            target.health = 0;
            std::cout << target.name << " 사망!" << std::endl;
        }
    }
    
    // Getter 함수들
    const std::string& GetName() const { return name; }
    int GetLevel() const { return level; }
    int GetHealth() const { return health; }
    bool IsAlive() const { return health > 0; }
    bool IsOnline() const { return online; }
    
    void Disconnect() { 
        online = false; 
        std::cout << name << " 접속 종료" << std::endl;
    }
};

// 🎮 게임 서버 클래스
class GameServer {
private:
    std::map<std::string, std::shared_ptr<Player>> players;
    int max_players;

public:
    GameServer(int max) : max_players(max) {
        std::cout << "🎮 게임 서버 시작! (최대 " << max_players << "명)" << std::endl;
    }
    
    bool AddPlayer(const std::string& name, int level) {
        if (players.size() >= max_players) {
            std::cout << "❌ 서버 만원! " << name << " 입장 실패" << std::endl;
            return false;
        }
        
        if (players.find(name) != players.end()) {
            std::cout << "❌ 이미 존재하는 플레이어: " << name << std::endl;
            return false;
        }
        
        auto player = std::make_shared<Player>(name, level);
        players[name] = player;
        return true;
    }
    
    void RemovePlayer(const std::string& name) {
        auto it = players.find(name);
        if (it != players.end()) {
            it->second->Disconnect();
            players.erase(it);
        }
    }
    
    void SimulateBattle(const std::string& attacker, const std::string& target) {
        auto att_it = players.find(attacker);
        auto tar_it = players.find(target);
        
        if (att_it == players.end() || tar_it == players.end()) {
            std::cout << "❌ 플레이어를 찾을 수 없습니다." << std::endl;
            return;
        }
        
        auto& att_player = *att_it->second;
        auto& tar_player = *tar_it->second;
        
        if (!att_player.IsAlive() || !tar_player.IsAlive()) {
            std::cout << "❌ 사망한 플레이어는 전투할 수 없습니다." << std::endl;
            return;
        }
        
        std::cout << "\n⚔️ 전투 시작: " << attacker << " vs " << target << std::endl;
        att_player.Attack(tar_player);
    }
    
    void ShowServerStatus() {
        std::cout << "\n📊 서버 현황" << std::endl;
        std::cout << "온라인 플레이어: " << players.size() << "/" << max_players << std::endl;
        
        // 레벨 순으로 정렬해서 표시
        std::vector<std::shared_ptr<Player>> sorted_players;
        for (const auto& pair : players) {
            if (pair.second->IsOnline()) {
                sorted_players.push_back(pair.second);
            }
        }
        
        std::sort(sorted_players.begin(), sorted_players.end(),
                  [](const std::shared_ptr<Player>& a, const std::shared_ptr<Player>& b) {
                      return a->GetLevel() > b->GetLevel();
                  });
        
        std::cout << "=== 플레이어 목록 (레벨순) ===" << std::endl;
        for (const auto& player : sorted_players) {
            std::cout << "- " << player->GetName() 
                      << " (레벨 " << player->GetLevel() 
                      << ", 체력 " << player->GetHealth() << ")"
                      << (player->IsAlive() ? " [생존]" : " [사망]") << std::endl;
        }
    }
};

int main() {
    // 🎮 게임 서버 생성
    GameServer server(10);
    
    // 플레이어들 입장
    server.AddPlayer("DragonSlayer", 25);
    server.AddPlayer("MagicMaster", 30);
    server.AddPlayer("SwordKing", 18);
    server.AddPlayer("FireMage", 22);
    server.AddPlayer("IceWizard", 20);
    
    // 서버 상태 확인
    server.ShowServerStatus();
    
    // 전투 시뮬레이션
    std::cout << "\n🎯 전투 시뮬레이션 시작!" << std::endl;
    
    server.SimulateBattle("DragonSlayer", "SwordKing");
    server.SimulateBattle("MagicMaster", "FireMage");
    server.SimulateBattle("IceWizard", "DragonSlayer");
    
    // 최종 상태
    server.ShowServerStatus();
    
    // 일부 플레이어 퇴장
    std::cout << "\n👋 일부 플레이어 퇴장" << std::endl;
    server.RemovePlayer("SwordKing");
    server.RemovePlayer("FireMage");
    
    server.ShowServerStatus();
    
    std::cout << "\n🎮 게임 서버 시뮬레이션 완료!" << std::endl;
    
    return 0;
}
```

---

## 📚 6. 다음 단계를 위한 학습 가이드

### **6.1 필수 연습 과제**

1. **기본 문법 숙달**
   ```cpp
   // ✅ 해보세요: 플레이어 인벤토리 클래스 만들기
   class Inventory {
       // 아이템 이름과 개수를 저장하는 map
       // AddItem, RemoveItem, ShowInventory 함수 구현
   };
   ```

2. **STL 컨테이너 연습**
   ```cpp
   // ✅ 해보세요: 친구 시스템 구현
   // map을 사용해서 플레이어별 친구 목록 관리
   // 친구 추가/삭제/검색 기능 구현
   ```

3. **스마트 포인터 연습**
   ```cpp
   // ✅ 해보세요: 길드 시스템 구현
   // shared_ptr을 사용해서 플레이어가 여러 길드에 소속될 수 있는 시스템
   ```

### **6.2 추천 학습 순서**

1. **1주차**: C++ 기본 문법, 클래스, STL 기초
2. **2주차**: 스마트 포인터, 모던 C++ 기능 (auto, 람다)
3. **3주차**: 실전 프로젝트 - 간단한 게임 로직 구현
4. **4주차**: 다음 단계 준비 - 네트워크 프로그래밍 기초 학습

### **6.3 유용한 참고 자료**

- **온라인 컴파일러**: [https://godbolt.org/](https://godbolt.org/) (코드 즉시 테스트)
- **C++ 레퍼런스**: [https://cppreference.com/](https://cppreference.com/)
- **연습 문제**: [https://www.hackerrank.com/](https://www.hackerrank.com/) C++ 도메인

---

## 🎯 마무리

이제 여러분은 게임 서버 개발에 필요한 **C++ 기초 지식**을 갖추었습니다!

**다음 2단계에서 배울 내용:**
- **네트워크 프로그래밍 기초**: 클라이언트와 서버가 어떻게 통신하는가
- **TCP vs UDP**: 언제 어떤 프로토콜을 사용해야 하는가
- **소켓 프로그래밍**: 실제 네트워크 코드 작성법

**💪 지금 할 수 있는 것들:**
- ✅ 게임 객체를 클래스로 설계
- ✅ STL 컨테이너로 데이터 관리
- ✅ 스마트 포인터로 안전한 메모리 관리
- ✅ 모던 C++ 기능으로 깔끔한 코드 작성

## 💡 실습 과제 답안 예시

### 과제 1: 플레이어 인벤토리 클래스
```cpp
class Inventory {
private:
    std::map<std::string, int> items;
    int max_slots;
    
public:
    Inventory(int slots = 20) : max_slots(slots) {}
    
    bool AddItem(const std::string& item_name, int count = 1) {
        if (items.size() >= max_slots && items.find(item_name) == items.end()) {
            std::cout << "인벤토리가 가득 찼습니다!" << std::endl;
            return false;
        }
        
        items[item_name] += count;
        std::cout << item_name << " " << count << "개 추가됨" << std::endl;
        return true;
    }
    
    bool RemoveItem(const std::string& item_name, int count = 1) {
        auto it = items.find(item_name);
        if (it == items.end()) {
            std::cout << item_name << "을(를) 찾을 수 없습니다." << std::endl;
            return false;
        }
        
        if (it->second < count) {
            std::cout << "수량이 부족합니다." << std::endl;
            return false;
        }
        
        it->second -= count;
        if (it->second == 0) {
            items.erase(it);
        }
        return true;
    }
    
    void ShowInventory() const {
        std::cout << "=== 인벤토리 (" << items.size() << "/" << max_slots << ") ===" << std::endl;
        for (const auto& [item, count] : items) {
            std::cout << "- " << item << ": " << count << "개" << std::endl;
        }
    }
};

// 사용 예시
int main() {
    Inventory inv(5);  // 5칸 인벤토리
    
    inv.AddItem("체력 포션", 3);
    inv.AddItem("마나 포션", 2);
    inv.AddItem("철 검");
    
    inv.ShowInventory();
    
    inv.RemoveItem("체력 포션", 1);
    inv.ShowInventory();
    
    return 0;
}
```

### 과제 2: 친구 시스템
```cpp
class FriendSystem {
private:
    std::map<std::string, std::set<std::string>> friend_lists;
    
public:
    bool AddFriend(const std::string& player, const std::string& friend_name) {
        if (player == friend_name) {
            std::cout << "자기 자신은 친구로 추가할 수 없습니다." << std::endl;
            return false;
        }
        
        // 양방향 친구 관계
        friend_lists[player].insert(friend_name);
        friend_lists[friend_name].insert(player);
        
        std::cout << player << "님과 " << friend_name << "님이 친구가 되었습니다!" << std::endl;
        return true;
    }
    
    bool RemoveFriend(const std::string& player, const std::string& friend_name) {
        auto it = friend_lists.find(player);
        if (it == friend_lists.end() || it->second.find(friend_name) == it->second.end()) {
            std::cout << "친구 관계가 아닙니다." << std::endl;
            return false;
        }
        
        friend_lists[player].erase(friend_name);
        friend_lists[friend_name].erase(player);
        
        return true;
    }
    
    void ShowFriends(const std::string& player) const {
        auto it = friend_lists.find(player);
        if (it == friend_lists.end() || it->second.empty()) {
            std::cout << player << "님의 친구가 없습니다." << std::endl;
            return;
        }
        
        std::cout << "=== " << player << "님의 친구 목록 ===" << std::endl;
        for (const auto& friend_name : it->second) {
            std::cout << "- " << friend_name << std::endl;
        }
    }
    
    bool AreFriends(const std::string& player1, const std::string& player2) const {
        auto it = friend_lists.find(player1);
        return it != friend_lists.end() && it->second.count(player2) > 0;
    }
};
```

### 과제 3: 길드 시스템
```cpp
class Guild {
public:
    std::string name;
    std::string master;
    std::vector<std::string> members;
    int level;
    
    Guild(const std::string& n, const std::string& m) 
        : name(n), master(m), level(1) {
        members.push_back(master);
    }
};

class GuildSystem {
private:
    std::map<std::string, std::shared_ptr<Guild>> guilds;
    std::map<std::string, std::vector<std::shared_ptr<Guild>>> player_guilds;
    
public:
    std::shared_ptr<Guild> CreateGuild(const std::string& guild_name, 
                                      const std::string& master_name) {
        if (guilds.find(guild_name) != guilds.end()) {
            std::cout << "이미 존재하는 길드명입니다." << std::endl;
            return nullptr;
        }
        
        auto guild = std::make_shared<Guild>(guild_name, master_name);
        guilds[guild_name] = guild;
        player_guilds[master_name].push_back(guild);
        
        std::cout << guild_name << " 길드가 생성되었습니다!" << std::endl;
        return guild;
    }
    
    bool JoinGuild(const std::string& player_name, const std::string& guild_name) {
        auto it = guilds.find(guild_name);
        if (it == guilds.end()) {
            std::cout << "존재하지 않는 길드입니다." << std::endl;
            return false;
        }
        
        auto guild = it->second;
        
        // 이미 멤버인지 확인
        auto& members = guild->members;
        if (std::find(members.begin(), members.end(), player_name) != members.end()) {
            std::cout << "이미 길드 멤버입니다." << std::endl;
            return false;
        }
        
        members.push_back(player_name);
        player_guilds[player_name].push_back(guild);
        
        std::cout << player_name << "님이 " << guild_name << " 길드에 가입했습니다!" << std::endl;
        return true;
    }
    
    void ShowPlayerGuilds(const std::string& player_name) const {
        auto it = player_guilds.find(player_name);
        if (it == player_guilds.end() || it->second.empty()) {
            std::cout << player_name << "님은 길드에 소속되어 있지 않습니다." << std::endl;
            return;
        }
        
        std::cout << "=== " << player_name << "님의 소속 길드 ===" << std::endl;
        for (const auto& guild : it->second) {
            std::cout << "- " << guild->name << " (레벨 " << guild->level << ")" << std::endl;
        }
    }
};
```

## 실습 프로젝트

### 프로젝트 1: 플레이어 관리 시스템 (기초)
**목표**: 기본적인 플레이어 정보 관리

**구현 사항**:
- Player 클래스 (이름, 레벨, 경험치)
- 플레이어 목록 관리 (vector 사용)
- 레벨업 시스템
- 플레이어 검색 기능

### 프로젝트 2: 인벤토리 & 아이템 시스템 (중급)
**목표**: 게임 아이템 관리 시스템 구축

**구현 사항**:
- Item 기본 클래스와 파생 클래스들
- 인벤토리 관리 (map 사용)
- 아이템 거래 시스템
- 스마트 포인터로 메모리 관리

### 프로젝트 3: 미니 게임 서버 (고급)
**목표**: 실제 동작하는 게임 서버 프로토타입

**구현 사항**:
- 다중 플레이어 관리
- 전투 시스템
- 이벤트 처리 (queue 사용)
- 상태 저장/로드 기능

## 🔥 흔한 실수와 해결방법

### 1. 메모리 누수

#### [SEQUENCE: 1] new/delete 불일치
```cpp
// ❌ 잘못된 예: new[]로 할당했는데 delete로 해제
int* array = new int[100];
delete array;  // 버그! delete[] array;가 맞음

// ✅ 올바른 예: 배열은 delete[]로 해제
int* array = new int[100];
delete[] array;

// 🌟 더 나은 방법: vector 사용
std::vector<int> array(100);  // 자동 메모리 관리
```

### 2. 포인터 오용

#### [SEQUENCE: 2] 댕글링 포인터
```cpp
// ❌ 잘못된 예: 삭제된 객체를 가리키는 포인터
Player* player = new Player("Hero", 10);
delete player;
player->ShowInfo();  // 크래시!

// ✅ 올바른 예: 삭제 후 nullptr로 설정
Player* player = new Player("Hero", 10);
delete player;
player = nullptr;
if (player) player->ShowInfo();  // 안전

// 🌟 더 나은 방법: 스마트 포인터
auto player = std::make_unique<Player>("Hero", 10);
// 자동으로 메모리 관리됨
```

### 3. STL 컨테이너 실수

#### [SEQUENCE: 3] 반복 중 요소 삭제
```cpp
// ❌ 잘못된 예: 반복 중 erase로 인한 무효화
std::vector<int> numbers = {1, 2, 3, 4, 5};
for (auto it = numbers.begin(); it != numbers.end(); ++it) {
    if (*it == 3) {
        numbers.erase(it);  // it가 무효화됨!
    }
}

// ✅ 올바른 예: erase의 반환값 사용
for (auto it = numbers.begin(); it != numbers.end(); ) {
    if (*it == 3) {
        it = numbers.erase(it);
    } else {
        ++it;
    }
}

// 🌟 더 나은 방법: remove_if 사용
numbers.erase(
    std::remove_if(numbers.begin(), numbers.end(), 
                   [](int n) { return n == 3; }),
    numbers.end());
```

## 🎓 학습 확인 체크리스트

### 기초 레벨 ✅
- [ ] 변수 선언과 기본 데이터 타입 사용
- [ ] 함수 정의와 호출
- [ ] 기본적인 클래스 작성
- [ ] new/delete의 위험성 이해

### 중급 레벨 📚
- [ ] STL 컨테이너 3개 이상 활용
- [ ] 스마트 포인터로 안전한 메모리 관리
- [ ] 범위 기반 for문과 auto 활용
- [ ] 람다 함수 기본 사용

### 고급 레벨 🚀
- [ ] 복잡한 클래스 계층 구조 설계
- [ ] STL 알고리즘과 람다 조합
- [ ] 메모리 최적화 기법
- [ ] 모던 C++ 기능 활용

### 전문가 레벨 🏆
- [ ] 템플릿 프로그래밍
- [ ] 완벽한 전달과 이동 의미론
- [ ] 커스텀 메모리 할당자
- [ ] 컴파일 시간 최적화

## 추가 학습 자료

### 추천 도서
- "Effective C++" - Scott Meyers
- "C++ Primer" - Stanley Lippman
- "Game Programming Patterns" - Robert Nystrom
- "C++ Concurrency in Action" - Anthony Williams

### 온라인 리소스
- [C++ Reference](https://en.cppreference.com/)
- [LearnCpp](https://www.learncpp.com/)
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/)
- [Compiler Explorer](https://godbolt.org/)

### 실습 도구
- Visual Studio Community
- CLion (학생 라이선스)
- Online GDB
- Wandbox (온라인 컴파일러)

**🚀 계속 연습하면서 다음 단계를 준비해보세요!**