add_library(engine_events INTERFACE)

target_sources(engine_events INTERFACE
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/engine/events/dispatcher.hpp
)

target_link_libraries(engine_events INTERFACE project_warnings EnTT::EnTT)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
