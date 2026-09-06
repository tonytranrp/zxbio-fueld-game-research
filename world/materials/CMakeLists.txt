add_library(world_materials INTERFACE)

target_sources(world_materials INTERFACE
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/world/materials/material_def.hpp
        include/world/materials/terrain_query.hpp
        include/world/materials/registry.hpp
        include/world/materials/materials.hpp
        include/world/materials/defs/air.hpp
        include/world/materials/defs/stone.hpp
        include/world/materials/defs/dirt.hpp
        include/world/materials/defs/water.hpp
        include/world/materials/defs/wood.hpp
        include/world/materials/defs/leaves.hpp
        include/world/materials/defs/sand.hpp
        include/world/materials/defs/grass.hpp
)

target_link_libraries(world_materials INTERFACE project_warnings)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
