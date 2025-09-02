# PvP System Test Guide

## Overview

This directory contains comprehensive test scenarios for both Arena and Open World PvP systems. The tests verify functionality, integration, and performance characteristics.

## Test Structure

### Arena Test Scenarios (`arena_test_scenarios.cpp`)
Tests arena-specific functionality:
- **Matchmaking**: Rating-based queue system
- **Rating Spread**: Dynamic expansion over time
- **Team Formation**: Balanced team creation
- **Victory Conditions**: Elimination-based wins
- **Rating Changes**: ELO calculation verification
- **Queue Management**: Join/leave functionality
- **Concurrent Matches**: Multiple simultaneous arenas

### Open World Test Scenarios (`openworld_test_scenarios.cpp`)
Tests open world PvP mechanics:
- **Zone Boundaries**: AABB detection and flagging
- **Faction Hostility**: Attack permission rules
- **Zone Capture**: Progress-based territory control
- **Territory Buffs**: Stat bonuses for controlling faction
- **Honor System**: Kill rewards and diminishing returns
- **Objective Capture**: Point-based objectives
- **PvP Flag Timeout**: 5-minute cooldown mechanics
- **Contested Capture**: Multi-faction capture conflicts

### Integration Tests (`pvp_integration_test.cpp`)
Tests both systems working together:
- **Shared Stats**: Common PvPStatsComponent
- **Combined Honor**: Accumulation from both sources
- **State Transitions**: Queue while flagged scenarios
- **Kill Tracking**: Unified statistics
- **Rating Impact**: Cross-system effects
- **Performance**: Scalability with 100+ players
- **System Priorities**: Update order verification

## Running Tests

### Quick Run
```bash
./run_pvp_tests.sh
```

### Build and Run
```bash
./run_pvp_tests.sh --build
```

### Memory Check
```bash
./run_pvp_tests.sh --memcheck
```

### Individual Tests
```bash
# Run specific test suite
../../build/tests/pvp/arena_test_scenarios

# Run specific test case
../../build/tests/pvp/arena_test_scenarios --gtest_filter=ArenaTestScenarios.Arena1v1Matchmaking
```

## Test Coverage

### Arena System Coverage
- Queue management: ✓
- Matchmaking algorithm: ✓
- Match lifecycle: ✓
- Rating calculations: ✓
- Reward distribution: ✓
- Player state management: ✓

### Open World Coverage
- Zone detection: ✓
- Faction mechanics: ✓
- Territory control: ✓
- Honor calculations: ✓
- Objective system: ✓
- Anti-griefing: ✓

### Missing Coverage
- Network synchronization (requires network tests)
- Visual feedback (requires rendering tests)
- Persistence (requires database tests)
- Cross-server features (future enhancement)

## Performance Benchmarks

Expected performance characteristics:
- Arena matchmaking: <50ms for 100 players
- Zone updates: <10ms for 100 players
- Memory usage: ~11KB per player (1KB open world + 10KB if in match)
- Update frequency: 30Hz arena, 1Hz zone checks

## Common Test Patterns

### Creating Test Players
```cpp
auto player = CreateTestPlayer(faction_id, position);
// or
auto player = CreateFullPlayer(faction, rating);
```

### Simulating Time
```cpp
// Update systems
system->Update(delta_time);

// Multiple updates
for (int i = 0; i < seconds; i++) {
    system->Update(1.0f);
}
```

### Checking Results
```cpp
EXPECT_TRUE(condition);
EXPECT_EQ(actual, expected);
EXPECT_GT(value1, value2);
EXPECT_NEAR(float1, float2, tolerance);
```

## Debugging Failed Tests

1. **Check Prerequisites**: Ensure spatial system is initialized
2. **Verify Components**: All required components must be present
3. **Time Dependencies**: Some tests require specific update intervals
4. **State Order**: Operations must happen in correct sequence

## Adding New Tests

1. Choose appropriate test file based on system
2. Follow existing test patterns
3. Use descriptive test names
4. Add [SEQUENCE: n] comments
5. Update this guide with new coverage

## Integration with CI/CD

These tests can be integrated into continuous integration:
```yaml
test:
  stage: test
  script:
    - cd build
    - ctest -L pvp --output-on-failure
```