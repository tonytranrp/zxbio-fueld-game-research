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
        include/world/chunk/coord_containers.hpp
        include/world/chunk/chunk.hpp
        include/world/chunk/chunk_load_state.hpp
        include/world/chunk/chunk_store.hpp
)

# Boost::unordered backs coord_containers.hpp's CoordMap/CoordSet (header-only usage).
# world_materials: material.hpp re-exports the component registry's MaterialID / kMaterialCount.
target_link_libraries(world_chunk PUBLIC project_warnings Boost::unordered world_materials)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
