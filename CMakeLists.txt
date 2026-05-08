cmake_minimum_required(VERSION 3.20)
project(BiofuelGame VERSION 0.1.0 LANGUAGES CXX)

# ------------------------------------------------------------------------------
# C++ Standard
# ------------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ------------------------------------------------------------------------------
# Build Output Directories
# ------------------------------------------------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# ------------------------------------------------------------------------------
# Compiler Optimizations (Release only)
# ------------------------------------------------------------------------------
if(MSVC)
    # Whole program optimization + link-time code generation
    add_compile_options($<$<CONFIG:Release>:/GL>)
    add_link_options($<$<CONFIG:Release>:/LTCG>)
    # Remove dead code
    add_link_options($<$<CONFIG:Release>:/OPT:REF>)
    add_link_options($<$<CONFIG:Release>:/OPT:ICF>)
    # Maximize speed
    add_compile_options($<$<CONFIG:Release>:/O2>)
    add_compile_options($<$<CONFIG:Release>:/Ob2>)
endif()

# ------------------------------------------------------------------------------
# CPM.cmake - CMake Package Manager
# ------------------------------------------------------------------------------
set(CPM_DOWNLOAD_VERSION 0.40.2)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")

if(NOT EXISTS ${CPM_DOWNLOAD_LOCATION})
    message(STATUS "Downloading CPM.cmake v${CPM_DOWNLOAD_VERSION}...")
    file(DOWNLOAD
        https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake
        ${CPM_DOWNLOAD_LOCATION}
        STATUS CPM_DOWNLOAD_STATUS
    )
    list(GET CPM_DOWNLOAD_STATUS 0 CPM_DOWNLOAD_STATUS_CODE)
    if(NOT CPM_DOWNLOAD_STATUS_CODE EQUAL 0)
        message(FATAL_ERROR "Failed to download CPM.cmake")
    endif()
endif()

include(${CPM_DOWNLOAD_LOCATION})

# ------------------------------------------------------------------------------
# Fetch Dependencies
# ------------------------------------------------------------------------------

# Raylib - 2D/3D game library (minimal build)
CPMAddPackage(
    NAME raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
    OPTIONS
        "BUILD_EXAMPLES OFF"
        "BUILD_GAMES OFF"
        "SHARED OFF"
        "STATIC ON"
)

# entt - Entity Component System (header-only)
CPMAddPackage(
    NAME entt
    URL https://github.com/skypjack/entt/archive/refs/tags/v3.14.0.tar.gz
    OPTIONS
        "ENTT_BUILD_TESTING OFF"
)

# nlohmann/json - JSON parsing (header-only)
CPMAddPackage(
    NAME nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    OPTIONS
        "JSON_BuildTests OFF"
        "JSON_Install OFF"
)

# taskflow - Parallel task scheduling (header-only)
CPMAddPackage(
    NAME Taskflow
    URL https://github.com/taskflow/taskflow/archive/refs/tags/v3.7.0.tar.gz
    OPTIONS
        "TF_BUILD_TESTS OFF"
        "TF_BUILD_EXAMPLES OFF"
)

# spdlog - Logging (header-only mode)
CPMAddPackage(
    NAME spdlog
    URL https://github.com/gabime/spdlog/archive/refs/tags/v1.14.1.tar.gz
    OPTIONS
        "SPDLOG_BUILD_EXAMPLE OFF"
        "SPDLOG_INSTALL OFF"
        "SPDLOG_FMT_EXTERNAL OFF"
)

# ------------------------------------------------------------------------------
# Source Code
# ------------------------------------------------------------------------------
add_subdirectory(src)
