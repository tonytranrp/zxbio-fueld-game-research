# Not analyzed by clang-tidy: Catch2 assertion macros are structurally noisy under several bugprone checks, and tests are not product code (see the root VOXEL_CLANG_TIDY option).
set(CMAKE_CXX_CLANG_TIDY "")
add_executable(world_collision_tests
    test_aabb_sweep.cpp
    test_terrain_collider.cpp
)
target_link_libraries(world_collision_tests PRIVATE world_collision world_generation Catch2::Catch2WithMain project_warnings)

include(Catch)
catch_discover_tests(world_collision_tests)
