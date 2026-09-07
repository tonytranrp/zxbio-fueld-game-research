# Goal 255/257's probe: the cell grid measured against the single tree it would replace -- per-cell
# rebuild cost (the number goal 254 predicted at 7-14 ms) and traversal cost. Not shipped.
add_executable(cell_grid_probe main.cpp)
target_link_libraries(cell_grid_probe PRIVATE world_svo world_generation engine_jobs project_warnings)
