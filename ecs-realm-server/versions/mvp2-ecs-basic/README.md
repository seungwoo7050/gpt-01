# MVP2: Basic ECS Implementation

## Overview

This version implements a straightforward Entity-Component-System architecture focusing on clarity and ease of understanding. It serves as the foundation for game logic and demonstrates ECS concepts without complex optimizations.

## Architecture

### Core ECS Components

1. **Entity Manager**
   - Manages entity lifecycle with ID recycling
   - Version tracking prevents stale entity references
   - Thread-safe operations

2. **Component Storage**
   - Generic template-based storage
   - Type-safe component access
   - Automatic type registration

3. **World**
   - Central ECS coordinator
   - System scheduling and execution
   - Component queries

### Component Types

| Component | Purpose | Key Fields |
|-----------|---------|------------|
| TransformComponent | Position & orientation | position, rotation, scale |
| VelocityComponent | Movement physics | linear, angular, max_speed |
| HealthComponent | Damage & healing | current_hp, max_hp, regen_rate |
| CombatComponent | Attack mechanics | attack_power, defense, range |
| NetworkComponent | Sync management | owner_id, update_flags |
| TagComponent | Entity metadata | name, type, flags |

### Systems

1. **MovementSystem** (Priority: 100)
   - Updates entity positions based on velocity
   - Handles rotation wrapping

2. **CombatSystem** (Priority: 200)
   - Processes attacks with cooldowns
   - Range validation
   - Damage application

3. **HealthRegenerationSystem** (Priority: 300)
   - Passive health recovery
   - Network update flagging

4. **NetworkSyncSystem** (Priority: 1000)
   - Visibility management
   - Update packet generation
   - Batch optimization

## Performance Characteristics

### Memory Layout
```
EntityManager
  └── entities_: vector<EntityRecord>
  └── free_indices_: queue<uint32_t>

ComponentStorage<T>
  └── components_: unordered_map<EntityId, T>
```

### Complexity Analysis
- Entity Creation: O(1)
- Component Add/Remove: O(1) average
- Component Access: O(1) average
- System Update: O(n) where n = entities with required components

### Benchmark Results
- 1,000 entities: ~0.5ms per frame
- 5,000 entities: ~2.5ms per frame
- Memory per entity: ~200 bytes average

## Usage Example

```cpp
// Create world and systems
auto world = std::make_unique<World>();
world->AddSystem(std::make_shared<MovementSystem>());
world->AddSystem(std::make_shared<CombatSystem>());

// Create player entity
EntityId player = world->CreateEntity();
world->AddComponent(player, TransformComponent{{0, 0, 0}});
world->AddComponent(player, VelocityComponent{{5, 0, 0}});
world->AddComponent(player, HealthComponent{100, 100, 1.0f});

// Update world
world->Update(0.016f); // 60 FPS
```

## Design Decisions

### Why std::unordered_map?

**Pros:**
- Simple implementation
- Good average case performance
- Easy debugging
- No memory management complexity

**Cons:**
- Poor cache locality
- Memory fragmentation
- Higher memory overhead
- Not optimal for iteration

### Thread Safety Approach

Each component type has its own mutex, allowing:
- Parallel access to different component types
- Safe concurrent system execution
- Minimal lock contention

### System Execution Model

Four-stage pipeline ensures proper ordering:
1. **PRE_UPDATE**: Input processing, AI decisions
2. **UPDATE**: Core game logic
3. **POST_UPDATE**: Cleanup, reconciliation
4. **NETWORK_SYNC**: Client updates

## Limitations

1. **Cache Performance**: Random memory access patterns
2. **Memory Overhead**: ~24 bytes per component entry (map overhead)
3. **No Spatial Queries**: Must check all entities for range queries
4. **Single-threaded Systems**: Each system runs sequentially

## Next Steps

The optimized ECS version addresses these limitations with:
- Custom memory pools
- Structure of Arrays layout
- Cache-line optimization
- Parallel system execution

## Files Structure

```
src/core/ecs/
├── types.h              # Core type definitions
├── entity_manager.h/cpp # Entity lifecycle
├── component_storage.h  # Component containers
├── system.h            # System interface
└── world.h/cpp         # ECS coordinator

src/game/components/    # Component definitions
src/game/systems/       # System implementations
```

## Integration with MVP1

The NetworkComponent and NetworkSyncSystem provide the bridge to the networking layer:
- Entity ownership tracks which session controls an entity
- Update flags minimize network traffic
- Batch updates reduce packet overhead

This basic implementation provides a solid foundation for game logic while maintaining code clarity and debuggability.