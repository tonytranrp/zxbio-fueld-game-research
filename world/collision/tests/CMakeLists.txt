# Not analyzed by clang-tidy: Catch2 assertion macros are structurally noisy under several bugprone checks, and tests are not product code (see the root VOXEL_CLANG_TIDY option).
set(CMAKE_CXX_CLANG_TIDY "")
add_executable(world_collision_tests
    test_aabb_sweep.cpp
    test_terrain_collider.cpp
    test_octree_collider.cpp
    test_grid_collider.cpp
)
target_link_libraries(world_collision_tests PRIVATE world_collision world_generation world_svo
                                                   world_materials Catch2::Catch2WithMain project_warnings)
# detail/tree_builder_impl.hpp: the octree test instantiates build_tree for its own sampler, the
# same way world/svo's own tests do (the production instantiation is TerrainSampler-only).
target_include_directories(world_collision_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../svo")

include(Catch)
catch_discover_tests(world_collision_tests)
