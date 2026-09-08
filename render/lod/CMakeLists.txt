# Prompt 007 goal 326. Header-only and dependency-free on purpose: `world/svo`'s LOD ladder cites
# it as well as the renderer, and a perceptual criterion is not a rendering-backend concern.
add_library(render_lod INTERFACE)
target_include_directories(render_lod INTERFACE include)
target_link_libraries(render_lod INTERFACE project_warnings)
target_sources(render_lod
    INTERFACE
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/render/lod/perceptual.hpp
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
