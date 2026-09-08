add_executable(terrain_dump src/main.cpp)
target_include_directories(terrain_dump PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../svo_render")
target_link_libraries(terrain_dump PRIVATE project_warnings world_generation)
