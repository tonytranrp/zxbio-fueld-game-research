add_executable(mesh_dump src/main.cpp src/dump_options.cpp)
target_link_libraries(mesh_dump PRIVATE project_warnings engine_cli world_generation world_meshing)
