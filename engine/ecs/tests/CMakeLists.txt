add_executable(engine_ecs_tests test_registry.cpp)
target_link_libraries(engine_ecs_tests PRIVATE engine_ecs Catch2::Catch2WithMain project_warnings)

include(Catch)
catch_discover_tests(engine_ecs_tests)
