add_library(world_collision STATIC
    src/terrain_collider.cpp
)

target_sources(world_collision PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/world/collision/solid_query.hpp
        include/world/collision/aabb_sweep.hpp
        include/world/collision/terrain_collider.hpp
)

target_link_libraries(world_collision PUBLIC project_warnings engine_core world_chunk world_generation)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
