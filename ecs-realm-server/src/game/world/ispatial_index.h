#pragma once

#include "core/ecs/types.h"
#include "core/utils/vector3.h"
#include <vector>

namespace mmorpg::game::world {

class ISpatialIndex {
public:
    virtual ~ISpatialIndex() = default;

    virtual void AddEntity(core::ecs::EntityId entity, const core::utils::Vector3& position) = 0;
    virtual void RemoveEntity(core::ecs::EntityId entity) = 0;
    virtual void UpdateEntity(core::ecs::EntityId entity, const core::utils::Vector3& old_pos, const core::utils::Vector3& new_pos) = 0;

    virtual std::vector<core::ecs::EntityId> GetEntitiesInRadius(const core::utils::Vector3& center, float radius) const = 0;
    virtual std::vector<core::ecs::EntityId> GetEntitiesInBox(const core::utils::Vector3& min, const core::utils::Vector3& max) const = 0;
};

} // namespace mmorpg::game::world
