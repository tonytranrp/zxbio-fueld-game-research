# dev/ -- the development harness (Prompt 002). The scenario MODEL and the telemetry types are
# GPU-free and build in the gating no-GPU CI job; the RUNNER needs the renderer and is gated the
# same way app/ is.
add_subdirectory(scenario)
add_subdirectory(telemetry)
if(VOXEL_BUILD_RENDERER)
  add_subdirectory(harness)
endif()
