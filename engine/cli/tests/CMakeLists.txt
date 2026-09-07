# Not analyzed by clang-tidy: Catch2 assertion macros are structurally noisy under several bugprone checks, and tests are not product code (see the root VOXEL_CLANG_TIDY option).
set(CMAKE_CXX_CLANG_TIDY "")
add_executable(engine_cli_tests
    test_option_table.cpp
)
target_link_libraries(engine_cli_tests PRIVATE engine_cli Catch2::Catch2WithMain project_warnings)

include(Catch)
catch_discover_tests(engine_cli_tests)
