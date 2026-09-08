add_library(world_generation STATIC
    src/heightmap_generator.cpp
    src/field/terrain_field.cpp
    src/field/macro_pipeline.cpp
    src/field/fluvial.cpp
    src/field/rivers.cpp
    src/validation/acceptance.cpp
    src/field/climate.cpp
    src/field/biome.cpp
    src/field/stencils.cpp
    src/terrain_fill.cpp
    src/tree_placement.cpp
    src/tree_skeleton.cpp
    src/tree_sway.cpp
)

target_sources(world_generation PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/world/generation/heightmap_generator.hpp
        include/world/generation/field/terrain_field.hpp
        include/world/generation/field/field_sampler.hpp
        include/world/generation/field/macro_pipeline.hpp
        include/world/generation/field/fluvial.hpp
        include/world/generation/field/rivers.hpp
        include/world/generation/validation/acceptance.hpp
        include/world/generation/field/climate.hpp
        include/world/generation/field/biome.hpp
        include/world/generation/field/stencils.hpp
        include/world/generation/terrain_fill.hpp
        include/world/generation/tree_placement.hpp
        include/world/generation/tree_skeleton.hpp
        include/world/generation/tree_sway.hpp
)

# engine_core for the GLM include convention (engine/core/math.hpp) tree_placement.hpp uses.
# world_wind: the sway model (tree_sway) reads the ONE wind field rather than inventing a
# second one. render_lod: `sway_joint_budget` asks the perceptual module what is too small to see
# instead of hard-coding a distance -- header-only INTERFACE targets, both.
target_link_libraries(world_generation PUBLIC project_warnings world_chunk engine_core world_wind render_lod)
target_link_libraries(world_generation PRIVATE FastNoise2)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
