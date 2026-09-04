# Not analyzed by clang-tidy: Catch2 assertion macros are structurally noisy under several bugprone checks, and tests are not product code (see the root VOXEL_CLANG_TIDY option).
set(CMAKE_CXX_CLANG_TIDY "")
add_executable(engine_ecs_tests test_registry.cpp)
target_link_libraries(engine_ecs_tests PRIVATE engine_ecs Catch2::Catch2WithMain project_warnings)

include(Catch)
catch_discover_tests(engine_ecs_tests)
