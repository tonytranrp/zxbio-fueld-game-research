add_library(world_wind STATIC
    src/wind_field.cpp
)

target_sources(world_wind PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/world/wind/wind_field.hpp
)

target_link_libraries(world_wind PUBLIC project_warnings engine_core)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
