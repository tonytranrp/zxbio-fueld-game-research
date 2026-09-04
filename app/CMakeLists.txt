add_executable(voxel_app
    src/main.cpp
    src/glfw_window.cpp
    src/spectator_camera.cpp
    src/chunk_streaming.cpp
    src/crash_handler.cpp
)
target_link_libraries(voxel_app PRIVATE dbghelp) # crash_handler's stack symbolization

target_link_libraries(voxel_app PRIVATE
    project_warnings
    engine_core
    engine_ecs
    engine_input
    engine_jobs
    world_chunk
    world_generation
    world_meshing
    world_streaming
    render_interface
    render_diligent
    glfw
    Tracy::TracyClient
)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
