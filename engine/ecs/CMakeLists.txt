add_library(engine_ecs INTERFACE)

target_sources(engine_ecs INTERFACE
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/engine/ecs/registry.hpp
        include/engine/ecs/components.hpp
)

target_link_libraries(engine_ecs INTERFACE project_warnings engine_core EnTT::EnTT)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
