# Catch2's own macro expansion trips project clang-tidy checks; tests are exempt product-wide.
set(CMAKE_CXX_CLANG_TIDY "")

add_executable(test_engine_events test_dispatcher.cpp)
target_link_libraries(test_engine_events PRIVATE engine_events Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(test_engine_events)
