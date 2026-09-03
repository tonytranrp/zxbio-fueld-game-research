add_library(engine_core STATIC
    src/clock.cpp
    src/log.cpp
    src/application.cpp
)

target_sources(engine_core PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/engine/core/clock.hpp
        include/engine/core/log.hpp
        include/engine/core/config.hpp
        include/engine/core/application.hpp
        include/engine/core/math.hpp
)

target_link_libraries(engine_core PUBLIC project_warnings glm::glm)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
