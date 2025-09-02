#!/bin/bash

echo "Updating cross-references in all documents..."

# Define the base directory
BASE_DIR="/Users/woopinbell/Desktop/tmp/clear-claude-prompt/clear-knowledge/use-portfolio/cpp-multiplayer-game-server/knowledge"

# Update references in all markdown files
find "$BASE_DIR" -name "*.md" -type f | while read -r file; do
    echo "Processing: $file"
    
    # Create temporary file
    cp "$file" "$file.bak"
    
    # Apply all replacements
    sed -i '' \
        -e 's/73_next_gen_game_tech/37_next_gen_game_tech/g' \
        -e 's/66_realtime_analytics_bigdata_pipeline/36_realtime_analytics_bigdata_pipeline/g' \
        -e 's/65_global_service_operations/35_global_service_operations/g' \
        -e 's/64_cloud_native_game_server/34_cloud_native_game_server/g' \
        -e 's/63_kubernetes_operations_guide/33_kubernetes_operations_guide/g' \
        -e 's/56_advanced_security_compliance/32_advanced_security_compliance/g' \
        -e 's/55_security_authentication_guide/31_security_authentication_guide/g' \
        -e 's/54_performance_engineering_advanced/30_performance_engineering_advanced/g' \
        -e 's/53_performance_optimization_basics/29_performance_optimization_basics/g' \
        -e 's/46_event_driven_architecture_guide/28_event_driven_architecture_guide/g' \
        -e 's/45_grpc_services_guide/27_grpc_services_guide/g' \
        -e 's/44_microservices_architecture_advanced/26_microservices_architecture_advanced/g' \
        -e 's/44_microservices_architecture\.md/25_microservices_architecture.md/g' \
        -e 's/44_microservices_architecture/25_microservices_architecture/g' \
        -e 's/43_message_queue_systems/24_message_queue_systems/g' \
        -e 's/36_advanced_database_operations/23_advanced_database_operations/g' \
        -e 's/35_redis_cluster_caching_master/22_redis_cluster_caching_master/g' \
        -e 's/34_nosql_integration_guide/21_nosql_integration_guide/g' \
        -e 's/33_database_design_optimization_guide/20_database_design_optimization_guide/g' \
        -e 's/27_ai_game_logic_engine/19_ai_game_logic_engine/g' \
        -e 's/26_realtime_physics_synchronization/18_realtime_physics_synchronization/g' \
        -e 's/25_system_separation_world_management/17_system_separation_world_management/g' \
        -e 's/24_ecs_architecture_system/16_ecs_architecture_system/g' \
        -e 's/23_game_server_core_concepts/15_game_server_core_concepts/g' \
        -e 's/16_advanced_networking_optimization/14_advanced_networking_optimization/g' \
        -e 's/15_network_programming_basics/13_network_programming_basics/g' \
        -e 's/14_lockfree_programming_guide/12_lockfree_programming_guide/g' \
        -e 's/13_multithreading_basics/11_multithreading_basics/g' \
        "$file"
    
    # Check if file changed
    if diff -q "$file" "$file.bak" > /dev/null; then
        echo "  No changes needed"
        # Remove backup if no changes
        rm "$file.bak"
    else
        echo "  Updated references"
        # Remove backup after successful update
        rm "$file.bak"
    fi
done

echo "Cross-reference update complete!"