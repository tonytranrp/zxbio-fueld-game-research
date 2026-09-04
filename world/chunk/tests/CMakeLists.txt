add_executable(world_chunk_tests
    test_chunk_voxels.cpp
    test_chunk_coord.cpp
)
target_link_libraries(world_chunk_tests PRIVATE world_chunk Catch2::Catch2WithMain project_warnings)

include(Catch)
catch_discover_tests(world_chunk_tests)
