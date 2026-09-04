# Not analyzed by clang-tidy: Catch2 assertion macros are structurally noisy under several bugprone checks, and tests are not product code (see the root VOXEL_CLANG_TIDY option).
set(CMAKE_CXX_CLANG_TIDY "")
# Headless tests only: frustum math and the allocation tracker have no GPU dependency by design.
# Device/PSO/draw paths are exercised by voxel_app's own --frames/--verify-frame smoke run
# (Phase 1 completion brief Group B), not by ctest, so the suite stays runnable on GPU-less CI.
add_executable(render_diligent_tests
    test_frustum.cpp
    test_memory_tracking.cpp
)
target_link_libraries(render_diligent_tests PRIVATE render_diligent Catch2::Catch2WithMain project_warnings)

include(Catch)
catch_discover_tests(render_diligent_tests)
