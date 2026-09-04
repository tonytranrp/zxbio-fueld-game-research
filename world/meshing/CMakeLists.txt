add_library(world_meshing STATIC
    src/mesh_extractor.cpp
)

target_sources(world_meshing PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/world/meshing/mesh_data.hpp
        include/world/meshing/mesh_extractor.hpp
)

target_link_libraries(world_meshing PUBLIC project_warnings world_chunk engine_core)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
