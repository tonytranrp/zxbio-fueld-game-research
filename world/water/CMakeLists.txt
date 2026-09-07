add_library(world_water STATIC
    src/gerstner.cpp
)

target_sources(world_water PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/world/water/gerstner.hpp
)

target_link_libraries(world_water PUBLIC project_warnings engine_core world_wind)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
