# Not analyzed by clang-tidy: Catch2 assertion macros are structurally noisy under several bugprone checks, and tests are not product code (see the root VOXEL_CLANG_TIDY option).
set(CMAKE_CXX_CLANG_TIDY "")
add_executable(world_meshing_tests
    test_mesh_extractor.cpp
    test_concurrent_meshing.cpp
    test_octahedral.cpp
    test_vertex_quantization.cpp
    test_normal_continuity.cpp
    test_surface_coverage.cpp
    test_baked_ao.cpp
)
target_link_libraries(world_meshing_tests PRIVATE world_meshing world_generation Catch2::Catch2WithMain project_warnings)

include(Catch)
catch_discover_tests(world_meshing_tests)
