# Collect all source files recursively from all subdirectories
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)
list(FILTER SOURCES EXCLUDE REGEX "/build/")

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
        -P "${CMAKE_SOURCE_DIR}/cmake/GenerateTypedRegistries.cmake"
    DEPENDS
        "${CMAKE_SOURCE_DIR}/cmake/GenerateTypedRegistries.cmake"
        ${SOURCES}
    COMMENT "Generating typed module registries"
)

add_custom_target(GenerateTypedRegistries
    DEPENDS
        ${GENERATED_TYPED_EVENT_REGISTRY}
        ${GENERATED_TYPED_SERVICE_REGISTRY}
        ${GENERATED_TYPED_ASSET_REGISTRY}
        ${GENERATED_TYPED_SHADER_REGISTRY}
)

add_executable(${PROJECT_NAME} ${SOURCES})
add_dependencies(${PROJECT_NAME} GenerateShaderSources GenerateTypedRegistries)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${GENERATED_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${entt_SOURCE_DIR}/src
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        raylib
        nlohmann_json::nlohmann_json
        spdlog::spdlog
        biofuel_rapier_bridge
        biofuel_rapier_bridge_cxx
)

if(BIOFUEL_ENABLE_HAND_TRACKING)
    set(HAND_TRACKING_PYTHON_ROOT "${CMAKE_BINARY_DIR}/python/hand_tracking")
    if(WIN32)
        set(HAND_TRACKING_PYTHON_EXECUTABLE "${HAND_TRACKING_PYTHON_ROOT}/.venv/Scripts/python.exe")
    else()
        set(HAND_TRACKING_PYTHON_EXECUTABLE "${HAND_TRACKING_PYTHON_ROOT}/.venv/bin/python")
    endif()

    add_custom_target(SetupHandTrackingPython
        COMMAND ${CMAKE_COMMAND}
            -D OUTPUT_DIR="${HAND_TRACKING_PYTHON_ROOT}"
            -D REQUIREMENTS_FILE="${CMAKE_SOURCE_DIR}/tools/python/biofuel_hand_tracking/requirements.txt"
            -P "${CMAKE_SOURCE_DIR}/cmake/SetupHandTrackingPython.cmake"
        DEPENDS
            "${CMAKE_SOURCE_DIR}/tools/python/biofuel_hand_tracking/requirements.txt"
            "${CMAKE_SOURCE_DIR}/cmake/SetupHandTrackingPython.cmake"
        COMMENT "Provisioning build-local Python 3.12 hand-tracking runtime"
    )
    add_dependencies(${PROJECT_NAME} SetupHandTrackingPython)

    target_include_directories(${PROJECT_NAME} SYSTEM PRIVATE "${asio_SOURCE_DIR}/asio/include")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        BIOFUEL_ENABLE_HAND_TRACKING
        ASIO_STANDALONE
        ASIO_NO_DEPRECATED
        BIOFUEL_HAND_TRACKING_PYTHON_EXECUTABLE="${HAND_TRACKING_PYTHON_EXECUTABLE}"
    )
    if(WIN32)
        target_compile_definitions(${PROJECT_NAME} PRIVATE _WIN32_WINNT=0x0601 WIN32_LEAN_AND_MEAN NOMINMAX)
        target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
    endif()
endif()

find_program(FFMPEG_EXECUTABLE ffmpeg)
if(FFMPEG_EXECUTABLE)
    message(STATUS "ffmpeg found: ${FFMPEG_EXECUTABLE}")
    target_compile_definitions(${PROJECT_NAME} PRIVATE BIOFUEL_FFMPEG_EXECUTABLE="${FFMPEG_EXECUTABLE}")
else()
    message(STATUS "ffmpeg not found at configure time - runtime video will search PATH")
endif()

option(BIOFUEL_STRICT_COMPILE_CHECKS "Enable stricter compiler diagnostics for BiofuelGame." ON)
option(BIOFUEL_WARNINGS_AS_ERRORS "Treat BiofuelGame compiler warnings as errors." ON)

if(BIOFUEL_STRICT_COMPILE_CHECKS)
    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE
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
        target_compile_options(${PROJECT_NAME} PRIVATE
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

target_compile_definitions(${PROJECT_NAME} PRIVATE SPDLOG_HEADER_ONLY)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    $<$<CONFIG:Debug>:BIOFUEL_ENABLE_DEV_SCREENS>
)

