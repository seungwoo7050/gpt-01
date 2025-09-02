# STL 알고리즘 완전 정복 - 게임 서버 최적화의 핵심
*모든 STL 알고리즘을 게임 서버 개발 관점에서 마스터하기*

> **🎯 목표**: STL의 모든 알고리즘을 이해하고 게임 서버 개발에 효과적으로 활용하여 성능과 코드 품질을 극대화

---

## 📋 문서 정보

- **난이도**: 🟡 중급 → 🔴 고급
- **예상 학습 시간**: 6-8시간
- **필요 선수 지식**: 
  - ✅ C++ 기본 문법과 STL 컨테이너
  - ✅ 템플릿과 람다 함수 이해
  - ✅ 반복자(Iterator) 개념
- **실습 환경**: C++17 이상 (C++20 권장)

---

## 🤔 왜 STL 알고리즘이 게임 서버에 중요할까?

### **게임 서버의 전형적인 데이터 처리 문제들**

```cpp
// 😰 수작업 루프로 작성된 비효율적인 코드
class IneffientGameServer {
public:
    void ProcessPlayers() {
        std::vector<Player> players = GetAllPlayers();
        
        // 문제 1: 수동으로 작성된 정렬 (버그 위험)
        for (size_t i = 0; i < players.size() - 1; ++i) {
            for (size_t j = 0; j < players.size() - i - 1; ++j) {
                if (players[j].GetLevel() < players[j + 1].GetLevel()) {
                    std::swap(players[j], players[j + 1]);
                }
            }
        }
        
        // 문제 2: 비효율적인 검색
        Player* high_level_player = nullptr;
        for (auto& player : players) {
            if (player.GetLevel() >= 50) {
                high_level_player = &player;
                break;  // 첫 번째만 찾고 끝
            }
        }
        
        // 문제 3: 복잡한 필터링 로직
        std::vector<Player> online_players;
        for (const auto& player : players) {
            if (player.IsOnline() && player.GetLevel() >= 10) {
                online_players.push_back(player);
            }
        }
    }
};

// ✨ STL 알고리즘으로 개선된 효율적인 코드
class EfficientGameServer {
public:
    void ProcessPlayers() {
        std::vector<Player> players = GetAllPlayers();
        
        // 해결 1: 최적화된 정렬 (O(n log n))
        std::sort(players.begin(), players.end(), 
            [](const Player& a, const Player& b) {
                return a.GetLevel() > b.GetLevel();
            });
        
        // 해결 2: 효율적인 검색
        auto high_level_it = std::find_if(players.begin(), players.end(),
            [](const Player& p) { return p.GetLevel() >= 50; });
        
        // 해결 3: 간결한 필터링
        std::vector<Player> online_players;
        std::copy_if(players.begin(), players.end(), 
                    std::back_inserter(online_players),
            [](const Player& p) {
                return p.IsOnline() && p.GetLevel() >= 10;
            });
    }
};
```

**💡 STL 알고리즘의 장점:**
- **성능**: 고도로 최적화된 구현
- **안정성**: 철저히 테스트된 코드
- **가독성**: 의도가 명확한 코드
- **표준성**: 모든 C++ 개발자가 이해

---

## 📚 1. 비변경 알고리즘 (Non-modifying Algorithms)

### **1.1 검색 알고리즘들**

