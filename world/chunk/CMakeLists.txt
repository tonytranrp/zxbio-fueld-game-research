add_library(world_chunk STATIC
    src/chunk_voxels.cpp
    src/chunk_store.cpp
)

target_sources(world_chunk PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/world/chunk/material.hpp
        include/world/chunk/chunk_voxels.hpp
        include/world/chunk/chunk_coord.hpp
        include/world/chunk/chunk.hpp
        include/world/chunk/chunk_load_state.hpp
        include/world/chunk/chunk_store.hpp
)

target_link_libraries(world_chunk PUBLIC project_warnings)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