option(BIOFUEL_DEV_STARTUP_PAUSE_POPUP "Dev mode: start on pause popup for shader testing" OFF)
if(BIOFUEL_DEV_STARTUP_PAUSE_POPUP)
    target_compile_definitions(${PROJECT_NAME} PRIVATE BIOFUEL_DEV_STARTUP_PAUSE_POPUP)
endif()

option(BIOFUEL_DEV_STARTUP_MENU_TRANSITION "Dev mode: auto-run the main-menu hand transition after load" OFF)
if(BIOFUEL_DEV_STARTUP_MENU_TRANSITION)
    target_compile_definitions(${PROJECT_NAME} PRIVATE BIOFUEL_DEV_STARTUP_MENU_TRANSITION)
endif()

option(BIOFUEL_DEV_STARTUP_HAND_LAB "Dev mode: start directly on the procedural hand lab after loading" OFF)
if(BIOFUEL_DEV_STARTUP_HAND_LAB)
    target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:BIOFUEL_DEV_STARTUP_HAND_LAB>)
endif()

option(BIOFUEL_DEBUG_MEMORY_STATS "Debug mode: show memory/resource telemetry overlay." OFF)
if(BIOFUEL_DEBUG_MEMORY_STATS)
    target_compile_definitions(${PROJECT_NAME} PRIVATE BIOFUEL_DEBUG_MEMORY_STATS)
endif()

option(BIOFUEL_DEV_STARTUP_IDLE_VIDEO "Dev mode: start directly on idle video screen" OFF)
if(BIOFUEL_DEV_STARTUP_IDLE_VIDEO)
    target_compile_definitions(${PROJECT_NAME} PRIVATE BIOFUEL_DEV_STARTUP_IDLE_VIDEO)
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

if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
elseif(UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE m dl pthread rt X11)
endif()

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE Psapi)
endif()

option(BIOFUEL_BUILD_TESTS "Build BiofuelGame smoke-test executables." ON)
if(BIOFUEL_BUILD_TESTS AND BUILD_TESTING)
    add_executable(BiofuelPhysicsSmokeTest
        "${CMAKE_SOURCE_DIR}/tests/physics/PhysicsSmoke.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/engine/events/EventManager.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/engine/custom/procedural/hand/HandPhysicsInteraction.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/engine/physics/PhysicsSystem.cpp"
    )
    add_dependencies(BiofuelPhysicsSmokeTest GenerateTypedRegistries)
    target_include_directories(BiofuelPhysicsSmokeTest
        PRIVATE
            ${GENERATED_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${entt_SOURCE_DIR}/src
    )
    target_link_libraries(BiofuelPhysicsSmokeTest
        PRIVATE
            raylib
            biofuel_rapier_bridge
            biofuel_rapier_bridge_cxx
    )
    add_test(NAME BiofuelPhysicsSmoke COMMAND BiofuelPhysicsSmokeTest)

    add_executable(BiofuelHandRetargetSmokeTest
        "${CMAKE_SOURCE_DIR}/tests/procedural/HandRetargetSmoke.cpp"
    )
    target_include_directories(BiofuelHandRetargetSmokeTest
        PRIVATE
            ${GENERATED_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${entt_SOURCE_DIR}/src
    )
    target_link_libraries(BiofuelHandRetargetSmokeTest
        PRIVATE
            raylib
    )
    add_test(NAME BiofuelHandRetargetSmoke COMMAND BiofuelHandRetargetSmokeTest)

    add_executable(BiofuelEngineUsabilitySmokeTest
        "${CMAKE_SOURCE_DIR}/tests/engine/EngineUsabilitySmoke.cpp"
    )
    add_dependencies(BiofuelEngineUsabilitySmokeTest GenerateShaderSources GenerateTypedRegistries)
    target_include_directories(BiofuelEngineUsabilitySmokeTest
        PRIVATE
            ${GENERATED_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${entt_SOURCE_DIR}/src
    )
    target_link_libraries(BiofuelEngineUsabilitySmokeTest
        PRIVATE
            raylib
    )
    add_test(NAME BiofuelEngineUsabilitySmoke COMMAND BiofuelEngineUsabilitySmokeTest)
endif()

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

if(BIOFUEL_ENABLE_HAND_TRACKING)
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/python/hand_tracking
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/tools/python/biofuel_hand_tracking
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/python/hand_tracking
        COMMENT "Copying Python hand-tracking worker to output directory"
    )
endif()
