add_library(engine_input STATIC
    src/glfw_input.cpp
)

target_sources(engine_input PUBLIC
    FILE_SET HEADERS
    BASE_DIRS include
    FILES
        include/engine/input/input_state.hpp
        include/engine/input/glfw_input.hpp
)

# glfw is PRIVATE on purpose: the public headers expose only a GLFWwindow forward declaration and
# plain InputState data, so consumers of engine_input never compile against GLFW.
target_link_libraries(engine_input
    PUBLIC project_warnings engine_core
    PRIVATE glfw
)
