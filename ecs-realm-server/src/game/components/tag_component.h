#pragma once

#include <string>
#include <cstdint>

namespace mmorpg::game::components {

// [SEQUENCE: 1] Entity type enumeration
enum class EntityType : uint8_t {
    NONE = 0,
    PLAYER = 1,
    NPC = 2,
    MONSTER = 3,
    ITEM = 4,
    PROJECTILE = 5,
    EFFECT = 6,
    TRIGGER = 7
};

// [SEQUENCE: 2] Tag component for entity metadata
struct TagComponent {
    std::string name;
    EntityType type = EntityType::NONE;
    uint32_t flags = 0;  // Bit flags for various properties
    
    // [SEQUENCE: 3] Common flags
    enum Flags : uint32_t {
        INVISIBLE = 1 << 0,
        INVULNERABLE = 1 << 1,
        STATIC = 1 << 2,      // Doesn't move
        NO_COLLISION = 1 << 3,
        FRIENDLY = 1 << 4,
        HOSTILE = 1 << 5,
        NEUTRAL = 1 << 6,
        QUEST_GIVER = 1 << 7,
        MERCHANT = 1 << 8
    };
    
    // [SEQUENCE: 4] Flag helpers
    void SetFlag(uint32_t flag) { flags |= flag; }
    void ClearFlag(uint32_t flag) { flags &= ~flag; }
    bool HasFlag(uint32_t flag) const { return (flags & flag) != 0; }
    
    // [SEQUENCE: 5] Type helpers
    bool IsPlayer() const { return type == EntityType::PLAYER; }
    bool IsNPC() const { return type == EntityType::NPC; }
    bool IsMonster() const { return type == EntityType::MONSTER; }
    bool IsHostile() const { return HasFlag(HOSTILE); }
    bool IsFriendly() const { return HasFlag(FRIENDLY); }
};

} // namespace mmorpg::game::components