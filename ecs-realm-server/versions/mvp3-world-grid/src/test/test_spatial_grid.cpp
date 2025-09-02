#include "core/ecs/optimized/optimized_world.h"
#include "game/components/transform_component.h"
#include "game/systems/spatial_indexing_system.h"
#include <iostream>
#include <chrono>
#include <random>

// [SEQUENCE: 1] Test spatial grid performance and correctness
int main() {
    using namespace mmorpg;
    
    // Create world and spatial system
    core::ecs::optimized::OptimizedWorld world;
    auto spatial_system = std::make_unique<game::systems::SpatialIndexingSystem>();
    
    // Initialize system
    spatial_system->SetWorld(&world);
    world.AddSystem(std::move(spatial_system));
    world.Init();
    
    // Get spatial system reference
    auto* spatial = dynamic_cast<game::systems::SpatialIndexingSystem*>(
        world.GetSystem("SpatialIndexingSystem")
    );
    
    std::cout << "=== Spatial Grid Test ===\n";
    
    // [SEQUENCE: 2] Create test entities
    constexpr size_t ENTITY_COUNT = 1000;
    std::vector<core::ecs::EntityId> entities;
    entities.reserve(ENTITY_COUNT);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> pos_dist(-2000.0f, 2000.0f);
    
    std::cout << "Creating " << ENTITY_COUNT << " entities...\n";
    
    for (size_t i = 0; i < ENTITY_COUNT; ++i) {
        auto entity = world.CreateEntity();
        entities.push_back(entity);
        
        // Add transform at random position
        game::components::TransformComponent transform;
        transform.position = core::utils::Vector3(
            pos_dist(gen), pos_dist(gen), 0.0f
        );
        transform.scale = core::utils::Vector3(1, 1, 1);
        
        world.AddComponent(entity, transform);
    }
    
    // [SEQUENCE: 3] Test spatial queries
    std::cout << "\nTesting spatial queries...\n";
    
    // Update spatial index
    world.Update(0.016f);  // One frame
    
    // Test range query
    core::utils::Vector3 query_center(0, 0, 0);
    float query_radius = 200.0f;
    
    auto start = std::chrono::high_resolution_clock::now();
    auto nearby = spatial->GetEntitiesInRadius(query_center, query_radius);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Range query (radius " << query_radius << "): "
              << nearby.size() << " entities found in "
              << duration.count() << " microseconds\n";
    
    // [SEQUENCE: 4] Test entity movement
    std::cout << "\nTesting entity movement...\n";
    
    // Move some entities
    std::uniform_real_distribution<float> move_dist(-10.0f, 10.0f);
    size_t entities_to_move = 100;
    
    for (size_t i = 0; i < entities_to_move; ++i) {
        auto* transform = world.GetComponent<game::components::TransformComponent>(entities[i]);
        if (transform) {
            transform->position.x += move_dist(gen);
            transform->position.y += move_dist(gen);
        }
    }
    
    // Update spatial index
    start = std::chrono::high_resolution_clock::now();
    world.Update(0.016f);
    end = std::chrono::high_resolution_clock::now();
    
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "Spatial update after movement: " << duration.count() << " microseconds\n";
    
    // [SEQUENCE: 5] Test visibility query
    if (!entities.empty()) {
        auto observer = entities[0];
        float view_distance = 300.0f;
        
        start = std::chrono::high_resolution_clock::now();
        auto visible = spatial->GetEntitiesInView(observer, view_distance);
        end = std::chrono::high_resolution_clock::now();
        
        duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "\nVisibility query (distance " << view_distance << "): "
                  << visible.size() << " entities visible in "
                  << duration.count() << " microseconds\n";
    }
    
    // [SEQUENCE: 6] Grid statistics
    if (auto* grid = spatial->GetWorldGrid()) {
        std::cout << "\n=== Grid Statistics ===\n";
        std::cout << "Total entities: " << grid->GetEntityCount() << "\n";
        std::cout << "Occupied cells: " << grid->GetOccupiedCellCount() << "\n";
        
        // Calculate density
        float density = static_cast<float>(ENTITY_COUNT) / 
                       static_cast<float>(grid->GetOccupiedCellCount());
        std::cout << "Average entities per cell: " << density << "\n";
    }
    
    // Cleanup
    world.Shutdown();
    
    std::cout << "\nTest complete!\n";
    return 0;
}