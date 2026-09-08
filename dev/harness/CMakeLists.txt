# voxel_harness. It compiles app/src's own translation units directly rather than linking
# voxel_app: an executable target's objects are not linkable, and the alternative -- promoting the
# frame loop to a library just so two exes can share it -- would invert the dependency story
# (app/ is the composition root; dev/ is a tool that drives it). This is the same pattern
# app/tests already uses, for the same reason, and it is what makes "the harness runs the SAME
# loop" a fact about the build rather than a claim in a comment.
add_executable(voxel_harness
    src/main.cpp
    src/harness_options.cpp
    src/harness_run.cpp
    src/image_compare.cpp
    src/moire_metric.cpp
    src/report.cpp
    src/run_one.cpp
    src/run_ramp.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/app_options.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/app_run.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/aim_query.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/sway_forest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/grass_field.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/crash_handler.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/glfw_window.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/spectator_camera.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/svo_world.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/tree_decoration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../app/src/world_loader.cpp
)

target_include_directories(voxel_harness PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../app/src")

target_link_libraries(voxel_harness PRIVATE dbghelp) # crash_handler's stack symbolization

target_link_libraries(voxel_harness PRIVATE
    project_warnings
    dev_scenario
    dev_telemetry
    engine_cli
    engine_core
    engine_ecs
    engine_events
    engine_input
    engine_jobs
    world_chunk
    world_generation
    world_meshing
    world_streaming
    world_svo
    world_collision
    world_player
    world_wind
    world_water
    render_interface
    render_diligent
    glfw
    Tracy::TracyClient
)

# Provenance for the report (goal 216): a baseline whose git SHA and preset are unknown is not a
# baseline. Resolved at configure time, so a stale value means a stale configure, not a lie.
execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short HEAD
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                OUTPUT_VARIABLE VOXEL_GIT_SHA
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET)
if(NOT VOXEL_GIT_SHA)
  set(VOXEL_GIT_SHA "unknown")
endif()
target_compile_definitions(voxel_harness PRIVATE
    VOXEL_GIT_SHA="${VOXEL_GIT_SHA}"
    VOXEL_BUILD_PRESET="${CMAKE_BUILD_TYPE}")

if(BUILD_TESTING)
  add_subdirectory(tests)

  # The cheap scenarios, as ctest tests under the `scenario` label (goal 214).
  #
  # The comment that used to be here said they were "kept OUT of a plain `ctest` run" because they
  # need a GPU. That was an INTENT, never an implementation: a CMake label does not exclude
  # anything by default, so a plain `ctest` has always run them -- and CI run 34114224721's Windows
  # renderer job failed all four for a reason no code change could fix, namely that the GitHub
  # runner enumerates no Vulkan ICD at all ("no compatible GPU/driver enumerated"; WARP is D3D12
  # only). Excluding them is now the CALLER's job and CI does it with `-LE scenario`, which is a
  # thing labels genuinely can do.
  #
  # They stay registered because on a machine WITH a GPU a plain `ctest` running them is exactly
  # what caught seven stale goldens this pass. The working directory is the repo root so
  # dev/scenarios and dev/goldens resolve.
  # stress_pose and fly_transect joined this list for goal 273: they carry the frame-time regression
  # gate, and a gate that is not in `ctest` is a number in a log file. They are the two most
  # expensive scenarios here (11 s and 13 s on vk) and that is the price of the gate.
  foreach(scenario_name spawn_stand macro_ground valley_far clip_stress walk_cliff stress_pose fly_transect)
    add_test(NAME "scenario.${scenario_name}"
             COMMAND voxel_harness --scenario ${scenario_name} --headless --backend vk
                     --out-dir "${CMAKE_CURRENT_BINARY_DIR}/scenario-out"
                     --report "${CMAKE_CURRENT_BINARY_DIR}/scenario-out/${scenario_name}.json"
             WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
    # RESOURCE_LOCK serialises these against each other: they each drive a real swap chain, and two
    # at once on one GPU changes frame timing enough to move a golden. Caught for real -- valley_far
    # failed its golden under `ctest -j 2` and passed three consecutive standalone runs at 0.002%,
    # which is a flake in the harness, not a regression in the scene.
    set_tests_properties("scenario.${scenario_name}" PROPERTIES LABELS "scenario" TIMEOUT 600
                         RESOURCE_LOCK gpu)
  endforeach()
endif()