```cpp
#include <algorithm>
#include <vector>
#include <string>

// 🔍 게임 서버에서 자주 사용하는 검색 알고리즘들

class SearchAlgorithmsDemo {
private:
    std::vector<Player> players_;
    
public:
    void DemonstrateSearchAlgorithms() {
        // 샘플 데이터 준비
        InitializePlayers();
        
        // 1. std::find - 값으로 찾기
        auto player_it = std::find_if(players_.begin(), players_.end(),
            [](const Player& p) { return p.GetName() == "DragonSlayer"; });
        
        if (player_it != players_.end()) {
            std::cout << "Found player: " << player_it->GetName() << std::endl;
        }
        
        // 2. std::binary_search - 이진 검색 (정렬된 데이터)
        // 먼저 레벨로 정렬
        std::sort(players_.begin(), players_.end(),
            [](const Player& a, const Player& b) {
                return a.GetLevel() < b.GetLevel();
            });
        
        Player dummy_player(0, "", 25);  // 레벨 25 플레이어 찾기
        bool found = std::binary_search(players_.begin(), players_.end(), 
                                       dummy_player,
            [](const Player& a, const Player& b) {
                return a.GetLevel() < b.GetLevel();
            });
        
        // 3. std::lower_bound / std::upper_bound - 범위 검색
        auto lower = std::lower_bound(players_.begin(), players_.end(), 
                                     dummy_player,
            [](const Player& a, const Player& b) {
                return a.GetLevel() < b.GetLevel();
            });
        
        auto upper = std::upper_bound(players_.begin(), players_.end(), 
                                     dummy_player,
            [](const Player& a, const Player& b) {
                return a.GetLevel() < b.GetLevel();
            });
        
        std::cout << "Players with level 25: " 
                  << std::distance(lower, upper) << std::endl;
        
        // 4. std::equal_range - 동시에 범위 찾기
        auto range = std::equal_range(players_.begin(), players_.end(), 
                                     dummy_player,
            [](const Player& a, const Player& b) {
                return a.GetLevel() < b.GetLevel();
            });
        
        std::cout << "Equal range size: " 
                  << std::distance(range.first, range.second) << std::endl;
    }
    
    // 게임 서버 특화 검색 함수들
    std::vector<Player*> FindPlayersByGuild(const std::string& guild_name) {
        std::vector<Player*> result;
        
        // std::find_if를 사용한 길드 멤버 찾기
        for (auto it = players_.begin(); it != players_.end(); ) {
            it = std::find_if(it, players_.end(),
                [&guild_name](const Player& p) {
                    return p.GetGuildName() == guild_name;
                });
            
            if (it != players_.end()) {
                result.push_back(&(*it));
                ++it;
            }
        }
        
        return result;
    }
    
    Player* FindTopPlayerByLevel() {
        auto max_it = std::max_element(players_.begin(), players_.end(),
            [](const Player& a, const Player& b) {
                return a.GetLevel() < b.GetLevel();
            });
        
        return (max_it != players_.end()) ? &(*max_it) : nullptr;
    }
    
    bool HasPlayersInLevel(int min_level, int max_level) {
        return std::any_of(players_.begin(), players_.end(),
            [min_level, max_level](const Player& p) {
                int level = p.GetLevel();
                return level >= min_level && level <= max_level;
            });
    }
    
    bool AllPlayersOnline() {
        return std::all_of(players_.begin(), players_.end(),
            [](const Player& p) { return p.IsOnline(); });
    }
    
    size_t CountOnlinePlayers() {
        return std::count_if(players_.begin(), players_.end(),
            [](const Player& p) { return p.IsOnline(); });
    }

private:
    void InitializePlayers() {
        players_ = {
            Player(1, "DragonSlayer", 50),
            Player(2, "MageKnight", 25),
            Player(3, "ShadowArcher", 35),
            Player(4, "HolyPriest", 25),
            Player(5, "WarriorKing", 40)
        };
    }
};

// 간단한 Player 클래스 정의
class Player {
private:
    uint21_t id_;
    std::string name_;
    int level_;
    bool online_ = true;
    std::string guild_name_ = "DefaultGuild";
    
public:
    Player(uint21_t id, const std::string& name, int level)
        : id_(id), name_(name), level_(level) {}
    
    uint21_t GetId() const { return id_; }
    const std::string& GetName() const { return name_; }
    int GetLevel() const { return level_; }
    bool IsOnline() const { return online_; }
    const std::string& GetGuildName() const { return guild_name_; }
    
    void SetOnline(bool online) { online_ = online; }
    void SetGuildName(const std::string& name) { guild_name_ = name; }
};
```

### **1.2 비교와 매칭 알고리즘들**

