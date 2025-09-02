# Phase 121: Dynamic Quest Generation

## Overview
Implemented comprehensive dynamic quest generation system that creates procedural quests based on player actions, world events, and game state, providing endless content variety for players.

## Key Features

### 1. Quest Template System
Ten distinct quest types with flexible templates:
- **KILL**: Monster elimination quests
- **COLLECT**: Resource gathering quests
- **DELIVERY**: Package delivery missions
- **ESCORT**: NPC protection quests
- **EXPLORATION**: Zone discovery quests
- **CRAFT**: Item creation quests
- **INTERACTION**: Social/dialogue quests
- **SURVIVAL**: Time-based survival challenges
- **PUZZLE**: Problem-solving quests
- **COMPETITION**: PvP or race quests

### 2. Context-Aware Generation
Quest generation considers multiple factors:

#### Player Context
- Current level and position
- Completed quest history
- Active quest load
- Reputation standings
- Play preferences

#### World Context
- Time of day effects
- Current zone properties
- Nearby NPCs and monsters
- Active world events
- Environmental hazards

### 3. Dynamic Objective System

#### Objective Templates
```cpp
struct ObjectiveTemplate {
    vector<uint32_t> possible_targets;
    uint32_t min_count{1};
    uint32_t max_count{10};
    float difficulty_modifier{1.0f};
    uint32_t time_limit{0};
};
```

#### Target Selection
- Proximity-based filtering
- Level-appropriate targets
- Zone-specific entities
- Event-influenced selection

### 4. Intelligent Reward Scaling

#### Base Rewards
- Experience points
- Gold currency
- Reputation gains
- Skill unlocks

#### Scaling Factors
- Level-based scaling (1.1x per level)
- Difficulty scaling (1.2x per tier)
- Time bonus scaling (1.5x for speed)
- World event bonuses

#### Item Rewards
- Probabilistic item drops
- Level-gated rewards
- Rarity tiers
- Set item chances

### 5. Quest Chain System

#### Chain Management
```cpp
struct QuestChain {
    string chain_id;
    vector<string> template_ids;
    uint32_t current_index;
    unordered_map<string, any> chain_data;
};
```

#### Chain Features
- Sequential quest progression
- Shared data between quests
- Branching paths
- Cumulative rewards

### 6. World Event Integration

#### Event Types
- Monster invasions
- Seasonal festivals
- Resource shortages
- Territory conflicts
- Weather disasters

#### Event Effects
- Quest urgency levels
- Bonus objectives
- Enhanced rewards
- Time-limited availability

## Technical Implementation

### Generation Flow
```cpp
// 1. Collect context
auto params = BuildGenerationParams(player);

// 2. Select template
auto template = SelectTemplate(params);

// 3. Generate objectives
auto objectives = GenerateObjectives(template, params);

// 4. Calculate rewards
auto rewards = CalculateRewards(template, params, difficulty);

// 5. Create quest instance
auto quest = make_shared<GeneratedQuest>(id, template_id);
```

### Template Selection Algorithm
```cpp
float CalculateTemplateWeight(template, params) {
    float weight = 1.0f;
    
    // Player preference bonus
    if (matches_preference) weight *= 2.0f;
    
    // World event influence
    for (auto& [event, intensity] : world_events) {
        if (template_relates_to(event)) {
            weight *= (1.0f + intensity);
        }
    }
    
    // Cooldown penalty
    if (recently_used) weight *= 0.5f;
    
    return weight;
}
```

### Difficulty Calculation
```cpp
float CalculateDifficulty(params) {
    float base = 1.0f;
    
    // Player progression
    base *= (1.0f + player_level * 0.01f);
    
    // World event intensity
    base *= (1.0f + max_event_intensity * 0.5f);
    
    // Zone danger level
    base *= zone_difficulty_modifier;
    
    return clamp(base, 0.5f, 3.0f);
}
```

## Performance Optimizations

1. **Template Caching**
   - Frequently used templates in memory
   - Type-based indexing
   - Quick validation checks

2. **Asynchronous Generation**
   - Background quest creation
   - Pre-generated quest pools
   - Just-in-time assignment

3. **Spatial Optimization**
   - Zone-based target caching
   - Proximity searches
   - Entity clustering

## Usage Examples

### Daily Quest Generation
```cpp
auto& manager = DynamicQuestManager::Instance();
auto daily_quests = manager.GenerateDailyQuests(player, 5);

// Ensures variety
// - No duplicate templates
// - Mixed quest types
// - Appropriate difficulty
```

### Event Quest Creation
```cpp
// World event triggers special quests
manager.OnWorldEvent("dragon_invasion", 0.9f);
auto event_quest = manager.GenerateEventQuest(
    "dragon_invasion", params);

// Features:
// - Urgent priority
// - Enhanced rewards
// - Time limits
// - Special objectives
```

### Quest Chain Progress
```cpp
// Start story chain
manager.StartQuestChain(player, "hero_journey");

// On quest completion
manager.ProgressQuestChain(player, completed_quest_id);
// Automatically generates next quest in chain
// Carries over chain data and decisions
```

## Database Schema
```sql
-- Quest templates
CREATE TABLE quest_templates (
    template_id VARCHAR(64) PRIMARY KEY,
    template_type VARCHAR(32),
    template_data JSON,
    min_level INT,
    max_level INT,
    generation_weight FLOAT,
    cooldown_hours INT
);

-- Generated quests
CREATE TABLE generated_quests (
    quest_id INT PRIMARY KEY,
    template_id VARCHAR(64),
    player_id BIGINT,
    generation_seed BIGINT,
    generation_params JSON,
    objectives JSON,
    rewards JSON,
    generated_at TIMESTAMP,
    expires_at TIMESTAMP
);
```

## Files Created
- src/quest/dynamic_quest.h
- src/quest/dynamic_quest.cpp

## Future Enhancements
1. Machine learning for preference detection
2. Collaborative quest generation
3. Player-created quest templates
4. Cross-server event quests
5. Procedural story generation