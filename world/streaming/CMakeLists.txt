add_library(world_streaming STATIC
    src/chunk_streamer.cpp
)

target_sources(world_streaming PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/world/streaming/chunk_streamer.hpp
        include/world/streaming/chunk_events.hpp
)

# Depends on world_chunk for ChunkCoord only -- a system beside the data modules, not an owner of
# any of them (Phase 1 completion brief §3): the generate/mesh/upload pipeline it commands is
# wired by the caller.
target_link_libraries(world_streaming PUBLIC project_warnings world_chunk)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
