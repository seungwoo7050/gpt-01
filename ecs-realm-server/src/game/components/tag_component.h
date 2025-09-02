#pragma once

#include <string>
#include <cstdint>

namespace mmorpg::game::components {

// [SEQUENCE: MVP2-19] Stores metadata about an entity, such as its name and type (e.g., Player, NPC, Monster).
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

// [SEQUENCE: MVP2-15] Tag component for entity metadata
struct TagComponent {
    std::string name;
    EntityType type = EntityType::NONE;
    uint32_t flags = 0;
};

} // namespace mmorpg::game::components
