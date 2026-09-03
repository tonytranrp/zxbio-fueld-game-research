add_executable(engine_core_tests
    test_clock.cpp
    test_application.cpp
)
target_link_libraries(engine_core_tests PRIVATE engine_core Catch2::Catch2WithMain project_warnings)

include(Catch)
catch_discover_tests(engine_core_tests)
