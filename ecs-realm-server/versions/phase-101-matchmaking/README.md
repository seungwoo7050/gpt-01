# Phase 101: Matchmaking Service

## Overview
This phase implements a comprehensive matchmaking service for competitive game modes in the MMORPG server. The system supports various match types (1v1 to 20v20), ELO-based rating, intelligent team balancing, and real-time queue management.

## Key Components

### 1. Match Types & Player Profiles (SEQUENCE: 2630-2634)
- **Match types**: Arena (1v1, 2v2, 3v3, 5v5), Battlegrounds (10v10, 20v20), Ranked modes
- **Player profiles**: Individual ratings per match type
- **Rating system**: ELO with rating deviation tracking
- **Preferences**: Max ping, preferred region, blocked players

### 2. Queue Management (SEQUENCE: 2635-2639)
- **Dynamic expansion**: Rating range grows over time (50 points/second)
- **Compatibility checks**: Blocked players, recent opponents
- **Priority system**: Longer wait = higher priority
- **Queue status**: Real-time statistics and estimates

### 3. Team Balancing (SEQUENCE: 2640-2643)
- **Snake draft**: 1-2-2-1 pattern for fair distribution
- **Quality metrics**: Rating balance, wait time, ping scores
- **Premade support**: Groups matched fairly
- **Role composition**: Tank/Healer/DPS balance

### 4. Service Architecture (SEQUENCE: 2644-2655)
- **Worker thread**: Continuous matching every second
- **Event-driven**: Callbacks for match creation/timeout
- **Thread-safe**: Mutex-protected queue operations
- **Scalable design**: Multiple queues processed concurrently

### 5. Rating System (SEQUENCE: 2665-2670)
- **ELO formula**: Standard 400-point scale
- **K-factors**: 16-32 based on match type
- **Team ratings**: Average-based calculations
- **Performance bonus**: MVP gets 20% extra points

## Technical Implementation

### Queue Entry Structure
```cpp
struct QueueEntry {
    std::shared_ptr<MatchmakingProfile> player;
    MatchType match_type;
    std::chrono::steady_clock::time_point queue_time;
    
    // Dynamic rating range
    int32_t GetAcceptableRatingRange(requirements) {
        auto seconds = duration_since_queue_time;
        return min(initial_range + seconds * expansion_rate, max_range);
    }
};
```

### Match Quality Formula
```cpp
quality = rating_balance * 0.5 +    // Most important
          wait_time_score * 0.3 +   // Player satisfaction
          ping_score * 0.2;         // Network quality

// Minimum quality threshold: 0.3
```

### Team Balancing Algorithm
1. Sort players by rating (descending)
2. Use snake draft pattern:
   - Team A gets: 1st, 4th, 5th, 8th...
   - Team B gets: 2nd, 3rd, 6th, 7th...
3. Ensures balanced average ratings

### Rating Update
```cpp
// Expected win probability
E = 1 / (1 + 10^((opponent_rating - my_rating) / 400))

// Rating change
ΔR = K * (actual_score - expected_score)

// K-factors:
- Ranked: 32
- Arena: 24  
- Battleground: 16
```

## Integration Examples

### Server Integration
```cpp
// Match creation callback
matchmaking->OnMatchCreated = [server](const MatchGroup& match) {
    // Create instance
    auto instance = server->CreateInstance(SelectMap(match.match_type));
    
    // Reserve slots for players
    for (const auto& team : match.teams) {
        for (uint64_t player_id : team.player_ids) {
            instance->ReserveSlot(player_id, team.team_id);
        }
    }
    
    // Start countdown
    server->ScheduleTask(30s, [=]() { instance->StartMatch(); });
};
```

### Queue Operations
```cpp
// Add player to queue
auto profile = std::make_shared<MatchmakingProfile>();
profile->player_id = 12345;
profile->ratings[ARENA_3V3].current_rating = 1650;
matchmaking.AddToQueue(profile, ARENA_3V3);

// Check status
auto status = matchmaking.GetQueueStatus(ARENA_3V3);
// Returns: players_in_queue, avg_wait_time, estimated_wait

// Remove from queue
matchmaking.RemoveFromQueue(player_id);
```

## Advanced Features

### 1. Role-Based Composition
```cpp
struct RoleRequirements {
    int tanks = 1;
    int healers = 1;  
    int damage_dealers = 3;
};
```

### 2. Premade Group Handling
- Groups matched against similar-sized groups
- Rating averaged for group members
- Special balancing for mixed solo/group

### 3. Analytics & Monitoring
- Queue abandonment tracking
- Match quality trends by hour
- Automatic recommendations
- Performance metrics

### 4. Fairness Prediction
- Considers rating balance
- Premade vs solo penalties
- Win/loss streak adjustments
- Historical performance

## Performance Characteristics

- **Queue operations**: O(1) add/remove with hash map
- **Match creation**: O(n log n) for sorting players
- **Memory usage**: ~500 bytes per queued player
- **CPU usage**: Minimal (1 thread, 1s intervals)

## Configuration

### Match Requirements
```cpp
match_requirements_[ARENA_3V3] = {
    .min_players = 6,
    .players_per_team = 3,
    .rating_constraints = {
        .initial_range = 150,
        .max_range = 500,
        .expansion_rate = 40.0
    },
    .team_balance = {
        .balance_by_rating = true,
        .max_team_rating_diff = 100
    },
    .max_queue_time = 600s
};
```

## Testing Scenarios

1. **Basic matching**: 6 players, similar ratings
2. **Rating expansion**: Test growing ranges over time
3. **Team balance**: Verify fair team creation
4. **Timeout handling**: Players removed after max time
5. **Concurrent queues**: Multiple match types simultaneously

## Monitoring & Alerts

- Queue times exceeding thresholds
- Match quality dropping below 0.5
- High abandonment rates (>20%)
- System performance degradation

## Future Enhancements

1. **Machine learning**: Predict optimal match times
2. **Dynamic requirements**: Adjust based on population
3. **Cross-region matching**: For low population times
4. **Tournament integration**: Special tournament queues
5. **Spectator support**: Allow match spectating