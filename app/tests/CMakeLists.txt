# Not analyzed by clang-tidy: Catch2 assertion macros are structurally noisy under several bugprone checks, and tests are not product code (see the root VOXEL_CLANG_TIDY option).
set(CMAKE_CXX_CLANG_TIDY "")
# Tests the app-local spectator controller (pure math over plain components -- no GLFW, no GPU).
# It compiles the controller TU directly rather than linking voxel_app: an executable target's
# objects aren't linkable, and promoting app glue to a library just for tests would invert the
# dependency story.
add_executable(app_tests
    test_spectator_camera.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/spectator_camera.cpp
)
target_include_directories(app_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")
target_link_libraries(app_tests PRIVATE engine_core engine_ecs engine_input Catch2::Catch2WithMain project_warnings)

include(Catch)
catch_discover_tests(app_tests)
