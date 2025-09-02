# Phase 100: A/B Test Framework

## Overview
This phase implements a comprehensive A/B testing framework for the MMORPG server, enabling data-driven decisions through controlled experiments. The system supports variant allocation, player targeting, real-time metrics tracking, and statistical analysis.

## Key Components

### 1. Experiment Configuration (SEQUENCE: 2577-2582)
- **Variant definitions**: Multiple test groups with allocation percentages
- **Targeting criteria**: Level, region, platform, and custom features
- **Scheduling**: Time-based experiment activation
- **Success metrics**: Primary and secondary KPIs

### 2. Player Assignment System (SEQUENCE: 2583-2595)
- **Consistent hashing**: Deterministic variant assignment
- **Profile matching**: Check targeting criteria
- **Parameter application**: Apply variant-specific settings
- **Persistence**: Store assignments for consistency

### 3. Metrics Tracking (SEQUENCE: 2578, 2588-2592)
- **Basic metrics**: Player count, sessions, playtime, revenue
- **Conversion tracking**: Purchase events and goals
- **Custom events**: Flexible event tracking system
- **Real-time updates**: Atomic operations for accuracy

### 4. Statistical Analysis (SEQUENCE: 2597-2599)
- **Significance testing**: Z-score and p-value calculations
- **Lift calculation**: Percentage improvement measurement
- **Confidence intervals**: Statistical confidence levels
- **Sample size estimation**: Required participants calculation

### 5. Monitoring & Safety (SEQUENCE: 2612-2619)
- **Sample Ratio Mismatch (SRM)**: Chi-square test for allocation
- **Metric anomaly detection**: Identify harmful changes
- **Auto-stop**: Halt experiments showing negative impact
- **Health monitoring**: Continuous experiment validation

### 6. Integration Features (SEQUENCE: 2605-2611)
- **Login hooks**: Assign experiments on player login
- **Session tracking**: Monitor engagement metrics
- **Event integration**: Track game events for analysis
- **Parameter injection**: Apply test configurations

## Technical Implementation

### Experiment Definition Format
```json
{
  "id": "xp_progression_test",
  "name": "XP Progression Rate Test",
  "variants": [
    {
      "name": "control",
      "allocation": 50.0,
      "parameters": {"xp_multiplier": 1.0}
    },
    {
      "name": "treatment",
      "allocation": 50.0,
      "parameters": {"xp_multiplier": 1.25}
    }
  ],
  "targeting": {
    "min_level": 1,
    "max_level": 20,
    "region": "NA"
  },
  "schedule": {
    "start_time": "2024-01-20 00:00:00",
    "end_time": "2024-02-03 23:59:59"
  }
}
```

### Assignment Algorithm
- Uses consistent hashing: `hash(player_id + experiment_id) % 10000 / 100.0`
- Ensures same player always gets same variant
- Supports gradual rollout by adjusting allocations

### Statistical Methods
- **Proportion test**: For conversion rate comparisons
- **Z-score**: `(p2 - p1) / sqrt(p_pooled * (1-p_pooled) * (1/n1 + 1/n2))`
- **P-value**: Two-tailed test using normal distribution
- **Lift**: `(treatment - control) / control * 100`

## Usage Examples

### Basic Setup
```cpp
// Initialize service
ABTestingService ab_service;
ab_service.LoadExperiments("config/experiments.json");

// Player login
PlayerProfile profile{
    .player_id = 12345,
    .level = 25,
    .region = "NA",
    .platform = "PC"
};

auto assignments = ab_service.GetPlayerAssignments(profile);
```

### Event Tracking
```cpp
// Track conversion event
ab_service.TrackEvent(player_id, "store_ui_test", "purchase", 9.99);

// Track engagement
ab_service.TrackEvent(player_id, "guild_feature", "guild_created");

// Session metrics
ab_service.UpdateSessionMetrics(player_id, 
    session_duration_seconds, 
    session_revenue);
```

### Results Analysis
```cpp
auto results = ab_service.GetExperimentResults("xp_progression_test");

// Check statistical significance
if (results["statistical_analysis"]["is_significant"].asBool()) {
    double lift = results["statistical_analysis"]["revenue_lift_percentage"].asDouble();
    
    if (lift > 0) {
        // Ship the feature
        spdlog::info("Experiment shows +{}% revenue lift", lift);
    }
}
```

## Common Experiment Types

### 1. Feature Flags
```cpp
auto config = ABTestConfigManager::CreateFeatureFlagExperiment(
    "new_combat_system",
    10.0  // 10% rollout
);
```

### 2. Parameter Tuning
```cpp
auto config = ABTestConfigManager::CreateParameterTuningExperiment(
    "drop_rate",
    {1.0, 1.1, 1.2, 1.3}  // Test different multipliers
);
```

### 3. UI Variations
```json
{
  "id": "store_layout_test",
  "variants": [
    {"name": "grid", "allocation": 33.33},
    {"name": "list", "allocation": 33.33},
    {"name": "featured", "allocation": 33.34}
  ]
}
```

## Monitoring Dashboard

### Key Metrics
- **Conversion rate**: Percentage of players who convert
- **ARPU**: Average revenue per user by variant
- **Engagement**: Session length and frequency
- **Statistical power**: Current significance level

### Alerts
- Sample ratio mismatch > 5%
- Revenue drop > 10%
- Conversion drop > 20%
- Error rate increase > 2x

## Best Practices

1. **Pre-experiment**:
   - Define clear success metrics
   - Calculate required sample size
   - Set experiment duration
   - Document hypothesis

2. **During experiment**:
   - Monitor for SRM daily
   - Check metric anomalies
   - Don't peek at results early
   - Let experiments run full duration

3. **Post-experiment**:
   - Analyze all metrics, not just primary
   - Consider long-term effects
   - Document learnings
   - Clean up experiment code

## Performance Considerations

- Assignment lookup: O(1) with caching
- Metric updates: Lock-free atomic operations
- Statistical calculations: Computed on-demand
- Memory usage: ~1KB per active experiment

## Security & Privacy

- No PII in experiment logs
- Hashed player IDs for assignments
- Aggregate metrics only
- Configurable data retention

## Future Enhancements

1. **Multi-armed bandits**: Dynamic allocation adjustment
2. **Segmentation analysis**: Results by player segments
3. **Sequential testing**: Early stopping for clear winners
4. **Interaction effects**: Test multiple features together
5. **Bayesian analysis**: Alternative statistical approach

## Integration with Game Systems

The A/B testing framework integrates with:
- Login system for assignment
- Event system for tracking
- Configuration system for parameters
- Monitoring system for alerts
- Analytics pipeline for reporting

This completes MVP14 (Monitoring & Analytics) with all 7 phases implemented.