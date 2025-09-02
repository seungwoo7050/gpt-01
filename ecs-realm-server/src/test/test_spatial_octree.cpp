#include "core/ecs/optimized/optimized_world.h"
#include "game/components/transform_component.h"
#include "game/systems/octree_spatial_system.h"
#include <iostream>
#include <chrono>
#include <random>

// [SEQUENCE: MVP3-61] test_spatial_octree.cpp: Test octree performance and correctness
int main() {
    using namespace mmorpg;
    
    // Create world and octree system
    core::ecs::optimized::OptimizedWorld world;
    auto octree_system = std::make_unique<game::systems::OctreeSpatialSystem>();
    
    // Initialize system
    octree_system->SetWorld(&world);
    world.AddSystem(std::move(octree_system));
    world.Init();
    
    // Get octree system reference
    auto* spatial = dynamic_cast<game::systems::OctreeSpatialSystem*>(
        world.GetSystem("OctreeSpatialSystem")
    );
    
    std::cout << "=== Octree Spatial Test ===\n";
    
    // [SEQUENCE: MVP3-62] test_spatial_octree.cpp: Create test entities with 3D distribution
    constexpr size_t ENTITY_COUNT = 1000;
    std::vector<core::ecs::EntityId> entities;
    entities.reserve(ENTITY_COUNT);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> pos_dist(-2000.0f, 2000.0f);
    std::uniform_real_distribution<float> height_dist(-500.0f, 500.0f);
    
    std::cout << "Creating " << ENTITY_COUNT << " entities in 3D space...\n";
    
    for (size_t i = 0; i < ENTITY_COUNT; ++i) {
        auto entity = world.CreateEntity();
        entities.push_back(entity);
        
        // Add transform with 3D position
        game::components::TransformComponent transform;
        transform.position = core::utils::Vector3(
            pos_dist(gen), pos_dist(gen), height_dist(gen)
        );
        transform.scale = core::utils::Vector3(1, 1, 1);
        
        world.AddComponent(entity, transform);
    }
    
    // [SEQUENCE: MVP3-63] test_spatial_octree.cpp: Test spatial queries
    std::cout << "\nTesting 3D spatial queries...\n";
    
    // Update spatial index
    world.Update(0.016f);  // One frame
    
    // Test sphere query
    core::utils::Vector3 query_center(0, 0, 0);
    float query_radius = 300.0f;
    
    auto start = std::chrono::high_resolution_clock::now();
    auto nearby = spatial->GetEntitiesInRadius(query_center, query_radius);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Sphere query (radius " << query_radius << "): "
              << nearby.size() << " entities found in "
              << duration.count() << " microseconds\n";
    
    // [SEQUENCE: MVP3-64] test_spatial_octree.cpp: Test 3D box query
    core::utils::Vector3 box_min(-200, -200, -100);
    core::utils::Vector3 box_max(200, 200, 100);
    
    start = std::chrono::high_resolution_clock::now();
    auto in_box = spatial->GetEntitiesInBox(box_min, box_max);
    end = std::chrono::high_resolution_clock::now();
    
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Box query: " << in_box.size() << " entities found in "
              << duration.count() << " microseconds\n";
    
    // [SEQUENCE: MVP3-65] test_spatial_octree.cpp: Test vertical queries
    core::utils::Vector3 ground_pos(0, 0, 0);
    
    start = std::chrono::high_resolution_clock::now();
    auto above = spatial->GetEntitiesAbove(ground_pos, 200.0f);
    end = std::chrono::high_resolution_clock::now();
    
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "\nVertical queries:\n";
    std::cout << "Entities above ground: " << above.size() << " in "
              << duration.count() << " microseconds\n";
    
    start = std::chrono::high_resolution_clock::now();
    auto below = spatial->GetEntitiesBelow(ground_pos, 200.0f);
    end = std::chrono::high_resolution_clock::now();
    
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Entities below ground: " << below.size() << " in "
              << duration.count() << " microseconds\n";
    
    // [SEQUENCE: MVP3-66] test_spatial_octree.cpp: Test entity movement with large displacements
    std::cout << "\nTesting 3D entity movement...\n";
    
    // Move entities with varying distances
    std::uniform_real_distribution<float> move_dist(-50.0f, 50.0f);
    size_t entities_to_move = 200;
    
    for (size_t i = 0; i < entities_to_move; ++i) {
        auto* transform = world.GetComponent<game::components::TransformComponent>(entities[i]);
        if (transform) {
            transform->position.x += move_dist(gen);
            transform->position.y += move_dist(gen);
            transform->position.z += move_dist(gen) * 0.5f; // Less vertical movement
        }
    }
    
    // Update octree
    start = std::chrono::high_resolution_clock::now();
    world.Update(0.016f);
    end = std::chrono::high_resolution_clock::now();
    
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "Octree update after movement: " << duration.count() << " microseconds\n";
    
    // [SEQUENCE: MVP3-67] test_spatial_octree.cpp: Octree statistics
    size_t total_nodes, leaf_nodes, tree_entities, depth;
    spatial->GetOctreeStats(total_nodes, leaf_nodes, tree_entities, depth);
    
    std::cout << "\n=== Octree Statistics ===\n";
    std::cout << "Total nodes: " << total_nodes << "\n";
    std::cout << "Leaf nodes: " << leaf_nodes << "\n";
    std::cout << "Tree depth: " << depth << "\n";
    std::cout << "Entities tracked: " << tree_entities << "\n";
    
    if (leaf_nodes > 0) {
        float avg_per_leaf = static_cast<float>(tree_entities) / static_cast<float>(leaf_nodes);
        std::cout << "Average entities per leaf: " << avg_per_leaf << "\n";
    }
    
    // [SEQUENCE: MVP3-68] test_spatial_octree.cpp: Performance comparison message
    std::cout << "\n=== Grid vs Octree Comparison ===\n";
    std::cout << "Grid advantages: Faster updates, predictable performance\n";
    std::cout << "Octree advantages: 3D queries, memory efficiency, sparse worlds\n";
    
    // Cleanup
    world.Shutdown();
    
    std::cout << "\nTest complete!\n";
    return 0;
}