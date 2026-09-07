# Not analyzed by clang-tidy: Catch2 assertion macros are structurally noisy under several bugprone checks, and tests are not product code (see the root VOXEL_CLANG_TIDY option).
set(CMAKE_CXX_CLANG_TIDY "")
add_executable(dev_telemetry_tests
    test_frame_report.cpp
)
target_include_directories(dev_telemetry_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(dev_telemetry_tests PRIVATE dev_telemetry Catch2::Catch2WithMain project_warnings)

include(Catch)
catch_discover_tests(dev_telemetry_tests)
