# ------------------------------------------------------------------------------
# Source group discovery
# ------------------------------------------------------------------------------
file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/*.hpp"
)
file(GLOB_RECURSE GAME_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/game/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/game/*.hpp"
)
list(FILTER ENGINE_SOURCES EXCLUDE REGEX "/build/")
list(FILTER GAME_SOURCES EXCLUDE REGEX "/build/")

# BIOFUEL_ENABLE_BEVY_BRIDGE (see root CMakeLists.txt) gates both the Rust
# crate and this C++ glue -- these files unconditionally include the
# cxxbridge-generated header, so they must not be compiled at all when the
# crate isn't imported.
if(NOT BIOFUEL_ENABLE_BEVY_BRIDGE)
    list(FILTER ENGINE_SOURCES EXCLUDE REGEX "/engine/bevy/")
    list(FILTER GAME_SOURCES EXCLUDE REGEX "/game/screens/bevy_demo/")
endif()

# ------------------------------------------------------------------------------
# Shader source embedding - BUILD TIME, not configure time.
# ------------------------------------------------------------------------------
set(SHADER_DIR "${CMAKE_SOURCE_DIR}/assets/shaders")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_DIR})
set(GENERATED_INCLUDE_ROOT "${CMAKE_BINARY_DIR}/engine-include-root")
file(REMOVE_RECURSE "${GENERATED_INCLUDE_ROOT}/engine")
file(MAKE_DIRECTORY "${GENERATED_INCLUDE_ROOT}/engine")
foreach(ENGINE_SOURCE IN LISTS ENGINE_SOURCES)
    if(ENGINE_SOURCE MATCHES "\\.(h|hpp)$")
        file(RELATIVE_PATH ENGINE_HEADER_REL
            "${CMAKE_CURRENT_SOURCE_DIR}/engine"
            "${ENGINE_SOURCE}"
        )
        string(REPLACE "\\" "/" ENGINE_HEADER_REL "${ENGINE_HEADER_REL}")
        get_filename_component(ENGINE_HEADER_DIR "${ENGINE_HEADER_REL}" DIRECTORY)
        if(ENGINE_HEADER_DIR)
            file(MAKE_DIRECTORY "${GENERATED_INCLUDE_ROOT}/engine/${ENGINE_HEADER_DIR}")
        endif()
        string(REPLACE "\\" "/" ENGINE_HEADER_ABS "${ENGINE_SOURCE}")
        file(WRITE "${GENERATED_INCLUDE_ROOT}/engine/${ENGINE_HEADER_REL}"
            "#pragma once\n#include \"${ENGINE_HEADER_ABS}\"\n"
        )
    endif()
endforeach()

set(SHADER_NAMES blur_h blur_v blur_composite crossfade loading_prelude procedural_backdrop menu_option)
set(SHADER_SOURCE_FILES "")
foreach(NAME ${SHADER_NAMES})
    list(APPEND SHADER_SOURCE_FILES "${SHADER_DIR}/${NAME}.glsl")
endforeach()

set(ENGINE_TYPED_MODULE_HEADERS
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/animation/AnimationServiceModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/audio/AudioAssetModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/audio/AudioServiceModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/debug/DebugOverlayServiceModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/events/animation/AnimationEventModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/events/EventServiceModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/events/input/InputEventModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/events/model/ModelEventModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/events/mouse/MouseEventModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/events/physics/PhysicsEventModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/events/screen/ScreenEventModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/events/video/VideoEventModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/events/window/WindowEventModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/fonts/FontServiceModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/game/GameWorldServiceModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/graphics/RenderServiceModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/graphics/shaders/BlurCompositeModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/graphics/shaders/BlurHModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/graphics/shaders/BlurVModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/graphics/shaders/CrossfadeModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/graphics/shaders/LoadingPreludeModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/graphics/shaders/ProceduralBackdropModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/graphics/shaders/MenuOptionModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/input/InputServiceModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/models/ModelServiceModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/physics/PhysicsServiceModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/tasks/TaskServiceModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/ui/typed/ScreenServiceModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/video/VideoAssetModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/video/VideoServiceModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/window/WindowServiceModule.hpp"
)
if(BIOFUEL_ENABLE_BEVY_BRIDGE)
    list(APPEND ENGINE_TYPED_MODULE_HEADERS
        "${CMAKE_CURRENT_SOURCE_DIR}/engine/bevy/BevyRenderServiceModule.hpp"
    )
endif()

set(GENERATED_SHADER_HEADER "${GENERATED_DIR}/ShaderSources.hpp")
set(GENERATED_TYPED_EVENT_REGISTRY "${GENERATED_DIR}/engine/runtime/typed/generated/GeneratedEventRegistry.hpp")
set(GENERATED_TYPED_SERVICE_REGISTRY "${GENERATED_DIR}/engine/runtime/typed/generated/GeneratedServiceRegistry.hpp")
set(GENERATED_TYPED_ASSET_REGISTRY "${GENERATED_DIR}/engine/runtime/typed/generated/GeneratedAssetRegistry.hpp")
set(GENERATED_TYPED_SHADER_REGISTRY "${GENERATED_DIR}/engine/runtime/typed/generated/GeneratedShaderRegistry.hpp")

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

add_custom_target(GenerateShaderSources DEPENDS ${GENERATED_SHADER_HEADER})

add_custom_command(
    OUTPUT
        ${GENERATED_TYPED_EVENT_REGISTRY}
        ${GENERATED_TYPED_SERVICE_REGISTRY}
        ${GENERATED_TYPED_ASSET_REGISTRY}
        ${GENERATED_TYPED_SHADER_REGISTRY}
    COMMAND ${CMAKE_COMMAND}
        -D SRC_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        -D OUTPUT_DIR="${GENERATED_DIR}"
        -D "TYPED_MODULE_HEADERS=${ENGINE_TYPED_MODULE_HEADERS}"
        -P "${CMAKE_SOURCE_DIR}/cmake/GenerateTypedRegistries.cmake"
    DEPENDS
        "${CMAKE_SOURCE_DIR}/cmake/GenerateTypedRegistries.cmake"
        ${ENGINE_TYPED_MODULE_HEADERS}
    COMMENT "Generating engine typed module registries"
)

add_custom_target(GenerateTypedRegistries
    DEPENDS
        ${GENERATED_TYPED_EVENT_REGISTRY}
        ${GENERATED_TYPED_SERVICE_REGISTRY}
        ${GENERATED_TYPED_ASSET_REGISTRY}
        ${GENERATED_TYPED_SHADER_REGISTRY}
)

# ------------------------------------------------------------------------------
# Targets
# ------------------------------------------------------------------------------
add_library(biofuel_engine STATIC ${ENGINE_SOURCES})
add_dependencies(biofuel_engine GenerateShaderSources GenerateTypedRegistries)

target_include_directories(biofuel_engine
    PUBLIC
        ${GENERATED_DIR}
        ${GENERATED_INCLUDE_ROOT}
        ${entt_SOURCE_DIR}/src
)

target_link_libraries(biofuel_engine
    PUBLIC
        raylib
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        biofuel_rapier_bridge
        biofuel_rapier_bridge_cxx
        biofuel_game_ecs
        biofuel_game_ecs_cxx
        pb::core
    PRIVATE
        taskflow
)

if(BIOFUEL_ENABLE_BEVY_BRIDGE)
    target_link_libraries(biofuel_engine PUBLIC biofuel_bevy_bridge biofuel_bevy_bridge_cxx)
    target_compile_definitions(biofuel_engine PUBLIC BIOFUEL_WITH_BEVY_BRIDGE)
endif()

add_library(biofuel_game STATIC ${GAME_SOURCES})
add_dependencies(biofuel_game GenerateShaderSources GenerateTypedRegistries)
target_link_libraries(biofuel_game
    PUBLIC
        biofuel_engine
        pb::runtime
)

target_include_directories(biofuel_game
    PUBLIC
        ${GENERATED_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${entt_SOURCE_DIR}/src
)

add_executable(${PROJECT_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
target_link_libraries(${PROJECT_NAME} PRIVATE biofuel_game)

# ------------------------------------------------------------------------------
# Precompiled headers - raylib.h/entt/json/spdlog are each included, directly
# or transitively, by most translation units in both libraries; precompiling
# them once instead of re-parsing per TU is a large chunk of total build time
# on a single-core-bound compile.
#
# Each target gets its own PCH rather than biofuel_game reusing biofuel_engine's
# (CMake's REUSE_FROM): MSVC requires byte-for-byte identical preprocessor
# definitions between the compile that creates a .pch and every compile that
# reuses it (C4651, promoted to a hard error by /WX otherwise), but
# biofuel_engine and biofuel_game deliberately carry different PRIVATE
# compile definitions (BIOFUEL_DEV_STARTUP_MENU_TRANSITION,
# BIOFUEL_DEBUG_MEMORY_STATS, BIOFUEL_DEV_STARTUP_IDLE_VIDEO -- see the
# per-target dev-toggle options below). REUSE_FROM would silently break the
# moment any one of those is turned on. Two small PCH compiles instead of one
# costs a few seconds; hand-synchronizing defines across the engine/game
# boundary forever does not seem worth it.
# ------------------------------------------------------------------------------
set(BIOFUEL_PCH_HEADERS
    <raylib.h>
    <entt/signal/dispatcher.hpp>
    <nlohmann/json.hpp>
    <spdlog/spdlog.h>
    <memory>
    <string>
    <string_view>
    <vector>
    <functional>
    <unordered_map>
)
target_precompile_headers(biofuel_engine PRIVATE ${BIOFUEL_PCH_HEADERS})
target_precompile_headers(biofuel_game PRIVATE ${BIOFUEL_PCH_HEADERS})

# MemoryTelemetry.cpp deliberately never touches raylib (it's pure Win32/Psapi
# process-memory stats -- see tests/architecture/RuntimeSafetyGuard.cmake's
# raylib-lifetime allowlist, which it is intentionally not part of). Forcing
# raylib.h into it via the shared PCH would declare CloseWindow/ShowCursor
# with C++ linkage before its own <Windows.h> declares the real extern "C"
# WinAPI versions of the same names (C2733). Simplest fix: this one file
# skips the PCH and compiles its own includes normally.
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/debug/MemoryTelemetry.cpp"
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON
)

# ------------------------------------------------------------------------------
# Optional subsystems and compile definitions
# ------------------------------------------------------------------------------
find_program(FFMPEG_EXECUTABLE ffmpeg)
if(FFMPEG_EXECUTABLE)
    message(STATUS "ffmpeg found: ${FFMPEG_EXECUTABLE}")
    target_compile_definitions(biofuel_engine PRIVATE BIOFUEL_FFMPEG_EXECUTABLE="${FFMPEG_EXECUTABLE}")
else()
    message(STATUS "ffmpeg not found at configure time - runtime video will search PATH")
endif()

option(BIOFUEL_STRICT_COMPILE_CHECKS "Enable stricter compiler diagnostics for BiofuelGame." ON)
option(BIOFUEL_WARNINGS_AS_ERRORS "Treat BiofuelGame compiler warnings as errors." ON)

function(biofuel_apply_strict_warnings TARGET_NAME)
    if(BIOFUEL_STRICT_COMPILE_CHECKS)
        if(MSVC)
            target_compile_options(${TARGET_NAME} PRIVATE
                /W4
                /permissive-
                /Zc:__cplusplus
                /Zc:preprocessor
                /Zc:inline
                /Zc:throwingNew
                /EHsc
                /FS
                /sdl
                $<$<BOOL:${BIOFUEL_WARNINGS_AS_ERRORS}>:/WX>
            )
        else()
            target_compile_options(${TARGET_NAME} PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wshadow
                -Wconversion
                -Wsign-conversion
                -Wnon-virtual-dtor
                $<$<BOOL:${BIOFUEL_WARNINGS_AS_ERRORS}>:-Werror>
            )
        endif()
    endif()
endfunction()

biofuel_apply_strict_warnings(biofuel_engine)
biofuel_apply_strict_warnings(biofuel_game)
biofuel_apply_strict_warnings(${PROJECT_NAME})

option(BIOFUEL_DEV_STARTUP_MENU_TRANSITION "Dev mode: auto-run the main-menu dismiss transition after load" OFF)
if(BIOFUEL_DEV_STARTUP_MENU_TRANSITION)
    target_compile_definitions(biofuel_game PRIVATE BIOFUEL_DEV_STARTUP_MENU_TRANSITION)
endif()

option(BIOFUEL_DEBUG_MEMORY_STATS "Debug mode: show memory/resource telemetry overlay." OFF)
if(BIOFUEL_DEBUG_MEMORY_STATS)
    target_compile_definitions(biofuel_engine PRIVATE BIOFUEL_DEBUG_MEMORY_STATS)
endif()

option(BIOFUEL_DEV_STARTUP_IDLE_VIDEO "Dev mode: start directly on idle video screen" OFF)
if(BIOFUEL_DEV_STARTUP_IDLE_VIDEO)
    target_compile_definitions(biofuel_game PRIVATE BIOFUEL_DEV_STARTUP_IDLE_VIDEO)
endif()

option(BIOFUEL_DEV_DISABLE_IDLE_TIMEOUT "Dev mode: disable the main-menu idle-screen auto-trigger (music/video overlay after inactivity)" ON)
if(BIOFUEL_DEV_DISABLE_IDLE_TIMEOUT)
    target_compile_definitions(biofuel_game PRIVATE BIOFUEL_DEV_DISABLE_IDLE_TIMEOUT)
endif()

# ------------------------------------------------------------------------------
# Optional shader validation with glslc (Vulkan SDK / shaderc) or glslangValidator
# ------------------------------------------------------------------------------
if(WIN32)
    set(NULL_DEVICE "NUL")
else()
    set(NULL_DEVICE "/dev/null")
endif()
# SHADER_NAMES covers the 7 shaders text-embedded into ShaderSources.hpp.
set(SHADER_VALIDATION_NAMES ${SHADER_NAMES})
find_program(GLSLC glslc)
find_program(GLSLANG_VALIDATOR glslangValidator)
if(GLSLC)
    foreach(SHADER IN ITEMS ${SHADER_VALIDATION_NAMES})
        add_custom_command(
            TARGET ${PROJECT_NAME} PRE_BUILD
            COMMAND ${GLSLC} -fshader-stage=fragment --target-env=opengl -o ${NULL_DEVICE} "${SHADER_DIR}/${SHADER}.glsl"
            COMMENT "Validating shader: ${SHADER}.glsl"
        )
    endforeach()
elseif(GLSLANG_VALIDATOR)
    foreach(SHADER IN ITEMS ${SHADER_VALIDATION_NAMES})
        add_custom_command(
            TARGET ${PROJECT_NAME} PRE_BUILD
            COMMAND ${GLSLANG_VALIDATOR} -S frag "${SHADER_DIR}/${SHADER}.glsl"
            COMMENT "Validating shader: ${SHADER}.glsl"
        )
    endforeach()
else()
    message(STATUS "glslc and glslangValidator not found - shader validation skipped. Install Vulkan SDK (glslc) or a standalone glslangValidator for shader validation.")
endif()

if(APPLE)
    target_link_libraries(biofuel_engine PUBLIC "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
elseif(UNIX AND NOT APPLE)
    target_link_libraries(biofuel_engine PUBLIC m dl pthread rt X11)
endif()

if(WIN32)
    target_link_libraries(biofuel_engine PUBLIC Psapi)
endif()

# ------------------------------------------------------------------------------
# Tests
# ------------------------------------------------------------------------------
option(BIOFUEL_BUILD_TESTS "Build BiofuelGame smoke-test executables." ON)
if(BIOFUEL_BUILD_TESTS AND BUILD_TESTING)
    add_executable(BiofuelPhysicsSmokeTest "${CMAKE_SOURCE_DIR}/tests/physics/PhysicsSmoke.cpp")
    target_link_libraries(BiofuelPhysicsSmokeTest PRIVATE biofuel_engine)
    add_test(NAME BiofuelPhysicsSmoke COMMAND BiofuelPhysicsSmokeTest)

    add_executable(BiofuelCharacterControllerSmokeTest "${CMAKE_SOURCE_DIR}/tests/physics/CharacterControllerSmoke.cpp")
    target_link_libraries(BiofuelCharacterControllerSmokeTest PRIVATE biofuel_engine)
    add_test(NAME BiofuelCharacterControllerSmoke COMMAND BiofuelCharacterControllerSmokeTest)

    add_executable(BiofuelEngineUsabilitySmokeTest "${CMAKE_SOURCE_DIR}/tests/engine/EngineUsabilitySmoke.cpp")
    target_link_libraries(BiofuelEngineUsabilitySmokeTest PRIVATE biofuel_engine)
    add_test(NAME BiofuelEngineUsabilitySmoke COMMAND BiofuelEngineUsabilitySmokeTest)

    add_executable(BiofuelLoadingTaskQueueSmokeTest "${CMAKE_SOURCE_DIR}/tests/engine/LoadingTaskQueueSmoke.cpp")
    target_link_libraries(BiofuelLoadingTaskQueueSmokeTest PRIVATE biofuel_engine)
    add_test(NAME BiofuelLoadingTaskQueueSmoke COMMAND BiofuelLoadingTaskQueueSmokeTest)

    add_executable(BiofuelTaskManagerSmokeTest "${CMAKE_SOURCE_DIR}/tests/engine/TaskManagerSmoke.cpp")
    target_link_libraries(BiofuelTaskManagerSmokeTest PRIVATE biofuel_engine)
    add_test(NAME BiofuelTaskManagerSmoke COMMAND BiofuelTaskManagerSmokeTest)

    add_executable(BiofuelVideoFailureSmokeTest "${CMAKE_SOURCE_DIR}/tests/engine/VideoFailureSmoke.cpp")
    target_link_libraries(BiofuelVideoFailureSmokeTest PRIVATE biofuel_engine)
    add_test(NAME BiofuelVideoFailureSmoke COMMAND BiofuelVideoFailureSmokeTest)

    add_test(NAME BiofuelEngineBoundaryGuard
        COMMAND ${CMAKE_COMMAND}
            -D SOURCE_DIR=${CMAKE_SOURCE_DIR}
            -P "${CMAKE_SOURCE_DIR}/tests/architecture/EngineBoundaryGuard.cmake")

    add_test(NAME BiofuelRuntimeSafetyGuard
        COMMAND ${CMAKE_COMMAND}
            -D SOURCE_DIR=${CMAKE_SOURCE_DIR}
            -P "${CMAKE_SOURCE_DIR}/tests/architecture/RuntimeSafetyGuard.cmake")

    add_test(NAME BiofuelRegistryManifestGuard
        COMMAND ${CMAKE_COMMAND}
            -D SOURCE_DIR=${CMAKE_SOURCE_DIR}
            -P "${CMAKE_SOURCE_DIR}/tests/architecture/RegistryManifestGuard.cmake")
    add_test(NAME BiofuelModelSafetyGuard
        COMMAND ${CMAKE_COMMAND}
            -D SOURCE_DIR=${CMAKE_SOURCE_DIR}
            -P "${CMAKE_SOURCE_DIR}/tests/architecture/ModelSafetyGuard.cmake")
    add_test(NAME BiofuelVideoSafetyGuard
        COMMAND ${CMAKE_COMMAND}
            -D SOURCE_DIR=${CMAKE_SOURCE_DIR}
            -P "${CMAKE_SOURCE_DIR}/tests/architecture/VideoSafetyGuard.cmake")
    add_test(NAME BiofuelScreenFlowGuard
        COMMAND ${CMAKE_COMMAND}
            -D SOURCE_DIR=${CMAKE_SOURCE_DIR}
            -P "${CMAKE_SOURCE_DIR}/tests/architecture/ScreenFlowGuard.cmake")
    add_test(NAME BiofuelPauseFlowGuard
        COMMAND ${CMAKE_COMMAND}
            -D SOURCE_DIR=${CMAKE_SOURCE_DIR}
            -P "${CMAKE_SOURCE_DIR}/tests/architecture/PauseFlowGuard.cmake")
    add_test(NAME BiofuelTaskManagerBoundaryGuard
        COMMAND ${CMAKE_COMMAND}
            -D SOURCE_DIR=${CMAKE_SOURCE_DIR}
            -P "${CMAKE_SOURCE_DIR}/tests/architecture/TaskManagerBoundaryGuard.cmake")

    add_test(NAME BiofuelTargetBuild_Engine
        COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" --config "$<CONFIG>" --target biofuel_engine)
    add_test(NAME BiofuelTargetBuild_Game
        COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" --config "$<CONFIG>" --target biofuel_game)
    add_test(NAME BiofuelTargetBuild_App
        COMMAND ${CMAKE_COMMAND} --build "${CMAKE_BINARY_DIR}" --config "$<CONFIG>" --target ${PROJECT_NAME})
endif()

# ------------------------------------------------------------------------------
# Runtime assets
# ------------------------------------------------------------------------------
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:raylib>"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMENT "Copying raylib.dll to output directory"
)

# biofuel_game_ecs is a cdylib (see root CMakeLists.txt) -- same reason and
# same POST_BUILD treatment as biofuel_bevy_bridge below, but unconditional
# since this crate always builds.
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:biofuel_game_ecs-shared>"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    COMMENT "Copying biofuel_game_ecs.dll to output directory"
)

# biofuel_bevy_bridge is a cdylib (see root CMakeLists.txt) -- Corrosion copies
# the built .dll into build/lib/, but Windows only searches next to the .exe
# (build/bin/) at process start, so it needs the same POST_BUILD treatment as
# raylib.dll above.
if(BIOFUEL_ENABLE_BEVY_BRIDGE)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:biofuel_bevy_bridge-shared>"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
        COMMENT "Copying biofuel_bevy_bridge.dll to output directory"
    )
endif()

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)
