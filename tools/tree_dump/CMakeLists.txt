add_executable(tree_dump src/main.cpp src/dump_options.cpp)
# ../svo_render for png_writer.hpp: the sway flipbook (goal 335) writes PNGs, and that writer is
# already the tools' dependency-free one.
target_include_directories(tree_dump PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../svo_render")
target_link_libraries(tree_dump PRIVATE project_warnings engine_cli world_generation world_wind)
