# Goal 258's headroom probe: how many distinct materials a real brick holds, which is the number that
# decides whether a per-brick palette is worth its indirection. Not shipped -- a measurement tool.
add_executable(palette_probe main.cpp)
target_link_libraries(palette_probe PRIVATE world_svo world_generation project_warnings)
