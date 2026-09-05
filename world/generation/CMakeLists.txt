add_library(world_generation STATIC
    src/heightmap_generator.cpp
    src/terrain_fill.cpp
    src/tree_placement.cpp
)

target_sources(world_generation PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/world/generation/heightmap_generator.hpp
        include/world/generation/terrain_fill.hpp
        include/world/generation/tree_placement.hpp
)

# engine_core for the GLM include convention (engine/core/math.hpp) tree_placement.hpp uses.
target_link_libraries(world_generation PUBLIC project_warnings world_chunk engine_core)
target_link_libraries(world_generation PRIVATE FastNoise2)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
