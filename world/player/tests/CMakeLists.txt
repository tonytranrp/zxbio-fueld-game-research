# Not analyzed by clang-tidy: Catch2 assertion macros are structurally noisy under several bugprone checks, and tests are not product code (see the root VOXEL_CLANG_TIDY option).
set(CMAKE_CXX_CLANG_TIDY "")
add_executable(world_player_tests
    test_fixed_step.cpp
    test_player_state.cpp
    test_controller.cpp
)
target_link_libraries(world_player_tests PRIVATE world_player Catch2::Catch2WithMain project_warnings)

include(Catch)
catch_discover_tests(world_player_tests)
