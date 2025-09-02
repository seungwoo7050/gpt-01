#include "ai_behavior_system.h"
#include "core/ecs/world.h"
#include "game/components/ai_component.h"

namespace mmorpg::game::systems {

using namespace core::ecs;
using namespace game::components;

// [SEQUENCE: MVP9-40] Update loop for the AI behavior system
void AiBehaviorSystem::Update(float delta_time) {
    auto& world = World::Instance();
    for (EntityId entity : world.GetEntitiesWith<AiComponent>()) {
        ProcessAi(entity, delta_time);
    }
}

// [SEQUENCE: MVP9-41] Process AI for a single entity
void AiBehaviorSystem::ProcessAi(EntityId entity_id, float delta_time) {
    auto& world = World::Instance();
    auto& ai_comp = world.GetComponent<AiComponent>(entity_id);
    
    if (ai_comp.behavior_tree) {
        ai_comp.behavior_tree->Execute(entity_id);
    }
}

} // namespace mmorpg::game::systems