add_library(world_svo STATIC
    src/brick.cpp
    src/brick_tree.cpp
    src/height_field.cpp
    src/terrain_sampler.cpp
    src/tree_builder.cpp
    src/ray_trace.cpp
)

target_sources(world_svo PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/world/svo/brick.hpp
        include/world/svo/tree_layout.hpp
        include/world/svo/brick_tree.hpp
        include/world/svo/sampler.hpp
        include/world/svo/height_field.hpp
        include/world/svo/terrain_sampler.hpp
        include/world/svo/tree_builder.hpp
        include/world/svo/ray_trace.hpp
)

# detail/ holds the build_tree template body: this module's own src/ instantiates it for the
# production sampler; tests include it directly for their analytic samplers. Deliberately not part
# of the public FILE_SET, matching render/diligent's detail/ convention.
target_include_directories(world_svo PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

# engine_jobs: build_tree takes an optional ThreadPool for its parallel subtree jobs.
target_link_libraries(world_svo PUBLIC project_warnings engine_core engine_jobs world_chunk world_generation)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
