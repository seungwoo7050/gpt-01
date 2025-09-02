# PvP System Tests

# [SEQUENCE: 1] Arena test scenarios
add_executable(arena_test_scenarios
    arena_test_scenarios.cpp
)

target_link_libraries(arena_test_scenarios
    PRIVATE
    game_systems
    core_ecs
    GTest::gtest
    GTest::gtest_main
)

# [SEQUENCE: 2] Open world PvP test scenarios
add_executable(openworld_test_scenarios
    openworld_test_scenarios.cpp
)

target_link_libraries(openworld_test_scenarios
    PRIVATE
    game_systems
    core_ecs
    GTest::gtest
    GTest::gtest_main
)

# [SEQUENCE: 3] PvP integration tests
add_executable(pvp_integration_test
    pvp_integration_test.cpp
)

target_link_libraries(pvp_integration_test
    PRIVATE
    game_systems
    core_ecs
    GTest::gtest
    GTest::gtest_main
)

# [SEQUENCE: 4] Add tests to CTest
add_test(NAME ArenaTestScenarios COMMAND arena_test_scenarios)
add_test(NAME OpenWorldTestScenarios COMMAND openworld_test_scenarios)
add_test(NAME PvPIntegrationTest COMMAND pvp_integration_test)

# [SEQUENCE: 5] Set test properties
set_tests_properties(
    ArenaTestScenarios
    OpenWorldTestScenarios
    PvPIntegrationTest
    PROPERTIES
    TIMEOUT 30
    LABELS "pvp"
)