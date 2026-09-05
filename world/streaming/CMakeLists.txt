# Group S (Voxel Representation Redesign SS3.4): header-only now that ChunkStreamer's decision-
# logic .cpp is gone (removed, not disabled -- the static world has no per-tick load/unload
# decision left to make). What remains is pure world-shape (world_bounds.hpp) and lifecycle event
# PODs (chunk_events.hpp), both templates/inline free functions with nothing to compile separately.
add_library(world_streaming INTERFACE)

target_sources(world_streaming INTERFACE
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/world/streaming/world_bounds.hpp
        include/world/streaming/chunk_events.hpp
)

target_link_libraries(world_streaming INTERFACE project_warnings world_chunk)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