```cpp
// 🎯 데이터 비교와 패턴 매칭을 위한 알고리즘들

class ComparisonAlgorithmsDemo {
public:
    void DemonstrateComparisons() {
        std::vector<int> player_scores1 = {100, 200, 300, 400, 500};
        std::vector<int> player_scores2 = {100, 200, 300, 400, 500};
        std::vector<int> player_scores3 = {100, 200, 350, 400, 500};
        
        // 1. std::equal - 두 범위가 같은지 확인
        bool same_scores = std::equal(player_scores1.begin(), player_scores1.end(),
                                     player_scores2.begin());
        std::cout << "Scores are equal: " << same_scores << std::endl;
        
        // 2. std::mismatch - 첫 번째 차이점 찾기
        auto mismatch_pair = std::mismatch(player_scores1.begin(), player_scores1.end(),
                                          player_scores3.begin());
        
        if (mismatch_pair.first != player_scores1.end()) {
            std::cout << "First difference: " << *mismatch_pair.first 
                     << " vs " << *mismatch_pair.second << std::endl;
        }
        
        // 3. std::lexicographical_compare - 사전식 비교
        bool less_than = std::lexicographical_compare(
            player_scores1.begin(), player_scores1.end(),
            player_scores3.begin(), player_scores3.end());
        
        std::cout << "First is lexicographically less: " << less_than << std::endl;
    }
    
    // 게임 서버 특화 비교 함수들
    bool ComparePlayerStats(const std::vector<int>& stats1, 
                           const std::vector<int>& stats2) {
        return std::equal(stats1.begin(), stats1.end(), stats2.begin());
    }
    
    bool IsValidScoreSequence(const std::vector<int>& scores) {
        // 점수가 내림차순인지 확인
        return std::is_sorted(scores.begin(), scores.end(), std::greater<int>());
    }
    
    std::pair<bool, size_t> FindFirstDifference(const std::vector<Player>& team1,
                                               const std::vector<Player>& team2) {
        auto mismatch = std::mismatch(team1.begin(), team1.end(),
                                     team2.begin(),
            [](const Player& a, const Player& b) {
                return a.GetLevel() == b.GetLevel();
            });
        
        bool found_diff = (mismatch.first != team1.end());
        size_t position = std::distance(team1.begin(), mismatch.first);
        
        return {found_diff, position};
    }
};
```

---

## 📚 2. 변경 알고리즘 (Modifying Algorithms)

### **2.1 복사와 이동 알고리즘들**

```cpp
// 📋 데이터 복사와 이동을 위한 알고리즘들

class CopyMoveAlgorithmsDemo {
public:
    void DemonstrateCopyAlgorithms() {
        std::vector<Player> all_players = CreateSamplePlayers();
        
        // 1. std::copy - 기본 복사
        std::vector<Player> copied_players(all_players.size());
        std::copy(all_players.begin(), all_players.end(), copied_players.begin());
        
        // 2. std::copy_if - 조건부 복사
        std::vector<Player> high_level_players;
        std::copy_if(all_players.begin(), all_players.end(),
                    std::back_inserter(high_level_players),
            [](const Player& p) { return p.GetLevel() >= 30; });
        
        // 3. std::copy_n - n개만 복사
        std::vector<Player> first_three;
        first_three.reserve(3);
        std::copy_n(all_players.begin(), 3, std::back_inserter(first_three));
        
        // 4. std::copy_backward - 뒤에서부터 복사 (겹치는 범위에서 안전)
        std::vector<Player> extended_players(all_players.size() + 2);
        std::copy_backward(all_players.begin(), all_players.end(),
                          extended_players.end());
        
        // 5. std::move - 이동 의미론 활용
        std::vector<Player> moved_players;
        moved_players.reserve(all_players.size());
        std::move(all_players.begin(), all_players.end(),
                 std::back_inserter(moved_players));
        // all_players의 요소들은 이제 moved-from 상태
    }
    
    // 게임 서버 특화 복사/이동 예제들
    std::vector<Player> GetOnlinePlayersInGuild(const std::vector<Player>& players,
                                               const std::string& guild_name) {
        std::vector<Player> result;
        
        std::copy_if(players.begin(), players.end(),
                    std::back_inserter(result),
            [&guild_name](const Player& p) {
                return p.IsOnline() && p.GetGuildName() == guild_name;
            });
        
        return result;
    }
    
    void BackupTopPlayers(const std::vector<Player>& players, size_t count) {
        // 레벨 순으로 정렬된 플레이어들 중 상위 n명 백업
        std::vector<Player> sorted_players = players;
        std::partial_sort(sorted_players.begin(), 
                         sorted_players.begin() + count,
                         sorted_players.end(),
            [](const Player& a, const Player& b) {
                return a.GetLevel() > b.GetLevel();
            });
        
        backup_players_.clear();
        backup_players_.reserve(count);
        std::copy_n(sorted_players.begin(), count, 
                   std::back_inserter(backup_players_));
    }
    
    void MovePlayersToNewServer(std::vector<Player>& source_players) {
        // 효율적인 이동을 위해 move 사용
        server_players_.clear();
        server_players_.reserve(source_players.size());
        
        std::move(source_players.begin(), source_players.end(),
                 std::back_inserter(server_players_));
        
        source_players.clear();  // 이동 후 정리
    }

private:
    std::vector<Player> backup_players_;
    std::vector<Player> server_players_;
    
    std::vector<Player> CreateSamplePlayers() {
        return {
            Player(1, "Player1", 25),
            Player(2, "Player2", 35),
            Player(3, "Player3", 45),
            Player(4, "Player4", 15),
            Player(5, "Player5", 55)
        };
    }
};
```

