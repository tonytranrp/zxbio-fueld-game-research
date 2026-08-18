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
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/events/world/WorldEventModule.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/fonts/FontServiceModule.hpp"
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
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/world/WorldPhysicsEventModule.hpp"
)

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
        pb::core
    PRIVATE
        taskflow
)

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

# ------------------------------------------------------------------------------
# Optional shader validation with glslc (Vulkan SDK / shaderc) or glslangValidator
# ------------------------------------------------------------------------------
if(WIN32)
    set(NULL_DEVICE "NUL")
else()
    set(NULL_DEVICE "/dev/null")
endif()
# SHADER_NAMES covers the 7 shaders text-embedded into ShaderSources.hpp.
# raymarched_voxels is loaded from disk by GamePlayScreen at runtime and is NOT
# embedded, but it still needs syntax validation, so it is validated separately.
set(SHADER_VALIDATION_NAMES ${SHADER_NAMES} raymarched_voxels)
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

    add_executable(BiofuelFarmStateSmokeTest
        "${CMAKE_SOURCE_DIR}/tests/game/FarmStateSmoke.cpp")
    target_link_libraries(BiofuelFarmStateSmokeTest PRIVATE biofuel_game)
    add_test(NAME BiofuelFarmStateSmoke COMMAND BiofuelFarmStateSmokeTest)

    add_executable(BiofuelPassThroughStageAliasesSmokeTest
        "${CMAKE_SOURCE_DIR}/tests/game/PassThroughStageAliasesSmoke.cpp")
    target_link_libraries(BiofuelPassThroughStageAliasesSmokeTest PRIVATE biofuel_game)
    add_test(NAME BiofuelPassThroughStageAliasesSmoke COMMAND BiofuelPassThroughStageAliasesSmokeTest)

    add_executable(BiofuelPipelineSmokeTest
        "${CMAKE_SOURCE_DIR}/tests/pipeline/PipelineSmoke.cpp")
    target_link_libraries(BiofuelPipelineSmokeTest PRIVATE biofuel_game)
    add_test(NAME BiofuelPipelineSmoke COMMAND BiofuelPipelineSmokeTest)

    add_executable(BiofuelTurnPipelineSmokeTest
        "${CMAKE_SOURCE_DIR}/tests/pipeline/TurnPipelineSmoke.cpp")
    target_link_libraries(BiofuelTurnPipelineSmokeTest PRIVATE biofuel_game)
    add_test(NAME BiofuelTurnPipelineSmoke COMMAND BiofuelTurnPipelineSmokeTest)

    add_executable(BiofuelHarvestPipelineSmokeTest
        "${CMAKE_SOURCE_DIR}/tests/pipeline/HarvestPipelineSmoke.cpp")
    target_link_libraries(BiofuelHarvestPipelineSmokeTest PRIVATE biofuel_game)
    add_test(NAME BiofuelHarvestPipelineSmoke COMMAND BiofuelHarvestPipelineSmokeTest)

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

add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)
