add_executable(svo_render src/main.cpp)
target_link_libraries(svo_render PRIVATE project_warnings engine_jobs world_chunk world_generation world_svo)

if(BUILD_TESTING)
  # End-to-end CPU proof that the sparse-brick octree pipeline renders real terrain, runnable in the
  # GPU-less CI jobs: build a 128 m sub-cm tree around a default pose, march a small frame, and apply
  # the app's own local-contrast verification metric (exit code carries the verdict).
  add_test(NAME svo_render_smoke
           COMMAND svo_render --root-log2 7 --size 320x180 --verify
                   --out "${CMAKE_CURRENT_BINARY_DIR}/svo_render_smoke.png")
endif()