### **2.2 변환 알고리즘들**

```cpp
// 🔄 데이터 변환을 위한 알고리즘들

class TransformAlgorithmsDemo {
public:
    void DemonstrateTransformAlgorithms() {
        std::vector<Player> players = CreateSamplePlayers();
        
        // 1. std::transform - 단일 범위 변환
        std::vector<std::string> player_names;
        player_names.reserve(players.size());
        
        std::transform(players.begin(), players.end(),
                      std::back_inserter(player_names),
            [](const Player& p) { return p.GetName(); });
        
        // 2. std::transform - 이중 범위 변환
        std::vector<int> levels = {10, 20, 30, 40, 50};
        std::vector<int> bonuses = {5, 10, 15, 20, 25};
        std::vector<int> total_levels;
        total_levels.reserve(levels.size());
        
        std::transform(levels.begin(), levels.end(),
                      bonuses.begin(),
                      std::back_inserter(total_levels),
            [](int level, int bonus) { return level + bonus; });
        
        // 3. std::replace - 값 교체
        std::vector<int> scores = {100, 200, 0, 300, 0, 400};
        std::replace(scores.begin(), scores.end(), 0, -1);  // 0을 -1로 교체
        
        // 4. std::replace_if - 조건부 교체
        std::replace_if(scores.begin(), scores.end(),
            [](int score) { return score < 0; }, 0);  // 음수를 0으로
        
        // 5. std::replace_copy - 복사하면서 교체
        std::vector<int> corrected_scores;
        corrected_scores.reserve(scores.size());
        
        std::replace_copy(scores.begin(), scores.end(),
                         std::back_inserter(corrected_scores),
                         -1, 0);  // -1을 0으로 바꿔서 복사
    }
    
    // 게임 서버 특화 변환 예제들
    std::vector<PlayerSummary> CreatePlayerSummaries(const std::vector<Player>& players) {
        std::vector<PlayerSummary> summaries;
        summaries.reserve(players.size());
        
        std::transform(players.begin(), players.end(),
                      std::back_inserter(summaries),
            [](const Player& p) {
                return PlayerSummary{
                    p.GetId(),
                    p.GetName(),
                    p.GetLevel(),
                    p.IsOnline() ? "Online" : "Offline"
                };
            });
        
        return summaries;
    }
    
    void ApplyLevelBonus(std::vector<Player>& players, int bonus) {
        // 모든 플레이어에게 레벨 보너스 적용
        std::for_each(players.begin(), players.end(),
            [bonus](Player& p) {
                // Player 클래스에 SetLevel 메서드가 있다고 가정
                // p.SetLevel(p.GetLevel() + bonus);
            });
    }
    
    std::vector<int> CalculateCombinedStats(const std::vector<int>& base_stats,
                                           const std::vector<int>& bonus_stats) {
        std::vector<int> combined_stats;
        combined_stats.reserve(base_stats.size());
        
        std::transform(base_stats.begin(), base_stats.end(),
                      bonus_stats.begin(),
                      std::back_inserter(combined_stats),
            [](int base, int bonus) {
                return base + bonus;  // 기본 스탯 + 보너스 스탯
            });
        
        return combined_stats;
    }
    
    void NormalizePlayerScores(std::vector<int>& scores) {
        // 0점을 기본값 1로 변경
        std::replace(scores.begin(), scores.end(), 0, 1);
        
        // 음수 점수를 0으로 변경
        std::replace_if(scores.begin(), scores.end(),
            [](int score) { return score < 0; }, 0);
    }

private:
    struct PlayerSummary {
        uint21_t id;
        std::string name;
        int level;
        std::string status;
    };
    
    std::vector<Player> CreateSamplePlayers() {
        return {
            Player(1, "Warrior", 25),
            Player(2, "Mage", 30),
            Player(3, "Archer", 20),
            Player(4, "Priest", 35),
        };
    }
};
```

