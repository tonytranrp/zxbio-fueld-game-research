# Collect all source files recursively from all subdirectories
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)

# ------------------------------------------------------------------------------
# Shader source embedding — reads .glsl files at configure time, generates
# a header with constexpr std::string_view for each shader.
# ------------------------------------------------------------------------------
set(SHADER_DIR "${CMAKE_SOURCE_DIR}/assets/shaders")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_DIR})

set(SHADER_SOURCES_CONTENT "#pragma once\n#include <string_view>\n\nnamespace biofuel::shader_source {\n")

foreach(SHADER_FILE IN ITEMS blur_h blur_v crossfade)
    file(READ "${SHADER_DIR}/${SHADER_FILE}.glsl" SHADER_CONTENT)
    string(APPEND SHADER_SOURCES_CONTENT
        "inline constexpr std::string_view ${SHADER_FILE}_source = R\"shader(${SHADER_CONTENT})shader\";\n\n"
    )
endforeach()

string(APPEND SHADER_SOURCES_CONTENT "} // namespace biofuel::shader_source\n")

file(WRITE "${GENERATED_DIR}/ShaderSources.hpp" "${SHADER_SOURCES_CONTENT}")

add_executable(${PROJECT_NAME} ${SOURCES})

# Header-only library include paths (CPM sets these variables)
target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${GENERATED_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/Core
        ${CMAKE_CURRENT_SOURCE_DIR}/Utils
        ${CMAKE_CURRENT_SOURCE_DIR}/Utils/render
        ${CMAKE_CURRENT_SOURCE_DIR}/Utils/render/Shader
        ${CMAKE_CURRENT_SOURCE_DIR}/Utils/event
        ${CMAKE_CURRENT_SOURCE_DIR}/Utils/json
        ${CMAKE_CURRENT_SOURCE_DIR}/Utils/task
        ${CMAKE_CURRENT_SOURCE_DIR}/Utils/font
        ${CMAKE_CURRENT_SOURCE_DIR}/Systems
        ${CMAKE_CURRENT_SOURCE_DIR}/UI
        ${CMAKE_CURRENT_SOURCE_DIR}/UI/screens
        ${CMAKE_CURRENT_SOURCE_DIR}/Data
        ${CMAKE_CURRENT_SOURCE_DIR}/AnimationController
        ${CMAKE_CURRENT_SOURCE_DIR}/AnimationController/screen
        ${CMAKE_CURRENT_SOURCE_DIR}/AnimationController/animation
        ${entt_SOURCE_DIR}/src
        ${Taskflow_SOURCE_DIR}
        ${Taskflow_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        raylib
        nlohmann_json::nlohmann_json
        spdlog::spdlog
)

# spdlog header-only mode for faster compile
target_compile_definitions(${PROJECT_NAME} PRIVATE SPDLOG_HEADER_ONLY)

# Development compile-time flag: set to ON to start directly on PausePopupScreen
option(BIOFUEL_DEV_STARTUP_PAUSE_POPUP "Dev mode: start on pause popup for shader testing" OFF)
if(BIOFUEL_DEV_STARTUP_PAUSE_POPUP)
    target_compile_definitions(${PROJECT_NAME} PRIVATE BIOFUEL_DEV_STARTUP_PAUSE_POPUP)
endif()

# ------------------------------------------------------------------------------
# Optional shader validation with glslc (Vulkan SDK / shaderc)
# Non-fatal — warns if glslc not found, proceeds without validation.
# ------------------------------------------------------------------------------
find_program(GLSLC glslc)
if(GLSLC)
    foreach(SHADER IN ITEMS blur_h blur_v crossfade)
        add_custom_command(
            TARGET ${PROJECT_NAME} PRE_BUILD
            COMMAND ${GLSLC} --target-env=opengl -o NUL "${SHADER_DIR}/${SHADER}.glsl"
            COMMENT "Validating shader: ${SHADER}.glsl"
        )
    endforeach()
else()
    message(STATUS "glslc not found — shader validation skipped. Install Vulkan SDK for shader validation.")
endif()

# Platform-specific link libraries
if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
elseif(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE m dl pthread rt X11)
endif()

# Copy assets to build directory on build
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)
