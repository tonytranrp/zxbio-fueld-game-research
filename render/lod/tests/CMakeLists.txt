set(CMAKE_CXX_CLANG_TIDY "")
add_executable(render_lod_tests
    test_perceptual.cpp
)
target_link_libraries(render_lod_tests PRIVATE render_lod Catch2::Catch2WithMain project_warnings)

include(Catch)
catch_discover_tests(render_lod_tests)