---

## 📚 3. 정렬 알고리즘 (Sorting Algorithms)

### **3.1 정렬 관련 알고리즘들**

```cpp
// 📊 정렬을 위한 다양한 알고리즘들

class SortingAlgorithmsDemo {
public:
    void DemonstrateSortingAlgorithms() {
        // 샘플 데이터 준비
        std::vector<Player> players = CreateRandomPlayers();
        
        // 1. std::sort - 일반적인 정렬
        auto players_copy1 = players;
        std::sort(players_copy1.begin(), players_copy1.end(),
            [](const Player& a, const Player& b) {
                return a.GetLevel() > b.GetLevel();  // 레벨 내림차순
            });
        
        // 2. std::stable_sort - 안정 정렬 (같은 값의 상대적 순서 보장)
        auto players_copy2 = players;
        std::stable_sort(players_copy2.begin(), players_copy2.end(),
            [](const Player& a, const Player& b) {
                return a.GetLevel() > b.GetLevel();
            });
        
        // 3. std::partial_sort - 부분 정렬 (상위 N개만 정렬)
        auto players_copy3 = players;
        size_t top_count = 3;
        std::partial_sort(players_copy3.begin(), 
                         players_copy3.begin() + top_count,
                         players_copy3.end(),
            [](const Player& a, const Player& b) {
                return a.GetLevel() > b.GetLevel();
            });
        
        std::cout << "Top 3 players by level:" << std::endl;
        for (size_t i = 0; i < top_count; ++i) {
            std::cout << players_copy3[i].GetName() 
                     << " (Level " << players_copy3[i].GetLevel() << ")" << std::endl;
        }
        
        // 4. std::nth_element - N번째 요소 찾기
        auto players_copy4 = players;
        size_t median_index = players_copy4.size() / 2;
        std::nth_element(players_copy4.begin(),
                        players_copy4.begin() + median_index,
                        players_copy4.end(),
            [](const Player& a, const Player& b) {
                return a.GetLevel() < b.GetLevel();
            });
        
        std::cout << "Median level player: " 
                 << players_copy4[median_index].GetName() 
                 << " (Level " << players_copy4[median_index].GetLevel() << ")" << std::endl;
        
        // 5. std::make_heap, std::push_heap, std::pop_heap - 힙 연산
        std::vector<int> scores = {100, 300, 200, 500, 400};
        
        // 힙 생성 (최대 힙)
        std::make_heap(scores.begin(), scores.end());
        
        // 새 원소 추가
        scores.push_back(600);
        std::push_heap(scores.begin(), scores.end());
        
        // 최대값 제거
        std::pop_heap(scores.begin(), scores.end());
        scores.pop_back();
        
        std::cout << "Max score after operations: " << scores.front() << std::endl;
    }
    
    // 게임 서버 특화 정렬 예제들
    std::vector<Player> GetTopPlayersByLevel(std::vector<Player> players, size_t count) {
        if (count >= players.size()) {
            // 모든 플레이어 정렬
            std::sort(players.begin(), players.end(),
                [](const Player& a, const Player& b) {
                    return a.GetLevel() > b.GetLevel();
                });
        } else {
            // 상위 count개만 부분 정렬 (더 효율적)
            std::partial_sort(players.begin(), 
                            players.begin() + count,
                            players.end(),
                [](const Player& a, const Player& b) {
                    return a.GetLevel() > b.GetLevel();
                });
            players.resize(count);
        }
        
        return players;
    }
    
    Player FindMedianLevelPlayer(std::vector<Player> players) {
        size_t median_idx = players.size() / 2;
        
        std::nth_element(players.begin(),
                        players.begin() + median_idx,
                        players.end(),
            [](const Player& a, const Player& b) {
                return a.GetLevel() < b.GetLevel();
            });
        
        return players[median_idx];
    }
    
    void SortPlayersByMultipleCriteria(std::vector<Player>& players) {
        // 복합 정렬: 레벨 내림차순, 레벨이 같으면 이름 오름차순
        std::stable_sort(players.begin(), players.end(),
            [](const Player& a, const Player& b) {
                if (a.GetLevel() != b.GetLevel()) {
                    return a.GetLevel() > b.GetLevel();  // 레벨 내림차순
                }
                return a.GetName() < b.GetName();  // 이름 오름차순
            });
    }

private:
    std::vector<Player> CreateRandomPlayers() {
        return {
            Player(1, "Alice", 25),
            Player(2, "Bob", 35),
            Player(3, "Charlie", 20),
            Player(4, "David", 40),
            Player(5, "Eve", 30),
            Player(6, "Frank", 15),
            Player(7, "Grace", 45),
            Player(8, "Henry", 50)
        };
    }
};
```

