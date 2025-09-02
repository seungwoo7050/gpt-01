# MVP4: Action Combat System

## Overview

This version implements a modern action-oriented combat system with free-aim abilities, skillshots, dodge mechanics, and physics-based projectiles. Combat is fast-paced and position-dependent, similar to games like TERA, Black Desert Online, or New World.

## Features Implemented

### 1. Skillshot System
- **Projectile Physics**: Velocity-based movement
- **Collision Detection**: Radius-based hit detection
- **Piercing Support**: Projectiles can hit multiple targets
- **Range Tracking**: Auto-destroy at max range

### 2. Combat Mechanics
- **Free Aim**: No target locking required
- **Melee Swings**: Cone-based area attacks
- **Ground AoE**: Click-to-place abilities
- **Dodge Roll**: Invulnerability frames
- **Hit Recording**: Prevents multi-hits

### 3. Dodge System
- **Dodge Charges**: 2 charges with recharge
- **I-Frames**: 0.5s invulnerability
- **Distance**: 10 unit roll
- **Cooldown**: 0.5s between dodges
- **Recharge**: 1 charge per 5 seconds

## Combat Components

### ProjectileComponent (Internal)
```cpp
struct ProjectileComponent {
    EntityId owner;
    Vector3 velocity;
    float speed = 20.0f;
    float range;
    float damage;
    float radius;      // Collision size
    bool piercing;
    uint32_t skill_id;
};
```

### DodgeComponent (Internal)
```cpp
struct DodgeComponent {
    bool is_dodging;
    int dodge_charges = 2;
    Vector3 dodge_direction;
    time_point dodge_end_time;
};
```

## Combat Flow

### 1. Skillshot Usage
```cpp
// Fire projectile in direction
Vector3 aim_direction = GetAimDirection();
combat_system->UseSkillshot(player_id, ARROW_SHOT_ID, aim_direction);

// Creates projectile entity that:
1. Moves at 20 units/second
2. Checks collisions each frame
3. Applies damage on hit
4. Destroys at max range
```

### 2. Melee Combat
```cpp
// Swing in direction
combat_system->UseMeleeSwing(player_id, face_direction, 90.0f);

// Hits all enemies in:
- 90 degree cone
- 5 unit range
- Instant damage
- Can hit multiple targets
```

### 3. Dodge Mechanics
```cpp
// Dodge roll
combat_system->DodgeRoll(player_id, roll_direction);

// During dodge:
- Invulnerable for 0.5s
- Move 10 units instantly
- Consume 1 dodge charge
- Cannot be targeted
```

## Projectile System

### Update Loop (60Hz)
```
1. Move projectile by velocity * delta_time
2. Check entities within radius + padding
3. For each potential hit:
   - Skip if already hit
   - Skip if owner
   - Check exact collision
   - Apply damage
   - Record hit
4. Destroy if hit (unless piercing)
5. Destroy if max range reached
```

### Collision Detection
```cpp
// Sphere-based collision
for (entity in GetEntitiesInRadius(projectile_pos, radius)) {
    if (!already_hit && IsValidTarget(entity)) {
        ApplyDamage(entity, damage);
        record_hit(entity);
    }
}
```

## Area Attacks

### Cone Detection
```cpp
// For melee swings
std::vector<EntityId> GetEntitiesInCone(
    Vector3 origin,      // Attacker position
    Vector3 direction,   // Facing direction  
    float range,         // Max distance
    float angle          // Cone angle in degrees
);
```

### Ground AoE
```cpp
// Click to place
bool UseAreaSkill(
    EntityId caster,
    uint32_t skill_id,
    Vector3 target_position  // Where clicked
);

// Instant damage in radius
// No projectile needed
// Range limited from caster
```

## Performance Characteristics

| Operation | Time | Notes |
|-----------|------|-------|
| Projectile Update | <10μs | Per projectile |
| Collision Check | <50μs | Spatial query |
| Cone Calculation | <30μs | Math heavy |
| Dodge State | <5μs | Simple flag |
| Hit Recording | <10μs | Hash lookup |

### Optimization Strategies
1. **Spatial Queries**: Use grid/octree for collision
2. **Early Exit**: Skip out-of-range projectiles
3. **Batch Updates**: Process similar projectiles together
4. **Hit Caching**: Prevent redundant calculations

## Usage Examples

### Archer Gameplay
```cpp
// Rapid fire arrows
for (int i = 0; i < 3; i++) {
    Vector3 spread = AddSpread(aim_dir, 5.0f);
    UseSkillshot(archer, MULTI_SHOT, spread);
}

// Dodge incoming attack
if (DetectIncomingProjectile()) {
    DodgeRoll(archer, GetSafeDirection());
}
```

### Warrior Gameplay  
```cpp
// Spin attack
UseMeleeSwing(warrior, direction, 360.0f);  // Full circle

// Leap and slam
Vector3 leap_target = position + direction * 15.0f;
UseAreaSkill(warrior, GROUND_SLAM, leap_target);
```

## Design Decisions

### Why Action Combat?
1. **Engagement**: More interactive gameplay
2. **Skill Expression**: Player skill matters
3. **Modern Feel**: Matches current trends
4. **Spectator Friendly**: Exciting to watch

### Projectile Design
1. **Entity-based**: Each projectile is an entity
2. **Fixed Speed**: Simplifies prediction
3. **Radius Collision**: Forgiving hitboxes
4. **Client Prediction**: Can show immediately

### Dodge Design
1. **Limited Charges**: Prevents spam
2. **I-Frames**: Clear defensive option
3. **Fixed Distance**: Predictable outcome
4. **No Collision**: Phases through enemies

## Network Considerations

### High Update Rate Required
- Position updates: 30Hz minimum
- Projectile spawns: Must be synchronized
- Hit confirmation: Server authoritative
- Dodge state: Broadcast immediately

### Lag Compensation
- Client-side prediction for projectiles
- Rollback for hit detection
- Interpolation for smooth movement
- Favor shooter for hits

## Limitations

1. **Network Intensive**: Requires low latency
2. **Complexity**: Harder to learn
3. **Mobile Difficulty**: Hard to control on touch
4. **Balance Challenge**: Skill gaps emerge

## Future Enhancements

1. **Combos**: Chain abilities together
2. **Parry System**: Active blocking
3. **Charged Attacks**: Hold to power up
4. **Environmental**: Use terrain
5. **Momentum**: Speed affects damage

## Comparison with Target-Based

| Aspect | Action | Target-Based |
|--------|--------|-------------|
| **Fun Factor** | High | Moderate |
| **Skill Ceiling** | Very High | Moderate |
| **Accessibility** | Low | High |
| **Network Cost** | High | Low |
| **Development** | Complex | Simple |
| **Esports** | Excellent | Limited |

The action combat system provides dynamic, skill-based gameplay that rewards positioning, timing, and mechanical skill.