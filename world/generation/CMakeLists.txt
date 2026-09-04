add_library(world_generation STATIC
    src/heightmap_generator.cpp
    src/terrain_fill.cpp
)

target_sources(world_generation PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/world/generation/heightmap_generator.hpp
        include/world/generation/terrain_fill.hpp
)

target_link_libraries(world_generation PUBLIC project_warnings world_chunk)
target_link_libraries(world_generation PRIVATE FastNoise2)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
