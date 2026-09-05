# Not analyzed by clang-tidy: Catch2 assertion macros are structurally noisy under several bugprone checks, and tests are not product code (see the root VOXEL_CLANG_TIDY option).
set(CMAKE_CXX_CLANG_TIDY "")
add_executable(world_chunk_tests
    test_chunk_voxels.cpp
    test_chunk_coord.cpp
    test_block_type.cpp
)
target_link_libraries(world_chunk_tests PRIVATE world_chunk Catch2::Catch2WithMain project_warnings)

include(Catch)
catch_discover_tests(world_chunk_tests)