---

## 📚 4. 집합 연산 알고리즘 (Set Operations)

### **4.1 집합 연산들**

```cpp
// 📊 집합 연산을 위한 알고리즘들 (정렬된 데이터 필요)

class SetOperationsDemo {
public:
    void DemonstrateSetOperations() {
        // 두 길드의 플레이어 ID 목록 (정렬된 상태)
        std::vector<int> guild1_players = {1, 3, 5, 7, 9, 11, 13};
        std::vector<int> guild2_players = {2, 3, 6, 7, 10, 11, 14};
        
        // 1. std::set_union - 합집합
        std::vector<int> all_players;
        std::set_union(guild1_players.begin(), guild1_players.end(),
                      guild2_players.begin(), guild2_players.end(),
                      std::back_inserter(all_players));
        
        std::cout << "All unique players: ";
        for (int id : all_players) {
            std::cout << id << " ";
        }
        std::cout << std::endl;
        
        // 2. std::set_intersection - 교집합
        std::vector<int> common_players;
        std::set_intersection(guild1_players.begin(), guild1_players.end(),
                             guild2_players.begin(), guild2_players.end(),
                             std::back_inserter(common_players));
        
        std::cout << "Players in both guilds: ";
        for (int id : common_players) {
            std::cout << id << " ";
        }
        std::cout << std::endl;
        
        // 3. std::set_difference - 차집합
        std::vector<int> guild1_only;
        std::set_difference(guild1_players.begin(), guild1_players.end(),
                           guild2_players.begin(), guild2_players.end(),
                           std::back_inserter(guild1_only));
        
        std::cout << "Players only in guild1: ";
        for (int id : guild1_only) {
            std::cout << id << " ";
        }
        std::cout << std::endl;
        
        // 4. std::set_symmetric_difference - 대칭 차집합
        std::vector<int> exclusive_players;
        std::set_symmetric_difference(guild1_players.begin(), guild1_players.end(),
                                     guild2_players.begin(), guild2_players.end(),
                                     std::back_inserter(exclusive_players));
        
        std::cout << "Players in exactly one guild: ";
        for (int id : exclusive_players) {
            std::cout << id << " ";
        }
        std::cout << std::endl;
        
        // 5. std::includes - 부분집합 확인
        std::vector<int> subset = {3, 7, 11};
        bool is_subset = std::includes(guild1_players.begin(), guild1_players.end(),
                                      subset.begin(), subset.end());
        
        std::cout << "Subset check: " << (is_subset ? "Yes" : "No") << std::endl;
    }
    
    // 게임 서버 특화 집합 연산 예제들
    std::vector<uint21_t> GetAllUniquePlayerIds(const std::vector<uint21_t>& online_players,
                                               const std::vector<uint21_t>& offline_players) {
        std::vector<uint21_t> result;
        
        // 두 리스트가 정렬되어 있다고 가정
        std::set_union(online_players.begin(), online_players.end(),
                      offline_players.begin(), offline_players.end(),
                      std::back_inserter(result));
        
        return result;
    }
    
    std::vector<uint21_t> FindPlayersInBothEvents(const std::vector<uint21_t>& event1_participants,
                                                 const std::vector<uint21_t>& event2_participants) {
        std::vector<uint21_t> common_participants;
        
        std::set_intersection(event1_participants.begin(), event1_participants.end(),
                             event2_participants.begin(), event2_participants.end(),
                             std::back_inserter(common_participants));
        
        return common_participants;
    }
    
    bool IsGuildSubsetOfAnother(const std::vector<uint21_t>& small_guild,
                               const std::vector<uint21_t>& large_guild) {
        // 작은 길드의 모든 멤버가 큰 길드에 포함되어 있는지 확인
        return std::includes(large_guild.begin(), large_guild.end(),
                           small_guild.begin(), small_guild.end());
    }
    
    void AnalyzePvPParticipation(const std::vector<uint21_t>& all_players,
                                const std::vector<uint21_t>& pvp_players) {
        // PvP에 참여하지 않은 플레이어들
        std::vector<uint21_t> non_pvp_players;
        std::set_difference(all_players.begin(), all_players.end(),
                           pvp_players.begin(), pvp_players.end(),
                           std::back_inserter(non_pvp_players));
        
        std::cout << "Non-PvP players count: " << non_pvp_players.size() << std::endl;
        std::cout << "PvP participation rate: " 
                  << (100.0 * pvp_players.size() / all_players.size()) << "%" << std::endl;
    }
};
```

