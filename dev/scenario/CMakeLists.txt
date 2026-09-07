# The scenario MODEL. No Diligent, no GLFW, no FastNoise2 -- dev/harness executes one, this
# describes one. That firewall is what lets the whole module build and test under
# -DVOXEL_BUILD_RENDERER=OFF, which is the gating CI job (see README.md).
add_library(dev_scenario STATIC
    src/motion_script.cpp
    src/names.cpp
    src/parser.cpp
    src/registry.cpp
)

target_sources(dev_scenario PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/dev/scenario/assertion.hpp
        include/dev/scenario/backend.hpp
        include/dev/scenario/capture.hpp
        include/dev/scenario/input_frame.hpp
        include/dev/scenario/motion_script.hpp
        include/dev/scenario/parser.hpp
        include/dev/scenario/pose.hpp
        include/dev/scenario/registry.hpp
        include/dev/scenario/scenario.hpp
)

target_link_libraries(dev_scenario PUBLIC project_warnings engine_core world_player)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
