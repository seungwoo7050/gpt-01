# Phase 120: NPC Dialogue System

## Overview
Implemented comprehensive dialogue system with conversation trees, choice requirements, dynamic conditions, and integrated quest/trade/reputation management for rich NPC interactions.

## Key Features

### 1. Dialogue Node Types
Five distinct node types for flexible conversations:
- **TEXT**: Simple text display with speaker
- **CHOICE**: Multiple choice options with requirements
- **CONDITION**: Conditional branching based on game state
- **ACTION**: Execute game actions during dialogue
- **END**: Dialogue termination

### 2. Choice System
Comprehensive choice requirements and effects:

#### Requirements
- Minimum player level
- Reputation thresholds
- Item possession
- Quest completion
- Dialogue flags

#### Effects
- Reputation changes
- Item exchanges (give/take)
- Quest triggers (start/complete)
- Flag setting
- Game state modifications

### 3. Dialogue State Management

#### State Tracking
- Current node position
- Conversation history
- Choice selections
- Dialogue flags
- Time tracking

#### Session Management
- Active dialogue tracking
- Automatic cleanup
- State persistence
- Concurrent dialogue prevention

### 4. Dialogue Builder
Fluent API for creating dialogue trees:
```cpp
DialogueBuilder builder("merchant_dialogue");
builder.Name("Merchant Dialogue")
    .Text("start", "Merchant", "Welcome!", "menu")
    .Choice("menu", "Merchant", "How can I help?")
        .AddOption(1, "Show wares", "shop")
        .RequireLevel(1, 10)
        .AddOption(2, "Goodbye", "end")
    .Action("shop", "open_shop", "menu")
    .End("end", "Come again!");
```

### 5. Common Dialogue Patterns

#### Pre-built Templates
- Merchant dialogues
- Quest giver conversations
- Guard interactions
- Innkeeper services
- Skill trainers

#### Customization Support
- Variable substitution
- Localization ready
- Dynamic content
- Context-aware responses

### 6. Global Conditions & Actions

#### Condition System
```cpp
RegisterGlobalCondition("has_gold", [](const Player& p, const NPC& n) {
    return p.GetGold() >= 100;
});
```

#### Action System
```cpp
RegisterGlobalAction("give_reward", [](Player& p, NPC& n) {
    p.GiveItem(REWARD_ITEM, 1);
    p.AddExperience(500);
});
```

## Technical Implementation

### Dialogue Flow
```cpp
// 1. Start dialogue
auto state = StartDialogue(player, npc, "merchant_tree");

// 2. Get current node content
auto response = ContinueDialogue(player_id);
// Returns: text, speaker, available_choices

// 3. Player makes choice
auto result = MakeChoice(player_id, choice_id);
// Applies effects, advances dialogue

// 4. Continue or end
if (response.is_end) {
    EndDialogue(player_id);
}
```

### Choice Filtering
```cpp
bool CheckChoiceRequirements(choice, player) {
    // Check level
    if (player.GetLevel() < choice.min_level) return false;
    
    // Check items
    for (auto item_id : choice.required_items) {
        if (!player.HasItem(item_id)) return false;
    }
    
    // Check quests
    for (auto quest_id : choice.required_quests) {
        if (!player.HasCompletedQuest(quest_id)) return false;
    }
    
    return true;
}
```

### Memory Management
- Shared pointers for tree nodes
- Automatic cleanup on dialogue end
- Lazy loading for large trees
- Cache frequently used dialogues

## Performance Optimizations

1. **Tree Caching**
   - LRU cache for dialogue trees
   - Preload common dialogues
   - Memory limits

2. **Condition Evaluation**
   - Result caching
   - Early termination
   - Batch evaluation

3. **Choice Filtering**
   - Pre-filter impossible choices
   - Cache requirement checks
   - Optimize check order

## Integration Examples

### Quest Integration
```cpp
builder.Choice("quest_offer", "NPC", "I have a task for you")
    .AddOption(1, "I'll help", "accept")
    .StartQuest(1, QUEST_ID)
    .AddOption(2, "Not now", "decline");
```

### Shop Integration
```cpp
builder.Action("open_shop", "show_merchant_inventory")
    .SetFlag("visited_shop");
```

### Reputation Integration
```cpp
builder.AddOption(1, "Threaten", "threaten")
    .ChangeReputation(1, -50)
    .RequireLevel(1, 20);
```

## Database Schema
```sql
-- Dialogue trees stored as JSON
CREATE TABLE npc_dialogue_trees (
    tree_id VARCHAR(64) PRIMARY KEY,
    tree_name VARCHAR(128),
    tree_data JSON,
    created_date TIMESTAMP,
    last_modified TIMESTAMP
);

-- Track dialogue history
CREATE TABLE dialogue_history (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    player_id BIGINT,
    npc_id BIGINT,
    tree_id VARCHAR(64),
    choices_made JSON,
    duration_seconds INT,
    completed BOOLEAN,
    timestamp TIMESTAMP
);
```

## Files Created
- src/npc/npc_dialogue.h
- src/npc/npc_dialogue.cpp

## Future Enhancements
1. Voice-acted dialogue support
2. Branching dialogue achievements
3. Dynamic dialogue generation
4. Emotion/mood system
5. Multi-NPC conversations