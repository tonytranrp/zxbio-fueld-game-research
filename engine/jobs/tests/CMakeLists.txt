# Not analyzed by clang-tidy: Catch2 assertion macros are structurally noisy under several bugprone checks, and tests are not product code (see the root VOXEL_CLANG_TIDY option).
set(CMAKE_CXX_CLANG_TIDY "")
add_executable(engine_jobs_tests test_thread_pool.cpp)
target_link_libraries(engine_jobs_tests PRIVATE engine_jobs Catch2::Catch2WithMain project_warnings)

include(Catch)
catch_discover_tests(engine_jobs_tests)
