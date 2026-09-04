add_subdirectory(core)
add_subdirectory(ecs)
if(VOXEL_BUILD_RENDERER)
  add_subdirectory(input) # GLFW-bound; the callback layer has no meaning in a headless build
endif()
add_subdirectory(jobs)
