# Not analyzed by clang-tidy: Catch2 assertion macros are structurally noisy under several bugprone checks, and tests are not product code (see the root VOXEL_CLANG_TIDY option).
set(CMAKE_CXX_CLANG_TIDY "")
add_executable(world_svo_tests
    test_brick.cpp
    test_tree_layout.cpp
    test_height_field.cpp
    test_tree_builder.cpp
    test_ray_trace.cpp
    test_terrain_sampler.cpp
)
# detail/tree_builder_impl.hpp: the tests instantiate build_tree for their own analytic samplers.
target_include_directories(world_svo_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_link_libraries(world_svo_tests PRIVATE world_svo world_generation engine_jobs Catch2::Catch2WithMain project_warnings)

include(Catch)
catch_discover_tests(world_svo_tests)
