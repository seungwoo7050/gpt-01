#!/bin/bash

cd /Users/woopinbell/Desktop/tmp/clear-claude-prompt/clear-knowledge/use-portfolio/cpp-multiplayer-game-server/knowledge/reference

# Create mapping file for reference updates
echo "Creating number mapping..."
cat > number_mapping.txt << 'EOF'
00 -> 00
01 -> 01
02 -> 02
03 -> 03
04 -> 04
05 -> 05
06 -> 06
07 -> 07
08 -> 08
09 -> 09
10 -> 10
13 -> 11
14 -> 12
15 -> 13
16 -> 14
23 -> 15
24 -> 16
25 -> 17
26 -> 18
27 -> 19
33 -> 20
34 -> 21
35 -> 22
36 -> 23
43 -> 24
44_microservices_architecture.md -> 25
44_microservices_architecture_advanced.md -> 26
45 -> 27
46 -> 28
53 -> 29
54 -> 30
55 -> 31
56 -> 32
63 -> 33
64 -> 34
65 -> 35
66 -> 36
73 -> 37
EOF

# Rename files to temporary names first to avoid conflicts
echo "Renaming files to temporary names..."
for file in [0-9]*.md; do
    if [[ ! "$file" == backup* ]]; then
        mv "$file" "TEMP_$file" 2>/dev/null || true
    fi
done

# Now rename to final sequential numbers
echo "Renaming to sequential numbers..."
mv "TEMP_00_cpp_basics_for_game_server.md" "00_cpp_basics_for_game_server.md"
mv "TEMP_01_advanced_cpp_features.md" "01_advanced_cpp_features.md"
mv "TEMP_02_modern_cpp_containers_utilities.md" "02_modern_cpp_containers_utilities.md"
mv "TEMP_03_lambda_functional_programming.md" "03_lambda_functional_programming.md"
mv "TEMP_04_move_semantics_perfect_forwarding.md" "04_move_semantics_perfect_forwarding.md"
mv "TEMP_05_exception_handling_safety.md" "05_exception_handling_safety.md"
mv "TEMP_06_stl_algorithms_comprehensive.md" "06_stl_algorithms_comprehensive.md"
mv "TEMP_07_build_systems_advanced.md" "07_build_systems_advanced.md"
mv "TEMP_08_testing_frameworks_complete.md" "08_testing_frameworks_complete.md"
mv "TEMP_09_debugging_profiling_toolchain.md" "09_debugging_profiling_toolchain.md"
mv "TEMP_10_practical_setup_guide.md" "10_practical_setup_guide.md"
mv "TEMP_13_multithreading_basics.md" "11_multithreading_basics.md"
mv "TEMP_14_lockfree_programming_guide.md" "12_lockfree_programming_guide.md"
mv "TEMP_15_network_programming_basics.md" "13_network_programming_basics.md"
mv "TEMP_16_advanced_networking_optimization.md" "14_advanced_networking_optimization.md"
mv "TEMP_23_game_server_core_concepts.md" "15_game_server_core_concepts.md"
mv "TEMP_24_ecs_architecture_system.md" "16_ecs_architecture_system.md"
mv "TEMP_25_system_separation_world_management.md" "17_system_separation_world_management.md"
mv "TEMP_26_realtime_physics_synchronization.md" "18_realtime_physics_synchronization.md"
mv "TEMP_27_ai_game_logic_engine.md" "19_ai_game_logic_engine.md"
mv "TEMP_33_database_design_optimization_guide.md" "20_database_design_optimization_guide.md"
mv "TEMP_34_nosql_integration_guide.md" "21_nosql_integration_guide.md"
mv "TEMP_35_redis_cluster_caching_master.md" "22_redis_cluster_caching_master.md"
mv "TEMP_36_advanced_database_operations.md" "23_advanced_database_operations.md"
mv "TEMP_43_message_queue_systems.md" "24_message_queue_systems.md"
mv "TEMP_44_microservices_architecture.md" "25_microservices_architecture.md"
mv "TEMP_44_microservices_architecture_advanced.md" "26_microservices_architecture_advanced.md"
mv "TEMP_45_grpc_services_guide.md" "27_grpc_services_guide.md"
mv "TEMP_46_event_driven_architecture_guide.md" "28_event_driven_architecture_guide.md"
mv "TEMP_53_performance_optimization_basics.md" "29_performance_optimization_basics.md"
mv "TEMP_54_performance_engineering_advanced.md" "30_performance_engineering_advanced.md"
mv "TEMP_55_security_authentication_guide.md" "31_security_authentication_guide.md"
mv "TEMP_56_advanced_security_compliance.md" "32_advanced_security_compliance.md"
mv "TEMP_63_kubernetes_operations_guide.md" "33_kubernetes_operations_guide.md"
mv "TEMP_64_cloud_native_game_server.md" "34_cloud_native_game_server.md"
mv "TEMP_65_global_service_operations.md" "35_global_service_operations.md"
mv "TEMP_66_realtime_analytics_bigdata_pipeline.md" "36_realtime_analytics_bigdata_pipeline.md"
mv "TEMP_73_next_gen_game_tech.md" "37_next_gen_game_tech.md"

echo "File renaming complete!"