add_executable(world_generation_tests
    test_terrain_fill.cpp
    test_determinism.cpp
)
target_link_libraries(world_generation_tests PRIVATE world_generation engine_jobs Catch2::Catch2WithMain project_warnings)

include(Catch)
catch_discover_tests(world_generation_tests)
