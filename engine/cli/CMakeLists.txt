add_library(engine_cli STATIC
    src/parser.cpp
    src/help.cpp
    src/response_file.cpp
)

target_sources(engine_cli PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/engine/cli/value.hpp
        include/engine/cli/option.hpp
        include/engine/cli/parser.hpp
        include/engine/cli/help.hpp
        include/engine/cli/response_file.hpp
        include/engine/cli/detail/assign.hpp
)

# glm only for the vec3 conversion (--pos x,y,z). Deliberately NOT engine_core: this module returns
# its diagnostics as strings instead of logging them, which is what lets a test assert the exact
# wording and what keeps the dependency edge from pointing at the log.
target_link_libraries(engine_cli PUBLIC project_warnings glm::glm)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
