# Not analyzed by clang-tidy: Catch2 assertion macros are structurally noisy under several bugprone checks, and tests are not product code (see the root VOXEL_CLANG_TIDY option).
set(CMAKE_CXX_CLANG_TIDY "")
add_executable(dev_scenario_tests
    test_motion_script.cpp
    test_scenario_format.cpp
)
target_link_libraries(dev_scenario_tests PRIVATE dev_scenario Catch2::Catch2WithMain project_warnings)
# The checked-in scenario library's path, so the round-trip test does not depend on the working
# directory ctest happens to launch from.
target_compile_definitions(dev_scenario_tests
    PRIVATE DEV_SCENARIO_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../scenarios")

include(Catch)
catch_discover_tests(dev_scenario_tests)
