# Collect all source files recursively from all subdirectories
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)

# ------------------------------------------------------------------------------
# Shader source embedding - BUILD TIME, not configure time.
# Editing a .glsl file and hitting Build (F7) in Visual Studio will now
# regenerate ShaderSources.hpp and recompile dependent files automatically.
# ------------------------------------------------------------------------------
set(SHADER_DIR "${CMAKE_SOURCE_DIR}/assets/shaders")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_DIR})

set(SHADER_NAMES blur_h blur_v blur_composite crossfade loading_prelude mainmenu_bg menu_option)
set(SHADER_SOURCE_FILES "")
foreach(NAME ${SHADER_NAMES})
    list(APPEND SHADER_SOURCE_FILES "${SHADER_DIR}/${NAME}.glsl")
endforeach()

set(GENERATED_SHADER_HEADER "${GENERATED_DIR}/ShaderSources.hpp")

# Build-time command: regenerate header whenever any .glsl changes
add_custom_command(
    OUTPUT ${GENERATED_SHADER_HEADER}
    COMMAND ${CMAKE_COMMAND}
        -D SHADER_DIR="${SHADER_DIR}"
        -D OUTPUT_FILE="${GENERATED_SHADER_HEADER}"
        -D "SHADER_NAMES=${SHADER_NAMES}"
        -P "${CMAKE_SOURCE_DIR}/cmake/EmbedShaders.cmake"
    DEPENDS
        ${SHADER_SOURCE_FILES}
        "${CMAKE_SOURCE_DIR}/cmake/EmbedShaders.cmake"
    COMMENT "Embedding shader sources into ShaderSources.hpp"
)

# Dummy target so the executable can depend on the generated header
add_custom_target(GenerateShaderSources DEPENDS ${GENERATED_SHADER_HEADER})

add_executable(${PROJECT_NAME} ${SOURCES})
add_dependencies(${PROJECT_NAME} GenerateShaderSources)

# Header-only library include paths (CPM sets these variables)
target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${GENERATED_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/Core
        ${CMAKE_CURRENT_SOURCE_DIR}/Utils
        ${CMAKE_CURRENT_SOURCE_DIR}/Utils/render
        ${CMAKE_CURRENT_SOURCE_DIR}/Utils/render/Shader
        ${CMAKE_CURRENT_SOURCE_DIR}/Utils/font
        ${CMAKE_CURRENT_SOURCE_DIR}/Systems
        ${CMAKE_CURRENT_SOURCE_DIR}/UI
        ${CMAKE_CURRENT_SOURCE_DIR}/UI/screens
        ${CMAKE_CURRENT_SOURCE_DIR}/UI/screens/LoadingScreen
        ${CMAKE_CURRENT_SOURCE_DIR}/UI/screens/MainMenu
        ${CMAKE_CURRENT_SOURCE_DIR}/UI/screens/PausePopupScreen
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

# Development compile-time flag: set to ON to auto-trigger the main-menu hand transition
option(BIOFUEL_DEV_STARTUP_MENU_TRANSITION "Dev mode: auto-run the main-menu hand transition after load" OFF)
if(BIOFUEL_DEV_STARTUP_MENU_TRANSITION)
    target_compile_definitions(${PROJECT_NAME} PRIVATE BIOFUEL_DEV_STARTUP_MENU_TRANSITION)
endif()

# Development compile-time flag: set to ON to enable in-game model transform controls
option(BIOFUEL_DEV_MODEL_CONTROLLER "Dev mode: enable model controller overlay for runtime tuning" OFF)
if(BIOFUEL_DEV_MODEL_CONTROLLER)
    target_compile_definitions(${PROJECT_NAME} PRIVATE BIOFUEL_DEV_MODEL_CONTROLLER)
endif()

# ------------------------------------------------------------------------------
# Optional shader validation with glslc (Vulkan SDK / shaderc)
# Non-fatal - warns if glslc not found, proceeds without validation.
# ------------------------------------------------------------------------------
if(WIN32)
    set(NULL_DEVICE "NUL")
else()
    set(NULL_DEVICE "/dev/null")
endif()
find_program(GLSLC glslc)
if(GLSLC)
    foreach(SHADER IN ITEMS ${SHADER_NAMES})
        add_custom_command(
            TARGET ${PROJECT_NAME} PRE_BUILD
            COMMAND ${GLSLC} --target-env=opengl -o ${NULL_DEVICE} "${SHADER_DIR}/${SHADER}.glsl"
            COMMENT "Validating shader: ${SHADER}.glsl"
        )
    endforeach()
else()
    message(STATUS "glslc not found - shader validation skipped. Install Vulkan SDK for shader validation.")
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
