# Not analyzed by clang-tidy: Catch2 assertion macros are structurally noisy under several bugprone checks, and tests are not product code (see the root VOXEL_CLANG_TIDY option).
set(CMAKE_CXX_CLANG_TIDY "")
add_executable(world_streaming_tests test_world_bounds.cpp)
target_link_libraries(world_streaming_tests PRIVATE world_streaming Catch2::Catch2WithMain project_warnings)

include(Catch)
catch_discover_tests(world_streaming_tests)
