# Not analyzed by clang-tidy: Catch2 assertion macros are structurally noisy under several bugprone
# checks, and tests are not product code (see the root VOXEL_CLANG_TIDY option).
set(CMAKE_CXX_CLANG_TIDY "")
# moire_metric.cpp is compiled in directly rather than linked: voxel_harness is an executable and an
# executable's objects are not linkable, which is the same reason the harness itself compiles
# app/src's translation units. The metric needs no PNG decoder -- its tests build Images in memory --
# so image_compare.cpp and libpng stay out of this target.
add_executable(dev_harness_tests
    test_moire_metric.cpp
    ../src/moire_metric.cpp
)
target_include_directories(dev_harness_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../src")
target_link_libraries(dev_harness_tests PRIVATE Catch2::Catch2WithMain project_warnings)

include(Catch)
catch_discover_tests(dev_harness_tests)