---

## 📚 5. 수치 알고리즘 (Numeric Algorithms)

```cpp
#include <numeric>

// 🧮 수치 계산을 위한 알고리즘들

class NumericAlgorithmsDemo {
public:
    void DemonstrateNumericAlgorithms() {
        std::vector<int> player_scores = {100, 200, 150, 300, 250};
        std::vector<int> bonus_multipliers = {2, 3, 1, 4, 2};
        
        // 1. std::accumulate - 집계 연산
        int total_score = std::accumulate(player_scores.begin(), player_scores.end(), 0);
        std::cout << "Total score: " << total_score << std::endl;
        
        // 커스텀 연산으로 최대값 찾기
        int max_score = std::accumulate(player_scores.begin(), player_scores.end(), 
                                       player_scores[0],
            [](int max_val, int current) {
                return std::max(max_val, current);
            });
        std::cout << "Max score: " << max_score << std::endl;
        
        // 2. std::inner_product - 내적 계산
        int weighted_total = std::inner_product(player_scores.begin(), player_scores.end(),
                                               bonus_multipliers.begin(), 0);
        std::cout << "Weighted total: " << weighted_total << std::endl;
        
        // 3. std::adjacent_difference - 인접한 요소들의 차이
        std::vector<int> score_differences;
        std::adjacent_difference(player_scores.begin(), player_scores.end(),
                                std::back_inserter(score_differences));
        
        std::cout << "Score differences: ";
        for (int diff : score_differences) {
            std::cout << diff << " ";
        }
        std::cout << std::endl;
        
        // 4. std::partial_sum - 부분합 계산
        std::vector<int> cumulative_scores;
        std::partial_sum(player_scores.begin(), player_scores.end(),
                        std::back_inserter(cumulative_scores));
        
        std::cout << "Cumulative scores: ";
        for (int sum : cumulative_scores) {
            std::cout << sum << " ";
        }
        std::cout << std::endl;
        
        // 5. std::iota - 순차적 값 생성
        std::vector<int> player_ids(10);
        std::iota(player_ids.begin(), player_ids.end(), 1000);  // 1000부터 시작
        
        std::cout << "Generated player IDs: ";
        for (int id : player_ids) {
            std::cout << id << " ";
        }
        std::cout << std::endl;
    }
    
    // 게임 서버 특화 수치 연산 예제들
    double CalculateAverageLevel(const std::vector<Player>& players) {
        if (players.empty()) return 0.0;
        
        int total_level = std::accumulate(players.begin(), players.end(), 0,
            [](int sum, const Player& p) {
                return sum + p.GetLevel();
            });
        
        return static_cast<double>(total_level) / players.size();
    }
    
    int CalculateTotalDamage(const std::vector<int>& base_damages,
                            const std::vector<float>& multipliers) {
        return std::inner_product(base_damages.begin(), base_damages.end(),
                                 multipliers.begin(), 0.0f,
            [](float sum, float product) { return sum + product; },      // 합계 연산
            [](int damage, float mult) { return damage * mult; });      // 곱셈 연산
    }
    
    std::vector<int> CalculateExperienceProgression(int base_exp, size_t level_count) {
        std::vector<int> level_requirements(level_count);
        
        // 레벨별 필요 경험치를 기하급수적으로 증가
        level_requirements[0] = base_exp;
        std::partial_sum(level_requirements.begin(), level_requirements.end(),
                        level_requirements.begin(),
            [multiplier = 1.2](int prev, int) mutable {
                prev = static_cast<int>(prev * multiplier);
                multiplier += 0.1;  // 배수도 점진적 증가
                return prev;
            });
        
        return level_requirements;
    }
    
    std::vector<int> GenerateUniquePlayerIds(size_t count, int start_id = 1) {
        std::vector<int> ids(count);
        std::iota(ids.begin(), ids.end(), start_id);
        return ids;
    }
    
    PlayerStats CalculateGuildStats(const std::vector<Player>& guild_members) {
        if (guild_members.empty()) {
            return {0, 0.0, 0, 0};
        }
        
        // 총 레벨
        int total_level = std::accumulate(guild_members.begin(), guild_members.end(), 0,
            [](int sum, const Player& p) { return sum + p.GetLevel(); });
        
        // 평균 레벨
        double avg_level = static_cast<double>(total_level) / guild_members.size();
        
        // 최고 레벨
        int max_level = std::accumulate(guild_members.begin(), guild_members.end(), 0,
            [](int max_val, const Player& p) {
                return std::max(max_val, p.GetLevel());
            });
        
        // 온라인 플레이어 수
        int online_count = std::count_if(guild_members.begin(), guild_members.end(),
            [](const Player& p) { return p.IsOnline(); });
        
        return {total_level, avg_level, max_level, online_count};
    }

private:
    struct PlayerStats {
        int total_level;
        double average_level;
        int max_level;
        int online_count;
    };
};
```

---

## 📝 다음 단계 예고

이 문서에서는 STL 알고리즘의 첫 번째 부분을 다뤘습니다:

✅ **완료된 내용**
- 비변경 알고리즘 (검색, 비교)
- 변경 알고리즘 (복사, 이동, 변환)
- 정렬 알고리즘
- 집합 연산 알고리즘
- 수치 알고리즘

🔄 **다음 문서에서 계속**
- C++17/20 병렬 알고리즘
- 범위 기반 알고리즘 (C++20 Ranges)
- 성능 최적화 기법
- 실전 프로젝트 예제

**다음 문서**: `20_stl_algorithms_advanced.md`에서 고급 STL 알고리즘과 최적화 기법을 계속 학습합니다.